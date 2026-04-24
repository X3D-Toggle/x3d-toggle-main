#!/bin/sh
## Central Plexus of the X3D Toggle Project
## `framework.sh` - Scripting Librarian
## Project-wide framework for path discovery, UI templates, and logging context.

_l_curr="$(cd "$(dirname "$0")" && pwd)"
X3D_TOGGLE=""

if [ "${_l_curr#"/usr/lib/x3d-toggle"}" != "$_l_curr" ]; then
    X3D_TOGGLE="/usr/lib/x3d-toggle"
else
    while [ "$_l_curr" != "/" ] && [ -n "$_l_curr" ]; do
        if [ -f "$_l_curr/Makefile" ]; then
            X3D_TOGGLE="$(readlink -f "$_l_curr")"
            break
        fi
        _l_curr="$(dirname "$_l_curr")"
    done
fi

if [ -z "$X3D_TOGGLE" ]; then
    printf_step "2,${XOUT} Error: Could not resolve X3D_TOGGLE root (Makefile not found)."
    exit 1
fi

[ "$X3D_EXEC" = "1" ] && export X3D_EXEC
export X3D_TOGGLE
export DIR_SRC="$X3D_TOGGLE/src"

export X3D_BIN="$X3D_TOGGLE/bin"
export X3D_BUILD="$X3D_TOGGLE/build"

export PREFIX="${PREFIX:-/usr}"
export USR_BIN="${USR_BIN:-$PREFIX/bin}"
export USR_SHARE="${USR_SHARE:-$PREFIX/share}"
export USR_LIB="${USR_LIB:-$PREFIX/lib}"
export USR_LIBS="${USR_LIBS:-$USR_LIB/x3d-toggle}"
export USR_ASSETS="${USR_ASSETS:-$USR_SHARE/x3d-toggle}"
export USR_PIXMAPS="${USR_PIXMAPS:-$USR_SHARE/pixmaps}"
export USR_APPS="${USR_APPS:-$USR_SHARE/applications}"
export USR_MAN="${USR_MAN:-$USR_SHARE/man/man1}"

export VAR_LIB="${VAR_LIB:-/var/lib/x3d-toggle}"
export VAR_LOG="${VAR_LOG:-/var/log/x3d-toggle}"
export VAR_LOGS="${VAR_LOGS:-$VAR_LOG/logs}"
export VAR_AUDITS="${VAR_AUDITS:-$VAR_LOG/audits}"
export VAR_DUMPS="${VAR_DUMPS:-$VAR_LOG/coredumps}"

export JOURNAL_LOGS="$VAR_LOGS"
export JOURNAL_AUDITS="$VAR_AUDITS"
export JOURNAL_DUMPS="$VAR_DUMPS"

export DIR_ETC="${DIR_ETC:-/etc/x3d-toggle.d}"
export SYS_UDEV="${SYS_UDEV:-/lib/udev/rules.d}"
export SYS_SYSTEMD="${SYS_SYSTEMD:-$PREFIX/lib/systemd/system}"
export SYS_POLKIT="${SYS_POLKIT:-$USR_SHARE/polkit-1/rules.d}"
export SYS_SYSUSERS="${SYS_SYSUSERS:-$PREFIX/lib/sysusers.d}"
export SYS_TMPFILES="${SYS_TMPFILES:-$PREFIX/lib/tmpfiles.d}"

if [ "$X3D_TOGGLE" = "$VAR_LIB" ] || [ "$X3D_TOGGLE" = "/usr/lib/x3d-toggle" ]; then
    export X3D_LOGS="$VAR_LOGS"
else
    export X3D_LOGS="$X3D_TOGGLE/dev/logging/logs"
fi

framework_init_dirs() {
    # 1. Build & Binary Infrastructure
    mkdir -p "$X3D_BIN" "$X3D_BUILD" 2>/dev/null

    # 2. Diagnostic Journals (Standard & Developer)
    mkdir -p "$VAR_LOGS" "$VAR_AUDITS" "$VAR_DUMPS" "$X3D_LOGS" 2>/dev/null

    # 3. Configuration Root (Only if Root, standard install handles this)
    if [ "$(id -u)" -eq 0 ]; then
        mkdir -p "$DIR_ETC" 2>/dev/null
    fi
}

# Execute immediate directory lifecycle verification
framework_init_dirs


_XUI_LOCATIONS="
    $X3D_TOGGLE/scripts/framework/xui.sh
    /usr/lib/x3d-toggle/scripts/framework/xui.sh
    ./scripts/framework/xui.sh
