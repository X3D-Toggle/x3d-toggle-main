/* Advanced Configuration (incl. AOD Controls) Page Slice Implementation
 * `pages/gui_config.c`
 */

#include "gui_config.h"
#include "../gui_popups.h"
#include "../gui_stubs.h"
#include "gui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int printf_sn(char *buf, size_t size, const char *fmt, ...);

static const char *const k_ssh[3] = {"local", "remote", "enabled"};
static const char *const k_thp[3] = {"always", "madvise", "never"};
static const char *const k_aspm[4] = {"default", "performance", "powersave",
                                      "powersupersave"};

static struct {
  X3dPageCtx ctx;
  GtkWidget *root;
  AodState aod_state;

  /* 7A. AOD Controls */
  X3dStagedWidget *stg_aod;
  GtkWidget *lbl_aod_reboot_badge;
  GtkWidget *lbl_aod_baseline_hint;
  GtkWidget *row_aod_ppt;
  GtkRange *sc_aod_ppt;
  GtkWidget *row_aod_tdc;
  GtkRange *sc_aod_tdc;
  GtkWidget *row_aod_edc;
  GtkRange *sc_aod_edc;
  GtkWidget *row_aod_mem;
  GtkRange *sc_aod_mem;
  GtkWidget *row_aod_smt;
  GtkToggleButton *tb_aod_smt;

  /* 7B. Hysteresis */
  X3dStagedWidget *stg_hysteresis;
  GtkSpinButton *sp_hyst_start_stop;
  GtkRange *sc_hyst_bearing;

  /* 7C. Network */
  X3dStagedWidget *stg_network;
  GtkEditable *ent_net_qdisc;
  GtkDropDown *dd_net_cong;
  GtkDropDown *dd_net_buf_preset;
  GtkSpinButton *sp_net_fastopen;
  GtkToggleButton *tb_net_slow_idle;
  GtkToggleButton *tb_net_ecn;
  GtkToggleButton *tb_net_5g_keepalive;
  GtkToggleButton *tb_net_coalesce;
  GtkSpinButton *sp_net_rp_filter;
  GtkToggleButton *tb_net_source_route;
  GtkEditable *ent_srv_addr;
  GtkEditable *ent_srv_port;
  GtkDropDown *dd_srv_ssh;

  /* 7D. Kernel */
  X3dStagedWidget *stg_kernel;
  GtkToggleButton *tb_sched_bore;
  GtkSpinButton *sp_sched_bit_shift;
  GtkToggleButton *tb_sched_burst_fork;
  GtkSpinButton *sp_sched_slice_us;
  GtkEditable *ent_vm_max_map;
  GtkToggleButton *tb_split_lock;
  GtkToggleButton *tb_nmi_watchdog;
  GtkDropDown *dd_pcie_aspm;
  GtkToggleButton *tb_cpu_dma_lat;
  GtkDropDown *dd_thp_mode;

  /* 7E. Developer Mode */
  GtkToggleButton *tb_dev_mode;
  gboolean suppress_live_toggles;
} g_cfg;

static guint find_str_idx(const char *const *arr, guint n, const char *target) {
  if (!target)
    return 0;
  for (guint i = 0; i < n; i++) {
    if (g_ascii_strcasecmp(arr[i], target) == 0)
      return i;
  }
  return 0;
}

/* ── Domain / IPC Helpers ─────────────────────────────────────── */

static void read_tcp_congestion(GStrv *avail_out, char *current_out,
                                size_t cur_sz) {
  if (current_out && cur_sz > 0) {
    if (!x3d_ipc_advanced_get("net.ipv4.tcp_congestion_control", current_out,
                              cur_sz)) {
      snprintf(current_out, cur_sz, "cubic");
    }
  }
  if (avail_out) {
    char raw[256] = {0};
    if (!x3d_ipc_advanced_get("net.ipv4.tcp_available_congestion_control", raw,
                              sizeof(raw))) {
      snprintf(raw, sizeof(raw), "cubic bbr reno");
    }
    g_strstrip(raw);
    *avail_out = g_strsplit_set(raw, " \t", -1);
  }
}

static void read_aspm_policy(char *current_out, size_t cur_sz) {
  if (!current_out || cur_sz == 0)
    return;
  char raw[256] = {0};
  if (!x3d_ipc_advanced_get("pcie.aspm_policy", raw, sizeof(raw))) {
    snprintf(current_out, cur_sz, "default");
    return;
  }
  const char *lb = strchr(raw, '[');
  const char *rb = lb ? strchr(lb, ']') : NULL;
  if (lb && rb && rb > lb + 1) {
    size_t len = (size_t)(rb - lb - 1);
    if (len >= cur_sz)
      len = cur_sz - 1;
    memcpy(current_out, lb + 1, len);
    current_out[len] = '\0';
  } else {
    snprintf(current_out, cur_sz, "default");
  }
}

static void refresh_aod_bounds(void) {
  memset(&g_cfg.aod_state, 0, sizeof(g_cfg.aod_state));
  g_cfg.aod_state.smt_cur = -1;

  char ipc_buf[512] = {0};
  if (x3d_ipc_send("AOD_INFO REFRESH", ipc_buf, sizeof(ipc_buf)) == 0 &&
      strncmp(ipc_buf, "SUPPORTED=", 10) == 0) {
    g_cfg.aod_state.supported = atoi(ipc_buf + 10);
    const char *p = strstr(ipc_buf, "PPT=");
    if (p)
      (void)sscanf(p + 4, "%d,%d,%d,%d,%d,%d,%d", &g_cfg.aod_state.has_ppt,
                   &g_cfg.aod_state.ppt_writable, &g_cfg.aod_state.ppt_min_w,
                   &g_cfg.aod_state.ppt_max_w, &g_cfg.aod_state.ppt_step_w,
                   &g_cfg.aod_state.ppt_stock_w, &g_cfg.aod_state.ppt_bios_w);
    p = strstr(ipc_buf, "TDC=");
    if (p)
      (void)sscanf(p + 4, "%d,%d,%d,%d,%d,%d,%d", &g_cfg.aod_state.has_tdc,
                   &g_cfg.aod_state.tdc_writable, &g_cfg.aod_state.tdc_min_a,
                   &g_cfg.aod_state.tdc_max_a, &g_cfg.aod_state.tdc_step_a,
                   &g_cfg.aod_state.tdc_stock_a, &g_cfg.aod_state.tdc_bios_a);
    p = strstr(ipc_buf, "EDC=");
    if (p)
      (void)sscanf(p + 4, "%d,%d,%d,%d,%d,%d,%d", &g_cfg.aod_state.has_edc,
                   &g_cfg.aod_state.edc_writable, &g_cfg.aod_state.edc_min_a,
                   &g_cfg.aod_state.edc_max_a, &g_cfg.aod_state.edc_step_a,
                   &g_cfg.aod_state.edc_stock_a, &g_cfg.aod_state.edc_bios_a);
    p = strstr(ipc_buf, "MEM=");
    if (p)
      (void)sscanf(p + 4, "%d,%d,%d,%d,%d,%d,%d", &g_cfg.aod_state.has_mem,
                   &g_cfg.aod_state.mem_writable,
                   &g_cfg.aod_state.mem_min_mhz, &g_cfg.aod_state.mem_max_mhz,
                   &g_cfg.aod_state.mem_step_mhz,
                   &g_cfg.aod_state.mem_stock_mhz,
                   &g_cfg.aod_state.mem_cur_mhz);
    p = strstr(ipc_buf, "SMT=");
    if (p)
      (void)sscanf(p + 4, "%d,%d", &g_cfg.aod_state.has_smt,
                   &g_cfg.aod_state.smt_cur);
    if (g_cfg.ctx.ipc)
      x3d_gui_ipc_set_aod_supported(g_cfg.ctx.ipc,
                                    g_cfg.aod_state.supported != 0);
  }
}

