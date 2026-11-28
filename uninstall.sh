#!/bin/sh
## `uninstall.sh`
## Atomically removes the X3D Toggle suite, gracefully stops daemons, and explicitly
## releases the AMD PMF hardware locks to restore native CPPC heuristics.
## Intended for developer teardown and deep-cleaning.
## Usage: sudo ./uninstall.sh

set -e

_l_dir_root="$(cd "$(dirname "$0")/../.." && pwd)"

if [ -f "$_l_dir_root/scripts/framework/framework.sh" ]; then
    . "$_l_dir_root/scripts/framework/framework.sh"
else
    echo "❌ Error: framework.sh not found. Cannot load UI formatting."
    exit 1
fi

if [ "$(id -u)" -ne 0 ]; then
    printf_step "${PADLOCK} Requesting elevated privileges for deep uninstallation..."
    sudo -v || exit 1
    exec sudo bash "$0" "$@"
fi

printf_br
printf_center "${ROCKET} Welcome to the x3D Toggle Deep Uninstaller"
printf_divider

printf_step "${STOPSIGN} Stopping and disabling X3D Toggle service..."
if systemctl is-active --quiet x3d-toggle.service; then
    systemctl stop x3d-toggle.service 2>/dev/null
    systemctl disable x3d-toggle.service 2>/dev/null
    printf_step "2,${STOPSIGN} Service stopped."
fi

killall -9 x3d-daemon x3d-run x3d-gui 2>/dev/null || true
pkill -u x3d-toggle 2>/dev/null || true

printf_step "${UNLOCK} Releasing hardware lock (Restoring CPPC defaults)..."
RESET_TOOL="/usr/lib/x3d-toggle/scripts/tools/reset.sh"
[ ! -f "$RESET_TOOL" ] && RESET_TOOL="$_l_dir_root/scripts/tools/reset.sh"
if [ -f "$RESET_TOOL" ]; then
    X3D_EXEC=1 bash "$RESET_TOOL"
    printf_step "2,${GEAR} Hardware returned to native CPPC control."
else
    printf_step "2,${WARN} Reset tool not found at $RESET_TOOL."
    printf_step "2,${WARN} Hardware may remain in current state until reboot."
fi

if [ -d "/sys/fs/bpf/x3d" ]; then
    printf_step "${WIPE} Removing pinned eBPF objects from /sys/fs/bpf/x3d..."
    rm -rf /sys/fs/bpf/x3d
fi

if [ -f "/usr/lib/tmpfiles.d/x3d-toggle.conf" ]; then
    printf_step "${WIPE} Cleaning up hardware node permissions..."
    rm -f "/usr/lib/tmpfiles.d/x3d-toggle.conf"
fi

rm -f "/etc/udev/rules.d/99-x3d-irq.rules"
rm -f "/etc/sysctl.d/99-x3d-networking.conf"
rm -rf "/run/x3d-toggle"

# --- STANDARD FILE UNINSTALLATION ---

printf_step "${PACKAGE} Checking installation method..."
if command -v pacman >/dev/null 2>&1; then
    INSTALLED_PKGS="$(pacman -Qq x3d-toggle-gui x3d-toggle 2>/dev/null || true)"
    if [ -n "$INSTALLED_PKGS" ]; then
        printf_step "2,${WARN} Found pacman package(s): $INSTALLED_PKGS. Removing..."
        # shellcheck disable=SC2086
        pacman -Rns --noconfirm $INSTALLED_PKGS
        printf_step "2,${TRASHCAN} Package(s) removed."
    fi
fi

# Always sweep residual/untracked files (e.g., from dev `sudo make install`)
printf_step "${WIPE} Sweeping residual binaries, assets, and fonts..."

