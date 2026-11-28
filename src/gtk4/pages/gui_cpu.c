/* CPU Page & Standalone System Profile / Hardware Modes Slice Implementation
 * `pages/gui_cpu.c`
 */

#include "gui_cpu.h"
#include "../gui_popups.h"
#include "../gui_stubs.h"
#include "gui_widgets.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int printf_sn(char *buf, size_t size, const char *fmt, ...);

typedef struct {
  char pstate[64];
  char governor[64];
  char epp[64];
  gboolean dynamic_epp;
  gboolean has_dynamic_epp;
  gboolean boost;
} X3dSysProfileData;

static const char *const k_pstates[3] = {"active", "passive", "guided"};
static const char *const k_govs[3] = {"powersave", "performance", "schedutil"};
static const char *const k_epps[4] = {"performance", "balance_performance",
                                      "balance_power", "power"};
static const char *const k_hw_cmds[5] = {"cache", "frequency", "dual",
                                         "default", "reset"};

static struct {
  X3dPageCtx ctx;
  GtkWidget *cpu_root;
  GtkWidget *hw_standalone_root;
  GtkWidget *sp_standalone_root;

  GtkWidget *lbl_cpu_meta;
  GtkWidget *lbl_cpu_telemetry;

  X3dStagedWidget *stg_cpu_sysprof;
  GtkDropDown *dd_cpu_pstate;
  GtkDropDown *dd_cpu_gov;
  GtkDropDown *dd_cpu_epp;
  GtkToggleButton *tb_cpu_dynepp;
  GtkToggleButton *tb_cpu_boost;

  GtkWidget *cpu_mode_btns[5];
  GtkWidget *hw_standalone_btns[5];

  X3dStagedWidget *stg_sp_standalone;
  GtkDropDown *dd_sp_pstate;
  GtkDropDown *dd_sp_gov;
  GtkDropDown *dd_sp_epp;
  GtkToggleButton *tb_sp_dynepp;
  GtkToggleButton *tb_sp_boost;

  gboolean suppress_hw_mode_btns;
  guint cpu_timer_id;

  /* Cached UI telemetry state */
  gboolean meta_loaded;
  char model_name[128];
  char hwmon_dir[256];
  gboolean prev_valid;
  uint64_t prev_tot[129];
  uint64_t prev_idle[129];
} g_cpu;

static guint find_str_idx(const char *const *arr, guint n, const char *target) {
  if (!target)
    return 0;
  for (guint i = 0; i < n; i++) {
    if (g_ascii_strcasecmp(arr[i], target) == 0)
      return i;
  }
  return 0;
}

static void str_to_lower(char *s) {
  for (size_t i = 0; s[i]; i++)
    s[i] = (char)tolower((unsigned char)s[i]);
}

static gboolean cpu_read_file(const char *path, char *out, size_t out_sz) {
  if (!out || out_sz == 0)
    return FALSE;
  out[0] = '\0';
  int fd = open(path, O_RDONLY);
  if (fd < 0)
    return FALSE;
  ssize_t n = read(fd, out, out_sz - 1);
  close(fd);
  if (n <= 0)
    return FALSE;
  out[n] = '\0';
  return TRUE;
}

static gboolean cpu_read_ll(const char *dir, const char *attr,
                            long long *val_out) {
  char path[640], buf[64];
  snprintf(path, sizeof(path), "%s/%s", dir, attr);
  if (!cpu_read_file(path, buf, sizeof(buf)))
    return FALSE;
  char *endptr = NULL;
  errno = 0;
  long long v = strtoll(buf, &endptr, 10);
  if (endptr == buf || errno == ERANGE)
    return FALSE;
  *val_out = v;
  return TRUE;
}

static void cpu_ensure_meta(void) {
  if (g_cpu.meta_loaded)
    return;
  g_cpu.meta_loaded = TRUE;
  snprintf(g_cpu.model_name, sizeof(g_cpu.model_name),
           "AMD Ryzen X3D Processor");
  g_cpu.hwmon_dir[0] = '\0';

  FILE *f = fopen("/proc/cpuinfo", "r");
  if (f) {
    char line[256];
    while (fgets(line, sizeof(line), f)) {
      if (strncmp(line, "model name", 10) == 0) {
        const char *colon = strchr(line, ':');
        if (colon) {
          colon++;
          while (*colon == ' ' || *colon == '\t')
            colon++;
          snprintf(g_cpu.model_name, sizeof(g_cpu.model_name), "%s", colon);
          g_cpu.model_name[strcspn(g_cpu.model_name, "\r\n")] = '\0';
        }
        break;
      }
    }
    fclose(f);
  }

  DIR *dir = opendir("/sys/class/hwmon");
  if (dir) {
    const struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
      if (strncmp(de->d_name, "hwmon", 5) != 0)
        continue;
      char name_path[320], drv[64];
      snprintf(name_path, sizeof(name_path), "/sys/class/hwmon/%s/name",
               de->d_name);
      if (!cpu_read_file(name_path, drv, sizeof(drv)))
        continue;
      drv[strcspn(drv, "\r\n")] = '\0';
      if (strcmp(drv, "zenpower") == 0 || strcmp(drv, "k10temp") == 0) {
        snprintf(g_cpu.hwmon_dir, sizeof(g_cpu.hwmon_dir), "/sys/class/hwmon/%s",
                 de->d_name);
        if (strcmp(drv, "zenpower") == 0)
          break;
      }
    }
    closedir(dir);
  }
}

static gboolean parse_proc_stat_line(const char *p, uint64_t *tot_out,
                                     uint64_t *idle_out) {
  unsigned long long user = 0, nice_v = 0, sys = 0, idle = 0, iowait = 0,
                     irq = 0, softirq = 0, steal = 0;
  int n = sscanf(p, "%llu %llu %llu %llu %llu %llu %llu %llu", &user, &nice_v,
                 &sys, &idle, &iowait, &irq, &softirq, &steal);
  if (n < 4)
    return FALSE;
  *idle_out = (uint64_t)(idle + iowait);
  *tot_out =
      (uint64_t)(user + nice_v + sys + idle + iowait + irq + softirq + steal);
  return TRUE;
}

