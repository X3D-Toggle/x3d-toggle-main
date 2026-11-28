/* Quarantined Phase 6 Feature Addon Stubs Implementation
 * `gui_stubs.c`
 */

#include "gui_stubs.h"
#include <stdio.h>

static X3dStubState g_stubs;
static AdwToastOverlay *g_toast_overlay = NULL;
static guint g_pinned_poll_id = 0;
static GtkWidget *g_pinned_lbl_status = NULL;

void x3d_stubs_set_toast_overlay(AdwToastOverlay *overlay) {
  g_toast_overlay = overlay;
}

void x3d_stubs_toast(const char *msg) {
  if (!g_toast_overlay || !msg)
    return;
  AdwToast *t = adw_toast_new(msg);
  adw_toast_set_timeout(t, 3);
  adw_toast_overlay_add_toast(g_toast_overlay, t);
}

X3dStubState *x3d_stubs_get(void) { return &g_stubs; }

static gboolean probe_systemd_boot_enabled(void) {
  gint exit_status = 1;
  char *argv[] = {(char *)"/usr/bin/systemctl", (char *)"is-enabled",
                  (char *)"--quiet", (char *)"x3d-toggle.service", NULL};
  if (g_spawn_sync(NULL, argv, NULL,
                   G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                   NULL, NULL, NULL, NULL, &exit_status, NULL)) {
    return g_spawn_check_wait_status(exit_status, NULL);
  }
  return FALSE;
}

void x3d_stubs_init(X3dGuiIpc *ipc) {
  memset(&g_stubs, 0, sizeof(g_stubs));

  for (int i = 0; i < 64; i++)
    g_stubs.floor_freq_khz[i] = 3000000;

  g_stubs.systemd_boot_persist = probe_systemd_boot_enabled();

  g_stubs.process_rules = g_ptr_array_new_with_free_func(g_free);
  g_ptr_array_add(g_stubs.process_rules,
                  g_strdup("discord -> Frequency CCD (Stub)"));
  g_ptr_array_add(g_stubs.process_rules,
                  g_strdup("obs -> Frequency CCD (Stub)"));

  g_stubs.saved_profiles = g_ptr_array_new_with_free_func(g_free);
  g_ptr_array_add(g_stubs.saved_profiles, g_strdup("hardcore-gaming"));
  g_ptr_array_add(g_stubs.saved_profiles, g_strdup("llm-compute"));
  g_ptr_array_add(g_stubs.saved_profiles, g_strdup("silent-desktop"));

  x3d_stub_hysteresis_reset_defaults();
  x3d_stub_network_addons_reset_defaults(ipc);
  x3d_stub_kernel_addons_reset_defaults();
  x3d_stub_kde_sni_reset_defaults();
  x3d_stub_desktop_widgets_reset_defaults();
}

void x3d_stubs_shutdown(void) {
  if (g_pinned_poll_id != 0) {
    g_source_remove(g_pinned_poll_id);
    g_pinned_poll_id = 0;
  }
  if (g_stubs.pinned_desktop_win) {
    gtk_window_destroy(g_stubs.pinned_desktop_win);
    g_stubs.pinned_desktop_win = NULL;
  }
  if (g_stubs.process_rules) {
    g_ptr_array_unref(g_stubs.process_rules);
    g_stubs.process_rules = NULL;
  }
  if (g_stubs.saved_profiles) {
    g_ptr_array_unref(g_stubs.saved_profiles);
    g_stubs.saved_profiles = NULL;
  }
}

void x3d_stub_cpu_save_profile(GtkWindow *parent) {
  (void)parent;
  x3d_stubs_toast("Stub: Save Profile queued for Phase 6 Profile Manager wiring");
}

void x3d_stub_cpu_load_profile(GtkWindow *parent) {
  (void)parent;
  x3d_stubs_toast("Stub: Load Profile queued for Phase 6 Profile Manager wiring");
}

void x3d_stub_apply_dynamic_epp(gboolean enabled) {
  g_stubs.dynamic_epp_staged = enabled;
}

void x3d_stub_apply_floor_freq(void) {
  x3d_stubs_toast(
      "Stub: Per-core amd_pstate_floor_freq staged (Phase 6 IPC pending)");
}

void x3d_stub_toggle_systemd_persistence(gboolean enabled) {
  g_stubs.systemd_boot_persist = enabled;
  x3d_stubs_toast(enabled
                      ? "Stub: Systemd unit boot persistence -> ENABLED"
                      : "Stub: Systemd unit boot persistence -> DISABLED");
}