"
XUI_FOUND=0
XUI_PATH=""
for _l_loc in $_XUI_LOCATIONS; do
    if [ -f "$_l_loc" ]; then
        XUI_PATH="$_l_loc"
        XUI_FOUND=1
        break
    fi
done

if [ "$XUI_FOUND" -eq 0 ]; then
    printf_step "2,${XOUT} Error: xui.sh not found. Rerun installer."
    exit 1
fi

. "$XUI_PATH"

# Self-healing provisioning (Skipped during formal generation cycles to ensure accurate UI reporting)
if [ "$X3D_FRAMEWORK" != "1" ] && { [ ! -f "$X3D_BUILD/xui.c" ] || [ ! -f "$X3D_BUILD/xui.h" ]; }; then
    sh "$XUI_PATH" --gen-xui
    if [ ! -f "$X3D_BUILD/xui.c" ] || [ ! -f "$X3D_BUILD/xui.h" ]; then
        printf_step "2,${XOUT} Error: XUI Components Missing" \
                    "2,Please verify scripts/framework/xui.sh and" \
                    "2,rerun installer."
        exit 1
    fi
fi

if [ -z "$X3D_XUI_LOADED" ] && [ -z "$NOTICE" ]; then
    printf_step "2,${WARN} Warning: xui.sh sourced but notice/color variables not exported correctly."
fi



hw_check() {
    _l_CPU_MODEL=""
    while IFS=: read -r _l_key _l_value; do
        case "$_l_key" in
            *"model name"*)
                _l_CPU_MODEL="$_l_value"
                break ;;
        esac
    done < /proc/cpuinfo

    case "$_l_CPU_MODEL" in
        *7900X3D*|*7950X3D*|*9900X3D*|*9950X3D*) : ;; # Valid
        *)
            printf_center "${XOUT} Hardware Incompatibility Detected ${XOUT}" \
                          "2,${XOUT} Error: Heterogeneous dual-CCD AMD X3D processor not detected." \
                          "2,This utility is specifically designed for the 7950X3D/7900X3D." \
                          "2,Exiting for hardware safety."
            exit 1
            ;;
    esac
    
    if [ ! -d "/sys/bus/platform/drivers/amd_x3d_vcache" ]; then
        printf_step "2,${WARN} Notice: amd_x3d_vcache driver not loaded."
    fi
}

resolve_node() {
    for _l_node in /sys/bus/platform/drivers/amd_x3d_vcache/*/amd_x3d_mode /sys/devices/platform/AMDI*/amd_x3d_mode; do
        if [ -e "$_l_node" ]; then
            export NODE_PATH="$_l_node"
            return 0
        fi
    done
    return 1
}

journal_trace() {
    if [ "$DEBUG_ENABLE" != "1" ]; then return; fi
    
    
    if [ -z "$TRACE_FILE" ]; then
        _l_ts=$(date +%Y%m%d-%H%M%S)
        export TRACE_FILE="$X3D_LOGS/trace-$_l_ts.log"
        echo "--- X3D TRACE SESSION STARTED: $(date) ---" > "$TRACE_FILE"
    fi
    
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" >> "$TRACE_FILE"
}

confirm() {
    _l_prompt="${QUERY} $1 [y/N] "
    if [ ! -t 0 ]; then return 1; fi
    printf_step_no_nl "$_l_prompt"
    read -r _l_reply
    case "$_l_reply" in
        [Yy]*) return 0 ;;
        *) return 1 ;;
    esac
}

check_deps() {
    _l_missing=""
    for _l_req in $1; do
        if ! command -v "$_l_req" >/dev/null 2>&1; then
            _l_missing="${_l_missing} ${_l_req}"
        fi
    done
    
    if [ -n "$_l_missing" ]; then
        printf_step "2,${XOUT} Error: Missing dependencies:${_l_missing}"
        exit 1
    fi
}

config_generate() {
    _l_settings_src="$1"
    _l_games_src="$2"
    
    if [ ! -f "$_l_settings_src" ]; then
        printf_step "2,${XOUT} Error: Settings source $_l_settings_src not found."
        return 1
    fi
    
    printf_step "[SETTINGS]"
    sh "$_l_settings_src" --gen-xui
    
    printf_br
    printf_step "[GAMES_SYS]"
    if [ -f "$_l_games_src" ]; then
        while IFS= read -r _l_line || [ -n "$_l_line" ]; do
            case "$_l_line" in
                "#"*|"") continue ;;
                *) printf_string "$_l_line" ;;
            esac
        done < "$_l_games_src"
    fi
    
    printf_br
    printf_step "[GAMES_USR]"
    
    sh "$X3D_TOGGLE/scripts/framework/ccd.sh" --gen-config
}

