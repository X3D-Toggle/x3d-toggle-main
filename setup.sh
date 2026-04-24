#!/bin/sh
## `setup.sh`
## Interactive Post-Installation Setup Guide for X3D Toggle.
## Configures user preferences, daemon state, and all settings.conf variables.
## Must be run as root after the package is installed.
## Usage: sudo make setup

set -e
export X3D_SETUP=1
GUI_INSTALLED=0

if [ -f "./scripts/framework/framework.sh" ]; then
    . "./scripts/framework/framework.sh"
elif [ -f "/usr/lib/x3d-toggle/scripts/framework/framework.sh" ]; then
    . "/usr/lib/x3d-toggle/scripts/framework/framework.sh"
else
    echo "⚠️  Warning: framework.sh not found. Proceeding with standard terminal I/O."
    XOUT="❌"; WARN="⚠️"; ALRIGHT="✔️"; QUERY="❓"; ROCKET="🚀"; GEAR="⚙️"; STOPSIGN="🛑"; INFO="🌐"
    printf_step() { echo "  > $*"; }
    printf_step_no_nl() { echo -n "  > $*"; }
    printf_center() { echo "=== $* ==="; }
    printf_br() { echo ""; }
    printf_divider() { echo "----------------------------------------"; }
fi

if [ "$(id -u)" -ne 0 ]; then
    printf_step "2,${XOUT} Error: setup.sh must be run with sudo/root privileges."
    printf_step "2,Try: sudo make setup"
    exit 1
fi

CONF_FILE="${X3D_CONF:-/etc/x3d-toggle.d/settings.conf}"

if [ ! -f "$CONF_FILE" ]; then
    printf_step "${XOUT} Error: Configuration file not found at $CONF_FILE."
    printf_step "Please ensure X3D-Toggle is installed via 'make install' or pacman first."
    exit 1
fi

. "$CONF_FILE"

printf_step "${RELOAD} Syncing system identity and hardware permissions..."
systemd-sysusers
systemd-tmpfiles --create "/usr/lib/tmpfiles.d/x3d_toggle-tmpfiles.conf"
udevadm control --reload-rules
udevadm trigger
systemctl daemon-reload
printf_step "2,${ALRIGHT} System synchronized."

set_config() {
    _key="$1"
    _val="$2"
    _temp_file="${CONF_FILE}.tmp"
    _found=0
    while IFS= read -r _l_line || [ -n "$_l_line" ]; do
        case "$_l_line" in
            "${_key}="*) 
                echo "${_key}=${_val}"
                _found=1
                ;;
            *) echo "$_l_line" ;;
        esac
    done < "$CONF_FILE" > "$_temp_file"
    if [ "$_found" -eq 0 ]; then
        echo "${_key}=${_val}" >> "$_temp_file"
    fi
    if ! mv -f "$_temp_file" "$CONF_FILE"; then
        x3d-toggle gui-log "setup.sh failed to write configuration key: $_key" 2>/dev/null || true
        printf_step "${XOUT} Error: Failed to write to $CONF_FILE"
    fi
}

quick_setup() {
    _mode="$1"
    case "$_mode" in
        "y") _m_text="FULL (Everything + Dev)"; _dev=1; _int=1 ;;
        "n") _m_text="MINIMAL (Basic Config)"; _dev=0; _int=0 ;;
        "s") _m_text="STANDARD (Suite - No Dev)"; _dev=0; _int=1 ;;
        "l") _m_text="DEV-MINIMAL (Minimal + Dev)"; _dev=1; _int=0 ;;
    esac
    printf_step "${GEAR} Running Quick Setup: $_m_text"
    
    set_config "EBPF_ENABLE" "1"
    set_config "DETECTION_LEVEL" "2"
    set_config "POLLING_INTERVAL" "3"
    set_config "LOAD_THRESHOLD" "50"
    set_config "AFFINITY_LEVEL" "0"
    set_config "FALLBACK_PROFILE" "default"
    set_config "DEV_ENABLE" "$_dev"
    set_config "DEBUG_ENABLE" "$_dev"

    ACTUAL_USER="${SUDO_USER:-$USER}"
    if [ "$_int" = "1" ]; then
        # Desktop Integration
        _d_dir="/home/$ACTUAL_USER/Desktop"
        if [ -d "$_d_dir" ] && [ -f "/usr/share/applications/x3d-toggle.desktop" ]; then
            cp "/usr/share/applications/x3d-toggle.desktop" "$_d_dir/"
            chown "$ACTUAL_USER":"$ACTUAL_USER" "$_d_dir/x3d-toggle.desktop"
            chmod 644 "$_d_dir/x3d-toggle.desktop"
            printf_step "2,${ALRIGHT} Desktop icon created."
        fi

        # GTK4 GUI
        if [ -f "./scripts/framework/gui.sh" ]; then
            X3D_EXEC=1 sh ./scripts/framework/gui.sh && GUI_INSTALLED=1
        fi

        # Group Configuration
        if [ -n "$ACTUAL_USER" ] && [ "$ACTUAL_USER" != "root" ]; then
            usermod -aG x3d-toggle "$ACTUAL_USER"
            printf_step "2,${ALRIGHT} User '$ACTUAL_USER' added to 'x3d-toggle' group."
        fi
    fi

    printf_br
    printf_divider
    printf_center "${GEAR} Setup Complete! ${GEAR}"
    printf_center "Settings saved to $CONF_FILE"
    if [ "$GUI_INSTALLED" -eq 1 ]; then
        printf_br
        printf_center "${GEAR} You can now launch the GUI by running 'x3d-gui' or"
        printf_center "from your application menu."
    fi
    printf_signature
    exit 0
}

