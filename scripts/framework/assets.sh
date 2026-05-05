#!/bin/sh
## Assets Package Manager for the X3D Toggle Project
## `assets.sh`
## Installation bridge for icon assets.

_l_dir_lib="$(cd "$(dirname "$0")" && pwd)"
. "$_l_dir_lib/framework.sh"

if [ "$X3D_EXEC" != "1" ]; then
    journal_write -37
fi

install_assets() {
    install -dm755 "$DESTDIR$DIR_ASSETS"

    if [ -f "$X3D_TOGGLE/assets/ryzen.jpeg" ]; then
        install -m644 "$X3D_TOGGLE/assets/ryzen.jpeg" "$DESTDIR$DIR_ASSETS/x3d-toggle-launch.jpeg"
    fi
    if [ -f "$X3D_TOGGLE/assets/x3d-toggle.jpg" ]; then
        install -m644 "$X3D_TOGGLE/assets/x3d-toggle.jpg" "$DESTDIR$DIR_ASSETS/x3d-toggle.jpg"
    fi
    if [ -f "$X3D_TOGGLE/assets/amd.svg" ]; then
        install -m644 "$X3D_TOGGLE/assets/amd.svg" "$DESTDIR$DIR_ASSETS/x3d-toggle-amd.svg"
    fi
    if [ -f "$X3D_TOGGLE/assets/ryzen.svg" ]; then
        install -m644 "$X3D_TOGGLE/assets/ryzen.svg" "$DESTDIR$DIR_ASSETS/x3d-toggle-ryzen.svg"
    fi
}

case "$1" in
    --install-assets)
        install_assets ;;
    *)
        printf_step "Usage: assets.sh --install-assets"
        exit 1 ;;
esac

## end of assets.sh
