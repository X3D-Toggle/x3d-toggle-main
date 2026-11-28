/* Developer Options Page Slice Implementation
 * `pages/gui_dev.c`
 */

#include "gui_dev.h"
#include "../gui_popups.h"
#include "../gui_stubs.h"
#include "gui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const k_clang_keys[7] = {
    "LINT_CLANG_DIAGNOSTIC",  "LINT_CLANG_BUGPRONE",  "LINT_CLANG_MODERNIZE",
    "LINT_CLANG_READABILITY", "LINT_CLANG_PERFORMANCE",
    "LINT_CLANG_PORTABILITY", "LINT_CLANG_ANALYZER"};
static const char *const k_cpp_keys[7] = {
    "LINT_CPPCHECK_ALL",         "LINT_CPPCHECK_WARNING",
    "LINT_CPPCHECK_STYLE",       "LINT_CPPCHECK_PERFORMANCE",
    "LINT_CPPCHECK_PORTABILITY", "LINT_CPPCHECK_INFORMATION",
    "LINT_CPPCHECK_UNUSED"};
static const char *const k_vmodes[3] = {"full", "summary", "disabled"};
static const char *const k_vkinds[5] = {"all", "definite", "indirect",
                                        "possible", "reachable"};

static struct {
  X3dPageCtx ctx;
  GtkWidget *root;

  GtkToggleButton *tb_clang[7];
  GtkToggleButton *tb_cppcheck[7];
  GtkDropDown *dd_valgrind_mode;
  GtkDropDown *dd_valgrind_kinds;
  GtkToggleButton *tb_valgrind_origins;
  GtkToggleButton *tb_debug_logging;

  X3dStagedWidget *stg_journal_rot;
  GtkSpinButton *sp_journal_keep;
  GtkEditable *ent_journal_max_mb;

  GtkToggleButton *tb_audit_latency_timer;

  X3dStagedWidget *stg_ui_overrides;
  GtkToggleButton *tb_force_unsupported;

  gboolean suppress_live_toggles;
} g_dev;

static guint find_str_idx(const char *const *arr, guint n, const char *target) {
  if (!target)
    return 0;
  for (guint i = 0; i < n; i++) {
    if (g_ascii_strcasecmp(arr[i], target) == 0)
      return i;
  }
  return 0;
}

static void mod_dev_refresh(const X3dPageCtx *ctx) {
  if (ctx)
    g_dev.ctx = *ctx;

  g_dev.suppress_live_toggles = TRUE;
  for (int i = 0; i < 7; i++) {
    gtk_toggle_button_set_active(
        g_dev.tb_clang[i],
        strcmp(x3d_ipc_config_get(k_clang_keys[i]), "enabled") == 0);
    gtk_toggle_button_set_active(
        g_dev.tb_cppcheck[i],
        strcmp(x3d_ipc_config_get(k_cpp_keys[i]), "enabled") == 0);
  }
  gtk_drop_down_set_selected(
      g_dev.dd_valgrind_mode,
      find_str_idx(k_vmodes, G_N_ELEMENTS(k_vmodes),
                   x3d_ipc_config_get("LINT_VALGRIND_MODE")));
  gtk_drop_down_set_selected(
      g_dev.dd_valgrind_kinds,
      find_str_idx(k_vkinds, G_N_ELEMENTS(k_vkinds),
                   x3d_ipc_config_get("LINT_VALGRIND_KINDS")));
  gtk_toggle_button_set_active(
      g_dev.tb_valgrind_origins,
      strcmp(x3d_ipc_config_get("LINT_VALGRIND_ORIGINS"), "enabled") == 0);
  gtk_toggle_button_set_active(g_dev.tb_debug_logging,
                               atoi(x3d_ipc_config_get("DEBUG_ENABLE")) != 0);
  gtk_toggle_button_set_active(g_dev.tb_audit_latency_timer,
                               x3d_stubs_get()->audit_latency_timer);
  g_dev.suppress_live_toggles = FALSE;

  x3d_staged_widget_set_syncing(g_dev.stg_journal_rot, TRUE);
  const char *jk = x3d_ipc_config_get("JOURNAL_KEEP");
  gtk_spin_button_set_value(g_dev.sp_journal_keep, jk[0] ? atoi(jk) : 7);
  const char *jm = x3d_ipc_config_get("JOURNAL_MAX_MB");
  gtk_editable_set_text(g_dev.ent_journal_max_mb, jm[0] ? jm : "10");
  x3d_staged_widget_set_syncing(g_dev.stg_journal_rot, FALSE);

  x3d_staged_widget_set_syncing(g_dev.stg_ui_overrides, TRUE);
  gtk_toggle_button_set_active(
      g_dev.tb_force_unsupported,
      x3d_gui_ipc_get_force_unsupported(g_dev.ctx.ipc));
  x3d_staged_widget_set_syncing(g_dev.stg_ui_overrides, FALSE);
}