static void get_aod_stock(int *ppt, int *tdc, int *edc, int *mem) {
  const AodState *s = &g_cfg.aod_state;
  int p = (s->ppt_stock_w > 0) ? s->ppt_stock_w : 162;
  int t = (s->tdc_stock_a > 0) ? s->tdc_stock_a : 120;
  int e = (s->edc_stock_a > 0) ? s->edc_stock_a : 180;
  int m = (s->mem_stock_mhz > 0) ? s->mem_stock_mhz : 2600;

  if (s->supported) {
    if (s->ppt_writable && s->ppt_max_w >= s->ppt_min_w && s->ppt_min_w > 0)
      p = CLAMP(p, s->ppt_min_w, s->ppt_max_w);
    if (s->tdc_writable && s->tdc_max_a >= s->tdc_min_a && s->tdc_min_a > 0)
      t = CLAMP(t, s->tdc_min_a, s->tdc_max_a);
    if (s->edc_writable && s->edc_max_a >= s->edc_min_a && s->edc_min_a > 0)
      e = CLAMP(e, s->edc_min_a, s->edc_max_a);
    if (s->mem_writable && s->mem_max_mhz >= s->mem_min_mhz &&
        s->mem_min_mhz > 0)
      m = CLAMP(m, s->mem_min_mhz, s->mem_max_mhz);
  }
  *ppt = p;
  *tdc = t;
  *edc = e;
  *mem = m;
}

static void get_aod_saved_or_stock(int *ppt, int *tdc, int *edc, int *mem,
                                   gboolean *smt) {
  const AodState *s = &g_cfg.aod_state;
  int p = 162, t = 120, e = 180, m = 2600;
  get_aod_stock(&p, &t, &e, &m);
  if (s->ppt_bios_w > 0)
    p = s->ppt_bios_w;
  if (s->tdc_bios_a > 0)
    t = s->tdc_bios_a;
  if (s->edc_bios_a > 0)
    e = s->edc_bios_a;
  if (s->mem_cur_mhz > 0)
    m = s->mem_cur_mhz;

  const char *v = x3d_ipc_config_get("AOD_PPT_W");
  if (v[0] && atoi(v) > 0)
    p = atoi(v);
  v = x3d_ipc_config_get("AOD_TDC_A");
  if (v[0] && atoi(v) > 0)
    t = atoi(v);
  v = x3d_ipc_config_get("AOD_EDC_A");
  if (v[0] && atoi(v) > 0)
    e = atoi(v);
  v = x3d_ipc_config_get("AOD_MEM_MHZ");
  if (v[0] && atoi(v) > 0)
    m = atoi(v);

  if (s->supported) {
    if (s->ppt_writable && s->ppt_max_w >= s->ppt_min_w && s->ppt_min_w > 0)
      p = CLAMP(p, s->ppt_min_w, s->ppt_max_w);
    if (s->tdc_writable && s->tdc_max_a >= s->tdc_min_a && s->tdc_min_a > 0)
      t = CLAMP(t, s->tdc_min_a, s->tdc_max_a);
    if (s->edc_writable && s->edc_max_a >= s->edc_min_a && s->edc_min_a > 0)
      e = CLAMP(e, s->edc_min_a, s->edc_max_a);
    if (s->mem_writable && s->mem_max_mhz >= s->mem_min_mhz &&
        s->mem_min_mhz > 0)
      m = CLAMP(m, s->mem_min_mhz, s->mem_max_mhz);
  }

  *ppt = p;
  *tdc = t;
  *edc = e;
  *mem = m;
  if (s->smt_cur >= 0) {
    *smt = (s->smt_cur != 0);
  } else {
    v = x3d_ipc_config_get("AOD_SMT_ENABLE");
    *smt = v[0] ? (atoi(v) != 0) : TRUE;
  }
}

static gboolean apply_aod_guarded(int ppt, int tdc, int edc, int mem,
                                  gboolean apply_smt, gboolean smt_on) {
  /* Re-read dynamic *_min and *_max bounds fresh before writing */
  refresh_aod_bounds();
  const AodState *st = &g_cfg.aod_state;

  if (!x3d_gui_ipc_get_aod_supported(g_cfg.ctx.ipc)) {
    if (!x3d_gui_ipc_get_force_unsupported(g_cfg.ctx.ipc))
      return FALSE;
    char vbuf[32];
    gboolean ok = TRUE;
    if (ppt > 0) {
      printf_sn(vbuf, sizeof(vbuf), "%d", ppt);
      if (!x3d_ipc_config_set("AOD_PPT_W", vbuf))
        ok = FALSE;
    }
    if (tdc > 0) {
      printf_sn(vbuf, sizeof(vbuf), "%d", tdc);
      if (!x3d_ipc_config_set("AOD_TDC_A", vbuf))
        ok = FALSE;
    }
    if (edc > 0) {
      printf_sn(vbuf, sizeof(vbuf), "%d", edc);
      if (!x3d_ipc_config_set("AOD_EDC_A", vbuf))
        ok = FALSE;
    }
    if (mem > 0) {
      printf_sn(vbuf, sizeof(vbuf), "%d", mem);
      if (!x3d_ipc_config_set("AOD_MEM_MHZ", vbuf))
        ok = FALSE;
    }
    if (apply_smt && !x3d_ipc_config_set("AOD_SMT_ENABLE", smt_on ? "1" : "0"))
      ok = FALSE;
    if (ok)
      x3d_gui_ipc_set_aod_reboot_pending(g_cfg.ctx.ipc, TRUE);
    return ok;
  }

  /* Fail closed per knob if *_min or *_max is missing or out of range */
  int send_ppt = (st->ppt_writable && ppt > 0) ? ppt : 0;
  int send_tdc = (st->tdc_writable && tdc > 0) ? tdc : 0;
  int send_edc = (st->edc_writable && edc > 0) ? edc : 0;
  int send_mem = (st->mem_writable && mem > 0) ? mem : 0;

  if (send_ppt > 0 && (send_ppt < st->ppt_min_w || send_ppt > st->ppt_max_w))
    return FALSE;
  if (send_tdc > 0 && (send_tdc < st->tdc_min_a || send_tdc > st->tdc_max_a))
    return FALSE;
  if (send_edc > 0 && (send_edc < st->edc_min_a || send_edc > st->edc_max_a))
    return FALSE;
  if (send_mem > 0 &&
      (send_mem < st->mem_min_mhz || send_mem > st->mem_max_mhz))
    return FALSE;

  gboolean ok = TRUE;
  gboolean any_sent = FALSE;
  if (send_ppt > 0 || send_tdc > 0 || send_edc > 0 || send_mem > 0) {
    char cmd[128];
    printf_sn(cmd, sizeof(cmd), "AOD_APPLY %d %d %d %d", send_ppt, send_tdc,
              send_edc, send_mem);
    if (!x3d_ipc_cmd_ok(cmd))
      ok = FALSE;
    any_sent = TRUE;
  }

  if (apply_smt && st->has_smt) {
    char smt_cmd[32];
    printf_sn(smt_cmd, sizeof(smt_cmd), "AOD_SET_SMT %d", smt_on ? 1 : 0);
    if (!x3d_ipc_cmd_ok(smt_cmd))
      ok = FALSE;
    any_sent = TRUE;
  }

  if (!any_sent)
    return FALSE;
  if (ok)
    x3d_gui_ipc_set_aod_reboot_pending(g_cfg.ctx.ipc, TRUE);
  return ok;
}

