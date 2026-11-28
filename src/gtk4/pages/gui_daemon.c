/* Daemon Settings Page Slice Implementation
 * `pages/gui_daemon.c`
 * Routes all daemon state and lifecycle operations through x3d-daemon via gui_ipc.
 */

#include "gui_daemon.h"
#include "../gui_stubs.h"
#include "gui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const k_falls[3] = {"default", "cache", "frequency"};
static const char *const k_states[3] = {"default", "auto", "manual"};

static struct {
  X3dPageCtx ctx;
  GtkWidget *root;

  X3dStagedWidget *stg_daemon_behavior;
  GtkRange *sc_polling_interval;
  GtkRange *sc_load_threshold;
  GtkToggleButton *tb_watch_mode;
  GtkDropDown *dd_detection_level;
  GtkDropDown *dd_fallback_profile;
  GtkDropDown *dd_daemon_state;
  GtkToggleButton *tb_ebpf_enable;

  GtkToggleButton *tb_systemd_persist;
  GtkToggleButton *tb_adv_options;
  gboolean suppress_live_toggles;
} g_dmn;

static guint find_str_idx(const char *const *arr, guint n, const char *target) {
  if (!target)
    return 0;
  for (guint i = 0; i < n; i++) {
    if (g_ascii_strcasecmp(arr[i], target) == 0)
      return i;
  }
  return 0;
}

static void exec_daemon_lifecycle(const char *action_id) {
  if (!action_id)
    return;
  if (strcmp(action_id, "wake") == 0) {
    x3d_ipc_send("CONFIG_SYNC", NULL, 0);
    x3d_ipc_send("SET_DAEMON default", NULL, 0);
    x3d_ipc_send("SET_MODE frequency", NULL, 0);
  } else if (strcmp(action_id, "sleep") == 0) {
    x3d_ipc_send("SET_DAEMON manual", NULL, 0);
  } else if (strcmp(action_id, "stop") == 0) {
    x3d_ipc_send("DAEMON_DISABLE", NULL, 0);
  } else if (strcmp(action_id, "enable") == 0) {
    if (!x3d_ipc_cmd_ok("SET_DAEMON default")) {
      char *args[] = {(char *)"/usr/bin/x3d-toggle", (char *)"enable", NULL};
      x3d_ipc_spawn(args, NULL);
    }
  } else if (strcmp(action_id, "autostart") == 0) {
    if (!x3d_ipc_cmd_ok("SET_DAEMON auto")) {
      char *args[] = {(char *)"/usr/bin/x3d-toggle", (char *)"start", NULL};
      x3d_ipc_spawn(args, NULL);
    }
  }
}

static void mod_daemon_refresh(const X3dPageCtx *ctx) {
  if (ctx)
    g_dmn.ctx = *ctx;

  x3d_staged_widget_set_syncing(g_dmn.stg_daemon_behavior, TRUE);
  const char *pv = x3d_ipc_config_get("POLLING_INTERVAL");
  x3d_slider_entry_set_value(g_dmn.sc_polling_interval,
                             atof(pv[0] ? pv : "3"));
  const char *lv = x3d_ipc_config_get("LOAD_THRESHOLD");
  x3d_slider_entry_set_value(g_dmn.sc_load_threshold,
                             atof(lv[0] ? lv : "50"));
  gtk_toggle_button_set_active(g_dmn.tb_watch_mode,
                               atoi(x3d_ipc_config_get("IRQ_WATCH")) != 0);
  int det = atoi(x3d_ipc_config_get("DETECTION_LEVEL"));
  gtk_drop_down_set_selected(g_dmn.dd_detection_level, (det >= 2) ? 1U : 0U);
  gtk_drop_down_set_selected(
      g_dmn.dd_fallback_profile,
      find_str_idx(k_falls, G_N_ELEMENTS(k_falls),
                   x3d_ipc_config_get("FALLBACK_PROFILE")));
  gtk_drop_down_set_selected(
      g_dmn.dd_daemon_state,
      find_str_idx(k_states, G_N_ELEMENTS(k_states),
                   x3d_ipc_config_get("DAEMON_STATE")));
  const char *eb = x3d_ipc_config_get("EBPF_ENABLE");
  gtk_toggle_button_set_active(g_dmn.tb_ebpf_enable,
                               eb[0] ? (atoi(eb) != 0) : TRUE);
  x3d_staged_widget_set_syncing(g_dmn.stg_daemon_behavior, FALSE);

  g_dmn.suppress_live_toggles = TRUE;
  gtk_toggle_button_set_active(g_dmn.tb_systemd_persist,
                               x3d_stubs_get()->systemd_boot_persist);
  gtk_toggle_button_set_active(
      g_dmn.tb_adv_options, x3d_gui_ipc_get_adv_enabled(g_dmn.ctx.ipc));
  g_dmn.suppress_live_toggles = FALSE;
}

