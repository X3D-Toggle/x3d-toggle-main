/* Extras Page Slice Implementation
 * `pages/gui_extras.c`
 */

#include "gui_extras.h"
#include "../gui_popups.h"
#include "../gui_stubs.h"
#include "gui_widgets.h"

static struct {
  X3dPageCtx ctx;
  GtkWidget *root;

  X3dStagedWidget *stg_theme;
  GtkDropDown *dd_theme_main;
  GtkDropDown *dd_theme_color;
  GtkDropDown *dd_theme_font;
  GtkSpinButton *sp_theme_size;
  GtkSpinButton *sp_update_interval_ms;
} g_ext;

static void mod_extras_refresh(const X3dPageCtx *ctx) {
  if (ctx)
    g_ext.ctx = *ctx;

  X3dGuiThemePrefs prefs;
  x3d_ipc_load_theme_prefs(&prefs);
  x3d_staged_widget_set_syncing(g_ext.stg_theme, TRUE);
  gtk_drop_down_set_selected(g_ext.dd_theme_main, prefs.theme_idx);
  gtk_drop_down_set_selected(g_ext.dd_theme_color, prefs.color_idx);
  gtk_drop_down_set_selected(g_ext.dd_theme_font, prefs.font_idx);
  gtk_spin_button_set_value(g_ext.sp_theme_size, prefs.font_size_pt);
  gtk_spin_button_set_value(g_ext.sp_update_interval_ms,
                            (double)prefs.update_interval_ms);
  x3d_staged_widget_set_syncing(g_ext.stg_theme, FALSE);

  gui_widgets_refresh_desktop_section();
}

static void on_theme_default(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  gtk_drop_down_set_selected(g_ext.dd_theme_main, 0);
  gtk_drop_down_set_selected(g_ext.dd_theme_color, 0);
  gtk_drop_down_set_selected(g_ext.dd_theme_font, 0);
  gtk_spin_button_set_value(g_ext.sp_theme_size, 11);
  gtk_spin_button_set_value(g_ext.sp_update_interval_ms, 500);
}

static void on_theme_cancel(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  mod_extras_refresh(NULL);
}

static gboolean on_theme_apply(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  X3dGuiThemePrefs prefs = {
      .theme_idx = gtk_drop_down_get_selected(g_ext.dd_theme_main),
      .color_idx = gtk_drop_down_get_selected(g_ext.dd_theme_color),
      .font_idx = gtk_drop_down_get_selected(g_ext.dd_theme_font),
      .font_size_pt = (int)gtk_spin_button_get_value(g_ext.sp_theme_size),
      .update_interval_ms =
          (guint)gtk_spin_button_get_value(g_ext.sp_update_interval_ms),
  };
  x3d_gui_view_apply_theme_prefs(&prefs);
  x3d_gui_ipc_set_update_interval_ms(g_ext.ctx.ipc, prefs.update_interval_ms);
  if (!x3d_ipc_save_theme_prefs(&prefs)) {
    x3d_stubs_toast("Failed to write ~/.config/x3d-toggle/gui.conf");
    return FALSE;
  }
  return TRUE;
}

static void on_doc_viewer_clicked(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  gui_popups_show_doc_viewer(g_ext.ctx.view->main_window);
}

static void on_emergency_confirmed(X3dDangerWidget *w, gpointer d) {
  (void)w;
  (void)d;
  x3d_stub_emergency_override_execute(g_ext.ctx.ipc);
  if (g_ext.ctx.poll_dashboard_now)
    g_ext.ctx.poll_dashboard_now(g_ext.ctx.frame);
}

