module;

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <expected>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module cap.build.native_binary;

import cap.build.actions;
import cap.build.modules;
import cap.core.diagnostic;
import cap.core.filesystem;
import cap.core.process;

export namespace cap::build {

/// Owns the inputs and output for one standalone native binary execution.
struct NativeBinaryRequest {
  /// std::string — Caller-created, exclusive canonical private build
  /// directory; must be an absolute path without NUL or a trailing slash.
  std::string directory;
  /// std::string — Trusted absolute C++ driver used for linking; the runner's
  /// allowed PATH must itself be trusted for any child-tool lookup.
  std::string link_driver;
  /// std::string — Direct-child executable output, distinct from all object
  /// and report paths, and metadata-verified as a regular output on success.
  std::string executable;
  /// std::vector<CompileRequest> — Nonempty ordered compile inputs whose cwd
  /// is directory and whose object outputs are distinct direct children.
  std::vector<CompileRequest> compiles;
  /// LinkInputs — Optional ordered archive paths and SDK operands; empty by
  /// default. Paths are absolute, caller-resolved, and passed to plan_link
  /// unchanged; neither vector may alias a generated output.
  LinkInputs link_inputs{};
  /// std::optional<ModuleMap> — Disengaged preserves standalone behavior;
  /// engaged provides caller-approved external BMI bindings for workspace use.
  std::optional<ModuleMap> workspace_modules{};
};

/// Reports the verified binary and exact ordered compile actions executed.
struct NativeBinaryResult {
  /// std::string — Executable path verified as a regular output; the file is
  /// not read or executed by the engine.
  std::string executable;
  /// std::vector<Action> — Compile actions in actual execution order.
  std::vector<Action> compile_actions;
};

/// Owns the inputs and output for one standalone static archive execution.
struct NativeArchiveRequest {
  /// std::string — Caller-created, exclusive canonical private build
  /// directory; must be an absolute path without NUL or a trailing slash.
  std::string directory;
  /// std::string — Trusted absolute archiver executable; the runner's allowed
  /// PATH must itself be trusted for any child-tool lookup.
  std::string archiver;
  /// std::string — Direct-child archive output, distinct from all object
  /// and report paths, and metadata-verified as a regular output on success.
  std::string archive;
  /// std::vector<CompileRequest> — Nonempty ordered compile inputs whose cwd
  /// is directory and whose object outputs are distinct direct children.
  std::vector<CompileRequest> compiles;
  /// std::optional<ModuleMap> — Disengaged preserves standalone behavior;
  /// engaged provides caller-approved external BMI bindings for workspace use.
  std::optional<ModuleMap> workspace_modules{};
};

/// Reports the verified archive, exact ordered compile actions, and local
/// module BMI bindings verified as regular outputs.
struct NativeArchiveResult {
  /// std::string — Archive path verified as a regular output; the file is
  /// neither read nor executed by the engine.
  std::string archive;
  /// std::vector<Action> — Compile actions in actual execution order.
  std::vector<Action> compile_actions;
  /// ModuleMap — Local named-module bindings verified as regular outputs.
  ModuleMap provided_modules{};
};

/// Builds, links and verifies one binary without executing it.
/// Requires an exclusive private cwd, absolute trusted driver and compatible,
/// noncolliding compile inputs. Scans module units, compiles serially, links,
/// and metadata-checks each output; does not read or execute the binary.
/// @param[in] request const NativeBinaryRequest& — Required fields and path
///            constraints are documented above.
/// @param[in] process_runner const cap::core::IProcessRunner& — Injected
///            direct runner; its allowed PATH must be trusted for child tools.
/// @param[in] filesystem const cap::core::IFileSystem& — Injected reader for
///            scan-report bytes only.
/// @param[in] check_output const std::function<cap::core::Status(const
///            std::string&)>& — Required metadata-only output verifier;
///            reports failure as Status.
/// @return cap::core::Result<NativeBinaryResult> — Verified executable and
///         executed compile actions, or a diagnostic for invalid input,
///         planning, process, report, or output-check failure.
/// @throws std::bad_alloc If executor-owned storage cannot be allocated.
/// @throws std::length_error If executor-owned storage exceeds its size limit.
/// @note Exceptions from the injected adapters propagate unchanged.
cap::core::Result<NativeBinaryResult> run_native_binary(
    const NativeBinaryRequest &request,
    const cap::core::IProcessRunner &process_runner,
    const cap::core::IFileSystem &filesystem,
    const std::function<cap::core::Status(const std::string &)> &check_output);

/// Builds and verifies one static archive without reading or executing it.
/// Requires an exclusive private cwd, absolute trusted archiver and
/// compatible, noncolliding compile inputs. Scans module units, compiles
/// serially, archives, and metadata-checks each output; does not read or
/// execute the archive.
/// @param[in] request const NativeArchiveRequest& — Required fields and path
///            constraints are documented above.
/// @param[in] process_runner const cap::core::IProcessRunner& — Injected
///            direct runner; its allowed PATH must be trusted for child tools.
/// @param[in] filesystem const cap::core::IFileSystem& — Injected reader for
///            scan-report bytes only.
/// @param[in] check_output const std::function<cap::core::Status(const
///            std::string&)>& — Required metadata-only output verifier;
///            reports failure as Status.
/// @return cap::core::Result<NativeArchiveResult> — Verified archive and
///         executed compile actions, or a diagnostic for invalid input,
///         planning, process, report, or output-check failure.
/// @throws std::bad_alloc If executor-owned storage cannot be allocated.
/// @throws std::length_error If executor-owned storage exceeds its size limit.
/// @note Exceptions from the injected adapters propagate unchanged.
cap::core::Result<NativeArchiveResult> run_native_archive(
    const NativeArchiveRequest &request,
    const cap::core::IProcessRunner &process_runner,
    const cap::core::IFileSystem &filesystem,
    const std::function<cap::core::Status(const std::string &)> &check_output);
} // namespace cap::build

