#!/bin/sh
## Deployment of system policies for the X3D Toggle Project
##
## `policies.sh`
##
## Installation bridge for system policies (udev, polkit, tmpfiles, sysusers).

if [ "$X3D_EXEC" != "1" ]; then exit 1; fi

_l_dir_lib="$(cd "$(dirname "$0")" && pwd)"
. "$_l_dir_lib/framework.sh"

install_policies() {

    if [ -f "$X3D_TOGGLE/packaging/x3d-toggle.service" ]; then
        install -dm755 "$DESTDIR$SYS_SYSTEMD"
        install -m644 "$X3D_TOGGLE/packaging/x3d-toggle.service" "$DESTDIR$SYS_SYSTEMD/x3d-toggle.service"
    fi

    if [ -f "$X3D_TOGGLE/packaging/sysfs.rules" ]; then
        install -dm755 "$DESTDIR$SYS_UDEV"
        install -m644 "$X3D_TOGGLE/packaging/sysfs.rules" "$DESTDIR$SYS_UDEV/99-x3d_toggle-sysfs.rules"
    fi

    if [ -f "$X3D_TOGGLE/packaging/50-x3d_toggle-service.rules" ]; then
        install -dm755 "$DESTDIR$SYS_POLKIT"
        install -m644 "$X3D_TOGGLE/packaging/50-x3d_toggle-service.rules" "$DESTDIR$SYS_POLKIT/50-x3d_toggle-service.rules"
    fi

    if [ -f "$X3D_TOGGLE/packaging/sysusers.conf" ]; then
        install -dm755 "$DESTDIR$SYS_SYSUSERS"
        install -m644 "$X3D_TOGGLE/packaging/sysusers.conf" "$DESTDIR$SYS_SYSUSERS/x3d_toggle-sysusers.conf"
    fi
    if [ -f "$X3D_TOGGLE/packaging/tmpfiles.conf" ]; then
        install -dm755 "$DESTDIR$SYS_TMPFILES"
        install -m644 "$X3D_TOGGLE/packaging/tmpfiles.conf" "$DESTDIR$SYS_TMPFILES/x3d_toggle-tmpfiles.conf"
    fi

    # 5. Desktop Entry
    if [ -f "$X3D_TOGGLE/packaging/x3d-toggle.desktop" ]; then
        install -dm755 "$DESTDIR$USR_APPS"
        install -m644 "$X3D_TOGGLE/packaging/x3d-toggle.desktop" "$DESTDIR$USR_APPS/x3d-toggle.desktop"
    fi

    # Silenced
}

case "$1" in
    --install-policies)
        install_policies ;;
    *)
        printf_step "Usage: policies.sh --install-policies"
        exit 1 ;;
esac

## end of POLICIES.SH