static GtkWidget *mod_extras_build(const X3dPageCtx *ctx) {
  g_ext.ctx = *ctx;

  GtkBox *page =
      x3d_page_vbox_new(ctx->view->content_stack, "extras", "Extras");
  g_ext.root = GTK_WIDGET(page);

  GtkWidget *stg_thm = x3d_staged_widget_new("INTERFACE THEME", FALSE);
  g_ext.stg_theme = X3D_STAGED_WIDGET(stg_thm);
  GtkBox *tbody = x3d_staged_widget_get_body(g_ext.stg_theme);

  const char *themes[] = {"Default (X3D Obsidian)",
                          "Imperial (Shiar Dark & Gold)",
                          "LACT (Breeze Slate & Blue)",
                          "Light",
                          "Dark",
                          "System (Match GTK / Portal)",
                          NULL};
  const char *colors[] = {"Theme Default",    "Ryzen Ember",
                          "Imperial Gold",    "LACT Plasma Blue",
                          "Cyber Cyan",       "Terminal Emerald",
                          NULL};
  const char *fonts[] = {"Theme Default", "Inter", "System Sans", "Monospace",
                         "Cinzel", NULL};
  GtkWidget *sp_sz = gtk_spin_button_new_with_range(9, 18, 1);
  GtkWidget *sp_int = gtk_spin_button_new_with_range(250, 5000, 250);

  g_ext.dd_theme_main = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(themes));
  g_ext.dd_theme_color = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(colors));
  g_ext.dd_theme_font = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(fonts));
  g_ext.sp_theme_size = GTK_SPIN_BUTTON(sp_sz);
  g_ext.sp_update_interval_ms = GTK_SPIN_BUTTON(sp_int);

  gtk_box_append(tbody,
                 x3d_form_row_new("Main Theme", "Visual stylesheet preset",
                                  GTK_WIDGET(g_ext.dd_theme_main)));
  gtk_box_append(tbody,
                 x3d_form_row_new("Color", "Primary accent color override",
                                  GTK_WIDGET(g_ext.dd_theme_color)));
  gtk_box_append(tbody,
                 x3d_form_row_new("Font", "Interface typography family",
                                  GTK_WIDGET(g_ext.dd_theme_font)));
  gtk_box_append(tbody,
                 x3d_form_row_new("Size (ptx)", "Base interface font size",
                                  sp_sz));
  gtk_box_append(
      tbody, x3d_form_row_new("Widget Update Interval (ms)",
                              "Live telemetry refresh cadence for dashboards "
                              "and popouts (250–5000 ms)",
                              sp_int));

  x3d_staged_widget_watch_control(g_ext.stg_theme,
                                  GTK_WIDGET(g_ext.dd_theme_main));
  x3d_staged_widget_watch_control(g_ext.stg_theme,
                                  GTK_WIDGET(g_ext.dd_theme_color));
  x3d_staged_widget_watch_control(g_ext.stg_theme,
                                  GTK_WIDGET(g_ext.dd_theme_font));
  x3d_staged_widget_watch_control(g_ext.stg_theme, sp_sz);
  x3d_staged_widget_watch_control(g_ext.stg_theme, sp_int);
  gtk_box_append(page, stg_thm);

  g_signal_connect(g_ext.stg_theme, "default", G_CALLBACK(on_theme_default),
                   NULL);
  g_signal_connect(g_ext.stg_theme, "cancel", G_CALLBACK(on_theme_cancel),
                   NULL);
  g_signal_connect(g_ext.stg_theme, "apply", G_CALLBACK(on_theme_apply), NULL);

  /* Mount Desktop Widgets & SNI section from pages/gui_widgets.c */
  gtk_box_append(page, gui_widgets_build_desktop_section(ctx));

  GtkWidget *imm_doc = x3d_immediate_widget_new("DOCUMENTATION & LORE");
  GtkWidget *btn_doc = x3d_popup_btn_new("Document Viewer");
  GtkWidget *ditems[] = {btn_doc};
  x3d_immediate_widget_set_grid_items(X3D_IMMEDIATE_WIDGET(imm_doc), ditems, 1);
  gtk_box_append(page, imm_doc);
  g_signal_connect(btn_doc, "clicked", G_CALLBACK(on_doc_viewer_clicked), NULL);

  GtkWidget *about = x3d_current_widget_new("ABOUT", FALSE);
  GtkWidget *about_lbl = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(about_lbl),
                       "<span font_family=\"monospace\">"
                       "  <b>Version:</b>       2.1.0\n"
                       "  <b>Author:</b>        Pyrotiger\n"
                       "  <b>Release Year:</b>  2026  •  License: GPLv3"
                       "</span>");
  gtk_widget_add_css_class(about_lbl, "x3d-value");
  gtk_label_set_xalign(GTK_LABEL(about_lbl), 0.0f);
  gtk_box_append(x3d_current_widget_get_body(X3D_CURRENT_WIDGET(about)),
                 about_lbl);
  gtk_box_append(page, about);

  GtkWidget *danger = x3d_danger_widget_new(
      "EMERGENCY OVERRIDE",
      "Resets all hardware and daemon states to stock defaults, returns CCD "
      "and CPPC control to native kernel drivers, and erases saved profiles.",
      "EMERGENCY OVERRIDE", "Execute Emergency Override?",
      "This will reset all states to hardware defaults, return control to "
      "native kernel drivers, and erase saved profiles. Proceed?");
  gtk_box_append(page, danger);
  g_signal_connect(danger, "confirmed", G_CALLBACK(on_emergency_confirmed),
                   NULL);

  x3d_widgets_tree_connect_dirty_notify(g_ext.root, ctx->notify_dirty_cb,
                                        ctx->notify_dirty_data);
  return g_ext.root;
}

static gboolean mod_extras_has_dirty(void) {
  return x3d_widgets_tree_has_dirty(g_ext.root);
}
static gboolean mod_extras_has_staged(void) {
  return x3d_widgets_tree_has_staged(g_ext.root);
}
static gboolean mod_extras_apply_all(void) {
  return x3d_widgets_tree_apply_dirty(g_ext.root);
}
static void mod_extras_cancel_all(void) {
  x3d_widgets_tree_cancel_dirty(g_ext.root);
}
static void mod_extras_default_all(void) {
  x3d_widgets_tree_default_all(g_ext.root);
}

const X3dPageModule *gui_extras_get_module(void) {
  static const X3dPageModule k_mod = {
      .id = "extras",
      .title = "Extras",
      .is_subnav = FALSE,
      .initially_hidden = FALSE,
      .divider_after = FALSE,
      .build = mod_extras_build,
      .refresh = mod_extras_refresh,
      .activate = NULL,
      .deactivate = NULL,
      .has_dirty = mod_extras_has_dirty,
      .has_staged = mod_extras_has_staged,
      .apply_all = mod_extras_apply_all,
      .cancel_all = mod_extras_cancel_all,
      .default_all = mod_extras_default_all,
      .on_interval_changed = NULL,
      .highlight_hw_mode = NULL,
  };
  return &k_mod;
}