static void on_lint_toggle(GtkToggleButton *tb, gpointer d) {
  (void)d;
  if (g_dev.suppress_live_toggles)
    return;
  const char *key = g_object_get_data(G_OBJECT(tb), "x3d-cfg-key");
  if (key)
    x3d_ipc_config_set(key,
                       gtk_toggle_button_get_active(tb) ? "enabled"
                                                        : "disabled");
}

static void on_val_mode_dd(GObject *o, GParamSpec *p, gpointer d) {
  (void)p;
  (void)d;
  if (g_dev.suppress_live_toggles)
    return;
  guint idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(o));
  if (idx < G_N_ELEMENTS(k_vmodes))
    x3d_ipc_config_set("LINT_VALGRIND_MODE", k_vmodes[idx]);
}

static void on_val_kinds_dd(GObject *o, GParamSpec *p, gpointer d) {
  (void)p;
  (void)d;
  if (g_dev.suppress_live_toggles)
    return;
  guint idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(o));
  if (idx < G_N_ELEMENTS(k_vkinds))
    x3d_ipc_config_set("LINT_VALGRIND_KINDS", k_vkinds[idx]);
}

static void on_dev_observer_clicked(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  x3d_ipc_spawn_terminal(
      "pkexec /usr/lib/x3d-toggle/scripts/tools/debug.sh --gui-term");
}

static void on_dev_coredump_clicked(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  x3d_ipc_spawn_terminal("/usr/lib/x3d-toggle/scripts/tools/coredump.sh");
}

static void on_clang_run(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  x3d_ipc_spawn_terminal("/usr/lib/x3d-toggle/scripts/tools/linter.sh clang");
}

static void on_cpp_run(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  x3d_ipc_spawn_terminal("/usr/lib/x3d-toggle/scripts/tools/linter.sh cppc");
}

static void on_val_run(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  x3d_ipc_spawn_terminal(
      "/usr/lib/x3d-toggle/scripts/tools/linter.sh valgrind");
}

static void on_dbg_log_toggled(GtkToggleButton *tb, gpointer d) {
  (void)d;
  if (!g_dev.suppress_live_toggles)
    x3d_ipc_config_set("DEBUG_ENABLE",
                       gtk_toggle_button_get_active(tb) ? "1" : "0");
}

static void on_journal_arch_clicked(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  gui_popups_show_journal_maint(g_dev.ctx.view->main_window, FALSE);
}

static void on_journal_rot_clicked(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  gui_popups_show_journal_maint(g_dev.ctx.view->main_window, TRUE);
}

static void on_jrot_default(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  gtk_spin_button_set_value(g_dev.sp_journal_keep, 7);
  gtk_editable_set_text(g_dev.ent_journal_max_mb, "10");
}

static void on_jrot_cancel(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  mod_dev_refresh(NULL);
}

static gboolean on_jrot_apply(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  char buf[32];
  gboolean ok = TRUE;
  snprintf(buf, sizeof(buf), "%d",
           (int)gtk_spin_button_get_value(g_dev.sp_journal_keep));
  if (!x3d_ipc_config_set("JOURNAL_KEEP", buf))
    ok = FALSE;
  const char *mb = gtk_editable_get_text(g_dev.ent_journal_max_mb);
  if (mb && mb[0] && !x3d_ipc_config_set("JOURNAL_MAX_MB", mb))
    ok = FALSE;
  if (!ok)
    x3d_stubs_toast("Failed to save Journal Rotation Settings (IPC error)");
  return ok;
}

static void on_audit_lat_toggled(GtkToggleButton *tb, gpointer d) {
  (void)d;
  if (!g_dev.suppress_live_toggles)
    x3d_stub_toggle_latency_timer(gtk_toggle_button_get_active(tb));
}

static void on_audit_dry_clicked(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  gui_popups_show_dry_run(g_dev.ctx.view->main_window, g_dev.ctx.ipc);
}

static void on_ovr_default(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  gtk_toggle_button_set_active(g_dev.tb_force_unsupported, FALSE);
}

