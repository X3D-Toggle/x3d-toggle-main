/* Dashboard Page Slice Implementation
 * `pages/gui_dashboard.c`
 * Polls x3d-daemon STATUS via gui_ipc and renders the 2x3 Dashboard cards.
 */

#include "gui_dashboard.h"
#include "../gui_popups.h"
#include "gui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char daemon_state[64];
  char ipc_status[64];
  char ebpf_status[64];
  char sched_mode[64];

  char vcache_mode[32];
  char boost_mode[32];
  char ccd_state[64];
  char ccd0_state[32];
  char ccd1_state[32];

  char driver_mode[64];
  char epp_profile[64];
  char dynamic_epp_status[64];
  char governor[64];
  char smt_status[64];
  char platform[64];

  char ppt_str[64];
  char tdc_str[64];
  char edc_str[64];
  char mem_str[64];
  char aod_status[64];
} X3dDashboardSnapshot;

static struct {
  X3dPageCtx ctx;
  GtkWidget *root;
  GtkWidget *lbl_daemon;
  GtkWidget *lbl_hardware;
  GtkWidget *lbl_system;
  GtkWidget *lbl_aod;
  int daemon_retry_cooldown;
} g_dash;

static void fill_offline_dashboard(X3dDashboardSnapshot *out) {
  snprintf(out->daemon_state, sizeof(out->daemon_state), "STOPPED");
  snprintf(out->ipc_status, sizeof(out->ipc_status), "SOCKET OFFLINE");
  snprintf(out->ebpf_status, sizeof(out->ebpf_status), "INACTIVE");
  snprintf(out->sched_mode, sizeof(out->sched_mode), "INACTIVE");
  snprintf(out->vcache_mode, sizeof(out->vcache_mode), "N/A");
  snprintf(out->boost_mode, sizeof(out->boost_mode), "N/A");
  snprintf(out->ccd_state, sizeof(out->ccd_state), "N/A");
  snprintf(out->ccd0_state, sizeof(out->ccd0_state), "N/A");
  snprintf(out->ccd1_state, sizeof(out->ccd1_state), "N/A");
  snprintf(out->driver_mode, sizeof(out->driver_mode), "N/A");
  snprintf(out->epp_profile, sizeof(out->epp_profile), "N/A");
  snprintf(out->dynamic_epp_status, sizeof(out->dynamic_epp_status), "N/A");
  snprintf(out->governor, sizeof(out->governor), "N/A");
  snprintf(out->smt_status, sizeof(out->smt_status), "N/A");
  snprintf(out->platform, sizeof(out->platform), "N/A");
  snprintf(out->ppt_str, sizeof(out->ppt_str), "N/A");
  snprintf(out->tdc_str, sizeof(out->tdc_str), "N/A");
  snprintf(out->edc_str, sizeof(out->edc_str), "N/A");
  snprintf(out->mem_str, sizeof(out->mem_str), "N/A");
  snprintf(out->aod_status, sizeof(out->aod_status), "OFFLINE");
}

