// amp-core filesystem contract (M0/T02). Narrow injected IO; see CONTRACTS.md.
//
// shortcut: POSIX-only adapter (Linux stage-0). Split into _linux/_windows/
// _darwin translation units when the first non-POSIX host is targeted.
module;

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <system_error>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

export module amp.core.filesystem;

import amp.core.diagnostic;

namespace amp::core {

/// Narrow filesystem boundary for byte reads, atomic writes, directory
/// enumeration, and canonical path resolution. Implementations return
/// structured diagnostics for operational failures; inputs and returned values
/// are owned by the caller except for the duration of a write span.
export class IFileSystem {
public:
  /// Destroys the filesystem interface object.
  /// @return No value.
  /// @note Does not throw.
  virtual ~IFileSystem() = default;
  /// Reads all bytes from an existing regular file, following symlinks.
  /// The path must remain stable during the metadata check and read; this is
  /// not a race-proof sandbox.
  /// @param[in] path const std::string& — Filesystem path to read.
  /// @return Result<std::vector<std::byte>> — File bytes, or a diagnostic:
  ///         missing_resource for a missing path, unsupported_capability for
  ///         a directory or other nonregular path, and transient_io for IO
  ///         failures.
  /// @throws std::bad_alloc if path or result storage allocation fails.
  /// @throws std::length_error if the byte vector exceeds its maximum size.
  virtual Result<std::vector<std::byte>>
  read(const std::string &path) const = 0;
  /// Atomically replaces the target according to the implementation's
  /// documented storage and durability guarantees. Filesystem paths must
  /// remain stable during the operation; this interface is not a sandbox.
  /// @param[in] path const std::string& — Target path to replace.
  /// @param[in] bytes std::span<const std::byte> — Payload to write; the
  ///            caller retains ownership for the duration of the call.
  /// @return Status — Success, or an implementation diagnostic describing a
  ///         missing resource, unsupported operation, or transient storage
  ///         failure. Implementations document their exact error mapping.
  /// @throws std::bad_alloc if path or diagnostic storage allocation fails.
  /// @throws std::length_error if path or diagnostic storage exceeds its
  ///         maximum size.
  virtual Status write_atomic(const std::string &path,
                              std::span<const std::byte> bytes) const = 0;
  /// Returns immediate child names in deterministic lexicographic order.
  /// @param[in] path const std::string& — Directory path to enumerate.
  /// @return Result<std::vector<std::string>> — Sorted child names, or
  ///         missing_resource when the path is missing, invalid_input when an
  ///         existing path is not a directory, and transient_io for directory
  ///         iteration setup failures.
  /// @throws std::bad_alloc if path or result storage allocation fails.
  /// @throws std::length_error if result storage exceeds its maximum size.
  /// @throws std::filesystem::filesystem_error if incrementing the directory
  ///         iterator fails after successful setup.
  virtual Result<std::vector<std::string>>
  enumerate(const std::string &path) const = 0;
  /// Resolves an existing path to its absolute, symlink-resolved form.
  /// @param[in] path const std::string& — Existing path to resolve.
  /// @return Result<std::string> — Canonical path, or missing_resource when
  ///         resolution fails, including for inaccessible paths.
  /// @throws std::bad_alloc if path or result storage allocation fails.
  /// @throws std::length_error if path or result storage exceeds its maximum
  ///         size.
  virtual Result<std::string> canonicalize(const std::string &path) const = 0;
};

/// POSIX local filesystem adapter implementing regular-file reads, atomic
/// replacement, sorted directory enumeration, and canonicalization.
/// Operations use the host filesystem and assume paths remain stable where
/// noted by individual methods; this adapter is not a sandbox.
export class LocalFileSystem final : public IFileSystem {
public:
  /// Reads all bytes from an existing regular file, following symlinks.
  /// The path must remain stable during the metadata check and read; this is
  /// not a race-proof sandbox.
  /// @param[in] path const std::string& — Filesystem path to read.
  /// @return Result<std::vector<std::byte>> — File bytes, or a diagnostic:
  ///         missing_resource for a missing path, unsupported_capability for
  ///         a directory or other nonregular path, and transient_io for IO
  ///         failures.
  /// @throws std::bad_alloc if path or result storage allocation fails.
  /// @throws std::length_error if the byte vector exceeds its maximum size.
  Result<std::vector<std::byte>> read(const std::string &path) const override;
  /// Replaces the target using a unique same-directory temporary file, file
  /// fsync, close, and rename. The target path must remain stable during the
  /// operation; this is not a race-proof sandbox. The parent directory is not
  /// fsynced, so rename durability across a crash is not guaranteed.
  /// @param[in] path const std::string& — Target path to replace.
  /// @param[in] bytes std::span<const std::byte> — Payload; caller-owned for
  ///            the duration of the call.
  /// @return Status — Success, missing_resource for a missing parent,
  ///         unsupported_capability when payload size exceeds ssize_t max, or
  ///         transient_io for temp open, write, file fsync, close, or rename
  ///         failure.
  /// @throws std::bad_alloc if path, temporary-name, or diagnostic storage
  ///         allocation fails.
  /// @throws std::length_error if temporary-name or diagnostic storage exceeds
  ///         its maximum size.
  Status write_atomic(const std::string &path,
                      std::span<const std::byte> bytes) const override;
  /// Lists immediate child names in lexicographic order.
  /// @param[in] path const std::string& — Directory to enumerate.
  /// @return Result<std::vector<std::string>> — Sorted names, or
  ///         missing_resource for a missing path, invalid_input for an
  ///         existing nondirectory, and transient_io if directory iterator
  ///         construction reports an error through its error_code overload.
  /// @throws std::bad_alloc if path or result storage allocation fails.
  /// @throws std::length_error if result storage exceeds its maximum size.
  /// @throws std::filesystem::filesystem_error if incrementing the directory
  ///         iterator fails after successful construction.
  Result<std::vector<std::string>>
  enumerate(const std::string &path) const override;
  /// Resolves a path to its absolute, symlink-resolved form.
  /// @param[in] path const std::string& — Existing path to resolve.
  /// @return Result<std::string> — Canonical path, or missing_resource on
  ///         resolution failure, including inaccessible paths.
  /// @throws std::bad_alloc if path or result storage allocation fails.
  /// @throws std::length_error if path or result storage exceeds its maximum
  ///         size.
  Result<std::string> canonicalize(const std::string &path) const override;
};

Result<std::vector<std::byte>>
LocalFileSystem::read(const std::string &path) const {
  std::error_code ec;
  const std::filesystem::path p(path);
  const std::filesystem::file_status status = std::filesystem::status(p, ec);
  if (ec && ec != std::errc::no_such_file_or_directory &&
      ec != std::errc::not_a_directory) {
    Diagnostic d;
    d.code = ErrorCode::transient_io;
    d.message = path + ": " + ec.message();
    return std::unexpected<Diagnostic>(d);
  }
  if (ec || !std::filesystem::exists(status)) {
    Diagnostic d;
    d.code = ErrorCode::missing_resource;
    d.message = path + ": no such file";
    return std::unexpected<Diagnostic>(d);
  }
  if (std::filesystem::is_directory(status)) {
    Diagnostic d;
    d.code = ErrorCode::unsupported_capability;
    d.message = path + ": read of a directory";
    d.remediation = "use enumerate() to list directory contents";
    return std::unexpected<Diagnostic>(d);
  }
  if (!std::filesystem::is_regular_file(status)) {
    Diagnostic d;
    d.code = ErrorCode::unsupported_capability;
    d.message = path + ": read of a non-regular file";
    return std::unexpected<Diagnostic>(d);
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    Diagnostic d;
    d.code = ErrorCode::transient_io;
    d.message = path + ": open failed";
    return std::unexpected<Diagnostic>(d);
  }
  in.seekg(0, std::ios::end);
  const std::streamoff size = in.tellg();
  in.seekg(0, std::ios::beg);
  std::vector<std::byte> buffer(static_cast<std::size_t>(size < 0 ? 0 : size));
  if (size >= 0) {
    in.read(reinterpret_cast<char *>(buffer.data()), size);
  }
  if (size < 0 || static_cast<std::size_t>(in.gcount()) != buffer.size()) {
    Diagnostic d;
    d.code = ErrorCode::transient_io;
    d.message = path + ": short read";
    return std::unexpected<Diagnostic>(d);
  }
  return buffer;
}

Status LocalFileSystem::write_atomic(const std::string &path,
                                     std::span<const std::byte> bytes) const {
  std::error_code ec;
  const std::filesystem::path target(path);
  const std::filesystem::path dir = target.parent_path();
  // libstdc++ sets ec to ENOENT for missing paths in is_directory.
  if (!dir.empty() && !std::filesystem::is_directory(dir, ec)) {
    Diagnostic d;
    d.code = (ec && ec != std::errc::no_such_file_or_directory)
                 ? ErrorCode::transient_io
                 : ErrorCode::missing_resource;
    d.message = path + (ec ? (": " + ec.message()) : ": no such directory");
    return std::unexpected<Diagnostic>(d);
  }
  if (bytes.size() >
      static_cast<std::size_t>(std::numeric_limits<ssize_t>::max())) {
    Diagnostic d;
    d.code = ErrorCode::unsupported_capability;
    d.message = path + ": payload larger than ssize_max";
    return std::unexpected<Diagnostic>(d);
  }
  static std::atomic<int> sequence{0};
  const std::string suffix = ".amp-tmp-" + std::to_string(::getpid()) + "-" +
                             std::to_string(sequence++);
  std::filesystem::path tmp;
  if (dir.empty()) {
    tmp = target.filename().string() + suffix;
  } else {
    tmp = dir / (target.filename().string() + suffix);
  }
  const int fd =
      ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
  if (fd < 0) {
    const int err = errno;
    Diagnostic d;
    d.code = ErrorCode::transient_io;
    d.message = path + ": temp open failed: " + std::strerror(err);
    return std::unexpected<Diagnostic>(d);
  }
  std::size_t offset = 0;
  bool ok = true;
  while (offset < bytes.size()) {
    const ssize_t n = ::write(fd, bytes.data() + offset, bytes.size() - offset);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      ok = false;
      break;
    }
    offset += static_cast<std::size_t>(n);
  }
  if (ok && ::fsync(fd) < 0)
    ok = false; // durability: file fsync; directory fsync omitted
  const int close_err = ::close(fd);
  if (!ok || close_err < 0) {
    Diagnostic d;
    d.code = ErrorCode::transient_io;
    d.message = path + ": temp write/fsync failed";
    ::unlink(tmp.c_str());
    return std::unexpected<Diagnostic>(d);
  }
  std::error_code rename_ec;
  std::filesystem::rename(tmp, target, rename_ec);
  if (rename_ec) {
    ::unlink(tmp.c_str());
    Diagnostic d;
    d.code = ErrorCode::transient_io;
    d.message = path + ": atomic rename failed: " + rename_ec.message();
    return std::unexpected<Diagnostic>(d);
  }
  return {};
}

