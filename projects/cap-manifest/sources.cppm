module;
#include <algorithm>
#include <expected>
#include <set>
#include <string>
#include <string_view>
#include <vector>

export module cap.manifest.sources;
import cap.core.diagnostic;
import cap.core.filesystem;
import cap.manifest;

export namespace cap::manifest {
core::Result<std::vector<std::string>>
select_sources(const std::string &, const Binary &, const core::IFileSystem &);
core::Result<std::vector<std::string>>
select_sources(const std::string &, const Library &, const core::IFileSystem &);
} // namespace cap::manifest

namespace cap::manifest {
namespace {
using Paths = std::vector<std::string>;
using core::ErrorCode;
core::Diagnostic invalid(std::string message) {
  core::Diagnostic d;
  d.code = ErrorCode::invalid_input;
  d.message = std::move(message);
  return d;
}
core::Result<Paths> segments(const std::string &s, bool pattern) {
  if (s.empty() || s.front() == '/' || s.find('\0') != s.npos ||
      s.find_first_of("?[]\\") != s.npos)
    return std::unexpected(invalid("invalid source path: " + s));
  Paths parts;
  std::size_t begin = 0;
  for (;;) {
    auto end = s.find('/', begin);
    auto part = s.substr(begin, end - begin);
    if (part.empty() || part == "." || part == ".." ||
        (!pattern && part.find('*') != part.npos) ||
        (part != "**" && part.find("**") != part.npos))
      return std::unexpected(invalid("invalid source pattern: " + s));
    parts.push_back(part);
    if (end == s.npos)
      break;
    begin = end + 1;
  }
  if (parts.back() == "**")
    return std::unexpected(
        invalid("recursive wildcard requires a following segment: " + s));
  return parts;
}
bool matches(std::string_view p, std::string_view s) {
  std::size_t i = 0, j = 0, star = p.npos, retry = 0;
  while (j < s.size()) {
    if (i < p.size() && p[i] == s[j]) {
      ++i;
      ++j;
    } else if (i < p.size() && p[i] == '*') {
      star = i++;
      retry = j;
    } else if (star != p.npos) {
      i = star + 1;
      j = ++retry;
    } else
      return false;
  }
  while (i < p.size() && p[i] == '*')
    ++i;
  return i == p.size();
}
bool eligible(const std::string &path) {
  auto slash = path.rfind('/');
  std::string_view name(path);
  if (slash != path.npos)
    name.remove_prefix(slash + 1);
  auto dot = name.rfind('.');
  if (dot == name.npos)
    return false;
  auto ext = name.substr(dot);
  if (ext != ".c" && ext != ".cc" && ext != ".cpp" && ext != ".cxx" &&
      ext != ".cppm")
    return false;
  name = name.substr(0, dot);
  if (ext == ".cpp" && name.ends_with("_test"))
    return false;
  for (auto arch : {"x86_64", "amd64", "arm64"}) {
    std::string suffix = std::string("_") + arch;
    if (name.ends_with(suffix)) {
      if (std::string_view(arch) == "arm64")
        return false;
      name.remove_suffix(suffix.size());
      break;
    }
  }
  for (auto os : {"linux", "windows", "darwin", "freebsd"}) {
    std::string suffix = std::string("_") + os;
    if (name.ends_with(suffix))
      return std::string_view(os) == "linux";
  }
  return true;
}
class Selector {
  const std::string &root;
  const core::IFileSystem &fs;
  std::string absolute(const std::string &rel) const {
    return rel.empty() ? root : (root == "/" ? root + rel : root + "/" + rel);
  }
  core::Result<std::string> safe(const std::string &rel) const {
    auto path = fs.canonicalize(absolute(rel));
    if (!path)
      return std::unexpected(path.error());
    if (*path != root &&
        !(root == "/" ? path->starts_with('/') : path->starts_with(root + "/")))
      return std::unexpected(invalid("source escapes package root: " + rel));
    return path;
  }
  core::Status walk(const Paths &parts, std::size_t index,
                    const std::string &rel, std::set<std::string> &out) const {
    if (index == parts.size()) {
      auto resolved = safe(rel);
      if (!resolved)
        return std::unexpected(resolved.error());
      auto dir = fs.enumerate(absolute(rel));
      if (dir)
        return std::unexpected(invalid("source is a directory: " + rel));
      if (dir.error().code != ErrorCode::invalid_input)
        return std::unexpected(dir.error());
      auto slash = rel.rfind('/');
      auto parent = safe(slash == rel.npos ? "" : rel.substr(0, slash));
      if (!parent)
        return std::unexpected(parent.error());
      auto name = rel.substr(slash == rel.npos ? 0 : slash + 1);
      if (*resolved != (*parent == "/" ? *parent + name : *parent + "/" + name))
        return std::unexpected(invalid("source file alias: " + rel));
      out.insert(rel);
      return {};
    }
    const auto &part = parts[index];
    if (part.find('*') == part.npos) {
      return walk(parts, index + 1, rel.empty() ? part : rel + "/" + part, out);
    }
    auto directory = safe(rel);
    if (!directory)
      return std::unexpected(directory.error());
    if (part == "**") {
      auto result = walk(parts, index + 1, rel, out);
      if (!result && result.error().code != ErrorCode::missing_resource)
        return result;
    }
    auto entries = fs.enumerate(absolute(rel));
    if (!entries) {
      if (entries.error().code == ErrorCode::invalid_input)
        return {};
      return std::unexpected(entries.error());
    }
    for (const auto &name : *entries) {
      if (name.empty() || name == "." || name == ".." ||
          name.find('/') != name.npos || name.find('\0') != name.npos)
        return std::unexpected(invalid("invalid filesystem entry"));
      if (part != "**" && !matches(part, name))
        continue;
      auto child = rel.empty() ? name : rel + "/" + name;
      auto resolved = safe(child);
      if (!resolved)
        return std::unexpected(resolved.error());
      if (part == "**") {
        if (*resolved != absolute(child))
          continue;
        auto children = fs.enumerate(absolute(child));
        if (!children) {
          if (children.error().code == ErrorCode::invalid_input)
            continue;
          return std::unexpected(children.error());
        }
      } else if (index + 1 < parts.size() && *resolved != absolute(child)) {
        continue;
      }
      auto result = walk(parts, part == "**" ? index : index + 1, child, out);
      if (!result && result.error().code != ErrorCode::missing_resource)
        return result;
    }
    return {};
  }

public:
  Selector(const std::string &r, const core::IFileSystem &f) : root(r), fs(f) {}
  core::Result<std::set<std::string>> expand(const Paths &patterns) const {
    std::set<std::string> out;
    for (const auto &p : patterns) {
      auto parts = segments(p, true);
      if (!parts)
        return std::unexpected(parts.error());
      auto status = walk(*parts, 0, "", out);
      if (!status)
        return std::unexpected(status.error());
    }
    return out;
  }
};
core::Result<Paths> select(const std::string &root, const Paths &sources,
                           const Paths &exclude, const std::string *entry,
                           const core::IFileSystem &fs) {
  if (root.empty() || root.front() != '/' || root.find('\0') != root.npos)
    return std::unexpected(invalid("invalid canonical package root"));
  auto canonical = fs.canonicalize(root);
  if (!canonical)
    return std::unexpected(canonical.error());
  if (*canonical != root)
    return std::unexpected(invalid("package root is not canonical"));
  Selector selector(root, fs);
  auto included = selector.expand(sources);
  if (!included)
    return std::unexpected(included.error());
  if (entry) {
    auto parsed = segments(*entry, false);
    if (!parsed)
      return std::unexpected(parsed.error());
    if (!eligible(*entry))
      return std::unexpected(invalid("unsupported binary entry: " + *entry));
    auto selected = selector.expand({*entry});
    if (!selected)
      return std::unexpected(selected.error());
    included->insert(*entry);
  }
  auto excluded = selector.expand(exclude);
  if (!excluded)
    return std::unexpected(excluded.error());
  if (entry && excluded->contains(*entry))
    return std::unexpected(invalid("binary entry excluded: " + *entry));
  Paths result;
  for (const auto &p : *included)
    if (!excluded->contains(p) && eligible(p))
      result.push_back(p);
  if (result.empty())
    return std::unexpected(invalid("empty source target"));
  return result;
}
} // namespace
core::Result<Paths> select_sources(const std::string &root, const Binary &b,
                                   const core::IFileSystem &fs) {
  return select(root, b.sources, b.exclude, &b.entry, fs);
}
core::Result<Paths> select_sources(const std::string &root, const Library &l,
                                   const core::IFileSystem &fs) {
  return select(root, l.sources, {}, nullptr, fs);
}
} // namespace cap::manifest
