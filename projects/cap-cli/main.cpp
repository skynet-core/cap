#include <iostream>
#include <span>
#include <string_view>
#include <vector>

import cap.core.diagnostic;
import cap.cli.standalone;

/// Dispatch the build command and render help, paths or a diagnostic.
/// @param[in] argc int — Number of process argument pointers.
/// @param[in] argv char** — Process arguments, valid through the call.
/// @return int — Zero on success, one on invalid command or build diagnostic.
/// @throws std::bad_alloc If constructing argument views or results fails.
/// @throws std::exception If a dependency throws an untranslated exception.
int main(int argc, char **argv) {
  if (argc < 2 || std::string_view(argv[1]) != "build") {
    cap::core::Diagnostic error;
    error.code = cap::core::ErrorCode::invalid_input;
    error.message = "expected build command";
    std::cerr << error.format() << '\n';
    return 1;
  }
  std::vector<std::string_view> args;
  for (int i = 2; i < argc; ++i)
    args.emplace_back(argv[i]);
  auto result = cap::cli::run_standalone(args);
  if (!result) {
    std::cerr << result.error().format() << '\n';
    return 1;
  }
  if (!result->help.empty())
    std::cout << result->help;
  else {
    for (const auto &artifact : result->artifacts)
      std::cout << "artifact: " << artifact << '\n';
    std::cout << "compdb: " << result->compdb << '\n';
  }
}
