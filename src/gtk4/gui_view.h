/* GTK4 Frontend View Shell & Modular Page Contract Header
 * `gui_view.h`
 * Owns the root AdwApplicationWindow / AdwOverlaySplitView / GtkStack shell,
 * dynamic CSS theme engine, and the page-agnostic X3dPageModule contract.
 * Strictly zero `#include "pages/..."` dependencies.
 */

#ifndef X3D_GUI_VIEW_H
#define X3D_GUI_VIEW_H

#include "gui_ipc.h"
#include <adwaita.h>
#include <gtk/gtk.h>

typedef struct _X3dGuiView {
  GtkWindow *main_window;
  AdwToastOverlay *toast_overlay;
  AdwOverlaySplitView *split_view;
  GtkToggleButton *btn_sidebar_toggle;
  GtkListBox *sidebar_list;
  GtkWidget *btn_global_apply;
  GtkWidget *btn_global_cancel;
  GtkWidget *btn_global_default;
  GtkStack *content_stack;
} X3dGuiView;

typedef void (*X3dNavRequestFn)(gpointer frame, const char *target_page,
                                gboolean sync_sidebar);
typedef void (*X3dSimpleCb)(gpointer frame);
typedef void (*X3dHwModeSyncFn)(const char *mode_upper);

typedef struct _X3dPageCtx {
  X3dGuiIpc *ipc;
  X3dGuiView *view;
  gpointer frame;
  X3dNavRequestFn request_navigate;
  X3dNavRequestFn perform_switch;
  X3dSimpleCb sync_sidebar_gating;
  X3dSimpleCb sync_global_cta;
  X3dSimpleCb poll_dashboard_now;
  X3dHwModeSyncFn broadcast_hw_mode;
  GCallback notify_dirty_cb;
  gpointer notify_dirty_data;
} X3dPageCtx;

typedef struct _X3dPageModule {
  const char *id;            /* Stack page id (NULL for embedded-only modules) */
  const char *title;         /* Sidebar row label */
  gboolean is_subnav;        /* Indented sub-navigation row */
  gboolean initially_hidden; /* Hidden until gated on (advanced / developer) */
  gboolean divider_after;    /* Insert structural divider row after this item */

  GtkWidget *(*build)(const X3dPageCtx *ctx);
  void (*refresh)(const X3dPageCtx *ctx);
  void (*activate)(const X3dPageCtx *ctx);
  void (*deactivate)(const X3dPageCtx *ctx);
  gboolean (*has_dirty)(void);
  gboolean (*has_staged)(void);
  gboolean (*apply_all)(void);
  void (*cancel_all)(void);
  void (*default_all)(void);
  void (*on_interval_changed)(const X3dPageCtx *ctx, guint interval_ms);
  void (*highlight_hw_mode)(const char *mode_upper);
} X3dPageModule;

X3dGuiView *x3d_gui_view_build(GtkApplication *app);
void x3d_gui_view_free(X3dGuiView *view);

/* Sidebar Row Construction & Lookup */
GtkWidget *x3d_gui_view_add_sidebar_row(X3dGuiView *v, const char *id,
                                        const char *title, gboolean is_subnav,
                                        gboolean initially_hidden);
void x3d_gui_view_add_sidebar_separator(X3dGuiView *v);
GtkListBoxRow *x3d_gui_view_find_sidebar_row(X3dGuiView *v,
                                             const char *page_id);
void x3d_gui_view_set_sidebar_row_visible(X3dGuiView *v, const char *page_id,
                                          gboolean visible);

/* Theme & Dynamic CSS Engine */
void x3d_gui_view_init_css(void);
void x3d_gui_view_apply_theme_prefs(const X3dGuiThemePrefs *prefs);

#endif /* X3D_GUI_VIEW_H */
