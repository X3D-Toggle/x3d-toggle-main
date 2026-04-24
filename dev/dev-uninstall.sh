#!/bin/sh
## Uninstaller script for x3D Toggle Developer Mode Environment
## `uninstall-dev.sh`
## Uninstall script which will remove all instances of the Developer Mode
## Environment and restore x3D Toggle to Standard User Environment.

_l_dir_lib="$(cd "$(dirname "$0")" && pwd)"
if [ -f "$_l_dir_lib/../../scripts/framework/framework.sh" ]; then
    . "$_l_dir_lib/../../scripts/framework/framework.sh"
fi

printf_step "${TRASHCAN}  Removing X3D Toggle Developer Suite..."

_l_dev_root="$X3D_TOGGLE/dev"
_l_abs_cwd="$(readlink -f "$PWD")"
_l_abs_dev="$(readlink -f "$_l_dev_root")"

if [ "$_l_abs_cwd" = "$_l_abs_dev" ]; then
    journal_write -33 "$_l_dev_root"
fi

if confirm "Wipe Active Developer Toolsets (scripts/tools/*.sh)?"; then
    printf_step "${WIPE} Removing compiled toolsets..."
    rm -f "$DESTDIR$DIR_LIB/tools"/*.sh 2>/dev/null
fi

printf_step "${GEAR} Resetting Sandbox Environment..."
rm -rf "$_l_dev_root/sandbox"/* 2>/dev/null
rm -rf "$_l_dev_root/tools"/* 2>/dev/null
if confirm "Wipe Developer Audit History (clang-tidy, valgrind, etc)?"; then
    printf_step "${WIPE} Wiping audit archives (contents only)..."
    rm -rf "$_l_dev_root/logging/audits"/* 2>/dev/null
fi

if confirm "Wipe Developer Diagnostic Logs?"; then
    printf_step "${WIPE} Wiping diagnostic log contents..."
    rm -f "$_l_dev_root/logging/logs"/*.log 2>/dev/null
fi

printf_step "${ALRIGHT} Developer environment reset. (Source code preserved)."

# end of UNINSTALL-DEV.SH
