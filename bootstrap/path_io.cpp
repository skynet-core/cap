module;

#include <expected>
#include <string>

module amp.bootstrap.path_io;

import amp.core.diagnostic;
import amp.core.filesystem;

namespace amp::bootstrap {
amp::core::Result<std::string> canonicalize(const std::string &path) {
  return amp::core::LocalFileSystem().canonicalize(path);
}

amp::core::Status check_readable(const std::string &path) {
  auto result = amp::core::LocalFileSystem().read(path);
  if (!result)
    return std::unexpected(result.error());
  return {};
}
} // namespace amp::bootstrap