static void on_daemon_default(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  x3d_slider_entry_set_value(g_dmn.sc_polling_interval, 3);
  x3d_slider_entry_set_value(g_dmn.sc_load_threshold, 50);
  gtk_toggle_button_set_active(g_dmn.tb_watch_mode, FALSE);
  gtk_drop_down_set_selected(g_dmn.dd_detection_level, 1);
  gtk_drop_down_set_selected(g_dmn.dd_fallback_profile, 0);
  gtk_drop_down_set_selected(g_dmn.dd_daemon_state, 0);
  gtk_toggle_button_set_active(g_dmn.tb_ebpf_enable, TRUE);
}

static void on_daemon_cancel(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  mod_daemon_refresh(NULL);
}

static gboolean on_daemon_apply(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  char buf[32];
  gboolean ok = TRUE;

  snprintf(buf, sizeof(buf), "%d",
           x3d_slider_entry_get_int(g_dmn.sc_polling_interval));
  if (!x3d_ipc_config_set("POLLING_INTERVAL", buf))
    ok = FALSE;

  snprintf(buf, sizeof(buf), "%d",
           x3d_slider_entry_get_int(g_dmn.sc_load_threshold));
  if (!x3d_ipc_config_set("LOAD_THRESHOLD", buf))
    ok = FALSE;

  if (!x3d_ipc_config_set("IRQ_WATCH",
                          gtk_toggle_button_get_active(g_dmn.tb_watch_mode)
                              ? "1"
                              : "0"))
    ok = FALSE;

  guint det = gtk_drop_down_get_selected(g_dmn.dd_detection_level);
  if (!x3d_ipc_config_set("DETECTION_LEVEL", (det == 1) ? "2" : "1"))
    ok = FALSE;

  guint fi = gtk_drop_down_get_selected(g_dmn.dd_fallback_profile);
  if (fi < G_N_ELEMENTS(k_falls) &&
      !x3d_ipc_config_set("FALLBACK_PROFILE", k_falls[fi]))
    ok = FALSE;

  guint si = gtk_drop_down_get_selected(g_dmn.dd_daemon_state);
  if (si < G_N_ELEMENTS(k_states)) {
    char cmd[64];
    if (!x3d_ipc_config_set("DAEMON_STATE", k_states[si]))
      ok = FALSE;
    snprintf(cmd, sizeof(cmd), "SET_DAEMON %s", k_states[si]);
    if (!x3d_ipc_cmd_ok(cmd))
      ok = FALSE;
  }

  if (!x3d_ipc_config_set("EBPF_ENABLE",
                          gtk_toggle_button_get_active(g_dmn.tb_ebpf_enable)
                              ? "1"
                              : "0"))
    ok = FALSE;

  if (g_dmn.ctx.poll_dashboard_now)
    g_dmn.ctx.poll_dashboard_now(g_dmn.ctx.frame);
  if (!ok)
    x3d_stubs_toast("Failed to apply Daemon Behavior (daemon IPC error)");
  return ok;
}

static void on_lifecycle_btn_clicked(GtkButton *btn, gpointer d) {
  (void)d;
  const char *act = g_object_get_data(G_OBJECT(btn), "x3d-lc-act");
  exec_daemon_lifecycle(act);
  gchar *msg = g_strdup_printf("Daemon lifecycle dispatched: %s", act);
  x3d_stubs_toast(msg);
  g_free(msg);
  if (g_dmn.ctx.poll_dashboard_now)
    g_dmn.ctx.poll_dashboard_now(g_dmn.ctx.frame);
}

static void on_systemd_persist_toggled(GtkToggleButton *tb, gpointer d) {
  (void)d;
  if (g_dmn.suppress_live_toggles)
    return;
  x3d_stub_toggle_systemd_persistence(gtk_toggle_button_get_active(tb));
}

