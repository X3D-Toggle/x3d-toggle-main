/* KDE6 SNI Style, Desktop Widgets & 7-Mode Live Telemetry Visualizer
 * `popups/popup_style.c`
 */

#include "popup_style.h"
#include <math.h>
#include <stdio.h>

/* ── 1. KDE6 SNI Style PopUp ──────────────────────────────────── */

static void on_sni_style_dd(GObject *o, GParamSpec *p, gpointer d) {
  (void)p;
  x3d_stubs_get()->kde_sni_style_idx =
      gtk_drop_down_get_selected(GTK_DROP_DOWN(o));
  if (d)
    x3d_staged_widget_set_dirty(X3D_STAGED_WIDGET(d), TRUE);
}
static void on_sni_badge_tb(GtkToggleButton *b, gpointer d) {
  x3d_stubs_get()->kde_sni_show_badge = gtk_toggle_button_get_active(b);
  if (d)
    x3d_staged_widget_set_dirty(X3D_STAGED_WIDGET(d), TRUE);
}
static void on_sni_tip_tb(GtkToggleButton *b, gpointer d) {
  x3d_stubs_get()->kde_sni_rich_tooltip = gtk_toggle_button_get_active(b);
  if (d)
    x3d_staged_widget_set_dirty(X3D_STAGED_WIDGET(d), TRUE);
}

void popup_style_show_kde_sni(GtkWindow *parent,
                              X3dStagedWidget *owner_staged) {
  GtkBox *body = NULL;
  GtkWindow *win = gui_popups_create_shell(
      parent, "KDE6 SNI System Tray Style", 460, 320, TRUE, &body);

  const X3dStubState *st = x3d_stubs_get();
  GtkWidget *card = x3d_current_widget_new("TRAY APPEARANCE (STUB)", FALSE);
  GtkBox *cbody = x3d_current_widget_get_body(X3D_CURRENT_WIDGET(card));

  const char *styles[] = {"Obsidian Emblem", "Imperial Gold Crest",
                          "Breeze Symbolic Monochrome", NULL};
  GtkWidget *dd = gtk_drop_down_new_from_strings(styles);
  gtk_drop_down_set_selected(GTK_DROP_DOWN(dd), st->kde_sni_style_idx);
  g_signal_connect(dd, "notify::selected", G_CALLBACK(on_sni_style_dd),
                   owner_staged);
  gtk_box_append(cbody,
                 x3d_form_row_new("Icon Set", "StatusNotifierItem icon theme",
                                  dd));

  GtkWidget *tb_badge = x3d_toggle_new("Active Mode Badge");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tb_badge),
                               st->kde_sni_show_badge);
  g_signal_connect(tb_badge, "toggled", G_CALLBACK(on_sni_badge_tb),
                   owner_staged);

  GtkWidget *tb_tip = x3d_toggle_new("Live Telemetry Tooltip");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tb_tip),
                               st->kde_sni_rich_tooltip);
  g_signal_connect(tb_tip, "toggled", G_CALLBACK(on_sni_tip_tb), owner_staged);

  GtkWidget *titems[] = {tb_badge, tb_tip};
  GtkWidget *tgrid = x3d_centered_3col_grid_new();
  x3d_grid_pack_centered_3col(GTK_GRID(tgrid), titems, 2);
  gtk_widget_set_margin_top(tgrid, 8);
  gtk_box_append(cbody, tgrid);

  gtk_box_append(body, card);

  GtkWidget *close_btn = gtk_button_new_with_label("Done");
  gtk_widget_add_css_class(close_btn, "x3d-primary");
  gtk_widget_set_halign(close_btn, GTK_ALIGN_END);
  g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(gtk_window_close),
                           win);
  gtk_box_append(body, close_btn);

  gtk_window_present(win);
}

/* ── 2. Dual-Pathway Desktop Widgets Configuration PopUp ──────── */

static void on_wmod_sw_toggled(GtkToggleButton *b, gpointer d) {
  x3d_stubs_get()->widget_mod_switcher = gtk_toggle_button_get_active(b);
  if (d)
    x3d_staged_widget_set_dirty(X3D_STAGED_WIDGET(d), TRUE);
}
static void on_wmod_tel_toggled(GtkToggleButton *b, gpointer d) {
  x3d_stubs_get()->widget_mod_telemetry = gtk_toggle_button_get_active(b);
  if (d)
    x3d_staged_widget_set_dirty(X3D_STAGED_WIDGET(d), TRUE);
}
static void on_wmod_aod_toggled(GtkToggleButton *b, gpointer d) {
  x3d_stubs_get()->widget_mod_aod = gtk_toggle_button_get_active(b);
  if (d)
    x3d_staged_widget_set_dirty(X3D_STAGED_WIDGET(d), TRUE);
}

