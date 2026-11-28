/* Main Window Frame Orchestrator Implementation
 * `gui_frame.c`
 * Strictly routes all page operations through `gui_linker` (zero direct page includes).
 */

#include "gui_frame.h"
#include "gui_handler.h"
#include "gui_linker.h"
#include "gui_stubs.h"
#include <string.h>

struct _X3dGuiFrame {
  X3dGuiIpc *ipc;
  X3dGuiView *view;
  gchar *current_page;
  gchar *pending_page;
  guint dash_timer_id;
  gboolean suppress_nav;
  gboolean force_closing;
  gboolean in_dirty_guard;
};

X3dGuiView *x3d_gui_frame_get_view(const X3dGuiFrame *frame) {
  return frame ? frame->view : NULL;
}

const char *x3d_gui_frame_get_current_page(const X3dGuiFrame *frame) {
  return frame ? frame->current_page : NULL;
}

gboolean x3d_gui_frame_is_suppressing_nav(const X3dGuiFrame *frame) {
  return frame ? frame->suppress_nav : FALSE;
}

gboolean x3d_gui_frame_is_force_closing(const X3dGuiFrame *frame) {
  return frame ? frame->force_closing : FALSE;
}

void x3d_gui_frame_set_force_closing(X3dGuiFrame *frame, gboolean closing) {
  if (frame)
    frame->force_closing = closing;
}

/* ── Sidebar Gating & Universal CTA Sync ──────────────────────── */

void x3d_gui_frame_sync_sidebar_gating(X3dGuiFrame *f) {
  if (!f || !f->view || !f->ipc)
    return;
  gboolean adv = x3d_gui_ipc_get_adv_enabled(f->ipc);
  gboolean dev = x3d_gui_ipc_get_dev_enabled(f->ipc);

  x3d_gui_view_set_sidebar_row_visible(f->view, "advanced", adv);
  x3d_gui_view_set_sidebar_row_visible(f->view, "developer", adv && dev);

  if (!adv && f->current_page &&
      (strcmp(f->current_page, "advanced") == 0 ||
       strcmp(f->current_page, "developer") == 0)) {
    x3d_gui_frame_perform_switch(f, "daemon_settings", TRUE);
  } else if (!dev && f->current_page &&
             strcmp(f->current_page, "developer") == 0) {
    x3d_gui_frame_perform_switch(f, "advanced", TRUE);
  }
}

void x3d_gui_frame_sync_global_cta(X3dGuiFrame *f) {
  if (!f || !f->view)
    return;
  const X3dPageModule *mod = gui_linker_find_page(f->current_page);
  gboolean has_dirty = (mod && mod->has_dirty) ? mod->has_dirty() : FALSE;
  gboolean has_staged = (mod && mod->has_staged) ? mod->has_staged() : FALSE;

  gtk_widget_set_sensitive(f->view->btn_global_apply, has_dirty);
  gtk_widget_set_sensitive(f->view->btn_global_cancel, has_dirty);
  gtk_widget_set_sensitive(f->view->btn_global_default, has_staged);
}

/* ── Dashboard Polling & Interval Cadence ─────────────────────── */

static gboolean dash_poll_timer_cb(gpointer data) {
  (void)data;
  gui_linker_refresh_active("dashboard");
  return G_SOURCE_CONTINUE;
}

void x3d_gui_frame_poll_dashboard_now(const X3dGuiFrame *f) {
  if (!f)
    return;
  gui_linker_refresh_active("dashboard");
}

static void on_update_interval_changed(const X3dGuiIpc *ipc, GParamSpec *pspec,
                                       gpointer data) {
  (void)pspec;
  X3dGuiFrame *f = (X3dGuiFrame *)data;
  guint ms = x3d_gui_ipc_get_update_interval_ms(ipc);
  if (f->dash_timer_id != 0) {
    g_source_remove(f->dash_timer_id);
    f->dash_timer_id = g_timeout_add(ms, dash_poll_timer_cb, f);
  }
  gui_linker_notify_interval_changed(ms);
}

/* ── Navigation & Uniform Dirty-State Safeguard ───────────────── */

void x3d_gui_frame_perform_switch(X3dGuiFrame *f, const char *target_page,
                                  gboolean sync_sidebar) {
  if (!f || !target_page || f->in_dirty_guard)
    return;

  gchar *old_page = g_strdup(f->current_page);
  gtk_stack_set_visible_child_name(f->view->content_stack, target_page);
  g_free(f->current_page);
  f->current_page = g_strdup(target_page);

  gui_linker_activate_page(old_page, target_page);
  g_free(old_page);

  if (sync_sidebar) {
    f->suppress_nav = TRUE;
    GtkListBoxRow *r = x3d_gui_view_find_sidebar_row(f->view, target_page);
    gtk_list_box_select_row(f->view->sidebar_list, r);
    f->suppress_nav = FALSE;
  }

  x3d_gui_frame_sync_global_cta(f);

  if (f->view->split_view &&
      adw_overlay_split_view_get_collapsed(f->view->split_view)) {
    adw_overlay_split_view_set_show_sidebar(f->view->split_view, FALSE);
  }
}