static void on_adv_options_toggled(GtkToggleButton *tb, gpointer d) {
  (void)d;
  if (g_dmn.suppress_live_toggles)
    return;
  gboolean active = gtk_toggle_button_get_active(tb);
  x3d_gui_ipc_set_adv_enabled(g_dmn.ctx.ipc, active);
  x3d_ipc_config_set("ADVANCED_CONFIG_ENABLE", active ? "1" : "0");
  if (g_dmn.ctx.sync_sidebar_gating)
    g_dmn.ctx.sync_sidebar_gating(g_dmn.ctx.frame);
}

static GtkWidget *mod_daemon_build(const X3dPageCtx *ctx) {
  g_dmn.ctx = *ctx;

  GtkBox *page = x3d_page_vbox_new(ctx->view->content_stack, "daemon_settings",
                                   "Daemon Settings");
  g_dmn.root = GTK_WIDGET(page);

  GtkWidget *stg = x3d_staged_widget_new("DAEMON BEHAVIOR", FALSE);
  g_dmn.stg_daemon_behavior = X3D_STAGED_WIDGET(stg);
  GtkBox *body = x3d_staged_widget_get_body(g_dmn.stg_daemon_behavior);

  GtkWidget *se_poll =
      x3d_slider_entry_new(1, 10, 1, 3, "s", &g_dmn.sc_polling_interval);
  GtkWidget *se_load =
      x3d_slider_entry_new(10, 90, 1, 50, "%", &g_dmn.sc_load_threshold);
  const char *dets[] = {"Strict", "Loose", NULL};
  const char *falls[] = {"default", "cache", "frequency", NULL};
  const char *states[] = {"default", "auto", "manual", NULL};

  g_dmn.dd_detection_level =
      GTK_DROP_DOWN(gtk_drop_down_new_from_strings(dets));
  g_dmn.dd_fallback_profile =
      GTK_DROP_DOWN(gtk_drop_down_new_from_strings(falls));
  g_dmn.dd_daemon_state = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(states));

  gtk_box_append(body, x3d_form_row_new("Polling Interval",
                                        "Seconds between workload checks (1–10)",
                                        se_poll));
  gtk_box_append(body,
                 x3d_form_row_new("Load Threshold",
                                  "CPU % to trigger throughput mode (10–90)",
                                  se_load));
  gtk_box_append(body,
                 x3d_form_row_new("Detection Level",
                                  "Strict (Steam/Gamemode) or Loose",
                                  GTK_WIDGET(g_dmn.dd_detection_level)));
  gtk_box_append(body,
                 x3d_form_row_new("Fallback Profile",
                                  "Baseline profile when idle",
                                  GTK_WIDGET(g_dmn.dd_fallback_profile)));
  gtk_box_append(
      body, x3d_form_row_new("State",
                             "Daemon orchestration mode (default / auto / manual)",
                             GTK_WIDGET(g_dmn.dd_daemon_state)));

  GtkWidget *tb_watch = x3d_toggle_new("Watch Mode");
  GtkWidget *tb_ebpf = x3d_toggle_new("eBPF Scheduler");
  g_dmn.tb_watch_mode = GTK_TOGGLE_BUTTON(tb_watch);
  g_dmn.tb_ebpf_enable = GTK_TOGGLE_BUTTON(tb_ebpf);

  GtkWidget *titems[] = {tb_watch, tb_ebpf};
  x3d_staged_widget_set_grid_items(g_dmn.stg_daemon_behavior, titems, 2);

  x3d_staged_widget_watch_control(g_dmn.stg_daemon_behavior,
                                  GTK_WIDGET(g_dmn.sc_polling_interval));
  x3d_staged_widget_watch_control(g_dmn.stg_daemon_behavior,
                                  GTK_WIDGET(g_dmn.sc_load_threshold));
  x3d_staged_widget_watch_control(g_dmn.stg_daemon_behavior,
                                  GTK_WIDGET(g_dmn.dd_detection_level));
  x3d_staged_widget_watch_control(g_dmn.stg_daemon_behavior,
                                  GTK_WIDGET(g_dmn.dd_fallback_profile));
  x3d_staged_widget_watch_control(g_dmn.stg_daemon_behavior,
                                  GTK_WIDGET(g_dmn.dd_daemon_state));
  x3d_staged_widget_watch_control(g_dmn.stg_daemon_behavior, tb_watch);
  x3d_staged_widget_watch_control(g_dmn.stg_daemon_behavior, tb_ebpf);
  gtk_box_append(page, stg);

  g_signal_connect(g_dmn.stg_daemon_behavior, "default",
                   G_CALLBACK(on_daemon_default), NULL);
  g_signal_connect(g_dmn.stg_daemon_behavior, "cancel",
                   G_CALLBACK(on_daemon_cancel), NULL);
  g_signal_connect(g_dmn.stg_daemon_behavior, "apply",
                   G_CALLBACK(on_daemon_apply), NULL);

  GtkWidget *imm = x3d_immediate_widget_new("LIFECYCLE MODES");
  static const char *const lc_lbls[5] = {"Wake", "Sleep", "Stop", "Enable",
                                         "Autostart"};
  static const char *const lc_acts[5] = {"wake", "sleep", "stop", "enable",
                                         "autostart"};
  GtkWidget *lc_btns[5];
  for (int i = 0; i < 5; i++) {
    lc_btns[i] = gtk_button_new_with_label(lc_lbls[i]);
    gtk_widget_add_css_class(lc_btns[i], "x3d-toggle-pill");
    g_object_set_data(G_OBJECT(lc_btns[i]), "x3d-lc-act",
                      (gpointer)lc_acts[i]);
    g_signal_connect(lc_btns[i], "clicked",
                     G_CALLBACK(on_lifecycle_btn_clicked), NULL);
  }
  x3d_immediate_widget_set_grid_items(X3D_IMMEDIATE_WIDGET(imm), lc_btns, 5);
  gtk_box_append(page, imm);

  GtkWidget *tb_sysd = NULL;
  gtk_box_append(
      page, x3d_live_toggle_row_new(
                "Systemd Unit / Service Boot Persistence",
                "Enable or disable x3d-toggle.service automatic boot start",
                &tb_sysd));
  g_dmn.tb_systemd_persist = GTK_TOGGLE_BUTTON(tb_sysd);
  g_signal_connect(g_dmn.tb_systemd_persist, "toggled",
                   G_CALLBACK(on_systemd_persist_toggled), NULL);

  GtkWidget *tb_adv = NULL;
  gtk_box_append(
      page,
      x3d_live_toggle_row_new(
          "Advanced Options",
          "Exposes Advanced Configuration (including AOD Controls when WMI is present)",
          &tb_adv));
  g_dmn.tb_adv_options = GTK_TOGGLE_BUTTON(tb_adv);
  g_signal_connect(g_dmn.tb_adv_options, "toggled",
                   G_CALLBACK(on_adv_options_toggled), NULL);

  x3d_widgets_tree_connect_dirty_notify(g_dmn.root, ctx->notify_dirty_cb,
                                        ctx->notify_dirty_data);
  return g_dmn.root;
}

