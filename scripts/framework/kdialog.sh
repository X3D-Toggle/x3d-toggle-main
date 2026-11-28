#!/bin/sh
## Optional KDialog GUI Installation Script for X3D Toggle
## `kdialog.sh`
## Called conditionally by install.sh, setup.sh, deploy.sh, or `make kdialog`.

_l_dir_lib="$(cd "$(dirname "$0")" && pwd)"
. "$_l_dir_lib/framework.sh"

if [ "$X3D_EXEC" != "1" ]; then
    journal_write -37
fi

if [ -z "$X3D_XUI_LOADED" ]; then
    echo "    ❌ XUI framework (xui.sh) could not be initialized."
    echo "    Please ensure scripts/framework/xui.sh exists and is valid."
    exit 1
fi

if ! command -v kdialog >/dev/null 2>&1; then
    printf_step "2,${XOUT} Error: KDialog Dependency Missing" \
                "2,Please install 'kdialog' via your distribution package manager."
    exit 1
fi

if [ -n "${X3D_TOGGLE:-}" ] && [ -f "$X3D_TOGGLE/src/kdialog/x3d-toggle-kdialog" ]; then
    _l_kdialog_src="$X3D_TOGGLE/src/kdialog/x3d-toggle-kdialog"
    _l_kdialog_desktop="$X3D_TOGGLE/src/kdialog/x3d-toggle-kdialog.desktop"
elif [ -f "/usr/lib/x3d-toggle/kdialog/x3d-toggle-kdialog" ]; then
    _l_kdialog_src="/usr/lib/x3d-toggle/kdialog/x3d-toggle-kdialog"
    _l_kdialog_desktop="/usr/lib/x3d-toggle/kdialog/x3d-toggle-kdialog.desktop"
elif [ -f "./src/kdialog/x3d-toggle-kdialog" ]; then
    _l_kdialog_src="./src/kdialog/x3d-toggle-kdialog"
    _l_kdialog_desktop="./src/kdialog/x3d-toggle-kdialog.desktop"
else
    printf_step "2,${XOUT} Error: KDialog frontend source not found (src/kdialog/x3d-toggle-kdialog)."
    exit 1
fi

printf_step "2,${HAMMER} Staging and Installing KDialog Frontend"

if [ -n "${X3D_TOGGLE:-}" ] && [ "$X3D_TOGGLE" != "/usr/lib/x3d-toggle" ]; then
    mkdir -p "$X3D_TOGGLE/bin"
    cp "$_l_kdialog_src" "$X3D_TOGGLE/bin/x3d-toggle-kdialog"
    chmod 755 "$X3D_TOGGLE/bin/x3d-toggle-kdialog"
    if [ -n "${SUDO_USER:-}" ] && [ "$SUDO_USER" != "root" ]; then
        chown "$SUDO_USER:$(id -gn "$SUDO_USER")" "$X3D_TOGGLE/bin/x3d-toggle-kdialog" 2>/dev/null || true
    fi
fi

if [ "$(id -u)" -eq 0 ]; then
    install -Dm755 "$_l_kdialog_src" "${DESTDIR:-}${USR_BIN:-/usr/bin}/x3d-toggle-kdialog"
    if [ -f "$_l_kdialog_desktop" ]; then
        install -Dm644 "$_l_kdialog_desktop" "${DESTDIR:-}${USR_APPS:-/usr/share/applications}/x3d-toggle-kdialog.desktop"
    fi
    if [ -z "${DESTDIR:-}" ] && command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database -q "${USR_APPS:-/usr/share/applications}" 2>/dev/null || true
    fi
else
    printf_step "2,${WARN} Warning: Root privileges required for system installation (staged locally at bin/x3d-toggle-kdialog)."
    printf_step "2,Run with sudo to finalize installation."
fi

if [ "$(id -u)" -eq 0 ]; then
    printf_step "2,${ALRIGHT} KDialog GUI installed successfully (run: x3d-toggle kdialog)"
else
    printf_step "2,${ALRIGHT} KDialog GUI staged locally (bin/x3d-toggle-kdialog)"
fi
exit 0

## end of KDIALOG.SH
