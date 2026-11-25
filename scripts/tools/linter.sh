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

# C Auditing targets (relative to project root)
LINT_TARGETS_C="src/toggle.c \
                src/run.c \
                src/affinity.c \
                src/error.c \
                src/games.c \
                src/irq.c \
                src/libc.c \
                src/scheduler.c \
                src/socket.c \
                src/status.c \
                src/stress.c \
                src/systemd.c \
                src/worker.c \
                src/cli/*.c \
                src/daemon/*.c \
                src/daemon/polling/*.c \
                src/daemon/bpf/bpf-user.c"

LINT_TARGETS_SH="*.sh \
                 scripts/framework/*.sh \
                 scripts/tools/*.sh \
                 dev/*.sh"

ROOT_REPO="$(cd "$(dirname "$0")/../.." && pwd)"
if [ ! -f "$ROOT_REPO/Makefile" ] && [ -f "$PWD/Makefile" ]; then
    ROOT_REPO="$PWD"
fi
. "$ROOT_REPO/scripts/framework/framework.sh"

if [ -f "/etc/x3d-toggle.d/settings.conf" ]; then
    . "/etc/x3d-toggle.d/settings.conf"
elif [ -f "$ROOT_REPO/config/settings.conf" ]; then
    . "$ROOT_REPO/config/settings.conf"
fi

# Safely parse LINT_* keys from daemon.conf without sourcing [GAMES_*] sections
load_daemon_lint_conf() {
    _conf="$1"
    [ -f "$_conf" ] || return 0
    _cr=$(printf '\r')
    while IFS= read -r _line || [ -n "$_line" ]; do
        case "$_line" in
            "[GAMES_"*) break ;;
            LINT_*=*)
                _key="${_line%%=*}"
                _val="${_line#*=}"
                _val="${_val%"$_cr"}"
                case "$_key" in
                    LINT_CLANG_DIAGNOSTIC) LINT_CLANG_DIAGNOSTIC="$_val" ;;
                    LINT_CLANG_BUGPRONE) LINT_CLANG_BUGPRONE="$_val" ;;
                    LINT_CLANG_MODERNIZE) LINT_CLANG_MODERNIZE="$_val" ;;
                    LINT_CLANG_READABILITY) LINT_CLANG_READABILITY="$_val" ;;
                    LINT_CLANG_PERFORMANCE) LINT_CLANG_PERFORMANCE="$_val" ;;
                    LINT_CLANG_PORTABILITY) LINT_CLANG_PORTABILITY="$_val" ;;
                    LINT_CLANG_ANALYZER) LINT_CLANG_ANALYZER="$_val" ;;
                    LINT_CPPCHECK_ALL) LINT_CPPCHECK_ALL="$_val" ;;
                    LINT_CPPCHECK_WARNING) LINT_CPPCHECK_WARNING="$_val" ;;
                    LINT_CPPCHECK_STYLE) LINT_CPPCHECK_STYLE="$_val" ;;
                    LINT_CPPCHECK_PERFORMANCE) LINT_CPPCHECK_PERFORMANCE="$_val" ;;
                    LINT_CPPCHECK_PORTABILITY) LINT_CPPCHECK_PORTABILITY="$_val" ;;
                    LINT_CPPCHECK_INFORMATION) LINT_CPPCHECK_INFORMATION="$_val" ;;
                    LINT_CPPCHECK_UNUSED) LINT_CPPCHECK_UNUSED="$_val" ;;
                    LINT_VALGRIND_MODE) LINT_VALGRIND_MODE="$_val" ;;
                    LINT_VALGRIND_KINDS) LINT_VALGRIND_KINDS="$_val" ;;
                    LINT_VALGRIND_ORIGINS) LINT_VALGRIND_ORIGINS="$_val" ;;
                esac
                ;;
        esac
    done < "$_conf"
}

if [ -f "/etc/x3d-toggle.d/daemon.conf" ]; then
    load_daemon_lint_conf "/etc/x3d-toggle.d/daemon.conf"
elif [ -f "$ROOT_REPO/build/daemon.conf" ]; then
    load_daemon_lint_conf "$ROOT_REPO/build/daemon.conf"
fi

MODE="${1:-all}"

