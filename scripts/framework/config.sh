#!/bin/sh
## Configuration Interface Generator for the X3D Toggle Project
## `config.sh`
## Generates `daemon.conf` in /etc/x3d-toggle.d/ for use/editing
## to build the x3d-toggle binary and modify runtime behavior
## Generates `config.h` and `config.c` for building the x3d-toggle binary

_l_dir_lib="$(cd "$(dirname "$0")" && pwd)"
. "$_l_dir_lib/framework.sh"

if [ "$X3D_EXEC" != "1" ]; then
    journal_write -37
fi
. "$X3D_TOGGLE/config/settings.conf"
if [ -f "$X3D_TOGGLE/config/irq.conf" ]; then
    . "$X3D_TOGGLE/config/irq.conf"
fi

# ==============================================================================
# PURE POSIX PARSER FOR SYSCTL-FORMATTED FILES (NO EXTERNAL BINARIES)
# ==============================================================================
get_conf_value() {
    _target_key="$1"
    _file="$2"
    _result=""

    if [ ! -f "$_file" ]; then
        printf ""
        return
    fi

    # Read line-by-line using '=' as the delimiter
    while IFS='=' read -r _key _raw_val || [ -n "$_key" ]; do
        # Skip comments and empty lines natively
        case "$_key" in
            \#*|"") continue ;;
        esac
        
        # Disable globbing to safely use positional parameters for stripping spaces
        set -f
        
        # Strip leading/trailing whitespace from the Key
        set -- $_key
        _clean_key="$1"
        
        # Strip leading/trailing whitespace from the Value
        set -- $_raw_val
        _clean_val="$1"
        
        set +f
        
        # Strip double quotes (e.g. "madvise" -> madvise) natively
        _clean_val="${_clean_val%\"}"
        _clean_val="${_clean_val#\"}"
        
        # Match standard sysctl keys OR keys prefixed with '-' (ignore-error flag)
        if [ "$_clean_key" = "$_target_key" ] || [ "$_clean_key" = "-$_target_key" ]; then
            _result="$_clean_val"
            break
        fi
    done < "$_file"
    
    printf "%s" "$_result"
}

# 1. Safely Parse Advanced Configuration
ADV_CONF="$X3D_TOGGLE/config/advanced.conf"
SCHED_SLICE_US=$(get_conf_value "kernel.sched_cfs_bandwidth_slice_us" "$ADV_CONF")
SCHED_BORE=$(get_conf_value "kernel.sched_bore" "$ADV_CONF")
SCHED_BIT_SHIFT=$(get_conf_value "kernel.sched_bit_shift" "$ADV_CONF")
SCHED_BURST_FORK=$(get_conf_value "kernel.sched_burst_fork_at_init" "$ADV_CONF")
VM_MAX_MAP=$(get_conf_value "vm.max_map_count" "$ADV_CONF")
SPLIT_LOCK_DETECT=$(get_conf_value "split_lock_detect" "$ADV_CONF")
NMI_WATCHDOG=$(get_conf_value "nmi_watchdog" "$ADV_CONF")
THP_MODE=$(get_conf_value "target_thp_mode" "$ADV_CONF")

# 2. Safely Parse Networking Configuration
NET_CONF="$X3D_TOGGLE/config/networking.conf"
NET_QDISC=$(get_conf_value "net.core.default_qdisc" "$NET_CONF")
NET_FASTOPEN=$(get_conf_value "net.ipv4.tcp_fastopen" "$NET_CONF")
NET_RP_FILTER=$(get_conf_value "net.ipv4.conf.default.rp_filter" "$NET_CONF")
NET_SOURCE_ROUTE=$(get_conf_value "net.ipv4.conf.default.accept_source_route" "$NET_CONF")

# 2.5 Safely Parse Server Configuration
SERVER_ENABLED=$(get_conf_value "ENABLED" "$NET_CONF")
SERVER_ADDRESS=$(get_conf_value "SERVER_ADDRESS" "$NET_CONF")
SERVER_PORT=$(get_conf_value "SERVER_PORT" "$NET_CONF")
SERVER_SSH=$(get_conf_value "SERVER_SSH" "$NET_CONF")

