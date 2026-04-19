#!/bin/sh
## Assets Package Manager for the X3D Toggle Project
##
## `assets.sh`
##
## Installation bridge for icon assets.

if [ "$X3D_EXEC" != "1" ]; then exit 1; fi

_l_dir_lib="$(cd "$(dirname "$0")" && pwd)"
. "$_l_dir_lib/framework.so"

install_assets() {
    install -dm755 "$DESTDIR$DIR_ASSETS"

    if [ -f "$X3D_TOGGLE/assets/ryzen.jpeg" ]; then
        install -m644 "$X3D_TOGGLE/assets/ryzen.jpeg" "$DESTDIR$DIR_ASSETS/x3d-toggle-launch.jpeg"
    fi
    if [ -f "$X3D_TOGGLE/assets/ryzenlogo.svg" ]; then
        install -m644 "$X3D_TOGGLE/assets/ryzenlogo.svg" "$DESTDIR$DIR_ASSETS/x3d-toggle.svg"
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
