/* Topology & Per-Core Frequency Floor Popups Implementation
 * `popups/popup_topology.c`
 */

#include "popup_topology.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── 1. Per-Core Frequency Floor (amd_pstate_floor_freq) PopUp ── */

typedef struct {
  X3dStagedWidget *owner;
  int core_idx;
} FloorCoreCtx;

static void free_floor_ctx(gpointer data, GClosure *closure) {
  (void)closure;
  g_free(data);
}

static void on_floor_spin_changed(GtkSpinButton *sp, gpointer data) {
  FloorCoreCtx *ctx = (FloorCoreCtx *)data;
  x3d_stubs_get()->floor_freq_khz[ctx->core_idx] =
      (int)(gtk_spin_button_get_value(sp) * 1000.0);
  if (ctx->owner)
    x3d_staged_widget_set_dirty(ctx->owner, TRUE);
}

void popup_topology_show_floor_freq(GtkWindow *parent,
                                    X3dStagedWidget *owner_staged) {
  GtkBox *body = NULL;
  GtkWindow *win = gui_popups_create_shell(
      parent, "Per-Core Frequency Floor (amd_pstate_floor_freq)", 560, 480,
      TRUE, &body);

  GtkWidget *note = gtk_label_new(
      "Set minimum nonlinear frequency floor (MHz) per logical CPU. "
      "Changes mark System Profile dirty and commit on Apply (Phase 6 stub).");
  gtk_label_set_wrap(GTK_LABEL(note), TRUE);
  gtk_label_set_xalign(GTK_LABEL(note), 0.0f);
  gtk_widget_add_css_class(note, "x3d-gpu-subtitle");
  gtk_box_append(body, note);

  char dummy[128] = {0};
  int ccd1 = 0, total = 0;
  if (x3d_ipc_read_topology(dummy, sizeof(dummy), &ccd1, &total) != 0 ||
      total <= 0)
    total = 16;
  if (total > 64)
    total = 64;

  const X3dStubState *st = x3d_stubs_get();
  GtkWidget *card = x3d_current_widget_new("PER-CORE FLOOR (MHZ)", FALSE);
  GtkBox *cbody = x3d_current_widget_get_body(X3D_CURRENT_WIDGET(card));

  for (int i = 0; i < total; i++) {
    char title[64];
    snprintf(title, sizeof(title), "CPU %02d (%s)", i,
             (ccd1 > 0 && i >= ccd1) ? "CCD1" : "CCD0");
    GtkWidget *sp = gtk_spin_button_new_with_range(400, 5800, 50);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(sp),
                              (double)(st->floor_freq_khz[i] / 1000));
    FloorCoreCtx *ctx = g_new0(FloorCoreCtx, 1);
    ctx->owner = owner_staged;
    ctx->core_idx = i;
    g_signal_connect_data(sp, "value-changed",
                          G_CALLBACK(on_floor_spin_changed), ctx,
                          free_floor_ctx, 0);
    gtk_box_append(cbody, x3d_form_row_new(title, NULL, sp));
  }
  gtk_box_append(body, card);

  GtkWidget *close_btn = gtk_button_new_with_label("Done");
  gtk_widget_add_css_class(close_btn, "x3d-primary");
  gtk_widget_set_halign(close_btn, GTK_ALIGN_END);
  g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(gtk_window_close),
                           win);
  gtk_box_append(body, close_btn);

  gtk_window_present(win);
}

/* ── 2. Topology Pin/Bind Map PopUp (Polls PREFCORE_RANKS via IPC) ── */

typedef struct {
  X3dGuiIpc *ipc;
  X3dStagedWidget *owner;
  char *mask_buf;
  size_t mask_sz;
  GtkWidget *summary_lbl;
  GtkWidget *prefcore_lbl;
  GtkGrid *core_grid;
  int total_cores;
  guint timer_id;
} TopoPopupCtx;

