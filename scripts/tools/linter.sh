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
if [ ! -f "$ROOT_REPO/Makefile" ]; then
    if [ -f "$PWD/Makefile" ]; then
        ROOT_REPO="$PWD"
    elif [ -f "/etc/x3d-toggle.d/source_root" ]; then
        # Installed copy (e.g. launched from the GUI): use the recorded checkout
        _saved_repo="$(head -n 1 /etc/x3d-toggle.d/source_root 2>/dev/null)"
        if [ -n "$_saved_repo" ] && [ -f "$_saved_repo/Makefile" ]; then
            ROOT_REPO="$_saved_repo"
        fi
    fi
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

VALGRIND_FLAGS="--show-leak-kinds=${LINT_VALGRIND_KINDS:-all} --track-fds=yes --error-exitcode=99"
if [ "${LINT_VALGRIND_MODE:-disabled}" != "disabled" ]; then
    VALGRIND_FLAGS="${VALGRIND_FLAGS} --leak-check=${LINT_VALGRIND_MODE} --errors-for-leak-kinds=definite,indirect"
fi
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
    cppcheck --inline-suppr --suppress=unmatchedSuppression $CPPCHECK_FLAGS --library=gtk -Ibuild -Iinclude -Isrc/gtk4 -Isrc/gtk4/pages -Isrc/gtk4/popups -D__asm__\(x\)= -ibuild -isrc/libc.c -isrc/daemon/bpf/bpf.c src > "$JOURNAL_AUDIT/cppcheck.txt" 2>&1 || journal_write -audit "cppcheck" "$JOURNAL_AUDIT/cppcheck.txt" "$?"
fi

# ── 3. Valgrind ─────────────────────────────────────────────────
# Valgrind cannot decode the AVX-512 (EVEX) instructions that x86-64-v4 glibc
# builds (e.g. CachyOS) use inside ld-linux, which kills it with SIGILL before
# the program starts. We probe natively first and, if that fails, run the exact
# same smoke tests inside the `arch-valgrind` container (generic x86-64 glibc).
if [ "$RUN_VALGRIND" -eq 1 ]; then
    printf_step "${WAND} Running valgrind (CLI smoke tests) for memory and FD leaks..."
    _val_bin="$ROOT_REPO/bin/x3d-toggle"
    [ ! -x "$_val_bin" ] && [ -x "/usr/bin/x3d-toggle" ] && _val_bin="/usr/bin/x3d-toggle"
    _val_out="$JOURNAL_AUDIT/valgrind.txt"
    _val_img="${X3D_VALGRIND_IMAGE:-arch-valgrind}"
    : > "$_val_out"

    _real_user="${SUDO_USER:-${USER:-$(id -un)}}"
    _real_home="$(getent passwd "$_real_user" 2>/dev/null | cut -d: -f6)"
    [ -z "$_real_home" ] && _real_home="${HOME:-/root}"

    # Smoke test driver, shared by the native and containerized runs
    _smoke="$JOURNAL_AUDIT/valgrind_smoke.sh"
    cat > "$_smoke" << 'EOF'
#!/bin/sh
# Usage: valgrind_smoke.sh <x3d-toggle binary>   (flags come from $VG_FLAGS)
_bin="$1"
_rc=0
for _args in "status" "--help" "--invalid-flag" "invalid-cmd"; do
    echo "=== valgrind: x3d-toggle $_args ==="
    # shellcheck disable=SC2086
    valgrind $VG_FLAGS "$_bin" $_args
    _r=$?
    echo "=== exit code: $_r ==="
    [ "$_r" -eq 99 ] && _rc=99
