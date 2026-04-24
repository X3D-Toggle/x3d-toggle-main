#!/bin/sh
## Configuration Interface Generator for the X3D Toggle Project
## `config.sh`
## Generates `daemon.conf` in /etc/x3d-toggle.d/ for use/editing
## to build the x3d-toggle binary and modify runtime behavior

_l_dir_lib="$(cd "$(dirname "$0")" && pwd)"
. "$_l_dir_lib/framework.sh"

if [ "$X3D_EXEC" != "1" ]; then
    journal_write -37
fi
. "$X3D_TOGGLE/config/settings.conf"

guard() {
    _var_name="$1"
    
    eval "_current_val=\"\$${_var_name}\""

    if [ -z "$_current_val" ]; then
        if [ "$_var_name" = "DIR_BIN" ]; then
            _default_val="/etc/x3d-toggle.d"
        else
            _default_val=""
            while IFS='=' read -r key val; do
                if [ "$key" = "$_var_name" ]; then
                    _default_val="$val"
                    break
                fi
            done < "$X3D_TOGGLE/config/settings.conf"
        fi
        
        printf_step "3,${WARN} config.sh: '$_var_name' null/unset, falling back to '$_default_val'"
        
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
guard "FALLBACK_PROFILE"
guard "DAEMON_STATE"
guard "SERVER_ADDRESS"

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
                    "FALLBACK_PROFILE=${FALLBACK_PROFILE}" \
                    "DAEMON_STATE=${DAEMON_STATE}" \
                    "SERVER_ADDRESS=${SERVER_ADDRESS}"
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

#include \"xui.h\"

#define CONFIG_PATH      \"${_DAEMON_CONF}\"
#define GAMES_PATH       \"${_DAEMON_CONF}\"
#define DAEMON_CONF_PATH \"${_DAEMON_CONF}\"
#define CONFIG_POLLING_INTERVAL  ${POLLING_INTERVAL}
#define CONFIG_REFRESH_INTERVAL  ${REFRESH_INTERVAL}
#define CONFIG_DEV_ENABLE        ${DEV_ENABLE}
#define CONFIG_AFFINITY_LEVEL    ${AFFINITY_LEVEL}
#define CONFIG_AFFINITY_MASK     \"${AFFINITY_MASK}\"
#define CONFIG_LOAD_THRESHOLD    ${LOAD_THRESHOLD}
#define CONFIG_DETECTION_LEVEL   ${DETECTION_LEVEL}
#define CONFIG_EBPF_ENABLE       ${EBPF_ENABLE}
#define CONFIG_DEBUG_ENABLE      ${DEBUG_ENABLE}
#define CONFIG_FALLBACK_PROFILE  \"${FALLBACK_PROFILE}\"
#define CONFIG_SERVER_ADDRESS    \"${SERVER_ADDRESS}\"

typedef struct {
int    polling_interval;
double refresh_interval;
int    dev_enable;
int    affinity_level;
char   affinity_mask[64];
double load_threshold;
int    detection_level;
int    ebpf_enable;
int    debug_enable;
char   daemon_state[32];
char   fallback_profile[64];
char   server_address[128];
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
                "SERVER_ADDRESS=${SERVER_ADDRESS}"
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

## end of config.sh