static void sample_cpu_telemetry(X3dGuiIpc *ipc, X3dCpuTelemetry *out) {
  (void)ipc;
  if (!out)
    return;
  memset(out, 0, sizeof(*out));
  snprintf(out->model_name, sizeof(out->model_name), "AMD Ryzen Processor");
  snprintf(out->topology_str, sizeof(out->topology_str), "N/A");
  snprintf(out->load_str, sizeof(out->load_str), "N/A");
  snprintf(out->clocks_str, sizeof(out->clocks_str), "N/A");
  snprintf(out->temps_str, sizeof(out->temps_str), "N/A");
  snprintf(out->power_str, sizeof(out->power_str), "N/A");
  snprintf(out->profile_str, sizeof(out->profile_str), "N/A");

  cpu_ensure_meta();
  snprintf(out->model_name, sizeof(out->model_name), "%s", g_cpu.model_name);

  char c_mask[128] = {0}, f_mask[128] = {0};
  int ccd1_start = 0, total_cores = 0;
  if (x3d_ipc_read_topology_masks(c_mask, sizeof(c_mask), f_mask,
                                  sizeof(f_mask), &ccd1_start,
                                  &total_cores) != 0 ||
      total_cores <= 0) {
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);
    total_cores = (nproc > 0) ? (int)nproc : 16;
  }
  if (total_cores > 128)
    total_cores = 128;
  out->core_count = total_cores;
  out->ccd1_start = ccd1_start;

  gboolean dual_ccd = (ccd1_start > 0 && ccd1_start < total_cores);
  if (dual_ccd) {
    snprintf(out->topology_str, sizeof(out->topology_str),
             "%d Logical Threads  •  Dual-CCD (CCD0: 0-%d / CCD1: %d-%d)",
             total_cores, ccd1_start - 1, ccd1_start, total_cores - 1);
  } else {
    snprintf(out->topology_str, sizeof(out->topology_str),
             "%d Logical Threads  •  Single-CCD", total_cores);
  }

  FILE *sf = fopen("/proc/stat", "r");
  if (sf) {
    char line[256];
    uint64_t new_tot[129] = {0};
    uint64_t new_idle[129] = {0};
    gboolean seen[129] = {FALSE};

    while (fgets(line, sizeof(line), sf)) {
      if (strncmp(line, "cpu", 3) != 0)
        break;
      int slot = -1;
      const char *vals = NULL;
      if (line[3] == ' ') {
        slot = 0;
        vals = line + 4;
      } else if (line[3] >= '0' && line[3] <= '9') {
        int cpu_id = atoi(line + 3);
        if (cpu_id >= 0 && cpu_id < 128) {
          slot = cpu_id + 1;
          vals = strchr(line, ' ');
        }
      }
      if (slot >= 0 && vals &&
          parse_proc_stat_line(vals, &new_tot[slot], &new_idle[slot])) {
        seen[slot] = TRUE;
      }
    }
    fclose(sf);

    if (g_cpu.prev_valid) {
      if (seen[0] && new_tot[0] > g_cpu.prev_tot[0]) {
        uint64_t dt = new_tot[0] - g_cpu.prev_tot[0];
        uint64_t di = (new_idle[0] >= g_cpu.prev_idle[0])
                          ? (new_idle[0] - g_cpu.prev_idle[0])
                          : 0;
        out->total_load_pct =
            CLAMP(100.0 * (double)(dt - di) / (double)dt, 0.0, 100.0);
      }
      double c0_sum = 0.0, c1_sum = 0.0;
      int c0_n = 0, c1_n = 0;
      for (int i = 0; i < total_cores; i++) {
        int slot = i + 1;
        if (seen[slot] && new_tot[slot] > g_cpu.prev_tot[slot]) {
          uint64_t dt = new_tot[slot] - g_cpu.prev_tot[slot];
          uint64_t di = (new_idle[slot] >= g_cpu.prev_idle[slot])
                            ? (new_idle[slot] - g_cpu.prev_idle[slot])
                            : 0;
          out->core_load_pct[i] =
              CLAMP(100.0 * (double)(dt - di) / (double)dt, 0.0, 100.0);
          if (!dual_ccd || i < ccd1_start) {
            c0_sum += out->core_load_pct[i];
            c0_n++;
          } else {
            c1_sum += out->core_load_pct[i];
            c1_n++;
          }
        }
      }
      if (c0_n > 0)
        out->ccd0_load_pct = c0_sum / (double)c0_n;
      if (c1_n > 0)
        out->ccd1_load_pct = c1_sum / (double)c1_n;
    }

    memcpy(g_cpu.prev_tot, new_tot, sizeof(new_tot));
    memcpy(g_cpu.prev_idle, new_idle, sizeof(new_idle));
    g_cpu.prev_valid = TRUE;
  }

  if (dual_ccd) {
    snprintf(out->load_str, sizeof(out->load_str),
             "%.1f%% Total  •  CCD0 %.0f%%  •  CCD1 %.0f%%",
             out->total_load_pct, out->ccd0_load_pct, out->ccd1_load_pct);
  } else {
    snprintf(out->load_str, sizeof(out->load_str), "%.1f%% Total",
             out->total_load_pct);
  }

  double sum_mhz = 0.0, peak_mhz = 0.0, max_mhz = 0.0;
  int freq_n = 0;
  for (int i = 0; i < total_cores; i++) {
    char fpath[128], buf[32];
    snprintf(fpath, sizeof(fpath),
             "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);
    if (cpu_read_file(fpath, buf, sizeof(buf))) {
      double mhz = (double)strtoll(buf, NULL, 10) / 1000.0;
      if (mhz > 0.0) {
        sum_mhz += mhz;
        if (mhz > peak_mhz)
          peak_mhz = mhz;
        freq_n++;
      }
    }
    if (i == 0) {
      snprintf(fpath, sizeof(fpath),
               "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
      if (cpu_read_file(fpath, buf, sizeof(buf)))
        max_mhz = (double)strtoll(buf, NULL, 10) / 1000.0;
    }
  }
  if (freq_n > 0) {
    out->avg_clock_mhz = sum_mhz / (double)freq_n;
    out->peak_clock_mhz = peak_mhz;
    out->max_clock_mhz = (max_mhz > peak_mhz) ? max_mhz : 5700.0;
    snprintf(out->clocks_str, sizeof(out->clocks_str),
             "%.0f MHz Avg  •  %.0f MHz Peak", out->avg_clock_mhz,
             out->peak_clock_mhz);
  }

  if (g_cpu.hwmon_dir[0] != '\0') {
    double tctl = 0.0, tccd1 = 0.0, tccd2 = 0.0;
    gboolean has_tctl = FALSE, has_c1 = FALSE, has_c2 = FALSE;
    for (int idx = 1; idx <= 8; idx++) {
      char lbl_attr[32], in_attr[32], lbl_path[320], lbl[64];
      snprintf(lbl_attr, sizeof(lbl_attr), "temp%d_label", idx);
      snprintf(in_attr, sizeof(in_attr), "temp%d_input", idx);
      snprintf(lbl_path, sizeof(lbl_path), "%s/%s", g_cpu.hwmon_dir, lbl_attr);
      long long mc = 0;
      if (!cpu_read_ll(g_cpu.hwmon_dir, in_attr, &mc))
        continue;
      double deg = (double)mc / 1000.0;
      if (cpu_read_file(lbl_path, lbl, sizeof(lbl))) {
        lbl[strcspn(lbl, "\r\n")] = '\0';
        if (strcmp(lbl, "Tctl") == 0 || strcmp(lbl, "Tdie") == 0) {
          if (!has_tctl) {
            tctl = deg;
            has_tctl = TRUE;
          }
        } else if (strcmp(lbl, "Tccd1") == 0) {
          tccd1 = deg;
          has_c1 = TRUE;
        } else if (strcmp(lbl, "Tccd2") == 0) {
          tccd2 = deg;
          has_c2 = TRUE;
        }
      } else if (idx == 1 && !has_tctl) {
        tctl = deg;
        has_tctl = TRUE;
      }
    }
    out->temp_tctl_c = tctl;
    out->temp_ccd1_c = tccd1;
    out->temp_ccd2_c = tccd2;
    if (has_tctl && has_c1 && has_c2) {
      snprintf(out->temps_str, sizeof(out->temps_str),
               "%.1f °C Tctl  •  %.1f °C CCD0  •  %.1f °C CCD1", tctl, tccd1,
               tccd2);
    } else if (has_tctl && has_c1) {
      snprintf(out->temps_str, sizeof(out->temps_str),
               "%.1f °C Tctl  •  %.1f °C CCD0", tctl, tccd1);
    } else if (has_tctl) {
      snprintf(out->temps_str, sizeof(out->temps_str), "%.1f °C Tctl", tctl);
    }
  }

  AodLiveStats aod_live;
  x3d_ipc_read_aod_live(&aod_live);
  if (aod_live.has_ppt) {
    out->ppt_w = aod_live.ppt_val_w;
    out->ppt_lim_w = aod_live.ppt_lim_w;
    if (aod_live.ppt_lim_w > 0.0) {
      snprintf(out->power_str, sizeof(out->power_str), "%.1f W / %.0f W (PPT)",
               aod_live.ppt_val_w, aod_live.ppt_lim_w);
    } else {
      snprintf(out->power_str, sizeof(out->power_str), "%.1f W (PPT)",
               aod_live.ppt_val_w);
    }
  }

  char status_resp[1024] = {0};
  if (x3d_ipc_send("STATUS", status_resp, sizeof(status_resp)) == 0 &&
      status_resp[0] != '\0' && strcmp(status_resp, "ERR") != 0) {
    char drv[32] = "N/A", gov[32] = "N/A", epp[32] = "N/A", bst[16] = "N/A";
    x3d_ipc_kv_get(status_resp, "DRIVER", '|', drv, sizeof(drv));
    x3d_ipc_kv_get(status_resp, "GOV", '|', gov, sizeof(gov));
    x3d_ipc_kv_get(status_resp, "EPP", '|', epp, sizeof(epp));
    x3d_ipc_kv_get(status_resp, "BOOST", '|', bst, sizeof(bst));
    snprintf(out->profile_str, sizeof(out->profile_str),
             "%s  •  %s  •  %s  •  Boost %s", drv, gov, epp,
             (g_ascii_strcasecmp(bst, "ENABLED") == 0) ? "ON" : "OFF");
  }
}

/* ── System Profile & Hardware Mode IPC Logic ─────────────────── */

static void default_sys_profile(X3dSysProfileData *out) {
  snprintf(out->pstate, sizeof(out->pstate), "active");
  snprintf(out->governor, sizeof(out->governor), "powersave");
  snprintf(out->epp, sizeof(out->epp), "balance_performance");
  out->dynamic_epp = FALSE;
  out->has_dynamic_epp = FALSE;
  out->boost = TRUE;
}

static void read_sys_profile(X3dSysProfileData *out) {
  default_sys_profile(out);
  char resp[1024] = {0};
  if (x3d_ipc_send("STATUS", resp, sizeof(resp)) != 0 || resp[0] == '\0' ||
      strcmp(resp, "ERR") == 0) {
    return;
  }

  if (x3d_ipc_kv_get(resp, "DRIVER", '|', out->pstate, sizeof(out->pstate)))
    str_to_lower(out->pstate);
  if (x3d_ipc_kv_get(resp, "GOV", '|', out->governor, sizeof(out->governor)))
    str_to_lower(out->governor);
  if (x3d_ipc_kv_get(resp, "EPP", '|', out->epp, sizeof(out->epp))) {
    str_to_lower(out->epp);
    if (strcmp(out->epp, "default") == 0)
      snprintf(out->epp, sizeof(out->epp), "performance");
  }

  char bst[32] = {0};
  if (x3d_ipc_kv_get(resp, "BOOST", '|', bst, sizeof(bst)))
    out->boost = (g_ascii_strcasecmp(bst, "ENABLED") == 0);

  char depp[32] = {0};
  if (x3d_ipc_kv_get(resp, "DYNEPP", '|', depp, sizeof(depp)) &&
      g_ascii_strcasecmp(depp, "UNSUPPORTED") != 0 &&
      g_ascii_strcasecmp(depp, "N/A") != 0) {
    out->has_dynamic_epp = TRUE;
    out->dynamic_epp =
        (strcmp(depp, "1") == 0 || g_ascii_strcasecmp(depp, "ENABLED") == 0);
  }
}

static gboolean apply_sys_profile(X3dSysProfileData *in) {
  char cmd[128];
  gboolean ok = TRUE;

  if (strcmp(in->pstate, "active") == 0 && strcmp(in->epp, "performance") != 0 &&
      strcmp(in->governor, "performance") == 0) {
    snprintf(in->governor, sizeof(in->governor), "powersave");
  }

  printf_sn(cmd, sizeof(cmd), "SET_PSTATE %s", in->pstate);
  if (!x3d_ipc_cmd_ok(cmd))
    ok = FALSE;

  printf_sn(cmd, sizeof(cmd), "SET_GOVERNOR %s", in->governor);
  if (!x3d_ipc_cmd_ok(cmd))
    ok = FALSE;

  if (!in->dynamic_epp) {
    printf_sn(cmd, sizeof(cmd), "SET_EPP %s", in->epp);
    if (!x3d_ipc_cmd_ok(cmd))
      ok = FALSE;
  }

  printf_sn(cmd, sizeof(cmd), "SET_BOOST %s", in->boost ? "on" : "off");
  if (!x3d_ipc_cmd_ok(cmd))
    ok = FALSE;

  return ok;
}

static void load_sysprof_pair(GtkDropDown *dd_ps, GtkDropDown *dd_gov,
                              GtkDropDown *dd_epp, GtkToggleButton *tb_dyn,
                              GtkToggleButton *tb_bst, X3dStagedWidget *stg,
                              gboolean use_defaults) {
  X3dSysProfileData d;
  if (use_defaults)
    default_sys_profile(&d);
  else
    read_sys_profile(&d);

  x3d_staged_widget_set_syncing(stg, TRUE);
  gtk_drop_down_set_selected(
      dd_ps, find_str_idx(k_pstates, G_N_ELEMENTS(k_pstates), d.pstate));
  gtk_drop_down_set_selected(
      dd_gov, find_str_idx(k_govs, G_N_ELEMENTS(k_govs), d.governor));
  gtk_drop_down_set_selected(
      dd_epp, find_str_idx(k_epps, G_N_ELEMENTS(k_epps), d.epp));
  gtk_toggle_button_set_active(tb_dyn, d.dynamic_epp);
  gtk_widget_set_sensitive(GTK_WIDGET(dd_epp), !d.dynamic_epp);
  gtk_toggle_button_set_active(tb_bst, d.boost);
  x3d_staged_widget_set_syncing(stg, FALSE);
}

static gboolean apply_sysprof_widgets(GtkDropDown *dd_ps, GtkDropDown *dd_gov,
                                      GtkDropDown *dd_epp,
                                      GtkToggleButton *tb_dyn,
                                      GtkToggleButton *tb_bst) {
  X3dSysProfileData d;
  memset(&d, 0, sizeof(d));
  guint pi = gtk_drop_down_get_selected(dd_ps);
  guint gi = gtk_drop_down_get_selected(dd_gov);
  guint ei = gtk_drop_down_get_selected(dd_epp);
  snprintf(d.pstate, sizeof(d.pstate), "%s",
           pi < G_N_ELEMENTS(k_pstates) ? k_pstates[pi] : "active");
  snprintf(d.governor, sizeof(d.governor), "%s",
           gi < G_N_ELEMENTS(k_govs) ? k_govs[gi] : "powersave");
  snprintf(d.epp, sizeof(d.epp), "%s",
           ei < G_N_ELEMENTS(k_epps) ? k_epps[ei] : "performance");
  d.dynamic_epp = gtk_toggle_button_get_active(tb_dyn);
  d.boost = gtk_toggle_button_get_active(tb_bst);

  gboolean ok = apply_sys_profile(&d);
  x3d_stub_apply_dynamic_epp(d.dynamic_epp);
  if (g_cpu.ctx.poll_dashboard_now)
    g_cpu.ctx.poll_dashboard_now(g_cpu.ctx.frame);
  if (!ok)
    x3d_stubs_toast("Failed to commit CPU System Profile (daemon IPC error)");
  return ok;
}

static void mod_cpu_highlight_hw_mode(const char *mode_upper) {
  if (!mode_upper)
    return;
  g_cpu.suppress_hw_mode_btns = TRUE;
  static const char *const keys[5] = {"CACHE", "FREQUENCY", "DUAL", "DEFAULT",
                                      "AUTO"};
  for (int i = 0; i < 5; i++) {
    gboolean match = (strstr(mode_upper, keys[i]) != NULL);
    if (g_cpu.cpu_mode_btns[i]) {
      if (match)
        gtk_widget_add_css_class(g_cpu.cpu_mode_btns[i], "active");
      else
        gtk_widget_remove_css_class(g_cpu.cpu_mode_btns[i], "active");
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_cpu.cpu_mode_btns[i]),
                                   match);
    }
    if (g_cpu.hw_standalone_btns[i]) {
      if (match)
        gtk_widget_add_css_class(g_cpu.hw_standalone_btns[i], "active");
      else
        gtk_widget_remove_css_class(g_cpu.hw_standalone_btns[i], "active");
      gtk_toggle_button_set_active(
          GTK_TOGGLE_BUTTON(g_cpu.hw_standalone_btns[i]), match);
    }
  }
  g_cpu.suppress_hw_mode_btns = FALSE;
}

