#!/bin/sh
# Optional GTK4 GUI Compilation Script for X3D Toggle
# Called conditionally by setup.sh

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

if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists gtk4 libadwaita-1 gio-unix-2.0; then
    printf_step "2,${XOUT} Error: GTK4 Development Libraries Missing" \
                "2,Please install 'gtk4' and 'libadwaita-1' development headers."
    exit 1
fi

if [ -n "${X3D_TOGGLE:-}" ] && [ -f "$X3D_TOGGLE/src/gtk4/gui.c" ]; then
    cd "$X3D_TOGGLE" || exit 1
fi

printf_step "2,${HAMMER} Compiling and Installing GTK4 Frontend"

mkdir -p bin

if ! glib-compile-resources --sourcedir=src/gtk4 \
    --generate-source --target=src/gtk4/x3d-gui-resources.c \
    src/gtk4/x3d-toggle-gui.gresource.xml; then
    printf_step "2,${XOUT} Error: GResource Compilation Failed"
    exit 1
fi

# shellcheck disable=SC2046
if ! clang $(pkg-config --cflags gtk4 libadwaita-1 gio-unix-2.0) -Wall -Wextra -O2 \
    -Ibuild -Iinclude -DGUI_BUILD -DLIBC_NO_BOOTSTRAP \
    src/gtk4/gui.c src/gtk4/x3d-gui-resources.c src/gtk4/stubs.c \
    src/libc.c src/worker.c build/xui.c build/ccd.c src/error.c src/cli/misc.c \
    -o bin/x3d-gui \
    $(pkg-config --libs gtk4 libadwaita-1 gio-unix-2.0); then
    printf_step "2,${XOUT} Error: Compilation Failed"
    rm -f src/gtk4/x3d-gui-resources.c
    exit 1
fi

rm -f src/gtk4/x3d-gui-resources.c

if [ -n "${SUDO_USER:-}" ] && [ "$SUDO_USER" != "root" ]; then
    chown "$SUDO_USER:$(id -gn "$SUDO_USER")" bin/x3d-gui 2>/dev/null || true
fi

if [ "$(id -u)" -eq 0 ]; then
    install -Dm755 bin/x3d-gui "${DESTDIR:-}${USR_BIN:-/usr/bin}/x3d-gui"
else
    printf_step "2,${WARN} Warning: Root privileges required for installation (built locally at bin/x3d-gui)."
    printf_step "2,Run with sudo to finalize installation."
fi

patch_desktop() {
    _file="$1"
    if [ -f "$_file" ]; then
        _tmp="${_file}.tmp"
        while IFS= read -r line; do
            case "$line" in
                Exec=*) printf "Exec=x3d-gui\n" ;;
                *)      printf "%s\n" "$line" ;;
            esac
        done < "$_file" > "$_tmp"
        chmod --reference="$_file" "$_tmp" 2>/dev/null || chmod 644 "$_tmp"
        mv "$_tmp" "$_file"
    fi
}

DESKTOP_SYS="/usr/share/applications/x3d-toggle.desktop"
if [ -f "$DESKTOP_SYS" ]; then
    if [ "$(id -u)" -eq 0 ]; then
        patch_desktop "$DESKTOP_SYS"
        printf_step "2,${ALRIGHT} System Desktop entry patched to launch frontend."
    fi
fi

if [ -n "$ACTUAL_USER" ]; then
    DESKTOP_USR="/home/$ACTUAL_USER/Desktop/x3d-toggle.desktop"
    if [ -f "$DESKTOP_USR" ]; then
        patch_desktop "$DESKTOP_USR"
        printf_step "2,${ALRIGHT} User desktop entry patched to launch frontend."
    fi
fi

if [ "$(id -u)" -eq 0 ]; then
    printf_step "2,${ALRIGHT} GTK4 GUI installed successfully"
else
    printf_step "2,${ALRIGHT} GTK4 GUI built locally (bin/x3d-gui)"
fi
exit 0

## end of GUI.SH
