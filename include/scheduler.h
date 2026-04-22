/* Configurable Optimizations - CPU Scheduler
 *
 *`scheduler.h` - Header only
 */

#ifndef X3D_SCHEDULER_H
#define X3D_SCHEDULER_H

#include <stdbool.h>

/* Performance Profiles */
typedef enum {
    SCHED_BALANCED,
    SCHED_GAMING
} sched_t;

/* Sets the system-wide scheduler profile (Requires Root) */
int scheduler_set(sched_t mode);

/* Checks for BORE support in the current kernel */
bool scheduler_check(void);

#endif // SCHEDULER.H