void popup_style_show_desktop_widgets(GtkWindow *parent, X3dGuiIpc *ipc,
                                      X3dStagedWidget *owner_staged) {
  (void)ipc;
  GtkBox *body = NULL;
  GtkWindow *win = gui_popups_create_shell(
      parent, "Desktop Widgets (GTK4 Pinned & KDE6 Plasmoid)", 540, 380, TRUE,
      &body);

  const X3dStubState *st = x3d_stubs_get();
  GtkWidget *card = x3d_current_widget_new("WIDGET MODULES & PATHWAYS", FALSE);
  GtkBox *cbody = x3d_current_widget_get_body(X3D_CURRENT_WIDGET(card));

  GtkWidget *desc = gtk_label_new(
      "Select which sub-modules appear in the detached GTK4 'Pin to Desktop' "
      "window (non-Plasma desktops) and are registered via kpackagetool6 in "
      "the KDE6 Plasma Widget Manager (kdialog addon, Phase 6).");
  gtk_label_set_wrap(GTK_LABEL(desc), TRUE);
  gtk_label_set_xalign(GTK_LABEL(desc), 0.0f);
  gtk_widget_add_css_class(desc, "x3d-gpu-subtitle");
  gtk_box_append(cbody, desc);

  GtkWidget *tb_sw = x3d_toggle_new("Mode Quick-Switcher");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tb_sw),
                               st->widget_mod_switcher);
  g_signal_connect(tb_sw, "toggled", G_CALLBACK(on_wmod_sw_toggled),
                   owner_staged);

  GtkWidget *tb_tel = x3d_toggle_new("CCD / v-Cache Telemetry");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tb_tel),
                               st->widget_mod_telemetry);
  g_signal_connect(tb_tel, "toggled", G_CALLBACK(on_wmod_tel_toggled),
                   owner_staged);

  GtkWidget *tb_aod = x3d_toggle_new("AOD Power Readout");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tb_aod), st->widget_mod_aod);
  g_signal_connect(tb_aod, "toggled", G_CALLBACK(on_wmod_aod_toggled),
                   owner_staged);

  GtkWidget *items[] = {tb_sw, tb_tel, tb_aod};
  GtkWidget *grid = x3d_centered_3col_grid_new();
  x3d_grid_pack_centered_3col(GTK_GRID(grid), items, 3);
  gtk_widget_set_margin_top(grid, 8);
  gtk_box_append(cbody, grid);

  gtk_box_append(body, card);

  GtkWidget *close_btn = gtk_button_new_with_label("Done");
  gtk_widget_add_css_class(close_btn, "x3d-primary");
  gtk_widget_set_halign(close_btn, GTK_ALIGN_END);
  g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(gtk_window_close),
                           win);
  gtk_box_append(body, close_btn);

  gtk_window_present(win);
}

/* ── 3. Live Telemetry Visualizer PopUp (7 Modes via Cairo) ───── */

#define TELEM_MAX_METRICS 6
#define TELEM_HIST_LEN 60
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
  char name[32];
  char unit[16];
  double val;
  double max_val;
  double pct;
} TelemMetric;

typedef struct {
  X3dGuiIpc *ipc;
  X3dTelemetryKind kind;
  gboolean has_gpu;
  X3dGpuInfo gpu;

  guint mode_idx;
  GtkWidget *stack;
  GtkWidget *lbl_text;
  GtkWidget *lbl_subtitle;
  GtkWidget *drawing_area;

  guint metric_count;
  TelemMetric metrics[TELEM_MAX_METRICS];
  double hist[TELEM_MAX_METRICS][TELEM_HIST_LEN];
  guint hist_count;
  guint hist_head;

  guint timer_id;
} TelemPopupCtx;

static const double k_telem_colors[TELEM_MAX_METRICS][3] = {
    {0.910, 0.380, 0.102}, /* #e8611a Ryzen Orange */
    {0.239, 0.682, 0.914}, /* #3daee9 Plasma Cyan  */
    {0.290, 0.871, 0.502}, /* #4ade80 Emerald      */
    {0.961, 0.620, 0.043}, /* #f59e0b Amber Gold   */
    {0.655, 0.545, 0.980}, /* #a78bfa Violet       */
    {0.973, 0.443, 0.443}  /* #f87171 Coral Red    */
};

static void telem_set_metric(TelemPopupCtx *ctx, guint idx, const char *name,
                             const char *unit, double val, double max_val) {
  if (idx >= TELEM_MAX_METRICS)
    return;
  TelemMetric *m = &ctx->metrics[idx];
  snprintf(m->name, sizeof(m->name), "%s", name);
  snprintf(m->unit, sizeof(m->unit), "%s", unit);
  m->val = (val > 0.0) ? val : 0.0;
  m->max_val = (max_val > 0.0) ? max_val : 100.0;
  m->pct = CLAMP(m->val / m->max_val, 0.0, 1.0);
}

static void telem_record_history(TelemPopupCtx *ctx) {
  for (guint i = 0; i < ctx->metric_count; i++)
    ctx->hist[i][ctx->hist_head] = ctx->metrics[i].pct;
  ctx->hist_head = (ctx->hist_head + 1) % TELEM_HIST_LEN;
  if (ctx->hist_count < TELEM_HIST_LEN)
    ctx->hist_count++;
}

