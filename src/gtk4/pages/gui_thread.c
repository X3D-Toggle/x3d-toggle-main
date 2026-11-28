/* Thread Settings Page Slice Implementation
 * `pages/gui_thread.c`
 */

#include "gui_thread.h"
#include "../gui_popups.h"
#include "../gui_stubs.h"
#include "gui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int printf_sn(char *buf, size_t size, const char *fmt, ...);

typedef struct {
  gboolean irq_enabled;
  char gpu[48];
  char nvme[48];
  char usb[48];
  char nic[48];
  char audio[48];
} X3dIrqReadout;

static struct {
  X3dPageCtx ctx;
  GtkWidget *root;
  GtkWidget *lbl_irq_cells[6];

  X3dStagedWidget *stg_irq_vectors;
  GtkToggleButton *tb_irq_enable;
  GtkToggleButton *tb_irq_gpu;
  GtkToggleButton *tb_irq_nvme;
  GtkToggleButton *tb_irq_usb;
  GtkToggleButton *tb_irq_nic;
  GtkToggleButton *tb_irq_audio;

  X3dStagedWidget *stg_topology_map;
  GtkDropDown *dd_affinity_level;
  GtkWidget *lbl_topology_summary;
  char staged_topo_mask[256];
} g_thr;

static void read_irq_readout(X3dIrqReadout *out) {
  memset(out, 0, sizeof(*out));
  snprintf(out->gpu, sizeof(out->gpu), "Unbound");
  snprintf(out->nvme, sizeof(out->nvme), "Unbound");
  snprintf(out->usb, sizeof(out->usb), "Unbound");
  snprintf(out->nic, sizeof(out->nic), "Unbound");
  snprintf(out->audio, sizeof(out->audio), "Unbound");

  char resp[512] = {0};
  if (x3d_ipc_send("IRQ_STATUS", resp, sizeof(resp)) != 0 || resp[0] == '\0' ||
      strcmp(resp, "ERR") == 0) {
    return;
  }

  char en_s[8] = {0};
  if (x3d_ipc_kv_get(resp, "EN", '|', en_s, sizeof(en_s)))
    out->irq_enabled = (atoi(en_s) != 0);
  x3d_ipc_kv_get(resp, "GPU", '|', out->gpu, sizeof(out->gpu));
  x3d_ipc_kv_get(resp, "NVME", '|', out->nvme, sizeof(out->nvme));
  x3d_ipc_kv_get(resp, "USB", '|', out->usb, sizeof(out->usb));
  x3d_ipc_kv_get(resp, "NIC", '|', out->nic, sizeof(out->nic));
  x3d_ipc_kv_get(resp, "AUDIO", '|', out->audio, sizeof(out->audio));
}

static void render_irq_readout(const X3dIrqReadout *ro) {
  static const char *const keys[6] = {"IRQ Affinity", "GPU", "NVMe",
                                      "USB",          "NIC", "Audio"};
  const char *vals[6] = {
      ro->irq_enabled ? "Enabled" : "Disabled",
      ro->gpu,
      ro->nvme,
      ro->usb,
      ro->nic,
      ro->audio,
  };
  for (int i = 0; i < 6; i++) {
    if (!g_thr.lbl_irq_cells[i])
      continue;
    gchar *m = g_markup_printf_escaped(
        "<span font_family=\"monospace\"><b>%s:</b> %s</span>", keys[i],
        vals[i]);
    gtk_label_set_markup(GTK_LABEL(g_thr.lbl_irq_cells[i]), m);
    g_free(m);
  }
}

static gboolean apply_irq_vectors(gboolean enable, gboolean gpu, gboolean nvme,
                                  gboolean usb, gboolean nic, gboolean audio) {
  gboolean ok = TRUE;
  if (!x3d_ipc_config_set("IRQ_ENABLE", enable ? "1" : "0"))
    ok = FALSE;
  if (!x3d_ipc_config_set("IRQ_GPU", gpu ? "1" : "0"))
    ok = FALSE;
  if (!x3d_ipc_config_set("IRQ_NVME", nvme ? "1" : "0"))
    ok = FALSE;
  if (!x3d_ipc_config_set("IRQ_USB", usb ? "1" : "0"))
    ok = FALSE;
  if (!x3d_ipc_config_set("IRQ_NIC", nic ? "1" : "0"))
    ok = FALSE;
  if (!x3d_ipc_config_set("IRQ_AUDIO", audio ? "1" : "0"))
    ok = FALSE;
  if (!x3d_ipc_cmd_ok(enable ? "IRQ_BIND" : "IRQ_RESET"))
    ok = FALSE;
  return ok;
}

