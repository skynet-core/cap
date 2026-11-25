# cap bootstrap orchestrator — GNU Make only. No CMake, Meson or Ninja.
# Requires git, POSIX shell, a C++26-capable C++ compiler, and GNU Make.
# v1.4: every Boost build requires explicit verified BOOST_VERSION and full BOOST_COMMIT.
SHELL := /bin/sh
# Pinned system compiler for this host: Homebrew GCC 16 (C++26).
# Plain assignment so environment CXX cannot silently switch the toolchain;
# override explicitly on the make command line for other hosts.
CXX = /home/linuxbrew/.linuxbrew/bin/g++-16
CC = /home/linuxbrew/.linuxbrew/bin/gcc-16
# Boost.Build ignores CC/CXX env vars for toolset selection (it searches PATH
# for g++); pin it explicitly via user-config.jam + BOOST_BUILD_USER_CONFIG.
# Recursive assignment: BOOTSTRAP_ROOT is defined later in the file.
BOOST_USER_CONFIG = $(BOOTSTRAP_ROOT)/user-config.jam
export BOOST_BUILD_USER_CONFIG = $(BOOST_USER_CONFIG)
export CC CXX
# Second pinned toolchain for sanitizer jobs only (docs/QUALITY.md mandates
# Clang configurations); g++-16 stays the build and coverage compiler.
CLANG = /home/linuxbrew/.linuxbrew/bin/clang++
# Coverage tooling (approved 2026-11-20): lcov merges gcov data, genhtml reports.
LCov = /home/linuxbrew/.linuxbrew/bin/lcov
GCovTool = /home/linuxbrew/.linuxbrew/bin/gcov-16
# The brew genhtml bottle is broken on this host (requires an LCOV::Util
# module the formula never installs); use the upstream lcov-2.6 genhtml.
# LCOV_TARBALL is fetched once into .bootstrap; perl deps live in
# .bootstrap/perl-lib (cpanm --local-lib; untracked).
LCOV_VERSION := 2.6
LCOV_TARBALL = $(BOOTSTRAP_ROOT)/lcov-$(LCOV_VERSION).tar.gz
LCOV_SRC = $(BOOTSTRAP_ROOT)/lcov-$(LCOV_VERSION)
GENHTML = $(LCOV_SRC)/bin/genhtml
LCOV_PERL_LIB = $(BOOTSTRAP_ROOT)/perl-lib/lib/perl5
# TSan job scope: process_test excluded (children are uninstrumented).
CAP_CORE_TSAN_TESTS := diagnostic_test filesystem_test
# Recorded coverage baseline; waiver: projects/cap-core/COVERAGE-WAIVER.md.
# Raw policy gates (85/75) apply once T05 fault injection makes the excluded
# paths testable; until then the gate is no-regression vs this baseline.
CAP_COV_BASELINE_LINE := 79
CAP_COV_BASELINE_BRANCH := 66
GIT ?= git
BOOST_VERSION ?=
BOOST_COMMIT ?=
BOOST_REPO ?= https://github.com/boostorg/boost.git
BOOST_COMPONENTS ?= filesystem program_options json
BOOST_JOBS ?= 4
BOOST_VARIANT ?= release
BOOTSTRAP_ROOT ?= $(CURDIR)/.bootstrap
BOOST_SRC := $(BOOTSTRAP_ROOT)/boost-src
BOOST_PREFIX := $(BOOTSTRAP_ROOT)/boost-install
BOOST_STAMP := $(BOOST_PREFIX)/.cap-boost-built
CAP_BOOTSTRAP_SRC ?= bootstrap/main.cpp
CAP_BOOTSTRAP_BIN := $(BOOTSTRAP_ROOT)/bin/cap-bootstrap
CAP_CORE_TESTS := \
  projects/cap-core/tests/diagnostic_test.cpp \
  projects/cap-core/tests/filesystem_test.cpp \
  projects/cap-core/tests/process_test.cpp \
  projects/cap-core/tests/digest_test.cpp \
  projects/cap-core/tests/primitives_test.cpp