/* ── Sub-Section Refreshers ───────────────────────────────────── */

static void refresh_aod_widget(void) {
  refresh_aod_bounds();
  const AodState *s = &g_cfg.aod_state;

  if (s->supported) {
    gtk_widget_set_sensitive(g_cfg.row_aod_ppt, s->ppt_writable != 0);
    gtk_widget_set_sensitive(g_cfg.row_aod_tdc, s->tdc_writable != 0);
    gtk_widget_set_sensitive(g_cfg.row_aod_edc, s->edc_writable != 0);
    gtk_widget_set_sensitive(g_cfg.row_aod_mem, s->mem_writable != 0);

    if (s->ppt_writable)
      x3d_slider_entry_set_bounds(g_cfg.sc_aod_ppt, s->ppt_min_w, s->ppt_max_w,
                                  s->ppt_step_w);
    if (s->tdc_writable)
      x3d_slider_entry_set_bounds(g_cfg.sc_aod_tdc, s->tdc_min_a, s->tdc_max_a,
                                  s->tdc_step_a);
    if (s->edc_writable)
      x3d_slider_entry_set_bounds(g_cfg.sc_aod_edc, s->edc_min_a, s->edc_max_a,
                                  s->edc_step_a);
    if (s->mem_writable)
      x3d_slider_entry_set_bounds(g_cfg.sc_aod_mem, s->mem_min_mhz,
                                  s->mem_max_mhz, s->mem_step_mhz);
  } else {
    gtk_widget_set_sensitive(g_cfg.row_aod_ppt, TRUE);
    gtk_widget_set_sensitive(g_cfg.row_aod_tdc, TRUE);
    gtk_widget_set_sensitive(g_cfg.row_aod_edc, TRUE);
    gtk_widget_set_sensitive(g_cfg.row_aod_mem, TRUE);
    x3d_slider_entry_set_bounds(g_cfg.sc_aod_ppt, 45, 300, 1);
    x3d_slider_entry_set_bounds(g_cfg.sc_aod_tdc, 40, 260, 1);
    x3d_slider_entry_set_bounds(g_cfg.sc_aod_edc, 60, 320, 1);
    x3d_slider_entry_set_bounds(g_cfg.sc_aod_mem, 1600, 4400, 50);
  }

  int ppt = 0, tdc = 0, edc = 0, mem = 0;
  gboolean smt = TRUE;
  get_aod_saved_or_stock(&ppt, &tdc, &edc, &mem, &smt);

  x3d_staged_widget_set_syncing(g_cfg.stg_aod, TRUE);
  x3d_slider_entry_set_value(g_cfg.sc_aod_ppt, (double)ppt);
  x3d_slider_entry_set_value(g_cfg.sc_aod_tdc, (double)tdc);
  x3d_slider_entry_set_value(g_cfg.sc_aod_edc, (double)edc);
  x3d_slider_entry_set_value(g_cfg.sc_aod_mem, (double)mem);
  gtk_toggle_button_set_active(g_cfg.tb_aod_smt, smt);
  x3d_staged_widget_set_syncing(g_cfg.stg_aod, FALSE);

  char bios_part[128];
  if (s->ppt_bios_w > 0 || s->tdc_bios_a > 0 || s->edc_bios_a > 0)
    snprintf(bios_part, sizeof(bios_part),
             "SMU Active Limits: %d W / %d A / %d A", s->ppt_bios_w,
             s->tdc_bios_a, s->edc_bios_a);
  else
    snprintf(bios_part, sizeof(bios_part),
             "Firmware Power Attributes: Write-only");
  char hint[288];
  snprintf(hint, sizeof(hint),
           "%s  •  AMD SKU Baseline (Default): %d W / %d A / %d A @ %d MHz",
           bios_part, s->ppt_stock_w, s->tdc_stock_a, s->edc_stock_a,
           s->mem_stock_mhz);
  gtk_label_set_text(GTK_LABEL(g_cfg.lbl_aod_baseline_hint), hint);
  gtk_widget_set_visible(g_cfg.lbl_aod_reboot_badge,
                         x3d_gui_ipc_get_aod_reboot_pending(g_cfg.ctx.ipc));
}

static void refresh_hysteresis_widget(void) {
  const X3dStubState *st = x3d_stubs_get();
  x3d_staged_widget_set_syncing(g_cfg.stg_hysteresis, TRUE);
  gtk_spin_button_set_value(g_cfg.sp_hyst_start_stop, st->hyst_start_stop);
  x3d_slider_entry_set_value(g_cfg.sc_hyst_bearing, st->hyst_bearing);
  x3d_staged_widget_set_syncing(g_cfg.stg_hysteresis, FALSE);
}

static void refresh_network_widget(void) {
  const X3dStubState *st = x3d_stubs_get();
  x3d_staged_widget_set_syncing(g_cfg.stg_network, TRUE);
  const char *qd = x3d_ipc_config_get("NET_QDISC");
  gtk_editable_set_text(g_cfg.ent_net_qdisc, qd[0] ? qd : "fq_codel");
  gtk_drop_down_set_selected(g_cfg.dd_net_buf_preset, st->net_buffer_preset);
  gtk_spin_button_set_value(g_cfg.sp_net_fastopen,
                            atoi(x3d_ipc_config_get("NET_FASTOPEN")));
  gtk_toggle_button_set_active(g_cfg.tb_net_slow_idle,
                               st->tcp_slow_start_idle_off);
  gtk_toggle_button_set_active(g_cfg.tb_net_ecn, st->tcp_ecn_on);
  gtk_toggle_button_set_active(g_cfg.tb_net_5g_keepalive, st->tcp_5g_keepalive);
  gtk_toggle_button_set_active(g_cfg.tb_net_coalesce,
                               atoi(x3d_ipc_config_get("IRQ_COALESCE")) != 0);
  gtk_spin_button_set_value(g_cfg.sp_net_rp_filter,
                            atoi(x3d_ipc_config_get("NET_RP_FILTER")));
  gtk_toggle_button_set_active(
      g_cfg.tb_net_source_route,
      atoi(x3d_ipc_config_get("NET_SOURCE_ROUTE")) != 0);
  const char *sa = x3d_ipc_config_get("SERVER_ADDRESS");
  gtk_editable_set_text(g_cfg.ent_srv_addr,
                        (sa[0] && strcmp(sa, "ERR") != 0) ? sa : "127.0.0.1");
  const char *sp = x3d_ipc_config_get("SERVER_PORT");
  gtk_editable_set_text(g_cfg.ent_srv_port,
                        (sp[0] && strcmp(sp, "ERR") != 0) ? sp : "9370");
  gtk_drop_down_set_selected(
      g_cfg.dd_srv_ssh, find_str_idx(k_ssh, G_N_ELEMENTS(k_ssh),
                                     x3d_ipc_config_get("SERVER_SSH")));
  x3d_staged_widget_set_syncing(g_cfg.stg_network, FALSE);
}