static gboolean telem_popup_poll_cb(gpointer data) {
  TelemPopupCtx *ctx = (TelemPopupCtx *)data;
  if (!ctx || !ctx->ipc)
    return G_SOURCE_REMOVE;

  gchar *markup = NULL;
  if (ctx->kind == X3D_TELEM_CPU) {
    X3dCpuTelemetry t;
    gui_popups_sample_cpu(ctx->ipc, &t);
    ctx->metric_count = 6;
    telem_set_metric(ctx, 0, "Total Load", "%", t.total_load_pct, 100.0);
    telem_set_metric(ctx, 1, "CCD0 Load", "%", t.ccd0_load_pct, 100.0);
    telem_set_metric(ctx, 2, "CCD1 Load", "%", t.ccd1_load_pct, 100.0);
    telem_set_metric(ctx, 3, "Peak Clock", "MHz", t.peak_clock_mhz,
                     t.max_clock_mhz > 0.0 ? t.max_clock_mhz : 5700.0);
    telem_set_metric(ctx, 4, "Tctl Temp", "°C", t.temp_tctl_c, 95.0);
    telem_set_metric(ctx, 5, "PPT Power", "W", t.ppt_w,
                     t.ppt_lim_w > 0.0 ? t.ppt_lim_w : 170.0);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_subtitle), t.model_name);
    markup = g_markup_printf_escaped(
        "<span font_family=\"monospace\">"
        "<b>Topology:</b>   %s\n"
        "<b>CPU Load:</b>   %s\n"
        "<b>Clocks:</b>     %s\n"
        "<b>Temps:</b>      %s\n"
        "<b>Power:</b>      %s\n"
        "<b>Profile:</b>    %s"
        "</span>",
        t.topology_str, t.load_str, t.clocks_str, t.temps_str, t.power_str,
        t.profile_str);
  } else if (ctx->kind == X3D_TELEM_GPU) {
    X3dGpuTelemetry t;
    gui_popups_sample_gpu(ctx->has_gpu ? &ctx->gpu : NULL, &t);
    ctx->metric_count = 6;
    telem_set_metric(ctx, 0, "GPU Load", "%", t.load_pct, 100.0);
    telem_set_metric(ctx, 1, "Core Clock", "MHz", t.sclk_mhz, 3200.0);
    telem_set_metric(ctx, 2, "VRAM Used", "GiB", t.vram_used_gib,
                     t.vram_total_gib > 0.0 ? t.vram_total_gib : 24.0);
    telem_set_metric(ctx, 3, "Power (PPT)", "W", t.power_w,
                     t.power_cap_w > 0.0 ? t.power_cap_w : 350.0);
    telem_set_metric(ctx, 4, "Hotspot", "°C",
                     t.temp_hotspot_c > 0.0 ? t.temp_hotspot_c : t.temp_edge_c,
                     110.0);
    telem_set_metric(ctx, 5, "Fan PWM", "%", t.fan_pct, 100.0);
    if (ctx->has_gpu)
      gtk_label_set_text(GTK_LABEL(ctx->lbl_subtitle), ctx->gpu.label);
    markup = g_markup_printf_escaped(
        "<span font_family=\"monospace\">"
        "<b>GPU Load:</b>     %s\n"
        "<b>Clocks:</b>       %s\n"
        "<b>VRAM Usage:</b>   %s\n"
        "<b>Power (PPT):</b>  %s\n"
        "<b>Temperatures:</b> %s\n"
        "<b>Fan Speed:</b>    %s"
        "</span>",
        t.load_str, t.clocks_str, t.vram_str, t.power_str, t.temps_str,
        t.fan_speed);
  } else {
    AodLiveStats a;
    x3d_ipc_read_aod_live(&a);
    gboolean wmi_on =
        (a.wmi_online || x3d_gui_ipc_get_aod_supported(ctx->ipc));
    ctx->metric_count = 5;
    telem_set_metric(ctx, 0, "PPT Power", "W", a.has_ppt ? a.ppt_val_w : 0.0,
                     a.ppt_lim_w > 0.0 ? a.ppt_lim_w : 230.0);
    telem_set_metric(ctx, 1, "TDC Current", "A", a.has_tdc ? a.tdc_val_a : 0.0,
                     a.tdc_lim_a > 0.0 ? a.tdc_lim_a : 160.0);
    telem_set_metric(ctx, 2, "EDC Current", "A", a.has_edc ? a.edc_val_a : 0.0,
                     a.edc_lim_a > 0.0 ? a.edc_lim_a : 225.0);
    telem_set_metric(ctx, 3, "Mem Clock", "MHz",
                     a.has_mem ? (double)a.mem_mhz : 0.0, 4000.0);
    telem_set_metric(ctx, 4, "AOD WMI", "%", wmi_on ? 100.0 : 0.0, 100.0);

    char ppt_s[48], tdc_s[48], edc_s[48], mem_s[32];
    if (a.has_ppt && a.ppt_lim_w > 0.0)
      snprintf(ppt_s, sizeof(ppt_s), "%.1f W / %.0f W", a.ppt_val_w,
               a.ppt_lim_w);
    else if (a.has_ppt)
      snprintf(ppt_s, sizeof(ppt_s), "%.1f W", a.ppt_val_w);
    else
      snprintf(ppt_s, sizeof(ppt_s), "N/A");

    if (a.has_tdc && a.tdc_lim_a > 0.0)
      snprintf(tdc_s, sizeof(tdc_s), "%.1f A / %.0f A", a.tdc_val_a,
               a.tdc_lim_a);
    else if (a.has_tdc)
      snprintf(tdc_s, sizeof(tdc_s), "%.1f A", a.tdc_val_a);
    else
      snprintf(tdc_s, sizeof(tdc_s), "N/A");

    if (a.has_edc && a.edc_lim_a > 0.0)
      snprintf(edc_s, sizeof(edc_s), "%.1f A / %.0f A", a.edc_val_a,
               a.edc_lim_a);
    else if (a.has_edc)
      snprintf(edc_s, sizeof(edc_s), "%.1f A", a.edc_val_a);
    else
      snprintf(edc_s, sizeof(edc_s), "N/A");

    if (a.has_mem)
      snprintf(mem_s, sizeof(mem_s), "%d MHz", a.mem_mhz);
    else
      snprintf(mem_s, sizeof(mem_s), "N/A");

    gtk_label_set_text(GTK_LABEL(ctx->lbl_subtitle),
                       wmi_on ? "AMD Overclocking (AOD) WMI — ONLINE"
                              : "SMU / RAPL Live Fallback — AOD WMI OFFLINE");
    markup = g_markup_printf_escaped(
        "<span font_family=\"monospace\">"
        "<b>PPT:</b>         %s\n"
        "<b>TDC:</b>         %s\n"
        "<b>EDC:</b>         %s\n"
        "<b>Mem Clock:</b>   %s\n"
        "<b>AOD Status:</b>  %s"
        "</span>",
        ppt_s, tdc_s, edc_s, mem_s, wmi_on ? "ONLINE" : "OFFLINE");
  }

  telem_record_history(ctx);
  if (markup) {
    gtk_label_set_markup(GTK_LABEL(ctx->lbl_text), markup);
    g_free(markup);
  }
  if (ctx->mode_idx != 0)
    gtk_widget_queue_draw(ctx->drawing_area);

  return G_SOURCE_CONTINUE;
}

