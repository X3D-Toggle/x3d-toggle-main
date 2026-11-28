#!/bin/sh
##
## `rotate.sh`
##
## Usage: x3d-toggle journal-[type] -rotate
## [Type] = log,audit,coredump,all
## [Args] = days
## Simple log rotation helper for the project `dev/logging/` subdirectories.
## - Rotates any `*.log` file when it exceeds MAX_BYTES (default 10 MB).
## - Compresses the rotated file with a timestamp suffix and truncates the live log.
## - Keeps the most recent N archives per log file (default 7).

umask 0002

_l_dir_lib="$(cd "$(dirname "$0")/../framework" && pwd)"
. "$_l_dir_lib/framework.sh"

DIR_LOGS="${VAR_LOGS:-/var/log/x3d-toggle/logs}"
DIR_AUDITS="${VAR_AUDITS:-/var/log/x3d-toggle/audits}"

# Respect configuration values, with defaults
KEEP=${JOURNAL_KEEP:-7}
MAX_MB=${JOURNAL_MAX_MB:-10}
MAX_BYTES=$((MAX_MB * 1024 * 1024))

prune_archives() {
    _dir="$1"
    _base="$2"
    set -- "$_dir/${_base}."*.gz
    if [ -e "$1" ]; then
        _count=$#
        while [ "$_count" -gt "$KEEP" ]; do
            rm -f -- "$1" || true
            shift
            _count=$((_count - 1))
        done
    fi
}

rotate_logs() {
    for logfile in "$DIR_LOGS"/*.log; do
        [ -f "$logfile" ] || continue
        filesize=$(stat -c%s -- "$logfile" 2>/dev/null || wc -c <"$logfile")
        if [ "$filesize" -ge "$MAX_BYTES" ]; then
            ts=$(date +%Y%m%d%H%M%S)
            archive="$logfile.$ts.gz"
            gzip -c -- "$logfile" >"$archive"
            : >"$logfile"

            base=$(basename -- "$logfile")
            dir=$(dirname -- "$logfile")
            prune_archives "$dir" "$base"
        fi
    done
}

rotate_audits() {
    for auditfile in "$DIR_AUDITS"/*.log "$DIR_AUDITS"/*.txt; do
        [ -f "$auditfile" ] || continue
        filesize=$(stat -c%s -- "$auditfile" 2>/dev/null || wc -c <"$auditfile")
        if [ "$filesize" -ge "$MAX_BYTES" ]; then
            ts=$(date +%Y%m%d%H%M%S)
            archive="$auditfile.$ts.gz"
            gzip -c -- "$auditfile" >"$archive"
            : >"$auditfile"

            base=$(basename -- "$auditfile")
            dir=$(dirname -- "$auditfile")
            prune_archives "$dir" "$base"
        fi
    done
}

case "${1:-}" in
    --log)
        rotate_logs ;;
    --audit)
        rotate_audits ;;
    --all|*)
        rotate_logs
        rotate_audits ;;
esac

exit 0


## end of ROTATE.SH