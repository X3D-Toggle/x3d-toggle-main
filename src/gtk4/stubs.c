/* GUI Linker Stubs for project-native modules 
 * 
 * Prevents pulling in the entire daemon dependency tree when 
 * linking autonomous CLI modules into the GTK frontend.
 */

#include "../../include/libc.h"

int unit_active(void) {
    return 0; 
}

void daemon_failsafe(int sig) {
    (void)sig;
}
