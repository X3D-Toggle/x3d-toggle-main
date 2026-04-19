/* CPU Stress Test Engine for the X3D Toggle Project
 *
 * `stress.c`
 *
 * Generates mock throughput load for CCD validation.
 * Utilizes project-native protocols and fork() for zero dependencies.
 */

#include "libc.h"
#include "xui.h"
#include "error.h"

static volatile int active = 1;

static void handle_sigint(int sig) {
    (void)sig;
    active = 0;
}

static void load_payload(void) {
    volatile double ops = 1.0;
    while (active) {
        ops *= 1.000001;
        if (ops > 1000000.0) ops = 1.0;
    }
    _exit(0);
}

int cli_stress_cpu(int argc, char *argv[]) {
    int interval = 0;
    
    /* Simple manual argument parser */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0 && (i + 1) < argc) {
            interval = atoi(argv[++i]);
        }
    }

    printf_step("${GEAR} Initializing Compute Load Engine...");
    
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    int total_cores = 16; // Default fallback for Zen 4/5
    // In a future update, we can parse /proc/stat here.

    if (interval > 0) {
        printf_step("${RELOAD} Mode: targeted CCD Migration (Interval: %ds)", interval);
        
        pid_t worker = fork();
        if (worker == 0) {
            load_payload();
        } else if (worker < 0) {
            return ERR_IO;
        }

        while (active) {
            printf_step("${ALRIGHT} Shifting load to LATENCY CCD (CCD0)...");
            affinity_apply_partition(worker, PART_CACHE);
            
            for (int i = 0; i < interval && active; i++) {
                struct timespec ts = {1, 0};
                nanosleep(&ts, NULL);
            }

            if (!active) break;

            printf_step("${ALRIGHT} Shifting load to THROUGHPUT CCD (CCD1)...");
            affinity_apply_partition(worker, PART_FREQ);

            for (int i = 0; i < interval && active; i++) {
                struct timespec ts = {1, 0};
                nanosleep(&ts, NULL);
            }
        }
        
        kill(worker, SIGTERM);
        waitpid(worker, NULL, 0);

    } else {
        printf_step("${STOPSIGN} Mode: Total CCD Saturation (100%% All Cores)");
        
        pid_t workers[128];
        int count = total_cores;
        if (count > 128) count = 128;

        for (int i = 0; i < count; i++) {
            workers[i] = fork();
            if (workers[i] == 0) {
                load_payload();
            }
        }

        while (active) {
            struct timespec ts = {0, 500000000L}; // 500ms
            nanosleep(&ts, NULL);
        }

        for (int i = 0; i < count; i++) {
            kill(workers[i], SIGTERM);
            waitpid(workers[i], NULL, 0);
        }
    }

    printf_step("${ALRIGHT} Compute stress engine gracefully stopped.");
    return 0;
}

/* end of STRESS.C */
