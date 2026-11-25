/* Universal Affinity Protocols for the X3D Toggle Project
 *
 * `affinity.c`
 *
 * Implements high-level process isolation protocols using Cgroups V2.
 * Handles migration between ephemeral core-sets for CCD alignment.
 */

#include "ccd.h"
#include "error.h"
#include "libc.h"
#include "xui.h"

#ifndef CGROUP_ROOT
#define CGROUP_ROOT "/sys/fs/cgroup/x3d-toggle"
#endif

static int cgroup_write_raw(const char *rel_path, const char *val) {
  char path[512];
  printf_sn(path, sizeof(path), "%s/%s", CGROUP_ROOT, rel_path);
  int fd = open(path, O_WRONLY);
  if (fd < 0)
    return ERR_CGROUP;
  ssize_t n = write(fd, val, strlen(val));
  close(fd);
  return (n > 0) ? ERR_SUCCESS : ERR_CGROUP;
}

int affinity_get_topology(char *cache, char *freq, size_t size) {
  int ccd1_start = 0;
  int total_cores = 0;
  if (ccd(cache, &ccd1_start, &total_cores) != ERR_SUCCESS)
    return -1;
  if (ccd_freq_mask(freq, size) != ERR_SUCCESS)
    return -1;
  return 0;
}
int affinity_init(void) {
  /* 1. Partitions are created by systemd ExecStartPre */
  if (mkdir(CGROUP_ROOT "/cache", 0755) < 0 && errno != 17) {
    return ERR_CGROUP;
  }
  if (mkdir(CGROUP_ROOT "/frequency", 0755) < 0 && errno != 17) {
    return ERR_CGROUP;
  }

  /* 2. Assign Default Cores using L3/SMT-aware Framework Discovery */
  char cache_mask[128] = {0};
  char freq_mask[128] = {0};
  int ccd1_start = 0;
  int total_cores = 0;

  if (ccd(cache_mask, &ccd1_start, &total_cores) == ERR_SUCCESS &&
      ccd_freq_mask(freq_mask, sizeof(freq_mask)) == ERR_SUCCESS) {
    cgroup_write_raw("cache/cpuset.cpus", cache_mask);
    cgroup_write_raw("frequency/cpuset.cpus", freq_mask);
  }

  cgroup_write_raw("cache/cpuset.mems", "0");
  cgroup_write_raw("frequency/cpuset.mems", "0");

  return ERR_SUCCESS;
}

static int cgroup_write_pid(const char *partition, pid_t pid) {
  char path[256];
  printf_sn(path, sizeof(path), "%s/%s/cgroup.procs", CGROUP_ROOT, partition);

  int fd = open(path, O_WRONLY);
  if (fd < 0)
    return ERR_CGROUP;

  char pid_str[16];
  int len = printf_sn(pid_str, sizeof(pid_str), "%d", pid);

  ssize_t n = write(fd, pid_str, len);
  close(fd);

  return (n > 0) ? ERR_SUCCESS : ERR_CGROUP;
}

int affinity_default(pid_t pid) { return cgroup_write_pid("", pid); }

int affinity_auto(pid_t pid) {
  const char path[] = "/sys/fs/cgroup/cgroup.procs";
  int fd = open(path, O_WRONLY);
  if (fd < 0)
    return ERR_CGROUP;

  char pid_str[16];
  int len = printf_sn(pid_str, sizeof(pid_str), "%d", pid);
  ssize_t n = write(fd, pid_str, len);
  close(fd);

  return (n > 0) ? ERR_SUCCESS : ERR_CGROUP;
}

int affinity_dual(pid_t pid) { return affinity_default(pid); }

int affinity_swap(pid_t pid) { return cgroup_write_pid("frequency", pid); }

int affinity_partition(pid_t pid, int partition) {
  switch (partition) {
  case 1:
    return cgroup_write_pid("cache", pid);
  case 2:
    return cgroup_write_pid("frequency", pid);
  default:
    return affinity_default(pid);
  }
}

int affinity_set_masks(const char *cache, const char *freq) {
  if (strcmp(cache, "none") == 0 || strcmp(freq, "none") == 0) {
    char cache_mask[128] = {0};
    char freq_mask[128] = {0};
    int ccd1_start = 0, total_cores = 0;
    if (ccd(cache_mask, &ccd1_start, &total_cores) == ERR_SUCCESS &&
        ccd_freq_mask(freq_mask, sizeof(freq_mask)) == ERR_SUCCESS) {
      int r1 = cgroup_write_raw("cache/cpuset.cpus", cache_mask);
      int r2 = cgroup_write_raw("frequency/cpuset.cpus", freq_mask);
      return (r1 == ERR_SUCCESS && r2 == ERR_SUCCESS) ? ERR_SUCCESS
                                                      : ERR_CGROUP;
    }
    return ERR_CGROUP;
  }
  int r1 = cgroup_write_raw("cache/cpuset.cpus", cache);
  int r2 = cgroup_write_raw("frequency/cpuset.cpus", freq);
  return (r1 == ERR_SUCCESS && r2 == ERR_SUCCESS) ? ERR_SUCCESS : ERR_CGROUP;
}

/* end of AFFINITY.C */