static void draw_graph_mode(cairo_t *cr, const TelemPopupCtx *ctx, double w,
                            double h) {
  const double pad_l = 44.0, pad_r = 16.0, pad_t = 44.0, pad_b = 26.0;
  double gw = w - pad_l - pad_r;
  double gh = h - pad_t - pad_b;
  if (gw < 40.0 || gh < 40.0)
    return;

  cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 10.5);
  double lx = pad_l;
  double ly = 16.0;
  for (guint i = 0; i < ctx->metric_count; i++) {
    const TelemMetric *m = &ctx->metrics[i];
    cairo_set_source_rgb(cr, k_telem_colors[i][0], k_telem_colors[i][1],
                         k_telem_colors[i][2]);
    cairo_rectangle(cr, lx, ly - 8.0, 9.0, 9.0);
    cairo_fill(cr);

    char item[64];
    snprintf(item, sizeof(item), "%s %.0f%s", m->name, m->val, m->unit);
    cairo_set_source_rgba(cr, 0.90, 0.92, 0.94, 0.95);
    cairo_move_to(cr, lx + 13.0, ly);
    cairo_show_text(cr, item);
    lx += (w - pad_l - pad_r) / 3.0;
    if ((i + 1) % 3 == 0) {
      lx = pad_l;
      ly += 16.0;
    }
  }

  cairo_set_line_width(cr, 1.0);
  cairo_set_font_size(cr, 10.0);
  for (int g = 0; g <= 4; g++) {
    double frac = (double)g / 4.0;
    double y = pad_t + gh * (1.0 - frac);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.10);
    cairo_move_to(cr, pad_l, y);
    cairo_line_to(cr, pad_l + gw, y);
    cairo_stroke(cr);

    char glbl[16];
    snprintf(glbl, sizeof(glbl), "%d%%", g * 25);
    cairo_set_source_rgba(cr, 0.72, 0.76, 0.80, 0.85);
    cairo_move_to(cr, 6.0, y + 3.5);
    cairo_show_text(cr, glbl);
  }

  if (ctx->hist_count < 2)
    return;

  guint n = ctx->hist_count;
  guint start = (ctx->hist_head + TELEM_HIST_LEN - n) % TELEM_HIST_LEN;

  cairo_new_path(cr);
  for (guint s = 0; s < n; s++) {
    guint slot = (start + s) % TELEM_HIST_LEN;
    double x = pad_l + gw * ((double)s / (double)(TELEM_HIST_LEN - 1));
    double y = pad_t + gh * (1.0 - ctx->hist[0][slot]);
    if (s == 0)
      cairo_move_to(cr, x, y);
    else
      cairo_line_to(cr, x, y);
  }
  double x_last =
      pad_l + gw * ((double)(n - 1) / (double)(TELEM_HIST_LEN - 1));
  cairo_line_to(cr, x_last, pad_t + gh);
  cairo_line_to(cr, pad_l, pad_t + gh);
  cairo_close_path(cr);
  cairo_set_source_rgba(cr, k_telem_colors[0][0], k_telem_colors[0][1],
                        k_telem_colors[0][2], 0.16);
  cairo_fill(cr);

  for (guint i = 0; i < ctx->metric_count; i++) {
    cairo_set_source_rgba(cr, k_telem_colors[i][0], k_telem_colors[i][1],
                          k_telem_colors[i][2], (i == 0) ? 1.0 : 0.85);
    cairo_set_line_width(cr, (i == 0) ? 2.4 : 1.7);
    for (guint s = 0; s < n; s++) {
      guint slot = (start + s) % TELEM_HIST_LEN;
      double x = pad_l + gw * ((double)s / (double)(TELEM_HIST_LEN - 1));
      double y = pad_t + gh * (1.0 - ctx->hist[i][slot]);
      if (s == 0)
        cairo_move_to(cr, x, y);
      else
        cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);
  }
}