CLANG_TIDY_CHECKS=""
[ "${LINT_CLANG_DIAGNOSTIC:-disabled}" = "enabled" ] && CLANG_TIDY_CHECKS="${CLANG_TIDY_CHECKS}clang-diagnostic-*,"
[ "${LINT_CLANG_BUGPRONE:-disabled}" = "enabled" ] && CLANG_TIDY_CHECKS="${CLANG_TIDY_CHECKS}bugprone-*,"
[ "${LINT_CLANG_MODERNIZE:-disabled}" = "enabled" ] && CLANG_TIDY_CHECKS="${CLANG_TIDY_CHECKS}modernize-*,"
[ "${LINT_CLANG_READABILITY:-disabled}" = "enabled" ] && CLANG_TIDY_CHECKS="${CLANG_TIDY_CHECKS}readability-*,"
[ "${LINT_CLANG_PERFORMANCE:-disabled}" = "enabled" ] && CLANG_TIDY_CHECKS="${CLANG_TIDY_CHECKS}performance-*,"
[ "${LINT_CLANG_PORTABILITY:-disabled}" = "enabled" ] && CLANG_TIDY_CHECKS="${CLANG_TIDY_CHECKS}portability-*,"
[ "${LINT_CLANG_ANALYZER:-disabled}" = "enabled" ] && CLANG_TIDY_CHECKS="${CLANG_TIDY_CHECKS}clang-analyzer-*,"
# Filter C++-only and C11/-nostdlib structural noise so genuine bugs stand out
CLANG_TIDY_CHECKS="${CLANG_TIDY_CHECKS}-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling,-bugprone-reserved-identifier,-bugprone-easily-swappable-parameters,-bugprone-unchecked-string-to-number-conversion,-bugprone-signed-bitwise,-bugprone-command-processor,-bugprone-random-generator-seed,-portability-no-assembler,-readability-magic-numbers,-readability-identifier-length,-readability-braces-around-statements,-readability-inconsistent-ifelse-braces,-readability-isolate-declaration,-readability-function-cognitive-complexity,-readability-redundant-declaration,-readability-trailing-comma,-readability-else-after-return,-readability-redundant-nested-if,-readability-redundant-parentheses,-readability-redundant-casting,-readability-implicit-bool-conversion,-readability-avoid-nested-conditional-operator,-readability-non-const-parameter,-modernize-macro-to-enum,-modernize-avoid-c-style-cast"

CPPCHECK_FLAGS="--error-exitcode=1 --std=c11 --library=posix --platform=unix64 --suppress=missingIncludeSystem --suppress=missingInclude --suppress=normalCheckLevelMaxBranches --suppress=checkLevelNormal --suppress=checkersReport --suppress=unusedFunction --suppress=staticFunction --suppress=constParameter --suppress=constParameterCallback"
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
if ! mkdir -p "$JOURNAL_AUDIT" 2>/dev/null || [ ! -w "$JOURNAL_AUDIT" ]; then
    JOURNAL_AUDIT="$ROOT_REPO/dev/logging/audits/linter_$TIMESTAMP"
    if ! mkdir -p "$JOURNAL_AUDIT" 2>/dev/null || [ ! -w "$JOURNAL_AUDIT" ]; then
        JOURNAL_AUDIT="${TMPDIR:-/tmp}/x3d-toggle-audits/linter_$TIMESTAMP"
        mkdir -p "$JOURNAL_AUDIT"
    fi
fi

HAS_SOURCE=0
[ -f "$ROOT_REPO/Makefile" ] && HAS_SOURCE=1

RUN_CLANG=0
RUN_CPPCHECK=0
RUN_VALGRIND=0
RUN_SHELL=0

case "$MODE" in
    clang) RUN_CLANG=1 ;;
    cppc) RUN_CPPCHECK=1 ;;
    valgrind) RUN_VALGRIND=1 ;;
    shell) RUN_SHELL=1 ;;
    *) RUN_CLANG=1; RUN_CPPCHECK=1; RUN_VALGRIND=1; RUN_SHELL=1 ;;
esac

if [ "$HAS_SOURCE" -eq 0 ] && { [ "$RUN_CLANG" -eq 1 ] || [ "$RUN_CPPCHECK" -eq 1 ] || [ "$RUN_SHELL" -eq 1 ]; }; then
    printf_step "2,${WARN} Source tree not found at $ROOT_REPO; run from source checkout for clang-tidy/cppcheck/shellcheck."
    RUN_CLANG=0
    RUN_CPPCHECK=0
    RUN_SHELL=0
fi

