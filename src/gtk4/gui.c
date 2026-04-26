/* GTK4 Frontend for X3D Toggle
 *
 * Wrapper: Loads declarative UI, binds button IDs to CLI commands.
 * `action_x3dtoggle_X` button -> executes `x3d-toggle X`
 */

#include <adwaita.h>
#include <gtk/gtk.h>
#include <string.h>

extern int socket_send(const char *cmd, char *response, size_t resp_len);
extern size_t scat(char *dest, const char *src, size_t dest_size);
extern int printf_sn(char *buf, size_t size, const char *fmt, ...);

#define BUFF_LINE 256
#define BUFF_INFO 128
#define BUFF_STATE 16
#define REFRESH_INTERVAL_KEY "REFRESH_INTERVAL="

static GtkWidget *lbl_status_dump = NULL;

static gboolean update_dashboard_cb(gpointer user_data) {
  (void)user_data;
  if (!lbl_status_dump)
    return G_SOURCE_CONTINUE;

  char info[BUFF_INFO] = {0};
  if (socket_send("DAEMON_INFO", info, sizeof(info)) != 0) {
    gtk_label_set_label(GTK_LABEL(lbl_status_dump),
                        "Daemon offline or unreachable.");
    return G_SOURCE_CONTINUE;
  }

  char display[BUFF_LINE * 2];
  char state_str[BUFF_STATE] = "Unknown";
  char active_str[BUFF_STATE] = "Inactive";
  char interval_str[BUFF_STATE] = "0.5";

  char *st = strstr(info, "STATE=");
  char *ba = strstr(info, "BPF_ACTIVE=");
  char *ri = strstr(info, REFRESH_INTERVAL_KEY);

  if (st) {
    scat(state_str, st + strlen("STATE="), sizeof(state_str));
    char *sem = strchr(state_str, ';');
    if (sem)
      *sem = '\0';
  }
  if (ba) {
    const char *bpf_value = ba + strlen("BPF_ACTIVE=");
    if (atoi(bpf_value))
      scat(active_str, "eBPF (Active)", sizeof(active_str));
    else
      scat(active_str, "Polling", sizeof(active_str));
  } else
    scat(active_str, "Polling", sizeof(active_str));

  if (ri) {
    scat(interval_str, ri + strlen(REFRESH_INTERVAL_KEY), sizeof(interval_str));
    char *sem = strchr(interval_str, ';');
    if (sem)
      *sem = '\0';
  }

  printf_sn(display, sizeof(display),
            "<b>Daemon State:</b>  %s\n"
            "<b>Detection:</b>     %s\n"
            "<b>Refresh Rate:</b>   %ss",
            state_str, active_str, interval_str);

  gtk_label_set_markup(GTK_LABEL(lbl_status_dump), display);
  return G_SOURCE_CONTINUE;
}

static void on_action_clicked(GtkButton *btn, gpointer user_data) {
  (void)btn;
  const char *cmd = (const char *)user_data;

  if (!cmd || !(g_strcmp0(cmd, "0") == 0 || g_strcmp0(cmd, "1") == 0)) {
    g_warning("Rejected invalid x3d-toggle command argument");
    return;
  }

  gchar *args[] = {"x3d-toggle", (gchar *)cmd, NULL};
  GError *error = NULL;
  if (!g_spawn_async(NULL, args, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL,
                     &error)) {
    g_warning("Failed to launch x3d-toggle: %s", error->message);
    g_clear_error(&error);
  }
}

static GtkWidget *add_nav_row(GtkListBox *list, const char *id,
                              const char *title) {
  GtkWidget *row = gtk_list_box_row_new();
  gtk_widget_set_name(row, id);
  GtkWidget *lbl = gtk_label_new(title);
  gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
  gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), lbl);
  gtk_list_box_append(list, row);
  return row;
}

static void on_nav_row_selected(GtkListBox *box, GtkListBoxRow *row,
                                gpointer user_data) {
  if (!row)
    return;
  GtkStack *stack = GTK_STACK(user_data);
  const char *id = gtk_widget_get_name(GTK_WIDGET(row));
  gtk_stack_set_visible_child_name(stack, id);
}

static void on_app_activate(GtkApplication *app, gpointer user_data) {
  (void)user_data;

  /* Silence libadwaita dark theme warnings and enforce dark mode */
  AdwStyleManager *style_manager = adw_style_manager_get_default();
  adw_style_manager_set_color_scheme(style_manager, ADW_COLOR_SCHEME_FORCE_DARK);

  /* Ensure GtkSettings doesn't conflict with libadwaita */
  g_object_set(gtk_settings_get_default(),
               "gtk-application-prefer-dark-theme", FALSE,
               NULL);

  /* Load CSS */
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_resource(provider,
                                      "/org/x3d-toggle/gui/theme.css");
  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  /* Load UI */
  GtkBuilder *builder =
      gtk_builder_new_from_resource("/org/x3d-toggle/gui/x3d-toggle.ui");

  GtkWindow *window =
      GTK_WINDOW(gtk_builder_get_object(builder, "main_window"));
  gtk_window_set_application(window, app);

  lbl_status_dump =
      GTK_WIDGET(gtk_builder_get_object(builder, "lbl_status_dump"));

  /* Bind Navigation */
  GtkListBox *sidebar =
      GTK_LIST_BOX(gtk_builder_get_object(builder, "sidebar_list"));
  GtkStack *stack = GTK_STACK(gtk_builder_get_object(builder, "content_stack"));

  add_nav_row(sidebar, "dashboard", "Dashboard");
  add_nav_row(sidebar, "modes", "Hardware Modes");
  add_nav_row(sidebar, "daemon", "Daemon Settings");

  g_signal_connect(sidebar, "row-selected", G_CALLBACK(on_nav_row_selected),
                   stack);

  /* Select Dashboard by default */
  GtkListBoxRow *first_row = gtk_list_box_get_row_at_index(sidebar, 0);
  if (first_row) {
    gtk_list_box_select_row(sidebar, first_row);
    g_signal_emit_by_name(sidebar, "row-selected", first_row);
  }

  /* Bind Actions automatically by ID (action_x3dtoggle_<cmd>) */
  const char *actions[] = {"cache",  "frequency", "dual",  "default",
                           "auto",   "wake",      "sleep", "stop",
                           "enable", "start",     NULL};

  for (int i = 0; actions[i] != NULL; i++) {
    char btn_id[64];
    printf_sn(btn_id, sizeof(btn_id), "action_x3dtoggle_%s", actions[i]);
    GObject *obj = gtk_builder_get_object(builder, btn_id);
    if (obj) {
      char *cmd_copy = g_strdup(actions[i]);
      g_signal_connect_data(obj, "clicked", G_CALLBACK(on_action_clicked),
                            cmd_copy, (GClosureNotify)g_free, 0);
    }
  }

  g_object_unref(builder);
  gtk_window_present(window);

  /* Start Live Dashboard Polling */
  g_timeout_add_seconds(1, update_dashboard_cb, NULL);
  update_dashboard_cb(NULL); /* Initial fetch */
}

int main(int argc, char **argv) {
  AdwApplication *app =
      adw_application_new("org.x3d-toggle.gui", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(on_app_activate), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
