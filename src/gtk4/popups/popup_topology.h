/* Topology & Per-Core Frequency Floor Popups Header
 * `popups/popup_topology.h`
 */

#ifndef X3D_POPUP_TOPOLOGY_H
#define X3D_POPUP_TOPOLOGY_H

#include "../gui_popups.h"

void popup_topology_show_floor_freq(GtkWindow *parent,
                                    X3dStagedWidget *owner_staged);

void popup_topology_show_map(GtkWindow *parent, X3dGuiIpc *ipc,
                             X3dStagedWidget *owner_staged,
                             char *staged_mask_buf, size_t mask_sz,
                             GtkWidget *summary_lbl);

#endif /* X3D_POPUP_TOPOLOGY_H */