static void draw_pie_mode(cairo_t *cr, const TelemPopupCtx *ctx, double w,
                          double h) {
  double cx = w * 0.34;
  double cy = h * 0.50;
  double r = MIN(w * 0.28, h * 0.40);
  if (r < 20.0)
    return;

  double total = 0.0;
  for (guint i = 0; i < ctx->metric_count; i++)
    total += MAX(ctx->metrics[i].pct, 0.02);

  double ang = -M_PI / 2.0;
  for (guint i = 0; i < ctx->metric_count; i++) {
    double weight = MAX(ctx->metrics[i].pct, 0.02);
    double sweep = (weight / total) * (2.0 * M_PI);
    cairo_move_to(cr, cx, cy);
    cairo_arc(cr, cx, cy, r, ang, ang + sweep);
    cairo_close_path(cr);
    cairo_set_source_rgba(cr, k_telem_colors[i][0], k_telem_colors[i][1],
                          k_telem_colors[i][2], 0.90);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.08, 0.09, 0.11, 0.95);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
    ang += sweep;
  }

  double lx = w * 0.64;
  double ly = cy - (double)ctx->metric_count * 16.0;
  cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 11.5);
  for (guint i = 0; i < ctx->metric_count; i++) {
    const TelemMetric *m = &ctx->metrics[i];
    double share = (MAX(m->pct, 0.02) / total) * 100.0;
    cairo_set_source_rgb(cr, k_telem_colors[i][0], k_telem_colors[i][1],
                         k_telem_colors[i][2]);
    cairo_rectangle(cr, lx, ly - 10.0, 12.0, 12.0);
    cairo_fill(cr);

    char txt[80];
    snprintf(txt, sizeof(txt), "%-11s %.1f%s (%.0f%%)", m->name, m->val, m->unit,
             share);
    cairo_set_source_rgba(cr, 0.92, 0.94, 0.96, 0.95);
    cairo_move_to(cr, lx + 18.0, ly);
    cairo_show_text(cr, txt);
    ly += 30.0;
  }
}

static void draw_chart_mode(cairo_t *cr, const TelemPopupCtx *ctx, double w,
                            double h) {
  const double pad_l = 44.0, pad_r = 20.0, pad_t = 32.0, pad_b = 46.0;
  double gw = w - pad_l - pad_r;
  double gh = h - pad_t - pad_b;
  if (gw < 40.0 || gh < 40.0 || ctx->metric_count == 0)
    return;

  cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 10.0);
  for (int g = 0; g <= 4; g++) {
    double frac = (double)g / 4.0;
    double y = pad_t + gh * (1.0 - frac);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.10);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, pad_l, y);
    cairo_line_to(cr, pad_l + gw, y);
    cairo_stroke(cr);

    char glbl[16];
    snprintf(glbl, sizeof(glbl), "%d%%", g * 25);
    cairo_set_source_rgba(cr, 0.72, 0.76, 0.80, 0.85);
    cairo_move_to(cr, 6.0, y + 3.5);
    cairo_show_text(cr, glbl);
  }

  double slot_w = gw / (double)ctx->metric_count;
  double bar_w = MIN(slot_w * 0.56, 56.0);
  for (guint i = 0; i < ctx->metric_count; i++) {
    const TelemMetric *m = &ctx->metrics[i];
    double cx = pad_l + slot_w * ((double)i + 0.5);
    double bh = MAX(gh * m->pct, 3.0);
    double bx = cx - bar_w * 0.5;
    double by = pad_t + gh - bh;

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.05);
    cairo_rectangle(cr, bx, pad_t, bar_w, gh);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, k_telem_colors[i][0], k_telem_colors[i][1],
                          k_telem_colors[i][2], 0.90);
    cairo_rectangle(cr, bx, by, bar_w, bh);
    cairo_fill(cr);

    char val_lbl[32];
    snprintf(val_lbl, sizeof(val_lbl), "%.0f%s", m->val, m->unit);
    cairo_text_extents_t ext;
    cairo_set_font_size(cr, 10.5);
    cairo_text_extents(cr, val_lbl, &ext);
    cairo_set_source_rgba(cr, 0.95, 0.96, 0.98, 0.95);
    cairo_move_to(cr, cx - ext.width * 0.5, by - 6.0);
    cairo_show_text(cr, val_lbl);

    cairo_set_font_size(cr, 10.0);
    cairo_text_extents(cr, m->name, &ext);
    cairo_set_source_rgba(cr, 0.78, 0.82, 0.86, 0.90);
    cairo_move_to(cr, cx - ext.width * 0.5, pad_t + gh + 18.0);
    cairo_show_text(cr, m->name);
  }
}

