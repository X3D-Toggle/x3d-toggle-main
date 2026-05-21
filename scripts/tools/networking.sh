#!/bin/bash
# Networking Guard & Apply Script for X3D Toggle
# /scripts/tools/networking.sh

set -e

# Guard 1: Root privileges
if [ "$EUID" -ne 0 ]; then
    echo "Error: Networking configuration requires root privileges."
    exit 1
fi

NET_ACTIVE="/etc/sysctl.d/99-x3d-networking.conf"
NET_TEMPLATE="/usr/lib/x3d-toggle/config/networking.conf"

# Guard 2: Idempotency / Restore
_needs_restore=0
if [ ! -f "$NET_ACTIVE" ]; then
    _needs_restore=1
else
    while read -r line || [ -n "$line" ]; do
        case "$line" in
            *\[Server\]*)
                _needs_restore=1
                break
                ;;
        esac
    done < "$NET_ACTIVE"
fi

if [ "$_needs_restore" -eq 1 ]; then
    echo "Status: Active networking file missing or outdated. Restoring from template..."
    if [ -f "$NET_TEMPLATE" ]; then
        rm -f "$NET_ACTIVE"
        while read -r line || [ -n "$line" ]; do
            case "$line" in
                \[*] | ENABLED=* | SERVER_ADDRESS=* | SERVER_PORT=* | SERVER_SSH=*)
                    continue
                    ;;
                *)
                    echo "$line" >> "$NET_ACTIVE"
                    ;;
            esac
        done < "$NET_TEMPLATE"
        chmod 0644 "$NET_ACTIVE"
    else
        echo "Error: Networking template not found at $NET_TEMPLATE"
        exit 1
    fi
fi

# Apply Action
echo "Status: Applying X3D Networking tunables via sysctl..."
sysctl --system

echo "Status: Networking optimization successfully applied."
exit 0