CAP_CORE_TEST_GOLDEN := projects/cap-core/tests/golden/diagnostic_format.txt
CAP_CORE_MODULES := diagnostic clock stop_token digest logger filesystem process
BOOTSTRAP_PROBES_ROOT := $(BOOTSTRAP_ROOT)/bootstrap-probes
BOOTSTRAP_MODULE_PROBE := projects/cap-bootstrap/tests/module_probe.cppm
BOOTSTRAP_MODULE_PROBE_TEST := projects/cap-bootstrap/tests/module_probe_test.cpp

.PHONY: help prerequisites bootstrap-probes boost-fetch boost-bootstrap boost-build boost bootstrap bootstrap-dev self-host core-test sanitize core-coverage test check coverage bench profile install clean boost-clean
help:
	@printf '%s\n' 'Targets: prerequisites bootstrap-probes boost-fetch boost-bootstrap boost-build boost bootstrap bootstrap-dev self-host core-test sanitize core-coverage test check coverage bench profile install clean'
prerequisites:
	@command -v $(GIT) >/dev/null || { echo 'ERROR: git is required' >&2; exit 1; }
	@command -v $(CXX) >/dev/null || { echo 'ERROR: C++ compiler is required' >&2; exit 1; }
	@command -v sort >/dev/null || { echo 'ERROR: sort is required' >&2; exit 1; }
	@printf 'int main() { return 0; }\n' | $(CXX) -std=c++26 -x c++ -fsyntax-only - >/dev/null || { echo 'ERROR: compiler does not accept C++26' >&2; exit 1; }
	@echo 'NOTE: reflection support requires a separate feature probe before compiling reflection-dependent cap sources.'

# Opt-in named-module check; each invocation retains its fresh directory as evidence.
bootstrap-probes:
	@set -eu; \
	  command -v $(CXX) >/dev/null || { echo 'ERROR: configured C++ compiler not found: $(CXX)' >&2; exit 1; }; \
	  mkdir -p '$(BOOTSTRAP_PROBES_ROOT)'; \
	  probe_dir=$$(mktemp -d '$(BOOTSTRAP_PROBES_ROOT)/run.XXXXXXXX'); \
	  echo "bootstrap-probes: evidence directory $$probe_dir"; \
	  (cd "$$probe_dir" && $(CXX) -std=c++26 -fmodules -x c++ -c '$(CURDIR)/$(BOOTSTRAP_MODULE_PROBE)' -o module_probe.o) || { echo 'ERROR: C++26 named module interface probe failed' >&2; exit 1; }; \
	  (cd "$$probe_dir" && $(CXX) -std=c++26 -fmodules '$(CURDIR)/$(BOOTSTRAP_MODULE_PROBE_TEST)' module_probe.o -o module_probe_test) || { echo 'ERROR: C++26 named module importer probe failed' >&2; exit 1; }; \
	  "$$probe_dir/module_probe_test" || { echo 'ERROR: C++26 named module importer runtime probe failed' >&2; exit 1; }; \
	  echo 'bootstrap-probes: separate C++26 named-module importer compiled and ran.'