/* ── Signal Callbacks ─────────────────────────────────────────── */

static void on_cpu_dash_activated(X3dCurrentWidget *w, gpointer d) {
  (void)w;
  (void)d;
  gui_popups_show_telemetry(g_cpu.ctx.view->main_window, g_cpu.ctx.ipc,
                            X3D_TELEM_CPU, NULL);
}

static void on_dynepp_cpu_toggled(GtkToggleButton *tb, gpointer d) {
  (void)d;
  gtk_widget_set_sensitive(GTK_WIDGET(g_cpu.dd_cpu_epp),
                           !gtk_toggle_button_get_active(tb));
}

static void on_dynepp_sp_toggled(GtkToggleButton *tb, gpointer d) {
  (void)d;
  gtk_widget_set_sensitive(GTK_WIDGET(g_cpu.dd_sp_epp),
                           !gtk_toggle_button_get_active(tb));
}

static void on_cpu_sysprof_default(X3dStagedWidget *w, gpointer d) {
  (void)d;
  load_sysprof_pair(g_cpu.dd_cpu_pstate, g_cpu.dd_cpu_gov, g_cpu.dd_cpu_epp,
                    g_cpu.tb_cpu_dynepp, g_cpu.tb_cpu_boost, w, TRUE);
}

static void on_cpu_sysprof_cancel(X3dStagedWidget *w, gpointer d) {
  (void)d;
  load_sysprof_pair(g_cpu.dd_cpu_pstate, g_cpu.dd_cpu_gov, g_cpu.dd_cpu_epp,
                    g_cpu.tb_cpu_dynepp, g_cpu.tb_cpu_boost, w, FALSE);
}