static void refresh_kernel_widget(void) {
  const X3dStubState *st = x3d_stubs_get();
  x3d_staged_widget_set_syncing(g_cfg.stg_kernel, TRUE);
  gtk_toggle_button_set_active(g_cfg.tb_sched_bore,
                               atoi(x3d_ipc_config_get("SCHED_BORE")) != 0);
  const char *bs = x3d_ipc_config_get("SCHED_BIT_SHIFT");
  gtk_spin_button_set_value(g_cfg.sp_sched_bit_shift, bs[0] ? atoi(bs) : 24);
  gtk_toggle_button_set_active(
      g_cfg.tb_sched_burst_fork,
      atoi(x3d_ipc_config_get("SCHED_BURST_FORK")) != 0);
  const char *sl = x3d_ipc_config_get("SCHED_SLICE_US");
  gtk_spin_button_set_value(g_cfg.sp_sched_slice_us, sl[0] ? atoi(sl) : 3000);
  const char *vm = x3d_ipc_config_get("VM_MAX_MAP");
  gtk_editable_set_text(g_cfg.ent_vm_max_map, vm[0] ? vm : "1048576");
  gtk_toggle_button_set_active(
      g_cfg.tb_split_lock,
      atoi(x3d_ipc_config_get("SPLIT_LOCK_DETECT")) != 0);
  gtk_toggle_button_set_active(g_cfg.tb_nmi_watchdog,
                               atoi(x3d_ipc_config_get("NMI_WATCHDOG")) != 0);
  char aspm_cur[32] = {0};
  read_aspm_policy(aspm_cur, sizeof(aspm_cur));
  gtk_drop_down_set_selected(
      g_cfg.dd_pcie_aspm,
      find_str_idx(k_aspm, G_N_ELEMENTS(k_aspm), aspm_cur));
  gtk_toggle_button_set_active(g_cfg.tb_cpu_dma_lat, st->cpu_dma_latency_guard);
  gtk_drop_down_set_selected(
      g_cfg.dd_thp_mode,
      find_str_idx(k_thp, G_N_ELEMENTS(k_thp), x3d_ipc_config_get("THP_MODE")));
  x3d_staged_widget_set_syncing(g_cfg.stg_kernel, FALSE);
}

static void mod_config_refresh(const X3dPageCtx *ctx) {
  if (ctx)
    g_cfg.ctx = *ctx;
  refresh_aod_widget();
  refresh_hysteresis_widget();
  refresh_network_widget();
  refresh_kernel_widget();

  g_cfg.suppress_live_toggles = TRUE;
  gtk_toggle_button_set_active(g_cfg.tb_dev_mode,
                               x3d_gui_ipc_get_dev_enabled(g_cfg.ctx.ipc));
  g_cfg.suppress_live_toggles = FALSE;
}

/* ── Signal Handlers ──────────────────────────────────────────── */

static void on_aod_default(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  int ppt = 162, tdc = 120, edc = 180, mem = 2600;
  get_aod_stock(&ppt, &tdc, &edc, &mem);
  x3d_slider_entry_set_value(g_cfg.sc_aod_ppt, (double)ppt);
  x3d_slider_entry_set_value(g_cfg.sc_aod_tdc, (double)tdc);
  x3d_slider_entry_set_value(g_cfg.sc_aod_edc, (double)edc);
  x3d_slider_entry_set_value(g_cfg.sc_aod_mem, (double)mem);
  gtk_toggle_button_set_active(g_cfg.tb_aod_smt, TRUE);
}

static void on_aod_cancel(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  refresh_aod_widget();
}

static gboolean on_aod_apply(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  int ppt = gtk_widget_get_sensitive(g_cfg.row_aod_ppt)
                ? x3d_slider_entry_get_int(g_cfg.sc_aod_ppt)
                : 0;
  int tdc = gtk_widget_get_sensitive(g_cfg.row_aod_tdc)
                ? x3d_slider_entry_get_int(g_cfg.sc_aod_tdc)
                : 0;
  int edc = gtk_widget_get_sensitive(g_cfg.row_aod_edc)
                ? x3d_slider_entry_get_int(g_cfg.sc_aod_edc)
                : 0;
  int mem = gtk_widget_get_sensitive(g_cfg.row_aod_mem)
                ? x3d_slider_entry_get_int(g_cfg.sc_aod_mem)
                : 0;
  gboolean smt_vis = gtk_widget_get_visible(g_cfg.row_aod_smt);
  gboolean smt_on = gtk_toggle_button_get_active(g_cfg.tb_aod_smt);
  if (!apply_aod_guarded(ppt, tdc, edc, mem, smt_vis, smt_on)) {
    refresh_aod_widget();
    x3d_stubs_toast(
        "AOD Guardrail: Rejected write (missing *_min or *_max bounds, out of "
        "dynamic firmware range, or WMI IPC error)");
    return FALSE;
  }
  gtk_widget_set_visible(g_cfg.lbl_aod_reboot_badge, TRUE);
  gui_popups_show_aod_restart(g_cfg.ctx.view->main_window);
  return TRUE;
}

static void on_hyst_default(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  x3d_stub_hysteresis_reset_defaults();
  gtk_spin_button_set_value(g_cfg.sp_hyst_start_stop,
                            x3d_stubs_get()->hyst_start_stop);
  x3d_slider_entry_set_value(g_cfg.sc_hyst_bearing,
                             x3d_stubs_get()->hyst_bearing);
}

static void on_hyst_cancel(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  refresh_hysteresis_widget();
}

static gboolean on_hyst_apply(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  x3d_stubs_get()->hyst_start_stop =
      (int)gtk_spin_button_get_value(g_cfg.sp_hyst_start_stop);
  x3d_stubs_get()->hyst_bearing =
      (double)x3d_slider_entry_get_int(g_cfg.sc_hyst_bearing);
  x3d_stub_hysteresis_apply();
  return TRUE;
}

static void on_hyst_baselines_clicked(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  gui_popups_show_hyst_baselines(g_cfg.ctx.view->main_window,
                                 g_cfg.stg_hysteresis);
}

static void on_net_default(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  x3d_stub_network_addons_reset_defaults(g_cfg.ctx.ipc);
  gtk_editable_set_text(g_cfg.ent_net_qdisc, "fq_codel");
  gtk_drop_down_set_selected(g_cfg.dd_net_buf_preset, 0);
  gtk_spin_button_set_value(g_cfg.sp_net_fastopen, 3);
  gtk_toggle_button_set_active(g_cfg.tb_net_slow_idle, FALSE);
  gtk_toggle_button_set_active(g_cfg.tb_net_ecn, FALSE);
  gtk_toggle_button_set_active(g_cfg.tb_net_5g_keepalive, FALSE);
  gtk_toggle_button_set_active(g_cfg.tb_net_coalesce, FALSE);
  gtk_spin_button_set_value(g_cfg.sp_net_rp_filter, 2);
  gtk_toggle_button_set_active(g_cfg.tb_net_source_route, FALSE);
  gtk_editable_set_text(g_cfg.ent_srv_addr, "127.0.0.1");
  gtk_editable_set_text(g_cfg.ent_srv_port, "9370");
  gtk_drop_down_set_selected(g_cfg.dd_srv_ssh, 0);
}