static void read_prefcore_rankings(int *ranks_out, int max_cores,
                                   int *count_out, gboolean *supported_out) {
  if (count_out)
    *count_out = 0;
  if (supported_out)
    *supported_out = FALSE;
  if (!ranks_out || max_cores <= 0)
    return;

  char resp[512] = {0};
  if (x3d_ipc_send("PREFCORE_RANKS", resp, sizeof(resp)) != 0 ||
      resp[0] == '\0' || strcmp(resp, "ERR") == 0) {
    return;
  }

  char v[384] = {0};
  if (supported_out && x3d_ipc_kv_get(resp, "SUPPORTED", '|', v, sizeof(v)))
    *supported_out = (atoi(v) != 0);

  if (x3d_ipc_kv_get(resp, "RANKS", '|', v, sizeof(v))) {
    int cnt = 0;
    const char *p = v;
    while (*p && cnt < max_cores) {
      ranks_out[cnt++] = atoi(p);
      const char *comma = strchr(p, ',');
      if (!comma)
        break;
      p = comma + 1;
    }
    if (count_out)
      *count_out = cnt;
  }
}

static gboolean mask_contains_core(const char *mask, int core) {
  if (!mask || !mask[0] || strcmp(mask, "none") == 0)
    return FALSE;
  gchar **parts = g_strsplit(mask, ",", -1);
  gboolean hit = FALSE;
  for (int i = 0; parts[i]; i++) {
    const char *hy = strchr(parts[i], '-');
    if (hy) {
      int a = atoi(parts[i]);
      int b = atoi(hy + 1);
      if (core >= a && core <= b) {
        hit = TRUE;
        break;
      }
    } else if (atoi(parts[i]) == core) {
      hit = TRUE;
      break;
    }
  }
  g_strfreev(parts);
  return hit;
}

static void rebuild_mask_from_grid(TopoPopupCtx *ctx) {
  if (!ctx || !ctx->core_grid || !ctx->mask_buf)
    return;
  GString *s = g_string_new("");
  for (int i = 0; i < ctx->total_cores; i++) {
    GtkWidget *w = gtk_grid_get_child_at(ctx->core_grid, i % 8, i / 8);
    if (w && GTK_IS_TOGGLE_BUTTON(w) &&
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
      if (s->len > 0)
        g_string_append_c(s, ',');
      g_string_append_printf(s, "%d", i);
    }
  }
  snprintf(ctx->mask_buf, ctx->mask_sz, "%s", s->len > 0 ? s->str : "none");
  if (ctx->summary_lbl) {
    gchar *sum = g_strdup_printf("Staged Pin Mask: %s", ctx->mask_buf);
    gtk_label_set_text(GTK_LABEL(ctx->summary_lbl), sum);
    g_free(sum);
  }
  if (ctx->owner)
    x3d_staged_widget_set_dirty(ctx->owner, TRUE);
  g_string_free(s, TRUE);
}

static void on_topo_core_toggled(GtkToggleButton *btn, gpointer data) {
  (void)btn;
  rebuild_mask_from_grid((TopoPopupCtx *)data);
}

static gboolean topo_popup_poll_cb(gpointer data) {
  TopoPopupCtx *ctx = (TopoPopupCtx *)data;
  if (!ctx || !ctx->prefcore_lbl)
    return G_SOURCE_REMOVE;

  int ranks[64] = {0};
  int count = 0;
  gboolean supported = FALSE;
  read_prefcore_rankings(ranks, 64, &count, &supported);

  if (!supported || count <= 0) {
    gtk_label_set_text(
        GTK_LABEL(ctx->prefcore_lbl),
        "amd_pstate_prefcore_ranking: Not exposed on active kernel");
    return G_SOURCE_CONTINUE;
  }

  GString *s = g_string_new("<span font_family=\"monospace\">");
  for (int i = 0; i < count; i++) {
    if (ranks[i] >= 0)
      g_string_append_printf(s, "<b>C%02d:</b> %3d%s", i, ranks[i],
                             ((i + 1) % 4 == 0 || i == count - 1) ? "\n"
                                                                  : "   ");
    else
      g_string_append_printf(s, "<b>C%02d:</b> N/A%s", i,
                             ((i + 1) % 4 == 0 || i == count - 1) ? "\n"
                                                                  : "   ");
  }
  g_string_append(s, "</span>");
  gtk_label_set_markup(GTK_LABEL(ctx->prefcore_lbl), s->str);
  g_string_free(s, TRUE);
  return G_SOURCE_CONTINUE;
}