printf_br
printf_center "${ROCKET} X3D Toggle Interactive Setup Guide ${ROCKET}"
printf_divider

printf_step "Press ${UP}    for FULL SUITE CONFIGURATION (Defaults + YES to all + DEV)"
printf_step "Press ${RIGHT} for STANDARD SUITE CONFIGURATION (Defaults + YES to all - NO DEV)"
printf_step "Press ${DOWN}  for MINIMAL SUITE CONFIGURATION (Defaults + NO to all - NO DEV)"
printf_step "Press ${LEFT}  for DEV-MINIMAL SUITE CONFIGURATION (Minimal Defaults + DEVELOPER MODE)"
printf_step "Press ${COLOR_RED}[ENTER]${COLOR_RESET} for INTERACTIVE SETUP AND CONFIGURATION"
printf_br

_old_stty=$(stty -g)
stty -icanon -echo min 1 time 30
_key=$(dd bs=3 count=1 2>/dev/null)
stty "$_old_stty"

if [ "$_key" = "$SEQ_UP" ]; then quick_setup "y"
elif [ "$_key" = "$SEQ_DOWN" ]; then quick_setup "n"
elif [ "$_key" = "$SEQ_RIGHT" ]; then quick_setup "s"
elif [ "$_key" = "$SEQ_LEFT" ]; then quick_setup "l"
fi

printf_step "This wizard will configure your daemon preferences."
printf_step "Target Configuration: $CONF_FILE"
printf_br

printf_step "[1] Workload Detection Engine"
printf_step "2,eBPF is highly recommended for zero-overhead kernel-level scheduling."
printf_step "2,Procfs polling is a fallback for incompatible kernels."
printf_step_no_nl "${QUERY} Enable eBPF Scheduler? (Highly Recommended) [Y/n] "
read -r opt_ebpf
case "$opt_ebpf" in
    [Nn]*) set_config "EBPF_ENABLE" "0" ;;
    *)     set_config "EBPF_ENABLE" "1" ;;
esac
printf_br

printf_step "[2] Detection Level"
printf_step "2,Strict (1): Only respects explicit gamemode/steam launches."
printf_step "2,Loose  (2): Aggressively scans high-load .desktop apps."
printf_step_no_nl "${QUERY} Set detection level [1/2] (Default: 2): "
read -r opt_detect
case "$opt_detect" in
    1) set_config "DETECTION_LEVEL" "1" ;;
    *) set_config "DETECTION_LEVEL" "2" ;;
esac
printf_br

printf_step "[3] Performance Thresholds"
printf_step "2,Polling Interval (1-10s): How often the daemon checks activity."
printf_step_no_nl "${QUERY} Set Polling Interval [Default: 3]: "
read -r opt_poll
[ -z "$opt_poll" ] && opt_poll="${POLLING_INTERVAL:-3}"
set_config "POLLING_INTERVAL" "$opt_poll"

printf_step "2,Load Threshold (10-90%): Total CPU usage required to trigger throughput mode."
printf_step_no_nl "${QUERY} Set Compute Load Threshold [Default: 50]: "
read -r opt_load
[ -z "$opt_load" ] && opt_load="${LOAD_THRESHOLD:-50}"
set_config "LOAD_THRESHOLD" "$opt_load"
printf_br

printf_step "[4] Core Affinity Management"
printf_step "2,Auto   (0): Sysfs caching only. OS handles thread priority."
printf_step "2,By Die (1): Strictly maps detected games to the 3D V-Cache CCD."
printf_step "2,Manual (2): Uses custom defined affinity mask."
printf_step_no_nl "${QUERY} Set Affinity Mode [0/1/2] (Default: 0): "
read -r opt_affinity
case "$opt_affinity" in
    1) set_config "AFFINITY_LEVEL" "1" ;;
    2) 
       set_config "AFFINITY_LEVEL" "2" 
       printf_step_no_nl "2,${QUERY} Enter Manual Affinity Mask (e.g., 0-7,16-23): "
       read -r opt_mask
       set_config "AFFINITY_MASK" "$opt_mask"
       ;;
    *) set_config "AFFINITY_LEVEL" "0" ;;
