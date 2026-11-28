/* Modular Page & Popup Linker Implementation
 * `gui_linker.c`
 */

#include "gui_linker.h"
#include "pages/gui_config.h"
#include "pages/gui_cpu.h"
#include "pages/gui_daemon.h"
#include "pages/gui_dashboard.h"
#include "pages/gui_dev.h"
#include "pages/gui_extras.h"
#include "pages/gui_gpu.h"
#include "pages/gui_profile.h"
#include "pages/gui_thread.h"
#include "pages/gui_widgets.h"
#include <string.h>

#define X3D_NUM_PAGE_MODULES 10

static X3dPageCtx g_ctx;
static const X3dPageModule *g_modules[X3D_NUM_PAGE_MODULES];
static gboolean g_initialized = FALSE;

void gui_linker_register_types(void) { x3d_widgets_register_types(); }

void gui_linker_init(const X3dPageCtx *ctx) {
  if (ctx)
    g_ctx = *ctx;

  /* Ordered to match canonical sidebar navigation & structural dividers */
  g_modules[0] = gui_widgets_get_module();
  g_modules[1] = gui_dashboard_get_module();
  g_modules[2] = gui_cpu_get_module();
  g_modules[3] = gui_gpu_get_module();
  g_modules[4] = gui_thread_get_module();
  g_modules[5] = gui_daemon_get_module();
  g_modules[6] = gui_config_get_module();
  g_modules[7] = gui_dev_get_module();
  g_modules[8] = gui_profile_get_module();
  g_modules[9] = gui_extras_get_module();
  g_initialized = TRUE;
}

void gui_linker_mount_all(void) {
  if (!g_initialized || !g_ctx.view)
    return;

  for (guint i = 0; i < X3D_NUM_PAGE_MODULES; i++) {
    const X3dPageModule *m = g_modules[i];
    if (!m)
      continue;
    if (m->id && m->title) {
      x3d_gui_view_add_sidebar_row(g_ctx.view, m->id, m->title, m->is_subnav,
                                   m->initially_hidden);
      if (m->divider_after)
        x3d_gui_view_add_sidebar_separator(g_ctx.view);
    }
    if (m->build)
      (void)m->build(&g_ctx);
  }
}

const X3dPageModule *gui_linker_find_page(const char *page_id) {
  if (!g_initialized || !page_id || !page_id[0])
    return NULL;

  /* Standalone Dashboard subviews belong to the CPU module */
  if (strcmp(page_id, "hardware_modes_view") == 0 ||
      strcmp(page_id, "system_profile_view") == 0) {
    return gui_cpu_get_module();
  }

  for (guint i = 0; i < X3D_NUM_PAGE_MODULES; i++) {
    const X3dPageModule *m = g_modules[i];
    if (m && m->id && strcmp(m->id, page_id) == 0)
      return m;
  }
  return NULL;
}

void gui_linker_refresh_active(const char *page_id) {
  const X3dPageModule *m = gui_linker_find_page(page_id);
  if (m && m->refresh)
    m->refresh(&g_ctx);
}

void gui_linker_activate_page(const char *old_page_id,
                              const char *new_page_id) {
  const X3dPageModule *old_m = gui_linker_find_page(old_page_id);
  const X3dPageModule *new_m = gui_linker_find_page(new_page_id);

  if (old_m && old_m->deactivate)
    old_m->deactivate(&g_ctx);

  if (new_m) {
    if (new_m->activate)
      new_m->activate(&g_ctx);
    else if (new_m->refresh)
      new_m->refresh(&g_ctx);
  }
}

void gui_linker_deactivate_all(void) {
  if (!g_initialized)
    return;
  for (guint i = 0; i < X3D_NUM_PAGE_MODULES; i++) {
    const X3dPageModule *m = g_modules[i];
    if (m && m->deactivate)
      m->deactivate(&g_ctx);
  }
}

void gui_linker_notify_interval_changed(guint interval_ms) {
  if (!g_initialized)
    return;
  for (guint i = 0; i < X3D_NUM_PAGE_MODULES; i++) {
    const X3dPageModule *m = g_modules[i];
    if (m && m->on_interval_changed)
      m->on_interval_changed(&g_ctx, interval_ms);
  }
}

void gui_linker_broadcast_hw_mode(const char *mode_upper) {
  if (!g_initialized || !mode_upper)
    return;
  for (guint i = 0; i < X3D_NUM_PAGE_MODULES; i++) {
    const X3dPageModule *m = g_modules[i];
    if (m && m->highlight_hw_mode)
      m->highlight_hw_mode(mode_upper);
  }
}
