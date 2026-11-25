module;
#include <algorithm>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module cap.workspace;
import cap.core.diagnostic;
import cap.core.filesystem;
import cap.manifest;

export namespace cap::workspace {
struct PackageMember {
  std::string root;
  manifest::Manifest manifest;
};
struct Workspace {
  std::string root;
  manifest::Manifest manifest;
  std::vector<PackageMember> members;
  std::vector<std::string> defaults;
  std::optional<std::string> current;
};
core::Result<Workspace> load_workspace(std::string_view start,
                                       const core::IFileSystem &fs);
core::Result<std::vector<PackageMember>>
select_packages(const Workspace &workspace,
                std::optional<std::string_view> name, bool all);
} // namespace cap::workspace

namespace cap::workspace {
namespace {
namespace path = std::filesystem;
core::Diagnostic invalid(std::string message) {
  return {core::ErrorCode::invalid_input,
          core::Severity::error,
          std::move(message),
          {},
          {},
          {}};
}
bool inside(const path::path &root, const path::path &child) {
  auto a = root.begin(), b = child.begin();
  for (; a != root.end(); ++a, ++b)
    if (b == child.end() || *a != *b)
      return false;
  return true;
}
core::Result<std::optional<manifest::Manifest>>
read_manifest(const std::string &dir, const core::IFileSystem &fs) {
  auto file = fs.canonicalize((path::path(dir) / "cap.toml").string());
  if (!file) {
    if (file.error().code == core::ErrorCode::missing_resource)
      return std::nullopt;
    return std::unexpected(file.error());
  }
  if (path::path(*file).parent_path() != path::path(dir))
    return std::unexpected(
        invalid("manifest escapes package directory: " + *file));
  auto bytes = fs.read(*file);
  if (!bytes)
    return std::unexpected(bytes.error());
  std::string text;
  text.reserve(bytes->size());
  for (auto byte : *bytes)
    text.push_back(static_cast<char>(byte));
  auto parsed = manifest::parse_manifest(text, *file);
  if (!parsed)
    return std::unexpected(parsed.error());
  return std::optional<manifest::Manifest>(std::move(*parsed));
}
core::Result<std::string> resolve(const std::string &root,
                                  std::string_view input,
                                  const core::IFileSystem &fs) {
  path::path relative{input};
  if (input.empty() || relative.is_absolute() ||
      input.find_first_of("*?[]\\") != input.npos)
    return std::unexpected(
        invalid("unsupported workspace path: " + std::string(input)));
  for (const auto &component : relative)
    if (component == "..")
      return std::unexpected(
          invalid("workspace path traversal: " + std::string(input)));
  auto canonical = fs.canonicalize((path::path(root) / relative).string());
  if (!canonical)
    return std::unexpected(canonical.error());
  if (!inside(root, *canonical))
    return std::unexpected(
        invalid("workspace path escapes root: " + std::string(input)));
  return canonical;
}
core::Result<Workspace> expand(std::string root, manifest::Manifest model,
                               const core::IFileSystem &fs) {
  Workspace out{root, std::move(model), {}, {}, {}};
  std::set<std::string> roots, names;
  auto add = [&](const std::string &dir,
                 const manifest::Manifest &m) -> core::Status {
    if (!m.package)
      return std::unexpected(invalid("member has no package: " + dir));
    if (dir != root && m.workspace)
      return std::unexpected(invalid("nested workspace: " + dir));
    if (!roots.insert(dir).second || !names.insert(m.package->name).second)
      return std::unexpected(invalid("duplicate workspace member: " + dir));
    out.members.push_back({dir, m});
    return {};
  };
  if (out.manifest.package) {
    auto status = add(root, out.manifest);
    if (!status)
      return std::unexpected(status.error());
  }
  if (!out.manifest.workspace)
    return out;
  const auto &spec = *out.manifest.workspace;
  std::vector<std::string> declared;
  for (const auto &entry : spec.members) {
    if (entry.size() >= 2 && entry.ends_with("/*")) {
      auto base = resolve(
          root, std::string_view(entry).substr(0, entry.size() - 2), fs);
      if (!base)
        return std::unexpected(base.error());
      auto children = fs.enumerate(*base);
      if (!children)
        return std::unexpected(children.error());
      if (children->empty())
        return std::unexpected(
            invalid("unmatched workspace wildcard: " + entry));
      std::sort(children->begin(), children->end());
      for (const auto &child : *children) {
        auto candidate = resolve(
            root,
            (path::path(entry.substr(0, entry.size() - 2)) / child).string(),
            fs);
        if (!candidate)
          return std::unexpected(candidate.error());
        declared.push_back(*candidate);
      }
    } else {
      auto member = resolve(root, entry, fs);
      if (!member)
        return std::unexpected(member.error());
      declared.push_back(*member);
    }
  }
  std::set<std::string> declared_set;
  for (const auto &dir : declared)
    if (!declared_set.insert(dir).second || roots.contains(dir))
      return std::unexpected(invalid("duplicate workspace member: " + dir));
  std::set<std::string> excluded;
  for (const auto &entry : spec.exclude) {
    auto dir = resolve(root, entry, fs);
    if (!dir)
      return std::unexpected(dir.error());
    if (!declared_set.contains(*dir) || !excluded.insert(*dir).second)
      return std::unexpected(
          invalid("unknown or duplicate exclusion: " + entry));
  }
  for (const auto &dir : declared) {
    if (excluded.contains(dir))
      continue;
    auto m = read_manifest(dir, fs);
    if (!m)
      return std::unexpected(m.error());
    if (!*m)
      return std::unexpected(invalid("member manifest missing: " + dir));
    auto status = add(dir, **m);
    if (!status)
      return std::unexpected(status.error());
  }
  std::set<std::string> defaults;
  for (const auto &entry : spec.default_members) {
    auto dir = resolve(root, entry, fs);
    if (!dir)
      return std::unexpected(dir.error());
    if (!roots.contains(*dir) || !defaults.insert(*dir).second)
      return std::unexpected(
          invalid("unknown, excluded or duplicate default: " + entry));
    out.defaults.push_back(*dir);
  }
  return out;
}
} // namespace

core::Result<Workspace> load_workspace(std::string_view start,
                                       const core::IFileSystem &fs) {
  if (start.empty() || start.find('\0') != start.npos)
    return std::unexpected(invalid("invalid start directory"));
  auto canonical = fs.canonicalize(std::string(start));
  if (!canonical)
    return std::unexpected(canonical.error());
  auto directory_check = fs.enumerate(*canonical);
  if (!directory_check)
    return std::unexpected(directory_check.error());
  std::optional<std::string> nearest;
  std::optional<manifest::Manifest> nearest_model;
  std::optional<Workspace> owner;
  for (path::path dir(*canonical);; dir = dir.parent_path()) {
    auto model = read_manifest(dir.string(), fs);
    if (!model)
      return std::unexpected(model.error());
    if (*model) {
      if (!nearest) {
        nearest = dir.string();
        nearest_model = **model;
      }
      if ((**model).workspace) {
        if (owner)
          return std::unexpected(invalid("nested workspaces"));
        auto candidate = expand(dir.string(), **model, fs);
        if (!candidate)
          return std::unexpected(candidate.error());
        owner = std::move(*candidate);
      }
    }
    if (dir == dir.root_path())
      break;
  }
  if (!nearest)
    return std::unexpected(invalid("no manifest found"));
  if (!owner) {
    auto single = expand(*nearest, *nearest_model, fs);
    if (!single)
      return std::unexpected(single.error());
    owner = std::move(*single);
  }
  if (nearest_model->package) {
    auto found =
        std::find_if(owner->members.begin(), owner->members.end(),
                     [&](const auto &m) { return m.root == *nearest; });
    if (found == owner->members.end())
      return std::unexpected(
          invalid("package is not a workspace member: " + *nearest));
    if (*nearest != owner->root)
      owner->current = *nearest;
  }
  return std::move(*owner);
}
core::Result<std::vector<PackageMember>>
select_packages(const Workspace &workspace,
                std::optional<std::string_view> name, bool all) {
  if (name && all)
    return std::unexpected(invalid("package and all selectors conflict"));
  std::vector<PackageMember> selected;
  for (const auto &member : workspace.members) {
    bool include = all || (name && member.manifest.package->name == *name);
    if (!name && !all) {
      if (workspace.current)
        include = member.root == *workspace.current;
      else if (!workspace.defaults.empty())
        include =
            std::find(workspace.defaults.begin(), workspace.defaults.end(),
                      member.root) != workspace.defaults.end();
      else
        include = true;
    }
    if (include)
      selected.push_back(member);
  }
  if (name && selected.empty())
    return std::unexpected(invalid("unknown package: " + std::string(*name)));
  return selected;
}
} // namespace cap::workspace
