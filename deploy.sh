#!/bin/sh
## `deploy.sh`
## Automated Full-Suite Deployment Pipeline for X3D Toggle.
## Executes a clean teardown, full backend + eBPF + GTK4 GUI build, FHS deployment,
## system configuration, group setup, desktop integration, and service activation
## automatically without interactive prompts.
##
## Usage:
##   sudo ./deploy.sh               # Full automated install (Everything + Dev)
##   sudo ./deploy.sh --standard    # Automated install without Dev/Debug flags
##   sudo ./deploy.sh --minimal     # Minimal backend install only
##   sudo ./deploy.sh --interactive # Run interactive setup wizard at Phase 5
##   sudo make deploy

set -e

DEPLOY_MODE="${X3D_SETUP_MODE:-full}"
INTERACTIVE=0
INSTALL_KDIALOG="auto"

for _arg in "$@"; do
    case "$_arg" in
        --full)        DEPLOY_MODE="full" ;;
        --standard)    DEPLOY_MODE="standard" ;;
        --minimal)     DEPLOY_MODE="minimal" ;;
        --dev)         DEPLOY_MODE="dev" ;;
        --kdialog)     INSTALL_KDIALOG="yes" ;;
        --no-kdialog)  INSTALL_KDIALOG="no" ;;
        --interactive) INTERACTIVE=1 ;;
        -h|--help)
            echo "Usage: sudo ./deploy.sh [--full|--standard|--minimal|--dev|--kdialog|--no-kdialog|--interactive]"
            exit 0
            ;;
        *)
            echo "  ❌ Error: Unknown option '$_arg'. Run './deploy.sh --help' for usage."
            exit 1
            ;;
    esac
done

_l_dir_root="$(cd "$(dirname "$0")" && pwd)"

if [ "$(id -u)" -ne 0 ]; then
    if [ -t 0 ] && command -v sudo >/dev/null 2>&1; then
        echo "  🔐 Elevating privileges via sudo for automated deployment..."
        exec sudo X3D_SETUP_MODE="$DEPLOY_MODE" sh "$0" "$@"
    fi
    echo "  ❌ Error: deploy.sh must be run with sudo/root privileges."
    echo "     Try: sudo ./deploy.sh  (or: sudo make deploy)"
    exit 1
fi

export X3D_EXEC=1
cd "$_l_dir_root"

if [ -f "$_l_dir_root/scripts/framework/framework.sh" ]; then
    . "$_l_dir_root/scripts/framework/framework.sh"
else
    echo "  ❌ Error: Could not locate scripts/framework/framework.sh ❌"
    exit 1
fi

# Resolve non-root workspace user so build artifacts remain user-owned
ACTUAL_USER="${SUDO_USER:-}"
if [ -z "$ACTUAL_USER" ] || [ "$ACTUAL_USER" = "root" ]; then
    _log_user="$(logname 2>/dev/null || true)"
    if [ -n "$_log_user" ] && [ "$_log_user" != "root" ] && id "$_log_user" >/dev/null 2>&1; then
        ACTUAL_USER="$_log_user"
    fi
fi
if [ -z "$ACTUAL_USER" ] || [ "$ACTUAL_USER" = "root" ]; then
    ACTUAL_USER="$(awk -F: '($3 >= 1000) && ($1 != "nobody") && ($7 !~ /(nologin|false)$/) { print $1; exit }' /etc/passwd)"
fi
ACTUAL_GROUP=""
if [ -n "$ACTUAL_USER" ] && id "$ACTUAL_USER" >/dev/null 2>&1; then
    ACTUAL_GROUP="$(id -gn "$ACTUAL_USER")"
fi

_deploy_log="$(mktemp "${TMPDIR:-/tmp}/x3d-deploy.XXXXXX")"
cleanup_deploy() {
    rm -f "$_deploy_log"
    if [ -n "$ACTUAL_USER" ] && [ -n "$ACTUAL_GROUP" ]; then
        chown -R "$ACTUAL_USER:$ACTUAL_GROUP" "$_l_dir_root/bin" "$_l_dir_root/build" 2>/dev/null || true
    fi
}
trap cleanup_deploy EXIT INT TERM HUP

run_logged() {
    _phase_msg="$1"
    shift
    printf_step "$_phase_msg"
    if ! "$@" </dev/null >"$_deploy_log" 2>&1; then
        printf_br
        printf_step "❌ Failure during: $_phase_msg"
        printf_divider
        cat "$_deploy_log" >&2
        printf_divider
        exit 1
    fi
}

printf_br
printf_center "🛠️ Starting Automated X3D Toggle Deployment ($DEPLOY_MODE)..."
printf_divider

# Phase 1: Clean teardown of previous installation & stale artifacts
run_logged "🧹 Phase 1: Cleansing previous installation (make purge)..." make purge

# Phase 2: Compile CLI, Daemon, Wrapper, eBPF CO-RE object, and synced config
run_logged "🏗️  Phase 2: Compiling core binaries and eBPF module (make all)..." make all

# Phase 3: Deploy binaries, configs, systemd, udev, polkit, icons, and manpage
run_logged "📦 Phase 3: Deploying FHS system files and policies (make install)..." make install DESTDIR=""