void x3d_stub_process_rule_add(const char *proc_name, const char *policy) {
  if (!proc_name || !proc_name[0] || !g_stubs.process_rules)
    return;
  g_ptr_array_add(g_stubs.process_rules,
                  g_strdup_printf("%s -> %s", proc_name,
                                  policy ? policy : "Cache CCD"));
}

void x3d_stub_process_rule_remove(guint idx) {
  if (!g_stubs.process_rules || idx >= g_stubs.process_rules->len)
    return;
  g_ptr_array_remove_index(g_stubs.process_rules, idx);
}

void x3d_stub_profile_mgmt_action(const char *action,
                                  const char *profile_name) {
  const char *name = (profile_name && profile_name[0]) ? profile_name : "default";
  if (g_strcmp0(action, "save") == 0 || g_strcmp0(action, "overwrite") == 0) {
    gboolean exists = FALSE;
    for (guint i = 0; i < g_stubs.saved_profiles->len; i++) {
      if (g_strcmp0(g_ptr_array_index(g_stubs.saved_profiles, i), name) == 0) {
        exists = TRUE;
        break;
      }
    }
    if (!exists)
      g_ptr_array_add(g_stubs.saved_profiles, g_strdup(name));
  } else if (g_strcmp0(action, "delete") == 0) {
    for (guint i = 0; i < g_stubs.saved_profiles->len; i++) {
      if (g_strcmp0(g_ptr_array_index(g_stubs.saved_profiles, i), name) == 0) {
        g_ptr_array_remove_index(g_stubs.saved_profiles, i);
        break;
      }
    }
  }
  gchar *msg = g_strdup_printf("Stub: Profile '%s' (%s) — Phase 6 IPC pending",
                               action ? action : "op", name);
  x3d_stubs_toast(msg);
  g_free(msg);
}

void x3d_stub_hysteresis_reset_defaults(void) {
  g_stubs.hyst_start_stop = 3;
  g_stubs.hyst_bearing = 50.0;
  g_stubs.hyst_up_threshold = 65;
  g_stubs.hyst_down_threshold = 35;
  g_stubs.hyst_cooldown_sec = 5;
  g_stubs.hyst_sample_ms = 250;
}

void x3d_stub_hysteresis_apply(void) {
  x3d_stubs_toast("Stub: Load / Polling Hysteresis parameters staged (Phase 6)");
}

void x3d_stub_network_addons_reset_defaults(X3dGuiIpc *ipc) {
  (void)ipc;
  char cur[32] = {0};
  if (!x3d_ipc_advanced_get("net.ipv4.tcp_congestion_control", cur,
                            sizeof(cur))) {
    snprintf(cur, sizeof(cur), "cubic");
  }
  snprintf(g_stubs.tcp_congestion, sizeof(g_stubs.tcp_congestion), "%s",
           cur[0] ? cur : "cubic");
  g_stubs.net_buffer_preset = 0;
  g_stubs.tcp_slow_start_idle_off = FALSE;
  g_stubs.tcp_ecn_on = FALSE;
  g_stubs.tcp_5g_keepalive = FALSE;
}

void x3d_stub_network_addons_apply(void) {
  /* Net-new TCP/buffer tunables stubbed for Phase 6 sysctl wiring */
}

void x3d_stub_kernel_addons_reset_defaults(void) {
  g_stubs.pcie_aspm_idx = 0;
  g_stubs.cpu_dma_latency_guard = FALSE;
}

void x3d_stub_kernel_addons_apply(void) {
  /* PCIe ASPM & /dev/cpu_dma_latency stubbed for Phase 6 daemon wiring */
}

void x3d_stub_kde_sni_reset_defaults(void) {
  g_stubs.kde_sni_enabled = FALSE;
  g_stubs.kde_sni_style_idx = 0;
  g_stubs.kde_sni_show_badge = TRUE;
  g_stubs.kde_sni_rich_tooltip = TRUE;
}

void x3d_stub_kde_sni_apply(void) {
  x3d_stubs_toast("Stub: KDE6 SNI tray addon settings staged (Phase 6)");
}

void x3d_stub_desktop_widgets_reset_defaults(void) {
  g_stubs.widget_gtk_pinned = FALSE;
  g_stubs.widget_kde_plasmoid = FALSE;
  g_stubs.widget_mod_switcher = TRUE;
  g_stubs.widget_mod_telemetry = TRUE;
  g_stubs.widget_mod_aod = FALSE;
}