# ── 1. Clang-Tidy ───────────────────────────────────────────────
if [ "$RUN_CLANG" -eq 1 ]; then
    printf_step "${HAMMER} Building Project (Silent Build) via bear for clang-tidy..."
    make -s framework > "$JOURNAL_AUDIT/build.log" 2>&1 || journal_write -audit "Build" "$JOURNAL_AUDIT/build.log" "$?"
    bear --output build/compile_commands.json -- make -s -B build SKIP_FRAMEWORK=1 >> "$JOURNAL_AUDIT/build.log" 2>&1 || journal_write -audit "Build" "$JOURNAL_AUDIT/build.log" "$?"
    
    printf_step "${GEAR} Running clang-tidy (Advanced Audit)..."
    # shellcheck disable=SC2086
    clang-tidy -p build/ -checks="${CLANG_TIDY_CHECKS:-clang-diagnostic-*,clang-analyzer-*}" $LINT_TARGETS_C > "$JOURNAL_AUDIT/clang-tidy.txt" 2>&1 || journal_write -audit "clang-tidy" "$JOURNAL_AUDIT/clang-tidy.txt" "$?"
fi

# ── 2. Cppcheck ─────────────────────────────────────────────────
if [ "$RUN_CPPCHECK" -eq 1 ]; then
    printf_step "${SEARCH} Running cppcheck (C11/POSIX Scan) for bugs..."
    make -s framework >/dev/null 2>&1 || true
    # shellcheck disable=SC2086
    cppcheck --inline-suppr --suppress=unmatchedSuppression $CPPCHECK_FLAGS -Ibuild -Iinclude -D__asm__\(x\)= -ibuild -isrc/libc.c -isrc/daemon/bpf/bpf.c src > "$JOURNAL_AUDIT/cppcheck.txt" 2>&1 || journal_write -audit "cppcheck" "$JOURNAL_AUDIT/cppcheck.txt" "$?"
fi

# ── 3. Valgrind ─────────────────────────────────────────────────
if [ "$RUN_VALGRIND" -eq 1 ]; then
    printf_step "${WAND} Running valgrind (Status Check) for memory leaks..."
    _val_bin="./bin/x3d-toggle"
    [ ! -x "$_val_bin" ] && [ -x "/usr/bin/x3d-toggle" ] && _val_bin="/usr/bin/x3d-toggle"
    # shellcheck disable=SC2086
    valgrind $VALGRIND_FLAGS "$_val_bin" status > "$JOURNAL_AUDIT/valgrind.txt" 2>&1
    VAL_RES=$?
    if [ $VAL_RES -ne 0 ]; then
        _fatal_error=0
        while IFS= read -r _l_line || [ -n "$_l_line" ]; do
            case "$_l_line" in
                *"Fatal error at startup"*|*"cannot be set up"*|*"ld-linux"*)
                    _fatal_error=1
                    break
                    ;;
            esac
        done < "$JOURNAL_AUDIT/valgrind.txt"
        if [ "$_fatal_error" -eq 1 ]; then
            rm -f vgcore.*
            printf_step "2,${WARN} Notice: Valgrind startup failed (Missing glibc debuginfo / stripped ld.so)."
            printf_step "2,        This is a system environment issue, not a project memory leak."
        else
            journal_write -audit "valgrind" "$JOURNAL_AUDIT/valgrind.txt" "$VAL_RES"
        fi
    fi
fi

# ── 4. Shellcheck ───────────────────────────────────────────────
if [ "$RUN_SHELL" -eq 1 ]; then
    printf_step "${SHIELD} Running shellcheck for syntax errors..."
    # shellcheck disable=SC2086
    shellcheck -e SC1090,SC1091 $LINT_TARGETS_SH > "$JOURNAL_AUDIT/shellcheck.txt" 2>&1 || journal_write -audit "shellcheck" "$JOURNAL_AUDIT/shellcheck.txt" "$?"
fi

# ── 5. Report Presentation ──────────────────────────────────────
if [ "$MODE" != "all" ]; then
    printf_br
    printf_step "📜 Analysis Report for $MODE:"
    case "$MODE" in
        clang) _rep_file="$JOURNAL_AUDIT/clang-tidy.txt" ;;
        cppc) _rep_file="$JOURNAL_AUDIT/cppcheck.txt" ;;
        valgrind) _rep_file="$JOURNAL_AUDIT/valgrind.txt" ;;
        shell) _rep_file="$JOURNAL_AUDIT/shellcheck.txt" ;;
    esac
    
    if [ -f "$_rep_file" ]; then
        if [ ! -s "$_rep_file" ]; then
            printf "    No issues found!\n"
        else
            while IFS= read -r _l_line || [ -n "$_l_line" ]; do
                echo "$_l_line"
            done < "$_rep_file"
        fi
    else
        printf "    Report file not found or check skipped.\n"
    fi
    printf_br
    if [ -t 0 ]; then
        printf "    Press [ENTER] to close window..."
        read -r _unused
    fi
else
    printf_step "${ALRIGHT} Linter suite complete. Results saved in $JOURNAL_AUDIT"
fi

exit 0

## end of LINTER.SH