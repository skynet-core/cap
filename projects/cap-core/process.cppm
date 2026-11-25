// cap-core process execution contract (M0/T02).
// argv is executed directly via exec; never shell-expanded. See CONTRACTS.md.
//
// shortcut: POSIX-only adapter (Linux stage-0); no descendant process-tree
// termination (v1.4 IProcessTreeController, separate type).
module;

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstring>
#include <expected>
#include <string>
#include <vector>

#include <fcntl.h>
#include <linux/close_range.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

export module cap.core.process;

import cap.core.diagnostic;

namespace cap::core {

export struct ProcessSpec {
  std::vector<std::string> argv;          // argv[0] = program; no shell
  std::string cwd;                        // empty = inherit
  std::vector<std::string> env_allowlist; // empty = empty environment
  std::vector<std::byte> stdin_bytes;     // written to child stdin, then closed
  std::chrono::milliseconds timeout{0};   // zero = no timeout
};

export struct ProcessResult {
  int exit_code = -1; // negative when terminated by signal (-signum)
  std::string stdout_text;
  std::string stderr_text;
  bool timed_out = false;
};

export class IProcessRunner {
public:
  virtual ~IProcessRunner() = default;
  // Infrastructure and known exec failures return diagnostics; child exit
  // outcomes return results.
  virtual Result<ProcessResult> run(const ProcessSpec &spec) const = 0;
};

export class LocalProcessRunner final : public IProcessRunner {
public:
  Result<ProcessResult> run(const ProcessSpec &spec) const override;
};

namespace detail {

// Build the child execve environment from the ambient environment filtered by
// the allowlist before fork so gcov can record the filtering logic.
inline std::vector<char *>
build_envp(const std::vector<std::string> &allowlist) {
  std::vector<char *> envp;
  if (allowlist.empty())
    return envp;
  char **env = ::environ;
  for (; env != nullptr && *env != nullptr; ++env) {
    char *entry = *env;
    const char *eq = std::strchr(entry, '=');
    if (eq == nullptr)
      continue;
    const std::string_view name(entry, static_cast<std::size_t>(eq - entry));
    for (const auto &wanted : allowlist) {
      if (wanted == name) {
        envp.push_back(entry);
        break;
      }
    }
  }
  return envp;
}

inline void report_exec_error(int fd, int error) {
  ssize_t written;
  do {
    written = ::write(fd, &error, sizeof error);
  } while (written < 0 && errno == EINTR);
}

} // namespace detail

Result<ProcessResult> LocalProcessRunner::run(const ProcessSpec &spec) const {
  const auto diagnostic = [](ErrorCode code,
                             std::string message) -> Result<ProcessResult> {
    Diagnostic d;
    d.code = code;
    d.message = std::move(message);
    return std::unexpected<Diagnostic>(d);
  };
  if (spec.argv.empty()) {
    return diagnostic(ErrorCode::invalid_input, "empty argv");
  }
  for (const auto &arg : spec.argv)
    if (arg.find('\0') != std::string::npos)
      return diagnostic(ErrorCode::invalid_input, "NUL in argv");
  if (spec.cwd.find('\0') != std::string::npos)
    return diagnostic(ErrorCode::invalid_input, "NUL in cwd");

  auto local_argv = spec.argv;
  std::vector<char *> argv_ptrs;
  argv_ptrs.reserve(local_argv.size() + 1);
  for (auto &arg : local_argv)
    argv_ptrs.push_back(arg.data());
  argv_ptrs.push_back(nullptr);
  auto envp = detail::build_envp(spec.env_allowlist);
  envp.push_back(nullptr);
  char *const *argv_data = argv_ptrs.data();
  char *const *env_data = envp.data();
  const char *executable = local_argv[0].c_str();
  const char *cwd = spec.cwd.empty() ? nullptr : spec.cwd.c_str();

  const auto start = std::chrono::steady_clock::now();
  int out_pipe[2] = {-1, -1};
  int err_pipe[2] = {-1, -1};
  int in_pipe[2] = {-1, -1};
  int exec_err_pipe[2] = {-1, -1};
  const auto close_fd = [](int &fd) {
    if (fd >= 0)
      ::close(fd);
    fd = -1;
  };
  const auto close_pairs = [&]() {
    for (int *pair : {out_pipe, err_pipe, in_pipe, exec_err_pipe}) {
      close_fd(pair[0]);
      close_fd(pair[1]);
    }
  };
  if (::pipe2(out_pipe, O_CLOEXEC) < 0 || ::pipe2(err_pipe, O_CLOEXEC) < 0 ||
      ::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, in_pipe) < 0 ||
      ::pipe2(exec_err_pipe, O_CLOEXEC) < 0) {
    const int error = errno;
    close_pairs();
    return diagnostic(ErrorCode::transient_io,
                      std::string("pipe: ") + std::strerror(error));
  }
  for (const int *pair : {out_pipe, err_pipe, in_pipe, exec_err_pipe}) {
    if (pair[0] < 3 || pair[1] < 3) {
      close_pairs();
      return diagnostic(ErrorCode::transient_io, "parent stdio is closed");
    }
  }