static gboolean pinned_desktop_poll_cb(gpointer data) {
  (void)data;
  if (!g_stubs.pinned_desktop_win || !g_pinned_lbl_status) {
    g_pinned_poll_id = 0;
    return G_SOURCE_REMOVE;
  }

  char resp[1024] = {0};
  char vcache[32] = "N/A", dstate[32] = "STOPPED";
  char ccd0[32] = "N/A", ccd1[32] = "N/A";
  char ppt[64] = "N/A", tdc[64] = "N/A";
  if (x3d_ipc_send("STATUS", resp, sizeof(resp)) == 0 && resp[0] &&
      strcmp(resp, "ERR") != 0) {
    x3d_ipc_kv_get(resp, "VCACHE", '|', vcache, sizeof(vcache));
    x3d_ipc_kv_get(resp, "DAEMON", '|', dstate, sizeof(dstate));
    x3d_ipc_kv_get(resp, "CCD0", '|', ccd0, sizeof(ccd0));
    x3d_ipc_kv_get(resp, "CCD1", '|', ccd1, sizeof(ccd1));
    x3d_ipc_kv_get(resp, "PPT", '|', ppt, sizeof(ppt));
    x3d_ipc_kv_get(resp, "TDC", '|', tdc, sizeof(tdc));
  }

  GString *s = g_string_new("<span font_family=\"monospace\">");
  if (g_stubs.widget_mod_telemetry) {
    g_string_append_printf(s, "<b>Mode:</b> %s  •  <b>Daemon:</b> %s\n", vcache,
                           dstate);
    g_string_append_printf(s, "<b>CCD0:</b> %s  |  <b>CCD1:</b> %s\n", ccd0,
                           ccd1);
  }
  if (g_stubs.widget_mod_aod) {
    g_string_append_printf(s, "<b>PPT:</b> %s  |  <b>TDC:</b> %s", ppt, tdc);
  }
  g_string_append(s, "</span>");
  gtk_label_set_markup(GTK_LABEL(g_pinned_lbl_status), s->str);
  g_string_free(s, TRUE);
  return G_SOURCE_CONTINUE;
}

static void on_pinned_mode_btn_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  const char *mode = (const char *)data;
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "SET_MODE %s", mode);
  x3d_ipc_cmd_ok(cmd);
}

static gboolean on_pinned_win_close_req(GtkWindow *win, gpointer data) {
  (void)win;
  (void)data;
  if (g_pinned_poll_id != 0) {
    g_source_remove(g_pinned_poll_id);
    g_pinned_poll_id = 0;
  }
  g_pinned_lbl_status = NULL;
  g_stubs.pinned_desktop_win = NULL;
  g_stubs.widget_gtk_pinned = FALSE;
  return FALSE;
}

void x3d_stub_sync_pinned_window(GtkWindow *main_win, X3dGuiIpc *ipc) {
  (void)main_win;
  if (!g_stubs.widget_gtk_pinned) {
    if (g_pinned_poll_id != 0) {
      g_source_remove(g_pinned_poll_id);
      g_pinned_poll_id = 0;
    }
    if (g_stubs.pinned_desktop_win) {
      gtk_window_destroy(g_stubs.pinned_desktop_win);
      g_stubs.pinned_desktop_win = NULL;
      g_pinned_lbl_status = NULL;
    }
    return;
  }

  if (g_stubs.pinned_desktop_win) {
    pinned_desktop_poll_cb(ipc);
    gtk_window_present(g_stubs.pinned_desktop_win);
    return;
  }

  GtkWidget *win = gtk_window_new();
  gtk_window_set_title(GTK_WINDOW(win), "X3D Pinned Desktop Widget");
  gtk_window_set_default_size(GTK_WINDOW(win), 320, -1);
  gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
  gtk_window_set_decorated(GTK_WINDOW(win), FALSE);

  GtkWidget *handle = gtk_window_handle_new();
  GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_add_css_class(root, "x3d-pinned-desktop");

  GtkWidget *hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *title = gtk_label_new("X3D DESKTOP WIDGET (STUB)");
  gtk_widget_add_css_class(title, "x3d-widget-title");
  gtk_widget_set_hexpand(title, TRUE);
  gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
  GtkWidget *close_btn = gtk_button_new_with_label("×");
  g_signal_connect_swapped(close_btn, "clicked",
                           G_CALLBACK(gtk_window_close), win);
  gtk_box_append(GTK_BOX(hdr), title);
  gtk_box_append(GTK_BOX(hdr), close_btn);
  gtk_box_append(GTK_BOX(root), hdr);

  g_pinned_lbl_status = gtk_label_new("Polling...");
  gtk_widget_add_css_class(g_pinned_lbl_status, "x3d-value");
  gtk_label_set_xalign(GTK_LABEL(g_pinned_lbl_status), 0.0f);
  gtk_box_append(GTK_BOX(root), g_pinned_lbl_status);

  if (g_stubs.widget_mod_switcher) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_set_homogeneous(GTK_BOX(row), TRUE);
    static const char *const modes[] = {"cache", "frequency", "dual", "auto"};
    static const char *const labels[] = {"Cache", "Freq", "Dual", "Auto"};
    for (int i = 0; i < 4; i++) {
      GtkWidget *b = gtk_button_new_with_label(labels[i]);
      gtk_widget_add_css_class(b, "x3d-toggle-pill");
      g_signal_connect(b, "clicked", G_CALLBACK(on_pinned_mode_btn_clicked),
                       (gpointer)modes[i]);
      gtk_box_append(GTK_BOX(row), b);
    }
    gtk_box_append(GTK_BOX(root), row);
  }

  gtk_window_handle_set_child(GTK_WINDOW_HANDLE(handle), root);
  gtk_window_set_child(GTK_WINDOW(win), handle);
  g_signal_connect(win, "close-request", G_CALLBACK(on_pinned_win_close_req),
                   NULL);

  g_stubs.pinned_desktop_win = GTK_WINDOW(win);
  pinned_desktop_poll_cb(ipc);
  g_pinned_poll_id = g_timeout_add_seconds(2, pinned_desktop_poll_cb, ipc);
  gtk_window_present(GTK_WINDOW(win));
}

