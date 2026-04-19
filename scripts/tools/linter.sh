#!/bin/sh
## Automated Linter Suite Auditing for the X3D Toggle Project
##
## `linter.sh`
##
## Usage: x3d-toggle linter [ARGS]
## [ARGS] = clang,cppc,valgrind,shell,all
##
## This script automates the auditing of the project by running a full suite of 
## linting checks including shellcheck, clang-tidy, cppcheck, and valgrind.
## Maintains journals in `/var/log/x3d-toggle/audits/`

# C/C++ Auditing targets (relative to project root): Adjust as needed
LINT_TARGETS_C="src/*.c \
                src/cli/*.c \
                src/daemon/*/*.c"

LINT_TARGETS_SH="*.sh scripts/framework/*.sh \
                *.sh scripts/tools/*.sh"

ROOT_REPO="$(cd "$(dirname "$0")/../.." && pwd)"
. "$ROOT_REPO/scripts/framework/framework.sh"

if [ -f "$ROOT_REPO/config/settings.conf" ]; then
    . "$ROOT_REPO/config/settings.conf"
fi

CLANG_TIDY_CHECKS=""
[ "${LINT_CLANG_DIAGNOSTIC:-disabled}" = "enabled" ] && CLANG_TIDY_CHECKS="${CLANG_TIDY_CHECKS}clang-diagnostic-*,"
[ "${LINT_CLANG_BUGPRONE:-disabled}" = "enabled" ] && CLANG_TIDY_CHECKS="${CLANG_TIDY_CHECKS}bugprone-*,"
[ "${LINT_CLANG_MODERNIZE:-disabled}" = "enabled" ] && CLANG_TIDY_CHECKS="${CLANG_TIDY_CHECKS}modernize-*,"
[ "${LINT_CLANG_READABILITY:-disabled}" = "enabled" ] && CLANG_TIDY_CHECKS="${CLANG_TIDY_CHECKS}readability-*,"
[ "${LINT_CLANG_PERFORMANCE:-disabled}" = "enabled" ] && CLANG_TIDY_CHECKS="${CLANG_TIDY_CHECKS}performance-*,"
[ "${LINT_CLANG_PORTABILITY:-disabled}" = "enabled" ] && CLANG_TIDY_CHECKS="${CLANG_TIDY_CHECKS}portability-*,"
[ "${LINT_CLANG_ANALYZER:-disabled}" = "enabled" ] && CLANG_TIDY_CHECKS="${CLANG_TIDY_CHECKS}clang-analyzer-*,"
CLANG_TIDY_CHECKS="${CLANG_TIDY_CHECKS%,}" # Strip trailing comma

CPPCHECK_FLAGS="--error-exitcode=1 --std=c11 --library=posix --platform=unix64 --suppress=missingIncludeSystem"
CPP_EN=""
[ "${LINT_CPPCHECK_ALL:-disabled}" = "enabled" ] && CPP_EN="${CPP_EN}all,"
[ "${LINT_CPPCHECK_WARNING:-disabled}" = "enabled" ] && CPP_EN="${CPP_EN}warning,"
[ "${LINT_CPPCHECK_STYLE:-disabled}" = "enabled" ] && CPP_EN="${CPP_EN}style,"
[ "${LINT_CPPCHECK_PERFORMANCE:-disabled}" = "enabled" ] && CPP_EN="${CPP_EN}performance,"
[ "${LINT_CPPCHECK_PORTABILITY:-disabled}" = "enabled" ] && CPP_EN="${CPP_EN}portability,"
[ "${LINT_CPPCHECK_INFORMATION:-disabled}" = "enabled" ] && CPP_EN="${CPP_EN}information,"
[ "${LINT_CPPCHECK_UNUSED:-disabled}" = "enabled" ] && CPP_EN="${CPP_EN}unusedFunction,"
CPP_EN="${CPP_EN%,}"
[ -n "$CPP_EN" ] && CPPCHECK_FLAGS="${CPPCHECK_FLAGS} --enable=${CPP_EN}"

VALGRIND_FLAGS="--show-leak-kinds=${LINT_VALGRIND_KINDS:-all}"
[ "${LINT_VALGRIND_MODE:-disabled}" != "disabled" ] && VALGRIND_FLAGS="${VALGRIND_FLAGS} --leak-check=${LINT_VALGRIND_MODE}"
[ "${LINT_VALGRIND_ORIGINS:-disabled}" = "enabled" ] && VALGRIND_FLAGS="${VALGRIND_FLAGS} --track-origins=yes"

TIMESTAMP=$(date +"%Y-%m-%d_%H-%M-%S")
JOURNAL_AUDIT="$VAR_AUDITS/linter_$TIMESTAMP"

cd "$ROOT_REPO" || exit 1



printf_step "${HAMMER} Building Project (Silent Build) via bear for clang-tidy..."
bear --output build/compile_commands.json -- make -s build > "$JOURNAL_AUDIT/build.log" 2>&1 || journal_write -audit "Build" "$JOURNAL_AUDIT/build.log" "$?"

printf_step "${GEAR} Running clang-tidy (Advanced Audit)..."
# shellcheck disable=SC2086
clang-tidy -p build/ -checks="$CLANG_TIDY_CHECKS" $LINT_TARGETS_C > "$JOURNAL_AUDIT/clang-tidy.txt" 2>&1 || journal_write -audit "clang-tidy" "$JOURNAL_AUDIT/clang-tidy.txt" "$?"

printf_step "${SEARCH} Running cppcheck (C11/POSIX Scan) for bugs..."
# shellcheck disable=SC2086
cppcheck $CPPCHECK_FLAGS -Iinclude -Ibuild . > "$JOURNAL_AUDIT/cppcheck.txt" 2>&1 || journal_write -audit "cppcheck" "$JOURNAL_AUDIT/cppcheck.txt" "$?"

printf_step "${WAND} Running valgrind (Status Check) for memory leaks..."
valgrind "$VALGRIND_FLAGS" ./bin/x3d-toggle status > "$JOURNAL_AUDIT/valgrind.txt" 2>&1 || journal_write -audit "valgrind" "$JOURNAL_AUDIT/valgrind.txt" "$?"

printf_step "${SHIELD} Running shellcheck for syntax errors..."
# shellcheck disable=SC2086
shellcheck $LINT_TARGETS_SH > "$JOURNAL_AUDIT/shellcheck.txt" 2>&1 || journal_write -audit "shellcheck" "$JOURNAL_AUDIT/shellcheck.txt" "$?"

printf_step "${ALRIGHT} Linter suite complete. Results saved in $JOURNAL_AUDIT"

## end of LINTER.SH