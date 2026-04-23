#!/bin/sh
# Optional GTK4 GUI Compilation Script for X3D Toggle
# Called conditionally by setup.sh

if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists gtk4 libadwaita-1; then
    printf "    ❌ GTK4 or libadwaita-1 development libraries not found.\n"
    printf "    Please install them via your package manager to build the GUI.\n"
    exit 1
fi

printf "    ⚙️ Compiling GTK4 GResource manifest...\n"
if ! glib-compile-resources --sourcedir=src/gtk4 \
    --generate-source --target=src/gtk4/x3d-gui-resources.c \
    src/gtk4/x3d-toggle-gui.gresource.xml; then
    printf "    ❌ Failed to compile GResources.\n"
    exit 1
fi

printf "    ⚙️ Compiling x3d-gui binary...\n"
if ! clang $(pkg-config --cflags gtk4 libadwaita-1) -Wall -O2 \
    src/gtk4/gui.c src/gtk4/x3d-gui-resources.c -o x3d-gui \
    $(pkg-config --libs gtk4 libadwaita-1); then
    printf "    ❌ Compilation failed.\n"
    rm -f src/gtk4/x3d-gui-resources.c
    exit 1
fi

printf "    📦 Installing x3d-gui to /usr/bin...\n"
install -m755 x3d-gui /usr/bin/x3d-gui

# Clean up build artifacts
rm -f x3d-gui src/gtk4/x3d-gui-resources.c

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

# Patch desktop entry
DESKTOP_SYS="/usr/share/applications/x3d-toggle.desktop"
if [ -f "$DESKTOP_SYS" ]; then
    patch_desktop "$DESKTOP_SYS"
    printf "    ✔️ System desktop entry patched to launch x3d-gui.\n"
fi

if [ -n "$ACTUAL_USER" ]; then
    DESKTOP_USR="/home/$ACTUAL_USER/Desktop/x3d-toggle.desktop"
    if [ -f "$DESKTOP_USR" ]; then
        patch_desktop "$DESKTOP_USR"
        printf "    ✔️ User desktop entry patched to launch x3d-gui.\n"
    fi
fi

printf "    ✅ GTK4 GUI successfully built and installed!\n"
exit 0
