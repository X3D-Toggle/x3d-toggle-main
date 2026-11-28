/* Global App-Level Signal Handlers Header
 * `gui_handler.h`
 * Binds sidebar row selection, universal sidebar CallToAction buttons,
 * window close-request Dirty-State Safeguard, and global keyboard shortcuts.
 */

#ifndef X3D_GUI_HANDLER_H
#define X3D_GUI_HANDLER_H

#include "gui_frame.h"

void x3d_gui_handler_bind(X3dGuiFrame *frame);
void x3d_gui_handler_on_dirty_notify(GObject *obj, GParamSpec *pspec,
                                     gpointer data);

#endif /* X3D_GUI_HANDLER_H */
