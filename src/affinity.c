/* Universal Affinity Protocols for the X3D Toggle Project
 *
 * `affinity.c`
 *
 * Implements high-level process isolation protocols using Cgroups V2.
 * Handles migration between ephemeral core-sets for CCD alignment.
 */

#include "libc.h"
#include "error.h"
#include "xui.h"

#ifndef CGROUP_ROOT
#define CGROUP_ROOT "/sys/fs/cgroup/x3d-toggle"
#endif

#ifndef CCD1_START
#define CCD1_START 8
#endif

static int cgroup_write_raw(const char *rel_path, const char *val) {
    char path[512];
    printf_sn(path, sizeof(path), "%s/%s", CGROUP_ROOT, rel_path);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return ERR_CGROUP;
    ssize_t n = write(fd, val, strlen(val));
    close(fd);
    return (n > 0) ? ERR_SUCCESS : ERR_CGROUP;
}

int affinity_init(void) {
    /* 1. Create root cgroup */
    if (mkdir(CGROUP_ROOT, 0755) < 0 && errno != 17 /* EEXIST */) {
        return ERR_CGROUP;
    }

    /* 2. Enable cgroup controllers (Assume V2) */
    cgroup_write_raw("../cgroup.subtree_control", "+cpuset");
    cgroup_write_raw("cgroup.subtree_control", "+cpuset");

    /* 3. Create partitions */
    mkdir(CGROUP_ROOT "/cache", 0755);
    mkdir(CGROUP_ROOT "/frequency", 0755);

    /* 4. Assign Cores (Hard-coded defaults for Zen 4/5 16-thread for now) */
    // Note: In production, these should be dynamic based on topology discovery.
    // CCD0: 0-7, CCD1: 8-15
    cgroup_write_raw("cache/cpuset.cpus", "0-7");
    cgroup_write_raw("frequency/cpuset.cpus", "8-15");
    
    /* Set memory nodes (Single node 0 for now) */
    cgroup_write_raw("cache/cpuset.mems", "0");
    cgroup_write_raw("frequency/cpuset.mems", "0");

    return ERR_SUCCESS;
}

static int cgroup_write_pid(const char *partition, pid_t pid) {
    char path[256];
    printf_sn(path, sizeof(path), "%s/%s/cgroup.procs", CGROUP_ROOT, partition);
    
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        return ERR_CGROUP;
    }

    char pid_str[16];
    int len = printf_sn(pid_str, sizeof(pid_str), "%d", pid);
    
    ssize_t n = write(fd, pid_str, len);
    close(fd);

    return (n > 0) ? ERR_SUCCESS : ERR_CGROUP;
}

int affinity_default(pid_t pid) {
    /* Migrate back to root of the x3d-toggle tree (All cores) */
    return cgroup_write_pid("", pid);
}

int affinity_auto(pid_t pid) {
    /* Release from x3d-toggle cgroup management entirely by moving to system root */
    char path[] = "/sys/fs/cgroup/cgroup.procs";
    int fd = open(path, O_WRONLY);
    if (fd < 0) return ERR_CGROUP;

    char pid_str[16];
    int len = printf_sn(pid_str, sizeof(pid_str), "%d", pid);
    ssize_t n = write(fd, pid_str, len);
    close(fd);
    
    return (n > 0) ? ERR_SUCCESS : ERR_CGROUP;
}

int affinity_dual(pid_t pid) {
    /* Same as default within our tree */
    return affinity_default(pid);
}

int affinity_swap(pid_t pid) {
    /* Logic to determine current partition and flip would go here.
     * For now, we utilize a semantic migration based on global daemon state 
     * or explicit request. Simplified for first iteration. */
    return cgroup_write_pid("frequency", pid); 
}

/* Internal implementation of the 'affinity_set' protocol used by the daemon 
 * to enforce rules from affinity.conf */
int affinity_apply_partition(pid_t pid, int partition) {
    switch (partition) {
        case 1: return cgroup_write_pid("cache", pid);
        case 2: return cgroup_write_pid("frequency", pid);
        default: return affinity_default(pid);
    }
}

/* end of AFFINITY.C */