  const pid_t pid = ::fork();
  if (pid < 0) {
    close_pairs();
    return diagnostic(ErrorCode::transient_io,
                      std::string("fork: ") + std::strerror(errno));
  }
  if (pid == 0) { // child
    ::close(out_pipe[0]);
    ::close(err_pipe[0]);
    ::close(in_pipe[1]);
    ::close(exec_err_pipe[0]); // keep write end for the exec-failure report
    if (::dup2(out_pipe[1], STDOUT_FILENO) < 0 ||
        ::dup2(err_pipe[1], STDERR_FILENO) < 0 ||
        ::dup2(in_pipe[0], STDIN_FILENO) < 0) {
      _exit(125);
    }
    ::close(out_pipe[1]);
    ::close(err_pipe[1]);
    ::close(in_pipe[0]);
    if (cwd != nullptr && ::chdir(cwd) < 0) {
      _exit(126); // reported as plain exit 126; documented
    }
    if (::syscall(SYS_close_range, 3u, UINT_MAX, CLOSE_RANGE_CLOEXEC) < 0) {
      detail::report_exec_error(exec_err_pipe[1], -errno);
      _exit(125);
    }
    ::execve(executable, argv_data, env_data);
    detail::report_exec_error(exec_err_pipe[1], errno);
    _exit(127);
  }
  // parent
  close_fd(out_pipe[1]);
  close_fd(err_pipe[1]);
  close_fd(in_pipe[0]);
  close_fd(exec_err_pipe[1]);

  const auto fail = [&](const char *operation, int error,
                        bool terminate) -> Result<ProcessResult> {
    if (terminate) {
      ::kill(pid, SIGKILL);
      pid_t waited;
      do {
        waited = ::waitpid(pid, nullptr, 0);
      } while (waited < 0 && errno == EINTR);
    }
    close_pairs();
    return diagnostic(ErrorCode::transient_io,
                      std::string(operation) + ": " + std::strerror(error));
  };
  for (int fd : {out_pipe[0], err_pipe[0], in_pipe[1], exec_err_pipe[0]}) {
    const int flags = ::fcntl(fd, F_GETFL);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
      return fail("fcntl", errno, true);
  }
  if (spec.stdin_bytes.empty())
    close_fd(in_pipe[1]);

