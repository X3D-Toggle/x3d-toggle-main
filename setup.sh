#!/bin/sh
##
## `setup.sh`
##
## Interactive Post-Installation Setup Guide for X3D Toggle.
## Configures user preferences, daemon state, and all settings.conf variables.
## Must be run as root after the package is installed.

set -e

if [ "$(id -u)" -ne 0 ]; then
    echo "❌ Error: setup.sh must be run with sudo/root privileges."
    exit 1
fi

if [ -f "./scripts/framework/framework.sh" ]; then
    . "./scripts/framework/framework.sh"
elif [ -f "/usr/lib/x3d-toggle/scripts/framework/framework.sh" ]; then
    . "/usr/lib/x3d-toggle/scripts/framework/framework.sh"
else
    echo "⚠️  Warning: framework.sh not found. Proceeding with standard terminal I/O."
    printf_step() { echo "  > $*"; }
    printf_step_no_nl() { echo -n "  > $*"; }
    printf_center() { echo "=== $* ==="; }
    printf_br() { echo ""; }
    printf_divider() { echo "----------------------------------------"; }
fi

CONF_FILE="${X3D_CONF:-/etc/x3d-toggle.d/settings.conf}"

if [ ! -f "$CONF_FILE" ]; then
    printf_step "❌ Error: Configuration file not found at $CONF_FILE."
    printf_step "Please ensure X3D-Toggle is installed via 'make install' or pacman first."
    exit 1
fi

. "$CONF_FILE"

set_config() {
    local key="$1"
    local val="$2"
    sed -i "s|^${key}=.*|${key}=${val}|" "$CONF_FILE"
}

printf_br
printf_center "🚀 X3D Toggle Interactive Setup Guide 🚀"
printf_divider
printf_step "This wizard will configure your daemon preferences."
printf_step "Target Configuration: $CONF_FILE"
printf_br

printf_step "[1] Workload Detection Engine"
printf_step "    eBPF is highly recommended for zero-overhead kernel-level scheduling."
printf_step "    Procfs polling is a fallback for incompatible kernels."
printf_step_no_nl "❓ Enable eBPF Scheduler? (Highly Recommended) [Y/n] "
read -r opt_ebpf
case "$opt_ebpf" in
    [Nn]*) set_config "EBPF_ENABLE" "0" ;;
    *)     set_config "EBPF_ENABLE" "1" ;;
esac
printf_br

printf_step "[2] Detection Level"
printf_step "    Strict (1): Only respects explicit gamemode/steam launches."
printf_step "    Loose  (2): Aggressively scans high-load .desktop apps."
printf_step_no_nl "❓ Set detection level [1/2] (Default: 2): "
read -r opt_detect
case "$opt_detect" in
    1) set_config "DETECTION_LEVEL" "1" ;;
    *) set_config "DETECTION_LEVEL" "2" ;;
esac
printf_br

printf_step "[3] Performance Thresholds"
printf_step "    Polling Interval (1-10s): How often the daemon checks activity."
printf_step_no_nl "❓ Set Polling Interval [Default: 3]: "
read -r opt_poll
[ -z "$opt_poll" ] && opt_poll="${POLLING_INTERVAL}"
set_config "POLLING_INTERVAL" "$opt_poll"

printf_step "    Load Threshold (10-90%): Total CPU usage required to trigger throughput mode."
printf_step_no_nl "❓ Set Compute Load Threshold [Default: 50]: "
read -r opt_load
[ -z "$opt_load" ] && opt_load="${LOAD_THRESHOLD}"
set_config "LOAD_THRESHOLD" "$opt_load"
printf_br

printf_step "[4] Core Affinity Management"
printf_step "    Auto   (0): Sysfs caching only. OS handles thread priority."
printf_step "    By Die (1): Strictly maps detected games to the 3D V-Cache CCD."
printf_step "    Manual (2): Uses custom defined affinity mask."
printf_step_no_nl "❓ Set Affinity Mode [0/1/2] (Default: 0): "
read -r opt_affinity
case "$opt_affinity" in
    1) set_config "AFFINITY_LEVEL" "1" ;;
    2) 
       set_config "AFFINITY_LEVEL" "2" 
       printf_step_no_nl "   ❓ Enter Manual Affinity Mask (e.g., 0-7,16-23): "
       read -r opt_mask
       set_config "AFFINITY_MASK" "$opt_mask"
       ;;
    *) set_config "AFFINITY_LEVEL" "0" ;;
esac
printf_br