static void poll_dashboard_snapshot(X3dGuiIpc *ipc, X3dDashboardSnapshot *out) {
  memset(out, 0, sizeof(*out));
  fill_offline_dashboard(out);
  if (!ipc)
    return;

  char resp[1024] = {0};
  if (x3d_ipc_send("STATUS", resp, sizeof(resp)) != 0 || resp[0] == '\0' ||
      strcmp(resp, "ERR") == 0) {
    if (g_dash.daemon_retry_cooldown <= 0) {
      g_dash.daemon_retry_cooldown = 10;
      char *argv[] = {(char *)"/usr/bin/systemctl", (char *)"start",
                      (char *)"x3d-toggle.service", NULL};
      x3d_ipc_spawn(argv, NULL);
    } else {
      g_dash.daemon_retry_cooldown--;
    }
    x3d_gui_ipc_set_aod_supported(ipc, FALSE);
    return;
  }

  g_dash.daemon_retry_cooldown = 0;
  snprintf(out->ipc_status, sizeof(out->ipc_status), "SOCKET ONLINE");

  x3d_ipc_kv_get(resp, "DAEMON", '|', out->daemon_state,
                 sizeof(out->daemon_state));
  x3d_ipc_kv_get(resp, "EBPF", '|', out->ebpf_status, sizeof(out->ebpf_status));
  x3d_ipc_kv_get(resp, "SCHED", '|', out->sched_mode, sizeof(out->sched_mode));
  x3d_ipc_kv_get(resp, "VCACHE", '|', out->vcache_mode,
                 sizeof(out->vcache_mode));
  x3d_ipc_kv_get(resp, "BOOST", '|', out->boost_mode, sizeof(out->boost_mode));
  x3d_ipc_kv_get(resp, "CCD", '|', out->ccd_state, sizeof(out->ccd_state));
  x3d_ipc_kv_get(resp, "CCD0", '|', out->ccd0_state, sizeof(out->ccd0_state));
  x3d_ipc_kv_get(resp, "CCD1", '|', out->ccd1_state, sizeof(out->ccd1_state));
  x3d_ipc_kv_get(resp, "DRIVER", '|', out->driver_mode,
                 sizeof(out->driver_mode));
  x3d_ipc_kv_get(resp, "EPP", '|', out->epp_profile, sizeof(out->epp_profile));
  x3d_ipc_kv_get(resp, "DYNEPP", '|', out->dynamic_epp_status,
                 sizeof(out->dynamic_epp_status));
  x3d_ipc_kv_get(resp, "GOV", '|', out->governor, sizeof(out->governor));
  x3d_ipc_kv_get(resp, "SMT", '|', out->smt_status, sizeof(out->smt_status));
  x3d_ipc_kv_get(resp, "PLAT", '|', out->platform, sizeof(out->platform));
  x3d_ipc_kv_get(resp, "PPT", '|', out->ppt_str, sizeof(out->ppt_str));
  x3d_ipc_kv_get(resp, "TDC", '|', out->tdc_str, sizeof(out->tdc_str));
  x3d_ipc_kv_get(resp, "EDC", '|', out->edc_str, sizeof(out->edc_str));
  x3d_ipc_kv_get(resp, "MEM", '|', out->mem_str, sizeof(out->mem_str));

  char caps_s[32] = {0};
  if (x3d_ipc_kv_get(resp, "CAPS", '|', caps_s, sizeof(caps_s))) {
    uint64_t daemon_caps = strtoull(caps_s, NULL, 16);
    x3d_gui_ipc_set_caps(ipc, daemon_caps);
  }

  char aod_s[8] = {0};
  gboolean aod_on = x3d_ipc_kv_get(resp, "AOD_SUP", '|', aod_s, sizeof(aod_s)) &&
                    atoi(aod_s) != 0;
  x3d_gui_ipc_set_aod_supported(ipc, aod_on);
  snprintf(out->aod_status, sizeof(out->aod_status), "%s",
           aod_on ? "ONLINE" : "OFFLINE");
}

static void render_dashboard(const X3dDashboardSnapshot *s) {
  if (!g_dash.lbl_daemon || !s)
    return;

  gchar *m_daemon = g_markup_printf_escaped(
      "<span font_family=\"monospace\">"
      "  <b>Daemon Status:</b>  %s\n"
      "  <b>IPC Status:</b>     %s\n"
      "  <b>eBPF Health:</b>    %s\n"
      "  <b>Scheduler Mode:</b> %s"
      "</span>",
      s->daemon_state, s->ipc_status, s->ebpf_status, s->sched_mode);

  gchar *m_hw = g_markup_printf_escaped(
      "<span font_family=\"monospace\">"
      "  <b>v-Cache Mode:</b>   %s\n"
      "  <b>Boost Mode:</b>     %s\n"
      "  <b>CCD State:</b>      %s\n"
      "  <b>CCD0 State:</b>     %s\n"
      "  <b>CCD1 State:</b>     %s"
      "</span>",
      s->vcache_mode, s->boost_mode, s->ccd_state, s->ccd0_state,
      s->ccd1_state);

  gchar *m_sys = g_markup_printf_escaped(
      "<span font_family=\"monospace\">"
      "  <b>Driver Mode:</b>    %s\n"
      "  <b>EPP Profile:</b>    %s\n"
      "  <b>Dynamic EPP:</b>    %s\n"
      "  <b>Governor:</b>       %s\n"
      "  <b>SMT Status:</b>     %s\n"
      "  <b>Platform:</b>       %s"
      "</span>",
      s->driver_mode, s->epp_profile, s->dynamic_epp_status, s->governor,
      s->smt_status, s->platform);

  gchar *m_aod = g_markup_printf_escaped(
      "<span font_family=\"monospace\">"
      "  <b>PPT:</b>            %s\n"
      "  <b>TDC:</b>            %s\n"
      "  <b>EDC:</b>            %s\n"
      "  <b>Mem Clock:</b>      %s\n"
      "  <b>AOD Status:</b>     %s"
      "</span>",
      s->ppt_str, s->tdc_str, s->edc_str, s->mem_str, s->aod_status);

  gtk_label_set_markup(GTK_LABEL(g_dash.lbl_daemon), m_daemon);
  gtk_label_set_markup(GTK_LABEL(g_dash.lbl_hardware), m_hw);
  gtk_label_set_markup(GTK_LABEL(g_dash.lbl_system), m_sys);
  gtk_label_set_markup(GTK_LABEL(g_dash.lbl_aod), m_aod);

  g_free(m_daemon);
  g_free(m_hw);
  g_free(m_sys);
  g_free(m_aod);
}

