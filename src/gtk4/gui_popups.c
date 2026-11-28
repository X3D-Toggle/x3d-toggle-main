/* Shared Modal Host & Popup Dispatcher Implementation
 * `gui_popups.c`
 */

#include "gui_popups.h"
#include "popups/popup_baselines.h"
#include "popups/popup_docs.h"
#include "popups/popup_profiles.h"
#include "popups/popup_style.h"
#include "popups/popup_topology.h"
#include <string.h>

static X3dSampleCpuFn g_cpu_sampler = NULL;
static X3dSampleGpuFn g_gpu_sampler = NULL;

void gui_popups_register_samplers(X3dSampleCpuFn cpu_fn,
                                  X3dSampleGpuFn gpu_fn) {
  if (cpu_fn)
    g_cpu_sampler = cpu_fn;
  if (gpu_fn)
    g_gpu_sampler = gpu_fn;
}

void gui_popups_sample_cpu(X3dGuiIpc *ipc, X3dCpuTelemetry *out) {
  if (!out)
    return;
  memset(out, 0, sizeof(*out));
  if (g_cpu_sampler)
    g_cpu_sampler(ipc, out);
}

void gui_popups_sample_gpu(const X3dGpuInfo *gpu, X3dGpuTelemetry *out) {
  if (!out)
    return;
  memset(out, 0, sizeof(*out));
  if (g_gpu_sampler)
    g_gpu_sampler(gpu, out);
}

GtkWindow *gui_popups_create_shell(GtkWindow *parent, const char *title,
                                   int width, int height, gboolean modal,
                                   GtkBox **content_out) {
  GtkWidget *win = adw_window_new();
  gtk_window_set_title(GTK_WINDOW(win), title);
  gtk_window_set_default_size(GTK_WINDOW(win), width, height);
  if (parent)
    gtk_window_set_transient_for(GTK_WINDOW(win), parent);
  gtk_window_set_modal(GTK_WINDOW(win), modal);
  gtk_window_set_destroy_with_parent(GTK_WINDOW(win), TRUE);

  GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *hb = adw_header_bar_new();
  GtkWidget *hb_lbl = gtk_label_new(title);
  gtk_widget_add_css_class(hb_lbl, "x3d-accent");
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(hb), hb_lbl);
  gtk_box_append(GTK_BOX(outer), hb);

  GtkWidget *scroll = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER,
                                 GTK_POLICY_AUTOMATIC);
  gtk_widget_set_vexpand(scroll, TRUE);
  gtk_widget_set_hexpand(scroll, TRUE);

  GtkWidget *body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
  gtk_widget_set_margin_start(body, 20);
  gtk_widget_set_margin_end(body, 20);
  gtk_widget_set_margin_top(body, 16);
  gtk_widget_set_margin_bottom(body, 18);

  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), body);
  gtk_box_append(GTK_BOX(outer), scroll);
  adw_window_set_content(ADW_WINDOW(win), outer);

  if (content_out)
    *content_out = GTK_BOX(body);
  return GTK_WINDOW(win);
}

void gui_popups_dispatch(const X3dPopupRequest *req) {
  if (!req)
    return;
  switch (req->id) {
  case X3D_POPUP_FLOOR_FREQ:
    popup_topology_show_floor_freq(req->parent, req->owner_staged);
    break;
  case X3D_POPUP_TOPOLOGY_MAP:
    popup_topology_show_map(req->parent, req->ipc, req->owner_staged,
                            req->staged_mask_buf, req->mask_sz,
                            req->summary_lbl);
    break;
  case X3D_POPUP_PROFILE_MGMT:
    popup_profiles_show_mgmt(req->parent);
    break;
  case X3D_POPUP_HYST_BASELINES:
    popup_baselines_show_hysteresis(req->parent, req->owner_staged);
    break;
  case X3D_POPUP_AOD_RESTART:
    popup_baselines_show_aod_restart(req->parent);
    break;
  case X3D_POPUP_KDE_SNI_STYLE:
    popup_style_show_kde_sni(req->parent, req->owner_staged);
    break;
  case X3D_POPUP_DESKTOP_WIDGETS:
    popup_style_show_desktop_widgets(req->parent, req->ipc, req->owner_staged);
    break;
  case X3D_POPUP_TELEMETRY:
    popup_style_show_telemetry(req->parent, req->ipc, req->telem_kind,
                               req->gpu_opt);
    break;
  case X3D_POPUP_DRY_RUN:
    popup_docs_show_dry_run(req->parent, req->ipc);
    break;
  case X3D_POPUP_JOURNAL_ARCHIVE:
    popup_docs_show_journal_maint(req->parent, FALSE);
    break;
  case X3D_POPUP_JOURNAL_ROTATE:
    popup_docs_show_journal_maint(req->parent, TRUE);
    break;
  case X3D_POPUP_DOC_VIEWER:
    popup_docs_show_viewer(req->parent);
    break;
  }
}

