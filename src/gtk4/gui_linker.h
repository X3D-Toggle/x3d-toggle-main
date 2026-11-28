/* Modular Page & Popup Linker Header
 * `gui_linker.h`
 * Central registry that wires gui_frame, gui_ipc, pages/gui_widgets, and all
 * 10 X3dPageModule slices together without exposing page headers to gui_frame.
 */

#ifndef X3D_GUI_LINKER_H
#define X3D_GUI_LINKER_H

#include "gui_view.h"

void gui_linker_register_types(void);
void gui_linker_init(const X3dPageCtx *ctx);
void gui_linker_mount_all(void);
const X3dPageModule *gui_linker_find_page(const char *page_id);
void gui_linker_refresh_active(const char *page_id);
void gui_linker_activate_page(const char *old_page_id, const char *new_page_id);
void gui_linker_deactivate_all(void);
void gui_linker_notify_interval_changed(guint interval_ms);
void gui_linker_broadcast_hw_mode(const char *mode_upper);

#endif /* X3D_GUI_LINKER_H */
