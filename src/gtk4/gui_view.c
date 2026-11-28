/* GTK4 Frontend View Shell & CSS Theme Engine Implementation
 * `gui_view.c`
 * Builds the root AdwApplicationWindow, AdwOverlaySplitView, sidebar list
 * primitives, and dynamic W3C/GTK4 CSS theme overrides.
 */

#include "gui_view.h"
#include <string.h>

static GtkCssProvider *g_base_theme_provider = NULL;
static GtkCssProvider *g_override_css_provider = NULL;

void x3d_gui_view_init_css(void) {
  GdkDisplay *disp = gdk_display_get_default();
  if (!disp)
    return;
  if (!g_base_theme_provider) {
    g_base_theme_provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_display(
        disp, GTK_STYLE_PROVIDER(g_base_theme_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  }
  if (!g_override_css_provider) {
    g_override_css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_display(
        disp, GTK_STYLE_PROVIDER(g_override_css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
  }
}

void x3d_gui_view_apply_theme_prefs(const X3dGuiThemePrefs *prefs) {
  if (!prefs || !g_base_theme_provider || !g_override_css_provider)
    return;

  AdwColorScheme scheme = ADW_COLOR_SCHEME_FORCE_DARK;
  const char *res_path = "/org/x3d-toggle/gui/theme.css";

  switch (prefs->theme_idx) {
  case 1:
    scheme = ADW_COLOR_SCHEME_FORCE_DARK;
    res_path = "/org/x3d-toggle/gui/theme-imperial.css";
    break;
  case 2:
    scheme = ADW_COLOR_SCHEME_FORCE_DARK;
    res_path = "/org/x3d-toggle/gui/theme-lact.css";
    break;
  case 3:
    scheme = ADW_COLOR_SCHEME_FORCE_LIGHT;
    res_path = "/org/x3d-toggle/gui/theme-adaptive.css";
    break;
  case 4:
    scheme = ADW_COLOR_SCHEME_FORCE_DARK;
    res_path = "/org/x3d-toggle/gui/theme-adaptive.css";
    break;
  case 5:
    scheme = ADW_COLOR_SCHEME_DEFAULT;
    res_path = "/org/x3d-toggle/gui/theme-adaptive.css";
    break;
  default:
    scheme = ADW_COLOR_SCHEME_FORCE_DARK;
    res_path = "/org/x3d-toggle/gui/theme.css";
    break;
  }

  adw_style_manager_set_color_scheme(adw_style_manager_get_default(), scheme);
  gtk_css_provider_load_from_resource(g_base_theme_provider, res_path);

  static const char *const accents[] = {NULL,      "#e8611a", "#C0A36A",
                                        "#3daee9", "#4dd0e1", "#4ade80"};
  static const char *const fonts[] = {NULL, "Inter", "Sans", "Monospace",
                                      "Cinzel"};

  GString *css = g_string_new("");
  int sz = CLAMP(prefs->font_size_pt, 9, 18);
  const char *font =
      (prefs->font_idx < G_N_ELEMENTS(fonts)) ? fonts[prefs->font_idx] : NULL;
  const char *acc = (prefs->color_idx < G_N_ELEMENTS(accents))
                        ? accents[prefs->color_idx]
                        : NULL;

  if (font || sz != 11) {
    g_string_append(css, "window.background { ");
    if (font)
      g_string_append_printf(css, "font-family: \"%s\"; ", font);
    g_string_append_printf(css, "font-size: %dpt; }\n", sz);
  }
  if (acc) {
    g_string_append_printf(
        css,
        ".x3d-accent, .x3d-subsection-title, "
        ".navigation-sidebar row:selected, "
        ".navigation-sidebar row:selected label { color: %s; }\n"
        "button.x3d-toggle-pill:checked, button.x3d-toggle-pill.active, "
        "button.x3d-primary { background-color: %s; color: #0a0a0a; border-color: %s; }\n"
        "button.x3d-sidebar-cta-apply:not(:disabled) { background-color: %s; color: #0a0a0a; }\n"
        "button.x3d-sidebar-cta-apply:disabled { background-color: color-mix(in srgb, %s 24%%, transparent); }\n"
        "scale trough highlight { background-image: none; background-color: %s; }\n"
        "scale slider, entry:focus, spinbutton:focus { border-color: %s; }\n",
        acc, acc, acc, acc, acc, acc, acc);
  }
  gtk_css_provider_load_from_string(g_override_css_provider, css->str);
  g_string_free(css, TRUE);
}

/* ── Sidebar Construction & Lookup ────────────────────────────── */

GtkWidget *x3d_gui_view_add_sidebar_row(X3dGuiView *v, const char *id,
                                        const char *title, gboolean is_subnav,
                                        gboolean initially_hidden) {
  if (!v || !v->sidebar_list)
    return NULL;
  GtkWidget *row = gtk_list_box_row_new();
  gtk_widget_set_name(row, id);
  if (is_subnav)
    gtk_widget_add_css_class(row, "x3d-subnav-row");
  if (initially_hidden)
    gtk_widget_set_visible(row, FALSE);

  GtkWidget *lbl = gtk_label_new(title);
  gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
  gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), lbl);
  gtk_list_box_append(v->sidebar_list, row);
  return row;
}

void x3d_gui_view_add_sidebar_separator(X3dGuiView *v) {
  if (!v || !v->sidebar_list)
    return;
  GtkWidget *row = gtk_list_box_row_new();
  gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
  gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
  gtk_widget_set_can_focus(row, FALSE);
  gtk_widget_add_css_class(row, "x3d-sidebar-sep-row");
  GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_add_css_class(sep, "x3d-sidebar-divider");
  gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), sep);
  gtk_list_box_append(v->sidebar_list, row);
}

