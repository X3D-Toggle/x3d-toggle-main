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

_l_dir_lib="$(cd "$(dirname "$0")/../framework" && pwd)"
. "$_l_dir_lib/framework.sh"

DIR_LOGS="$VAR_LOGS"
DIR_AUDITS="$VAR_AUDITS"

# Respect configuration values, with defaults
KEEP=${JOURNAL_KEEP:-7}
MAX_MB=${JOURNAL_MAX_MB:-10}
MAX_BYTES=$((MAX_MB * 1024 * 1024))

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
            
            # POSIX way to handle rotation limit
            count=$(ls -1tr "$dir/${base}."*.gz 2>/dev/null | wc -l)
            
            if [ "$count" -gt "$KEEP" ]; then
                diff=$((count - KEEP))
                ls -1tr "$dir/${base}."*.gz 2>/dev/null | head -n "$diff" | while read -r arc; do
                    [ -z "$arc" ] && continue
                    rm -f -- "$arc" || true
                done
            fi
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
            
            count=$(ls -1tr "$dir/${base}."*.gz 2>/dev/null | wc -l)
            
            if [ "$count" -gt "$KEEP" ]; then
                diff=$((count - KEEP))
                ls -1tr "$dir/${base}."*.gz 2>/dev/null | head -n "$diff" | while read -r arc; do
                    [ -z "$arc" ] && continue
                    rm -f -- "$arc" || true
                done
            fi
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