done
exit "$_rc"
EOF

    # Silent native probe: no SIGILL banner, no core dump
    sh -c 'ulimit -c 0 2>/dev/null; valgrind --leak-check=no --error-exitcode=99 "$1" --help >/dev/null 2>&1; exit $?' _ "$_val_bin" >/dev/null 2>&1
    _native_probe=$?
    _val_res=0
    _val_ran=1

    if [ "$_native_probe" -eq 0 ] || [ "$_native_probe" -eq 99 ]; then
        VG_FLAGS="$VALGRIND_FLAGS" sh -c 'ulimit -c 0 2>/dev/null; sh "$1" "$2"' _ "$_smoke" "$_val_bin" >> "$_val_out" 2>&1
        _val_res=$?
    else
        # Native Valgrind cannot start here; locate the container runtime
        _docker_mode=""
        if command -v docker >/dev/null 2>&1; then
            if docker image inspect "$_val_img" >/dev/null 2>&1; then
                _docker_mode="direct"
            elif command -v sg >/dev/null 2>&1 && sg docker -c "docker image inspect '$_val_img'" >/dev/null 2>&1; then
                _docker_mode="sg"
            elif sudo -n docker image inspect "$_val_img" >/dev/null 2>&1; then
                _docker_mode="sudo"
            fi
        fi

        run_docker() {
            case "$_docker_mode" in
                direct) docker "$@" ;;
                sudo) sudo -n docker "$@" ;;
                sg)
                    _dcmd="docker"
                    for _darg in "$@"; do
                        _dcmd="$_dcmd '$_darg'"
                    done
                    sg docker -c "$_dcmd"
                    ;;
            esac
        }

        if [ -n "$_docker_mode" ]; then
            printf_step "2,${INFO} Host ld-linux is not Valgrind-compatible; using '$_val_img' container."
            mkdir -p "$_real_home/.cache/valgrind-debuginfod" 2>/dev/null

            # One-time image prep: x3d-toggle links libbpf and libsystemd
            if ! run_docker run --rm "$_val_img" sh -c 'ls /usr/lib/libbpf.so* /usr/lib/libsystemd.so* >/dev/null 2>&1' >/dev/null 2>&1; then
                printf_step "2,${GEAR} Installing libbpf and systemd-libs into '$_val_img' (one-time)..."
                run_docker rm -f x3d-valgrind-prep >/dev/null 2>&1
                if run_docker run --name x3d-valgrind-prep "$_val_img" sh -c 'pacman -Sy --needed --noconfirm libbpf systemd-libs' >> "$_val_out" 2>&1; then
                    run_docker commit x3d-valgrind-prep "$_val_img" >/dev/null 2>&1
                fi
                run_docker rm -f x3d-valgrind-prep >/dev/null 2>&1
            fi

            run_docker run --rm --ulimit core=0 \
                -v "$ROOT_REPO:/work" -w /work \
                -v "$_val_bin:/tmp/x3d-toggle-bin:ro" \
                -v "$_smoke:/tmp/valgrind_smoke.sh:ro" \
                -v "$_real_home/.cache/valgrind-debuginfod:/root/.cache/debuginfod_client" \
                -v /sys:/sys:ro \
                -e "DEBUGINFOD_URLS=https://debuginfod.archlinux.org" \
                -e "VG_FLAGS=$VALGRIND_FLAGS" \
                "$_val_img" sh /tmp/valgrind_smoke.sh /tmp/x3d-toggle-bin >> "$_val_out" 2>&1
            _val_res=$?
        else
            _val_ran=0
            {
                echo "Valgrind could not run on this host (probe exit code: $_native_probe)."
                echo "The host dynamic linker (ld-linux-x86-64.so.2) uses x86-64-v4 AVX-512"
                echo "instructions that Valgrind cannot decode, and the '$_val_img' container"
                echo "is not reachable (docker missing, daemon stopped, or no permission)."
                echo "Build it once, then rerun: x3d-toggle linter valgrind"
            } >> "$_val_out"
            printf_step "2,${WARN} Notice: Valgrind cannot run natively (x86-64-v4 glibc) and '$_val_img' container is unavailable."
            printf_step "2,        This is a system environment issue, not a project memory leak."
        fi
    fi
    rm -f vgcore.* "$_smoke"

    if [ "$_val_ran" -eq 1 ] && [ "$_val_res" -ne 0 ]; then
        journal_write -audit "valgrind" "$_val_out" "$_val_res"
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
    if [ "${X3D_GUI_TERM:-0}" = "1" ] && [ -t 0 ]; then
        printf "    Press [ENTER] to close window..."
        read -r _unused
    fi
else
    printf_step "${ALRIGHT} Linter suite complete. Results saved in $JOURNAL_AUDIT"
fi

exit 0

## end of LINTER.SH