static void on_dash_daemon_activated(X3dCurrentWidget *w, gpointer data) {
  (void)w;
  (void)data;
  if (g_dash.ctx.request_navigate)
    g_dash.ctx.request_navigate(g_dash.ctx.frame, "daemon_settings", TRUE);
}

static void on_dash_hw_activated(X3dCurrentWidget *w, gpointer data) {
  (void)w;
  (void)data;
  if (g_dash.ctx.request_navigate)
    g_dash.ctx.request_navigate(g_dash.ctx.frame, "hardware_modes_view", TRUE);
}

static void on_dash_sys_activated(X3dCurrentWidget *w, gpointer data) {
  (void)w;
  (void)data;
  if (g_dash.ctx.request_navigate)
    g_dash.ctx.request_navigate(g_dash.ctx.frame, "system_profile_view", TRUE);
}

static void on_dash_aod_activated(X3dCurrentWidget *w, gpointer data) {
  (void)w;
  (void)data;
  gui_popups_show_telemetry(g_dash.ctx.view->main_window, g_dash.ctx.ipc,
                            X3D_TELEM_AOD, NULL);
}

static GtkWidget *mod_dashboard_build(const X3dPageCtx *ctx) {
  g_dash.ctx = *ctx;

  GtkWidget *dash_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
  gtk_widget_set_hexpand(dash_box, TRUE);
  gtk_widget_set_vexpand(dash_box, TRUE);

  GtkWidget *hero = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_add_css_class(hero, "x3d-hero");
  gtk_widget_set_hexpand(hero, TRUE);
  gtk_widget_set_vexpand(hero, FALSE);
  gtk_widget_set_size_request(hero, -1, 160);
  gtk_box_append(GTK_BOX(dash_box), hero);

  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 16);
  gtk_widget_set_hexpand(grid, TRUE);
  gtk_widget_set_vexpand(grid, TRUE);

  GtkWidget *w_daemon = x3d_current_widget_new("DAEMON STATUS", TRUE);
  gtk_widget_set_vexpand(w_daemon, TRUE);
  gtk_widget_set_valign(w_daemon, GTK_ALIGN_FILL);
  g_dash.lbl_daemon = gtk_label_new("Connecting to daemon...");
  gtk_widget_add_css_class(g_dash.lbl_daemon, "x3d-value");
  gtk_label_set_xalign(GTK_LABEL(g_dash.lbl_daemon), 0.0f);
  gtk_box_append(x3d_current_widget_get_body(X3D_CURRENT_WIDGET(w_daemon)),
                 g_dash.lbl_daemon);
  gtk_grid_attach(GTK_GRID(grid), w_daemon, 0, 0, 1, 1);

  GtkWidget *w_hw = x3d_current_widget_new("HARDWARE STATUS", TRUE);
  gtk_widget_set_vexpand(w_hw, TRUE);
  gtk_widget_set_valign(w_hw, GTK_ALIGN_FILL);
  g_dash.lbl_hardware = gtk_label_new("Probing CCD topology...");
  gtk_widget_add_css_class(g_dash.lbl_hardware, "x3d-value");
  gtk_label_set_xalign(GTK_LABEL(g_dash.lbl_hardware), 0.0f);
  gtk_box_append(x3d_current_widget_get_body(X3D_CURRENT_WIDGET(w_hw)),
                 g_dash.lbl_hardware);
  gtk_grid_attach(GTK_GRID(grid), w_hw, 1, 0, 1, 1);

  GtkWidget *w_sys = x3d_current_widget_new("SYSTEM PROFILE", TRUE);
  gtk_widget_set_vexpand(w_sys, TRUE);
  gtk_widget_set_valign(w_sys, GTK_ALIGN_FILL);
  g_dash.lbl_system = gtk_label_new("Reading amd_pstate...");
  gtk_widget_add_css_class(g_dash.lbl_system, "x3d-value");
  gtk_label_set_xalign(GTK_LABEL(g_dash.lbl_system), 0.0f);
  gtk_box_append(x3d_current_widget_get_body(X3D_CURRENT_WIDGET(w_sys)),
                 g_dash.lbl_system);
  gtk_grid_attach(GTK_GRID(grid), w_sys, 0, 1, 1, 1);

  GtkWidget *w_aod =
      x3d_current_widget_new("POWER & CURRENT LIMITS STATUS", TRUE);
  gtk_widget_set_vexpand(w_aod, TRUE);
  gtk_widget_set_valign(w_aod, GTK_ALIGN_FILL);
  g_dash.lbl_aod = gtk_label_new("Probing SMU / RAPL...");
  gtk_widget_add_css_class(g_dash.lbl_aod, "x3d-value");
  gtk_label_set_xalign(GTK_LABEL(g_dash.lbl_aod), 0.0f);
  gtk_box_append(x3d_current_widget_get_body(X3D_CURRENT_WIDGET(w_aod)),
                 g_dash.lbl_aod);
  gtk_grid_attach(GTK_GRID(grid), w_aod, 1, 1, 1, 1);

  GtkWidget *empty_left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *empty_right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_hexpand(empty_left, TRUE);
  gtk_widget_set_vexpand(empty_left, TRUE);
  gtk_widget_set_hexpand(empty_right, TRUE);
  gtk_widget_set_vexpand(empty_right, TRUE);
  gtk_grid_attach(GTK_GRID(grid), empty_left, 0, 2, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), empty_right, 1, 2, 1, 1);

  gtk_box_append(GTK_BOX(dash_box), grid);
  gtk_stack_add_named(ctx->view->content_stack, dash_box, "dashboard");

  g_signal_connect(w_daemon, "activated", G_CALLBACK(on_dash_daemon_activated),
                   NULL);
  g_signal_connect(w_hw, "activated", G_CALLBACK(on_dash_hw_activated), NULL);
  g_signal_connect(w_sys, "activated", G_CALLBACK(on_dash_sys_activated), NULL);
  g_signal_connect(w_aod, "activated", G_CALLBACK(on_dash_aod_activated), NULL);

  g_dash.root = dash_box;
  return dash_box;
}

