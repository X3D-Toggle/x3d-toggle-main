/* Native GObject / GtkWidget Subclasses & Control Primitives
 * `pages/gui_widgets.h`
 * Implements X3dCurrentWidget, X3dStagedWidget, X3dImmediateWidget, and
 * X3dDangerWidget with native dirty-state properties, default/cancel/apply
 * signals, 3-column centered/justified grids, capability bindings, and the
 * desktop widgets section builder.
 */

#ifndef X3D_GUI_WIDGETS_H
#define X3D_GUI_WIDGETS_H

#include "../gui_ipc.h"
#include "../gui_view.h"
#include <adwaita.h>
#include <gtk/gtk.h>

/* ── 1. X3dCurrentWidget (.current-widget) ────────────────────── */

#define X3D_TYPE_CURRENT_WIDGET (x3d_current_widget_get_type())
G_DECLARE_FINAL_TYPE(X3dCurrentWidget, x3d_current_widget, X3D, CURRENT_WIDGET,
                     GtkBox)

GtkWidget *x3d_current_widget_new(const char *title, gboolean clickable);
GtkBox *x3d_current_widget_get_body(X3dCurrentWidget *self);
void x3d_current_widget_add_header_suffix(X3dCurrentWidget *self,
                                          GtkWidget *suffix);

/* ── 2. X3dStagedWidget (.staged-widget) ──────────────────────── */

#define X3D_TYPE_STAGED_WIDGET (x3d_staged_widget_get_type())
G_DECLARE_FINAL_TYPE(X3dStagedWidget, x3d_staged_widget, X3D, STAGED_WIDGET,
                     GtkBox)

GtkWidget *x3d_staged_widget_new(const char *title, gboolean standalone);
GtkBox *x3d_staged_widget_get_body(X3dStagedWidget *self);
void x3d_staged_widget_add_header_suffix(X3dStagedWidget *self,
                                         GtkWidget *suffix);
void x3d_staged_widget_add_left_action(X3dStagedWidget *self, GtkWidget *btn);
void x3d_staged_widget_set_grid_items(X3dStagedWidget *self,
                                      GtkWidget *const *items, guint count);
void x3d_staged_widget_watch_control(X3dStagedWidget *self, GtkWidget *ctrl);
void x3d_staged_widget_set_syncing(X3dStagedWidget *self, gboolean syncing);
gboolean x3d_staged_widget_get_syncing(const X3dStagedWidget *self);
gboolean x3d_staged_widget_get_dirty(const X3dStagedWidget *self);
void x3d_staged_widget_set_dirty(X3dStagedWidget *self, gboolean dirty);
gboolean x3d_staged_widget_trigger_apply(X3dStagedWidget *self);
void x3d_staged_widget_trigger_cancel(X3dStagedWidget *self);
void x3d_staged_widget_trigger_default(X3dStagedWidget *self);

/* ── 3. X3dImmediateWidget (.immediate-widget) ────────────────── */

#define X3D_TYPE_IMMEDIATE_WIDGET (x3d_immediate_widget_get_type())
G_DECLARE_FINAL_TYPE(X3dImmediateWidget, x3d_immediate_widget, X3D,
                     IMMEDIATE_WIDGET, GtkBox)

GtkWidget *x3d_immediate_widget_new(const char *title);
GtkBox *x3d_immediate_widget_get_body(X3dImmediateWidget *self);
void x3d_immediate_widget_set_grid_items(X3dImmediateWidget *self,
                                         GtkWidget *const *items, guint count);

/* ── 4. X3dDangerWidget (.danger-widget) ──────────────────────── */

#define X3D_TYPE_DANGER_WIDGET (x3d_danger_widget_get_type())
G_DECLARE_FINAL_TYPE(X3dDangerWidget, x3d_danger_widget, X3D, DANGER_WIDGET,
                     GtkBox)

GtkWidget *x3d_danger_widget_new(const char *title, const char *description,
                                 const char *button_label,
                                 const char *confirm_heading,
                                 const char *confirm_body);

/* ── Reusable Layout, Capability & Control Primitives ─────────── */

void x3d_widgets_register_types(void);

GtkWidget *x3d_centered_3col_grid_new(void);
void x3d_grid_pack_centered_3col(GtkGrid *grid, GtkWidget *const *items,
                                 guint count);

void x3d_widget_bind_caps(GtkWidget *widget, X3dGuiIpc *ipc, uint64_t cap_mask);

/* Control Primitives (-Toggle, -LiveToggle, -PopUp, -Slider+Input, Form Rows) */
GtkWidget *x3d_toggle_new(const char *label);
GtkWidget *x3d_popup_btn_new(const char *label);
GtkWidget *x3d_slider_entry_new(double lo, double hi, double step, double init,
                                const char *unit, GtkRange **range_out);
void x3d_slider_entry_set_bounds(GtkRange *range, double lo, double hi,
                                 double step);
void x3d_slider_entry_set_value(GtkRange *range, double val);
int x3d_slider_entry_get_int(GtkRange *range);
GtkWidget *x3d_form_row_new(const char *title, const char *subtitle,
                            GtkWidget *control);
GtkWidget *x3d_live_toggle_row_new(const char *title, const char *subtitle,
                                   GtkWidget **toggle_out);
GtkWidget *x3d_subsection_label_new(const char *title);
GtkWidget *x3d_page_header_new(const char *title);
GtkBox *x3d_page_vbox_new(GtkStack *stack, const char *name, const char *title);

/* Recursive StagedWidget introspection helpers for page modules */
gboolean x3d_widgets_tree_has_staged(GtkWidget *root);
gboolean x3d_widgets_tree_has_dirty(GtkWidget *root);
gboolean x3d_widgets_tree_apply_dirty(GtkWidget *root);
void x3d_widgets_tree_cancel_dirty(GtkWidget *root);
void x3d_widgets_tree_default_all(GtkWidget *root);
void x3d_widgets_tree_connect_dirty_notify(GtkWidget *root, GCallback cb,
                                           gpointer user_data);

/* Desktop Widgets & Status Notifier Item Staged Widget Builder (mounted in Extras) */
GtkWidget *gui_widgets_build_desktop_section(const X3dPageCtx *ctx);
void gui_widgets_refresh_desktop_section(void);

/* Module descriptor for gui_linker registration */
const X3dPageModule *gui_widgets_get_module(void);

#endif /* X3D_GUI_WIDGETS_H */