static gboolean on_cpu_sysprof_apply(X3dStagedWidget *w, gpointer d) {
  (void)d;
  if (!apply_sysprof_widgets(g_cpu.dd_cpu_pstate, g_cpu.dd_cpu_gov,
                             g_cpu.dd_cpu_epp, g_cpu.tb_cpu_dynepp,
                             g_cpu.tb_cpu_boost)) {
    return FALSE;
  }
  load_sysprof_pair(g_cpu.dd_cpu_pstate, g_cpu.dd_cpu_gov, g_cpu.dd_cpu_epp,
                    g_cpu.tb_cpu_dynepp, g_cpu.tb_cpu_boost, w, FALSE);
  return TRUE;
}

static void on_sp_standalone_default(X3dStagedWidget *w, gpointer d) {
  (void)d;
  load_sysprof_pair(g_cpu.dd_sp_pstate, g_cpu.dd_sp_gov, g_cpu.dd_sp_epp,
                    g_cpu.tb_sp_dynepp, g_cpu.tb_sp_boost, w, TRUE);
}

static void on_sp_standalone_cancel(X3dStagedWidget *w, gpointer d) {
  (void)d;
  load_sysprof_pair(g_cpu.dd_sp_pstate, g_cpu.dd_sp_gov, g_cpu.dd_sp_epp,
                    g_cpu.tb_sp_dynepp, g_cpu.tb_sp_boost, w, FALSE);
}