journal_get() {
    _l_code="$1"
    _l_reg="registry_msg"
    _l_abs_idx=$_l_code
    _l_registry_found=0
    _l_capture_state=0 # 0: seeking index, 1: capturing strings

    _l_summary=""
    _l_details=""

    if [ "$_l_code" -gt 0 ] && [ "$_l_code" -le 16 ]; then
        _l_reg="registry_status"
    else
        if [ "$_l_code" -lt 0 ]; then
            _l_abs_idx=$(( -_l_code ))
        fi
    fi

    if [ ! -f "$X3D_TOGGLE/src/error.c" ]; then return 1; fi

    while IFS= read -r _l_line || [ -n "$_l_line" ]; do
        case "$_l_line" in
            *"static const msg_t $_l_reg"*)
                _l_registry_found=1
                continue ;;
        esac
        
        if [ "$_l_registry_found" -eq 1 ]; then
            case "$_l_line" in
                "};"*) break ;;
            esac
            
            if [ "$_l_capture_state" -eq 0 ]; then
                case "$_l_line" in
                    *"[$_l_abs_idx] = {"*) _l_capture_state=1 ;;
                esac
            fi

            if [ "$_l_capture_state" = "1" ]; then
                _l_line_part="$_l_line"
                while :; do
                    case "$_l_line_part" in
                        *\"*)
                            _l_line_part="${_l_line_part#*\"}"
                            _l_str="${_l_line_part%%\"*}"
                            _l_line_part="${_l_line_part#*\"}"
                            
                            if [ -z "$_l_summary" ]; then
                                _l_summary="$_l_str"
                            else
                                _l_details="${_l_details}${_l_str}"
                            fi ;;
                        *) break ;;
                    esac
                done

                case "$_l_line" in
                    *"},"*)
                        if [ -n "$_l_summary" ]; then
                            echo "$_l_summary|$_l_details"
                            return 0
                        fi ;;
                esac
            fi
        fi
    done < "$X3D_TOGGLE/src/error.c"
    
    return 1
}

journal_write() {
    _j_flag="$1"
    
    if [ "$_j_flag" = "-audit" ]; then
        shift
        _l_tool="$1"
        _l_log="$2"
        _l_status="$3"
        _l_semantic="ERR_LINTER_FAIL"

        [ "$_l_tool" = "valgrind" ] && _l_semantic="ERR_MEM_LEAK"

        printf_br
        printf_step "2,${COLOR_RED}ERROR: ${COLOR_RESET}$_l_tool failed [Code: $_l_status | $_l_semantic]"
        
        if [ -f "$_l_log" ]; then
            printf_step "2,${NOTICE} Diagnostic context from $(basename "$_l_log"):"
            printf_br
            _l_count=0
            while IFS= read -r _l_line || [ -n "$_l_line" ]; do
                _l_count=$((_l_count + 1))
            done < "$_l_log"

            _l_start=$((_l_count - 12))
            [ "$_l_start" -lt 0 ] && _l_start=0
            _l_current=0
            while IFS= read -r _l_line || [ -n "$_l_line" ]; do
                if [ "$_l_current" -ge "$_l_start" ]; then
                    printf_string "2,      | $_l_line"
                fi
                _l_current=$((_l_current + 1))
            done < "$_l_log"
            printf_br
        else
            printf_step "2,${XOUT} Error: Log file $(basename "$_l_log") not found."
        fi
        
        printf_step "${WARN} Continuing linter suite execution..."
        printf_br
    elif [ "$_j_flag" = "-log" ]; then
        shift
        _l_file="$1"
        _l_tag="${2:-TRACE}"
        _l_msg="$3"
        printf "[%s] [%s] %s\n" "$(date +%s)" "$_l_tag" "$_l_msg" >> "$_l_file"
    elif [ "$_j_flag" = "-dump" ]; then
        shift
        sh /usr/lib/x3d-toggle/scripts/tools/coredump.sh "$@"
    else
        [ "$_j_flag" = "-err" ] && shift
        _l_code="${1:-$_j_flag}"
        shift
        
        _l_raw_msg=$(journal_get "$_l_code")
        
        if [ $? -ne 0 ]; then
            printf_center "2,${XOUT} Critical Error: [X3D-$_l_code] ${XOUT}" \
                          "2,Diagnostic string not found in registry."
            exit 1
        fi

        _l_summary="${_l_raw_msg%|*}"
        _l_details="${_l_raw_msg#*|}"
        
        case "$_l_details" in
            *"%s"*|*"%d"*|*"%zu"*)
                _l_details=$(printf "$_l_details" "$@") ;;
        esac
        
        if [ "$_l_code" -lt 0 ]; then
            printf_br
            printf_center "${XOUT} $_l_summary [X3D-$(( -_l_code ))] ${XOUT}" \
                          "${_l_details}"
            printf_br
            exit 1
        else
            printf_step "2,${ALRIGHT} $_l_summary"
        fi
    fi
}

