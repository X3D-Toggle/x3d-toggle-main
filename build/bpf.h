#ifndef __VMLINUX_H__
#define __VMLINUX_H__

#ifndef BPF_NO_PRESERVE_ACCESS_INDEX
#pragma clang attribute push (__attribute__((preserve_access_index)), apply_to = record)
#endif

#ifndef __ksym
#define __ksym __attribute__((section(".ksyms")))
#endif

#ifndef __weak
#define __weak __attribute__((weak))
#endif

#ifndef __bpf_fastcall
#if __has_attribute(bpf_fastcall)
#define __bpf_fastcall __attribute__((bpf_fastcall))
#else
#define __bpf_fastcall
#endif
#endif

typedef int __kernel_pid_t;

typedef __kernel_pid_t pid_t;

typedef unsigned int __u32;

typedef __u32 u32;

struct trace_entry {
	unsigned short type;
	unsigned char flags;
	unsigned char preempt_count;
	int pid;
};

struct trace_event_raw_sched_process_exec {
	struct trace_entry ent;
	u32 __data_loc_filename;
	pid_t pid;
	pid_t old_pid;
	char __data[0];
};

struct trace_event_raw_sched_process_template {
	struct trace_entry ent;
	u32 __data_loc_comm;
	pid_t pid;
	int prio;
	char __data[0];
};


/* BPF kfuncs */
#ifndef BPF_NO_KFUNC_PROTOTYPES
#endif

#ifndef BPF_NO_PRESERVE_ACCESS_INDEX
#pragma clang attribute pop
#endif

#endif /* __VMLINUX_H__ */