static gboolean apply_affinity(guint level, const char *cache_mask,
                               const char *freq_mask) {
  const char *cm = (cache_mask && cache_mask[0]) ? cache_mask : "none";
  const char *fm = (freq_mask && freq_mask[0]) ? freq_mask : "none";
  gboolean ok = TRUE;

  char cmd[512];
  printf_sn(cmd, sizeof(cmd), "SET_AFFINITY_CONFIG %s %s", cm, fm);
  if (!x3d_ipc_cmd_ok(cmd))
    ok = FALSE;
  if (!x3d_ipc_config_set("AFFINITY_MASK", cm))
    ok = FALSE;
  if (!x3d_ipc_config_set("AFFINITY_FREQ_MASK", fm))
    ok = FALSE;
  char lvl_str[8];
  snprintf(lvl_str, sizeof(lvl_str), "%u", level);
  if (!x3d_ipc_config_set("AFFINITY_LEVEL", lvl_str))
    ok = FALSE;
  return ok;
}

static void mod_thread_refresh(const X3dPageCtx *ctx) {
  if (ctx)
    g_thr.ctx = *ctx;

  X3dIrqReadout irq_ro;
  read_irq_readout(&irq_ro);
  render_irq_readout(&irq_ro);

  x3d_staged_widget_set_syncing(g_thr.stg_irq_vectors, TRUE);
  gtk_toggle_button_set_active(g_thr.tb_irq_enable,
                               atoi(x3d_ipc_config_get("IRQ_ENABLE")) != 0);
  gtk_toggle_button_set_active(g_thr.tb_irq_gpu,
                               atoi(x3d_ipc_config_get("IRQ_GPU")) != 0);
  gtk_toggle_button_set_active(g_thr.tb_irq_nvme,
                               atoi(x3d_ipc_config_get("IRQ_NVME")) != 0);
  gtk_toggle_button_set_active(g_thr.tb_irq_usb,
                               atoi(x3d_ipc_config_get("IRQ_USB")) != 0);
  gtk_toggle_button_set_active(g_thr.tb_irq_nic,
                               atoi(x3d_ipc_config_get("IRQ_NIC")) != 0);
  gtk_toggle_button_set_active(g_thr.tb_irq_audio,
                               atoi(x3d_ipc_config_get("IRQ_AUDIO")) != 0);
  x3d_staged_widget_set_syncing(g_thr.stg_irq_vectors, FALSE);

  x3d_staged_widget_set_syncing(g_thr.stg_topology_map, TRUE);
  int lvl = atoi(x3d_ipc_config_get("AFFINITY_LEVEL"));
  gtk_drop_down_set_selected(g_thr.dd_affinity_level,
                             (guint)CLAMP(lvl, 0, 2));
  int ccd1 = 0, total = 0;
  if (x3d_ipc_read_topology(g_thr.staged_topo_mask,
                            sizeof(g_thr.staged_topo_mask), &ccd1,
                            &total) == 0) {
    gchar *s = g_strdup_printf("Staged Pin Mask: %s", g_thr.staged_topo_mask);
    gtk_label_set_text(GTK_LABEL(g_thr.lbl_topology_summary), s);
    g_free(s);
  }
  x3d_staged_widget_set_syncing(g_thr.stg_topology_map, FALSE);
}

static void on_irq_default(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  gtk_toggle_button_set_active(g_thr.tb_irq_enable, TRUE);
  gtk_toggle_button_set_active(g_thr.tb_irq_gpu, TRUE);
  gtk_toggle_button_set_active(g_thr.tb_irq_nvme, TRUE);
  gtk_toggle_button_set_active(g_thr.tb_irq_usb, TRUE);
  gtk_toggle_button_set_active(g_thr.tb_irq_nic, TRUE);
  gtk_toggle_button_set_active(g_thr.tb_irq_audio, TRUE);
}

static void on_irq_cancel(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  mod_thread_refresh(NULL);
}

static gboolean on_irq_apply(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  if (!apply_irq_vectors(gtk_toggle_button_get_active(g_thr.tb_irq_enable),
                         gtk_toggle_button_get_active(g_thr.tb_irq_gpu),
                         gtk_toggle_button_get_active(g_thr.tb_irq_nvme),
                         gtk_toggle_button_get_active(g_thr.tb_irq_usb),
                         gtk_toggle_button_get_active(g_thr.tb_irq_nic),
                         gtk_toggle_button_get_active(g_thr.tb_irq_audio))) {
    x3d_stubs_toast("Failed to apply IRQ Vectors (daemon IPC error)");
    return FALSE;
  }
  X3dIrqReadout irq_ro;
  read_irq_readout(&irq_ro);
  render_irq_readout(&irq_ro);
  return TRUE;
}

