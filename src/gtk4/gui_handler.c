/* Global App-Level Signal Handlers Implementation
 * `gui_handler.c`
 */

#include "gui_handler.h"
#include "gui_linker.h"
#include "gui_stubs.h"
#include <string.h>

void x3d_gui_handler_on_dirty_notify(GObject *obj, GParamSpec *pspec,
                                     gpointer data) {
  (void)obj;
  (void)pspec;
  x3d_gui_frame_sync_global_cta((X3dGuiFrame *)data);
}

static gboolean reset_global_apply_lbl_cb(gpointer data) {
  GtkButton *btn = GTK_BUTTON(data);
  gtk_button_set_label(btn, "Apply");
  g_object_unref(btn);
  return G_SOURCE_REMOVE;
}

static void on_global_apply_clicked(GtkButton *btn, gpointer data) {
  X3dGuiFrame *f = (X3dGuiFrame *)data;
  const X3dPageModule *mod =
      gui_linker_find_page(x3d_gui_frame_get_current_page(f));
  if (!mod)
    return;
  gboolean all_ok = mod->apply_all ? mod->apply_all() : TRUE;
  gtk_button_set_label(btn, all_ok ? "Applied!" : "Failed");
  g_timeout_add(1200, reset_global_apply_lbl_cb, g_object_ref(btn));
  x3d_gui_frame_sync_global_cta(f);
}

static void on_global_cancel_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  X3dGuiFrame *f = (X3dGuiFrame *)data;
  const X3dPageModule *mod =
      gui_linker_find_page(x3d_gui_frame_get_current_page(f));
  if (mod && mod->cancel_all)
    mod->cancel_all();
  x3d_gui_frame_sync_global_cta(f);
}

static void on_global_default_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  X3dGuiFrame *f = (X3dGuiFrame *)data;
  const X3dPageModule *mod =
      gui_linker_find_page(x3d_gui_frame_get_current_page(f));
  if (mod && mod->default_all)
    mod->default_all();
  x3d_gui_frame_sync_global_cta(f);
}

static void on_sidebar_row_selected(GtkListBox *lb, GtkListBoxRow *row,
                                    gpointer data) {
  (void)lb;
  X3dGuiFrame *f = (X3dGuiFrame *)data;
  if (x3d_gui_frame_is_suppressing_nav(f) || !row)
    return;
  const char *id = gtk_widget_get_name(GTK_WIDGET(row));
  if (!id || !id[0])
    return;
  x3d_gui_frame_request_navigate(f, id, FALSE);
}

static void on_win_close_guard_resp(AdwAlertDialog *dlg, const char *resp,
                                    gpointer data) {
  (void)dlg;
  X3dGuiFrame *f = (X3dGuiFrame *)data;
  X3dGuiView *view = x3d_gui_frame_get_view(f);
  const X3dPageModule *mod =
      gui_linker_find_page(x3d_gui_frame_get_current_page(f));

  if (g_strcmp0(resp, "save") == 0) {
    gboolean all_ok = (mod && mod->apply_all) ? mod->apply_all() : TRUE;
    if (all_ok) {
      x3d_gui_frame_set_force_closing(f, TRUE);
      gtk_window_close(view->main_window);
    } else {
      x3d_stubs_toast(
          "Save failed (daemon IPC error) — window close aborted to preserve "
          "staged edits");
      x3d_gui_frame_sync_global_cta(f);
    }
  } else if (g_strcmp0(resp, "discard") == 0) {
    if (mod && mod->cancel_all)
      mod->cancel_all();
    x3d_gui_frame_set_force_closing(f, TRUE);
    gtk_window_close(view->main_window);
  }
}

static gboolean on_main_win_close_request(GtkWindow *win, gpointer data) {
  X3dGuiFrame *f = (X3dGuiFrame *)data;
  if (x3d_gui_frame_is_force_closing(f))
    return FALSE;

  const X3dPageModule *mod =
      gui_linker_find_page(x3d_gui_frame_get_current_page(f));
  gboolean dirty = (mod && mod->has_dirty) ? mod->has_dirty() : FALSE;
  if (!dirty)
    return FALSE;

  AdwDialog *dlg = adw_alert_dialog_new(
      "Unapplied Staged Changes",
      "You have unapplied edits on this page. Save changes before closing, "
      "discard them, or cancel?");
  adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dlg), "cancel", "Cancel",
                                 "discard", "Discard", "save", "Save", NULL);
  adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dlg), "save",
                                           ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dlg), "discard",
                                           ADW_RESPONSE_DESTRUCTIVE);
  adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dlg), "save");
  adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dlg), "cancel");
  g_signal_connect(dlg, "response", G_CALLBACK(on_win_close_guard_resp), f);
  adw_dialog_present(dlg, GTK_WIDGET(win));
  return TRUE;
}

static gboolean on_win_key_pressed(GtkEventControllerKey *ctrl, guint keyval,
                                   guint keycode, GdkModifierType state,
                                   gpointer data) {
  (void)ctrl;
  (void)keycode;
  X3dGuiFrame *f = (X3dGuiFrame *)data;
  X3dGuiView *v = x3d_gui_frame_get_view(f);
  if ((state & GDK_CONTROL_MASK) &&
      (keyval == GDK_KEY_q || keyval == GDK_KEY_Q)) {
    gtk_window_close(v->main_window);
    return TRUE;
  }
  if (keyval == GDK_KEY_Escape) {
    const char *cur = x3d_gui_frame_get_current_page(f);
    if (cur && (strcmp(cur, "hardware_modes_view") == 0 ||
                strcmp(cur, "system_profile_view") == 0)) {
      x3d_gui_frame_request_navigate(f, "dashboard", TRUE);
      return TRUE;
    }
  }
  return FALSE;
}

void x3d_gui_handler_bind(X3dGuiFrame *frame) {
  X3dGuiView *view = x3d_gui_frame_get_view(frame);
  if (!view)
    return;

  g_signal_connect(view->sidebar_list, "row-selected",
                   G_CALLBACK(on_sidebar_row_selected), frame);
  g_signal_connect(view->main_window, "close-request",
                   G_CALLBACK(on_main_win_close_request), frame);

  g_signal_connect(view->btn_global_apply, "clicked",
                   G_CALLBACK(on_global_apply_clicked), frame);
  g_signal_connect(view->btn_global_cancel, "clicked",
                   G_CALLBACK(on_global_cancel_clicked), frame);
  g_signal_connect(view->btn_global_default, "clicked",
                   G_CALLBACK(on_global_default_clicked), frame);

  GtkEventController *key_ctrl = gtk_event_controller_key_new();
  g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(on_win_key_pressed),
                   frame);
  gtk_widget_add_controller(GTK_WIDGET(view->main_window), key_ctrl);
}