# Fetch complete Boost superproject with its submodules; never execute other project build systems.
boost-fetch: prerequisites
	@test -n '$(BOOST_VERSION)' && test '$(BOOST_VERSION)' != latest || { echo 'ERROR: set BOOST_VERSION to a verified exact Boost release' >&2; exit 1; }
	@printf '%s' '$(BOOST_COMMIT)' | grep -Eq '^[[:xdigit:]]{40}$$' || { echo 'ERROR: set BOOST_COMMIT to the verified full 40-hex commit' >&2; exit 1; }
	@mkdir -p '$(BOOTSTRAP_ROOT)'
	@if test -d '$(BOOST_SRC)/.git'; then \
	  echo 'Verifying preseeded/existing Boost checkout'; \
	else \
	  echo 'Fetching pinned Boost $(BOOST_VERSION)'; \
	  $(GIT) clone --branch 'boost-$(BOOST_VERSION)' --depth 1 --recurse-submodules --shallow-submodules '$(BOOST_REPO)' '$(BOOST_SRC)' || exit 1; \
	fi
	@actual=$$(cd '$(BOOST_SRC)' && $(GIT) rev-parse HEAD) || exit 1; \
	  test "$$actual" = '$(BOOST_COMMIT)' || { echo 'ERROR: Boost checkout revision mismatch; use make boost-clean then retry' >&2; exit 1; }; \
	  tag=$$($(GIT) -C '$(BOOST_SRC)' rev-parse --verify 'refs/tags/boost-$(BOOST_VERSION)^{commit}') || { echo 'ERROR: Boost release tag missing or invalid' >&2; exit 1; }; \
	  test "$$tag" = "$$actual" || { echo 'ERROR: Boost checkout is not at the requested release tag' >&2; exit 1; }; \
	  submodules=$$($(GIT) -C '$(BOOST_SRC)' submodule status --recursive) || { echo 'ERROR: cannot inspect Boost submodules' >&2; exit 1; }; \
	  if printf '%s\n' "$$submodules" | grep -E '^[+-U]' >/dev/null; then echo 'ERROR: Boost submodule checkout incomplete or modified' >&2; exit 1; else test $$? -eq 1 || exit 1; fi; \
	  check='flags=$$("'"$(GIT)"'" ls-files -v) || exit 1; \
	    if printf "%s\n" "$$flags" | grep -E "^[a-zS] " >/dev/null; then exit 1; else test $$? -eq 1 || exit 1; fi; \
	    "'"$(GIT)"'" diff --cached --quiet --ignore-submodules=none && "'"$(GIT)"'" --no-optional-locks diff --quiet --ignore-submodules=untracked'; \
	  (cd '$(BOOST_SRC)' && sh -c "$$check") && $(GIT) -C '$(BOOST_SRC)' submodule foreach --quiet --recursive "$$check" || { echo 'ERROR: Boost tracked files or index are modified or unverifiable' >&2; exit 1; }; \
	  printf '%s\n' "$$actual" > '$(BOOTSTRAP_ROOT)/boost-commit.txt' && \
	  printf '%s\n' '$(BOOST_VERSION)' > '$(BOOTSTRAP_ROOT)/boost-version.txt' && \
	  printf '%s\n' "$$submodules" > '$(BOOTSTRAP_ROOT)/boost-submodules.txt' || { echo 'ERROR: cannot record Boost source identities' >&2; exit 1; }

boost-bootstrap: boost-fetch
	@test -x '$(BOOST_SRC)/bootstrap.sh' || { echo 'ERROR: Boost bootstrap.sh missing (POSIX host expected)' >&2; exit 1; }
	@printf 'using gcc : : %s ;\n' '$(CXX)' > '$(BOOST_USER_CONFIG)'
	@cd '$(BOOST_SRC)' && ./bootstrap.sh

boost-build: boost-bootstrap
	@sh bootstrap/boost-install.sh prepare '$(BOOTSTRAP_ROOT)' '$(BOOST_PREFIX)' '$(BOOST_VERSION)' '$(BOOST_COMMIT)'
	@mkdir -p '$(BOOST_PREFIX)'
	@cd '$(BOOST_SRC)' && ./b2 -j'$(BOOST_JOBS)' variant='$(BOOST_VARIANT)' link=static threading=multi --prefix='$(BOOST_PREFIX)' $(foreach lib,$(BOOST_COMPONENTS),--with-$(lib)) install
	@cp '$(BOOTSTRAP_ROOT)/boost-commit.txt' '$(BOOST_PREFIX)/boost-commit.txt'
	@cp '$(BOOTSTRAP_ROOT)/boost-version.txt' '$(BOOST_PREFIX)/boost-version.txt'
	@sh bootstrap/boost-install.sh record '$(BOOTSTRAP_ROOT)' '$(BOOST_PREFIX)' '$(BOOST_VERSION)' '$(BOOST_COMMIT)'
	@touch '$(BOOST_STAMP)'