# 3. Apply Internal Fallbacks (Prevents "null/unset" warnings if file is missing)
SCHED_SLICE_US=${SCHED_SLICE_US:-3000}
SCHED_BORE=${SCHED_BORE:-1}
SCHED_BIT_SHIFT=${SCHED_BIT_SHIFT:-12}
SCHED_BURST_FORK=${SCHED_BURST_FORK:-1}
VM_MAX_MAP=${VM_MAX_MAP:-1048576}
SPLIT_LOCK_DETECT=${SPLIT_LOCK_DETECT:-0}
NMI_WATCHDOG=${NMI_WATCHDOG:-0}
THP_MODE=${THP_MODE:-madvise}
NET_QDISC=${NET_QDISC:-fq_codel}
NET_FASTOPEN=${NET_FASTOPEN:-3}
NET_RP_FILTER=${NET_RP_FILTER:-2}
NET_SOURCE_ROUTE=${NET_SOURCE_ROUTE:-0}

SERVER_ENABLED=${SERVER_ENABLED:-false}
SERVER_ADDRESS=${SERVER_ADDRESS:-127.0.0.1}
SERVER_PORT=${SERVER_PORT:-9370}
SERVER_SSH=${SERVER_SSH:-local}

if [ "$SERVER_ENABLED" = "true" ]; then
    SERVER_ENABLED_INT=1
else
    SERVER_ENABLED_INT=0
fi

LINT_CLANG_DIAGNOSTIC=${LINT_CLANG_DIAGNOSTIC:-enabled}
LINT_CLANG_BUGPRONE=${LINT_CLANG_BUGPRONE:-enabled}
LINT_CLANG_MODERNIZE=${LINT_CLANG_MODERNIZE:-enabled}
LINT_CLANG_READABILITY=${LINT_CLANG_READABILITY:-enabled}
LINT_CLANG_PERFORMANCE=${LINT_CLANG_PERFORMANCE:-enabled}
LINT_CLANG_PORTABILITY=${LINT_CLANG_PORTABILITY:-enabled}
LINT_CLANG_ANALYZER=${LINT_CLANG_ANALYZER:-enabled}
LINT_CPPCHECK_ALL=${LINT_CPPCHECK_ALL:-enabled}
LINT_CPPCHECK_WARNING=${LINT_CPPCHECK_WARNING:-enabled}
LINT_CPPCHECK_STYLE=${LINT_CPPCHECK_STYLE:-enabled}
LINT_CPPCHECK_PERFORMANCE=${LINT_CPPCHECK_PERFORMANCE:-enabled}
LINT_CPPCHECK_PORTABILITY=${LINT_CPPCHECK_PORTABILITY:-enabled}
LINT_CPPCHECK_INFORMATION=${LINT_CPPCHECK_INFORMATION:-disabled}
LINT_CPPCHECK_UNUSED=${LINT_CPPCHECK_UNUSED:-enabled}
LINT_VALGRIND_MODE=${LINT_VALGRIND_MODE:-full}
LINT_VALGRIND_KINDS=${LINT_VALGRIND_KINDS:-all}
LINT_VALGRIND_ORIGINS=${LINT_VALGRIND_ORIGINS:-enabled}

guard() {
    _var_name="$1"
    
    eval "_current_val=\"\$${_var_name}\""

    if [ -z "$_current_val" ]; then
        if [ "$_var_name" = "DIR_BIN" ]; then
            _default_val="/etc/x3d-toggle.d"
        else
            _default_val=""
            # Note: This inner loop checks settings.conf if a variable was totally missed
            while IFS='=' read -r key val; do
                if [ "$key" = "$_var_name" ]; then
                    _default_val="$val"
                    break
                fi
            done < "$X3D_TOGGLE/config/settings.conf"
        fi
        
        journal_write -38 "$_var_name" "$_default_val"
        eval "${_var_name}=\"${_default_val}\""
    fi
}

guard "DIR_BIN"
guard "POLLING_INTERVAL"
guard "REFRESH_INTERVAL"
guard "LOAD_THRESHOLD"
guard "DETECTION_LEVEL"
guard "EBPF_ENABLE"
guard "DEBUG_ENABLE"
guard "DEV_ENABLE"
guard "AFFINITY_LEVEL"
guard "AFFINITY_MASK"
guard "AFFINITY_FREQ_MASK"
guard "FALLBACK_PROFILE"
guard "DAEMON_STATE"
guard "SERVER_ENABLED"
guard "SERVER_ADDRESS"
guard "SERVER_PORT"
guard "SERVER_SSH"
guard "JOURNAL_KEEP"
guard "JOURNAL_MAX_MB"
guard "IRQ_PINNING"
guard "IRQ_CUSTOM"
guard "IRQ_ENABLE"
guard "IRQ_GPU"
guard "IRQ_NVME"
guard "IRQ_USB"
guard "IRQ_NIC"
guard "IRQ_AUDIO"
guard "IRQ_COALESCE"
guard "IRQ_WATCH"

