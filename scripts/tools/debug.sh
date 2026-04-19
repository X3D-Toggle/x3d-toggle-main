#!/bin/sh
## Debugger Script for the X3D Toggle Project
##
## `debug.sh`
##
## Usage: x3d-toggle debug
## [Args] = none
##
## MVC Observer: Decoupled real-time monitor for X3D Toggle.
## Targets the daemon's logic state and the kernel's eBPF events.
## Maintains journal in `/var/log/x3d-toggle/audits/`

if [ "$X3D_EXEC" != "1" ]; then exit 1; fi

ROOT_REPO="$(cd "$(dirname "$0")/../.." && pwd)"
. "$ROOT_REPO/scripts/framework/framework.sh"

DIR_AUDITS="$VAR_AUDITS"

DAEMON_PID=$(pidof x3d-daemon)
if [ -n "$DAEMON_PID" ]; then
    SESSION_TS=$(stat -c %Y /proc/"$DAEMON_PID")
    SESSION_DATE=$(date -d "@$SESSION_TS" +"%Y-%m-%d_%H-%M-%S")
else
    SESSION_DATE=$(date +"%Y-%m-%d_%H-%M-%S")
fi

export DEBUG_MODE=1
export X3D_TRACE_FILE="$DIR_AUDITS/debug_$SESSION_DATE.audit"
journal_trace "X3D Debug Observer session starting..."

printf_divider
printf_center "${TOOLS}  X3D Toggle v1.5.0 - Real-Time MVC Observer  ${TOOLS}"
printf_center "Logging to: $X3D_TRACE_FILE"
printf_divider

if [ "$(id -u)" -ne 0 ]; then
    printf_step "${XOUT} Error: Root privileges required for kernel trace access."
    exit 1
fi

_l_cleanup() {
    [ -n "$LOG_PID" ] && kill "$LOG_PID"
    printf_br
    printf_step "${ALRIGHT} Observer stopped. Log preserved at: $X3D_TRACE_FILE"
    printf_divider
    return 0
}
trap _l_cleanup SIGINT SIGTERM

printf_step "${SEARCH} Following Daemon Logs... (MVC Controller State)"
journalctl -u x3d-toggle.service -f -n 20 &
LOG_PID=$!

if [ -f /sys/kernel/debug/tracing/trace_pipe ]; then
    printf_step "${GEAR} Streaming eBPF Tracepipe... (Kernel Hardware Events)"
    printf_br
    while read -r _l_line || [ -n "$_l_line" ]; do
        case "$_l_line" in
            *"X3D"*) printf_string "$_l_line" ;;
        esac
    done < /sys/kernel/debug/tracing/trace_pipe
else
    printf_step "${WARN} Kernel trace_pipe not found. eBPF events will be skipped."
    wait $LOG_PID
fi

## end of DEBUG.SH