// cap stage-1 executable — compiled by the stage-0 bootstrap driver
// (bootstrap/main.cpp). M0 exit artifact: a real binary built without
// CMake/Meson/Ninja, linking the pinned installed Boost.
//
// Deterministic behavior: with the same --boost-prefix the output JSON is
// byte-identical across runs.
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/json.hpp>
#include <boost/program_options.hpp>
#include <boost/version.hpp>

import cap.core.diagnostic;
import cap.core.digest;
import cap.bootstrap.path_io;

namespace {

int self_check(const boost::filesystem::path &boost_prefix) {
  // The installed Boost tree must expose a readable version.hpp.
  const auto header = cap::bootstrap::check_readable(
      (boost_prefix / "include" / "boost" / "version.hpp").string());
  if (!header) {
    std::printf("stage1: %s\n", header.error().format().c_str());
    return 1;
  }
  // cap-core digest sanity against the published empty-input vector.
  const std::vector<std::byte> empty;
  const auto digest = cap::core::Digest::from_bytes(empty);
  if (digest.hex() !=
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") {
    cap::core::Diagnostic d;
    d.code = cap::core::ErrorCode::integrity_failure;
    d.message = "cap-core SHA-256 self-check failed";
    std::printf("stage1: %s\n", d.format().c_str());
    return 2;
  }
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  boost::program_options::options_description desc("cap stage-1");
  desc.add_options()("boost-prefix",
                     boost::program_options::value<std::string>(),
                     "installed Boost prefix")(
      "version", "print the stage-1 status JSON")("help,h", "show this help");
  boost::program_options::variables_map vm;
  boost::program_options::store(
      boost::program_options::parse_command_line(argc, argv, desc), vm);
  boost::program_options::notify(vm);
  if (vm.count("help")) {
    std::cout << desc << '\n';
    return 0;
  }
  if (!vm.count("boost-prefix")) {
    std::printf("stage1: ERROR: --boost-prefix is required\n");
    return 3;
  }

  const boost::filesystem::path prefix =
      boost::filesystem::path(vm.at("boost-prefix").as<std::string>());
  const int rc = self_check(prefix);
  if (rc != 0)
    return rc;

  boost::json::object status;
  status["stage"] = 1;
  status["cap_core"] = "ok";
  status["boost"] = BOOST_LIB_VERSION;
  status["self_check"] = "passed";
  std::printf("%s\n", boost::json::serialize(status).c_str());
  return 0;
}
