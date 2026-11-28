/* GTK4 Frontend Core IPC Transport & Global Capability Layer
 * `gui_ipc.h`
 * Pure UNIX domain socket transport to x3d-daemon, global capability flags
 * (X3D_CAP_*), reactive GObject state, and desktop process launchers.
 * Strictly zero GTK widget dependencies.
 */

#ifndef X3D_GUI_IPC_H
#define X3D_GUI_IPC_H

#include "../../build/config.h"
#include "../../include/aod.h"
#include "../../include/libc.h"
#include "error.h"
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <glib-object.h>

#ifdef __CPPCHECK__
#define G_DECLARE_FINAL_TYPE(ModuleObjName, module_obj_name, MODULE, OBJ_NAME, \
                             ParentName)                                       \
  typedef struct _##ModuleObjName ModuleObjName;                               \
  typedef struct {                                                             \
    ParentName##Class parent_class;                                            \
  } ModuleObjName##Class;                                                      \
  GType module_obj_name##_get_type(void);
#define G_DEFINE_TYPE(TN, t_n, T_P)                                            \
  static void t_n##_init(TN *self);                                            \
  static void t_n##_class_init(TN##Class *klass);                              \
  static gpointer t_n##_parent_class = NULL;                                   \
  GType t_n##_get_type(void) {                                                 \
    (void)t_n##_init;                                                          \
    (void)t_n##_class_init;                                                    \
    return (GType)0;                                                           \
  }
#endif

/* Global Capability Aliases mapped directly to daemon-probed CAP_* bits */
#define X3D_CAP_DYNAMIC_EPP   CAP_DYNAMIC_EPP
#define X3D_CAP_FLOOR_FREQ    CAP_FLOOR_FREQ
#define X3D_CAP_PREFCORE_RANK CAP_PREFCORE_RANK
#define X3D_CAP_TCP_CONG      CAP_TCP_CONG
#define X3D_CAP_PCIE_ASPM     CAP_PCIE_ASPM
#define X3D_CAP_CPU_DMA_LAT   CAP_CPU_DMA_LAT

#define X3D_TYPE_GUI_IPC (x3d_gui_ipc_get_type())
G_DECLARE_FINAL_TYPE(X3dGuiIpc, x3d_gui_ipc, X3D, GUI_IPC, GObject)

typedef struct {
  guint theme_idx; /* 0=Obsidian, 1=Imperial, 2=LACT, 3=Light, 4=Dark, 5=System */
  guint color_idx; /* 0=Theme Default, 1..5=Accent overrides */
  guint font_idx;  /* 0=Theme Default, 1..4=Font overrides */
  int font_size_pt;
  guint update_interval_ms; /* 250..5000 ms (default 500) */
} X3dGuiThemePrefs;

X3dGuiIpc *x3d_gui_ipc_new(void);

/* GObject Reactive Property Accessors */
uint64_t x3d_gui_ipc_get_caps(const X3dGuiIpc *self);
void x3d_gui_ipc_set_caps(X3dGuiIpc *self, uint64_t caps);
gboolean x3d_gui_ipc_get_force_unsupported(const X3dGuiIpc *self);
void x3d_gui_ipc_set_force_unsupported(X3dGuiIpc *self, gboolean force);
gboolean x3d_gui_ipc_get_adv_enabled(const X3dGuiIpc *self);
void x3d_gui_ipc_set_adv_enabled(X3dGuiIpc *self, gboolean enabled);
gboolean x3d_gui_ipc_get_dev_enabled(const X3dGuiIpc *self);
void x3d_gui_ipc_set_dev_enabled(X3dGuiIpc *self, gboolean enabled);
gboolean x3d_gui_ipc_get_aod_supported(const X3dGuiIpc *self);
void x3d_gui_ipc_set_aod_supported(X3dGuiIpc *self, gboolean supported);
gboolean x3d_gui_ipc_get_aod_reboot_pending(const X3dGuiIpc *self);
void x3d_gui_ipc_set_aod_reboot_pending(X3dGuiIpc *self, gboolean pending);
guint x3d_gui_ipc_get_update_interval_ms(const X3dGuiIpc *self);
void x3d_gui_ipc_set_update_interval_ms(X3dGuiIpc *self, guint ms);
gboolean x3d_gui_ipc_has_cap(const X3dGuiIpc *self, uint64_t cap_mask);

/* Core UNIX Socket IPC Transport & Framing */
int x3d_ipc_send(const char *cmd, char *resp, size_t resp_len);
gboolean x3d_ipc_cmd_ok(const char *cmd);
gboolean x3d_ipc_kv_get(const char *haystack, const char *key, char sep,
                        char *out, size_t out_sz);
const char *x3d_ipc_config_get(const char *key);
gboolean x3d_ipc_config_set(const char *key, const char *value);
gboolean x3d_ipc_advanced_get(const char *sysctl_key, char *out, size_t out_sz);
gboolean x3d_ipc_advanced_set(const char *sysctl_key, const char *value);
void x3d_ipc_read_aod_live(AodLiveStats *out);

/* Shared Topology IPC Helper (used by CPU, Thread Settings & Popups) */
int x3d_ipc_read_topology_masks(char *cache_out, size_t cache_sz,
                                char *freq_out, size_t freq_sz,
                                int *ccd1_start, int *total_cores);
int x3d_ipc_read_topology(char *mask_out, size_t mask_sz, int *ccd1_start,
                          int *total_cores);

/* Desktop-Session Process & Tool Launchers */
void x3d_ipc_spawn(char **args, char **env);
void x3d_ipc_spawn_terminal(const char *cmd_str);
gboolean x3d_ipc_is_tool_available(const char *bin_name,
                                   const char *desktop_id);
void x3d_ipc_launch_desktop_tool(const char *bin_name, const char *arg1,
                                 const char *desktop_id);

/* User GUI Preferences (~/.config/x3d-toggle/gui.conf) */
void x3d_ipc_load_theme_prefs(X3dGuiThemePrefs *prefs);
gboolean x3d_ipc_save_theme_prefs(const X3dGuiThemePrefs *prefs);

#endif /* X3D_GUI_IPC_H */