static void on_net_cancel(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  refresh_network_widget();
}

static gboolean on_net_apply(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  X3dStubState *st = x3d_stubs_get();
  char buf[32];
  gboolean ok = TRUE;

  const char *qd = gtk_editable_get_text(g_cfg.ent_net_qdisc);
  if (qd && qd[0] && x3d_gui_ipc_has_cap(g_cfg.ctx.ipc, CAP_QDISC)) {
    if (!x3d_ipc_advanced_set("net.core.default_qdisc", qd))
      ok = FALSE;
  }
  if (x3d_gui_ipc_has_cap(g_cfg.ctx.ipc, CAP_FASTOPEN)) {
    snprintf(buf, sizeof(buf), "%d",
             (int)gtk_spin_button_get_value(g_cfg.sp_net_fastopen));
    if (!x3d_ipc_advanced_set("net.ipv4.tcp_fastopen", buf))
      ok = FALSE;
  }

  snprintf(buf, sizeof(buf), "%d",
           (int)gtk_spin_button_get_value(g_cfg.sp_net_rp_filter));
  if (!x3d_ipc_advanced_set("net.ipv4.conf.all.rp_filter", buf))
    ok = FALSE;

  const char *sr =
      gtk_toggle_button_get_active(g_cfg.tb_net_source_route) ? "1" : "0";
  if (!x3d_ipc_advanced_set("net.ipv4.conf.all.accept_source_route", sr))
    ok = FALSE;

  if (!x3d_ipc_config_set(
          "IRQ_COALESCE",
          gtk_toggle_button_get_active(g_cfg.tb_net_coalesce) ? "1" : "0"))
    ok = FALSE;

  const char *addr = gtk_editable_get_text(g_cfg.ent_srv_addr);
  if (addr && addr[0] && !x3d_ipc_config_set("SERVER_ADDRESS", addr))
    ok = FALSE;
  const char *port = gtk_editable_get_text(g_cfg.ent_srv_port);
  if (port && port[0] && !x3d_ipc_config_set("SERVER_PORT", port))
    ok = FALSE;
  guint sidx = gtk_drop_down_get_selected(g_cfg.dd_srv_ssh);
  if (sidx < G_N_ELEMENTS(k_ssh) &&
      !x3d_ipc_config_set("SERVER_SSH", k_ssh[sidx]))
    ok = FALSE;

  st->net_buffer_preset = gtk_drop_down_get_selected(g_cfg.dd_net_buf_preset);
  st->tcp_slow_start_idle_off =
      gtk_toggle_button_get_active(g_cfg.tb_net_slow_idle);
  st->tcp_ecn_on = gtk_toggle_button_get_active(g_cfg.tb_net_ecn);
  st->tcp_5g_keepalive =
      gtk_toggle_button_get_active(g_cfg.tb_net_5g_keepalive);
  x3d_stub_network_addons_apply();

  if (!ok)
    x3d_stubs_toast("Failed to apply Network Settings (daemon IPC error)");
  return ok;
}

static void on_kern_default(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  x3d_stub_kernel_addons_reset_defaults();
  gtk_toggle_button_set_active(g_cfg.tb_sched_bore, TRUE);
  gtk_spin_button_set_value(g_cfg.sp_sched_bit_shift, 24);
  gtk_toggle_button_set_active(g_cfg.tb_sched_burst_fork, TRUE);
  gtk_spin_button_set_value(g_cfg.sp_sched_slice_us, 5000);
  gtk_editable_set_text(g_cfg.ent_vm_max_map, "1048576");
  gtk_toggle_button_set_active(g_cfg.tb_split_lock, FALSE);
  gtk_toggle_button_set_active(g_cfg.tb_nmi_watchdog, FALSE);
  gtk_drop_down_set_selected(g_cfg.dd_pcie_aspm, 0);
  gtk_toggle_button_set_active(g_cfg.tb_cpu_dma_lat, FALSE);
  gtk_drop_down_set_selected(g_cfg.dd_thp_mode, 1);
}

static void on_kern_cancel(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  refresh_kernel_widget();
}

static gboolean on_kern_apply(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  char buf[32];
  gboolean ok = TRUE;

  if (x3d_gui_ipc_has_cap(g_cfg.ctx.ipc, CAP_BORE)) {
    const char *bore =
        gtk_toggle_button_get_active(g_cfg.tb_sched_bore) ? "1" : "0";
    if (!x3d_ipc_advanced_set("kernel.sched_bore", bore))
      ok = FALSE;

    snprintf(buf, sizeof(buf), "%d",
             (int)gtk_spin_button_get_value(g_cfg.sp_sched_bit_shift));
    if (!x3d_ipc_advanced_set("kernel.sched_burst_penalty_offset", buf))
      ok = FALSE;

    const char *bf =
        gtk_toggle_button_get_active(g_cfg.tb_sched_burst_fork) ? "2" : "0";
    if (!x3d_ipc_advanced_set("kernel.sched_burst_inherit_type", bf))
      ok = FALSE;
  }

  snprintf(buf, sizeof(buf), "%d",
           (int)gtk_spin_button_get_value(g_cfg.sp_sched_slice_us));
  if (!x3d_ipc_config_set("SCHED_SLICE_US", buf))
    ok = FALSE;
  char cur_slice[32] = {0};
  if (x3d_ipc_advanced_get("kernel.sched_cfs_bandwidth_slice_us", cur_slice,
                           sizeof(cur_slice)) &&
      !x3d_ipc_advanced_set("kernel.sched_cfs_bandwidth_slice_us", buf))
    ok = FALSE;

  const char *vm = gtk_editable_get_text(g_cfg.ent_vm_max_map);
  if (vm && vm[0] && !x3d_ipc_advanced_set("vm.max_map_count", vm))
    ok = FALSE;

  if (x3d_gui_ipc_has_cap(g_cfg.ctx.ipc, CAP_SPLIT_LOCK)) {
    const char *sl =
        gtk_toggle_button_get_active(g_cfg.tb_split_lock) ? "1" : "0";
    if (!x3d_ipc_advanced_set("kernel.split_lock_mitigate", sl))
      ok = FALSE;
  }
  if (x3d_gui_ipc_has_cap(g_cfg.ctx.ipc, CAP_NMI_WATCHDOG)) {
    const char *nw =
        gtk_toggle_button_get_active(g_cfg.tb_nmi_watchdog) ? "1" : "0";
    if (!x3d_ipc_advanced_set("kernel.nmi_watchdog", nw))
      ok = FALSE;
  }
  if (x3d_gui_ipc_has_cap(g_cfg.ctx.ipc, CAP_THP)) {
    guint ti = gtk_drop_down_get_selected(g_cfg.dd_thp_mode);
    if (ti < G_N_ELEMENTS(k_thp) &&
        !x3d_ipc_advanced_set("target_thp_mode", k_thp[ti]))
      ok = FALSE;
  }

  x3d_stubs_get()->pcie_aspm_idx =
      gtk_drop_down_get_selected(g_cfg.dd_pcie_aspm);
  x3d_stubs_get()->cpu_dma_latency_guard =
      gtk_toggle_button_get_active(g_cfg.tb_cpu_dma_lat);
  x3d_stub_kernel_addons_apply();

  if (!ok)
    x3d_stubs_toast("Failed to apply Kernel Settings (daemon IPC error)");
  return ok;
}

