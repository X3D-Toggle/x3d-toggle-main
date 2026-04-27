#!/bin/sh
## Journal Coredump debugging tool for the X3D Toggle Project
##
## `coredump.sh`
##
## Usage: x3d-toggle journal-dump
## [Args] = vim,kate,vsc,kdevelop,none
##
## This shell script utilizes coredumpctl to capture and analyze coredumps from the x3d-daemon service.
## It will utilize your preferred editor to output the coredump, if none entered it will use the terminal instead.
## It is intended to be used as a debugging tool for the X3D Toggle Project.


_l_dir_lib="$(cd "$(dirname "$0")/../framework" && pwd)"
. "$_l_dir_lib/framework.sh"

EDITOR_CHOICE="$1"

if ! command -v coredumpctl >/dev/null 2>&1; then
    printf_step "❌ Error: coredumpctl not found. systemd-coredump is structurally required."
    exit 1
fi

printf_step "🔍 Analyzing system coredumps for 'x3d-daemon'..."

_DUMP_INFO="$JOURNAL_DUMPS/latest_coredump.txt"
_DUMP_BIN="$JOURNAL_DUMPS/latest.core"


if ! coredumpctl info x3d-daemon > "$_DUMP_INFO" 2>/dev/null; then
    printf_step "✅ No recent crashes detected for 'x3d-daemon'."
    printf_br
    printf "    Press [ENTER] to close window..."
    read -r _unused
    exit 0
fi
coredumpctl dump x3d-daemon -o "$_DUMP_BIN" >/dev/null 2>&1

printf_step "💾 Coredump artifacts successfully saved to: $VAR_DUMPS"

if [ -z "$EDITOR_CHOICE" ] || [ "$EDITOR_CHOICE" = "none" ] || [ "$EDITOR_CHOICE" = "terminal" ]; then
    printf_step "📜 Trace Output:"
    while IFS= read -r _l_line || [ -n "$_l_line" ]; do
        echo "$_l_line"
    done < "$_DUMP_INFO"
    printf_br
    printf "    Press [ENTER] to close window..."
    read -r _unused
else
    printf_step "✨ Launching IDE / Editor: $EDITOR_CHOICE"
    case "$EDITOR_CHOICE" in
        vim|vi|nano)
            "$EDITOR_CHOICE" "$_DUMP_INFO"
            ;;
        kate|kdevelop)
            "$EDITOR_CHOICE" "$_DUMP_INFO" >/dev/null 2>&1 &
            ;;
        vsc|vscode|code)
            code "$_DUMP_INFO" >/dev/null 2>&1 &
            ;;
        *)
            # Fallback attempt
            "$EDITOR_CHOICE" "$_DUMP_INFO" >/dev/null 2>&1 &
            ;;
    esac
fi

exit 0


## end of COREDUMP.SH