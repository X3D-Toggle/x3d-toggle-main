#!/bin/sh
## Generates the necessary eBPF files for the X3D Toggle Project.
## `ebpftool.sh`
## Modular eBPF artifact generator for the X3D Toggle Project.
## Synchronized with the framework and error registry.

_l_dir_lib="$(cd "$(dirname "$0")" && pwd)"
. "$_l_dir_lib/framework.sh"

if [ "$X3D_EXEC" != "1" ]; then
    journal_write -37
fi

VERIFY=0
GEN_EBPF=0
for arg in "$@"; do
    case "$arg" in
        --verify|-verify) VERIFY=1; shift ;;
        --gen-ebpf) GEN_EBPF=1; shift ;;
    esac
done

if [ "$GEN_EBPF" -eq 1 ]; then
    printf_step "3,Exporting kernel BTF: build/vmlinux.h"
    bpftool btf dump file /sys/kernel/btf/vmlinux format c > "$X3D_BUILD/vmlinux.h" 2>&1 || journal_write -32 "$X3D_BUILD/vmlinux.h"

    printf_step "3,Compiling eBPF module: src/daemon/bpf/bpf.c -> build/bpf.o"
    clang -g -O2 -target bpf -D__TARGET_ARCH_x86 -I"$X3D_BUILD" \
            -Wno-compare-distinct-pointer-types -c "$X3D_TOGGLE/src/daemon/bpf/bpf.c" -o "$X3D_BUILD/bpf.o" 2>&1 || journal_write -27 "$X3D_BUILD/bpf.o"

    printf_step "3,Exporting program BTF: build/bpf.h"
    bpftool btf dump file "$X3D_BUILD/bpf.o" format c > "$X3D_BUILD/bpf.h" 2>&1 || journal_write -27 "$X3D_BUILD/bpf.h"

    if [ ! -f "$X3D_BUILD/bpf.o" ]; then
        journal_write -27 "$X3D_BUILD/bpf.o"
    fi


    if [ "$VERIFY" -eq 1 ] && [ "$(id -u)" -eq 0 ]; then
        printf_step "3,Testing eBPF verifier pass..."
        if ! bpftool prog load "$X3D_BUILD/bpf.o" /sys/fs/bpf/x3d_verify autoattach 2>&1; then
             printf_step "3,Verifier Log captured. Analysis required."
             rm -f /sys/fs/bpf/x3d_verify
        fi
    fi

    printf_step "3,${ALRIGHT} eBPF Object compiled (synced with kernel)."
fi

## end of EBPFTOOL.SH
