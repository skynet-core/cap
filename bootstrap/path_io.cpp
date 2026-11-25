module;

#include <expected>
#include <string>

module cap.bootstrap.path_io;

import cap.core.diagnostic;
import cap.core.filesystem;

namespace cap::bootstrap {
cap::core::Result<std::string> canonicalize(const std::string &path) {
  return cap::core::LocalFileSystem().canonicalize(path);
}

cap::core::Status check_readable(const std::string &path) {
  auto result = cap::core::LocalFileSystem().read(path);
  if (!result)
    return std::unexpected(result.error());
  return {};
}
} // namespace cap::bootstrap
