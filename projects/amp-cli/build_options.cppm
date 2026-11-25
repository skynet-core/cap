module;
#include <cstddef>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module amp.cli.build_options;
import amp.core.diagnostic;
import amp.workspace;
import amp.manifest;

export namespace amp::cli {
struct BuildOptions {
  std::string cc, cxx, ar;
  std::optional<std::string> manifest_path, build_root, package, bin;
  bool workspace = false, lib = false, help = false;
};
struct TargetId {
  std::size_t member;
  std::optional<std::size_t> bin;
  bool operator==(const TargetId &) const = default;
};
core::Result<BuildOptions>
parse_build_options(std::span<const std::string_view> args);
core::Result<std::vector<TargetId>>
select_build_targets(const workspace::Workspace &workspace,
                     const BuildOptions &options);
/// Return the standalone build usage text without performing IO.
/// @return std::string_view — Static text valid for the process lifetime.
/// @note No failure result; does not throw.
std::string_view build_help();
} // namespace amp::cli

namespace amp::cli {
namespace {
core::Diagnostic invalid(std::string message) {
  return {core::ErrorCode::invalid_input,
          core::Severity::error,
          std::move(message),
          {},
          {},
          {}};
}
} // namespace

std::string_view build_help() {
  return "Build standalone targets or selected workspace targets:\n"
         "--cc PATH --cxx PATH --ar PATH [--manifest-path PATH] "
         "[--build-root PATH] [--package NAME] "
         "[--bin NAME | --lib] [--workspace]\n"
         "Workspace builds include required local static libraries.\n"
         "--help\n";
}

core::Result<BuildOptions>
parse_build_options(std::span<const std::string_view> args) {
  BuildOptions out;
  if (args.size() == 1 && args[0] == "--help") {
    out.help = true;
    return out;
  }
  for (std::size_t i = 0; i < args.size(); ++i) {
    auto flag = args[i];
    if (flag == "--workspace" || flag == "--lib") {
      bool &value = flag == "--workspace" ? out.workspace : out.lib;
      if (value)
        return std::unexpected(invalid("repeated flag: " + std::string(flag)));
      value = true;
      continue;
    }
    std::string *tool = nullptr;
    std::optional<std::string> *value = nullptr;
    if (flag == "--cc")
      tool = &out.cc;
    else if (flag == "--cxx")
      tool = &out.cxx;
    else if (flag == "--ar")
      tool = &out.ar;
    else if (flag == "--manifest-path")
      value = &out.manifest_path;
    else if (flag == "--build-root")
      value = &out.build_root;
    else if (flag == "--package")
      value = &out.package;
    else if (flag == "--bin")
      value = &out.bin;
    else
      return std::unexpected(invalid("unknown flag: " + std::string(flag)));
    if ((tool && !tool->empty()) || (value && value->has_value()))
      return std::unexpected(invalid("repeated flag: " + std::string(flag)));
    if (++i == args.size() || args[i].empty() ||
        args[i].find('\0') != std::string_view::npos ||
        args[i].starts_with("--"))
      return std::unexpected(
          invalid("missing or invalid value: " + std::string(flag)));
    if (tool) {
      if (!std::filesystem::path(args[i]).is_absolute())
        return std::unexpected(
            invalid("tool path must be absolute: " + std::string(flag)));
      *tool = args[i];
    } else {
      *value = std::string(args[i]);
    }
  }
  if (out.cc.empty() || out.cxx.empty() || out.ar.empty())
    return std::unexpected(invalid("--cc, --cxx and --ar are required"));
  if ((out.package && out.workspace) || (out.bin && out.lib))
    return std::unexpected(invalid("incompatible selectors"));
  return out;
}

core::Result<std::vector<TargetId>>
select_build_targets(const workspace::Workspace &workspace,
                     const BuildOptions &options) {
  if (options.help)
    return std::unexpected(invalid("help does not select targets"));
  std::optional<std::string_view> name;
  if (options.package)
    name = *options.package;
  auto selected =
      workspace::select_packages(workspace, name, options.workspace);
  if (!selected)
    return std::unexpected(selected.error());
  std::vector<TargetId> targets;
  std::size_t next = 0;
  for (std::size_t i = 0; i < workspace.members.size(); ++i) {
    const auto &member = workspace.members[i];
    if (next == selected->size() || (*selected)[next].root != member.root)
      continue;
    ++next;
    if (!options.bin && member.manifest.library)
      targets.push_back({i, std::nullopt});
    if (!options.lib)
      for (std::size_t j = 0; j < member.manifest.bins.size(); ++j)
        if (!options.bin || member.manifest.bins[j].name == *options.bin)
          targets.push_back({i, j});
  }
  if (targets.empty())
    return std::unexpected(invalid("no matching build targets"));
  if (options.bin && targets.size() != 1)
    return std::unexpected(invalid("ambiguous binary: " + *options.bin));
  return targets;
}
} // namespace amp::cli
