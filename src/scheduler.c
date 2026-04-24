#include "scheduler.h"
#include "error.h"
#include "libc.h"
#include "../build/xui.h"

#define PROC_KERNEL "/proc/sys/kernel/"

static int sysctl_write(const char *node, const char *value) {
    char path[128];
    strcpy(path, PROC_KERNEL);
    strcat(path, node);
    
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    
    write(fd, value, strlen(value));
    close(fd);
    return 0;
}

bool scheduler_check(void) {
    return access(PROC_KERNEL "sched_bore", F_OK) == 0;
}

int scheduler_set(sched_t mode) {
    if (geteuid() != 0) return -1;

    if (mode == SCHED_GAMING) {
        sysctl_write("sched_cfs_bandwidth_slice_us", "3000");
        
        if (scheduler_check()) {
            sysctl_write("sched_bore", "1");
            sysctl_write("sched_bit_shift", "14"); // Aggressive burst identification
        }
    } else {
        sysctl_write("sched_cfs_bandwidth_slice_us", "5000");
        
        if (scheduler_check()) {
            sysctl_write("sched_bit_shift", "12"); // Default BORE shift
        }
    }
    
    return 0;
}

int cli_scheduler_gaming(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    int res = scheduler_set(SCHED_GAMING);
    if (res == 0) {
        printf_step("${ALRIGHT} Scheduler: ${COLOR_CYAN}GAMING${COLOR_RESET} profile applied.");
    } else {
        printf_step("${WARN} Scheduler: Failed to apply GAMING profile.");
    }
    return res;
}

int cli_scheduler_balanced(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    int res = scheduler_set(SCHED_BALANCED);
    if (res == 0) {
        printf_step("${ALRIGHT} Scheduler: ${COLOR_CYAN}BALANCED${COLOR_RESET} profile applied.");
    } else {
        printf_step("${WARN} Scheduler: Failed to apply BALANCED profile.");
    }
    return res;
}