static void on_dev_mode_toggled(GtkToggleButton *tb, gpointer d) {
  (void)d;
  if (g_cfg.suppress_live_toggles)
    return;
  gboolean active = gtk_toggle_button_get_active(tb);
  x3d_gui_ipc_set_dev_enabled(g_cfg.ctx.ipc, active);
  x3d_ipc_config_set("DEV_ENABLE", active ? "1" : "0");
  if (g_cfg.ctx.sync_sidebar_gating)
    g_cfg.ctx.sync_sidebar_gating(g_cfg.ctx.frame);
}

/* ── Build ────────────────────────────────────────────────────── */

static GtkWidget *mod_config_build(const X3dPageCtx *ctx) {
  g_cfg.ctx = *ctx;
  refresh_aod_bounds();

  GtkBox *page = x3d_page_vbox_new(ctx->view->content_stack, "advanced",
                                   "Advanced Configuration");
  g_cfg.root = GTK_WIDGET(page);

  /* 7A. AOD Controls */
  GtkWidget *stg_aod =
      x3d_staged_widget_new("AOD CONTROLS (AMD-WMI-AOD)", FALSE);
  g_cfg.stg_aod = X3D_STAGED_WIDGET(stg_aod);
  x3d_widget_bind_caps(stg_aod, ctx->ipc, CAP_AOD);

  g_cfg.lbl_aod_reboot_badge = gtk_label_new("REBOOT PENDING");
  gtk_widget_add_css_class(g_cfg.lbl_aod_reboot_badge, "x3d-reboot-badge");
  gtk_widget_set_visible(g_cfg.lbl_aod_reboot_badge, FALSE);
  x3d_staged_widget_add_header_suffix(g_cfg.stg_aod,
                                      g_cfg.lbl_aod_reboot_badge);

  GtkBox *abody = x3d_staged_widget_get_body(g_cfg.stg_aod);
  g_cfg.lbl_aod_baseline_hint = gtk_label_new(
      "Firmware AOD setup writes take effect after system restart.");
  gtk_widget_add_css_class(g_cfg.lbl_aod_baseline_hint, "x3d-gpu-subtitle");
  gtk_label_set_wrap(GTK_LABEL(g_cfg.lbl_aod_baseline_hint), TRUE);
  gtk_label_set_xalign(GTK_LABEL(g_cfg.lbl_aod_baseline_hint), 0.0f);
  gtk_box_append(abody, g_cfg.lbl_aod_baseline_hint);

  GtkWidget *se_ppt =
      x3d_slider_entry_new(45, 300, 1, 162, "W", &g_cfg.sc_aod_ppt);
  GtkWidget *se_tdc =
      x3d_slider_entry_new(40, 260, 1, 120, "A", &g_cfg.sc_aod_tdc);
  GtkWidget *se_edc =
      x3d_slider_entry_new(60, 320, 1, 180, "A", &g_cfg.sc_aod_edc);
  GtkWidget *se_mem =
      x3d_slider_entry_new(1600, 4400, 50, 2600, "MHz", &g_cfg.sc_aod_mem);

  g_cfg.row_aod_ppt =
      x3d_form_row_new("PPT", "Package Power Tracking ceiling", se_ppt);
  g_cfg.row_aod_tdc = x3d_form_row_new(
      "TDC", "Sustained Thermal Design Current ceiling", se_tdc);
  g_cfg.row_aod_edc = x3d_form_row_new(
      "EDC", "Peak Electrical Design Current ceiling", se_edc);
  g_cfg.row_aod_mem =
      x3d_form_row_new("Mem Clock", "DRAM target clock frequency", se_mem);

  gtk_box_append(abody, g_cfg.row_aod_ppt);
  gtk_box_append(abody, g_cfg.row_aod_tdc);
  gtk_box_append(abody, g_cfg.row_aod_edc);
  gtk_box_append(abody, g_cfg.row_aod_mem);

  GtkWidget *tb_smt = x3d_toggle_new("IRQ / Firmware SMT");
  g_cfg.tb_aod_smt = GTK_TOGGLE_BUTTON(tb_smt);
  g_cfg.row_aod_smt = x3d_form_row_new(
      "IRQ / Firmware SMT",
      "Platform firmware Simultaneous Multithreading (requires reboot)", tb_smt);
  x3d_widget_bind_caps(g_cfg.row_aod_smt, ctx->ipc, CAP_AOD_SMT);
  gtk_box_append(abody, g_cfg.row_aod_smt);

  x3d_staged_widget_watch_control(g_cfg.stg_aod, GTK_WIDGET(g_cfg.sc_aod_ppt));
  x3d_staged_widget_watch_control(g_cfg.stg_aod, GTK_WIDGET(g_cfg.sc_aod_tdc));
  x3d_staged_widget_watch_control(g_cfg.stg_aod, GTK_WIDGET(g_cfg.sc_aod_edc));
  x3d_staged_widget_watch_control(g_cfg.stg_aod, GTK_WIDGET(g_cfg.sc_aod_mem));
  x3d_staged_widget_watch_control(g_cfg.stg_aod, tb_smt);
  gtk_box_append(page, stg_aod);

  g_signal_connect(g_cfg.stg_aod, "default", G_CALLBACK(on_aod_default), NULL);
  g_signal_connect(g_cfg.stg_aod, "cancel", G_CALLBACK(on_aod_cancel), NULL);
  g_signal_connect(g_cfg.stg_aod, "apply", G_CALLBACK(on_aod_apply), NULL);

  /* 7B. Hysteresis */
  GtkWidget *stg_hyst =
      x3d_staged_widget_new("LOAD / POLLING HYSTERESIS LOGIC", FALSE);
  g_cfg.stg_hysteresis = X3D_STAGED_WIDGET(stg_hyst);
  GtkBox *hbody = x3d_staged_widget_get_body(g_cfg.stg_hysteresis);

  GtkWidget *sp_ss = gtk_spin_button_new_with_range(1, 30, 1);
  GtkWidget *se_bear =
      x3d_slider_entry_new(0, 100, 1, 50, "%", &g_cfg.sc_hyst_bearing);
  g_cfg.sp_hyst_start_stop = GTK_SPIN_BUTTON(sp_ss);
  GtkWidget *btn_hyst_base = x3d_popup_btn_new("Baselines");

  gtk_box_append(hbody,
                 x3d_form_row_new("Start/Stop",
                                  "Consecutive sample count before switching",
                                  sp_ss));
  gtk_box_append(
      hbody,
      x3d_form_row_new("Bearing",
                       "Directional bias between latency and throughput",
                       se_bear));
  GtkWidget *hitems[] = {btn_hyst_base};
  x3d_staged_widget_set_grid_items(g_cfg.stg_hysteresis, hitems, 1);
  x3d_staged_widget_watch_control(g_cfg.stg_hysteresis, sp_ss);
  x3d_staged_widget_watch_control(g_cfg.stg_hysteresis,
                                  GTK_WIDGET(g_cfg.sc_hyst_bearing));
  gtk_box_append(page, stg_hyst);

  g_signal_connect(g_cfg.stg_hysteresis, "default", G_CALLBACK(on_hyst_default),
                   NULL);
  g_signal_connect(g_cfg.stg_hysteresis, "cancel", G_CALLBACK(on_hyst_cancel),
                   NULL);
  g_signal_connect(g_cfg.stg_hysteresis, "apply", G_CALLBACK(on_hyst_apply),
                   NULL);
  g_signal_connect(btn_hyst_base, "clicked",
                   G_CALLBACK(on_hyst_baselines_clicked), NULL);

  /* 7C. Network Settings */
  GtkWidget *stg_net = x3d_staged_widget_new("NETWORK SETTINGS", FALSE);
  g_cfg.stg_network = X3D_STAGED_WIDGET(stg_net);
  GtkBox *nbody = x3d_staged_widget_get_body(g_cfg.stg_network);

  gtk_box_append(nbody, x3d_subsection_label_new("Performance & Throughput"));

  GtkWidget *ent_qdisc = gtk_entry_new();
  gtk_widget_set_size_request(ent_qdisc, 160, -1);
  g_cfg.ent_net_qdisc = GTK_EDITABLE(ent_qdisc);
  GtkWidget *row_qdisc =
      x3d_form_row_new("Default QDisc", "Packet scheduler (fq_codel, cake)",
                       ent_qdisc);
  x3d_widget_bind_caps(row_qdisc, ctx->ipc, CAP_QDISC);
  gtk_box_append(nbody, row_qdisc);

  GStrv cong_avail = NULL;
  char cong_cur[32] = {0};
  read_tcp_congestion(&cong_avail, cong_cur, sizeof(cong_cur));
  g_cfg.dd_net_cong = GTK_DROP_DOWN(
      gtk_drop_down_new_from_strings((const char *const *)cong_avail));
  g_strfreev(cong_avail);
  GtkWidget *row_cong = x3d_form_row_new(
      "TCP Congestion Control",
      "Probed from /proc/sys/net/ipv4/tcp_available_congestion_control",
      GTK_WIDGET(g_cfg.dd_net_cong));
  x3d_widget_bind_caps(row_cong, ctx->ipc, X3D_CAP_TCP_CONG);
  gtk_box_append(nbody, row_cong);

  const char *buf_presets[] = {"Stock", "1 Gbps", "2.5+ Gbps", NULL};
  g_cfg.dd_net_buf_preset =
      GTK_DROP_DOWN(gtk_drop_down_new_from_strings(buf_presets));
  gtk_box_append(
      nbody,
      x3d_form_row_new("Throughput Buffer Preset",
                       "Matched rmem/wmem & netdev/syn backlog tuple",
                       GTK_WIDGET(g_cfg.dd_net_buf_preset)));

  GtkWidget *sp_tfo = gtk_spin_button_new_with_range(0, 3, 1);
  g_cfg.sp_net_fastopen = GTK_SPIN_BUTTON(sp_tfo);
  GtkWidget *row_tfo =
      x3d_form_row_new("TCP Fast Open", "Connection initiation mode (0–3)",
                       sp_tfo);
  x3d_widget_bind_caps(row_tfo, ctx->ipc, CAP_FASTOPEN);
  gtk_box_append(nbody, row_tfo);

  GtkWidget *tb_ssi = x3d_toggle_new("TCP Slow Start After Idle");
  GtkWidget *tb_ecn = x3d_toggle_new("TCP ECN");
  GtkWidget *tb_5g = x3d_toggle_new("5G / CGNAT Keepalive");
  GtkWidget *tb_coal = x3d_toggle_new("NIC Coalescing Hints");
  g_cfg.tb_net_slow_idle = GTK_TOGGLE_BUTTON(tb_ssi);
  g_cfg.tb_net_ecn = GTK_TOGGLE_BUTTON(tb_ecn);
  g_cfg.tb_net_5g_keepalive = GTK_TOGGLE_BUTTON(tb_5g);
  g_cfg.tb_net_coalesce = GTK_TOGGLE_BUTTON(tb_coal);

  GtkWidget *perf_grid = x3d_centered_3col_grid_new();
  GtkWidget *perf_toggles[] = {tb_ssi, tb_ecn, tb_5g, tb_coal};
  x3d_grid_pack_centered_3col(GTK_GRID(perf_grid), perf_toggles, 4);
  gtk_box_append(nbody, perf_grid);

  gtk_box_append(nbody, x3d_subsection_label_new("Security & Filtering"));
  GtkWidget *sp_rpf = gtk_spin_button_new_with_range(0, 2, 1);
  g_cfg.sp_net_rp_filter = GTK_SPIN_BUTTON(sp_rpf);
  gtk_box_append(nbody,
                 x3d_form_row_new("Reverse Path Filter",
                                  "0 = Off, 1 = Strict, 2 = Loose", sp_rpf));

  GtkWidget *tb_src = x3d_toggle_new("Accept Source Route");
  g_cfg.tb_net_source_route = GTK_TOGGLE_BUTTON(tb_src);
  GtkWidget *sec_grid = x3d_centered_3col_grid_new();
  GtkWidget *sec_items[] = {tb_src};
  x3d_grid_pack_centered_3col(GTK_GRID(sec_grid), sec_items, 1);
  gtk_box_append(nbody, sec_grid);

  gtk_box_append(nbody, x3d_subsection_label_new("Remote Management"));
  GtkWidget *ent_ip = gtk_entry_new();
  GtkWidget *ent_port = gtk_entry_new();
  const char *ssh_modes[] = {"Local", "Remote", "SSH Tunnel Only", NULL};
  g_cfg.ent_srv_addr = GTK_EDITABLE(ent_ip);
  g_cfg.ent_srv_port = GTK_EDITABLE(ent_port);
  g_cfg.dd_srv_ssh = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(ssh_modes));

  gtk_box_append(nbody,
                 x3d_form_row_new("Server IP Address",
                                  "Target daemon bind/remote IP", ent_ip));
  gtk_box_append(nbody,
                 x3d_form_row_new("Server Port", "Target daemon port (9370)",
                                  ent_port));
  gtk_box_append(nbody,
                 x3d_form_row_new("SSH Mode", "Transport security mode",
                                  GTK_WIDGET(g_cfg.dd_srv_ssh)));

  GtkWidget *net_watched[] = {ent_qdisc,
                              GTK_WIDGET(g_cfg.dd_net_cong),
                              GTK_WIDGET(g_cfg.dd_net_buf_preset),
                              sp_tfo,
                              tb_ssi,
                              tb_ecn,
                              tb_5g,
                              tb_coal,
                              sp_rpf,
                              tb_src,
                              ent_ip,
                              ent_port,
                              GTK_WIDGET(g_cfg.dd_srv_ssh)};
  for (size_t i = 0; i < G_N_ELEMENTS(net_watched); i++)
    x3d_staged_widget_watch_control(g_cfg.stg_network, net_watched[i]);
  gtk_box_append(page, stg_net);

  g_signal_connect(g_cfg.stg_network, "default", G_CALLBACK(on_net_default),
                   NULL);
  g_signal_connect(g_cfg.stg_network, "cancel", G_CALLBACK(on_net_cancel),
                   NULL);
  g_signal_connect(g_cfg.stg_network, "apply", G_CALLBACK(on_net_apply), NULL);

  /* 7D. Kernel Settings */
  GtkWidget *stg_kern = x3d_staged_widget_new("KERNEL SETTINGS", FALSE);
  g_cfg.stg_kernel = X3D_STAGED_WIDGET(stg_kern);
  GtkBox *kbody = x3d_staged_widget_get_body(g_cfg.stg_kernel);

  GtkWidget *sp_bpo = gtk_spin_button_new_with_range(0, 64, 1);
  GtkWidget *sp_cfs = gtk_spin_button_new_with_range(100, 10000, 100);
  GtkWidget *ent_vmm = gtk_entry_new();
  const char *aspm_opts[] = {"default", "performance", "powersave",
                             "powersupersave", NULL};
  const char *thp_opts[] = {"always", "madvise", "never", NULL};

  g_cfg.sp_sched_bit_shift = GTK_SPIN_BUTTON(sp_bpo);
  g_cfg.sp_sched_slice_us = GTK_SPIN_BUTTON(sp_cfs);
  g_cfg.ent_vm_max_map = GTK_EDITABLE(ent_vmm);
  g_cfg.dd_pcie_aspm = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(aspm_opts));
  g_cfg.dd_thp_mode = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(thp_opts));

  GtkWidget *row_bpo = x3d_form_row_new(
      "Burst Penalty Offset", "BORE interactivity protection offset (0–64)",
      sp_bpo);
  x3d_widget_bind_caps(row_bpo, ctx->ipc, CAP_BORE);
  gtk_box_append(kbody, row_bpo);

  gtk_box_append(
      kbody, x3d_form_row_new("CFS Bandwidth Slice (BORE Scheduler Tunables)",
                              "Scheduling slice in microseconds (us)", sp_cfs));
  gtk_box_append(kbody,
                 x3d_form_row_new("VM Max Map Count",
                                  "Max memory map areas (Wine / Proton)",
                                  ent_vmm));

  GtkWidget *row_aspm = x3d_form_row_new(
      "PCIe ASPM Policy", "Active State Power Management policy",
      GTK_WIDGET(g_cfg.dd_pcie_aspm));
  x3d_widget_bind_caps(row_aspm, ctx->ipc, X3D_CAP_PCIE_ASPM);
  gtk_box_append(kbody, row_aspm);

  GtkWidget *row_thp =
      x3d_form_row_new("Transparent Huge Pages", "Kernel THP allocation policy",
                       GTK_WIDGET(g_cfg.dd_thp_mode));
  x3d_widget_bind_caps(row_thp, ctx->ipc, CAP_THP);
  gtk_box_append(kbody, row_thp);

  GtkWidget *tb_bore = x3d_toggle_new("Sched BORE");
  GtkWidget *tb_bfork = x3d_toggle_new("Burst Inheritance");
  GtkWidget *tb_split = x3d_toggle_new("Split-Lock Mitigation");
  GtkWidget *tb_nmi = x3d_toggle_new("NMI Watchdog");
  GtkWidget *tb_dma = x3d_toggle_new("CPU DMA Latency / Deep C-State Guard");

  x3d_widget_bind_caps(tb_bore, ctx->ipc, CAP_BORE);
  x3d_widget_bind_caps(tb_bfork, ctx->ipc, CAP_BORE);
  x3d_widget_bind_caps(tb_split, ctx->ipc, CAP_SPLIT_LOCK);
  x3d_widget_bind_caps(tb_nmi, ctx->ipc, CAP_NMI_WATCHDOG);
  x3d_widget_bind_caps(tb_dma, ctx->ipc, X3D_CAP_CPU_DMA_LAT);

  g_cfg.tb_sched_bore = GTK_TOGGLE_BUTTON(tb_bore);
  g_cfg.tb_sched_burst_fork = GTK_TOGGLE_BUTTON(tb_bfork);
  g_cfg.tb_split_lock = GTK_TOGGLE_BUTTON(tb_split);
  g_cfg.tb_nmi_watchdog = GTK_TOGGLE_BUTTON(tb_nmi);
  g_cfg.tb_cpu_dma_lat = GTK_TOGGLE_BUTTON(tb_dma);

  GtkWidget *ktoggles[] = {tb_bore, tb_bfork, tb_split, tb_nmi, tb_dma};
  x3d_staged_widget_set_grid_items(g_cfg.stg_kernel, ktoggles, 5);

  GtkWidget *kwatched[] = {sp_bpo,
                           sp_cfs,
                           ent_vmm,
                           GTK_WIDGET(g_cfg.dd_pcie_aspm),
                           GTK_WIDGET(g_cfg.dd_thp_mode),
                           tb_bore,
                           tb_bfork,
                           tb_split,
                           tb_nmi,
                           tb_dma};
  for (size_t i = 0; i < G_N_ELEMENTS(kwatched); i++)
    x3d_staged_widget_watch_control(g_cfg.stg_kernel, kwatched[i]);
  gtk_box_append(page, stg_kern);

  g_signal_connect(g_cfg.stg_kernel, "default", G_CALLBACK(on_kern_default),
                   NULL);
  g_signal_connect(g_cfg.stg_kernel, "cancel", G_CALLBACK(on_kern_cancel),
                   NULL);
  g_signal_connect(g_cfg.stg_kernel, "apply", G_CALLBACK(on_kern_apply), NULL);

  /* 7E. Developer Mode LiveToggle */
  GtkWidget *tb_dev = NULL;
  gtk_box_append(
      page, x3d_live_toggle_row_new("Developer Mode",
                                    "Exposes Developer Options in sidebar",
                                    &tb_dev));
  g_cfg.tb_dev_mode = GTK_TOGGLE_BUTTON(tb_dev);
  g_signal_connect(g_cfg.tb_dev_mode, "toggled",
                   G_CALLBACK(on_dev_mode_toggled), NULL);

  x3d_widgets_tree_connect_dirty_notify(g_cfg.root, ctx->notify_dirty_cb,
                                        ctx->notify_dirty_data);
  return g_cfg.root;
}