static gboolean on_sp_standalone_apply(X3dStagedWidget *w, gpointer d) {
  (void)d;
  if (!apply_sysprof_widgets(g_cpu.dd_sp_pstate, g_cpu.dd_sp_gov,
                             g_cpu.dd_sp_epp, g_cpu.tb_sp_dynepp,
                             g_cpu.tb_sp_boost)) {
    return FALSE;
  }
  x3d_staged_widget_set_dirty(w, FALSE);
  if (g_cpu.ctx.perform_switch)
    g_cpu.ctx.perform_switch(g_cpu.ctx.frame, "dashboard", TRUE);
  return TRUE;
}

static void on_sp_standalone_close(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  if (g_cpu.ctx.perform_switch)
    g_cpu.ctx.perform_switch(g_cpu.ctx.frame, "dashboard", TRUE);
}

static void on_back_to_dash(GtkButton *btn, gpointer d) {
  (void)btn;
  (void)d;
  if (g_cpu.ctx.request_navigate)
    g_cpu.ctx.request_navigate(g_cpu.ctx.frame, "dashboard", TRUE);
}

static void on_cpu_hw_mode_clicked(GtkButton *btn, gpointer d) {
  (void)d;
  if (g_cpu.suppress_hw_mode_btns)
    return;
  const char *cmd_mode = g_object_get_data(G_OBJECT(btn), "x3d-mode-cmd");
  gboolean standalone =
      GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "x3d-standalone"));

  char ipc_cmd[64];
  printf_sn(ipc_cmd, sizeof(ipc_cmd), "SET_MODE %s",
            cmd_mode ? cmd_mode : "default");
  if (!x3d_ipc_cmd_ok(ipc_cmd)) {
    x3d_stubs_toast("Failed to switch Hardware Mode (daemon IPC error)");
    return;
  }

  gchar *up = g_ascii_strup(cmd_mode ? cmd_mode : "DEFAULT", -1);
  mod_cpu_highlight_hw_mode(up);
  g_free(up);

  if (g_cpu.ctx.poll_dashboard_now)
    g_cpu.ctx.poll_dashboard_now(g_cpu.ctx.frame);
  if (standalone && g_cpu.ctx.perform_switch)
    g_cpu.ctx.perform_switch(g_cpu.ctx.frame, "dashboard", TRUE);
}

static void on_cpu_floor_clicked(GtkButton *btn, gpointer d) {
  (void)btn;
  (void)d;
  gui_popups_show_floor_freq(g_cpu.ctx.view->main_window,
                             g_cpu.stg_cpu_sysprof);
}

static void on_sp_floor_clicked(GtkButton *btn, gpointer d) {
  (void)btn;
  (void)d;
  gui_popups_show_floor_freq(g_cpu.ctx.view->main_window,
                             g_cpu.stg_sp_standalone);
}

static void on_cpu_save_prof(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  x3d_stub_cpu_save_profile(g_cpu.ctx.view->main_window);
}

static void on_cpu_load_prof(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  x3d_stub_cpu_load_profile(g_cpu.ctx.view->main_window);
}

/* ── Live Telemetry Timer & View Refresh ──────────────────────── */