static void draw_breakdown_mode(cairo_t *cr, const TelemPopupCtx *ctx, double w,
                                double h) {
  if (ctx->metric_count == 0)
    return;
  const double pad_x = 24.0;
  double row_h = (h - 24.0) / (double)ctx->metric_count;
  double bar_h = MIN(16.0, row_h * 0.34);

  cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  for (guint i = 0; i < ctx->metric_count; i++) {
    const TelemMetric *m = &ctx->metrics[i];
    double y0 = 16.0 + (double)i * row_h;

    cairo_set_font_size(cr, 11.5);
    cairo_set_source_rgba(cr, 0.92, 0.94, 0.96, 0.95);
    cairo_move_to(cr, pad_x, y0 + 13.0);
    cairo_show_text(cr, m->name);

    char rtxt[64];
    snprintf(rtxt, sizeof(rtxt), "%.1f / %.0f %s (%.0f%%)", m->val, m->max_val,
             m->unit, m->pct * 100.0);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, rtxt, &ext);
    cairo_set_source_rgb(cr, k_telem_colors[i][0], k_telem_colors[i][1],
                         k_telem_colors[i][2]);
    cairo_move_to(cr, w - pad_x - ext.width, y0 + 13.0);
    cairo_show_text(cr, rtxt);

    double bw = w - 2.0 * pad_x;
    double by = y0 + 20.0;
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.10);
    cairo_rectangle(cr, pad_x, by, bw, bar_h);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, k_telem_colors[i][0], k_telem_colors[i][1],
                          k_telem_colors[i][2], 0.92);
    cairo_rectangle(cr, pad_x, by, MAX(bw * m->pct, 4.0), bar_h);
    cairo_fill(cr);
  }
}

static void draw_polygon_mode(cairo_t *cr, const TelemPopupCtx *ctx, double w,
                              double h) {
  guint n = ctx->metric_count;
  if (n < 3)
    return;
  double cx = w * 0.50;
  double cy = h * 0.52;
  double r = MIN(w, h) * 0.34;
  if (r < 24.0)
    return;

  cairo_set_line_width(cr, 1.0);
  for (int ring = 1; ring <= 4; ring++) {
    double rr = r * ((double)ring / 4.0);
    cairo_new_path(cr);
    for (guint i = 0; i < n; i++) {
      double ang = -M_PI / 2.0 + (2.0 * M_PI * (double)i) / (double)n;
      double x = cx + cos(ang) * rr;
      double y = cy + sin(ang) * rr;
      if (i == 0)
        cairo_move_to(cr, x, y);
      else
        cairo_line_to(cr, x, y);
    }
    cairo_close_path(cr);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.12);
    cairo_stroke(cr);
  }

  cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 10.5);
  for (guint i = 0; i < n; i++) {
    double ang = -M_PI / 2.0 + (2.0 * M_PI * (double)i) / (double)n;
    double x = cx + cos(ang) * r;
    double y = cy + sin(ang) * r;
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.14);
    cairo_move_to(cr, cx, cy);
    cairo_line_to(cr, x, y);
    cairo_stroke(cr);

    char lbl[48];
    snprintf(lbl, sizeof(lbl), "%s (%.0f%s)", ctx->metrics[i].name,
             ctx->metrics[i].val, ctx->metrics[i].unit);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, lbl, &ext);
    double lx = cx + cos(ang) * (r + 16.0);
    double ly = cy + sin(ang) * (r + 16.0);
    if (cos(ang) < -0.25)
      lx -= ext.width;
    else if (fabs(cos(ang)) <= 0.25)
      lx -= ext.width * 0.5;
    if (sin(ang) > 0.25)
      ly += ext.height * 0.5;
    cairo_set_source_rgba(cr, 0.90, 0.93, 0.96, 0.95);
    cairo_move_to(cr, lx, ly);
    cairo_show_text(cr, lbl);
  }

  cairo_new_path(cr);
  for (guint i = 0; i < n; i++) {
    double ang = -M_PI / 2.0 + (2.0 * M_PI * (double)i) / (double)n;
    double rr = r * MAX(ctx->metrics[i].pct, 0.06);
    double x = cx + cos(ang) * rr;
    double y = cy + sin(ang) * rr;
    if (i == 0)
      cairo_move_to(cr, x, y);
    else
      cairo_line_to(cr, x, y);
  }
  cairo_close_path(cr);
  cairo_set_source_rgba(cr, k_telem_colors[0][0], k_telem_colors[0][1],
                        k_telem_colors[0][2], 0.28);
  cairo_fill_preserve(cr);
  cairo_set_source_rgba(cr, k_telem_colors[0][0], k_telem_colors[0][1],
                        k_telem_colors[0][2], 0.95);
  cairo_set_line_width(cr, 2.2);
  cairo_stroke(cr);

  for (guint i = 0; i < n; i++) {
    double ang = -M_PI / 2.0 + (2.0 * M_PI * (double)i) / (double)n;
    double rr = r * MAX(ctx->metrics[i].pct, 0.06);
    cairo_set_source_rgb(cr, k_telem_colors[i][0], k_telem_colors[i][1],
                         k_telem_colors[i][2]);
    cairo_arc(cr, cx + cos(ang) * rr, cy + sin(ang) * rr, 4.0, 0, 2.0 * M_PI);
    cairo_fill(cr);
  }
}