static gboolean mod_daemon_has_dirty(void) {
  return x3d_widgets_tree_has_dirty(g_dmn.root);
}
static gboolean mod_daemon_has_staged(void) {
  return x3d_widgets_tree_has_staged(g_dmn.root);
}
static gboolean mod_daemon_apply_all(void) {
  return x3d_widgets_tree_apply_dirty(g_dmn.root);
}
static void mod_daemon_cancel_all(void) {
  x3d_widgets_tree_cancel_dirty(g_dmn.root);
}
static void mod_daemon_default_all(void) {
  x3d_widgets_tree_default_all(g_dmn.root);
}

const X3dPageModule *gui_daemon_get_module(void) {
  static const X3dPageModule k_mod = {
      .id = "daemon_settings",
      .title = "Daemon Settings",
      .is_subnav = FALSE,
      .initially_hidden = FALSE,
      .divider_after = FALSE,
      .build = mod_daemon_build,
      .refresh = mod_daemon_refresh,
      .activate = NULL,
      .deactivate = NULL,
      .has_dirty = mod_daemon_has_dirty,
      .has_staged = mod_daemon_has_staged,
      .apply_all = mod_daemon_apply_all,
      .cancel_all = mod_daemon_cancel_all,
      .default_all = mod_daemon_default_all,
      .on_interval_changed = NULL,
      .highlight_hw_mode = NULL,
  };
  return &k_mod;
}
