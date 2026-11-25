module;

#include <string>

export module amp.bootstrap.path_io;

import amp.core.diagnostic;

export namespace amp::bootstrap {
amp::core::Result<std::string> canonicalize(const std::string &path);
amp::core::Status check_readable(const std::string &path);
} // namespace amp::bootstrap