static void on_topo_popup_clicked(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  gui_popups_show_topology_map(g_thr.ctx.view->main_window, g_thr.ctx.ipc,
                               g_thr.stg_topology_map, g_thr.staged_topo_mask,
                               sizeof(g_thr.staged_topo_mask),
                               g_thr.lbl_topology_summary);
}

static void on_topo_default(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  gtk_drop_down_set_selected(g_thr.dd_affinity_level, 0);
  snprintf(g_thr.staged_topo_mask, sizeof(g_thr.staged_topo_mask), "none");
  gtk_label_set_text(GTK_LABEL(g_thr.lbl_topology_summary),
                     "Staged Pin Mask: Auto (Default)");
}

static void on_topo_cancel(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  mod_thread_refresh(NULL);
}

static gboolean on_topo_apply(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  guint lvl = gtk_drop_down_get_selected(g_thr.dd_affinity_level);
  gboolean ok;
  if (lvl == 0) {
    ok = apply_affinity(0, "none", "none");
  } else if (lvl == 1) {
    char cm[128] = {0}, fm[128] = {0};
    int c1 = 0, tot = 0;
    x3d_ipc_read_topology_masks(cm, sizeof(cm), fm, sizeof(fm), &c1, &tot);
    ok = apply_affinity(1, cm, fm);
  } else {
    ok = apply_affinity(2, g_thr.staged_topo_mask, "none");
  }
  if (!ok)
    x3d_stubs_toast(
        "Failed to apply Affinity & Topology settings (daemon IPC error)");
  return ok;
}