static void draw_concentric_ring_mode(cairo_t *cr, const TelemPopupCtx *ctx,
                                      double w, double h) {
  guint n = ctx->metric_count;
  if (n == 0)
    return;
  double cx = w * 0.34;
  double cy = h * 0.50;
  double max_r = MIN(w * 0.28, h * 0.42);
  double ring_step = (max_r * 0.72) / (double)n;
  double lw = MAX(ring_step * 0.64, 5.0);
  double start_a = -M_PI * 0.75;
  double span_a = M_PI * 1.50;

  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  for (guint i = 0; i < n; i++) {
    double r = max_r - (double)i * ring_step;
    if (r < 12.0)
      break;
    cairo_set_line_width(cr, lw);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.10);
    cairo_arc(cr, cx, cy, r, start_a, start_a + span_a);
    cairo_stroke(cr);

    double sweep = span_a * MAX(ctx->metrics[i].pct, 0.015);
    cairo_set_source_rgba(cr, k_telem_colors[i][0], k_telem_colors[i][1],
                          k_telem_colors[i][2], 0.95);
    cairo_arc(cr, cx, cy, r, start_a, start_a + sweep);
    cairo_stroke(cr);
  }

  cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  char center_txt[32];
  snprintf(center_txt, sizeof(center_txt), "%.0f%s", ctx->metrics[0].val,
           ctx->metrics[0].unit);
  cairo_text_extents_t ext;
  cairo_set_font_size(cr, 15.0);
  cairo_text_extents(cr, center_txt, &ext);
  cairo_set_source_rgba(cr, 0.95, 0.97, 0.99, 0.95);
  cairo_move_to(cr, cx - ext.width * 0.5, cy + ext.height * 0.4);
  cairo_show_text(cr, center_txt);

  double lx = w * 0.64;
  double ly = cy - (double)n * 15.0;
  cairo_set_font_size(cr, 11.5);
  for (guint i = 0; i < n; i++) {
    const TelemMetric *m = &ctx->metrics[i];
    cairo_set_source_rgb(cr, k_telem_colors[i][0], k_telem_colors[i][1],
                         k_telem_colors[i][2]);
    cairo_arc(cr, lx + 5.0, ly - 4.0, 5.0, 0, 2.0 * M_PI);
    cairo_fill(cr);

    char txt[80];
    snprintf(txt, sizeof(txt), "%-11s %.1f%s (%.0f%%)", m->name, m->val, m->unit,
             m->pct * 100.0);
    cairo_set_source_rgba(cr, 0.92, 0.94, 0.96, 0.95);
    cairo_move_to(cr, lx + 16.0, ly);
    cairo_show_text(cr, txt);
    ly += 30.0;
  }
}

static void on_telem_draw(GtkDrawingArea *area, cairo_t *cr, int width,
                          int height, gpointer data) {
  (void)area;
  const TelemPopupCtx *ctx = (const TelemPopupCtx *)data;
  double w = (double)width;
  double h = (double)height;

  cairo_set_source_rgba(cr, 0.07, 0.08, 0.10, 0.96);
  cairo_rectangle(cr, 0, 0, w, h);
  cairo_fill(cr);

  switch (ctx->mode_idx) {
  case 1:
    draw_graph_mode(cr, ctx, w, h);
    break;
  case 2:
    draw_pie_mode(cr, ctx, w, h);
    break;
  case 3:
    draw_chart_mode(cr, ctx, w, h);
    break;
  case 4:
    draw_breakdown_mode(cr, ctx, w, h);
    break;
  case 5:
    draw_polygon_mode(cr, ctx, w, h);
    break;
  case 6:
    draw_concentric_ring_mode(cr, ctx, w, h);
    break;
  default:
    break;
  }
}

static void on_telem_mode_changed(GObject *dd, GParamSpec *pspec,
                                  gpointer data) {
  (void)pspec;
  TelemPopupCtx *ctx = (TelemPopupCtx *)data;
  ctx->mode_idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
  gtk_stack_set_visible_child_name(GTK_STACK(ctx->stack),
                                   (ctx->mode_idx == 0) ? "text" : "canvas");
  if (ctx->mode_idx != 0)
    gtk_widget_queue_draw(ctx->drawing_area);
}

static void on_telem_interval_changed(const X3dGuiIpc *ipc, GParamSpec *pspec,
                                      gpointer user_data) {
  (void)pspec;
  GtkWindow *win = GTK_WINDOW(user_data);
  TelemPopupCtx *ctx = g_object_get_data(G_OBJECT(win), "x3d-telem-ctx");
  if (!ctx)
    return;
  if (ctx->timer_id != 0)
    g_source_remove(ctx->timer_id);
  ctx->timer_id = g_timeout_add(x3d_gui_ipc_get_update_interval_ms(ipc),
                                telem_popup_poll_cb, ctx);
}