static void on_ovr_cancel(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  gtk_toggle_button_set_active(
      g_dev.tb_force_unsupported,
      x3d_gui_ipc_get_force_unsupported(g_dev.ctx.ipc));
}

static gboolean on_ovr_apply(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  gboolean f = gtk_toggle_button_get_active(g_dev.tb_force_unsupported);
  if (!x3d_ipc_config_set("FORCE_UNSUPPORTED_DISPLAY", f ? "1" : "0")) {
    x3d_stubs_toast("Failed to persist UI Override to daemon (IPC error)");
    return FALSE;
  }
  x3d_gui_ipc_set_force_unsupported(g_dev.ctx.ipc, f);
  if (g_dev.ctx.sync_sidebar_gating)
    g_dev.ctx.sync_sidebar_gating(g_dev.ctx.frame);
  return TRUE;
}

static GtkWidget *mod_dev_build(const X3dPageCtx *ctx) {
  g_dev.ctx = *ctx;

  GtkBox *page = x3d_page_vbox_new(ctx->view->content_stack, "developer",
                                   "Developer Options");
  g_dev.root = GTK_WIDGET(page);

  GtkWidget *top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  GtkWidget *btn_obs = x3d_popup_btn_new("Real-Time Observer");
  GtkWidget *btn_cd =
      gtk_button_new_with_label("Latest Crash Dumps for CoreDump Analysis");
  gtk_widget_add_css_class(btn_cd, "x3d-cta-btn");
  GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand(spacer, TRUE);
  gtk_box_append(GTK_BOX(top_row), btn_obs);
  gtk_box_append(GTK_BOX(top_row), spacer);
  gtk_box_append(GTK_BOX(top_row), btn_cd);
  gtk_box_append(page, top_row);

  g_signal_connect(btn_obs, "clicked", G_CALLBACK(on_dev_observer_clicked),
                   NULL);
  g_signal_connect(btn_cd, "clicked", G_CALLBACK(on_dev_coredump_clicked),
                   NULL);

  GtkWidget *imm_clang =
      x3d_immediate_widget_new("CLANG-TIDY STATIC ANALYSIS");
  static const char *const clang_lbls[7] = {
      "Diagnostics", "Bugprone",    "Modernize", "Readability",
      "Performance", "Portability", "Analyzer"};
  GtkWidget *clang_items[8];
  for (int i = 0; i < 7; i++) {
    clang_items[i] = x3d_toggle_new(clang_lbls[i]);
    g_dev.tb_clang[i] = GTK_TOGGLE_BUTTON(clang_items[i]);
    g_object_set_data(G_OBJECT(clang_items[i]), "x3d-cfg-key",
                      (gpointer)k_clang_keys[i]);
    g_signal_connect(clang_items[i], "toggled", G_CALLBACK(on_lint_toggle),
                     NULL);
  }
  GtkWidget *btn_clang = x3d_popup_btn_new("Analyze and Present Report");
  clang_items[7] = btn_clang;
  x3d_immediate_widget_set_grid_items(X3D_IMMEDIATE_WIDGET(imm_clang),
                                      clang_items, 8);
  gtk_box_append(page, imm_clang);
  g_signal_connect(btn_clang, "clicked", G_CALLBACK(on_clang_run), NULL);

  GtkWidget *imm_cpp = x3d_immediate_widget_new("CPPCHECK BUG HUNTING");
  static const char *const cpp_lbls[7] = {"All Cppcheck Checks",
                                          "Warning Messages",
                                          "Coding Style Checks",
                                          "Performance Suggestions",
                                          "Portability Warnings",
                                          "Informational Messages",
                                          "Unused Functions and Variables"};
  GtkWidget *cpp_items[8];
  for (int i = 0; i < 7; i++) {
    cpp_items[i] = x3d_toggle_new(cpp_lbls[i]);
    g_dev.tb_cppcheck[i] = GTK_TOGGLE_BUTTON(cpp_items[i]);
    g_object_set_data(G_OBJECT(cpp_items[i]), "x3d-cfg-key",
                      (gpointer)k_cpp_keys[i]);
    g_signal_connect(cpp_items[i], "toggled", G_CALLBACK(on_lint_toggle), NULL);
  }
  GtkWidget *btn_cpp = x3d_popup_btn_new("Analyze and Present Report");
  cpp_items[7] = btn_cpp;
  x3d_immediate_widget_set_grid_items(X3D_IMMEDIATE_WIDGET(imm_cpp), cpp_items,
                                      8);
  gtk_box_append(page, imm_cpp);
  g_signal_connect(btn_cpp, "clicked", G_CALLBACK(on_cpp_run), NULL);

  GtkWidget *imm_val = x3d_immediate_widget_new("VALGRIND RUNTIME ANALYSIS");
  GtkBox *vbody = x3d_immediate_widget_get_body(X3D_IMMEDIATE_WIDGET(imm_val));
  const char *vmodes[] = {"full", "summary", "disabled", NULL};
  const char *vkinds[] = {"all", "definite", "indirect", "possible",
                          "reachable", NULL};
  g_dev.dd_valgrind_mode = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(vmodes));
  g_dev.dd_valgrind_kinds =
      GTK_DROP_DOWN(gtk_drop_down_new_from_strings(vkinds));
  gtk_box_append(vbody,
                 x3d_form_row_new("Analysis Mode", "full / summary / disabled",
                                  GTK_WIDGET(g_dev.dd_valgrind_mode)));
  gtk_box_append(vbody,
                 x3d_form_row_new("Leak Kinds", "Target leak classifications",
                                  GTK_WIDGET(g_dev.dd_valgrind_kinds)));

  GtkWidget *tb_vorig = x3d_toggle_new("High CPU Overhead / Origin Tracking");
  g_dev.tb_valgrind_origins = GTK_TOGGLE_BUTTON(tb_vorig);
  g_object_set_data(G_OBJECT(tb_vorig), "x3d-cfg-key",
                    (gpointer) "LINT_VALGRIND_ORIGINS");
  GtkWidget *btn_val = x3d_popup_btn_new("Analyze and Present Report");
  GtkWidget *vitems[] = {tb_vorig, btn_val};
  x3d_immediate_widget_set_grid_items(X3D_IMMEDIATE_WIDGET(imm_val), vitems, 2);
  gtk_box_append(page, imm_val);

  g_signal_connect(g_dev.dd_valgrind_mode, "notify::selected",
                   G_CALLBACK(on_val_mode_dd), NULL);
  g_signal_connect(g_dev.dd_valgrind_kinds, "notify::selected",
                   G_CALLBACK(on_val_kinds_dd), NULL);
  g_signal_connect(tb_vorig, "toggled", G_CALLBACK(on_lint_toggle), NULL);
  g_signal_connect(btn_val, "clicked", G_CALLBACK(on_val_run), NULL);

  GtkWidget *tb_dbg = NULL;
  gtk_box_append(page,
                 x3d_live_toggle_row_new("Debug Logging",
                                         "Enable trace-level diagnostic output",
                                         &tb_dbg));
  g_dev.tb_debug_logging = GTK_TOGGLE_BUTTON(tb_dbg);
  g_signal_connect(g_dev.tb_debug_logging, "toggled",
                   G_CALLBACK(on_dbg_log_toggled), NULL);

  GtkWidget *imm_jm = x3d_immediate_widget_new("JOURNAL MAINTENANCE");
  GtkWidget *btn_ja = x3d_popup_btn_new("Archive Session");
  GtkWidget *btn_jr = x3d_popup_btn_new("Manual Rotation");
  GtkWidget *jmitems[] = {btn_ja, btn_jr};
  x3d_immediate_widget_set_grid_items(X3D_IMMEDIATE_WIDGET(imm_jm), jmitems, 2);
  gtk_box_append(page, imm_jm);
  g_signal_connect(btn_ja, "clicked", G_CALLBACK(on_journal_arch_clicked),
                   NULL);
  g_signal_connect(btn_jr, "clicked", G_CALLBACK(on_journal_rot_clicked), NULL);

  GtkWidget *stg_jr = x3d_staged_widget_new("JOURNAL ROTATION SETTINGS", FALSE);
  g_dev.stg_journal_rot = X3D_STAGED_WIDGET(stg_jr);
  GtkBox *jrbody = x3d_staged_widget_get_body(g_dev.stg_journal_rot);
  GtkWidget *sp_keep = gtk_spin_button_new_with_range(1, 30, 1);
  GtkWidget *ent_mb = gtk_entry_new();
  gtk_editable_set_width_chars(GTK_EDITABLE(ent_mb), 8);
  g_dev.sp_journal_keep = GTK_SPIN_BUTTON(sp_keep);
  g_dev.ent_journal_max_mb = GTK_EDITABLE(ent_mb);
  gtk_box_append(jrbody,
                 x3d_form_row_new("Retention Days",
                                  "Daily log archives retained (1–30)",
                                  sp_keep));
  gtk_box_append(jrbody,
                 x3d_form_row_new("Max Size (MB)",
                                  "Maximum log size before rotation", ent_mb));
  x3d_staged_widget_watch_control(g_dev.stg_journal_rot, sp_keep);
  x3d_staged_widget_watch_control(g_dev.stg_journal_rot, ent_mb);
  gtk_box_append(page, stg_jr);

  g_signal_connect(g_dev.stg_journal_rot, "default",
                   G_CALLBACK(on_jrot_default), NULL);
  g_signal_connect(g_dev.stg_journal_rot, "cancel", G_CALLBACK(on_jrot_cancel),
                   NULL);
  g_signal_connect(g_dev.stg_journal_rot, "apply", G_CALLBACK(on_jrot_apply),
                   NULL);

  GtkWidget *imm_aud = x3d_immediate_widget_new("AUDITS");
  GtkWidget *tb_lat = NULL;
  gtk_box_append(
      x3d_immediate_widget_get_body(X3D_IMMEDIATE_WIDGET(imm_aud)),
      x3d_live_toggle_row_new("Timer Measuring Switching Latency",
                              "Measure kernel/sysfs CCD transition latency (stub)",
                              &tb_lat));
  g_dev.tb_audit_latency_timer = GTK_TOGGLE_BUTTON(tb_lat);
  GtkWidget *btn_dry = x3d_popup_btn_new("Hardware 'Dry Run' Mode");
  GtkWidget *aitems[] = {btn_dry};
  x3d_immediate_widget_set_grid_items(X3D_IMMEDIATE_WIDGET(imm_aud), aitems, 1);
  gtk_box_append(page, imm_aud);

  g_signal_connect(g_dev.tb_audit_latency_timer, "toggled",
                   G_CALLBACK(on_audit_lat_toggled), NULL);
  g_signal_connect(btn_dry, "clicked", G_CALLBACK(on_audit_dry_clicked), NULL);

  GtkWidget *stg_ovr = x3d_staged_widget_new("UI OVERRIDES", FALSE);
  g_dev.stg_ui_overrides = X3D_STAGED_WIDGET(stg_ovr);
  GtkWidget *tb_force = x3d_toggle_new(
      "Force Show Unsupported Options (Disables Discovery Logic)");
  g_dev.tb_force_unsupported = GTK_TOGGLE_BUTTON(tb_force);
  GtkWidget *oitems[] = {tb_force};
  x3d_staged_widget_set_grid_items(g_dev.stg_ui_overrides, oitems, 1);
  x3d_staged_widget_watch_control(g_dev.stg_ui_overrides, tb_force);
  gtk_box_append(page, stg_ovr);

  g_signal_connect(g_dev.stg_ui_overrides, "default",
                   G_CALLBACK(on_ovr_default), NULL);
  g_signal_connect(g_dev.stg_ui_overrides, "cancel", G_CALLBACK(on_ovr_cancel),
                   NULL);
  g_signal_connect(g_dev.stg_ui_overrides, "apply", G_CALLBACK(on_ovr_apply),
                   NULL);

  x3d_widgets_tree_connect_dirty_notify(g_dev.root, ctx->notify_dirty_cb,
                                        ctx->notify_dirty_data);
  return g_dev.root;
}