void x3d_stub_desktop_widgets_apply(GtkWindow *main_win, X3dGuiIpc *ipc) {
  x3d_stub_sync_pinned_window(main_win, ipc);
  if (g_stubs.widget_kde_plasmoid) {
    x3d_stubs_toast("Stub: KDE6 Plasmoid registration queued for Phase 6 "
                    "(kdialog addon / kpackagetool6)");
  } else {
    x3d_stubs_toast("Desktop Widget pathways updated");
  }
}

void x3d_stub_toggle_latency_timer(gboolean enabled) {
  g_stubs.audit_latency_timer = enabled;
  x3d_stubs_toast(
      enabled ? "Stub: Switching Latency Timer armed (Phase 6 backend pending)"
              : "Stub: Switching Latency Timer disarmed");
}

gchar *x3d_stub_run_dry_run_simulation(const X3dGuiIpc *ipc,
                                       const char *target_action) {
  char resp[1024] = {0};
  char vcache[32] = "N/A", dstate[32] = "STOPPED", drv[32] = "N/A",
       epp[32] = "N/A";
  const char *ipc_s = "SOCKET OFFLINE";
  if (x3d_ipc_send("STATUS", resp, sizeof(resp)) == 0 && resp[0] &&
      strcmp(resp, "ERR") != 0) {
    ipc_s = "SOCKET ONLINE";
    x3d_ipc_kv_get(resp, "VCACHE", '|', vcache, sizeof(vcache));
    x3d_ipc_kv_get(resp, "DAEMON", '|', dstate, sizeof(dstate));
    x3d_ipc_kv_get(resp, "DRIVER", '|', drv, sizeof(drv));
    x3d_ipc_kv_get(resp, "EPP", '|', epp, sizeof(epp));
  }
  return g_strdup_printf(
      "[DRY RUN SIMULATION — NO SYSFS / CGROUP WRITES COMMITTED]\n"
      "─────────────────────────────────────────────────────────\n"
      "Target Action   : %s\n"
      "Active Mode     : %s\n"
      "Daemon State    : %s (%s)\n"
      "Driver / EPP    : %s / %s\n"
      "Capabilities    : 0x%06llx\n"
      "Validation      : PASS (Stub — full dry-run validator scheduled in Phase 6)\n",
      target_action ? target_action : "mode cache", vcache, dstate, ipc_s, drv,
      epp, (unsigned long long)x3d_gui_ipc_get_caps(ipc));
}

void x3d_stub_emergency_override_execute(X3dGuiIpc *ipc) {
  (void)ipc;
  if (g_stubs.saved_profiles)
    g_ptr_array_set_size(g_stubs.saved_profiles, 0);
  if (g_stubs.process_rules)
    g_ptr_array_set_size(g_stubs.process_rules, 0);

  char *args[] = {(char *)"/usr/bin/x3d-toggle", (char *)"reset", NULL};
  x3d_ipc_spawn(args, NULL);
  x3d_stubs_toast(
      "EMERGENCY OVERRIDE executed: driver control restored & profiles cleared");
}

int unit_active(void) { return 0; }

void daemon_failsafe(int sig) { (void)sig; }
