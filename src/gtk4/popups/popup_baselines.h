/* Hysteresis Baselines & AOD Firmware Restart Notice Popups Header
 * `popups/popup_baselines.h`
 */

#ifndef X3D_POPUP_BASELINES_H
#define X3D_POPUP_BASELINES_H

#include "../gui_popups.h"

void popup_baselines_show_hysteresis(GtkWindow *parent,
                                     X3dStagedWidget *owner_staged);

void popup_baselines_show_aod_restart(GtkWindow *parent);

#endif /* X3D_POPUP_BASELINES_H */