static void on_telem_popup_destroy(GtkWidget *w, gpointer data) {
  (void)w;
  TelemPopupCtx *ctx = (TelemPopupCtx *)data;
  if (ctx->timer_id != 0)
    g_source_remove(ctx->timer_id);
  g_free(ctx);
}

void popup_style_show_telemetry(GtkWindow *parent, X3dGuiIpc *ipc,
                                X3dTelemetryKind kind,
                                const X3dGpuInfo *gpu_opt) {
  const char *title = "Live Telemetry Popout";
  if (kind == X3D_TELEM_CPU)
    title = "CPU Live Telemetry Visualizer";
  else if (kind == X3D_TELEM_GPU)
    title = "GPU Live Telemetry Visualizer";
  else if (kind == X3D_TELEM_AOD)
    title = "AOD Power & Current Visualizer";

  GtkBox *body = NULL;
  GtkWindow *win =
      gui_popups_create_shell(parent, title, 640, 480, FALSE, &body);

  TelemPopupCtx *ctx = g_new0(TelemPopupCtx, 1);
  ctx->ipc = ipc;
  ctx->kind = kind;
  if (gpu_opt) {
    ctx->has_gpu = TRUE;
    ctx->gpu = *gpu_opt;
  }
  g_object_set_data(G_OBJECT(win), "x3d-telem-ctx", ctx);

  GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  ctx->lbl_subtitle = gtk_label_new("Streaming live hardware sensors...");
  gtk_widget_add_css_class(ctx->lbl_subtitle, "x3d-gpu-subtitle");
  gtk_label_set_xalign(GTK_LABEL(ctx->lbl_subtitle), 0.0f);
  gtk_label_set_ellipsize(GTK_LABEL(ctx->lbl_subtitle), PANGO_ELLIPSIZE_END);
  gtk_widget_set_hexpand(ctx->lbl_subtitle, TRUE);
  gtk_box_append(GTK_BOX(bar), ctx->lbl_subtitle);

  const char *modes[] = {"Text",      "Graph",   "Pie",
                         "Chart",     "Breakdown", "Polygon",
                         "Concentric Ring", NULL};
  GtkWidget *dd_mode = gtk_drop_down_new_from_strings(modes);
  gtk_drop_down_set_selected(GTK_DROP_DOWN(dd_mode), 1);
  ctx->mode_idx = 1;
  g_signal_connect(dd_mode, "notify::selected",
                   G_CALLBACK(on_telem_mode_changed), ctx);
  gtk_box_append(GTK_BOX(bar), dd_mode);
  gtk_box_append(body, bar);

  ctx->stack = gtk_stack_new();
  gtk_widget_set_vexpand(ctx->stack, TRUE);
  gtk_widget_set_hexpand(ctx->stack, TRUE);

  GtkWidget *text_card = x3d_current_widget_new("LIVE TELEMETRY READOUT", FALSE);
  ctx->lbl_text = gtk_label_new("Sampling...");
  gtk_label_set_xalign(GTK_LABEL(ctx->lbl_text), 0.0f);
  gtk_label_set_yalign(GTK_LABEL(ctx->lbl_text), 0.0f);
  gtk_label_set_selectable(GTK_LABEL(ctx->lbl_text), TRUE);
  gtk_widget_add_css_class(ctx->lbl_text, "x3d-value");
  gtk_box_append(x3d_current_widget_get_body(X3D_CURRENT_WIDGET(text_card)),
                 ctx->lbl_text);
  gtk_stack_add_named(GTK_STACK(ctx->stack), text_card, "text");

  GtkWidget *canvas_frame = gtk_frame_new(NULL);
  gtk_widget_add_css_class(canvas_frame, "x3d-extras-frame");
  gtk_widget_set_hexpand(canvas_frame, TRUE);
  gtk_widget_set_vexpand(canvas_frame, TRUE);
  ctx->drawing_area = gtk_drawing_area_new();
  gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(ctx->drawing_area), 580);
  gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(ctx->drawing_area), 340);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(ctx->drawing_area),
                                 on_telem_draw, ctx, NULL);
  gtk_frame_set_child(GTK_FRAME(canvas_frame), ctx->drawing_area);
  gtk_stack_add_named(GTK_STACK(ctx->stack), canvas_frame, "canvas");

  gtk_stack_set_visible_child_name(GTK_STACK(ctx->stack), "canvas");
  gtk_box_append(body, ctx->stack);

  telem_popup_poll_cb(ctx);
  ctx->timer_id = g_timeout_add(x3d_gui_ipc_get_update_interval_ms(ipc),
                                telem_popup_poll_cb, ctx);
  g_signal_connect_object(ipc, "notify::update-interval-ms",
                          G_CALLBACK(on_telem_interval_changed), win, 0);
  g_signal_connect(win, "destroy", G_CALLBACK(on_telem_popup_destroy), ctx);

  gtk_window_present(win);
}
