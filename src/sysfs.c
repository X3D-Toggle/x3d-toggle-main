/*
 * x3d-sysfs.c
 * Standalone debug tool for sysfs node detection logic.
 * Prints globbing results for both vcache and AMDI* patterns.
 * Usage: build and run as x3d-toggle user to verify sysfs node visibility.
 */

#include "libc.h"
#include <glob.h>
#include "xui.h"

#define SYSFS_PATTERN1 "/sys/bus/platform/drivers/amd_x3d_vcache/*/amd_x3d_mode"
#define SYSFS_PATTERN2 "/sys/devices/platform/AMDI*/amd_x3d_mode"

void debug_sysfs(const char *pattern) {
    glob_t glob_result;
    int ret;
    char buf[256];
    printf_sn(buf, sizeof(buf), "Globbing pattern: %s\n", pattern);
    write(1, buf, strlen(buf));
    
    ret = glob(pattern, 0, NULL, &glob_result);
    printf_sn(buf, sizeof(buf), "glob() ret=%d, count=%zu\n", ret, glob_result.gl_pathc);
    write(1, buf, strlen(buf));
    
    for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
        printf_sn(buf, sizeof(buf), "  found: %s\n", glob_result.gl_pathv[i]);
        write(1, buf, strlen(buf));
    }
    globfree(&glob_result);
}

int main(void) {
    debug_sysfs(SYSFS_PATTERN1);
    debug_sysfs(SYSFS_PATTERN2);
    return 0;
}