static void cpu_refresh_telemetry_card(void) {
  X3dCpuTelemetry t;
  sample_cpu_telemetry(g_cpu.ctx.ipc, &t);
  char meta[256];
  snprintf(meta, sizeof(meta), "%s  •  %s", t.model_name, t.topology_str);
  gtk_label_set_text(GTK_LABEL(g_cpu.lbl_cpu_meta), meta);

  gchar *m = g_markup_printf_escaped("<span font_family=\"monospace\">"
                                     "  <b>CPU Load:</b>       %s\n"
                                     "  <b>Clocks:</b>         %s\n"
                                     "  <b>Temperatures:</b>   %s\n"
                                     "  <b>Power (PPT):</b>    %s\n"
                                     "  <b>Active Profile:</b> %s"
                                     "</span>",
                                     t.load_str, t.clocks_str, t.temps_str,
                                     t.power_str, t.profile_str);
  gtk_label_set_markup(GTK_LABEL(g_cpu.lbl_cpu_telemetry), m);
  g_free(m);
}

static gboolean cpu_timer_cb(gpointer data) {
  (void)data;
  const char *vis =
      g_cpu.ctx.view
          ? gtk_stack_get_visible_child_name(g_cpu.ctx.view->content_stack)
          : NULL;
  if (!vis || strcmp(vis, "cpu") != 0) {
    g_cpu.cpu_timer_id = 0;
    return G_SOURCE_REMOVE;
  }
  cpu_refresh_telemetry_card();
  return G_SOURCE_CONTINUE;
}