  ProcessResult result;
  int status = 0;
  std::size_t offset = 0;
  for (;;) {
    const pid_t w = ::waitpid(pid, &status, WNOHANG);
    if (w == pid)
      break;
    if (w < 0 && errno != EINTR)
      return fail("waitpid", errno, false);
    const auto now = std::chrono::steady_clock::now();
    if (spec.timeout.count() > 0 && now - start >= spec.timeout) {
      result.timed_out = true;
      ::kill(pid, SIGKILL);
      pid_t waited;
      do {
        waited = ::waitpid(pid, &status, 0);
      } while (waited < 0 && errno == EINTR);
      if (waited < 0)
        return fail("waitpid", errno, false);
      break;
    }
    // Periodically check waitpid even if descendants keep the streams open.
    int delay = 50;
    if (spec.timeout.count() > 0) {
      const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(
                                 spec.timeout - (now - start))
                                 .count();
      if (remaining < delay)
        delay = static_cast<int>(remaining);
    }
    pollfd fds[] = {{in_pipe[1], POLLOUT, 0},
                    {out_pipe[0], POLLIN, 0},
                    {err_pipe[0], POLLIN, 0}};
    if (::poll(fds, 3, delay) < 0) {
      if (errno == EINTR)
        continue;
      return fail("poll", errno, true);
    }
    if (in_pipe[1] >= 0 && fds[0].revents) {
      const ssize_t n = ::send(in_pipe[1], spec.stdin_bytes.data() + offset,
                               spec.stdin_bytes.size() - offset, MSG_NOSIGNAL);
      if (n > 0)
        offset += static_cast<std::size_t>(n);
      else if (n < 0 && errno != EINTR && errno != EAGAIN &&
               errno != EWOULDBLOCK) {
        if (errno != EPIPE && errno != ECONNRESET)
          return fail("send", errno, true);
        close_fd(in_pipe[1]);
      }
      if (offset == spec.stdin_bytes.size())
        close_fd(in_pipe[1]);
    }
    for (int i = 1; i < 3; ++i) {
      if (!fds[i].revents)
        continue;
      int &fd = i == 1 ? out_pipe[0] : err_pipe[0];
      std::string &output = i == 1 ? result.stdout_text : result.stderr_text;
      char buffer[16384];
      const ssize_t n = ::read(fd, buffer, sizeof buffer);
      if (n > 0)
        output.append(buffer, static_cast<std::size_t>(n));
      else if (n == 0)
        close_fd(fd);
      else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
        return fail("read", errno, true);
    }
  }
  // Snapshot each pipe's buffered bytes: descendants may retain or write it.
  for (int i = 0; i < 2; ++i) {
    int fd = i == 0 ? out_pipe[0] : err_pipe[0];
    if (fd < 0)
      continue;
    int available = 0;
    if (::ioctl(fd, FIONREAD, &available) < 0)
      return fail("ioctl", errno, false);
    std::string &output = i == 0 ? result.stdout_text : result.stderr_text;
    char buffer[16384];
    while (available > 0) {
      const auto amount =
          std::min(static_cast<std::size_t>(available), sizeof buffer);
      const ssize_t n = ::read(fd, buffer, amount);
      if (n > 0) {
        output.append(buffer, static_cast<std::size_t>(n));
        available -= static_cast<int>(n);
      } else if (n < 0 && errno == EINTR)
        continue;
      else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        return fail("read", errno, false);
      else
        break;
    }
  }
  int exec_errno = 0;
  ssize_t exec_err_read;
  do {
    exec_err_read = ::read(exec_err_pipe[0], &exec_errno, sizeof exec_errno);
  } while (exec_err_read < 0 && errno == EINTR);
  if (exec_err_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    return fail("read", errno, false);
  close_pairs();
  if (WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.exit_code = -WTERMSIG(status);
  }
  if (exec_err_read == static_cast<ssize_t>(sizeof exec_errno)) {
    if (exec_errno < 0)
      return diagnostic(ErrorCode::unsupported_capability,
                        std::string("close_range: ") +
                            std::strerror(-exec_errno));
    Diagnostic d;
    d.code = exec_errno == ENOENT   ? ErrorCode::missing_resource
             : exec_errno == EACCES ? ErrorCode::unsupported_capability
                                    : ErrorCode::internal;
    d.message = spec.argv[0] + ": exec failed: " + std::strerror(exec_errno);
    return std::unexpected<Diagnostic>(d);
  }
  return result;
}

} // namespace cap::core
