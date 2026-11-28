/* Document Viewer, Dry-Run Validator & Journal Maintenance Popups Implementation
 * `popups/popup_docs.c`
 */

#include "popup_docs.h"

/* ── 1. Hardware 'Dry Run' Validator PopUp ────────────────────── */

typedef struct {
  X3dGuiIpc *ipc;
  GtkDropDown *dd_target;
  GtkTextBuffer *buf;
} DryRunCtx;

static void on_dry_run_exec(GtkButton *btn, gpointer data) {
  (void)btn;
  DryRunCtx *ctx = (DryRunCtx *)data;
  GtkStringObject *so =
      GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(ctx->dd_target));
  const char *target = so ? gtk_string_object_get_string(so) : "mode cache";
  gchar *out = x3d_stub_run_dry_run_simulation(ctx->ipc, target);
  gtk_text_buffer_set_text(ctx->buf, out, -1);
  g_free(out);
}

void popup_docs_show_dry_run(GtkWindow *parent, X3dGuiIpc *ipc) {
  GtkBox *body = NULL;
  GtkWindow *win = gui_popups_create_shell(
      parent, "Hardware 'Dry Run' Validator", 600, 440, TRUE, &body);

  DryRunCtx *ctx = g_new0(DryRunCtx, 1);
  ctx->ipc = ipc;
  g_signal_connect_swapped(win, "destroy", G_CALLBACK(g_free), ctx);

  const char *targets[] = {"mode cache",       "mode frequency",
                           "mode dual",        "profile hardcore-gaming",
                           "sysctl kernel",    "irq-bind",
                           NULL};
  GtkWidget *dd = gtk_drop_down_new_from_strings(targets);
  ctx->dd_target = GTK_DROP_DOWN(dd);

  GtkWidget *run_btn = gtk_button_new_with_label("Simulate Transition");
  gtk_widget_add_css_class(run_btn, "x3d-primary");
  g_signal_connect(run_btn, "clicked", G_CALLBACK(on_dry_run_exec), ctx);

  GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_hexpand(dd, TRUE);
  gtk_box_append(GTK_BOX(top), dd);
  gtk_box_append(GTK_BOX(top), run_btn);
  gtk_box_append(body, top);

  GtkWidget *frame = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(frame, TRUE);
  gtk_widget_set_size_request(frame, -1, 240);
  gtk_widget_add_css_class(frame, "x3d-extras-frame");

  GtkWidget *tv = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
  gtk_text_view_set_monospace(GTK_TEXT_VIEW(tv), TRUE);
  gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tv), 10);
  gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(tv), 10);
  gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tv), 12);
  gtk_text_view_set_right_margin(GTK_TEXT_VIEW(tv), 12);
  ctx->buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(frame), tv);
  gtk_box_append(body, frame);

  on_dry_run_exec(NULL, ctx);
  gtk_window_present(win);
}

/* ── 2. Journal Maintenance PopUps (Archive / Manual Rotation) ── */

void popup_docs_show_journal_maint(GtkWindow *parent, gboolean is_rotate) {
  GtkBox *body = NULL;
  GtkWindow *win = gui_popups_create_shell(
      parent, is_rotate ? "Manual Log Rotation" : "Archive Session Logs", 460,
      240, TRUE, &body);

  if (is_rotate) {
    char *args[] = {(char *)"/bin/sh",
                    (char *)"/usr/lib/x3d-toggle/scripts/tools/rotate.sh",
                    (char *)"--all", NULL};
    char *env[] = {(char *)"X3D_EXEC=1", NULL};
    x3d_ipc_spawn(args, env);
  } else {
    char *args[] = {(char *)"/bin/sh",
                    (char *)"/usr/lib/x3d-toggle/scripts/tools/archive.sh",
                    NULL};
    char *env[] = {(char *)"X3D_EXEC=1", NULL};
    x3d_ipc_spawn(args, env);
  }

  GtkWidget *card = x3d_current_widget_new(
      is_rotate ? "ROTATION DISPATCHED" : "ARCHIVE DISPATCHED", FALSE);
  GtkWidget *lbl = gtk_label_new(
      is_rotate
          ? "Log rotation and retention cleanup triggered across active journal "
            "and audit directories."
          : "Current session logs and audit reports have been queued for gzip "
            "archival.");
  gtk_label_set_wrap(GTK_LABEL(lbl), TRUE);
  gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
  gtk_box_append(x3d_current_widget_get_body(X3D_CURRENT_WIDGET(card)), lbl);
  gtk_box_append(body, card);

  GtkWidget *close_btn = gtk_button_new_with_label("Close");
  gtk_widget_add_css_class(close_btn, "x3d-primary");
  gtk_widget_set_halign(close_btn, GTK_ALIGN_END);
  g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(gtk_window_close),
                           win);
  gtk_box_append(body, close_btn);

  gtk_window_present(win);
}

/* ── 3. Document Viewer PopUp ─────────────────────────────────── */