static void mod_dashboard_refresh(const X3dPageCtx *ctx) {
  if (ctx)
    g_dash.ctx = *ctx;
  X3dDashboardSnapshot snap;
  poll_dashboard_snapshot(g_dash.ctx.ipc, &snap);
  render_dashboard(&snap);
  if (g_dash.ctx.broadcast_hw_mode)
    g_dash.ctx.broadcast_hw_mode(snap.vcache_mode);
  if (g_dash.ctx.sync_sidebar_gating)
    g_dash.ctx.sync_sidebar_gating(g_dash.ctx.frame);
}

static gboolean mod_dashboard_has_dirty(void) { return FALSE; }
static gboolean mod_dashboard_has_staged(void) { return FALSE; }
static gboolean mod_dashboard_apply_all(void) { return TRUE; }
static void mod_dashboard_cancel_all(void) {}
static void mod_dashboard_default_all(void) {}

const X3dPageModule *gui_dashboard_get_module(void) {
  static const X3dPageModule k_mod = {
      .id = "dashboard",
      .title = "Dashboard",
      .is_subnav = FALSE,
      .initially_hidden = FALSE,
      .divider_after = FALSE,
      .build = mod_dashboard_build,
      .refresh = mod_dashboard_refresh,
      .activate = NULL,
      .deactivate = NULL,
      .has_dirty = mod_dashboard_has_dirty,
      .has_staged = mod_dashboard_has_staged,
      .apply_all = mod_dashboard_apply_all,
      .cancel_all = mod_dashboard_cancel_all,
      .default_all = mod_dashboard_default_all,
      .on_interval_changed = NULL,
      .highlight_hw_mode = NULL,
  };
  return &k_mod;
}