GtkListBoxRow *x3d_gui_view_find_sidebar_row(X3dGuiView *v,
                                             const char *page_id) {
  if (!v || !v->sidebar_list || !page_id)
    return NULL;
  for (int i = 0;; i++) {
    GtkListBoxRow *r = gtk_list_box_get_row_at_index(v->sidebar_list, i);
    if (!r)
      break;
    const char *nm = gtk_widget_get_name(GTK_WIDGET(r));
    if (nm && strcmp(nm, page_id) == 0)
      return r;
  }
  return NULL;
}

void x3d_gui_view_set_sidebar_row_visible(X3dGuiView *v, const char *page_id,
                                          gboolean visible) {
  GtkListBoxRow *r = x3d_gui_view_find_sidebar_row(v, page_id);
  if (r)
    gtk_widget_set_visible(GTK_WIDGET(r), visible);
}

/* ── Root Window Shell Builder ────────────────────────────────── */

X3dGuiView *x3d_gui_view_build(GtkApplication *app) {
  X3dGuiView *v = g_new0(X3dGuiView, 1);

  GtkBuilder *builder =
      gtk_builder_new_from_resource("/org/x3d-toggle/gui/x3d-toggle.ui");
  v->main_window = GTK_WINDOW(gtk_builder_get_object(builder, "main_window"));
  v->toast_overlay =
      ADW_TOAST_OVERLAY(gtk_builder_get_object(builder, "toast_overlay"));
  v->split_view =
      ADW_OVERLAY_SPLIT_VIEW(gtk_builder_get_object(builder, "split_view"));
  v->btn_sidebar_toggle = GTK_TOGGLE_BUTTON(
      gtk_builder_get_object(builder, "btn_sidebar_toggle"));
  v->sidebar_list =
      GTK_LIST_BOX(gtk_builder_get_object(builder, "sidebar_list"));
  v->btn_global_apply =
      GTK_WIDGET(gtk_builder_get_object(builder, "btn_global_apply"));
  v->btn_global_cancel =
      GTK_WIDGET(gtk_builder_get_object(builder, "btn_global_cancel"));
  v->btn_global_default =
      GTK_WIDGET(gtk_builder_get_object(builder, "btn_global_default"));
  v->content_stack =
      GTK_STACK(gtk_builder_get_object(builder, "content_stack"));
  gtk_window_set_application(v->main_window, app);

  g_object_bind_property(v->split_view, "show-sidebar", v->btn_sidebar_toggle,
                         "active",
                         G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  AdwBreakpoint *bp = adw_breakpoint_new(adw_breakpoint_condition_new_length(
      ADW_BREAKPOINT_CONDITION_MAX_WIDTH, 720.0, ADW_LENGTH_UNIT_PX));
  adw_breakpoint_add_setters(bp, G_OBJECT(v->split_view), "collapsed", TRUE,
                             G_OBJECT(v->content_stack), "margin-start", 12,
                             G_OBJECT(v->content_stack), "margin-end", 12,
                             NULL);
  adw_application_window_add_breakpoint(
      ADW_APPLICATION_WINDOW(v->main_window), bp);

  g_object_unref(builder);
  return v;
}

void x3d_gui_view_free(X3dGuiView *view) { g_free(view); }