printf_step "[5] Fallback Hardware Profile"
printf_step "    The hardware profile to apply when no games/load are detected."
printf_step "    Options: cache, frequency, default"
printf_step_no_nl "❓ Set Fallback Profile [Default: default]: "
read -r opt_fallback
case "$opt_fallback" in
    cache|frequency) set_config "FALLBACK_PROFILE" "$opt_fallback" ;;
    *) set_config "FALLBACK_PROFILE" "default" ;;
esac
printf_br

printf_step "[6] Advanced / Developer Options"
printf_step_no_nl "❓ Enable Developer Mode (Extended diagnostics/stress tests)? [y/N] "
read -r opt_dev
case "$opt_dev" in
    [Yy]*) set_config "DEV_ENABLE" "1" ;;
    *)     set_config "DEV_ENABLE" "0" ;;
esac

printf_step_no_nl "❓ Enable Debug Logging? [y/N] "
read -r opt_debug
case "$opt_debug" in
    [Yy]*) set_config "DEBUG_ENABLE" "1" ;;
    *)     set_config "DEBUG_ENABLE" "0" ;;
esac
printf_br

ACTUAL_USER="${SUDO_USER:-$USER}"
DESKTOP_DIR="/home/$ACTUAL_USER/Desktop"
if [ -d "$DESKTOP_DIR" ] && [ -f "/usr/share/applications/x3d-toggle.desktop" ]; then
    printf_step "[7] Desktop Integration"
    printf_step_no_nl "❓ Add X3D Toggle to your Desktop? [y/N] "
    read -r opt_desktop
    case "$opt_desktop" in
        [Yy]*)
            cp "/usr/share/applications/x3d-toggle.desktop" "$DESKTOP_DIR/"
            chown "$ACTUAL_USER":"$ACTUAL_USER" "$DESKTOP_DIR/x3d-toggle.desktop"
            chmod 700 "$DESKTOP_DIR/x3d-toggle.desktop"
            printf_step "✔️ Desktop icon created."
            ;;
    esac
    printf_br
fi

if [ -n "$ACTUAL_USER" ] && [ "$ACTUAL_USER" != "root" ]; then
    printf_step "[8] User Identity & System Synchronization"
    
    printf_step "    Syncing system identity and hardware permissions..."
    # Strict execution: Must succeed to guarantee hardware access
    systemd-sysusers
    systemd-tmpfiles --create "/usr/lib/tmpfiles.d/x3d_toggle-tmpfiles.conf"
    udevadm control --reload-rules
    udevadm trigger
    systemctl daemon-reload

    _l_groups="$(id -nG "$ACTUAL_USER")"
    case " $_l_groups " in
        *" x3d-toggle "*) 
            printf_step "✔️ User '$ACTUAL_USER' is already a member of the 'x3d-toggle' group."
            ;;
        *)
            printf_step "    The 'x3d-toggle' group is required for CLI interaction without sudo."
            printf_step_no_nl "❓ Add user '$ACTUAL_USER' to the group now? [Y/n] "
            read -r opt_group
            case "$opt_group" in
                [Nn]*) printf_step "⚠️  Skipping group addition. You will need 'sudo' for CLI commands." ;;
                *)
                    usermod -aG x3d-toggle "$ACTUAL_USER"
                    printf_step "✔️ User added to group. Logout/Login still required for the session."
                    ;;
            esac
            ;;
    esac
    printf_step "✔️ System synchronized."
    printf_br
fi

printf_step "[9] Systemd Daemon Activation"
printf_step "    Enabling the daemon allows autonomous workload detection in the background."
printf_step_no_nl "❓ Enable and start x3d-toggle.service now? [Y/n] "
read -r opt_daemon
case "$opt_daemon" in
    [Nn]*)
        printf_step "✔️ Skipping daemon activation. You can run 'x3d auto' manually."
        set_config "DAEMON_STATE" "manual"
        ;;
    *)
        set_config "DAEMON_STATE" "default"
        # No need to daemon-reload here, it was handled sequentially in step 8
        systemctl enable --now x3d-toggle.service
        sleep 2
        if systemctl is-active --quiet x3d-toggle.service; then
            # Strict execution: Will throw an error if the daemon cannot receive commands
            x3d-toggle default
            printf_step "✔️ Daemon activated successfully."
        else
            printf_step "❌ Warning: Daemon failed to start. Check 'journalctl -u x3d-toggle'."
            set_config "DAEMON_STATE" "manual"
        fi
        ;;
esac
printf_br

printf_divider
printf_center "⚙️ Setup Complete! ⚙️"
printf_step "You can change these settings later via the UI or by editing $CONF_FILE."
printf_br

## end of SETUP.SH