# Advanced Tunables
guard "SCHED_BORE"
guard "SCHED_BIT_SHIFT"
guard "SCHED_BURST_FORK"
guard "SCHED_SLICE_US"
guard "VM_MAX_MAP"
guard "SPLIT_LOCK_DETECT"
guard "NMI_WATCHDOG"
guard "THP_MODE"

# Networking Tunables
guard "NET_QDISC"
guard "NET_FASTOPEN"
guard "NET_RP_FILTER"
guard "NET_SOURCE_ROUTE"
guard "LINT_CLANG_DIAGNOSTIC"
guard "LINT_CLANG_BUGPRONE"
guard "LINT_CLANG_MODERNIZE"
guard "LINT_CLANG_READABILITY"
guard "LINT_CLANG_PERFORMANCE"
guard "LINT_CLANG_PORTABILITY"
guard "LINT_CLANG_ANALYZER"
guard "LINT_CPPCHECK_ALL"
guard "LINT_CPPCHECK_WARNING"
guard "LINT_CPPCHECK_STYLE"
guard "LINT_CPPCHECK_PERFORMANCE"
guard "LINT_CPPCHECK_PORTABILITY"
guard "LINT_CPPCHECK_INFORMATION"
guard "LINT_CPPCHECK_UNUSED"
guard "LINT_VALGRIND_MODE"
guard "LINT_VALGRIND_KINDS"
guard "LINT_VALGRIND_ORIGINS"

_DAEMON_CONF="${DIR_BIN}/daemon.conf"

if [ "$1" = "--update" ]; then
    printf_step "${GEAR} Regenerating system configuration: ${DIR_BIN}..."

    usr_content=""
    if [ -f "$_DAEMON_CONF" ]; then
        in_usr=0
        while IFS= read -r _l_line; do
            _l_line="${_l_line%$(printf '\r')}"
            
            if [ "$_l_line" = "[GAMES_USR]" ]; then
                in_usr=1
                continue
            fi
            if [ "$in_usr" = "1" ]; then
                usr_content="${usr_content}${_l_line}$(printf '\n')"
            fi
        done < "$_DAEMON_CONF"
    fi

    {
        printf_step "POLLING_INTERVAL=${POLLING_INTERVAL}" \
                    "REFRESH_INTERVAL=${REFRESH_INTERVAL}" \
                    "LOAD_THRESHOLD=${LOAD_THRESHOLD}" \
                    "DETECTION_LEVEL=${DETECTION_LEVEL}" \
                    "EBPF_ENABLE=${EBPF_ENABLE}" \
                    "DEBUG_ENABLE=${DEBUG_ENABLE}" \
                    "DEV_ENABLE=${DEV_ENABLE}" \
                    "AFFINITY_LEVEL=${AFFINITY_LEVEL}" \
                    "AFFINITY_MASK=${AFFINITY_MASK}" \
                    "AFFINITY_FREQ_MASK=${AFFINITY_FREQ_MASK}" \
                    "FALLBACK_PROFILE=${FALLBACK_PROFILE}" \
                    "DAEMON_STATE=${DAEMON_STATE}" \
                    "SERVER_ENABLED=${SERVER_ENABLED}" \
                    "SERVER_ADDRESS=${SERVER_ADDRESS}" \
                    "SERVER_PORT=${SERVER_PORT}" \
                    "SERVER_SSH=${SERVER_SSH}" \
                    "JOURNAL_KEEP=${JOURNAL_KEEP}" \
                    "JOURNAL_MAX_MB=${JOURNAL_MAX_MB}" \
                    "SCHED_BORE=${SCHED_BORE}" \
                    "SCHED_BIT_SHIFT=${SCHED_BIT_SHIFT}" \
                    "SCHED_BURST_FORK=${SCHED_BURST_FORK}" \
                    "SCHED_SLICE_US=${SCHED_SLICE_US}" \
                    "VM_MAX_MAP=${VM_MAX_MAP}" \
                    "SPLIT_LOCK_DETECT=${SPLIT_LOCK_DETECT}" \
                    "NMI_WATCHDOG=${NMI_WATCHDOG}" \
                    "THP_MODE=${THP_MODE}" \
                    "NET_QDISC=${NET_QDISC}" \
                    "NET_FASTOPEN=${NET_FASTOPEN}" \
                    "NET_RP_FILTER=${NET_RP_FILTER}" \
                    "NET_SOURCE_ROUTE=${NET_SOURCE_ROUTE}" \
                    "LINT_CLANG_DIAGNOSTIC=${LINT_CLANG_DIAGNOSTIC}" \
                    "LINT_CLANG_BUGPRONE=${LINT_CLANG_BUGPRONE}" \
                    "LINT_CLANG_MODERNIZE=${LINT_CLANG_MODERNIZE}" \
                    "LINT_CLANG_READABILITY=${LINT_CLANG_READABILITY}" \
                    "LINT_CLANG_PERFORMANCE=${LINT_CLANG_PERFORMANCE}" \
                    "LINT_CLANG_PORTABILITY=${LINT_CLANG_PORTABILITY}" \
                    "LINT_CLANG_ANALYZER=${LINT_CLANG_ANALYZER}" \
                    "LINT_CPPCHECK_ALL=${LINT_CPPCHECK_ALL}" \
                    "LINT_CPPCHECK_WARNING=${LINT_CPPCHECK_WARNING}" \
                    "LINT_CPPCHECK_STYLE=${LINT_CPPCHECK_STYLE}" \
                    "LINT_CPPCHECK_PERFORMANCE=${LINT_CPPCHECK_PERFORMANCE}" \
                    "LINT_CPPCHECK_PORTABILITY=${LINT_CPPCHECK_PORTABILITY}" \
                    "LINT_CPPCHECK_INFORMATION=${LINT_CPPCHECK_INFORMATION}" \
                    "LINT_CPPCHECK_UNUSED=${LINT_CPPCHECK_UNUSED}" \
                    "LINT_VALGRIND_MODE=${LINT_VALGRIND_MODE}" \
                    "LINT_VALGRIND_KINDS=${LINT_VALGRIND_KINDS}" \
                    "LINT_VALGRIND_ORIGINS=${LINT_VALGRIND_ORIGINS}"
        printf_br
        printf_step "[GAMES_SYS]"
        while IFS= read -r _l_game; do
            _l_game="${_l_game%$(printf '\r')}"
            case "$_l_game" in
                "#"*|"/*"*|"*/"*|"") continue ;;
            esac
            printf_step "$_l_game"
        done < "$X3D_TOGGLE/config/games.conf"
        printf_br
        printf_step "[GAMES_USR]"
        if [ -n "$usr_content" ]; then
            printf "%s" "$usr_content"
        fi
    } > "${_DAEMON_CONF}.tmp"
    
    mv -f "${_DAEMON_CONF}.tmp" "$_DAEMON_CONF"
    exit 0
