/* GTK4 / libadwaita Native Frontend Bootstrap for X3D Toggle
 * `gui.c`
 * Lean entry point: registers custom GObject widget types before GtkBuilder,
 * initializes IPC transport, CSS, Stubs, View shell, and Frame orchestrator.
 */

#include "gui_frame.h"
#include "gui_ipc.h"
#include "gui_linker.h"
#include "gui_stubs.h"
#include "gui_view.h"

static X3dGuiIpc *g_ipc = NULL;
static X3dGuiView *g_view = NULL;
static X3dGuiFrame *g_frame = NULL;

static void on_app_activate(GtkApplication *app, gpointer user_data) {
  (void)user_data;

  if (g_view && g_view->main_window) {
    gtk_window_present(g_view->main_window);
    return;
  }

  gtk_window_set_default_icon_name("x3d-toggle");
  gui_linker_register_types();
  x3d_gui_view_init_css();

  g_ipc = x3d_gui_ipc_new();
  x3d_stubs_init(g_ipc);

  X3dGuiThemePrefs prefs;
  x3d_ipc_load_theme_prefs(&prefs);
  x3d_gui_view_apply_theme_prefs(&prefs);

  g_view = x3d_gui_view_build(app);
  g_frame = x3d_gui_frame_new(g_ipc, g_view);

  gtk_window_present(g_view->main_window);
}

static void on_app_shutdown(GtkApplication *app, gpointer user_data) {
  (void)app;
  (void)user_data;

  x3d_stubs_shutdown();
  if (g_frame) {
    x3d_gui_frame_free(g_frame);
    g_frame = NULL;
  }
  if (g_view) {
    x3d_gui_view_free(g_view);
    g_view = NULL;
  }
  if (g_ipc) {
    g_object_unref(g_ipc);
    g_ipc = NULL;
  }
}

int main(int argc, char **argv) {
  g_set_prgname("x3d-toggle");

  AdwApplication *app =
      adw_application_new("org.x3d_toggle.gui", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(on_app_activate), NULL);
  g_signal_connect(app, "shutdown", G_CALLBACK(on_app_shutdown), NULL);

  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
