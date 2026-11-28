/* Quarantined Phase 6 Feature Addon Stubs Header
 * `gui_stubs.h`
 * Holds in-memory state and UI stub handlers for net-new features in Layout.md
 * whose daemon/IPC/packaging backends will be factored in Phase 6.
 */

#ifndef X3D_GUI_STUBS_H
#define X3D_GUI_STUBS_H

#include "gui_ipc.h"
#include <adwaita.h>
#include <gtk/gtk.h>

typedef struct {
  /* 2. CPU Stubs */
  gboolean dynamic_epp_staged;
  int floor_freq_khz[64];

  /* 5. Daemon Settings Stubs */
  gboolean systemd_boot_persist;

  /* 6. Profile Manager Stubs */
  GPtrArray *process_rules; /* strings: "proc_name -> CCD0 (Cache)" */
  GPtrArray *saved_profiles;

  /* 7. Advanced Configuration — Hysteresis Stubs */
  int hyst_start_stop;
  double hyst_bearing;
  int hyst_up_threshold;
  int hyst_down_threshold;
  int hyst_cooldown_sec;
  int hyst_sample_ms;

  /* 7. Advanced Configuration — Net-New Network & Kernel Stubs */
  char tcp_congestion[32];
  guint net_buffer_preset; /* 0 = Stock, 1 = 1 Gbps, 2 = 2.5+ Gbps */
  gboolean tcp_slow_start_idle_off;
  gboolean tcp_ecn_on;
  gboolean tcp_5g_keepalive;
  guint pcie_aspm_idx; /* 0 = default, 1 = performance, 2 = powersave, 3 = powersupersave */
  gboolean cpu_dma_latency_guard;

  /* 7. Extras — KDE6 SNI & Dual-Pathway Desktop Widgets */
  gboolean kde_sni_enabled;
  guint kde_sni_style_idx;
  gboolean kde_sni_show_badge;
  gboolean kde_sni_rich_tooltip;

  gboolean widget_gtk_pinned;
  gboolean widget_kde_plasmoid;
  gboolean widget_mod_switcher;
  gboolean widget_mod_telemetry;
  gboolean widget_mod_aod;
  GtkWindow *pinned_desktop_win;

  /* 8. Developer Options — Audits Stubs */
  gboolean audit_latency_timer;
} X3dStubState;

void x3d_stubs_init(X3dGuiIpc *ipc);
void x3d_stubs_shutdown(void);
X3dStubState *x3d_stubs_get(void);
void x3d_stubs_set_toast_overlay(AdwToastOverlay *overlay);
void x3d_stubs_toast(const char *msg);

/* Individual Stub Operations (Replaced one-by-one in Phase 6) */
void x3d_stub_cpu_save_profile(GtkWindow *parent);
void x3d_stub_cpu_load_profile(GtkWindow *parent);
void x3d_stub_apply_dynamic_epp(gboolean enabled);
void x3d_stub_apply_floor_freq(void);

void x3d_stub_toggle_systemd_persistence(gboolean enabled);

void x3d_stub_process_rule_add(const char *proc_name, const char *policy);
void x3d_stub_process_rule_remove(guint idx);
void x3d_stub_profile_mgmt_action(const char *action, const char *profile_name);

void x3d_stub_hysteresis_reset_defaults(void);
void x3d_stub_hysteresis_apply(void);

void x3d_stub_network_addons_reset_defaults(X3dGuiIpc *ipc);
void x3d_stub_network_addons_apply(void);

void x3d_stub_kernel_addons_reset_defaults(void);
void x3d_stub_kernel_addons_apply(void);

void x3d_stub_kde_sni_reset_defaults(void);
void x3d_stub_kde_sni_apply(void);

void x3d_stub_desktop_widgets_reset_defaults(void);
void x3d_stub_desktop_widgets_apply(GtkWindow *main_win, X3dGuiIpc *ipc);
void x3d_stub_sync_pinned_window(GtkWindow *main_win, X3dGuiIpc *ipc);

void x3d_stub_toggle_latency_timer(gboolean enabled);
gchar *x3d_stub_run_dry_run_simulation(const X3dGuiIpc *ipc,
                                       const char *target_action);
void x3d_stub_emergency_override_execute(X3dGuiIpc *ipc);

/* Linker shims for autonomous CLI modules compiled into x3d-gui */
int unit_active(void);
void daemon_failsafe(int sig);

#endif /* X3D_GUI_STUBS_H */
