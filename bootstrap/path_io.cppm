module;

#include <string>

export module cap.bootstrap.path_io;

import cap.core.diagnostic;

export namespace cap::bootstrap {
cap::core::Result<std::string> canonicalize(const std::string &path);
cap::core::Status check_readable(const std::string &path);
} // namespace cap::bootstrap
