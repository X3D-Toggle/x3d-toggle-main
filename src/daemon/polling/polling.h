#ifndef X3D_POLLING_H
#define X3D_POLLING_H

#include <stdbool.h>
#include "../../../include/status.h"

bool detect_game(void);
void polling_run(CPUStats *p_stat, CPUStats *c_stat, char *current, char *target);

#endif // X3D_POLLING_H