static GtkWidget *mod_cpu_build(const X3dPageCtx *ctx) {
  g_cpu.ctx = *ctx;
  gui_popups_register_samplers(sample_cpu_telemetry, NULL);

  const char *pstates[] = {"active", "passive", "guided", NULL};
  const char *govs[] = {"powersave", "performance", "schedutil", NULL};
  const char *epps[] = {"performance", "balance_performance", "balance_power",
                        "power", NULL};
  static const char *const m_lbls[5] = {"Cache", "Frequency", "Dual", "Default",
                                        "Auto/Reset"};

  /* 1. Main CPU Page */
  GtkBox *page = x3d_page_vbox_new(ctx->view->content_stack, "cpu", "CPU");
  g_cpu.cpu_root = GTK_WIDGET(page);

  GtkWidget *w_dash = x3d_current_widget_new("CPU DASHBOARD", TRUE);
  GtkBox *cbody = x3d_current_widget_get_body(X3D_CURRENT_WIDGET(w_dash));
  g_cpu.lbl_cpu_meta = gtk_label_new("Probing Ryzen CPU topology...");
  gtk_widget_add_css_class(g_cpu.lbl_cpu_meta, "x3d-gpu-subtitle");
  gtk_label_set_xalign(GTK_LABEL(g_cpu.lbl_cpu_meta), 0.0f);
  g_cpu.lbl_cpu_telemetry = gtk_label_new("Sampling CPU telemetry...");
  gtk_widget_add_css_class(g_cpu.lbl_cpu_telemetry, "x3d-value");
  gtk_label_set_xalign(GTK_LABEL(g_cpu.lbl_cpu_telemetry), 0.0f);
  gtk_box_append(cbody, g_cpu.lbl_cpu_meta);
  gtk_box_append(cbody, g_cpu.lbl_cpu_telemetry);
  gtk_box_append(page, w_dash);
  g_signal_connect(w_dash, "activated", G_CALLBACK(on_cpu_dash_activated),
                   NULL);

  GtkWidget *stg = x3d_staged_widget_new("SYSTEM PROFILE", FALSE);
  g_cpu.stg_cpu_sysprof = X3D_STAGED_WIDGET(stg);
  GtkBox *body = x3d_staged_widget_get_body(g_cpu.stg_cpu_sysprof);

  g_cpu.dd_cpu_pstate = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(pstates));
  g_cpu.dd_cpu_gov = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(govs));
  g_cpu.dd_cpu_epp = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(epps));

  gtk_box_append(body,
                 x3d_form_row_new(
                     "Driver Mode",
                     "amd_pstate operational mode (active / passive / guided)",
                     GTK_WIDGET(g_cpu.dd_cpu_pstate)));
  gtk_box_append(body,
                 x3d_form_row_new("Scaling Governor", "CPUFreq scaling policy",
                                  GTK_WIDGET(g_cpu.dd_cpu_gov)));
  gtk_box_append(
      body,
      x3d_form_row_new(
          "EPP Profile",
          "Energy Performance Preference (locked when Dynamic EPP is enabled)",
          GTK_WIDGET(g_cpu.dd_cpu_epp)));

  GtkWidget *dynepp = x3d_toggle_new("Dynamic EPP (dynamic_epp)");
  GtkWidget *boost = x3d_toggle_new("Precision Boost");
  GtkWidget *floor_btn = x3d_popup_btn_new("Per-Core Frequency Floor");
  x3d_widget_bind_caps(dynepp, ctx->ipc, X3D_CAP_DYNAMIC_EPP);
  x3d_widget_bind_caps(floor_btn, ctx->ipc, X3D_CAP_FLOOR_FREQ);

  g_cpu.tb_cpu_dynepp = GTK_TOGGLE_BUTTON(dynepp);
  g_cpu.tb_cpu_boost = GTK_TOGGLE_BUTTON(boost);

  GtkWidget *grid_items[] = {dynepp, boost, floor_btn};
  x3d_staged_widget_set_grid_items(g_cpu.stg_cpu_sysprof, grid_items, 3);

  GtkWidget *btn_save = gtk_button_new_with_label("Save Profile");
  GtkWidget *btn_load = gtk_button_new_with_label("Load Profile");
  gtk_widget_add_css_class(btn_save, "x3d-cta-btn");
  gtk_widget_add_css_class(btn_load, "x3d-cta-btn");
  x3d_staged_widget_add_left_action(g_cpu.stg_cpu_sysprof, btn_save);
  x3d_staged_widget_add_left_action(g_cpu.stg_cpu_sysprof, btn_load);

  x3d_staged_widget_watch_control(g_cpu.stg_cpu_sysprof,
                                  GTK_WIDGET(g_cpu.dd_cpu_pstate));
  x3d_staged_widget_watch_control(g_cpu.stg_cpu_sysprof,
                                  GTK_WIDGET(g_cpu.dd_cpu_gov));
  x3d_staged_widget_watch_control(g_cpu.stg_cpu_sysprof,
                                  GTK_WIDGET(g_cpu.dd_cpu_epp));
  x3d_staged_widget_watch_control(g_cpu.stg_cpu_sysprof, dynepp);
  x3d_staged_widget_watch_control(g_cpu.stg_cpu_sysprof, boost);
  gtk_box_append(page, stg);

  g_signal_connect(g_cpu.tb_cpu_dynepp, "toggled",
                   G_CALLBACK(on_dynepp_cpu_toggled), NULL);
  g_signal_connect(g_cpu.stg_cpu_sysprof, "default",
                   G_CALLBACK(on_cpu_sysprof_default), NULL);
  g_signal_connect(g_cpu.stg_cpu_sysprof, "cancel",
                   G_CALLBACK(on_cpu_sysprof_cancel), NULL);
  g_signal_connect(g_cpu.stg_cpu_sysprof, "apply",
                   G_CALLBACK(on_cpu_sysprof_apply), NULL);
  g_signal_connect(floor_btn, "clicked", G_CALLBACK(on_cpu_floor_clicked),
                   NULL);
  g_signal_connect(btn_save, "clicked", G_CALLBACK(on_cpu_save_prof), NULL);
  g_signal_connect(btn_load, "clicked", G_CALLBACK(on_cpu_load_prof), NULL);

  GtkWidget *imm = x3d_immediate_widget_new("HARDWARE MODES");
  for (int i = 0; i < 5; i++) {
    g_cpu.cpu_mode_btns[i] = x3d_toggle_new(m_lbls[i]);
    g_object_set_data(G_OBJECT(g_cpu.cpu_mode_btns[i]), "x3d-mode-cmd",
                      (gpointer)k_hw_cmds[i]);
    g_signal_connect(g_cpu.cpu_mode_btns[i], "clicked",
                     G_CALLBACK(on_cpu_hw_mode_clicked), NULL);
  }
  x3d_immediate_widget_set_grid_items(X3D_IMMEDIATE_WIDGET(imm),
                                      g_cpu.cpu_mode_btns, 5);
  gtk_box_append(page, imm);

  /* 2. Standalone View: Hardware Modes */
  GtkBox *hw_page = x3d_page_vbox_new(ctx->view->content_stack,
                                      "hardware_modes_view", "Hardware Modes");
  g_cpu.hw_standalone_root = GTK_WIDGET(hw_page);
  GtkWidget *hw_imm = x3d_immediate_widget_new(
      "SELECT HARDWARE MODE (APPLIES IMMEDIATELY & CLOSES VIEW)");
  for (int i = 0; i < 5; i++) {
    g_cpu.hw_standalone_btns[i] = x3d_toggle_new(m_lbls[i]);
    g_object_set_data(G_OBJECT(g_cpu.hw_standalone_btns[i]), "x3d-mode-cmd",
                      (gpointer)k_hw_cmds[i]);
    g_object_set_data(G_OBJECT(g_cpu.hw_standalone_btns[i]), "x3d-standalone",
                      GINT_TO_POINTER(1));
    g_signal_connect(g_cpu.hw_standalone_btns[i], "clicked",
                     G_CALLBACK(on_cpu_hw_mode_clicked), NULL);
  }
  x3d_immediate_widget_set_grid_items(X3D_IMMEDIATE_WIDGET(hw_imm),
                                      g_cpu.hw_standalone_btns, 5);
  gtk_box_append(hw_page, hw_imm);

  GtkWidget *btn_hw_close = gtk_button_new_with_label("Close");
  gtk_widget_add_css_class(btn_hw_close, "x3d-cta-btn");
  gtk_widget_set_halign(btn_hw_close, GTK_ALIGN_END);
  g_signal_connect(btn_hw_close, "clicked", G_CALLBACK(on_back_to_dash), NULL);
  gtk_box_append(hw_page, btn_hw_close);

  /* 3. Standalone View: System Profile */
  GtkBox *sp_page = x3d_page_vbox_new(ctx->view->content_stack,
                                      "system_profile_view", "System Profile");
  g_cpu.sp_standalone_root = GTK_WIDGET(sp_page);
  GtkWidget *sp_stg =
      x3d_staged_widget_new("AMD P-STATE & FREQUENCY SCALING", TRUE);
  g_cpu.stg_sp_standalone = X3D_STAGED_WIDGET(sp_stg);
  GtkBox *sp_body = x3d_staged_widget_get_body(g_cpu.stg_sp_standalone);

  g_cpu.dd_sp_pstate = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(pstates));
  g_cpu.dd_sp_gov = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(govs));
  g_cpu.dd_sp_epp = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(epps));

  gtk_box_append(sp_body,
                 x3d_form_row_new("Driver Mode", "amd_pstate operational mode",
                                  GTK_WIDGET(g_cpu.dd_sp_pstate)));
  gtk_box_append(sp_body,
                 x3d_form_row_new("Scaling Governor", "CPUFreq scaling policy",
                                  GTK_WIDGET(g_cpu.dd_sp_gov)));
  gtk_box_append(sp_body,
                 x3d_form_row_new("EPP Profile",
                                  "Energy Performance Preference bias",
                                  GTK_WIDGET(g_cpu.dd_sp_epp)));

  GtkWidget *sp_dynepp = x3d_toggle_new("Dynamic EPP (dynamic_epp)");
  GtkWidget *sp_boost = x3d_toggle_new("Precision Boost");
  GtkWidget *sp_floor = x3d_popup_btn_new("Per-Core Frequency Floor");
  x3d_widget_bind_caps(sp_dynepp, ctx->ipc, X3D_CAP_DYNAMIC_EPP);
  x3d_widget_bind_caps(sp_floor, ctx->ipc, X3D_CAP_FLOOR_FREQ);

  g_cpu.tb_sp_dynepp = GTK_TOGGLE_BUTTON(sp_dynepp);
  g_cpu.tb_sp_boost = GTK_TOGGLE_BUTTON(sp_boost);

  GtkWidget *sp_grid_items[] = {sp_dynepp, sp_boost, sp_floor};
  x3d_staged_widget_set_grid_items(g_cpu.stg_sp_standalone, sp_grid_items, 3);

  x3d_staged_widget_watch_control(g_cpu.stg_sp_standalone,
                                  GTK_WIDGET(g_cpu.dd_sp_pstate));
  x3d_staged_widget_watch_control(g_cpu.stg_sp_standalone,
                                  GTK_WIDGET(g_cpu.dd_sp_gov));
  x3d_staged_widget_watch_control(g_cpu.stg_sp_standalone,
                                  GTK_WIDGET(g_cpu.dd_sp_epp));
  x3d_staged_widget_watch_control(g_cpu.stg_sp_standalone, sp_dynepp);
  x3d_staged_widget_watch_control(g_cpu.stg_sp_standalone, sp_boost);
  gtk_box_append(sp_page, sp_stg);

  g_signal_connect(g_cpu.tb_sp_dynepp, "toggled",
                   G_CALLBACK(on_dynepp_sp_toggled), NULL);
  g_signal_connect(g_cpu.stg_sp_standalone, "default",
                   G_CALLBACK(on_sp_standalone_default), NULL);
  g_signal_connect(g_cpu.stg_sp_standalone, "cancel",
                   G_CALLBACK(on_sp_standalone_cancel), NULL);
  g_signal_connect(g_cpu.stg_sp_standalone, "apply",
                   G_CALLBACK(on_sp_standalone_apply), NULL);
  g_signal_connect(g_cpu.stg_sp_standalone, "close",
                   G_CALLBACK(on_sp_standalone_close), NULL);
  g_signal_connect(sp_floor, "clicked", G_CALLBACK(on_sp_floor_clicked), NULL);

  x3d_widgets_tree_connect_dirty_notify(g_cpu.cpu_root, ctx->notify_dirty_cb,
                                        ctx->notify_dirty_data);
  x3d_widgets_tree_connect_dirty_notify(
      g_cpu.sp_standalone_root, ctx->notify_dirty_cb, ctx->notify_dirty_data);

  return g_cpu.cpu_root;
}

