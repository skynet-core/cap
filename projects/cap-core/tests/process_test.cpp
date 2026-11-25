// Contract test for cap-core process execution (M0/T02).
// Uses host utilities (echo, cat, pwd, sleep) as fixtures; the runner itself
// never invokes a shell.
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <csignal>
#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>

#include "test_util.hpp"

import cap.core.diagnostic;
import cap.core.process;

namespace {

// Each task of the current uid (process and thread alike) appears as a
// numeric /proc entry; count them via the Uid line of /proc/<pid>/status.
std::size_t count_uid_tasks() {
  const std::string uid = std::to_string(::getuid());
  std::size_t count = 0;
  std::error_code ec;
  for (const auto &entry : std::filesystem::directory_iterator("/proc", ec)) {
    const std::string name = entry.path().filename().string();
    if (name.empty() ||
        name.find_first_not_of("0123456789") != std::string::npos) {
      continue;
    }
    std::ifstream status(entry.path().string() + "/status");
    std::string line;
    while (std::getline(status, line)) {
      if (line.rfind("Uid:", 0) != 0)
        continue;
      std::istringstream fields(line.substr(4));
      std::string real_uid;
      fields >> real_uid;
      if (real_uid == uid)
        ++count;
      break;
    }
  }
  return count;
}

struct rlimit g_saved_limit{0, 0};
bool g_limit_engaged = false;

// Lower RLIMIT_NPROC to the current task count so the next fork fails with
// EAGAIN. Restored by restore_forks(); the limit dies with the process.
bool exhaust_forks() {
  if (::getrlimit(RLIMIT_NPROC, &g_saved_limit) != 0)
    return false;
  const std::size_t tasks = count_uid_tasks();
  if (tasks == 0 || tasks > g_saved_limit.rlim_max)
    return false;
  // Lower on a copy; g_saved_limit must keep the original cur for restore.
  struct rlimit low = g_saved_limit;
  low.rlim_cur = tasks;
  if (::setrlimit(RLIMIT_NPROC, &low) != 0)
    return false;
  g_limit_engaged = true;
  return true;
}

void restore_forks() {
  if (g_limit_engaged)
    ::setrlimit(RLIMIT_NPROC, &g_saved_limit);
}
} // namespace

using cap::core::LocalProcessRunner;
using cap::core::ProcessResult;
using cap::core::ProcessSpec;
using cap::core::Result;

namespace {
Result<ProcessResult> run(LocalProcessRunner &runner, ProcessSpec spec) {
  return runner.run(spec);
}
} // namespace

