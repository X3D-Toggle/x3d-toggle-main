#!/bin/sh
##
## `archive.sh`
##
## Usage: x3d-toggle journal-archive
## Mandatory session finalization script for the X3D Toggle Project.
## - Gracefully "zips" (gzips) all active .log, .audit, and .txt files.
## - Intended to be called by ExecStopPost in systemd for final preservation.
## - Does NOT perform trimming or retention cleanup.



umask 0002

_l_dir_lib="$(cd "$(dirname "$0")/../framework" && pwd)"
. "$_l_dir_lib/framework.sh"

DIR_LOGS="${VAR_LOGS:-/var/log/x3d-toggle/logs}"
DIR_AUDITS="${VAR_AUDITS:-/var/log/x3d-toggle/audits}"
DIR_DUMPS="${VAR_DUMPS:-/var/log/x3d-toggle/coredumps}"

printf_step "📦 Finalizing diagnostics: Zipping current session data..."

archive_files() {
    _dir="$1"
    _ext="$2"
    
    for _file in "$_dir"/*"$_ext"; do
        [ -f "$_file" ] || continue
        [ -s "$_file" ] || continue
        
        _ts=$(date +%Y%m%d%H%M%S)
        _arc="$_file.$_ts.gz"
        
        printf_step "    - Compressing: $(basename "$_file")"
        if gzip -c -- "$_file" > "$_arc" 2>/dev/null; then
            if [ -w "$_file" ]; then
                : > "$_file"
            else
                rm -f -- "$_file" 2>/dev/null || true
            fi
        fi
    done
}

archive_files "$DIR_LOGS" ".log"

archive_files "$DIR_AUDITS" ".audit"
archive_files "$DIR_AUDITS" ".log"
archive_files "$DIR_DUMPS" ".txt"

printf_step "✅ Session finalization complete. All active journals zipped."

exit 0


## end of ARCHIVE.SH