conf_framework() {
    _l_name="$1"
    _l_script="$2"
    _l_target="$3"
    _l_deps="$4"
    shift; shift; shift; shift

    _l_upd=0
    if [ ! -f "$_l_target" ]; then
        _l_upd=1
    elif [ -n "$UPDATE" ]; then
        _l_upd=1
    elif [ "$_l_script" -nt "$_l_target" ]; then
        _l_upd=1
    else
        for _l_d in $_l_deps; do
            if [ -f "$_l_d" ] && [ "$_l_d" -nt "$_l_target" ]; then
                _l_upd=1; break
            fi
        done
    fi

    if [ "$_l_upd" -eq 1 ]; then
        if [ -f "$_l_target" ]; then
            printf_step "2,${GEAR} Updating $_l_name..."
        else
            printf_step "2,${HAMMER} Generating $_l_name..."
        fi
        X3D_EXEC=1 sh "$_l_script" "$@" "$UPDATE" "$VERIFY"
    fi
}

if [ "$X3D_FRAMEWORK" = "1" ] && [ "$(basename -- "$0")" = "framework.sh" ]; then
    for arg in "$@"; do
        case "$arg" in
            --update|--sync|-update|-sync) UPDATE="--update" ;;
            --verify|-verify) VERIFY="--verify" ;;
        esac
    done
    
    case "$1" in
        --sync)
            X3D_EXEC=1 X3D_FRAMEWORK=1 sh "$X3D_TOGGLE/scripts/framework/framework.sh" --gen-all
            
            printf_br
            printf_divider
            if [ -z "$DESTDIR" ]; then
                printf_step "Local install detected. Run 'sudo make setup' to configure."
            else
                printf_step "Packager install detected. User must configure via setup script post-install."
            fi
            printf_divider
            printf_br

            if [ -f "$X3D_TOGGLE/Makefile" ]; then
                if confirm "Development Root Detected: Rebuild X3D Toggle binary? [y/N]"; then
                    make build
                    journal_write 17
                fi
            fi
            exit 0
            ;;
        --gen-*)
            if [ "$X3D_EXEC" = "1" ]; then
                while [ "$#" -gt 0 ]; do
                    case "$1" in
                        --gen-xui)
                            conf_framework "XUI Template" "$X3D_TOGGLE/scripts/framework/xui.sh" "$X3D_BUILD/xui.c" "" --gen-xui; shift ;;
                        --gen-ccd)
                            conf_framework "CCD Bridge" "$X3D_TOGGLE/scripts/framework/ccd.sh" "$X3D_BUILD/ccd.c" "/proc/cpuinfo" --gen-ccd; shift ;;
                        --gen-ebpf|--gen-scheduler)
                            conf_framework "eBPF Object Constructor" "$X3D_TOGGLE/scripts/framework/ebpftool.sh" "$X3D_BUILD/bpf.o" "$X3D_TOGGLE/src/daemon/bpf/bpf.c" --gen-ebpf; shift ;;
                        --gen-config)
                            conf_framework "Core Configurator" "$X3D_TOGGLE/scripts/framework/config.sh" "$X3D_BUILD/config.h" "$X3D_TOGGLE/config/settings.conf $X3D_TOGGLE/config/games.conf"; shift ;;
                        --gen-config-file)
                            shift; config_generate "$1" "$2"; exit 0 ;;
                        --gen-all)
                            X3D_EXEC=1 X3D_FRAMEWORK=1 sh "$X3D_TOGGLE/scripts/framework/framework.sh" $UPDATE $VERIFY --gen-xui --gen-ccd --gen-ebpf --gen-config || exit 1; shift ;;
                        *)
                            journal_write -6 "$1"; exit 1 ;; # Syntax error
                    esac
                done
                exit 0
            fi ;;
        *)
            journal_write -6 "$1"; exit 1 ;; # Prevent unrecognized arguments
    esac
fi

## end of FRAMEWORK.SH