namespace cap::build {
/// Owns the compile actions and local module bindings from one execution.
struct NativeExecutionResult {
  /// std::vector<Action> — Actual compile actions in execution order.
  std::vector<Action> compile_actions;
  /// ModuleMap — Local BMI bindings verified after their producer actions.
  ModuleMap provided_modules;
};

namespace {
using cap::core::Diagnostic;
using cap::core::ErrorCode;

/// Check native execution's basic absolute path syntax without accessing IO.
/// @param[in] path std::string_view — Candidate path bytes.
/// @return bool — True for a non-root absolute path without a NUL.
/// @note Does not throw. This predicate does not verify canonicality.
bool valid_path(std::string_view path) {
  return path.size() > 1 && path.front() == '/' &&
         path.find('\0') == std::string_view::npos;
}

/// Check for a nonreserved direct child of the supplied directory.
/// @param[in] directory std::string_view — Private directory without final `/`.
/// @param[in] path std::string_view — Candidate output path.
/// @return bool — True for a direct-child name other than `.`, `..`,
///         `compile_commands.json`, or `gcm.cache`, without an embedded NUL.
/// @throws std::bad_alloc If constructing the directory prefix cannot allocate.
/// @throws std::length_error If constructing the prefix exceeds its size limit.
bool child_of(std::string_view directory, std::string_view path) {
  const auto prefix = std::string(directory) + "/";
  if (!path.starts_with(prefix) || path.find('/', prefix.size()) != path.npos)
    return false;
  const auto name = path.substr(prefix.size());
  return !name.empty() && name != "." && name != ".." &&
         name != "compile_commands.json" && name != "gcm.cache" &&
         name.find('\0') == name.npos;
}

/// Creates the common diagnostic result used by native executor validation.
/// @param[in] code ErrorCode — Diagnostic category to return.
/// @param[in] message std::string — Human-readable failure reason.
/// @return Diagnostic containing the selected error code and message.
/// @note Does not throw; moves the already-owned message into the diagnostic.
auto failure(ErrorCode code, std::string message) -> Diagnostic {
  Diagnostic d;
  d.code = code;
  d.message = std::move(message);
  return d;
}

/// Executes one planned action and preserves process failure context.
/// @param[in] action const Action& — Planned direct-exec action.
/// @param[in] runner const cap::core::IProcessRunner& — Direct runner.
/// @return cap::core::Result<cap::core::ProcessResult> — Process result on zero
///         exit without timeout, or a diagnostic for runner failure, timeout
///         or a nonzero exit.
/// @throws std::bad_alloc If the process spec or diagnostic cannot allocate.
/// @throws std::length_error If the process spec or diagnostic overflows.
/// @note Exceptions from the runner propagate unchanged.
auto run_action(const Action &action, const cap::core::IProcessRunner &runner)
    -> cap::core::Result<cap::core::ProcessResult> {
  cap::core::ProcessSpec spec;
  spec.argv = action.arguments;
  spec.cwd = action.directory;
  spec.env_allowlist = {"PATH"};
  spec.timeout = std::chrono::minutes(2);
  auto result = runner.run(spec);
  if (!result)
    return result;
  if (result->timed_out || result->exit_code != 0) {
    Diagnostic d;
    d.code = result->timed_out ? ErrorCode::transient_io : ErrorCode::internal;
    d.message =
        action.arguments.front() +
        (result->timed_out ? ": timed out"
                           : ": exited " + std::to_string(result->exit_code)) +
        (result->stderr_text.empty() ? "" : ": " + result->stderr_text);
    return std::unexpected(std::move(d));
  }
  return result;
}

/// Validates external BMI bindings before any compiler subprocess is started.
/// @param[in] directory const std::string& — Private target cwd.
/// @param[in] external const ModuleMap& — Caller-approved external bindings.
/// @param[in] terminal const Action& — Planned final archive or link action.
/// @param[in] inputs const std::set<std::string>& — Planned compile inputs.
/// @param[in] outputs const std::set<std::string>& — Planned action outputs.
/// @param[in] reports const std::set<std::string, std::less<>>& — Reserved scan
///            report paths.
/// @param[in] check_output const std::function<cap::core::Status(const
///            std::string&)>& — Metadata-only regular-file verifier.
/// @return cap::core::Status — Success for valid noncolliding regular BMI
///         bindings, or the first validation/output diagnostic.
/// @throws std::bad_alloc If validation storage cannot allocate.
/// @throws std::length_error If validation storage exceeds its size limit.
/// @note Exceptions from the verifier propagate unchanged.
auto validate_external_modules(
    const std::string &directory, const ModuleMap &external,
    const Action &terminal, const std::set<std::string> &inputs,
    const std::set<std::string> &outputs,
    const std::set<std::string, std::less<>> &reports,
    const std::function<cap::core::Status(const std::string &)> &check_output)
    -> cap::core::Status {
  auto formatted = format_module_map(external);
  if (!formatted)
    return std::unexpected(formatted.error());
  const std::string mapper_path = directory + "/.cap-module-map";
  for (const auto &binding : external) {
    const auto &path = binding.second;
    if (outputs.contains(path) || inputs.contains(path) ||
        reports.contains(path) || path == mapper_path ||
        std::find(terminal.inputs.begin(), terminal.inputs.end(), path) !=
            terminal.inputs.end())
      return std::unexpected(
          failure(ErrorCode::invalid_input, "external BMI path collision"));
    auto checked = check_output(path);
    if (!checked)
      return checked;
  }
  return {};
}

/// Prepares the target-scoped mapper and exact action metadata.
/// @param[in] directory const std::string& — Private target cwd.
/// @param[in,out] actions std::vector<Action>& — Planned actions receiving
///            mapper argv and BMI metadata.
/// @param[in] units const std::vector<ModuleUnit>& — Ordered scanned units.
/// @param[in] terminal const Action& — Planned archive or link action.
/// @param[in] external const ModuleMap& — Caller-approved BMI bindings.
/// @param[in] inputs const std::set<std::string>& — Compile inputs.
/// @param[in] outputs const std::set<std::string>& — Planned outputs.
/// @param[in] reports const std::set<std::string, std::less<>>& — Scan reports.
/// @param[in] filesystem const cap::core::IFileSystem& — Atomic mapper writer.
/// @param[in] check_output const std::function<cap::core::Status(const
///            std::string&)>& — Metadata-only output verifier.
/// @return cap::core::Result<ModuleMap> — Local provider bindings awaiting
///         execution-time output verification, or a validation/write/
///         metadata-check diagnostic.
/// @throws std::bad_alloc If plan or metadata storage cannot allocate.
/// @throws std::length_error If plan or metadata exceeds its size limit.
/// @note Exceptions from injected filesystem/check adapters propagate.
auto prepare_workspace_modules(
    const std::string &directory, std::vector<Action> &actions,
    const std::vector<ModuleUnit> &units, const Action &terminal,
    const ModuleMap &external, const std::set<std::string> &inputs,
    const std::set<std::string> &outputs,
    const std::set<std::string, std::less<>> &reports,
    const cap::core::IFileSystem &filesystem,
    const std::function<cap::core::Status(const std::string &)> &check_output)
    -> cap::core::Result<ModuleMap>;

/// Verifies every declared output of one successful compile action.
/// @param[in] action const Action& — Executed compile action with primary
///            object first and any local BMI outputs following it.
/// @param[in] check_output const std::function<cap::core::Status(const
///            std::string&)>& — Metadata-only regular-file verifier.
/// @return cap::core::Status — Success if all outputs are regular, else the
///         first verifier diagnostic.
/// @throws std::bad_alloc If verifier diagnostics cannot allocate.
/// @throws std::length_error If verifier diagnostics exceed their limit.
/// @note Exceptions from the verifier propagate unchanged.
auto verify_compile_outputs(
    const Action &action,
    const std::function<cap::core::Status(const std::string &)> &check_output)
    -> cap::core::Status {
  for (const auto &path : action.outputs) {
    auto checked = check_output(path);
    if (!checked)
      return checked;
  }
  return {};
}

/// Plans deterministic direct-child BMI paths for scanned local providers.
/// @param[in] directory const std::string& — Private target cwd.
/// @param[in] units const std::vector<ModuleUnit>& — Ordered scanned units.
/// @param[in] external const ModuleMap& — Caller-approved external providers.
/// @param[in,out] reserved_paths std::set<std::string, std::less<>>& — Combined
///            inputs, outputs, reports, link operands, mapper and BMI paths.
/// @return cap::core::Result<ModuleMap> — Local logical names and reserved BMI
///         paths, or a collision diagnostic.
/// @throws std::bad_alloc If map/path storage cannot allocate.
/// @throws std::length_error If map/path storage exceeds its size limit.
auto plan_local_module_bindings(
    const std::string &directory, const std::vector<ModuleUnit> &units,
    const ModuleMap &external,
    std::set<std::string, std::less<>> &reserved_paths)
    -> cap::core::Result<ModuleMap> {
  ModuleMap provided;
  std::size_t ordinal = 0;
  for (const auto &unit : units)
    for (const auto &name : unit.provides) {
      const std::string bmi =
          directory + "/.cap-bmi-" + std::to_string(ordinal++) + ".gcm";
      if (external.contains(name) || reserved_paths.contains(bmi) ||
          !provided.emplace(name, bmi).second)
        return std::unexpected(
            failure(ErrorCode::invalid_input, "module BMI path collision"));
      reserved_paths.insert(bmi);
    }
  return provided;
}

/// Writes and metadata-verifies the deterministic private GCC module map.
/// @param[in] mapper_path const std::string& — Private mapper destination.
/// @param[in] external const ModuleMap& — Caller-approved BMI bindings.
/// @param[in] provided const ModuleMap& — Planned local BMI bindings.
/// @param[in] filesystem const cap::core::IFileSystem& — Atomic writer.
/// @param[in] check_output const std::function<cap::core::Status(const
///            std::string&)>& — Metadata-only regular-file verifier.
/// @return cap::core::Status — Success after write and verification, otherwise
///         the formatter, writer, or verifier diagnostic.
/// @throws std::bad_alloc If map text cannot allocate.
/// @throws std::length_error If map text exceeds its size limit.
/// @note Exceptions from the injected adapters propagate unchanged.
auto write_workspace_mapper(
    const std::string &mapper_path, const ModuleMap &external,
    const ModuleMap &provided, const cap::core::IFileSystem &filesystem,
    const std::function<cap::core::Status(const std::string &)> &check_output)
    -> cap::core::Status {
  ModuleMap complete = external;
  complete.insert(provided.begin(), provided.end());
  auto map_text = format_module_map(complete);
  if (!map_text)
    return std::unexpected(map_text.error());
  const auto map_bytes =
      std::as_bytes(std::span(map_text->data(), map_text->size()));
  auto written = filesystem.write_atomic(mapper_path, map_bytes);
  if (!written)
    return written;
  return check_output(mapper_path);
}

/// Adds mapper arguments and local/transitive external BMI metadata.
/// @param[in,out] actions std::vector<Action>& — Compile actions to update.
/// @param[in] units const std::vector<ModuleUnit>& — Ordered scanned units.
/// @param[in] provided const ModuleMap& — Local provider BMI bindings.
/// @param[in] external const ModuleMap& — External provider BMI bindings.
/// @param[in] mapper_path const std::string& — Written mapper path.
/// @return void — Updates C++ actions; leaves C action argv and metadata
/// intact.
/// @throws std::bad_alloc If lookup or action metadata storage cannot allocate.
/// @throws std::length_error If action metadata exceeds its size limit.
void add_workspace_compile_metadata(std::vector<Action> &actions,
                                    const std::vector<ModuleUnit> &units,
                                    const ModuleMap &provided,
                                    const ModuleMap &external,
                                    const std::string &mapper_path) {
  std::map<std::string, const ModuleUnit *, std::less<>> unit_by_object;
  std::map<std::string, const ModuleUnit *, std::less<>> unit_by_module;
  for (const auto &unit : units) {
    unit_by_object.emplace(unit.object, &unit);
    for (const auto &name : unit.provides)
      unit_by_module.emplace(name, &unit);
  }
  std::set<std::string, std::less<>> external_paths;
  for (const auto &[name, path] : external) {
    (void)name;
    external_paths.insert(path);
  }
  for (auto &action : actions) {
    if (action.inputs.front().ends_with(".c"))
      continue;
    action.arguments.push_back("-fmodule-mapper=" + mapper_path);
    action.inputs.push_back(mapper_path);
    auto unit = unit_by_object.find(action.outputs.front());
    if (unit == unit_by_object.end())
      continue;
    std::set<std::string, std::less<>> needed_names;
    std::vector<std::string> pending = unit->second->required_modules;
    while (!pending.empty()) {
      auto name = std::move(pending.back());
      pending.pop_back();
      if (!needed_names.insert(name).second)
        continue;
      auto local = unit_by_module.find(name);
      if (local != unit_by_module.end())
        pending.insert(pending.end(), local->second->required_modules.begin(),
                       local->second->required_modules.end());
    }
    bool reaches_external = false;
    std::set<std::string, std::less<>> local_paths;
    for (const auto &name : needed_names) {
      auto local = provided.find(name);
      if (local != provided.end()) {
        local_paths.insert(local->second);
        continue;
      }
      auto imported = external.find(name);
      if (imported != external.end())
        reaches_external = true;
    }
    action.inputs.insert(action.inputs.end(), local_paths.begin(),
                         local_paths.end());
    if (reaches_external)
      action.inputs.insert(action.inputs.end(), external_paths.begin(),
                           external_paths.end());
    for (const auto &name : unit->second->provides)
      action.outputs.push_back(provided.at(name));
  }
}

auto prepare_workspace_modules(
    const std::string &directory, std::vector<Action> &actions,
    const std::vector<ModuleUnit> &units, const Action &terminal,
    const ModuleMap &external, const std::set<std::string> &inputs,
    const std::set<std::string> &outputs,
    const std::set<std::string, std::less<>> &reports,
    const cap::core::IFileSystem &filesystem,
    const std::function<cap::core::Status(const std::string &)> &check_output)
    -> cap::core::Result<ModuleMap> {
  const std::string mapper_path = directory + "/.cap-module-map";
  std::set<std::string, std::less<>> reserved_paths(inputs.begin(),
                                                    inputs.end());
  reserved_paths.insert(outputs.begin(), outputs.end());
  reserved_paths.insert(reports.begin(), reports.end());
  reserved_paths.insert(terminal.inputs.begin(), terminal.inputs.end());
  for (const auto &binding : external)
    reserved_paths.insert(binding.second);
  if (reserved_paths.contains(mapper_path))
    return std::unexpected(
        failure(ErrorCode::invalid_input, "module mapper path collision"));
  reserved_paths.insert(mapper_path);
  auto provided =
      plan_local_module_bindings(directory, units, external, reserved_paths);
  if (!provided)
    return std::unexpected(provided.error());
  auto written = write_workspace_mapper(mapper_path, external, *provided,
                                        filesystem, check_output);
  if (!written)
    return std::unexpected(written.error());
  add_workspace_compile_metadata(actions, units, *provided, external,
                                 mapper_path);
  return provided;
}
} // namespace

/// Runs the shared preflight, module scan/compile flow and selected terminal
/// planner for a binary or archive request.
/// @param[in] directory const std::string& — Canonical private directory.
/// @param[in] tool const std::string& — Explicit link driver or archiver.
/// @param[in] output const std::string& — Direct-child terminal output.
/// @param[in] compiles const std::vector<CompileRequest>& — Ordered requests.
/// @param[in] archive bool — Select archive planning instead of binary linking.
/// @param[in] link_inputs const LinkInputs& — Ordered binary archives and SDK
///            operands; ignored for archive execution.
/// @param[in] workspace_modules const std::optional<ModuleMap>& — Disengaged
///            for standalone mode or caller-approved external BMI bindings.
/// @param[in] runner const cap::core::IProcessRunner& — Direct runner.
/// @param[in] filesystem const cap::core::IFileSystem& — Report reader.
/// @param[in] check_output const std::function<cap::core::Status(const
///            std::string&)>& — Required metadata-only output verifier.
/// @return cap::core::Result<NativeExecutionResult> — Executed compile actions
///         and verified local BMI bindings, or the first validation, planning,
///         process or IO error.
/// @throws std::bad_alloc If allocation of owned plans or results fails.
/// @throws std::length_error If plan or result storage exceeds its size limit.
/// @note Exceptions from the injected adapters propagate unchanged.
auto run_native(
    const std::string &directory, const std::string &tool,
    const std::string &output, const std::vector<CompileRequest> &compiles,
    bool archive, const LinkInputs &link_inputs,
    const std::optional<ModuleMap> &workspace_modules,
    const cap::core::IProcessRunner &runner,
    const cap::core::IFileSystem &filesystem,
    const std::function<cap::core::Status(const std::string &)> &check_output)
    -> cap::core::Result<NativeExecutionResult> {
  if (!valid_path(directory) || !valid_path(tool) || directory.back() == '/' ||
      !valid_path(output) || !child_of(directory, output) || compiles.empty() ||
      !check_output)
    return std::unexpected(failure(ErrorCode::invalid_input,
                                   archive ? "invalid native archive request"
                                           : "invalid native binary request"));

  std::vector<Action> actions;
  std::vector<std::string> reports;
  std::set<std::string, std::less<>> report_names;
  std::vector<std::vector<std::string>> scans;
  std::set<std::string> inputs, outputs{output};
  for (const auto &compile : compiles) {
    if (compile.directory != directory ||
        (!compile.source.ends_with(".c") && !compile.cxx_modules) ||
        !valid_path(compile.source) || !child_of(directory, compile.output) ||
        !inputs.insert(compile.source).second ||
        !outputs.insert(compile.output).second)
      return std::unexpected(failure(ErrorCode::invalid_input,
                                     "invalid or colliding compile paths"));
    auto action = plan_compile(compile);
    if (!action)
      return std::unexpected(action.error());
    actions.push_back(std::move(*action));
  }
  for (const auto &action : actions)
    for (const auto &input : action.inputs)
      if (outputs.contains(input))
        return std::unexpected(failure(ErrorCode::invalid_input,
                                       "input collides with build output"));

  const bool mapped =
      workspace_modules.has_value() &&
      std::any_of(
          actions.begin(), actions.end(),
          /// Identifies C++ actions that can require GCC module mapping.
          /// @param[in] action const Action& — Planned compile action.
          /// @return bool — True unless the primary source is C.
          [](const Action &action) {
            return !action.inputs.front().ends_with(".c");
          });
  const std::string mapper_path = directory + "/.cap-module-map";
  if (mapped &&
      (outputs.contains(mapper_path) || inputs.contains(mapper_path) ||
       std::find(link_inputs.ordered_archives.begin(),
                 link_inputs.ordered_archives.end(),
                 mapper_path) != link_inputs.ordered_archives.end() ||
       std::find(link_inputs.system_libraries.begin(),
                 link_inputs.system_libraries.end(),
                 mapper_path) != link_inputs.system_libraries.end()))
    return std::unexpected(
        failure(ErrorCode::invalid_input, "module mapper path collision"));

  reports.reserve(actions.size());
  for (const auto &action : actions) {
    if (action.inputs.front().ends_with(".c")) {
      reports.emplace_back();
      continue;
    }
    const auto report = action.outputs.front() + ".p1689.json";
    if (!child_of(directory, report) || outputs.contains(report) ||
        inputs.contains(report) || (mapped && report == mapper_path) ||
        !report_names.insert(report).second)
      return std::unexpected(failure(ErrorCode::invalid_input,
                                     mapped && report == mapper_path
                                         ? "module mapper path collision"
                                         : "module report path collision"));
    reports.push_back(report);
  }

  for (std::size_t i = 0; i < actions.size(); ++i) {
    const auto &action = actions[i];
    if (reports[i].empty()) {
      scans.emplace_back();
      continue;
    }
    const auto &report = reports[i];
    auto scan = plan_module_scan(action, report);
    if (!scan)
      return std::unexpected(scan.error());
    scans.push_back(std::move(*scan));
  }
  Action terminal;
  if (archive) {
    ArchiveRequest request{tool, directory, output, {}};
    for (const auto &action : actions)
      request.objects.push_back(action.outputs.front());
    auto planned = plan_archive(request);
    if (!planned)
      return std::unexpected(planned.error());
    terminal = std::move(*planned);
  } else {
    LinkRequest request{tool,
                        directory,
                        output,
                        {},
                        link_inputs.ordered_archives,
                        link_inputs.system_libraries};
    for (const auto &action : actions)
      request.objects.push_back(action.outputs.front());
    auto planned = plan_link(request);
    if (!planned)
      return std::unexpected(planned.error());
    terminal = std::move(*planned);
    for (std::size_t i = request.objects.size(); i < terminal.inputs.size();
         ++i)
      if (outputs.contains(terminal.inputs[i]) ||
          report_names.contains(terminal.inputs[i]))
        return std::unexpected(failure(
            ErrorCode::invalid_input, "link input collides with build output"));
  }
  if (mapped) {
    auto external_checked =
        validate_external_modules(directory, *workspace_modules, terminal,
                                  inputs, outputs, report_names, check_output);
    if (!external_checked)
      return std::unexpected(external_checked.error());
  }

  std::vector<ModuleUnit> units;
  for (std::size_t i = 0; i < actions.size(); ++i) {
    const auto &action = actions[i];
    if (reports[i].empty())
      continue;
    cap::core::ProcessSpec spec{
        scans[i], action.directory, {"PATH"}, {}, std::chrono::minutes(2)};
    auto result = runner.run(spec);
    if (!result)
      return std::unexpected(result.error());
    if (result->timed_out || result->exit_code != 0)
      return std::unexpected(failure(
          result->timed_out ? ErrorCode::transient_io : ErrorCode::internal,
          scans[i].front() +
              (result->timed_out
                   ? ": timed out"
                   : ": exited " + std::to_string(result->exit_code)) +
              (result->stderr_text.empty() ? "" : ": " + result->stderr_text)));
    auto report_checked = check_output(reports[i]);
    if (!report_checked)
      return std::unexpected(report_checked.error());
    auto bytes = filesystem.read(reports[i]);
    if (!bytes)
      return std::unexpected(bytes.error());
    const std::string text(reinterpret_cast<const char *>(bytes->data()),
                           bytes->size());
    auto unit = parse_module_scan(text, action.outputs.front());
    if (!unit)
      return std::unexpected(unit.error());
    units.push_back(std::move(*unit));
  }
  auto ordered = mapped
                     ? order_module_units(std::move(units), *workspace_modules)
                     : order_module_units(std::move(units));
  if (!ordered)
    return std::unexpected(ordered.error());

  ModuleMap provided_modules;
  if (mapped) {
    auto prepared = prepare_workspace_modules(
        directory, actions, *ordered, terminal, *workspace_modules, inputs,
        outputs, report_names, filesystem, check_output);
    if (!prepared)
      return std::unexpected(prepared.error());
    provided_modules = std::move(*prepared);
  }

  std::map<std::string, Action, std::less<>> action_by_object;
  for (auto &action : actions)
    action_by_object.emplace(action.outputs.front(), std::move(action));
  std::vector<Action> ordered_actions;
  std::set<std::string> emitted;
  for (const auto &unit : *ordered) {
    auto found = action_by_object.find(unit.object);
    if (found != action_by_object.end()) {
      emitted.insert(unit.object);
      ordered_actions.push_back(found->second);
    }
  }
  for (const auto &[object, action] : action_by_object)
    if (!emitted.contains(object))
      ordered_actions.push_back(action);

  for (const auto &action : ordered_actions) {
    auto result = run_action(action, runner);
    if (!result)
      return std::unexpected(result.error());
    auto verified = verify_compile_outputs(action, check_output);
    if (!verified)
      return std::unexpected(verified.error());
  }
  auto result = run_action(terminal, runner);
  if (!result)
    return std::unexpected(result.error());
  auto checked = check_output(output);
  if (!checked)
    return std::unexpected(checked.error());
  return NativeExecutionResult{std::move(ordered_actions),
                               std::move(provided_modules)};
}

cap::core::Result<NativeBinaryResult> run_native_binary(
    const NativeBinaryRequest &r, const cap::core::IProcessRunner &runner,
    const cap::core::IFileSystem &filesystem,
    const std::function<cap::core::Status(const std::string &)> &check_output) {
  auto execution = run_native(
      r.directory, r.link_driver, r.executable, r.compiles, false,
      r.link_inputs, r.workspace_modules, runner, filesystem, check_output);
  if (!execution)
    return std::unexpected(execution.error());
  return NativeBinaryResult{r.executable,
                            std::move(execution->compile_actions)};
}

cap::core::Result<NativeArchiveResult> run_native_archive(
    const NativeArchiveRequest &r, const cap::core::IProcessRunner &runner,
    const cap::core::IFileSystem &filesystem,
    const std::function<cap::core::Status(const std::string &)> &check_output) {
  auto execution =
      run_native(r.directory, r.archiver, r.archive, r.compiles, true, {},
                 r.workspace_modules, runner, filesystem, check_output);
  if (!execution)
    return std::unexpected(execution.error());
  return NativeArchiveResult{r.archive, std::move(execution->compile_actions),
                             std::move(execution->provided_modules)};
}
} // namespace cap::build