Result<std::vector<std::string>>
LocalFileSystem::enumerate(const std::string &path) const {
  std::error_code ec;
  if (!std::filesystem::is_directory(path, ec)) {
    std::error_code exists_ec;
    const bool present = std::filesystem::exists(path, exists_ec);
    Diagnostic d;
    d.code = !present ? ErrorCode::missing_resource : ErrorCode::invalid_input;
    d.message = path + (ec ? (": " + ec.message()) : ": not a directory");
    return std::unexpected<Diagnostic>(d);
  }
  std::vector<std::string> names;
  for (const auto &entry : std::filesystem::directory_iterator(path, ec)) {
    names.push_back(entry.path().filename().string());
  }
  if (ec) {
    Diagnostic d;
    d.code = ErrorCode::transient_io;
    d.message = path + ": " + ec.message();
    return std::unexpected<Diagnostic>(d);
  }
  std::sort(names.begin(), names.end());
  return names;
}

Result<std::string>
LocalFileSystem::canonicalize(const std::string &path) const {
  std::error_code ec;
  const std::filesystem::path resolved = std::filesystem::canonical(path, ec);
  if (ec) {
    Diagnostic d;
    d.code = ErrorCode::missing_resource;
    d.message = path + ": " + ec.message();
    return std::unexpected<Diagnostic>(d);
  }
  return resolved.string();
}

} // namespace amp::core
