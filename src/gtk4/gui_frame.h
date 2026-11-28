/* Main Window Frame Orchestrator Header
 * `gui_frame.h`
 * Owns main window lifecycle, sidebar navigation, polling cadence, and the
 * uniform Dirty-State Safeguard across all X3dPageModule slices.
 * Strictly zero direct page header dependencies.
 */

#ifndef X3D_GUI_FRAME_H
#define X3D_GUI_FRAME_H

#include "gui_ipc.h"
#include "gui_view.h"

typedef struct _X3dGuiFrame X3dGuiFrame;

X3dGuiFrame *x3d_gui_frame_new(X3dGuiIpc *ipc, X3dGuiView *view);
void x3d_gui_frame_free(X3dGuiFrame *frame);

X3dGuiView *x3d_gui_frame_get_view(const X3dGuiFrame *frame);
const char *x3d_gui_frame_get_current_page(const X3dGuiFrame *frame);
gboolean x3d_gui_frame_is_suppressing_nav(const X3dGuiFrame *frame);
gboolean x3d_gui_frame_is_force_closing(const X3dGuiFrame *frame);
void x3d_gui_frame_set_force_closing(X3dGuiFrame *frame, gboolean closing);

void x3d_gui_frame_sync_sidebar_gating(X3dGuiFrame *frame);
void x3d_gui_frame_sync_global_cta(X3dGuiFrame *frame);
void x3d_gui_frame_poll_dashboard_now(const X3dGuiFrame *frame);

void x3d_gui_frame_perform_switch(X3dGuiFrame *frame, const char *target_page,
                                  gboolean sync_sidebar);
void x3d_gui_frame_request_navigate(X3dGuiFrame *frame, const char *target_page,
                                    gboolean sync_sidebar);

#endif /* X3D_GUI_FRAME_H */