rm -f "${USR_BIN:-/usr/bin}/x3d-toggle"
rm -f "${USR_BIN:-/usr/bin}/x3d-daemon"
rm -f "${USR_BIN:-/usr/bin}/x3d-run"
rm -f "${USR_BIN:-/usr/bin}/x3d-gui"
rm -f "${USR_BIN:-/usr/bin}/x3d-toggle-kdialog"
rm -f "${USR_BIN:-/usr/bin}/x3d"
rm -f "${USR_APPS:-/usr/share/applications}/x3d-toggle.desktop"
rm -f "${USR_APPS:-/usr/share/applications}/x3d-toggle-kdialog.desktop"
rm -rf "${USR_LIBS:-/usr/lib/x3d-toggle}"
rm -rf "${USR_ASSETS:-/usr/share/x3d-toggle}"
rm -rf "${USR_FONTS:-/usr/share/fonts/x3d-toggle}"
rm -f "${SYS_POLKIT:-/usr/lib/polkit-1/rules.d}/50-x3d_toggle-service.rules"
rm -f "${SYS_POLKIT:-/usr/lib/polkit-1/rules.d}/x3d-toggle.rules"
rm -f "${SYS_POLKIT_ACTIONS:-/usr/share/polkit-1/actions}/org.x3d_toggle.debug.policy"
rm -f "${SYS_UDEV:-/usr/lib/udev/rules.d}/99-x3d_toggle-sysfs.rules"
rm -f "${SYS_UDEV:-/usr/lib/udev/rules.d}/99-x3d-toggle.rules"
rm -f "${SYS_SYSTEMD:-/usr/lib/systemd/system}/x3d-toggle.service"
rm -f "${SYS_SYSUSERS:-/usr/lib/sysusers.d}/x3d_toggle-sysusers.conf"
rm -f "${SYS_TMPFILES:-/usr/lib/tmpfiles.d}/x3d_toggle-tmpfiles.conf"

if [ -f "$_l_dir_root/Makefile" ]; then
    printf_step "${WIPE} Running 'make uninstall' as fallback..."
    make -C "$_l_dir_root" -s uninstall
fi

# --- POST-UNINSTALL CLEANUP ---

printf_step "${WIPE} Performing Atomic Cleanup (wiping build artifacts)..."
rm -rf "${_l_dir_root:?}/bin"/* 2>/dev/null
rm -rf "${_l_dir_root:?}/build"/* 2>/dev/null

if getent passwd x3d-toggle >/dev/null; then
    printf_step "${WIPE} Removing x3d-toggle service user..."
    userdel -f x3d-toggle
fi

if getent group x3d-toggle >/dev/null; then
    printf_step "${WIPE} Removing x3d-toggle system group..."
    groupdel x3d-toggle
fi
printf_step "${TRASHCAN} All binaries and artifacts removed..."
printf_step "${RELOAD} Reloading system daemons (systemd, udev, fontconfig)..."
udevadm control --reload-rules
udevadm trigger
systemctl daemon-reload
command -v fc-cache >/dev/null 2>&1 && fc-cache -f 2>/dev/null || true

if [ -f "$_l_dir_root/Makefile" ]; then
    make -C "$_l_dir_root" -s clean
fi

if [ -t 0 ]; then
    printf_step_no_nl "${QUERY} Do you want to wipe the user configuration file? [y/N] "
    read -r WIPE_CONFIG
else
    WIPE_CONFIG="y"
fi

printf_br
case "$WIPE_CONFIG" in
    [Yy]*)
        if [ -d "${DIR_ETC:-/etc/x3d-toggle.d}" ]; then
            rm -rf "${DIR_ETC:-/etc/x3d-toggle.d}"
            printf_step "2,${TRASHCAN} User configuration removed: ${DIR_ETC:-/etc/x3d-toggle.d}"
        else
            printf_step "2,${WARN} Configuration file not found, wrapping up."
        fi ;;
    *)
        printf_step "2,${NOTICE} User configuration preserved: /etc/x3d-toggle.d" ;;
esac

if [ -t 0 ]; then
    printf_br
    printf_step_no_nl "${QUERY} Do you want to wipe the contents of 'audits/' and 'logs/'? [y/N] "
    read -r WIPE_LOGS
else
    WIPE_LOGS="n"
fi

case "$WIPE_LOGS" in
    [Yy]*)
        printf_step "${WIPE} Wiping journal logs and audits..."
        rm -rf "$_l_dir_root/dev/logging/logs"/* 2>/dev/null
        rm -rf "$_l_dir_root/dev/logging/audits"/* 2>/dev/null
        rm -rf "${VAR_LOG:-/var/log/x3d-toggle}"
        rm -rf "${VAR_LIBS:-/var/lib/x3d-toggle}"

        # Global coredump cleanup
        [ -d "/var/lib/systemd/coredump" ] && rm -f /var/lib/systemd/coredump/core.x3d-daemon.*.zst 2>/dev/null

        printf_step "2,${TRASHCAN} Logs and audits removed." ;;
    *)
        printf_step "2,${NOTICE} Logs and audits preserved." ;;
esac

printf_divider
printf_center "${SPARKLE} Cleanup Complete. ${SPARKLE}" \
              "$(hostname) restored."
printf_signature

# end of UNINSTALL.SH