static GtkWidget *mod_thread_build(const X3dPageCtx *ctx) {
  g_thr.ctx = *ctx;

  GtkBox *page =
      x3d_page_vbox_new(ctx->view->content_stack, "threads", "Thread Settings");
  g_thr.root = GTK_WIDGET(page);

  GtkWidget *banner =
      x3d_current_widget_new("LIVE IRQ / CCD VERIFICATION READOUT", FALSE);
  gtk_widget_add_css_class(banner, "x3d-banner-widget");

  GtkWidget *irq_grid = gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(irq_grid), TRUE);
  gtk_grid_set_column_spacing(GTK_GRID(irq_grid), 16);
  gtk_grid_set_row_spacing(GTK_GRID(irq_grid), 6);
  gtk_widget_set_hexpand(irq_grid, TRUE);

  for (int i = 0; i < 6; i++) {
    g_thr.lbl_irq_cells[i] = gtk_label_new("Scanning...");
    gtk_widget_add_css_class(g_thr.lbl_irq_cells[i], "x3d-value");
    gtk_label_set_xalign(GTK_LABEL(g_thr.lbl_irq_cells[i]), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(g_thr.lbl_irq_cells[i]),
                            PANGO_ELLIPSIZE_END);
    gtk_grid_attach(GTK_GRID(irq_grid), g_thr.lbl_irq_cells[i], i % 3, i / 3, 1,
                    1);
  }
  gtk_box_append(x3d_current_widget_get_body(X3D_CURRENT_WIDGET(banner)),
                 irq_grid);
  gtk_box_append(page, banner);

  GtkWidget *stg_irq = x3d_staged_widget_new("IRQ VECTORS", FALSE);
  g_thr.stg_irq_vectors = X3D_STAGED_WIDGET(stg_irq);
  gtk_box_append(x3d_staged_widget_get_body(g_thr.stg_irq_vectors),
                 x3d_subsection_label_new("Hardware Affinity Controls"));

  GtkWidget *t_en = x3d_toggle_new("IRQ Subsystem");
  GtkWidget *t_gpu = x3d_toggle_new("GPU Interrupts");
  GtkWidget *t_nvme = x3d_toggle_new("NVMe Storage");
  GtkWidget *t_usb = x3d_toggle_new("USB Controllers");
  GtkWidget *t_nic = x3d_toggle_new("Network Interfaces");
  GtkWidget *t_aud = x3d_toggle_new("Audio Devices");

  g_thr.tb_irq_enable = GTK_TOGGLE_BUTTON(t_en);
  g_thr.tb_irq_gpu = GTK_TOGGLE_BUTTON(t_gpu);
  g_thr.tb_irq_nvme = GTK_TOGGLE_BUTTON(t_nvme);
  g_thr.tb_irq_usb = GTK_TOGGLE_BUTTON(t_usb);
  g_thr.tb_irq_nic = GTK_TOGGLE_BUTTON(t_nic);
  g_thr.tb_irq_audio = GTK_TOGGLE_BUTTON(t_aud);

  GtkWidget *irq_items[] = {t_en, t_gpu, t_nvme, t_usb, t_nic, t_aud};
  x3d_staged_widget_set_grid_items(g_thr.stg_irq_vectors, irq_items, 6);
  for (int i = 0; i < 6; i++)
    x3d_staged_widget_watch_control(g_thr.stg_irq_vectors, irq_items[i]);
  gtk_box_append(page, stg_irq);

  g_signal_connect(g_thr.stg_irq_vectors, "default", G_CALLBACK(on_irq_default),
                   NULL);
  g_signal_connect(g_thr.stg_irq_vectors, "cancel", G_CALLBACK(on_irq_cancel),
                   NULL);
  g_signal_connect(g_thr.stg_irq_vectors, "apply", G_CALLBACK(on_irq_apply),
                   NULL);

  GtkWidget *stg_topo =
      x3d_staged_widget_new("PINNING / AFFINITY TOPOLOGY MAP", FALSE);
  g_thr.stg_topology_map = X3D_STAGED_WIDGET(stg_topo);
  GtkBox *topo_body = x3d_staged_widget_get_body(g_thr.stg_topology_map);

  const char *aff_lvls[] = {"Auto", "By Die", "Manual", NULL};
  g_thr.dd_affinity_level =
      GTK_DROP_DOWN(gtk_drop_down_new_from_strings(aff_lvls));
  gtk_box_append(topo_body,
                 x3d_form_row_new("Affinity Level",
                                  "Thread placement strategy across CCDs",
                                  GTK_WIDGET(g_thr.dd_affinity_level)));
  x3d_staged_widget_watch_control(g_thr.stg_topology_map,
                                  GTK_WIDGET(g_thr.dd_affinity_level));

  g_thr.lbl_topology_summary = gtk_label_new("Staged Pin Mask: Auto");
  gtk_widget_add_css_class(g_thr.lbl_topology_summary, "x3d-gpu-subtitle");
  gtk_label_set_xalign(GTK_LABEL(g_thr.lbl_topology_summary), 0.0f);
  gtk_box_append(topo_body, g_thr.lbl_topology_summary);

  GtkWidget *btn_topo = x3d_popup_btn_new("Topology Pin/Bind Map");
  GtkWidget *topo_items[] = {btn_topo};
  x3d_staged_widget_set_grid_items(g_thr.stg_topology_map, topo_items, 1);
  gtk_box_append(page, stg_topo);

  g_signal_connect(btn_topo, "clicked", G_CALLBACK(on_topo_popup_clicked),
                   NULL);
  g_signal_connect(g_thr.stg_topology_map, "default",
                   G_CALLBACK(on_topo_default), NULL);
  g_signal_connect(g_thr.stg_topology_map, "cancel", G_CALLBACK(on_topo_cancel),
                   NULL);
  g_signal_connect(g_thr.stg_topology_map, "apply", G_CALLBACK(on_topo_apply),
                   NULL);

  x3d_widgets_tree_connect_dirty_notify(g_thr.root, ctx->notify_dirty_cb,
                                        ctx->notify_dirty_data);
  return g_thr.root;
}

static gboolean mod_thread_has_dirty(void) {
  return x3d_widgets_tree_has_dirty(g_thr.root);
}
static gboolean mod_thread_has_staged(void) {
  return x3d_widgets_tree_has_staged(g_thr.root);
}
static gboolean mod_thread_apply_all(void) {
  return x3d_widgets_tree_apply_dirty(g_thr.root);
}
static void mod_thread_cancel_all(void) {
  x3d_widgets_tree_cancel_dirty(g_thr.root);
}
static void mod_thread_default_all(void) {
  x3d_widgets_tree_default_all(g_thr.root);
}

const X3dPageModule *gui_thread_get_module(void) {
  static const X3dPageModule k_mod = {
      .id = "threads",
      .title = "Thread Settings",
      .is_subnav = FALSE,
      .initially_hidden = FALSE,
      .divider_after = FALSE,
      .build = mod_thread_build,
      .refresh = mod_thread_refresh,
      .activate = NULL,
      .deactivate = NULL,
      .has_dirty = mod_thread_has_dirty,
      .has_staged = mod_thread_has_staged,
      .apply_all = mod_thread_apply_all,
      .cancel_all = mod_thread_cancel_all,
      .default_all = mod_thread_default_all,
      .on_interval_changed = NULL,
      .highlight_hw_mode = NULL,
  };
  return &k_mod;
}