fi

if [ "$1" = "--check" ]; then
    if [ ! -f "$_DAEMON_CONF" ] || \
       [ "$X3D_TOGGLE/config/settings.conf" -nt "$_DAEMON_CONF" ] || \
       [ "$X3D_TOGGLE/config/irq.conf"      -nt "$_DAEMON_CONF" ] || \
       [ "$X3D_TOGGLE/config/games.conf"    -nt "$_DAEMON_CONF" ]; then
        X3D_EXEC=1 sh "$0" --update
    fi
    exit 0
fi

printf_step "2,${GEAR} Writing synchronized Configuration ruleset: build/config.h..."

{
    printf "/* Configuration Interface Header for the X3D Toggle Project
*
* \`config.h\` - Header only
*
** AUTO-GENERATED FILE. DO NOT EDIT DIRECTLY.
** EDIT FILE: \`config.sh\`
*/

#ifndef CONFIG_H
#define CONFIG_H

#define CAP_BORE          (1ULL << 0)
#define CAP_THP           (1ULL << 1)
#define CAP_SPLIT_LOCK    (1ULL << 2)
#define CAP_NMI_WATCHDOG  (1ULL << 3)
#define CAP_FASTOPEN      (1ULL << 4)
#define CAP_QDISC         (1ULL << 5)

#ifndef GUI_BUILD
#include \"xui.h\"
#endif

#define CONFIG_PATH      \"${_DAEMON_CONF}\"
#define GAMES_PATH       \"${_DAEMON_CONF}\"
#define DAEMON_CONF_PATH \"${_DAEMON_CONF}\"
#define CONFIG_POLLING_INTERVAL  ${POLLING_INTERVAL}
#define CONFIG_REFRESH_INTERVAL  ${REFRESH_INTERVAL}
#define CONFIG_DEV_ENABLE        ${DEV_ENABLE}
#define CONFIG_AFFINITY_LEVEL    ${AFFINITY_LEVEL}
#define CONFIG_AFFINITY_MASK     \"${AFFINITY_MASK}\"
#define CONFIG_AFFINITY_FREQ_MASK \"${AFFINITY_FREQ_MASK}\"
#define CONFIG_LOAD_THRESHOLD    ${LOAD_THRESHOLD}
#define CONFIG_DETECTION_LEVEL   ${DETECTION_LEVEL}
#define CONFIG_EBPF_ENABLE       ${EBPF_ENABLE}
#define CONFIG_DEBUG_ENABLE      ${DEBUG_ENABLE}
#define CONFIG_FALLBACK_PROFILE  \"${FALLBACK_PROFILE}\"
#define CONFIG_SERVER_ENABLED    ${SERVER_ENABLED_INT}
#define CONFIG_SERVER_ADDRESS    \"${SERVER_ADDRESS}\"
#define CONFIG_SERVER_PORT       ${SERVER_PORT}
#define CONFIG_SERVER_SSH        \"${SERVER_SSH}\"

typedef struct {
int    polling_interval;
double refresh_interval;
int    dev_enable;
int    affinity_level;
char   affinity_mask[64];
char   affinity_freq_mask[64];
double load_threshold;
int    detection_level;
int    ebpf_enable;
int    debug_enable;
char   daemon_state[32];
char   fallback_profile[64];
int    server_enabled;
char   server_address[128];
int    server_port;
char   server_ssh[32];

/* Advanced Tunables */
int    sched_bore;
int    sched_bit_shift;
int    sched_burst_fork;
int    sched_slice_us;
int    vm_max_map;
int    split_lock_detect;
int    nmi_watchdog;
char   thp_mode[16];

/* Networking */
char   net_qdisc[32];
int    net_fastopen;
int    net_rp_filter;
int    net_source_route;

/* IRQ Subsystem */
int    irq_enable;
int    irq_gpu;
int    irq_nvme;
int    irq_usb;
int    irq_nic;
int    irq_audio;
int    irq_coalesce;
int    irq_watch;

/* Developer & Linting */
int    advanced_config_enable;
int    force_unsupported_display;
char   lint_clang_diagnostic[16];
char   lint_clang_bugprone[16];
char   lint_clang_modernize[16];
char   lint_clang_readability[16];
char   lint_clang_performance[16];
char   lint_clang_portability[16];
char   lint_clang_analyzer[16];
char   lint_cppcheck_all[16];
char   lint_cppcheck_warning[16];
char   lint_cppcheck_style[16];
char   lint_cppcheck_performance[16];
char   lint_cppcheck_portability[16];
char   lint_cppcheck_information[16];
char   lint_cppcheck_unused[16];
char   lint_valgrind_mode[16];
char   lint_valgrind_kinds[32];
char   lint_valgrind_origins[16];
int    journal_keep;
int    journal_max_mb;

unsigned long long capabilities;
} DaemonConfig;

void config_load(DaemonConfig *cfg);
void config_update(const char *key, const char *value);
int  config_generate(const char *settings_src, const char *games_src, const char *dest);
int cli_config_sync(int argc, char *argv[]);
int cli_config_update(int argc, char *argv[]);
int cli_config_interval(int argc, char *argv[]);
int cli_config_threshold(int argc, char *argv[]);
int cli_config_fallback(int argc, char *argv[]);
int cli_config_detection(int argc, char *argv[]);
int cli_config_polling(int argc, char *argv[]);
int cli_config_ebpf(int argc, char *argv[]);
int cli_config_server(int argc, char *argv[]);
int cli_config_add(int argc, char *argv[]);
int cli_config_remove(int argc, char *argv[]);
int cli_config_list(int argc, char *argv[]);
int cli_config_profile(int argc, char *argv[]);
int cli_config_generate(int argc, char *argv[]);
#endif /* CONFIG_H */
"
} > "$X3D_TOGGLE/build/config.h"

printf_step "2,${GEAR} Writing synchronized configuration payload: build/daemon.conf..."

{
    printf_step "POLLING_INTERVAL=${POLLING_INTERVAL}" \
                "REFRESH_INTERVAL=${REFRESH_INTERVAL}" \
                "LOAD_THRESHOLD=${LOAD_THRESHOLD}" \
                "DETECTION_LEVEL=${DETECTION_LEVEL}" \
                "EBPF_ENABLE=${EBPF_ENABLE}" \
                "DEBUG_ENABLE=${DEBUG_ENABLE}" \
                "DEV_ENABLE=${DEV_ENABLE}" \
                "AFFINITY_LEVEL=${AFFINITY_LEVEL}" \
                "AFFINITY_MASK=${AFFINITY_MASK}" \
                "FALLBACK_PROFILE=${FALLBACK_PROFILE}" \
                "DAEMON_STATE=${DAEMON_STATE}" \
                "SERVER_ENABLED=${SERVER_ENABLED}" \
                "SERVER_ADDRESS=${SERVER_ADDRESS}" \
                "SERVER_PORT=${SERVER_PORT}" \
                "SERVER_SSH=${SERVER_SSH}" \
                "JOURNAL_KEEP=${JOURNAL_KEEP}" \
                "JOURNAL_MAX_MB=${JOURNAL_MAX_MB}" \
                "IRQ_PINNING=${IRQ_PINNING}" \
                "IRQ_CUSTOM=${IRQ_CUSTOM}" \
                "IRQ_ENABLE=${IRQ_ENABLE}" \
                "IRQ_GPU=${IRQ_GPU}" \
                "IRQ_NVME=${IRQ_NVME}" \
                "IRQ_USB=${IRQ_USB}" \
                "IRQ_NIC=${IRQ_NIC}" \
                "IRQ_AUDIO=${IRQ_AUDIO}" \
                "IRQ_COALESCE=${IRQ_COALESCE}" \
                "IRQ_WATCH=${IRQ_WATCH}" \
                "SCHED_BORE=${SCHED_BORE}" \
                "SCHED_BIT_SHIFT=${SCHED_BIT_SHIFT}" \
                "SCHED_BURST_FORK=${SCHED_BURST_FORK}" \
                "SCHED_SLICE_US=${SCHED_SLICE_US}" \
                "VM_MAX_MAP=${VM_MAX_MAP}" \
                "SPLIT_LOCK_DETECT=${SPLIT_LOCK_DETECT}" \
                "NMI_WATCHDOG=${NMI_WATCHDOG}" \
                "THP_MODE=${THP_MODE}" \
                "NET_QDISC=${NET_QDISC}" \
                "NET_FASTOPEN=${NET_FASTOPEN}" \
                "NET_RP_FILTER=${NET_RP_FILTER}" \
                "NET_SOURCE_ROUTE=${NET_SOURCE_ROUTE}" \
                "LINT_CLANG_DIAGNOSTIC=${LINT_CLANG_DIAGNOSTIC}" \
                "LINT_CLANG_BUGPRONE=${LINT_CLANG_BUGPRONE}" \
                "LINT_CLANG_MODERNIZE=${LINT_CLANG_MODERNIZE}" \
                "LINT_CLANG_READABILITY=${LINT_CLANG_READABILITY}" \
                "LINT_CLANG_PERFORMANCE=${LINT_CLANG_PERFORMANCE}" \
                "LINT_CLANG_PORTABILITY=${LINT_CLANG_PORTABILITY}" \
                "LINT_CLANG_ANALYZER=${LINT_CLANG_ANALYZER}" \
                "LINT_CPPCHECK_ALL=${LINT_CPPCHECK_ALL}" \
                "LINT_CPPCHECK_WARNING=${LINT_CPPCHECK_WARNING}" \
                "LINT_CPPCHECK_STYLE=${LINT_CPPCHECK_STYLE}" \
                "LINT_CPPCHECK_PERFORMANCE=${LINT_CPPCHECK_PERFORMANCE}" \
                "LINT_CPPCHECK_PORTABILITY=${LINT_CPPCHECK_PORTABILITY}" \
                "LINT_CPPCHECK_INFORMATION=${LINT_CPPCHECK_INFORMATION}" \
                "LINT_CPPCHECK_UNUSED=${LINT_CPPCHECK_UNUSED}" \
                "LINT_VALGRIND_MODE=${LINT_VALGRIND_MODE}" \
                "LINT_VALGRIND_KINDS=${LINT_VALGRIND_KINDS}" \
                "LINT_VALGRIND_ORIGINS=${LINT_VALGRIND_ORIGINS}"
    printf_br
    printf_step "[GAMES_SYS]"
    while IFS= read -r _l_game; do
        _l_game="${_l_game%$(printf '\r')}"
        case "$_l_game" in
            "#"*|"/*"*|"*/"*|"") continue ;;
        esac
        printf_step "$_l_game"
    done < "$X3D_TOGGLE/config/games.conf"
    printf_br
    printf_step "[GAMES_USR]"
} > "$X3D_TOGGLE/build/daemon.conf"

## end of CONFIG.SH