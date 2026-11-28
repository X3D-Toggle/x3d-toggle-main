/* GPU Page Slice Implementation
 * `pages/gui_gpu.c`
 */

#include "gui_gpu.h"
#include "../gui_popups.h"
#include "gui_widgets.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct {
  X3dPageCtx ctx;
  GtkWidget *root;
  GtkDropDown *dd_gpu_select;
  GtkWidget *lbl_gpu_meta;
  GtkWidget *lbl_gpu_telemetry;
  GtkWidget *btn_gpu_lact;
  GtkWidget *btn_gpu_corectrl;
  GtkWidget *lbl_gpu_no_tools;

  X3dGpuInfo *gpus;
  guint gpu_count;
  gboolean updating_dd;
  guint gpu_timer_id;
} g_gpu;

static gboolean is_digits_only(const char *s) {
  if (!s || !s[0])
    return FALSE;
  for (size_t i = 0; s[i]; i++) {
    if (s[i] < '0' || s[i] > '9')
      return FALSE;
  }
  return TRUE;
}

static gboolean gpu_read_file(const char *path, char *out, size_t out_sz) {
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

static gboolean gpu_read_ll(const char *dir, const char *attr,
                            long long *val_out) {
  char path[640], buf[64];
  snprintf(path, sizeof(path), "%s/%s", dir, attr);
  if (!gpu_read_file(path, buf, sizeof(buf)))
    return FALSE;
  char *endptr = NULL;
  errno = 0;
  long long v = strtoll(buf, &endptr, 10);
  if (endptr == buf || errno == ERANGE)
    return FALSE;
  *val_out = v;
  return TRUE;
}

static void gpu_read_pci_slot(const char *card_name, char *slot_out,
                              size_t slot_sz) {
  char path[512], buf[1024];
  snprintf(path, sizeof(path), "/sys/class/drm/%s/device/uevent", card_name);
  if (!gpu_read_file(path, buf, sizeof(buf))) {
    snprintf(slot_out, slot_sz, "unknown");
    return;
  }
  const char *p = buf;
  while (*p) {
    const char *nl = strchr(p, '\n');
    size_t len = nl ? (size_t)(nl - p) : strlen(p);
    if (len > 14 && strncmp(p, "PCI_SLOT_NAME=", 14) == 0) {
      size_t slen = len - 14;
      if (p[14 + slen - 1] == '\r')
        slen--;
      if (slen >= slot_sz)
        slen = slot_sz - 1;
      memcpy(slot_out, p + 14, slen);
      slot_out[slen] = '\0';
      return;
    }
    if (!nl)
      break;
    p = nl + 1;
  }
  snprintf(slot_out, slot_sz, "unknown");
}

static int cmp_gpu_info(const void *a, const void *b) {
  const X3dGpuInfo *ga = (const X3dGpuInfo *)a;
  const X3dGpuInfo *gb = (const X3dGpuInfo *)b;
  int ca = atoi(ga->card_name + 4);
  int cb = atoi(gb->card_name + 4);
  if (ca != cb)
    return (ca < cb) ? -1 : 1;
  return strcmp(ga->hwmon_path, gb->hwmon_path);
}

static guint discover_gpus(X3dGpuInfo **gpus_out, guint *preferred_idx_out) {
  *gpus_out = NULL;
  if (preferred_idx_out)
    *preferred_idx_out = 0;

  X3dGpuInfo *gpus = NULL;
  guint count = 0;
  DIR *drm_dir = opendir("/sys/class/drm");
  if (drm_dir) {
    const struct dirent *de;
    while ((de = readdir(drm_dir)) != NULL) {
      if (strncmp(de->d_name, "card", 4) != 0 ||
          !is_digits_only(de->d_name + 4))
        continue;
      char hwmon_base[512];
      snprintf(hwmon_base, sizeof(hwmon_base), "/sys/class/drm/%s/device/hwmon",
               de->d_name);
      DIR *hw_dir = opendir(hwmon_base);
      if (!hw_dir)
        continue;
      const struct dirent *he;
      while ((he = readdir(hw_dir)) != NULL) {
        if (strncmp(he->d_name, "hwmon", 5) != 0 ||
            !is_digits_only(he->d_name + 5))
          continue;
        char name_path[640], drv_name[64];
        snprintf(name_path, sizeof(name_path), "%s/%s/name", hwmon_base,
                 he->d_name);
        if (!gpu_read_file(name_path, drv_name, sizeof(drv_name)))
          continue;
        drv_name[strcspn(drv_name, "\r\n")] = '\0';
        if (strcmp(drv_name, "amdgpu") != 0)
          continue;

        X3dGpuInfo info;
        memset(&info, 0, sizeof(info));
        snprintf(info.hwmon_path, sizeof(info.hwmon_path), "%s/%s", hwmon_base,
                 he->d_name);
        snprintf(info.card_name, sizeof(info.card_name), "%s", de->d_name);
        gpu_read_pci_slot(de->d_name, info.pci_slot, sizeof(info.pci_slot));

        char fan_path[640], cap_path[640];
        snprintf(fan_path, sizeof(fan_path), "%s/fan1_input", info.hwmon_path);
        snprintf(cap_path, sizeof(cap_path), "%s/power1_cap", info.hwmon_path);
        info.is_discrete =
            (access(fan_path, R_OK) == 0 || access(cap_path, R_OK) == 0);
        snprintf(info.label, sizeof(info.label), "AMD GPU (%s - %s)",
                 info.pci_slot, info.is_discrete ? "Discrete" : "Integrated");

        gpus = g_renew(X3dGpuInfo, gpus, count + 1);
        gpus[count++] = info;
      }
      closedir(hw_dir);
    }
    closedir(drm_dir);
  }

  if (count > 1)
    qsort(gpus, count, sizeof(X3dGpuInfo), cmp_gpu_info);

  if (preferred_idx_out) {
    for (guint i = 0; i < count; i++) {
      if (gpus[i].is_discrete) {
        *preferred_idx_out = i;
        break;
      }
    }
  }
  *gpus_out = gpus;
  return count;
}

static void read_gpu_telemetry(const X3dGpuInfo *gpu, X3dGpuTelemetry *out) {
  if (!out)
    return;
  memset(out, 0, sizeof(*out));
  snprintf(out->load_str, sizeof(out->load_str), "N/A");
  snprintf(out->clocks_str, sizeof(out->clocks_str), "N/A");
  snprintf(out->vram_str, sizeof(out->vram_str), "N/A");
  snprintf(out->power_str, sizeof(out->power_str), "N/A");
  snprintf(out->temps_str, sizeof(out->temps_str), "N/A");
  snprintf(out->fan_speed, sizeof(out->fan_speed), "N/A");
  if (!gpu)
    return;

  char dev_dir[512];
  snprintf(dev_dir, sizeof(dev_dir), "/sys/class/drm/%s/device",
           gpu->card_name);

  long long busy = 0;
  if (gpu_read_ll(dev_dir, "gpu_busy_percent", &busy)) {
    long long clamped = CLAMP(busy, 0LL, 100LL);
    out->load_pct = (double)clamped;
    snprintf(out->load_str, sizeof(out->load_str), "%lld%%", clamped);
  }

  long long sclk = 0, mclk = 0;
  gboolean has_sclk = gpu_read_ll(gpu->hwmon_path, "freq1_input", &sclk);
  gboolean has_mclk = gpu_read_ll(gpu->hwmon_path, "freq2_input", &mclk);
  if (has_sclk)
    out->sclk_mhz = (double)sclk / 1000000.0;
  if (has_mclk)
    out->mclk_mhz = (double)mclk / 1000000.0;
  if (has_sclk && has_mclk) {
    snprintf(out->clocks_str, sizeof(out->clocks_str),
             "%lld MHz (Core) / %lld MHz (VRAM)", sclk / 1000000LL,
             mclk / 1000000LL);
  } else if (has_sclk) {
    snprintf(out->clocks_str, sizeof(out->clocks_str), "%lld MHz (Core)",
             sclk / 1000000LL);
  }

  long long vram_used = 0, vram_tot = 0;
  if (gpu_read_ll(dev_dir, "mem_info_vram_used", &vram_used) &&
      gpu_read_ll(dev_dir, "mem_info_vram_total", &vram_tot) && vram_tot > 0) {
    out->vram_used_gib = (double)vram_used / 1073741824.0;
    out->vram_total_gib = (double)vram_tot / 1073741824.0;
    if (vram_tot >= 1073741824LL) {
      snprintf(out->vram_str, sizeof(out->vram_str), "%.2f / %.2f GiB",
               out->vram_used_gib, out->vram_total_gib);
    } else {
      snprintf(out->vram_str, sizeof(out->vram_str), "%lld / %lld MiB",
               vram_used / 1048576LL, vram_tot / 1048576LL);
    }
  }

  long long pwr_uw = 0, cap_uw = 0;
  gboolean has_pwr = gpu_read_ll(gpu->hwmon_path, "power1_input", &pwr_uw) ||
                     gpu_read_ll(gpu->hwmon_path, "power1_average", &pwr_uw);
  gboolean has_cap = gpu_read_ll(gpu->hwmon_path, "power1_cap", &cap_uw);
  if (has_pwr)
    out->power_w = (double)pwr_uw / 1000000.0;
  if (has_cap && cap_uw > 0)
    out->power_cap_w = (double)cap_uw / 1000000.0;
  if (has_pwr && has_cap && cap_uw > 0) {
    snprintf(out->power_str, sizeof(out->power_str), "%.1f W / %.0f W",
             out->power_w, out->power_cap_w);
  } else if (has_pwr) {
    snprintf(out->power_str, sizeof(out->power_str), "%.1f W", out->power_w);
  }

  long long t1 = 0, t2 = 0, t3 = 0;
  gboolean has_t1 = gpu_read_ll(gpu->hwmon_path, "temp1_input", &t1);
  gboolean has_t2 = gpu_read_ll(gpu->hwmon_path, "temp2_input", &t2);
  gboolean has_t3 = gpu_read_ll(gpu->hwmon_path, "temp3_input", &t3);
  if (has_t1)
    out->temp_edge_c = (double)t1 / 1000.0;
  if (has_t2)
    out->temp_hotspot_c = (double)t2 / 1000.0;
  if (has_t3)
    out->temp_mem_c = (double)t3 / 1000.0;
  if (has_t1 && has_t2 && has_t3) {
    snprintf(out->temps_str, sizeof(out->temps_str),
             "%.0f °C Edge  •  %.0f °C Hotspot  •  %.0f °C Mem",
             out->temp_edge_c, out->temp_hotspot_c, out->temp_mem_c);
  } else if (has_t1 && has_t2) {
    snprintf(out->temps_str, sizeof(out->temps_str),
             "%.1f °C Edge  •  %.1f °C Hotspot", out->temp_edge_c,
             out->temp_hotspot_c);
  } else if (has_t1) {
    snprintf(out->temps_str, sizeof(out->temps_str), "%.1f °C Edge",
             out->temp_edge_c);
  }

  long long fan_rpm = 0, pwm_raw = 0;
  gboolean has_fan = gpu_read_ll(gpu->hwmon_path, "fan1_input", &fan_rpm);
  gboolean has_pwm = gpu_read_ll(gpu->hwmon_path, "pwm1", &pwm_raw);
  if (has_fan)
    out->fan_rpm = (double)fan_rpm;
  if (has_pwm)
    out->fan_pct = (double)CLAMP((pwm_raw * 100 + 127) / 255, 0, 100);
  if (has_fan && has_pwm) {
    long long pct = CLAMP((pwm_raw * 100 + 127) / 255, 0, 100);
    if (fan_rpm == 0 && pct == 0)
      snprintf(out->fan_speed, sizeof(out->fan_speed), "0 RPM (Zero-RPM Idle)");
    else
      snprintf(out->fan_speed, sizeof(out->fan_speed), "%lld RPM (%lld%%)",
               fan_rpm, pct);
  } else if (has_fan) {
    snprintf(out->fan_speed, sizeof(out->fan_speed), "%lld RPM", fan_rpm);
  } else if (has_pwm) {
    long long pct = CLAMP((pwm_raw * 100 + 127) / 255, 0, 100);
    snprintf(out->fan_speed, sizeof(out->fan_speed), "%lld%%", pct);
  } else if (!gpu->is_discrete) {
    snprintf(out->fan_speed, sizeof(out->fan_speed), "Passive / Shared");
  }
}

static void refresh_gpu_card(void) {
  if (!g_gpu.gpus || g_gpu.gpu_count == 0) {
    gtk_widget_set_visible(g_gpu.lbl_gpu_meta, FALSE);
    gtk_label_set_text(GTK_LABEL(g_gpu.lbl_gpu_telemetry),
                       "  No supported AMD GPU detected");
    return;
  }
  guint idx = gtk_drop_down_get_selected(g_gpu.dd_gpu_select);
  if (idx == GTK_INVALID_LIST_POSITION || idx >= g_gpu.gpu_count)
    idx = 0;

  const X3dGpuInfo *gpu = &g_gpu.gpus[idx];
  char meta[192];
  snprintf(meta, sizeof(meta), "%s  •  PCI %s", gpu->card_name, gpu->pci_slot);
  gtk_label_set_text(GTK_LABEL(g_gpu.lbl_gpu_meta), meta);
  gtk_widget_set_visible(g_gpu.lbl_gpu_meta, TRUE);

  X3dGpuTelemetry tel;
  read_gpu_telemetry(gpu, &tel);
  gchar *m = g_markup_printf_escaped("<span font_family=\"monospace\">"
                                     "  <b>GPU Load:</b>       %s\n"
                                     "  <b>Clocks:</b>         %s\n"
                                     "  <b>VRAM Usage:</b>     %s\n"
                                     "  <b>Power (PPT):</b>    %s\n"
                                     "  <b>Temperatures:</b>   %s\n"
                                     "  <b>Fan Speed:</b>      %s"
                                     "</span>",
                                     tel.load_str, tel.clocks_str, tel.vram_str,
                                     tel.power_str, tel.temps_str,
                                     tel.fan_speed);
  gtk_label_set_markup(GTK_LABEL(g_gpu.lbl_gpu_telemetry), m);
  g_free(m);
}

static gboolean gpu_timer_cb(gpointer data) {
  (void)data;
  const char *vis =
      g_gpu.ctx.view
          ? gtk_stack_get_visible_child_name(g_gpu.ctx.view->content_stack)
          : NULL;
  if (!vis || strcmp(vis, "gpu") != 0) {
    g_gpu.gpu_timer_id = 0;
    return G_SOURCE_REMOVE;
  }
  refresh_gpu_card();
  return G_SOURCE_CONTINUE;
}

static void on_gpu_dd_changed(GObject *o, GParamSpec *p, gpointer d) {
  (void)o;
  (void)p;
  (void)d;
  if (!g_gpu.updating_dd)
    refresh_gpu_card();
}

static void on_gpu_dash_activated(X3dCurrentWidget *w, gpointer d) {
  (void)w;
  (void)d;
  const X3dGpuInfo *gpu = NULL;
  if (g_gpu.gpus && g_gpu.gpu_count > 0) {
    guint idx = gtk_drop_down_get_selected(g_gpu.dd_gpu_select);
    if (idx == GTK_INVALID_LIST_POSITION || idx >= g_gpu.gpu_count)
      idx = 0;
    gpu = &g_gpu.gpus[idx];
  }
  gui_popups_show_telemetry(g_gpu.ctx.view->main_window, g_gpu.ctx.ipc,
                            X3D_TELEM_GPU, gpu);
}

static void on_gpu_lact_clicked(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  x3d_ipc_launch_desktop_tool("lact", "gui",
                              "io.github.ilya_zlobintsev.LACT.desktop");
}

static void on_gpu_cc_clicked(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  x3d_ipc_launch_desktop_tool("corectrl", NULL,
                              "org.corectrl.CoreCtrl.desktop");
}

static GtkWidget *mod_gpu_build(const X3dPageCtx *ctx) {
  g_gpu.ctx = *ctx;
  gui_popups_register_samplers(NULL, read_gpu_telemetry);

  GtkBox *page = x3d_page_vbox_new(ctx->view->content_stack, "gpu", "GPU");
  g_gpu.root = GTK_WIDGET(page);

  GtkWidget *w_dash = x3d_current_widget_new("GPU DASHBOARD", TRUE);
  g_gpu.dd_gpu_select = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
  x3d_current_widget_add_header_suffix(X3D_CURRENT_WIDGET(w_dash),
                                       GTK_WIDGET(g_gpu.dd_gpu_select));

  GtkBox *body = x3d_current_widget_get_body(X3D_CURRENT_WIDGET(w_dash));
  g_gpu.lbl_gpu_meta = gtk_label_new("");
  gtk_widget_add_css_class(g_gpu.lbl_gpu_meta, "x3d-gpu-subtitle");
  gtk_label_set_xalign(GTK_LABEL(g_gpu.lbl_gpu_meta), 0.0f);
  gtk_widget_set_visible(g_gpu.lbl_gpu_meta, FALSE);

  g_gpu.lbl_gpu_telemetry = gtk_label_new("Scanning AMD GPUs...");
  gtk_widget_add_css_class(g_gpu.lbl_gpu_telemetry, "x3d-value");
  gtk_label_set_xalign(GTK_LABEL(g_gpu.lbl_gpu_telemetry), 0.0f);

  gtk_box_append(body, g_gpu.lbl_gpu_meta);
  gtk_box_append(body, g_gpu.lbl_gpu_telemetry);
  gtk_box_append(page, w_dash);

  GtkWidget *launch_grid = x3d_centered_3col_grid_new();
  g_gpu.btn_gpu_lact = gtk_button_new_with_label("LACT");
  g_gpu.btn_gpu_corectrl = gtk_button_new_with_label("CoreCtrl");
  gtk_widget_add_css_class(g_gpu.btn_gpu_lact, "x3d-primary");
  gtk_widget_add_css_class(g_gpu.btn_gpu_corectrl, "x3d-primary");

  GtkWidget *lbtns[] = {g_gpu.btn_gpu_lact, g_gpu.btn_gpu_corectrl};
  x3d_grid_pack_centered_3col(GTK_GRID(launch_grid), lbtns, 2);
  gtk_box_append(page, launch_grid);

  g_gpu.lbl_gpu_no_tools =
      gtk_label_new("No supported GPU launcher installed (LACT, CoreCtrl)");
  gtk_widget_add_css_class(g_gpu.lbl_gpu_no_tools, "x3d-gpu-subtitle");
  gtk_widget_set_halign(g_gpu.lbl_gpu_no_tools, GTK_ALIGN_CENTER);
  gtk_widget_set_visible(g_gpu.lbl_gpu_no_tools, FALSE);
  gtk_box_append(page, g_gpu.lbl_gpu_no_tools);

  g_signal_connect(w_dash, "activated", G_CALLBACK(on_gpu_dash_activated),
                   NULL);
  g_signal_connect(g_gpu.dd_gpu_select, "notify::selected",
                   G_CALLBACK(on_gpu_dd_changed), NULL);
  g_signal_connect(g_gpu.btn_gpu_lact, "clicked",
                   G_CALLBACK(on_gpu_lact_clicked), NULL);
  g_signal_connect(g_gpu.btn_gpu_corectrl, "clicked",
                   G_CALLBACK(on_gpu_cc_clicked), NULL);

  return g_gpu.root;
}

static void mod_gpu_deactivate(const X3dPageCtx *ctx) {
  (void)ctx;
  if (g_gpu.gpu_timer_id != 0) {
    g_source_remove(g_gpu.gpu_timer_id);
    g_gpu.gpu_timer_id = 0;
  }
  g_free(g_gpu.gpus);
  g_gpu.gpus = NULL;
  g_gpu.gpu_count = 0;
}

static void mod_gpu_activate(const X3dPageCtx *ctx) {
  if (ctx)
    g_gpu.ctx = *ctx;
  mod_gpu_deactivate(ctx);

  guint pref = 0;
  g_gpu.gpu_count = discover_gpus(&g_gpu.gpus, &pref);

  if (g_gpu.gpu_count == 0) {
    gtk_widget_set_visible(GTK_WIDGET(g_gpu.dd_gpu_select), FALSE);
  } else {
    GtkStringList *sl = gtk_string_list_new(NULL);
    for (guint i = 0; i < g_gpu.gpu_count; i++)
      gtk_string_list_append(sl, g_gpu.gpus[i].label);
    g_gpu.updating_dd = TRUE;
    gtk_drop_down_set_model(g_gpu.dd_gpu_select, G_LIST_MODEL(sl));
    g_object_unref(sl);
    gtk_drop_down_set_selected(g_gpu.dd_gpu_select, pref);
    g_gpu.updating_dd = FALSE;
    gtk_widget_set_visible(GTK_WIDGET(g_gpu.dd_gpu_select), TRUE);
  }

  gboolean has_lact = x3d_ipc_is_tool_available(
      "lact", "io.github.ilya_zlobintsev.LACT.desktop");
  gboolean has_cc = x3d_ipc_is_tool_available(
      "corectrl", "org.corectrl.CoreCtrl.desktop");
  gtk_widget_set_visible(g_gpu.btn_gpu_lact, has_lact);
  gtk_widget_set_visible(g_gpu.btn_gpu_corectrl, has_cc);
  gtk_widget_set_visible(g_gpu.lbl_gpu_no_tools, !has_lact && !has_cc);

  refresh_gpu_card();
  if (g_gpu.gpu_count > 0) {
    g_gpu.gpu_timer_id = g_timeout_add(
        x3d_gui_ipc_get_update_interval_ms(g_gpu.ctx.ipc), gpu_timer_cb, NULL);
  }
}

static void mod_gpu_refresh(const X3dPageCtx *ctx) {
  (void)ctx;
  refresh_gpu_card();
}

static void mod_gpu_on_interval_changed(const X3dPageCtx *ctx,
                                        guint interval_ms) {
  (void)ctx;
  if (g_gpu.gpu_timer_id != 0) {
    g_source_remove(g_gpu.gpu_timer_id);
    g_gpu.gpu_timer_id = g_timeout_add(interval_ms, gpu_timer_cb, NULL);
  }
}

static gboolean mod_gpu_has_dirty(void) { return FALSE; }
static gboolean mod_gpu_has_staged(void) { return FALSE; }
static gboolean mod_gpu_apply_all(void) { return TRUE; }
static void mod_gpu_cancel_all(void) {}
static void mod_gpu_default_all(void) {}

const X3dPageModule *gui_gpu_get_module(void) {
  static const X3dPageModule k_mod = {
      .id = "gpu",
      .title = "GPU",
      .is_subnav = FALSE,
      .initially_hidden = FALSE,
      .divider_after = TRUE,
      .build = mod_gpu_build,
      .refresh = mod_gpu_refresh,
      .activate = mod_gpu_activate,
      .deactivate = mod_gpu_deactivate,
      .has_dirty = mod_gpu_has_dirty,
      .has_staged = mod_gpu_has_staged,
      .apply_all = mod_gpu_apply_all,
      .cancel_all = mod_gpu_cancel_all,
      .default_all = mod_gpu_default_all,
      .on_interval_changed = mod_gpu_on_interval_changed,
      .highlight_hw_mode = NULL,
  };
  return &k_mod;
}
