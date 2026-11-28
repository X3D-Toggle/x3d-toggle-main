/* KDE6 SNI Style, Desktop Widgets & 7-Mode Live Telemetry Visualizer Header
 * `popups/popup_style.h`
 */

#ifndef X3D_POPUP_STYLE_H
#define X3D_POPUP_STYLE_H

#include "../gui_popups.h"

void popup_style_show_kde_sni(GtkWindow *parent, X3dStagedWidget *owner_staged);

void popup_style_show_desktop_widgets(GtkWindow *parent, X3dGuiIpc *ipc,
                                      X3dStagedWidget *owner_staged);

void popup_style_show_telemetry(GtkWindow *parent, X3dGuiIpc *ipc,
                                X3dTelemetryKind kind,
                                const X3dGpuInfo *gpu_opt);

#endif /* X3D_POPUP_STYLE_H */