boost: boost-build

# Local development CLI only; never fetch or rebuild Boost.
bootstrap-dev:
	@set -eu; \
	  test -n '$(BOOST_VERSION)' && test '$(BOOST_VERSION)' != latest || { echo 'ERROR: set BOOST_VERSION to a verified exact Boost release' >&2; exit 1; }; \
	  printf '%s' '$(BOOST_COMMIT)' | grep -Eq '^[[:xdigit:]]{40}$$' || { echo 'ERROR: set BOOST_COMMIT to the verified full 40-hex commit' >&2; exit 1; }; \
	  sh bootstrap/boost-install.sh verify '$(BOOTSTRAP_ROOT)' '$(BOOST_PREFIX)' '$(BOOST_VERSION)' '$(BOOST_COMMIT)'; \
	  mkdir -p '$(BOOTSTRAP_ROOT)/dev'; \
	  dir=$$(mktemp -d '$(BOOTSTRAP_ROOT)/dev/run.XXXXXXXX'); \
	  dir=$$(cd "$$dir" && pwd -P); \
	  cd "$$dir"; \
	  set --; \
	  for src in \
	    projects/cap-core/diagnostic.cppm projects/cap-core/filesystem.cppm projects/cap-core/process.cppm \
	    projects/cap-manifest/manifest.cppm projects/cap-manifest/sources.cppm projects/cap-workspace/workspace.cppm \
	    projects/cap-workspace/dependencies.cppm \
	    projects/cap-build/actions.cppm projects/cap-build/modules.cppm projects/cap-build/native_binary.cppm \
	    projects/cap-compdb/src/compdb.cppm projects/cap-toolchain/host.cppm \
	    projects/cap-cli/build_options.cppm projects/cap-cli/workspace_targets.cppm projects/cap-cli/standalone.cppm; do \
	    obj=$${src##*/}; obj=$${obj%.cppm}.o; \
	    $(CXX) -std=c++26 -Wall -Wextra -Werror -fmodules -isystem '$(abspath $(BOOST_PREFIX))/include' -x c++ -c '$(CURDIR)/'$$src -o "$$obj"; \
	    set -- "$$@" "$$obj"; \
	  done; \
	  $(CXX) -std=c++26 -Wall -Wextra -Werror -fmodules -c '$(CURDIR)/projects/cap-cli/main.cpp' -o main.o; \
	  $(CXX) -fmodules main.o "$$@" '$(abspath $(BOOST_PREFIX))/lib/libboost_json.a' -o cap; \
	  ./cap build --help >/dev/null; \
	  printf 'development cap: %s/cap\n' "$$dir"

# cap-core M0 contract tests (stage-0: pinned system compiler, assert-based runner, no framework).
core-test: prerequisites
	@mkdir -p '$(BOOTSTRAP_ROOT)/bin' '$(BOOTSTRAP_ROOT)/modules/gcc-normal'
	@for module in $(CAP_CORE_MODULES); do \
	  (cd '$(BOOTSTRAP_ROOT)/modules/gcc-normal' && $(CXX) -std=c++26 -Wall -Wextra -Werror -fmodules -x c++ -c '$(CURDIR)/projects/cap-core/'$$module.cppm -o $$module.o) || exit 1; \
	done
	@for src in $(CAP_CORE_TESTS); do \
	  name=$${src##*/}; name=$${name%.cpp}; \
	  (cd '$(BOOTSTRAP_ROOT)/modules/gcc-normal' && $(CXX) -std=c++26 -Wall -Wextra -Werror -fmodules -I'$(CURDIR)/projects/cap-core/include' -I'$(CURDIR)/projects/cap-core/tests' '$(CURDIR)/'$$src $(addsuffix .o,$(CAP_CORE_MODULES)) -o "$(BOOTSTRAP_ROOT)/bin/cap-core-$$name") || exit 1; \
	done
	@'$(BOOTSTRAP_ROOT)/bin/cap-core-diagnostic_test' '$(CAP_CORE_TEST_GOLDEN)'
	@'$(BOOTSTRAP_ROOT)/bin/cap-core-filesystem_test'
	@'$(BOOTSTRAP_ROOT)/bin/cap-core-process_test'
	@'$(BOOTSTRAP_ROOT)/bin/cap-core-digest_test'
	@'$(BOOTSTRAP_ROOT)/bin/cap-core-primitives_test'

# Sanitizer jobs on the pinned clang: ASan+UBSan+LeakSanitizer (all suites,
# separate binaries from TSan), TSan (diagnostic and filesystem suites only).
# Rejected combinations are documented, never claimed (docs/QUALITY.md):
# MSan (no fully instrumented dependencies on this host),
# TSan+process_test (child processes are uninstrumented).
sanitize: prerequisites
	@command -v $(CLANG) >/dev/null || { echo 'ERROR: pinned clang not found at $(CLANG)' >&2; exit 1; }
	@mkdir -p '$(BOOTSTRAP_ROOT)/sanitize'
	@mkdir -p '$(BOOTSTRAP_ROOT)/modules/clang-asan'
	@for module in $(CAP_CORE_MODULES); do \
	  $(CLANG) -std=c++26 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Iprojects/cap-core/include -Iprojects/cap-core/tests -fprebuilt-module-path='$(BOOTSTRAP_ROOT)/modules/clang-asan' --precompile projects/cap-core/$$module.cppm -o '$(BOOTSTRAP_ROOT)/modules/clang-asan/cap.core.'$$module.pcm || exit 1; \
	  $(CLANG) -std=c++26 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -fprebuilt-module-path='$(BOOTSTRAP_ROOT)/modules/clang-asan' -c '$(BOOTSTRAP_ROOT)/modules/clang-asan/cap.core.'$$module.pcm -o '$(BOOTSTRAP_ROOT)/modules/clang-asan/'$$module.o || exit 1; \
	done
	@for src in $(CAP_CORE_TESTS); do \
	  name=$${src##*/}; name=$${name%.cpp}; \
	  $(CLANG) -std=c++26 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Iprojects/cap-core/include -Iprojects/cap-core/tests -fprebuilt-module-path='$(BOOTSTRAP_ROOT)/modules/clang-asan' $$src $(addprefix $(BOOTSTRAP_ROOT)/modules/clang-asan/,$(addsuffix .o,$(CAP_CORE_MODULES))) -o "$(BOOTSTRAP_ROOT)/sanitize/asan-$$name" || exit 1; \
	done
	@'$(BOOTSTRAP_ROOT)/sanitize/asan-diagnostic_test' '$(CAP_CORE_TEST_GOLDEN)'
	@'$(BOOTSTRAP_ROOT)/sanitize/asan-filesystem_test'
	@'$(BOOTSTRAP_ROOT)/sanitize/asan-process_test'
	@'$(BOOTSTRAP_ROOT)/sanitize/asan-digest_test'
	@'$(BOOTSTRAP_ROOT)/sanitize/asan-primitives_test'
	@mkdir -p '$(BOOTSTRAP_ROOT)/modules/clang-tsan'
	@for module in $(CAP_CORE_MODULES); do \
	  $(CLANG) -std=c++26 -Wall -Wextra -fsanitize=thread -fno-sanitize-recover=all -fprebuilt-module-path='$(BOOTSTRAP_ROOT)/modules/clang-tsan' --precompile projects/cap-core/$$module.cppm -o '$(BOOTSTRAP_ROOT)/modules/clang-tsan/cap.core.'$$module.pcm || exit 1; \
	  $(CLANG) -std=c++26 -Wall -Wextra -fsanitize=thread -fno-sanitize-recover=all -fprebuilt-module-path='$(BOOTSTRAP_ROOT)/modules/clang-tsan' -c '$(BOOTSTRAP_ROOT)/modules/clang-tsan/cap.core.'$$module.pcm -o '$(BOOTSTRAP_ROOT)/modules/clang-tsan/'$$module.o || exit 1; \
	done
	@for name in $(CAP_CORE_TSAN_TESTS); do \
	  src=projects/cap-core/tests/$$name.cpp; \
	  $(CLANG) -std=c++26 -Wall -Wextra -fsanitize=thread -fno-sanitize-recover=all -Iprojects/cap-core/include -Iprojects/cap-core/tests -fprebuilt-module-path='$(BOOTSTRAP_ROOT)/modules/clang-tsan' $$src $(addprefix $(BOOTSTRAP_ROOT)/modules/clang-tsan/,$(addsuffix .o,$(CAP_CORE_MODULES))) -o "$(BOOTSTRAP_ROOT)/sanitize/tsan-$$name" || exit 1; \
	done
	@'$(BOOTSTRAP_ROOT)/sanitize/tsan-diagnostic_test' '$(CAP_CORE_TEST_GOLDEN)'
	@'$(BOOTSTRAP_ROOT)/sanitize/tsan-filesystem_test'
	@printf '%s\n' 'sanitize: ASan+UBSan+LeakSanitizer (all suites) and TSan (diagnostic_test, filesystem_test) passed.' 'sanitize: TSan digest_test and primitives_test not run; TSan process_test not run (uninstrumented children).' 'sanitize: MSan not run (no fully instrumented dependencies).'

# Module coverage: g++-16 instrumented build, lcov merge, genhtml report,
# baseline gate (projects/cap-core/COVERAGE-WAIVER.md).
# lcov's C/C++ branch filter needs cppm added to its default extensions.
core-coverage: prerequisites
	@command -v $(LCov) >/dev/null || { echo 'ERROR: lcov is required (brew install lcov)' >&2; exit 1; }
	@rm -rf '$(BOOTSTRAP_ROOT)/coverage'
	@mkdir -p '$(BOOTSTRAP_ROOT)/coverage'
	@for module in $(CAP_CORE_MODULES); do \
	  (cd '$(BOOTSTRAP_ROOT)/coverage' && $(CXX) -std=c++26 -O1 -fprofile-arcs -ftest-coverage -fmodules -x c++ -c '$(CURDIR)/projects/cap-core/'$$module.cppm -o $$module.o) || exit 1; \
	done
	@for src in $(CAP_CORE_TESTS); do \
	  name=$${src##*/}; name=$${name%.cpp}; \
	  (cd '$(BOOTSTRAP_ROOT)/coverage' && $(CXX) -std=c++26 -O1 -fprofile-arcs -ftest-coverage -fmodules -I'$(CURDIR)/projects/cap-core/include' -I'$(CURDIR)/projects/cap-core/tests' -c '$(CURDIR)/'$$src -o $$name.o) || exit 1; \
	done
	@for obj in '$(BOOTSTRAP_ROOT)/coverage'/*.o; do \
	  name=$${obj##*/}; name=$${name%.o}; \
	  if test -f "projects/cap-core/tests/$$name.cpp"; then \
	    $(CXX) -O1 -fprofile-arcs $$obj $(addprefix $(BOOTSTRAP_ROOT)/coverage/,$(addsuffix .o,$(CAP_CORE_MODULES))) -o "$(BOOTSTRAP_ROOT)/coverage/cov-$$name" || exit 1; \
	  fi; \
	done
	@'$(BOOTSTRAP_ROOT)/coverage/cov-diagnostic_test' '$(CAP_CORE_TEST_GOLDEN)'
	@'$(BOOTSTRAP_ROOT)/coverage/cov-filesystem_test'
	@'$(BOOTSTRAP_ROOT)/coverage/cov-process_test'
	@'$(BOOTSTRAP_ROOT)/coverage/cov-digest_test'
	@'$(BOOTSTRAP_ROOT)/coverage/cov-primitives_test'
	@$(LCov) --capture --directory '$(BOOTSTRAP_ROOT)/coverage' --gcov-tool $(GCovTool) --filter branch --rc branch_coverage=1 --rc c_file_extensions=c,h,i,C,H,I,icc,cpp,cc,cxx,hh,hpp,hxx,cppm $(foreach module,$(CAP_CORE_MODULES),--include '*/projects/cap-core/$(module).cppm') --output-file '$(BOOTSTRAP_ROOT)/coverage/module.info'
	@test -f '$(GENHTML)' || { \
	  test -f '$(LCOV_TARBALL)' || curl -fsSL -o '$(LCOV_TARBALL)' "https://github.com/linux-test-project/lcov/releases/download/v$(LCOV_VERSION)/lcov-$(LCOV_VERSION).tar.gz" || exit 1; \
	  tar xzf '$(LCOV_TARBALL)' -C '$(BOOTSTRAP_ROOT)' 'lcov-$(LCOV_VERSION)/bin/genhtml' 'lcov-$(LCOV_VERSION)/lib' || exit 1; \
	}
	@PERL5LIB='$(LCOV_PERL_LIB)' $(GENHTML) '$(BOOTSTRAP_ROOT)/coverage/module.info' --branch-coverage --rc branch_coverage=1 --output-directory '$(BOOTSTRAP_ROOT)/coverage/html' > /dev/null
	@$(LCov) --summary '$(BOOTSTRAP_ROOT)/coverage/module.info' --rc branch_coverage=1 > '$(BOOTSTRAP_ROOT)/coverage/summary.txt'
	@cat '$(BOOTSTRAP_ROOT)/coverage/summary.txt'
	@awk -F'[:%]' '/^ *lines/ {gsub(/[^0-9.]/,"",$$2); l=$$2} /^ *branches/ {gsub(/[^0-9.]/,"",$$2); b=$$2} END { printf "coverage gate: lines=%s%% branches=%s%% (baseline >=%s%% / >=%s%%)\n", l, b, MINL, MINB; exit !(l+0 >= MINL+0 && b+0 >= MINB+0) }' MINL='$(CAP_COV_BASELINE_LINE)' MINB='$(CAP_COV_BASELINE_BRANCH)' '$(BOOTSTRAP_ROOT)/coverage/summary.txt' \
	  || { echo 'ERROR: cap-core coverage regressed below the recorded baseline; see projects/cap-core/COVERAGE-WAIVER.md' >&2; exit 1; }
	@sh projects/cap-core/tests/digest-coverage-gate.sh '$(BOOTSTRAP_ROOT)/coverage/module.info' '$(CURDIR)/projects/cap-core/digest.cppm'
	@echo 'core-coverage: report at .bootstrap/coverage/html/index.html'

# Stage-0 bootstrap: the pinned compiler builds the driver, which compiles
# and verifies the stage-1 executable against the installed Boost.
# M0 exit criterion: stage-1 executable produced without an external build
# system. Stage-2 self-hosting is a later milestone (self-host target).
bootstrap: boost-build
	@sh bootstrap/boost-install.sh verify '$(BOOTSTRAP_ROOT)' '$(BOOST_PREFIX)' '$(BOOST_VERSION)' '$(BOOST_COMMIT)'
	@test -f '$(CAP_BOOTSTRAP_SRC)' || { echo 'ERROR: $(CAP_BOOTSTRAP_SRC) missing' >&2; exit 1; }
	@mkdir -p '$(BOOTSTRAP_ROOT)/bin' '$(BOOTSTRAP_ROOT)/modules/bootstrap'
	@cd '$(BOOTSTRAP_ROOT)/modules/bootstrap' && $(CXX) -std=c++26 -O2 -Wall -Wextra -fmodules -x c++ -c '$(CURDIR)/projects/cap-core/diagnostic.cppm' -o diagnostic.o
	@cd '$(BOOTSTRAP_ROOT)/modules/bootstrap' && $(CXX) -std=c++26 -O2 -Wall -Wextra -fmodules -x c++ -c '$(CURDIR)/projects/cap-core/filesystem.cppm' -o filesystem.o
	@cd '$(BOOTSTRAP_ROOT)/modules/bootstrap' && $(CXX) -std=c++26 -O2 -Wall -Wextra -fmodules -x c++ -c '$(CURDIR)/projects/cap-core/process.cppm' -o process.o
	@cd '$(BOOTSTRAP_ROOT)/modules/bootstrap' && $(CXX) -std=c++26 -O2 -Wall -Wextra -fmodules -x c++ -c '$(CURDIR)/bootstrap/path_io.cppm' -o path_io.cppm.o
	@cd '$(BOOTSTRAP_ROOT)/modules/bootstrap' && $(CXX) -std=c++26 -O2 -Wall -Wextra -fmodules -x c++ -c '$(CURDIR)/bootstrap/path_io.cpp' -o path_io.cpp.o
	@cd '$(BOOTSTRAP_ROOT)/modules/bootstrap' && $(CXX) -std=c++26 -O2 -Wall -Wextra -fmodules -I'$(BOOST_PREFIX)/include' -x c++ -c '$(CURDIR)/bootstrap/stage_status.cppm' -o stage_status.cppm.o
	@cd '$(BOOTSTRAP_ROOT)/modules/bootstrap' && $(CXX) -std=c++26 -O2 -Wall -Wextra -fmodules -I'$(BOOST_PREFIX)/include' -x c++ -c '$(CURDIR)/bootstrap/stage_status.cpp' -o stage_status.cpp.o
	@cd '$(BOOTSTRAP_ROOT)/modules/bootstrap' && $(CXX) -std=c++26 -O2 -Wall -Wextra -fmodules -I'$(BOOST_PREFIX)/include' -I'$(CURDIR)/projects/cap-core/include' '$(abspath $(CAP_BOOTSTRAP_SRC))' diagnostic.o filesystem.o process.o path_io.cppm.o path_io.cpp.o stage_status.cppm.o stage_status.cpp.o -L'$(BOOST_PREFIX)/lib' -lboost_filesystem -lboost_program_options -lboost_json -o '$(CAP_BOOTSTRAP_BIN)'
	@'$(CAP_BOOTSTRAP_BIN)' --boost-prefix '$(BOOST_PREFIX)' --source-root '$(CURDIR)' --output-dir '$(BOOTSTRAP_ROOT)/bin'
	@test -x '$(BOOTSTRAP_ROOT)/bin/cap-stage1' || { echo 'ERROR: stage-1 executable not produced' >&2; exit 1; }
	@'$(BOOTSTRAP_ROOT)/bin/cap-stage1' --boost-prefix '$(BOOST_PREFIX)'

self-host:
	@echo 'ERROR: self-host stage-2 build and stage comparison are not implemented.' >&2
	@exit 1

test check coverage bench profile install: bootstrap
	@echo 'NOT IMPLEMENTED: $@ will delegate to the bootstrapped cap binary.' >&2
	@exit 1

boost-clean:
	@rm -rf '$(BOOST_SRC)' '$(BOOST_PREFIX)' '$(BOOTSTRAP_ROOT)/boost-commit.txt' '$(BOOTSTRAP_ROOT)/boost-version.txt' '$(BOOTSTRAP_ROOT)/boost-submodules.txt'
clean:
	@rm -rf '$(BOOTSTRAP_ROOT)'
