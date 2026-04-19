#!/bin/sh
##
## `install.sh`
##
## Standardized local installation wrapper.
## Handles safe compilation, FHS deployment, and triggers the setup wizard.
## Usage: sudo ./install.sh

set -e

if [ -f "./scripts/framework/framework.sh" ]; then
    . "./scripts/framework/framework.sh"
else
    echo "❌ Error: framework.sh not found. Ensure you are in the X3D Toggle project root."
    exit 1
fi

if [ "$(id -u)" -ne 0 ]; then
    printf_step "${PADLOCK} Installation requires elevated privileges."
    printf_step "    Please run: sudo ./install.sh"
    exit 1
fi

printf_br
printf_center "${ROCKET} X3D Toggle System Installer"
printf_divider

if [ ! -f "bin/x3d-toggle" ] || [ ! -f "build/bpf.o" ]; then
    printf_step "${HAMMER} Compiled binaries not found. Initiating build phase..."
    
    _l_user="${SUDO_USER:-$USER}"
    if [ -n "$_l_user" ] && [ "$_l_user" != "root" ]; then
        sudo -u "$_l_user" make build || exit 1
    else
        make build || exit 1
    fi
    printf_step "2,${ALRIGHT} Build complete."
else
    printf_step "${ALRIGHT} Verified compiled binaries."
fi

printf_step "${STOPSIGN} Checking for active daemon instances..."
if systemctl is-active --quiet x3d-toggle.service 2>/dev/null; then
    systemctl stop x3d-toggle.service 2>/dev/null || true
    printf_step "2,${STOPSIGN} Existing service halted."
fi

killall -9 x3d-daemon x3d-run 2>/dev/null || true

printf_step "${PACKAGE} Deploying files to FHS standard directories..."
make install DESTDIR=""
printf_step "2,${ALRIGHT} Files successfully placed in /usr/bin, /usr/lib, and /etc."

if [ -x "./setup.sh" ]; then
    printf_br
    printf_step_no_nl "${QUERY} Would you like to run the configuration wizard now? [Y/n] "
    read -r setup
    case "$setup" in
        [Nn]*)
            printf_step "2,${NOTICE} Skipping configuration. You can run 'sudo ./setup.sh' manually later."
            ;;
        *)
            printf_step "${GEAR} Starting the configuration wizard..."
            sleep 1
            ./setup.sh
            ;;
    esac
else
    printf_step "2,${WARN} setup.sh not found or not executable. Skipping configuration."
fi

## end of `INSTALL.SH`