static void on_dirty_guard_resp(AdwAlertDialog *dlg, const char *resp,
                                gpointer data) {
  (void)dlg;
  X3dGuiFrame *f = (X3dGuiFrame *)data;
  if (!f->pending_page)
    return;

  const X3dPageModule *mod = gui_linker_find_page(f->current_page);

  if (g_strcmp0(resp, "save") == 0) {
    f->in_dirty_guard = TRUE;
    gboolean all_ok = (mod && mod->apply_all) ? mod->apply_all() : TRUE;
    f->in_dirty_guard = FALSE;

    if (all_ok) {
      gchar *dest = g_steal_pointer(&f->pending_page);
      x3d_gui_frame_perform_switch(f, dest, TRUE);
      g_free(dest);
    } else {
      g_clear_pointer(&f->pending_page, g_free);
      x3d_stubs_toast(
          "Save failed (daemon IPC error) — navigation aborted to preserve "
          "staged edits");
      x3d_gui_frame_sync_global_cta(f);
    }
  } else if (g_strcmp0(resp, "discard") == 0) {
    if (mod && mod->cancel_all)
      mod->cancel_all();
    gchar *dest = g_steal_pointer(&f->pending_page);
    x3d_gui_frame_perform_switch(f, dest, TRUE);
    g_free(dest);
  } else {
    g_clear_pointer(&f->pending_page, g_free);
  }
}

void x3d_gui_frame_request_navigate(X3dGuiFrame *f, const char *target_page,
                                    gboolean sync_sidebar) {
  if (!f || !target_page)
    return;
  if (f->current_page && strcmp(f->current_page, target_page) == 0)
    return;

  const X3dPageModule *mod = gui_linker_find_page(f->current_page);
  gboolean dirty = (mod && mod->has_dirty) ? mod->has_dirty() : FALSE;

  if (!dirty) {
    x3d_gui_frame_perform_switch(f, target_page, sync_sidebar);
    return;
  }

  f->suppress_nav = TRUE;
  GtkListBoxRow *cur_r =
      x3d_gui_view_find_sidebar_row(f->view, f->current_page);
  gtk_list_box_select_row(f->view->sidebar_list, cur_r);
  f->suppress_nav = FALSE;

  g_free(f->pending_page);
  f->pending_page = g_strdup(target_page);

  AdwDialog *dlg = adw_alert_dialog_new(
      "Unapplied Staged Changes",
      "You have unapplied edits on this page. Would you like to Save and "
      "apply them, Discard them, or Cancel navigation?");
  adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dlg), "cancel", "Cancel",
                                 "discard", "Discard", "save", "Save", NULL);
  adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dlg), "save",
                                           ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dlg), "discard",
                                           ADW_RESPONSE_DESTRUCTIVE);
  adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dlg), "save");
  adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dlg), "cancel");
  g_signal_connect(dlg, "response", G_CALLBACK(on_dirty_guard_resp), f);
  adw_dialog_present(dlg, GTK_WIDGET(f->view->main_window));
}

/* ── Context Callbacks Passed to gui_linker ───────────────────── */

static void ctx_req_nav_cb(gpointer frame, const char *target, gboolean sync) {
  x3d_gui_frame_request_navigate((X3dGuiFrame *)frame, target, sync);
}

static void ctx_perf_sw_cb(gpointer frame, const char *target, gboolean sync) {
  x3d_gui_frame_perform_switch((X3dGuiFrame *)frame, target, sync);
}

static void ctx_sync_gating_cb(gpointer frame) {
  x3d_gui_frame_sync_sidebar_gating((X3dGuiFrame *)frame);
}

static void ctx_sync_cta_cb(gpointer frame) {
  x3d_gui_frame_sync_global_cta((X3dGuiFrame *)frame);
}

static void ctx_poll_dash_cb(gpointer frame) {
  x3d_gui_frame_poll_dashboard_now((X3dGuiFrame *)frame);
}

/* ── Frame Lifecycle ──────────────────────────────────────────── */

X3dGuiFrame *x3d_gui_frame_new(X3dGuiIpc *ipc, X3dGuiView *view) {
  X3dGuiFrame *f = g_new0(X3dGuiFrame, 1);
  f->ipc = ipc;
  f->view = view;

  x3d_stubs_set_toast_overlay(view->toast_overlay);

  X3dPageCtx ctx = {
      .ipc = ipc,
      .view = view,
      .frame = f,
      .request_navigate = ctx_req_nav_cb,
      .perform_switch = ctx_perf_sw_cb,
      .sync_sidebar_gating = ctx_sync_gating_cb,
      .sync_global_cta = ctx_sync_cta_cb,
      .poll_dashboard_now = ctx_poll_dash_cb,
      .broadcast_hw_mode = gui_linker_broadcast_hw_mode,
      .notify_dirty_cb = G_CALLBACK(x3d_gui_handler_on_dirty_notify),
      .notify_dirty_data = f,
  };

  gui_linker_init(&ctx);
  gui_linker_mount_all();

  x3d_gui_frame_sync_sidebar_gating(f);
  x3d_gui_handler_bind(f);

  g_signal_connect(ipc, "notify::update-interval-ms",
                   G_CALLBACK(on_update_interval_changed), f);

  x3d_gui_frame_perform_switch(f, "dashboard", TRUE);
  x3d_gui_frame_poll_dashboard_now(f);
  f->dash_timer_id = g_timeout_add(x3d_gui_ipc_get_update_interval_ms(f->ipc),
                                   dash_poll_timer_cb, f);

  return f;
}

void x3d_gui_frame_free(X3dGuiFrame *f) {
  if (!f)
    return;
  if (f->dash_timer_id != 0)
    g_source_remove(f->dash_timer_id);
  gui_linker_deactivate_all();
  g_free(f->current_page);
  g_free(f->pending_page);
  g_free(f);
}