static void load_doc_into_buffer(GtkTextBuffer *buffer, guint index) {
  if (index == 1) {
    const char *help_txt =
        "X3D Toggle — Quick Help & Usage Guide\n"
        "═══════════════════════════════════════════════════════════\n\n"
        "• Dashboard: Live status widgets for Daemon, Hardware Modes,\n"
        "  System Profile, and SMU/RAPL Power & Current telemetry.\n"
        "• Staged Widgets: Edit fields freely; click 'Apply' to commit,\n"
        "  'Cancel' to restore active runtime values, or 'Default' to\n"
        "  load stock values without committing.\n"
        "• Immediate Widgets: Hardware Modes & Lifecycle actions take\n"
        "  effect immediately on press.\n"
        "• CLI Quick Reference:\n"
        "    x3d-toggle cache      Prioritize 3D V-Cache CCD (Rabbit)\n"
        "    x3d-toggle frequency  Prioritize Frequency CCD (Cheetah)\n"
        "    x3d-toggle dual       Unpark both CCDs (Bear)\n"
        "    x3d-toggle auto       Return to native CPPC control (Elk)\n"
        "    x3d-toggle status     Print live daemon & CCD snapshot\n";
    gtk_text_buffer_set_text(buffer, help_txt, -1);
    return;
  }

  if (index == 2) {
    extern const char *const insults[];
    extern const int insults_count;
    GString *str =
        g_string_new("X3D Toggle — The Great Book of Insults\n\n");
    for (int i = 0; i < insults_count; i++)
      g_string_append_printf(str, "  • %s\n", insults[i]);
    gtk_text_buffer_set_text(buffer, str->str, -1);
    g_string_free(str, TRUE);
    return;
  }

  const char *res_path = "/org/x3d-toggle/gui/README.md";
  switch (index) {
  case 0:
    res_path = "/org/x3d-toggle/gui/README.md";
    break;
  case 3:
    res_path = "/org/x3d-toggle/gui/x3d-toggle.1.md";
    break;
  case 4:
    res_path = "/org/x3d-toggle/gui/ARCHITECTURE.md";
    break;
  case 5:
    res_path = "/org/x3d-toggle/gui/DISCLAIMER";
    break;
  case 6:
    res_path = "/org/x3d-toggle/gui/LICENSE";
    break;
  case 7:
    res_path = "/org/x3d-toggle/gui/Layout.md";
    break;
  default:
    break;
  }

  GError *err = NULL;
  GBytes *bytes = g_resources_lookup_data(res_path, 0, &err);
  if (bytes) {
    gsize sz = 0;
    const gchar *data = g_bytes_get_data(bytes, &sz);
    gtk_text_buffer_set_text(buffer, data, (gint)sz);
    g_bytes_unref(bytes);
  } else {
    gtk_text_buffer_set_text(buffer, err ? err->message : "Document not found.",
                             -1);
    if (err)
      g_error_free(err);
  }
}

static void on_doc_dd_changed(GObject *dd, GParamSpec *pspec, gpointer data) {
  (void)pspec;
  load_doc_into_buffer(GTK_TEXT_BUFFER(data),
                       gtk_drop_down_get_selected(GTK_DROP_DOWN(dd)));
}

void popup_docs_show_viewer(GtkWindow *parent) {
  GtkWidget *win = adw_window_new();
  gtk_window_set_title(GTK_WINDOW(win), "X3D Toggle — Document Viewer");
  gtk_window_set_default_size(GTK_WINDOW(win), 780, 580);
  if (parent)
    gtk_window_set_transient_for(GTK_WINDOW(win), parent);
  gtk_window_set_destroy_with_parent(GTK_WINDOW(win), TRUE);

  GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *hb = adw_header_bar_new();

  const char *docs[] = {"README",
                        "Help",
                        "Great Book of Insults",
                        "Manpage (x3d-toggle.1)",
                        "Architecture",
                        "Disclaimer",
                        "License (GPLv3)",
                        "UI Layout Specification",
                        NULL};
  GtkWidget *dd = gtk_drop_down_new_from_strings(docs);
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(hb), dd);
  gtk_box_append(GTK_BOX(outer), hb);

  GtkWidget *scroll = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(scroll, TRUE);
  gtk_widget_set_hexpand(scroll, TRUE);
  gtk_widget_set_margin_start(scroll, 16);
  gtk_widget_set_margin_end(scroll, 16);
  gtk_widget_set_margin_top(scroll, 12);
  gtk_widget_set_margin_bottom(scroll, 16);
  gtk_widget_add_css_class(scroll, "x3d-extras-frame");

  GtkWidget *tv = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv), GTK_WRAP_WORD_CHAR);
  gtk_text_view_set_monospace(GTK_TEXT_VIEW(tv), TRUE);
  gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tv), 14);
  gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(tv), 14);
  gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tv), 16);
  gtk_text_view_set_right_margin(GTK_TEXT_VIEW(tv), 16);
  gtk_widget_add_css_class(tv, "x3d-value");

  GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
  g_signal_connect(dd, "notify::selected", G_CALLBACK(on_doc_dd_changed), buf);
  load_doc_into_buffer(buf, 0);

  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), tv);
  gtk_box_append(GTK_BOX(outer), scroll);
  adw_window_set_content(ADW_WINDOW(win), outer);

  gtk_window_present(GTK_WINDOW(win));
}
