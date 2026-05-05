#!/bin/sh
## GPU and CPU IRQ Discovery logic for IRQ Affinity binding
## `irq.sh`
## Executed after `config.sh` and `ccd.sh` as part of `framework.sh`
## Dynamically appends udev rules for IRQ binding during the discovery stage.

_l_dir_lib="$(cd "$(dirname "$0")" && pwd)"
. "$_l_dir_lib/framework.sh"

irq_load_config() {
    if [ -f "/etc/x3d-toggle.d/settings.conf" ]; then
        . "/etc/x3d-toggle.d/settings.conf"
    elif [ -f "$X3D_TOGGLE/config/settings.conf" ]; then
        . "$X3D_TOGGLE/config/settings.conf"
    fi
    
    if [ -f "/etc/x3d-toggle.d/irq.conf" ]; then
        . "/etc/x3d-toggle.d/irq.conf"
    elif [ -f "$X3D_TOGGLE/config/irq.conf" ]; then
        . "$X3D_TOGGLE/config/irq.conf"
    fi
}

irq_resolve_core() {
    case "$IRQ_PINNING" in
        0) echo "0" ;;
        1)
            _discovered=$(lscpu -p=CPU,CORE 2>/dev/null | grep -v "^#" | awk -F, '$1==$2 {print $1; exit}')
            [ -z "$_discovered" ] && _discovered="0"
            echo "$_discovered" > /run/x3d-toggle/irq_target_core
            echo "$_discovered"
            ;;
        2) echo "${IRQ_CUSTOM:-0}" ;;
        3) return 1 ;;
        *) echo "0" ;;
    esac
}

irq_disable_balancer() {
    if systemctl is-active --quiet irqbalance 2>/dev/null; then
        touch /run/x3d-toggle/irqbalance_was_active
        systemctl stop irqbalance 2>/dev/null
    fi
}

irq_restore_balancer() {
    if [ -f /run/x3d-toggle/irqbalance_was_active ]; then
        systemctl start irqbalance 2>/dev/null
        rm -f /run/x3d-toggle/irqbalance_was_active
    fi
}

irq_discover_gpu() {
    [ "$IRQ_GPU" = "1" ] || return 0
    IRQ_GPU_LIST=""
    while IFS=: read -r irq driver_info; do
        if echo "$driver_info" | grep -qE "amdgpu|nouveau"; then
            irq_num=$(echo "$irq" | tr -d ' ')
            if grep -qE "PCI-MSI|PCI-MSIX" "/proc/interrupts" | grep -q "^[ ]*${irq_num}:"; then
                IRQ_GPU_LIST="$IRQ_GPU_LIST $irq_num"
            else
                journal_write 4 "WARN" "Device is not using MSI; legacy APIC pin detected on IRQ $irq_num"
            fi
        fi
    done < /proc/interrupts
}

irq_discover_peripherals() {
    IRQ_NVME_LIST=""
    IRQ_USB_LIST=""
    IRQ_NIC_LIST=""
    IRQ_AUDIO_LIST=""

    while IFS=: read -r irq driver_info; do
        irq_num=$(echo "$irq" | tr -d ' ')
        [ "$IRQ_NVME" = "1" ] && echo "$driver_info" | grep -q "nvme" && IRQ_NVME_LIST="$IRQ_NVME_LIST $irq_num"
        [ "$IRQ_USB" = "1" ] && echo "$driver_info" | grep -q "xhci_hcd" && IRQ_USB_LIST="$IRQ_USB_LIST $irq_num"
        [ "$IRQ_NIC" = "1" ] && echo "$driver_info" | grep -qE "eth|enp|eno|wlan" && IRQ_NIC_LIST="$IRQ_NIC_LIST $irq_num"
        [ "$IRQ_AUDIO" = "1" ] && echo "$driver_info" | grep -qE "snd_hda|snd-hda" && IRQ_AUDIO_LIST="$IRQ_AUDIO_LIST $irq_num"
    done < /proc/interrupts
}

irq_apply_affinity() {
    _core="$1"
    for irq in $IRQ_GPU_LIST $IRQ_NVME_LIST $IRQ_USB_LIST $IRQ_NIC_LIST $IRQ_AUDIO_LIST; do
        if [ -f "/proc/irq/$irq/smp_affinity_list" ]; then
            cat "/proc/irq/$irq/smp_affinity_list" > "/run/x3d-toggle/irq_orig_$irq" 2>/dev/null
            echo "$_core" > "/proc/irq/$irq/smp_affinity_list" 2>/dev/null
        fi
    done
}

irq_gen_udev() {
    _udev_file="/etc/udev/rules.d/99-x3d-irq.rules"
    mkdir -p /etc/udev/rules.d
    cat << EOF > "$_udev_file"
# X3D Toggle IRQ Affinity Rules (auto-generated)
# Re-asserts IRQ affinity on device add/reset events

$([ "$IRQ_GPU" = "1" ] && echo 'ACTION=="add", SUBSYSTEM=="pci", DRIVER=="amdgpu|nouveau", RUN+="/usr/bin/x3d-toggle irq-bind"')
$([ "$IRQ_NVME" = "1" ] && echo 'ACTION=="add", SUBSYSTEM=="pci", DRIVER=="nvme", RUN+="/usr/bin/x3d-toggle irq-bind"')
$([ "$IRQ_USB" = "1" ] && echo 'ACTION=="add", SUBSYSTEM=="pci", DRIVER=="xhci_hcd", RUN+="/usr/bin/x3d-toggle irq-bind"')
EOF
    
    if command -v udevadm >/dev/null 2>&1; then
        udevadm control --reload-rules 2>/dev/null
    fi
}

irq_generate() {
    irq_load_config
    
    [ "$IRQ_ENABLE" = "1" ] || { journal_trace "IRQ_ENABLE=0, skipping"; return 0; }
    [ "$IRQ_PINNING" = "3" ] && { journal_trace "IRQ_PINNING=3, irqbalance active"; return 0; }
    
    mkdir -p /run/x3d-toggle
    _core=$(irq_resolve_core)
    [ -z "$_core" ] && return 0
    
    irq_disable_balancer
    irq_discover_gpu
    irq_discover_peripherals
    irq_apply_affinity "$_core"
    irq_gen_udev
    
    [ "$IRQ_WATCH" = "1" ] && /usr/bin/x3d-toggle irq-watch &
    
    touch "$X3D_BUILD/irq_rules.stamp"
    printf_step "3,${GEAR} IRQ Affinity: Rules generated for target core $_core"
}

[ "$X3D_EXEC" = "1" ] && [ "$1" = "--gen-irq" ] && irq_generate

## end of IRQ.SH