static void mod_cpu_deactivate(const X3dPageCtx *ctx) {
  (void)ctx;
  if (g_cpu.cpu_timer_id != 0) {
    g_source_remove(g_cpu.cpu_timer_id);
    g_cpu.cpu_timer_id = 0;
  }
}

static void mod_cpu_refresh(const X3dPageCtx *ctx) {
  if (ctx)
    g_cpu.ctx = *ctx;
  const char *vis =
      g_cpu.ctx.view
          ? gtk_stack_get_visible_child_name(g_cpu.ctx.view->content_stack)
          : "cpu";
  if (vis && strcmp(vis, "system_profile_view") == 0) {
    load_sysprof_pair(g_cpu.dd_sp_pstate, g_cpu.dd_sp_gov, g_cpu.dd_sp_epp,
                      g_cpu.tb_sp_dynepp, g_cpu.tb_sp_boost,
                      g_cpu.stg_sp_standalone, FALSE);
  } else if (!vis || strcmp(vis, "cpu") == 0) {
    load_sysprof_pair(g_cpu.dd_cpu_pstate, g_cpu.dd_cpu_gov, g_cpu.dd_cpu_epp,
                      g_cpu.tb_cpu_dynepp, g_cpu.tb_cpu_boost,
                      g_cpu.stg_cpu_sysprof, FALSE);
    cpu_refresh_telemetry_card();
  }
}

static void mod_cpu_activate(const X3dPageCtx *ctx) {
  if (ctx)
    g_cpu.ctx = *ctx;
  mod_cpu_deactivate(ctx);
  mod_cpu_refresh(ctx);

  const char *vis =
      g_cpu.ctx.view
          ? gtk_stack_get_visible_child_name(g_cpu.ctx.view->content_stack)
          : NULL;
  if (vis && strcmp(vis, "cpu") == 0) {
    g_cpu.cpu_timer_id = g_timeout_add(
        x3d_gui_ipc_get_update_interval_ms(g_cpu.ctx.ipc), cpu_timer_cb, NULL);
  }
}

static void mod_cpu_on_interval_changed(const X3dPageCtx *ctx,
                                        guint interval_ms) {
  (void)ctx;
  if (g_cpu.cpu_timer_id != 0) {
    g_source_remove(g_cpu.cpu_timer_id);
    g_cpu.cpu_timer_id = g_timeout_add(interval_ms, cpu_timer_cb, NULL);
  }
}

static GtkWidget *active_cpu_subroot(void) {
  if (!g_cpu.ctx.view)
    return g_cpu.cpu_root;
  return gtk_stack_get_visible_child(g_cpu.ctx.view->content_stack);
}

static gboolean mod_cpu_has_dirty(void) {
  return x3d_widgets_tree_has_dirty(active_cpu_subroot());
}

static gboolean mod_cpu_has_staged(void) {
  return x3d_widgets_tree_has_staged(active_cpu_subroot());
}

static gboolean mod_cpu_apply_all(void) {
  return x3d_widgets_tree_apply_dirty(active_cpu_subroot());
}

static void mod_cpu_cancel_all(void) {
  x3d_widgets_tree_cancel_dirty(active_cpu_subroot());
}

static void mod_cpu_default_all(void) {
  x3d_widgets_tree_default_all(active_cpu_subroot());
}

const X3dPageModule *gui_cpu_get_module(void) {
  static const X3dPageModule k_mod = {
      .id = "cpu",
      .title = "CPU",
      .is_subnav = FALSE,
      .initially_hidden = FALSE,
      .divider_after = FALSE,
      .build = mod_cpu_build,
      .refresh = mod_cpu_refresh,
      .activate = mod_cpu_activate,
      .deactivate = mod_cpu_deactivate,
      .has_dirty = mod_cpu_has_dirty,
      .has_staged = mod_cpu_has_staged,
      .apply_all = mod_cpu_apply_all,
      .cancel_all = mod_cpu_cancel_all,
      .default_all = mod_cpu_default_all,
      .on_interval_changed = mod_cpu_on_interval_changed,
      .highlight_hw_mode = mod_cpu_highlight_hw_mode,
  };
  return &k_mod;
}