int main(int argc, char **argv) {
  if (argc == 3 && std::string(argv[1]) == "--cap-test-closed-stdio") {
    const int fd = argv[2][0] - '0';
    if (fd < 0 || fd > 2 || ::close(fd) != 0)
      return 1;
    ProcessSpec spec;
    spec.argv = {"/bin/true"};
    auto result = LocalProcessRunner().run(spec);
    return !result &&
                   result.error().code == cap::core::ErrorCode::transient_io &&
                   result.error().message == "parent stdio is closed" &&
                   ::fcntl(fd, F_GETFD) < 0 && errno == EBADF
               ? 0
               : 1;
  }
  if (argc == 2 && std::string(argv[1]) == "--cap-test-check-fds") {
    for (int fd = 3; fd < 128; ++fd)
      if (::fcntl(fd, F_GETFD) >= 0 || errno != EBADF)
        return 1;
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--cap-test-isolated-fds") {
    for (int fd = 3; fd < 128; ++fd)
      ::close(fd);
    int extra[2];
    if (::pipe(extra) != 0)
      return 1;
    ProcessSpec spec;
    spec.argv = {"/proc/self/exe", "--cap-test-check-fds"};
    auto result = LocalProcessRunner().run(spec);
    ::close(extra[0]);
    ::close(extra[1]);
    return result && result->exit_code == 0 ? 0 : 1;
  }
  if (argc == 2 && std::string(argv[1]) == "--cap-test-reap-error") {
    if (std::signal(SIGCHLD, SIG_IGN) == SIG_ERR)
      return 1;
    ProcessSpec spec;
    spec.argv = {"/bin/true"};
    auto result = LocalProcessRunner().run(spec);
    return !result &&
                   result.error().code == cap::core::ErrorCode::transient_io &&
                   result.error().message.rfind("waitpid:", 0) == 0
               ? 0
               : 1;
  }
  if (argc == 2 && std::string(argv[1]) == "--cap-test-raise-sigterm") {
    std::raise(SIGTERM);
    return 99;
  }
  if (argc == 2 && std::string(argv[1]) == "--cap-test-streams") {
    const std::string out(4096, 'o');
    const std::string err(4096, 'e');
    for (int i = 0; i < 64; ++i) {
      if (::write(STDOUT_FILENO, out.data(), out.size()) !=
              static_cast<ssize_t>(out.size()) ||
          ::write(STDERR_FILENO, err.data(), err.size()) !=
              static_cast<ssize_t>(err.size()))
        return 1;
    }
    char buffer[4096];
    std::size_t total = 0;
    ssize_t n;
    while ((n = ::read(STDIN_FILENO, buffer, sizeof buffer)) > 0)
      total += static_cast<std::size_t>(n);
    return n == 0 && total == 1024 * 1024 ? 0 : 1;
  }
  if (argc == 2 && std::string(argv[1]) == "--cap-test-descendant") {
    const pid_t child = ::fork();
    if (child < 0)
      return 1;
    if (child == 0) {
      ::sleep(1);
      _exit(0);
    }
    return ::write(STDOUT_FILENO, "done", 4) == 4 ? 0 : 1;
  }

  // Guard the no-timeout regression against hanging the entire test job.
  ::alarm(20);

  LocalProcessRunner runner;

  for (const char *fd : {"0", "1", "2"}) {
    ProcessSpec closed_stdio;
    closed_stdio.argv = {"/proc/self/exe", "--cap-test-closed-stdio", fd};
    auto result = run(runner, closed_stdio);
    EXPECT_TRUE(static_cast<bool>(result));
    if (result)
      EXPECT_EQ(result->exit_code, 0);
  }

  // SIGCHLD is ignored only in the fixture, forcing its waitpid to fail.
  ProcessSpec reap_error;
  reap_error.argv = {"/proc/self/exe", "--cap-test-reap-error"};
  auto r_reap_error = run(runner, reap_error);
  EXPECT_TRUE(static_cast<bool>(r_reap_error));
  if (r_reap_error)
    EXPECT_EQ(r_reap_error->exit_code, 0);

  ProcessSpec isolated_fds;
  isolated_fds.argv = {"/proc/self/exe", "--cap-test-isolated-fds"};
  auto r_isolated_fds = run(runner, isolated_fds);
  EXPECT_TRUE(static_cast<bool>(r_isolated_fds));
  if (r_isolated_fds)
    EXPECT_EQ(r_isolated_fds->exit_code, 0);

  // basic run: exit code and stdout capture
  ProcessSpec echo;
  echo.argv = {"/bin/echo", "hello"};
  auto r1 = run(runner, echo);
  EXPECT_TRUE(static_cast<bool>(r1));
  if (r1) {
    EXPECT_EQ(r1->exit_code, 0);
    EXPECT_EQ(r1->stdout_text, "hello\n");
    EXPECT_TRUE(!r1->timed_out);
  }

  // missing program -> missing_resource diagnostic
  ProcessSpec missing;
  missing.argv = {"/no/such/cap-test-binary", "x"};
  auto r2 = run(runner, missing);
  EXPECT_TRUE(!r2);
  EXPECT_EQ(cap::core::error_code_name(r2.error().code), "missing-resource");

  // empty argv -> invalid_input diagnostic
  ProcessSpec empty;
  auto r3 = run(runner, empty);
  EXPECT_TRUE(!r3);
  EXPECT_EQ(cap::core::error_code_name(r3.error().code), "invalid-input");

  ProcessSpec nul;
  nul.argv = {std::string("/bin/true\0ignored", 17)};
  auto nul_executable = run(runner, nul);
  EXPECT_TRUE(!nul_executable);
  if (!nul_executable)
    EXPECT_TRUE(nul_executable.error().code ==
                cap::core::ErrorCode::invalid_input);
  nul.argv = {"/bin/true", std::string("arg\0ignored", 11)};
  auto nul_argument = run(runner, nul);
  EXPECT_TRUE(!nul_argument);
  if (!nul_argument)
    EXPECT_TRUE(nul_argument.error().code ==
                cap::core::ErrorCode::invalid_input);
  nul.argv = {"/bin/true"};
  nul.cwd = std::string("/tmp\0ignored", 12);
  auto nul_cwd = run(runner, nul);
  EXPECT_TRUE(!nul_cwd);
  if (!nul_cwd)
    EXPECT_TRUE(nul_cwd.error().code == cap::core::ErrorCode::invalid_input);

  // non-zero exit propagates
  ProcessSpec nonzero;
  nonzero.argv = {"/bin/sh", "-c", "exit 3"};
  auto r4 = run(runner, nonzero);
  EXPECT_TRUE(static_cast<bool>(r4));
  if (r4)
    EXPECT_EQ(r4->exit_code, 3);

  // environment: empty allowlist -> empty env; allowlist -> filtered env
  ProcessSpec env_empty;
  env_empty.argv = {"/usr/bin/env"};
  auto r5 = run(runner, env_empty);
  EXPECT_TRUE(static_cast<bool>(r5));
  if (r5)
    EXPECT_EQ(r5->stdout_text, "");
  ProcessSpec env_path;
  env_path.argv = {"/usr/bin/env"};
  env_path.env_allowlist = {"PATH"};
  auto r6 = run(runner, env_path);
  EXPECT_TRUE(static_cast<bool>(r6));
  if (r6)
    EXPECT_TRUE(r6->stdout_text.rfind("PATH=", 0) == 0);

  // timeout: child killed well before it would finish
  ProcessSpec sleeper;
  sleeper.argv = {"/bin/sleep", "5"};
  sleeper.timeout = std::chrono::milliseconds(200);
  const auto start = std::chrono::steady_clock::now();
  auto r7 = run(runner, sleeper);
  const auto elapsed = std::chrono::steady_clock::now() - start;
  EXPECT_TRUE(static_cast<bool>(r7));
  if (r7)
    EXPECT_TRUE(r7->timed_out);
  EXPECT_TRUE(elapsed < std::chrono::seconds(3));

  // A direct argv child terminates itself; the runner must preserve signal
  // status.
  ProcessSpec signaled;
  signaled.argv = {"/proc/self/exe", "--cap-test-raise-sigterm"};
  auto r_signal = run(runner, signaled);
  EXPECT_TRUE(static_cast<bool>(r_signal));
  if (r_signal) {
    EXPECT_EQ(r_signal->exit_code, -SIGTERM);
    EXPECT_TRUE(!r_signal->timed_out);
  }

  // stdin forwarding
  ProcessSpec cat;
  cat.argv = {"/bin/cat"};
  cat.stdin_bytes.push_back(std::byte{'h'});
  cat.stdin_bytes.push_back(std::byte{'i'});
  auto r8 = run(runner, cat);
  EXPECT_TRUE(static_cast<bool>(r8));
  if (r8)
    EXPECT_EQ(r8->stdout_text, "hi");

  ProcessSpec early;
  early.argv = {"/bin/true"};
  early.stdin_bytes.resize(1024 * 1024, std::byte{'x'});
  auto r_early = run(runner, early);
  EXPECT_TRUE(static_cast<bool>(r_early));
  if (r_early)
    EXPECT_EQ(r_early->exit_code, 0);

  ProcessSpec blocked;
  blocked.argv = {"/bin/sleep", "5"};
  blocked.stdin_bytes = early.stdin_bytes;
  blocked.timeout = std::chrono::milliseconds(200);
  const auto blocked_start = std::chrono::steady_clock::now();
  auto r_blocked = run(runner, blocked);
  EXPECT_TRUE(static_cast<bool>(r_blocked));
  if (r_blocked) {
    EXPECT_TRUE(r_blocked->timed_out);
    EXPECT_EQ(r_blocked->exit_code, -SIGKILL);
  }
  EXPECT_TRUE(std::chrono::steady_clock::now() - blocked_start <
              std::chrono::seconds(3));

  ProcessSpec streams;
  streams.argv = {"/proc/self/exe", "--cap-test-streams"};
  streams.stdin_bytes = early.stdin_bytes;
  auto r_streams = run(runner, streams);
  EXPECT_TRUE(static_cast<bool>(r_streams));
  if (r_streams) {
    EXPECT_EQ(r_streams->exit_code, 0);
    EXPECT_EQ(r_streams->stdout_text, std::string(64 * 4096, 'o'));
    EXPECT_EQ(r_streams->stderr_text, std::string(64 * 4096, 'e'));
  }

  ProcessSpec descendant;
  descendant.argv = {"/proc/self/exe", "--cap-test-descendant"};
  const auto descendant_start = std::chrono::steady_clock::now();
  auto r_descendant = run(runner, descendant);
  EXPECT_TRUE(static_cast<bool>(r_descendant));
  if (r_descendant)
    EXPECT_EQ(r_descendant->stdout_text, "done");
  EXPECT_TRUE(std::chrono::steady_clock::now() - descendant_start <
              std::chrono::milliseconds(800));

  // cwd override
  ProcessSpec pwd;
  pwd.argv = {"/bin/pwd"};
  pwd.cwd = "/tmp";
  auto r9 = run(runner, pwd);
  EXPECT_TRUE(static_cast<bool>(r9));
  if (r9)
    EXPECT_EQ(r9->stdout_text, "/tmp\n");

  // Exact-name filtering must not leak similarly prefixed environment keys.
  EXPECT_EQ(::setenv("CAP_PROCESS_TEST", "allowed", 1), 0);
  EXPECT_EQ(::setenv("CAP_PROCESS_TEST_EXTRA", "secret", 1), 0);
  ProcessSpec filtered;
  filtered.argv = {"/usr/bin/env"};
  filtered.env_allowlist = {"CAP_PROCESS_TEST"};
  auto filtered_result = run(runner, filtered);
  EXPECT_TRUE(static_cast<bool>(filtered_result));
  if (filtered_result)
    EXPECT_EQ(filtered_result->stdout_text, "CAP_PROCESS_TEST=allowed\n");
  ::unsetenv("CAP_PROCESS_TEST");
  ::unsetenv("CAP_PROCESS_TEST_EXTRA");

  // Fault injection: exec EACCES (file exists, not executable)
  const std::filesystem::path nonexec =
      std::filesystem::temp_directory_path() /
      ("cap-nonexec-" + std::to_string(::getpid()));
  {
    std::ofstream out(nonexec);
    out << "never executed\n";
  }
  std::error_code pec;
  std::filesystem::permissions(nonexec, std::filesystem::perms::owner_read,
                               pec);
  ProcessSpec noexec;
  noexec.argv = {nonexec.string()};
  auto r10 = run(runner, noexec);
  EXPECT_TRUE(!r10);
  EXPECT_EQ(cap::core::error_code_name(r10.error().code),
            "unsupported-capability");
  std::filesystem::remove(nonexec, pec);

  // Fault injection: fork failure via RLIMIT_NPROC exhaustion
  if (exhaust_forks()) {
    ProcessSpec forkfail;
    forkfail.argv = {"/bin/true"};
    auto r11 = run(runner, forkfail);
    EXPECT_TRUE(!r11);
    EXPECT_EQ(cap::core::error_code_name(r11.error().code), "transient-io");
    restore_forks();
  } else {
    std::cerr << "SKIP: fork-exhaustion fault unavailable (getrlimit failed)\n";
  }

  return cap_test::summary("cap-core process");
}