esac
printf_br

printf_step "[5] Fallback Hardware Profile"
printf_step "2,The hardware profile to apply when no games/load are detected."
printf_step "2,Options: cache, frequency, default"
printf_step_no_nl "${QUERY} Set Fallback Profile [Default: default]: "
read -r opt_fallback
case "$opt_fallback" in
    cache|frequency) set_config "FALLBACK_PROFILE" "$opt_fallback" ;;
    *) set_config "FALLBACK_PROFILE" "default" ;;
esac
printf_br

printf_step "[6] Advanced / Developer Options"
printf_step_no_nl "${QUERY} Enable Developer Mode (Extended diagnostics/stress tests)? [y/N] "
read -r opt_dev
case "$opt_dev" in
    [Yy]*) set_config "DEV_ENABLE" "1" ;;
    *)     set_config "DEV_ENABLE" "0" ;;
esac

printf_step_no_nl "${QUERY} Enable Debug Logging? [y/N] "
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
    printf_step_no_nl "${QUERY} Add X3D Toggle to your Desktop? [y/N] "
    read -r opt_desktop
    case "$opt_desktop" in
        [Yy]*)
            cp "/usr/share/applications/x3d-toggle.desktop" "$DESKTOP_DIR/"
            chown "$ACTUAL_USER":"$ACTUAL_USER" "$DESKTOP_DIR/x3d-toggle.desktop"
            chmod 644 "$DESKTOP_DIR/x3d-toggle.desktop"
            printf_step "2,${ALRIGHT} Desktop icon created."
            ;;
    esac
    printf_br
fi

if [ -n "$ACTUAL_USER" ] && [ "$ACTUAL_USER" != "root" ]; then
    printf_step "[8] GTK4 Graphical Dashboard/Management Tool (Optional)"
    printf_step_no_nl "${QUERY} Install the GTK4 GUI? [y/N] "
    read -r opt_gui
    case "$opt_gui" in
        [Yy]*)
            if [ -f "./scripts/framework/gui.sh" ]; then
                X3D_EXEC=1 sh ./scripts/framework/gui.sh && GUI_INSTALLED=1
            elif [ -f "/usr/lib/x3d-toggle/scripts/framework/gui.sh" ]; then
                X3D_EXEC=1 sh /usr/lib/x3d-toggle/scripts/framework/gui.sh && GUI_INSTALLED=1
            else
                printf_step "2,${WARN} Cannot locate gui.sh framework script. Skipping."
            fi
            ;;
    esac

    printf_br
    printf_step "[9] User Group Configuration"
    _l_groups="$(id -nG "$ACTUAL_USER")"
    case " $_l_groups " in
        *" x3d-toggle "*) 
            printf_step "2,${ALRIGHT} User '$ACTUAL_USER' is already a member of the 'x3d-toggle' group."
            ;;
        *)
            printf_step "2,The 'x3d-toggle' group is required for CLI interaction without sudo."
            printf_step_no_nl "2,${QUERY} Add user '$ACTUAL_USER' to the group now? [Y/n] "
            read -r opt_group
            case "$opt_group" in
                [Nn]*) printf_step "2,${WARN} Skipping group addition. You will need 'sudo' for CLI commands." ;;
                *)
                    usermod -aG x3d-toggle "$ACTUAL_USER"
                    printf_step "2,${ALRIGHT} User added to group. Logout/Login still required for the session."
                    ;;
            esac
            ;;
    esac
fi

printf_step "2,${INFO} Enabling daemon allows autonomous workload detection."
printf_step_no_nl "2,${QUERY} Enable and start x3d-toggle.service now? [Y/n] "
read -r opt_daemon
case "$opt_daemon" in
    [Nn]*)
        printf_step "2,${ALRIGHT} Skipping daemon activation. You can run 'x3d auto' manually."
        set_config "DAEMON_STATE" "manual"
        ;;
    *)
        set_config "DAEMON_STATE" "default"
        systemctl enable --now x3d-toggle.service
        sleep 2
        if systemctl is-active --quiet x3d-toggle.service; then
            x3d-toggle default
            printf_step "2,${ALRIGHT} Daemon activated successfully."
        else
            printf_step "2,${XOUT} Warning: Daemon failed to start. Check 'journalctl -u x3d-toggle'."
            set_config "DAEMON_STATE" "manual"
        fi
        ;;
esac

printf_divider
printf_center "${GEAR} Setup Complete! ${GEAR}"
printf_center "You can change these settings later via the UI"
printf_center "or by editing $CONF_FILE."
if [ "$GUI_INSTALLED" -eq 1 ]; then
    printf_br
    printf_center "${GEAR} You can now launch the GUI by running 'x3d-gui' or"
    printf_center "from your application menu."
fi
printf_signature

## end of SETUP.SH