static gboolean mod_config_has_dirty(void) {
  return x3d_widgets_tree_has_dirty(g_cfg.root);
}
static gboolean mod_config_has_staged(void) {
  return x3d_widgets_tree_has_staged(g_cfg.root);
}
static gboolean mod_config_apply_all(void) {
  return x3d_widgets_tree_apply_dirty(g_cfg.root);
}
static void mod_config_cancel_all(void) {
  x3d_widgets_tree_cancel_dirty(g_cfg.root);
}
static void mod_config_default_all(void) {
  x3d_widgets_tree_default_all(g_cfg.root);
}

const X3dPageModule *gui_config_get_module(void) {
  static const X3dPageModule k_mod = {
      .id = "advanced",
      .title = "Advanced Configuration",
      .is_subnav = TRUE,
      .initially_hidden = TRUE,
      .divider_after = FALSE,
      .build = mod_config_build,
      .refresh = mod_config_refresh,
      .activate = NULL,
      .deactivate = NULL,
      .has_dirty = mod_config_has_dirty,
      .has_staged = mod_config_has_staged,
      .apply_all = mod_config_apply_all,
      .cancel_all = mod_config_cancel_all,
      .default_all = mod_config_default_all,
      .on_interval_changed = NULL,
      .highlight_hw_mode = NULL,
  };
  return &k_mod;
}