void gui_popups_show_floor_freq(GtkWindow *parent,
                                X3dStagedWidget *owner_staged) {
  X3dPopupRequest r = {
      .id = X3D_POPUP_FLOOR_FREQ, .parent = parent, .owner_staged = owner_staged};
  gui_popups_dispatch(&r);
}

void gui_popups_show_topology_map(GtkWindow *parent, X3dGuiIpc *ipc,
                                  X3dStagedWidget *owner_staged,
                                  char *staged_mask_buf, size_t mask_sz,
                                  GtkWidget *summary_lbl) {
  X3dPopupRequest r = {.id = X3D_POPUP_TOPOLOGY_MAP,
                       .parent = parent,
                       .ipc = ipc,
                       .owner_staged = owner_staged,
                       .staged_mask_buf = staged_mask_buf,
                       .mask_sz = mask_sz,
                       .summary_lbl = summary_lbl};
  gui_popups_dispatch(&r);
}

void gui_popups_show_profile_mgmt(GtkWindow *parent) {
  X3dPopupRequest r = {.id = X3D_POPUP_PROFILE_MGMT, .parent = parent};
  gui_popups_dispatch(&r);
}

void gui_popups_show_hyst_baselines(GtkWindow *parent,
                                    X3dStagedWidget *owner_staged) {
  X3dPopupRequest r = {.id = X3D_POPUP_HYST_BASELINES,
                       .parent = parent,
                       .owner_staged = owner_staged};
  gui_popups_dispatch(&r);
}

void gui_popups_show_aod_restart(GtkWindow *parent) {
  X3dPopupRequest r = {.id = X3D_POPUP_AOD_RESTART, .parent = parent};
  gui_popups_dispatch(&r);
}

void gui_popups_show_sni_style(GtkWindow *parent,
                               X3dStagedWidget *owner_staged) {
  X3dPopupRequest r = {.id = X3D_POPUP_KDE_SNI_STYLE,
                       .parent = parent,
                       .owner_staged = owner_staged};
  gui_popups_dispatch(&r);
}

void gui_popups_show_desktop_widgets(GtkWindow *parent, X3dGuiIpc *ipc,
                                     X3dStagedWidget *owner_staged) {
  X3dPopupRequest r = {.id = X3D_POPUP_DESKTOP_WIDGETS,
                       .parent = parent,
                       .ipc = ipc,
                       .owner_staged = owner_staged};
  gui_popups_dispatch(&r);
}

void gui_popups_show_telemetry(GtkWindow *parent, X3dGuiIpc *ipc,
                               X3dTelemetryKind kind,
                               const X3dGpuInfo *gpu_opt) {
  X3dPopupRequest r = {.id = X3D_POPUP_TELEMETRY,
                       .parent = parent,
                       .ipc = ipc,
                       .telem_kind = kind,
                       .gpu_opt = gpu_opt};
  gui_popups_dispatch(&r);
}

void gui_popups_show_dry_run(GtkWindow *parent, X3dGuiIpc *ipc) {
  X3dPopupRequest r = {.id = X3D_POPUP_DRY_RUN, .parent = parent, .ipc = ipc};
  gui_popups_dispatch(&r);
}

void gui_popups_show_journal_maint(GtkWindow *parent, gboolean is_rotate) {
  X3dPopupRequest r = {
      .id = is_rotate ? X3D_POPUP_JOURNAL_ROTATE : X3D_POPUP_JOURNAL_ARCHIVE,
      .parent = parent};
  gui_popups_dispatch(&r);
}

void gui_popups_show_doc_viewer(GtkWindow *parent) {
  X3dPopupRequest r = {.id = X3D_POPUP_DOC_VIEWER, .parent = parent};
  gui_popups_dispatch(&r);
}
