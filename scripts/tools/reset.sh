#!/bin/sh
## Reset Script for the X3D Toggle Project
##
## `reset.sh` - Shell Object
##
## Usage: x3d-toggle reset
## [Args] = none
##
## Deterministically rips the state out of the amd_x3d_vcache driver
## and forces a re-probe to restore native CPPC heuristics.
## Since there is no `auto` software state for the x3d_mode, this method
## is used to restore the hardware to its default state.

#if [ "$X3D_EXEC" != "1" ]; then exit 1; fi

ROOT_REPO="$(cd "$(dirname "$0")/../.." && pwd)"
. "$ROOT_REPO/scripts/framework/framework.sh"

if ! resolve_node; then
    printf_step "${XOUT} Error: AMD 3D V-Cache sysfs node not found." >&2
    exit 1
fi

MODE_FILE="$NODE_PATH"

DEV_PATH=$(dirname "$MODE_FILE")
DEV_NAME=$(basename "$DEV_PATH")

DRIVER_DIR="/sys/bus/platform/drivers/amd_x3d_vcache"

if [ -w "$DRIVER_DIR/unbind" ] && [ -w "$DRIVER_DIR/bind" ]; then
    printf_step "${GEAR} Releasing hardware lock for $DEV_NAME..."
    echo "$DEV_NAME" > "$DRIVER_DIR/unbind"
    sleep 0.05
    echo "$DEV_NAME" > "$DRIVER_DIR/bind"
    
    if [ -f "$MODE_FILE" ]; then
        read -r CURRENT_MODE < "$MODE_FILE"
        printf_step "${ALRIGHT} Hardware reset successful. Current Mode: ${COLOR_CYAN}$CURRENT_MODE${COLOR_RESET}"
    fi
    exit 0
else
    printf_step "${XOUT} Error: Insufficient permissions to write to driver bind nodes." >&2
    exit 2
fi

## end of RESET.SH