static void on_topo_popup_destroy(GtkWidget *w, gpointer data) {
  (void)w;
  TopoPopupCtx *ctx = (TopoPopupCtx *)data;
  if (ctx->timer_id != 0)
    g_source_remove(ctx->timer_id);
  g_free(ctx);
}

void popup_topology_show_map(GtkWindow *parent, X3dGuiIpc *ipc,
                             X3dStagedWidget *owner_staged,
                             char *staged_mask_buf, size_t mask_sz,
                             GtkWidget *summary_lbl) {
  GtkBox *body = NULL;
  GtkWindow *win = gui_popups_create_shell(parent, "Topology Pin / Bind Map",
                                           640, 520, FALSE, &body);

  TopoPopupCtx *ctx = g_new0(TopoPopupCtx, 1);
  ctx->ipc = ipc;
  ctx->owner = owner_staged;
  ctx->mask_buf = staged_mask_buf;
  ctx->mask_sz = mask_sz;
  ctx->summary_lbl = summary_lbl;

  GtkWidget *rank_card = x3d_current_widget_new(
      "AMD_PSTATE_PREFCORE_RANKING (LIVE CPPC RANK PER CORE)", FALSE);
  ctx->prefcore_lbl = gtk_label_new("Probing CPPC preferred-core rankings...");
  gtk_widget_add_css_class(ctx->prefcore_lbl, "x3d-value");
  gtk_label_set_xalign(GTK_LABEL(ctx->prefcore_lbl), 0.0f);
  gtk_box_append(x3d_current_widget_get_body(X3D_CURRENT_WIDGET(rank_card)),
                 ctx->prefcore_lbl);
  gtk_box_append(body, rank_card);

  GtkWidget *pin_card = x3d_current_widget_new(
      "CORE PIN MAP (ACTIVE = CACHE / PRIMARY SET)", FALSE);
  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
  gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
  ctx->core_grid = GTK_GRID(grid);

  char fallback_mask[128] = {0};
  int ccd1 = 0, total = 0;
  if (x3d_ipc_read_topology(fallback_mask, sizeof(fallback_mask), &ccd1,
                            &total) != 0 ||
      total <= 0) {
    total = 16;
  }
  ctx->total_cores = total;

  const char *active_mask =
      (staged_mask_buf && staged_mask_buf[0]) ? staged_mask_buf : fallback_mask;

  for (int i = 0; i < total; i++) {
    char lbl[16];
    snprintf(lbl, sizeof(lbl), "C%d", i);
    GtkWidget *tb = x3d_toggle_new(lbl);
    if (mask_contains_core(active_mask, i))
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tb), TRUE);
    g_signal_connect(tb, "toggled", G_CALLBACK(on_topo_core_toggled), ctx);
    gtk_grid_attach(GTK_GRID(grid), tb, i % 8, i / 8, 1, 1);
  }

  gtk_box_append(x3d_current_widget_get_body(X3D_CURRENT_WIDGET(pin_card)),
                 grid);
  gtk_box_append(body, pin_card);

  GtkWidget *close_btn = gtk_button_new_with_label("Close Window");
  gtk_widget_add_css_class(close_btn, "x3d-primary");
  gtk_widget_set_halign(close_btn, GTK_ALIGN_END);
  g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(gtk_window_close),
                           win);
  gtk_box_append(body, close_btn);

  topo_popup_poll_cb(ctx);
  ctx->timer_id = g_timeout_add_seconds(1, topo_popup_poll_cb, ctx);
  g_signal_connect(win, "destroy", G_CALLBACK(on_topo_popup_destroy), ctx);

  gtk_window_present(win);
}