# Phase 4: Automatically build and install GTK4 Desktop Dashboard (if GTK4 headers exist)
X3D_GUI_BUILT=0
if [ "$DEPLOY_MODE" = "full" ] || [ "$DEPLOY_MODE" = "standard" ] || [ "$INTERACTIVE" -eq 1 ]; then
    if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists gtk4 libadwaita-1 gio-unix-2.0; then
        run_logged "🖥️  Phase 4: Compiling and installing GTK4 Desktop Dashboard..." \
            env X3D_EXEC=1 SUDO_USER="$ACTUAL_USER" sh "$_l_dir_root/scripts/framework/gui.sh"
        X3D_GUI_BUILT=1
    else
        printf_step "⚠️  Phase 4: GTK4/libadwaita headers not found — skipping optional GUI build."
    fi
else
    printf_step "⏭️  Phase 4: Skipping GTK4 GUI build for '$DEPLOY_MODE' profile."
fi

# Phase 4b: Optional KDialog Lightweight Frontend
X3D_KDIALOG_BUILT=0
X3D_KDIALOG_PROMPTED=0
if [ "$INSTALL_KDIALOG" = "no" ]; then
    printf_step "⏭️  Phase 4b: Skipping KDialog GUI (--no-kdialog)."
    X3D_KDIALOG_PROMPTED=1
elif [ "$INSTALL_KDIALOG" = "yes" ]; then
    run_logged "🖥️  Phase 4b: Installing KDialog Lightweight GUI..." \
        env X3D_EXEC=1 SUDO_USER="$ACTUAL_USER" sh "$_l_dir_root/scripts/framework/kdialog.sh"
    X3D_KDIALOG_BUILT=1
    X3D_KDIALOG_PROMPTED=1
elif [ "$INTERACTIVE" -eq 0 ] && { [ "$DEPLOY_MODE" = "full" ] || [ "$DEPLOY_MODE" = "standard" ]; }; then
    if command -v kdialog >/dev/null 2>&1; then
        run_logged "🖥️  Phase 4b: Installing KDialog Lightweight GUI..." \
            env X3D_EXEC=1 SUDO_USER="$ACTUAL_USER" sh "$_l_dir_root/scripts/framework/kdialog.sh"
        X3D_KDIALOG_BUILT=1
    else
        printf_step "⚠️  Phase 4b: kdialog not found — skipping optional KDialog GUI."
    fi
    X3D_KDIALOG_PROMPTED=1
fi

# Refresh system desktop and icon caches if utilities exist
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications 2>/dev/null || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1 && [ -d /usr/share/icons/hicolor ]; then
    gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor 2>/dev/null || true
fi

# Phase 5: Configure settings.conf + daemon.conf, user groups, desktop icon, and service
printf_step "⚙️  Phase 5: Configuring system settings, permissions, and service..."
if [ -f "$_l_dir_root/setup.sh" ]; then
    if [ "$INTERACTIVE" -eq 1 ]; then
        X3D_GUI_BUILT="$X3D_GUI_BUILT" X3D_KDIALOG_BUILT="$X3D_KDIALOG_BUILT" X3D_KDIALOG_PROMPTED="$X3D_KDIALOG_PROMPTED" SUDO_USER="$ACTUAL_USER" sh "$_l_dir_root/setup.sh"
    else
        X3D_SETUP_MODE="$DEPLOY_MODE" X3D_GUI_BUILT="$X3D_GUI_BUILT" X3D_KDIALOG_BUILT="$X3D_KDIALOG_BUILT" X3D_KDIALOG_PROMPTED="$X3D_KDIALOG_PROMPTED" SUDO_USER="$ACTUAL_USER" \
            sh "$_l_dir_root/setup.sh" </dev/null
    fi
else
    printf_step "⚠️  Notice: setup.sh not found in project root; skipping post-install config."
fi

# Restore workspace ownership so local dev builds never fail with Permission denied
if [ -n "$ACTUAL_USER" ] && [ -n "$ACTUAL_GROUP" ]; then
    chown -R "$ACTUAL_USER:$ACTUAL_GROUP" "$_l_dir_root/bin" "$_l_dir_root/build" 2>/dev/null || true
fi

# Phase 6: Final Verification Summary
printf_br
printf_divider
printf_center "✨ Automated X3D Toggle Deployment Complete ✨"
printf_divider
[ -x /usr/bin/x3d-toggle ] && printf_step "2,✔️ CLI & Wrapper:    /usr/bin/x3d-toggle, /usr/bin/x3d-run, /usr/bin/x3d"
[ -x /usr/bin/x3d-daemon ] && printf_step "2,✔️ Daemon & eBPF:    /usr/bin/x3d-daemon, /usr/lib/x3d-toggle/bpf.o"
if [ -x /usr/bin/x3d-gui ]; then
    printf_step "2,✔️ Desktop GUI:      /usr/bin/x3d-gui (launch via 'x3d-toggle' or app menu)"
fi
if [ -x /usr/bin/x3d-toggle-kdialog ]; then
    printf_step "2,✔️ KDialog GUI:      /usr/bin/x3d-toggle-kdialog (launch via 'x3d-toggle kdialog' or app menu)"
fi
if systemctl is-active --quiet x3d-toggle.service 2>/dev/null; then
    printf_step "2,✔️ Systemd Service:  x3d-toggle.service (ACTIVE & ENABLED)"
else
    printf_step "2,⚠️  Systemd Service:  x3d-toggle.service (INACTIVE)"
fi
printf_br