static gboolean mod_dev_has_dirty(void) {
  return x3d_widgets_tree_has_dirty(g_dev.root);
}
static gboolean mod_dev_has_staged(void) {
  return x3d_widgets_tree_has_staged(g_dev.root);
}
static gboolean mod_dev_apply_all(void) {
  return x3d_widgets_tree_apply_dirty(g_dev.root);
}
static void mod_dev_cancel_all(void) {
  x3d_widgets_tree_cancel_dirty(g_dev.root);
}
static void mod_dev_default_all(void) {
  x3d_widgets_tree_default_all(g_dev.root);
}

const X3dPageModule *gui_dev_get_module(void) {
  static const X3dPageModule k_mod = {
      .id = "developer",
      .title = "Developer Options",
      .is_subnav = TRUE,
      .initially_hidden = TRUE,
      .divider_after = TRUE,
      .build = mod_dev_build,
      .refresh = mod_dev_refresh,
      .activate = NULL,
      .deactivate = NULL,
      .has_dirty = mod_dev_has_dirty,
      .has_staged = mod_dev_has_staged,
      .apply_all = mod_dev_apply_all,
      .cancel_all = mod_dev_cancel_all,
      .default_all = mod_dev_default_all,
      .on_interval_changed = NULL,
      .highlight_hw_mode = NULL,
  };
  return &k_mod;
}
