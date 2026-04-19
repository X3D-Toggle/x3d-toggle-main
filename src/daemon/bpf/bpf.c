/* eBPF Game Detection Logic for the x3D Toggle Project
 *
 * `bpf.c`
 *
 * This is the Restricted C code that runs in the kernel context.
 * It monitors process execution and cross-references against a 
 * BPF hash map populated by the userspace daemon.
 */

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-declarations"
#include "vmlinux.h"
#pragma clang diagnostic pop

#define bpf_stream_vprintk _bpf_stream_vprintk
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#undef bpf_stream_vprintk

char LICENSE[] SEC("license") = "GPL";

struct process_event {
    int pid;
    char comm[16];
    unsigned char is_exit;
};

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 256);
    __type(key, char[16]);
    __type(value, __u32);
} game_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);
} events SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u32);
} state_map SEC(".maps");

SEC("tp/sched/sched_process_exec")
int handle_exec(struct trace_event_raw_sched_process_exec *ctx) {
    char comm[16];
    __u32 key = 0;
    __u32 active = 1;
    __u32 *val;

    bpf_get_current_comm(&comm, sizeof(comm));

    val = bpf_map_lookup_elem(&game_map, &comm);
    if (val) {
        bpf_map_update_elem(&state_map, &key, &active, BPF_ANY);

        struct process_event *e;
        e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
        if (e) {
            e->pid = bpf_get_current_pid_tgid() >> 32;
            bpf_get_current_comm(&e->comm, sizeof(e->comm));
            e->is_exit = 0;
            bpf_ringbuf_submit(e, 0);
        }
    }

    return 0;
}

SEC("tp/sched/sched_process_exit")
int handle_exit(struct trace_event_raw_sched_process_template *ctx) {
    char comm[16];
    __u32 *val;

    bpf_get_current_comm(&comm, sizeof(comm));

    val = bpf_map_lookup_elem(&game_map, &comm);
    if (val) {
        struct process_event *e;
        e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
        if (e) {
            e->pid = bpf_get_current_pid_tgid() >> 32;
            bpf_get_current_comm(&e->comm, sizeof(e->comm));
            e->is_exit = 1;
            bpf_ringbuf_submit(e, 0);
        }
    }
    
    return 0;
}

/*/ end of BPF.C /*/
