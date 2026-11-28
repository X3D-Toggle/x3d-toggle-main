/* Shared Modal Host & Popup Dispatcher Header
 * `gui_popups.h`
 */

#ifndef X3D_GUI_POPUPS_H
#define X3D_GUI_POPUPS_H

#include "gui_ipc.h"
#include "gui_stubs.h"
#include "pages/gui_widgets.h"

typedef enum {
  X3D_TELEM_CPU = 0,
  X3D_TELEM_GPU = 1,
  X3D_TELEM_AOD = 2
} X3dTelemetryKind;

typedef struct {
  char hwmon_path[512];
  char card_name[32];
  char pci_slot[64];
  char label[160];
  gboolean is_discrete;
} X3dGpuInfo;

typedef struct {
  char load_str[32];
  char clocks_str[64];
  char vram_str[64];
  char power_str[64];
  char temps_str[96];
  char fan_speed[64];

  double load_pct;
  double sclk_mhz;
  double mclk_mhz;
  double vram_used_gib;
  double vram_total_gib;
  double power_w;
  double power_cap_w;
  double temp_edge_c;
  double temp_hotspot_c;
  double temp_mem_c;
  double fan_pct;
  double fan_rpm;
} X3dGpuTelemetry;

typedef struct {
  char model_name[128];
  char topology_str[96];
  char load_str[64];
  char clocks_str[96];
  char temps_str[96];
  char power_str[64];
  char profile_str[96];

  double total_load_pct;
  double ccd0_load_pct;
  double ccd1_load_pct;
  double core_load_pct[128];
  int core_count;
  int ccd1_start;
  double avg_clock_mhz;
  double peak_clock_mhz;
  double max_clock_mhz;
  double temp_tctl_c;
  double temp_ccd1_c;
  double temp_ccd2_c;
  double ppt_w;
  double ppt_lim_w;
} X3dCpuTelemetry;

typedef void (*X3dSampleCpuFn)(X3dGuiIpc *ipc, X3dCpuTelemetry *out);
typedef void (*X3dSampleGpuFn)(const X3dGpuInfo *gpu, X3dGpuTelemetry *out);

void gui_popups_register_samplers(X3dSampleCpuFn cpu_fn, X3dSampleGpuFn gpu_fn);
void gui_popups_sample_cpu(X3dGuiIpc *ipc, X3dCpuTelemetry *out);
void gui_popups_sample_gpu(const X3dGpuInfo *gpu, X3dGpuTelemetry *out);

typedef enum {
  X3D_POPUP_FLOOR_FREQ = 0,
  X3D_POPUP_TOPOLOGY_MAP,
  X3D_POPUP_PROFILE_MGMT,
  X3D_POPUP_HYST_BASELINES,
  X3D_POPUP_AOD_RESTART,
  X3D_POPUP_KDE_SNI_STYLE,
  X3D_POPUP_DESKTOP_WIDGETS,
  X3D_POPUP_TELEMETRY,
  X3D_POPUP_DRY_RUN,
  X3D_POPUP_JOURNAL_ARCHIVE,
  X3D_POPUP_JOURNAL_ROTATE,
  X3D_POPUP_DOC_VIEWER
} X3dPopupId;

typedef struct {
  X3dPopupId id;
  GtkWindow *parent;
  X3dGuiIpc *ipc;
  X3dStagedWidget *owner_staged;
  char *staged_mask_buf;
  size_t mask_sz;
  GtkWidget *summary_lbl;
  X3dTelemetryKind telem_kind;
  const X3dGpuInfo *gpu_opt;
} X3dPopupRequest;

/* Shared AdwWindow modal shell constructor */
GtkWindow *gui_popups_create_shell(GtkWindow *parent, const char *title,
                                   int width, int height, gboolean modal,
                                   GtkBox **content_out);

/* Central popup dispatcher by popup ID */
void gui_popups_dispatch(const X3dPopupRequest *req);

/* Typed dispatcher helpers */
void gui_popups_show_floor_freq(GtkWindow *parent,
                                X3dStagedWidget *owner_staged);
void gui_popups_show_topology_map(GtkWindow *parent, X3dGuiIpc *ipc,
                                  X3dStagedWidget *owner_staged,
                                  char *staged_mask_buf, size_t mask_sz,
                                  GtkWidget *summary_lbl);
void gui_popups_show_profile_mgmt(GtkWindow *parent);
void gui_popups_show_hyst_baselines(GtkWindow *parent,
                                    X3dStagedWidget *owner_staged);
void gui_popups_show_aod_restart(GtkWindow *parent);
void gui_popups_show_sni_style(GtkWindow *parent,
                               X3dStagedWidget *owner_staged);
void gui_popups_show_desktop_widgets(GtkWindow *parent, X3dGuiIpc *ipc,
                                     X3dStagedWidget *owner_staged);
void gui_popups_show_telemetry(GtkWindow *parent, X3dGuiIpc *ipc,
                               X3dTelemetryKind kind,
                               const X3dGpuInfo *gpu_opt);
void gui_popups_show_dry_run(GtkWindow *parent, X3dGuiIpc *ipc);
void gui_popups_show_journal_maint(GtkWindow *parent, gboolean is_rotate);
void gui_popups_show_doc_viewer(GtkWindow *parent);

#endif /* X3D_GUI_POPUPS_H */
