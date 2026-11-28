/* Native GObject / GtkWidget Subclasses & Control Primitives Implementation
 * `pages/gui_widgets.c`
 */

#include "gui_widgets.h"
#include "../gui_popups.h"
#include "../gui_stubs.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  GtkRange *range;
  GtkEditable *entry;
  gboolean syncing;
} SliderEntryPair;

/* ── Shared 3-Column (6-Virtual-Column) Centered Grid Engine ──── */

GtkWidget *x3d_centered_3col_grid_new(void) {
  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
  gtk_widget_set_hexpand(grid, TRUE);
  return grid;
}

void x3d_grid_pack_centered_3col(GtkGrid *grid, GtkWidget *const *items,
                                 guint count) {
  if (!grid)
    return;
  GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(grid));
  while (child) {
    GtkWidget *next = gtk_widget_get_next_sibling(child);
    gtk_grid_remove(grid, child);
    child = next;
  }
  if (!items || count == 0)
    return;

  guint idx = 0;
  int row = 0;
  while (idx < count) {
    guint rem = count - idx;
    if (rem >= 3) {
      for (int c = 0; c < 3; c++) {
        GtkWidget *w = items[idx++];
        gtk_widget_set_hexpand(w, TRUE);
        gtk_grid_attach(grid, w, c * 2, row, 2, 1);
      }
    } else if (rem == 2) {
      GtkWidget *w0 = items[idx++];
      GtkWidget *w1 = items[idx++];
      gtk_widget_set_hexpand(w0, TRUE);
      gtk_widget_set_hexpand(w1, TRUE);
      gtk_grid_attach(grid, w0, 1, row, 2, 1);
      gtk_grid_attach(grid, w1, 3, row, 2, 1);
    } else {
      GtkWidget *w0 = items[idx++];
      gtk_widget_set_hexpand(w0, TRUE);
      gtk_grid_attach(grid, w0, 2, row, 2, 1);
    }
    row++;
  }
}

/* ── Capability-Bound Visibility Helper ───────────────────────── */

static void sync_cap_visibility(const X3dGuiIpc *ipc, GParamSpec *pspec,
                                GtkWidget *widget) {
  (void)pspec;
  if (!ipc || !widget)
    return;
  const uint64_t *mask_ptr = g_object_get_data(G_OBJECT(widget), "x3d-cap-mask");
  if (!mask_ptr)
    return;
  gboolean vis = x3d_gui_ipc_has_cap(ipc, *mask_ptr);
  gtk_widget_set_visible(widget, vis);
  if (!vis && X3D_IS_STAGED_WIDGET(widget) &&
      x3d_staged_widget_get_dirty(X3D_STAGED_WIDGET(widget))) {
    x3d_staged_widget_trigger_cancel(X3D_STAGED_WIDGET(widget));
  }
}

void x3d_widget_bind_caps(GtkWidget *widget, X3dGuiIpc *ipc,
                          uint64_t cap_mask) {
  if (!widget || !ipc || cap_mask == 0)
    return;
  uint64_t *m = g_new(uint64_t, 1);
  *m = cap_mask;
  g_object_set_data_full(G_OBJECT(widget), "x3d-cap-mask", m, g_free);

  g_signal_connect_object(ipc, "notify::caps",
                          G_CALLBACK(sync_cap_visibility), widget, 0);
  g_signal_connect_object(ipc, "notify::force-unsupported",
                          G_CALLBACK(sync_cap_visibility), widget, 0);
  sync_cap_visibility(ipc, NULL, widget);
}

/* ══════════════════════════════════════════════════════════════════
 * 1. X3dCurrentWidget (.current-widget)
 * ══════════════════════════════════════════════════════════════════ */

struct _X3dCurrentWidget {
  GtkBox parent_instance;
  GtkWidget *header_box;
  GtkWidget *title_lbl;
  GtkWidget *body_box;
  gboolean clickable;
};

enum { CUR_SIG_ACTIVATED, CUR_N_SIGS };
static guint cur_sigs[CUR_N_SIGS] = {0};

G_DEFINE_TYPE(X3dCurrentWidget, x3d_current_widget, GTK_TYPE_BOX)

static void on_current_clicked(GtkGestureClick *gesture, int n_press, double x,
                               double y, gpointer user_data) {
  (void)gesture;
  (void)n_press;
  X3dCurrentWidget *self = X3D_CURRENT_WIDGET(user_data);
  if (!self->clickable)
    return;
  GtkWidget *picked = gtk_widget_pick(GTK_WIDGET(self), x, y, GTK_PICK_DEFAULT);
  while (picked && picked != GTK_WIDGET(self)) {
    if (GTK_IS_BUTTON(picked) || GTK_IS_DROP_DOWN(picked))
      return;
    picked = gtk_widget_get_parent(picked);
  }
  g_signal_emit(self, cur_sigs[CUR_SIG_ACTIVATED], 0);
}

static gboolean on_current_key_pressed(GtkEventControllerKey *ctrl,
                                       guint keyval, guint keycode,
                                       GdkModifierType state,
                                       gpointer user_data) {
  (void)ctrl;
  (void)keycode;
  (void)state;
  X3dCurrentWidget *self = X3D_CURRENT_WIDGET(user_data);
  if (!self->clickable)
    return FALSE;
  if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter ||
      keyval == GDK_KEY_space) {
    g_signal_emit(self, cur_sigs[CUR_SIG_ACTIVATED], 0);
    return TRUE;
  }
  return FALSE;
}

// cppcheck-suppress constParameterPointer
static void x3d_current_widget_class_init(X3dCurrentWidgetClass *klass) {
  cur_sigs[CUR_SIG_ACTIVATED] =
      g_signal_new("activated", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
                   NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void x3d_current_widget_init(X3dCurrentWidget *self) {
  gtk_orientable_set_orientation(GTK_ORIENTABLE(self),
                                 GTK_ORIENTATION_VERTICAL);
  gtk_box_set_spacing(GTK_BOX(self), 8);
  gtk_widget_set_hexpand(GTK_WIDGET(self), TRUE);
  gtk_widget_add_css_class(GTK_WIDGET(self), "current-widget");

  self->header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  self->title_lbl = gtk_label_new("");
  gtk_widget_add_css_class(self->title_lbl, "x3d-widget-title");
  gtk_label_set_xalign(GTK_LABEL(self->title_lbl), 0.0f);
  gtk_widget_set_hexpand(self->title_lbl, TRUE);
  gtk_box_append(GTK_BOX(self->header_box), self->title_lbl);
  gtk_box_append(GTK_BOX(self), self->header_box);

  self->body_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_hexpand(self->body_box, TRUE);
  gtk_box_append(GTK_BOX(self), self->body_box);
}

GtkWidget *x3d_current_widget_new(const char *title, gboolean clickable) {
  X3dCurrentWidget *self = g_object_new(X3D_TYPE_CURRENT_WIDGET, NULL);
  if (title && title[0]) {
    gtk_label_set_text(GTK_LABEL(self->title_lbl), title);
  } else {
    gtk_widget_set_visible(self->header_box, FALSE);
  }
  self->clickable = clickable;
  if (clickable) {
    gtk_widget_add_css_class(GTK_WIDGET(self), "clickable");
    gtk_widget_set_focusable(GTK_WIDGET(self), TRUE);
    gtk_widget_set_cursor_from_name(GTK_WIDGET(self), "pointer");

    GtkGesture *click = gtk_gesture_click_new();
    g_signal_connect(click, "released", G_CALLBACK(on_current_clicked), self);
    gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(click));

    GtkEventController *key = gtk_event_controller_key_new();
    g_signal_connect(key, "key-pressed", G_CALLBACK(on_current_key_pressed),
                     self);
    gtk_widget_add_controller(GTK_WIDGET(self), key);
  }
  return GTK_WIDGET(self);
}

GtkBox *x3d_current_widget_get_body(X3dCurrentWidget *self) {
  return self ? GTK_BOX(self->body_box) : NULL;
}

void x3d_current_widget_add_header_suffix(X3dCurrentWidget *self,
                                          GtkWidget *suffix) {
  if (!self || !suffix)
    return;
  gtk_widget_set_visible(self->header_box, TRUE);
  gtk_box_append(GTK_BOX(self->header_box), suffix);
}

/* ══════════════════════════════════════════════════════════════════
 * 2. X3dStagedWidget (.staged-widget)
 * ══════════════════════════════════════════════════════════════════ */

struct _X3dStagedWidget {
  GtkBox parent_instance;
  GtkWidget *header_box;
  GtkWidget *title_lbl;
  GtkWidget *body_box;
  GtkWidget *grid;
  GtkWidget *footer_box;
  GtkWidget *left_actions;
  GtkWidget *cta_box;
  GtkWidget *btn_default;
  GtkWidget *btn_cancel;
  GtkWidget *btn_apply;
  GtkWidget *btn_close;

  gboolean dirty;
  gboolean syncing;
  gboolean standalone;
};

enum { STG_PROP_0, STG_PROP_DIRTY, STG_PROP_STANDALONE, STG_N_PROPS };
static GParamSpec *stg_props[STG_N_PROPS] = {NULL};

enum {
  STG_SIG_DEFAULT,
  STG_SIG_CANCEL,
  STG_SIG_APPLY,
  STG_SIG_CLOSE,
  STG_N_SIGS
};
static guint stg_sigs[STG_N_SIGS] = {0};

G_DEFINE_TYPE(X3dStagedWidget, x3d_staged_widget, GTK_TYPE_BOX)

static gboolean reset_apply_btn_cb(gpointer data) {
  GtkButton *btn = GTK_BUTTON(data);
  gtk_button_set_label(btn, "Apply");
  g_object_unref(btn);
  return G_SOURCE_REMOVE;
}

static void on_stg_default_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  x3d_staged_widget_trigger_default(X3D_STAGED_WIDGET(data));
}

static void on_stg_cancel_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  x3d_staged_widget_trigger_cancel(X3D_STAGED_WIDGET(data));
}

static void on_stg_apply_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  (void)x3d_staged_widget_trigger_apply(X3D_STAGED_WIDGET(data));
}

static gboolean apply_signal_accum(GSignalInvocationHint *ihint,
                                   GValue *return_accu,
                                   const GValue *handler_return,
                                   gpointer data) {
  (void)ihint;
  (void)data;
  gboolean ok = g_value_get_boolean(handler_return);
  g_value_set_boolean(return_accu, ok);
  return ok;
}

static void on_stg_close_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  X3dStagedWidget *self = X3D_STAGED_WIDGET(data);
  x3d_staged_widget_trigger_cancel(self);
  g_signal_emit(self, stg_sigs[STG_SIG_CLOSE], 0);
}

static void x3d_staged_widget_get_property(GObject *obj, guint prop_id,
                                           GValue *val, GParamSpec *pspec) {
  const X3dStagedWidget *self = X3D_STAGED_WIDGET(obj);
  switch (prop_id) {
  case STG_PROP_DIRTY:
    g_value_set_boolean(val, self->dirty);
    break;
  case STG_PROP_STANDALONE:
    g_value_set_boolean(val, self->standalone);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
    break;
  }
}

static void x3d_staged_widget_set_property(GObject *obj, guint prop_id,
                                           const GValue *val,
                                           GParamSpec *pspec) {
  X3dStagedWidget *self = X3D_STAGED_WIDGET(obj);
  switch (prop_id) {
  case STG_PROP_DIRTY:
    x3d_staged_widget_set_dirty(self, g_value_get_boolean(val));
    break;
  case STG_PROP_STANDALONE:
    self->standalone = g_value_get_boolean(val);
    if (self->btn_close)
      gtk_widget_set_visible(self->btn_close, self->standalone);
    if (self->footer_box)
      gtk_widget_set_visible(self->footer_box,
                             self->standalone ||
                                 gtk_widget_get_visible(self->left_actions));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
    break;
  }
}

static void x3d_staged_widget_class_init(X3dStagedWidgetClass *klass) {
  GObjectClass *gcls = G_OBJECT_CLASS(klass);
  gcls->get_property = x3d_staged_widget_get_property;
  gcls->set_property = x3d_staged_widget_set_property;

  stg_props[STG_PROP_DIRTY] = g_param_spec_boolean(
      "dirty", "Dirty", "Staged widget holds unapplied edits", FALSE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
  stg_props[STG_PROP_STANDALONE] = g_param_spec_boolean(
      "standalone", "Standalone", "Show standalone Close action", FALSE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_properties(gcls, STG_N_PROPS, stg_props);

  stg_sigs[STG_SIG_DEFAULT] =
      g_signal_new("default", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
                   NULL, NULL, NULL, G_TYPE_NONE, 0);
  stg_sigs[STG_SIG_CANCEL] =
      g_signal_new("cancel", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
                   NULL, NULL, NULL, G_TYPE_NONE, 0);
  stg_sigs[STG_SIG_APPLY] = g_signal_new(
      "apply", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
      apply_signal_accum, NULL, NULL, G_TYPE_BOOLEAN, 0);
  stg_sigs[STG_SIG_CLOSE] =
      g_signal_new("close", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
                   NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void x3d_staged_widget_init(X3dStagedWidget *self) {
  gtk_orientable_set_orientation(GTK_ORIENTABLE(self),
                                 GTK_ORIENTATION_VERTICAL);
  gtk_box_set_spacing(GTK_BOX(self), 12);
  gtk_widget_set_hexpand(GTK_WIDGET(self), TRUE);
  gtk_widget_add_css_class(GTK_WIDGET(self), "staged-widget");

  self->header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  self->title_lbl = gtk_label_new("");
  gtk_widget_add_css_class(self->title_lbl, "x3d-widget-title");
  gtk_label_set_xalign(GTK_LABEL(self->title_lbl), 0.0f);
  gtk_widget_set_hexpand(self->title_lbl, TRUE);
  gtk_box_append(GTK_BOX(self->header_box), self->title_lbl);
  gtk_box_append(GTK_BOX(self), self->header_box);

  self->body_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_hexpand(self->body_box, TRUE);
  gtk_box_append(GTK_BOX(self), self->body_box);

  self->grid = x3d_centered_3col_grid_new();
  gtk_widget_set_visible(self->grid, FALSE);
  gtk_box_append(GTK_BOX(self), self->grid);

  self->footer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_margin_top(self->footer_box, 6);
  gtk_widget_set_hexpand(self->footer_box, TRUE);

  self->left_actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_halign(self->left_actions, GTK_ALIGN_START);
  gtk_widget_set_visible(self->left_actions, FALSE);
  gtk_box_append(GTK_BOX(self->footer_box), self->left_actions);

  GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand(spacer, TRUE);
  gtk_box_append(GTK_BOX(self->footer_box), spacer);

  self->cta_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_halign(self->cta_box, GTK_ALIGN_END);
  gtk_widget_add_css_class(self->cta_box, "x3d-cta-bar");

  self->btn_default = gtk_button_new_with_label("Default");
  self->btn_cancel = gtk_button_new_with_label("Cancel");
  self->btn_apply = gtk_button_new_with_label("Apply");
  self->btn_close = gtk_button_new_with_label("Close");

  gtk_widget_add_css_class(self->btn_default, "x3d-cta-btn");
  gtk_widget_add_css_class(self->btn_cancel, "x3d-cta-btn");
  gtk_widget_add_css_class(self->btn_apply, "x3d-primary");
  gtk_widget_add_css_class(self->btn_close, "x3d-cta-btn");

  gtk_widget_set_sensitive(self->btn_cancel, FALSE);
  gtk_widget_set_sensitive(self->btn_apply, FALSE);
  gtk_widget_set_visible(self->btn_default, FALSE);
  gtk_widget_set_visible(self->btn_cancel, FALSE);
  gtk_widget_set_visible(self->btn_apply, FALSE);
  gtk_widget_set_visible(self->btn_close, FALSE);
  gtk_widget_set_visible(self->footer_box, FALSE);

  g_signal_connect(self->btn_default, "clicked",
                   G_CALLBACK(on_stg_default_clicked), self);
  g_signal_connect(self->btn_cancel, "clicked",
                   G_CALLBACK(on_stg_cancel_clicked), self);
  g_signal_connect(self->btn_apply, "clicked",
                   G_CALLBACK(on_stg_apply_clicked), self);
  g_signal_connect(self->btn_close, "clicked",
                   G_CALLBACK(on_stg_close_clicked), self);

  gtk_box_append(GTK_BOX(self->cta_box), self->btn_default);
  gtk_box_append(GTK_BOX(self->cta_box), self->btn_cancel);
  gtk_box_append(GTK_BOX(self->cta_box), self->btn_apply);
  gtk_box_append(GTK_BOX(self->cta_box), self->btn_close);

  gtk_box_append(GTK_BOX(self->footer_box), self->cta_box);
  gtk_box_append(GTK_BOX(self), self->footer_box);
}

GtkWidget *x3d_staged_widget_new(const char *title, gboolean standalone) {
  X3dStagedWidget *self =
      g_object_new(X3D_TYPE_STAGED_WIDGET, "standalone", standalone, NULL);
  if (title && title[0]) {
    gtk_label_set_text(GTK_LABEL(self->title_lbl), title);
  } else {
    gtk_widget_set_visible(self->header_box, FALSE);
  }
  return GTK_WIDGET(self);
}

GtkBox *x3d_staged_widget_get_body(X3dStagedWidget *self) {
  return self ? GTK_BOX(self->body_box) : NULL;
}

void x3d_staged_widget_add_header_suffix(X3dStagedWidget *self,
                                         GtkWidget *suffix) {
  if (!self || !suffix)
    return;
  gtk_widget_set_visible(self->header_box, TRUE);
  gtk_box_append(GTK_BOX(self->header_box), suffix);
}

void x3d_staged_widget_add_left_action(X3dStagedWidget *self, GtkWidget *btn) {
  if (!self || !btn)
    return;
  gtk_widget_set_visible(self->left_actions, TRUE);
  gtk_widget_set_visible(self->footer_box, TRUE);
  gtk_box_append(GTK_BOX(self->left_actions), btn);
}

void x3d_staged_widget_set_grid_items(X3dStagedWidget *self,
                                      GtkWidget *const *items, guint count) {
  if (!self)
    return;
  x3d_grid_pack_centered_3col(GTK_GRID(self->grid), items, count);
  gtk_widget_set_visible(self->grid, count > 0);
}

static void mark_staged_dirty_cb(X3dStagedWidget *self) {
  if (!self || self->syncing)
    return;
  x3d_staged_widget_set_dirty(self, TRUE);
}

static void on_watched_notify_cb(GObject *obj, GParamSpec *pspec,
                                 gpointer data) {
  (void)obj;
  (void)pspec;
  mark_staged_dirty_cb(X3D_STAGED_WIDGET(data));
}

static void on_watched_simple_cb(GtkWidget *w, gpointer data) {
  (void)w;
  mark_staged_dirty_cb(X3D_STAGED_WIDGET(data));
}

void x3d_staged_widget_watch_control(X3dStagedWidget *self, GtkWidget *ctrl) {
  if (!self || !ctrl)
    return;
  if (GTK_IS_TOGGLE_BUTTON(ctrl)) {
    g_signal_connect_object(ctrl, "toggled", G_CALLBACK(on_watched_simple_cb),
                            self, 0);
  } else if (GTK_IS_DROP_DOWN(ctrl)) {
    g_signal_connect_object(ctrl, "notify::selected",
                            G_CALLBACK(on_watched_notify_cb), self, 0);
  } else if (GTK_IS_SPIN_BUTTON(ctrl)) {
    g_signal_connect_object(ctrl, "value-changed",
                            G_CALLBACK(on_watched_simple_cb), self, 0);
  } else if (GTK_IS_RANGE(ctrl)) {
    g_signal_connect_object(ctrl, "value-changed",
                            G_CALLBACK(on_watched_simple_cb), self, 0);
    const SliderEntryPair *pair =
        g_object_get_data(G_OBJECT(ctrl), "x3d-slider-pair");
    if (pair && pair->entry) {
      g_signal_connect_object(pair->entry, "changed",
                              G_CALLBACK(on_watched_simple_cb), self, 0);
    }
  } else if (GTK_IS_EDITABLE(ctrl)) {
    g_signal_connect_object(ctrl, "changed", G_CALLBACK(on_watched_simple_cb),
                            self, 0);
  } else if (GTK_IS_SWITCH(ctrl)) {
    g_signal_connect_object(ctrl, "notify::active",
                            G_CALLBACK(on_watched_notify_cb), self, 0);
  }
}

void x3d_staged_widget_set_syncing(X3dStagedWidget *self, gboolean syncing) {
  if (self)
    self->syncing = syncing;
}

gboolean x3d_staged_widget_get_syncing(const X3dStagedWidget *self) {
  return self ? self->syncing : FALSE;
}

gboolean x3d_staged_widget_get_dirty(const X3dStagedWidget *self) {
  return self ? self->dirty : FALSE;
}

void x3d_staged_widget_set_dirty(X3dStagedWidget *self, gboolean dirty) {
  if (!self || self->dirty == dirty)
    return;
  self->dirty = dirty;
  if (dirty)
    gtk_widget_add_css_class(GTK_WIDGET(self), "is-dirty");
  else
    gtk_widget_remove_css_class(GTK_WIDGET(self), "is-dirty");
  gtk_widget_set_sensitive(self->btn_cancel, dirty);
  gtk_widget_set_sensitive(self->btn_apply, dirty);
  g_object_notify_by_pspec(G_OBJECT(self), stg_props[STG_PROP_DIRTY]);
}

gboolean x3d_staged_widget_trigger_apply(X3dStagedWidget *self) {
  if (!self)
    return FALSE;
  gboolean ok = TRUE;
  if (g_signal_has_handler_pending(self, stg_sigs[STG_SIG_APPLY], 0, FALSE)) {
    ok = FALSE;
    g_signal_emit(self, stg_sigs[STG_SIG_APPLY], 0, &ok);
  }
  if (!ok) {
    gtk_button_set_label(GTK_BUTTON(self->btn_apply), "Failed");
    g_timeout_add(1500, reset_apply_btn_cb, g_object_ref(self->btn_apply));
    return FALSE;
  }
  x3d_staged_widget_set_dirty(self, FALSE);
  gtk_button_set_label(GTK_BUTTON(self->btn_apply), "Applied!");
  g_timeout_add(1200, reset_apply_btn_cb, g_object_ref(self->btn_apply));
  return TRUE;
}

void x3d_staged_widget_trigger_cancel(X3dStagedWidget *self) {
  if (!self)
    return;
  self->syncing = TRUE;
  g_signal_emit(self, stg_sigs[STG_SIG_CANCEL], 0);
  self->syncing = FALSE;
  x3d_staged_widget_set_dirty(self, FALSE);
}

void x3d_staged_widget_trigger_default(X3dStagedWidget *self) {
  if (!self)
    return;
  self->syncing = TRUE;
  g_signal_emit(self, stg_sigs[STG_SIG_DEFAULT], 0);
  self->syncing = FALSE;
  x3d_staged_widget_set_dirty(self, TRUE);
}

/* ══════════════════════════════════════════════════════════════════
 * 3. X3dImmediateWidget (.immediate-widget)
 * ══════════════════════════════════════════════════════════════════ */

struct _X3dImmediateWidget {
  GtkBox parent_instance;
  GtkWidget *title_lbl;
  GtkWidget *body_box;
  GtkWidget *grid;
};

G_DEFINE_TYPE(X3dImmediateWidget, x3d_immediate_widget, GTK_TYPE_BOX)

static void x3d_immediate_widget_class_init(X3dImmediateWidgetClass *klass) {
  (void)klass;
}

static void x3d_immediate_widget_init(X3dImmediateWidget *self) {
  gtk_orientable_set_orientation(GTK_ORIENTABLE(self),
                                 GTK_ORIENTATION_VERTICAL);
  gtk_box_set_spacing(GTK_BOX(self), 10);
  gtk_widget_set_hexpand(GTK_WIDGET(self), TRUE);
  gtk_widget_add_css_class(GTK_WIDGET(self), "immediate-widget");

  self->title_lbl = gtk_label_new("");
  gtk_widget_add_css_class(self->title_lbl, "x3d-widget-title");
  gtk_label_set_xalign(GTK_LABEL(self->title_lbl), 0.0f);
  gtk_box_append(GTK_BOX(self), self->title_lbl);

  self->body_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_hexpand(self->body_box, TRUE);
  gtk_box_append(GTK_BOX(self), self->body_box);

  self->grid = x3d_centered_3col_grid_new();
  gtk_widget_set_visible(self->grid, FALSE);
  gtk_box_append(GTK_BOX(self), self->grid);
}

GtkWidget *x3d_immediate_widget_new(const char *title) {
  X3dImmediateWidget *self = g_object_new(X3D_TYPE_IMMEDIATE_WIDGET, NULL);
  if (title && title[0]) {
    gtk_label_set_text(GTK_LABEL(self->title_lbl), title);
  } else {
    gtk_widget_set_visible(self->title_lbl, FALSE);
  }
  return GTK_WIDGET(self);
}

GtkBox *x3d_immediate_widget_get_body(X3dImmediateWidget *self) {
  return self ? GTK_BOX(self->body_box) : NULL;
}

void x3d_immediate_widget_set_grid_items(X3dImmediateWidget *self,
                                         GtkWidget *const *items, guint count) {
  if (!self)
    return;
  x3d_grid_pack_centered_3col(GTK_GRID(self->grid), items, count);
  gtk_widget_set_visible(self->grid, count > 0);
}

/* ══════════════════════════════════════════════════════════════════
 * 4. X3dDangerWidget (.danger-widget) — Uses AdwAlertDialog
 * ══════════════════════════════════════════════════════════════════ */

struct _X3dDangerWidget {
  GtkBox parent_instance;
  GtkWidget *title_lbl;
  GtkWidget *desc_lbl;
  GtkWidget *action_btn;
  const char *confirm_heading;
  const char *confirm_body;
};

enum { DNG_SIG_CONFIRMED, DNG_N_SIGS };
static guint dng_sigs[DNG_N_SIGS] = {0};

G_DEFINE_TYPE(X3dDangerWidget, x3d_danger_widget, GTK_TYPE_BOX)

static void on_danger_alert_resp(AdwAlertDialog *dlg, const char *response,
                                 gpointer data) {
  (void)dlg;
  X3dDangerWidget *self = X3D_DANGER_WIDGET(data);
  if (g_strcmp0(response, "confirm") == 0) {
    g_signal_emit(self, dng_sigs[DNG_SIG_CONFIRMED], 0);
  }
}

static void on_danger_btn_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  X3dDangerWidget *self = X3D_DANGER_WIDGET(data);
  AdwDialog *dlg = adw_alert_dialog_new(
      self->confirm_heading ? self->confirm_heading : "Confirm Override?",
      self->confirm_body ? self->confirm_body
                         : "This action resets all states and cannot be undone.");
  adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dlg), "cancel", "Cancel",
                                 "confirm", "Execute Override", NULL);
  adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dlg), "confirm",
                                           ADW_RESPONSE_DESTRUCTIVE);
  adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dlg), "cancel");
  adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dlg), "cancel");
  g_signal_connect(dlg, "response", G_CALLBACK(on_danger_alert_resp), self);

  GtkRoot *root = gtk_widget_get_root(GTK_WIDGET(self));
  adw_dialog_present(dlg, root ? GTK_WIDGET(root) : GTK_WIDGET(self));
}

// cppcheck-suppress constParameterPointer
static void x3d_danger_widget_class_init(X3dDangerWidgetClass *klass) {
  dng_sigs[DNG_SIG_CONFIRMED] =
      g_signal_new("confirmed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
                   NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void x3d_danger_widget_init(X3dDangerWidget *self) {
  gtk_orientable_set_orientation(GTK_ORIENTABLE(self),
                                 GTK_ORIENTATION_VERTICAL);
  gtk_box_set_spacing(GTK_BOX(self), 10);
  gtk_widget_set_hexpand(GTK_WIDGET(self), TRUE);
  gtk_widget_add_css_class(GTK_WIDGET(self), "danger-widget");

  self->title_lbl = gtk_label_new("EMERGENCY OVERRIDE");
  gtk_widget_add_css_class(self->title_lbl, "x3d-widget-title");
  gtk_label_set_xalign(GTK_LABEL(self->title_lbl), 0.0f);
  gtk_box_append(GTK_BOX(self), self->title_lbl);

  self->desc_lbl = gtk_label_new("");
  gtk_label_set_wrap(GTK_LABEL(self->desc_lbl), TRUE);
  gtk_label_set_xalign(GTK_LABEL(self->desc_lbl), 0.0f);
  gtk_widget_add_css_class(self->desc_lbl, "x3d-gpu-subtitle");
  gtk_box_append(GTK_BOX(self), self->desc_lbl);

  GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_halign(btn_row, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top(btn_row, 4);
  self->action_btn = gtk_button_new_with_label("EMERGENCY OVERRIDE");
  gtk_widget_add_css_class(self->action_btn, "x3d-destructive");
  g_signal_connect(self->action_btn, "clicked",
                   G_CALLBACK(on_danger_btn_clicked), self);
  gtk_box_append(GTK_BOX(btn_row), self->action_btn);
  gtk_box_append(GTK_BOX(self), btn_row);
}

GtkWidget *x3d_danger_widget_new(const char *title, const char *description,
                                 const char *button_label,
                                 const char *confirm_heading,
                                 const char *confirm_body) {
  X3dDangerWidget *self = g_object_new(X3D_TYPE_DANGER_WIDGET, NULL);
  if (title)
    gtk_label_set_text(GTK_LABEL(self->title_lbl), title);
  if (description)
    gtk_label_set_text(GTK_LABEL(self->desc_lbl), description);
  if (button_label)
    gtk_button_set_label(GTK_BUTTON(self->action_btn), button_label);
  self->confirm_heading = confirm_heading;
  self->confirm_body = confirm_body;
  return GTK_WIDGET(self);
}

/* ── Control Primitive Factories ──────────────────────────────── */

void x3d_widgets_register_types(void) {
  g_type_ensure(X3D_TYPE_CURRENT_WIDGET);
  g_type_ensure(X3D_TYPE_STAGED_WIDGET);
  g_type_ensure(X3D_TYPE_IMMEDIATE_WIDGET);
  g_type_ensure(X3D_TYPE_DANGER_WIDGET);
}

GtkWidget *x3d_toggle_new(const char *label) {
  GtkWidget *btn = gtk_toggle_button_new_with_label(label);
  gtk_widget_add_css_class(btn, "x3d-toggle-pill");
  GtkWidget *child = gtk_button_get_child(GTK_BUTTON(btn));
  if (GTK_IS_LABEL(child)) {
    gtk_label_set_wrap(GTK_LABEL(child), TRUE);
    gtk_label_set_justify(GTK_LABEL(child), GTK_JUSTIFY_CENTER);
  }
  return btn;
}

GtkWidget *x3d_popup_btn_new(const char *label) {
  GtkWidget *btn = gtk_button_new_with_label(label);
  gtk_widget_add_css_class(btn, "x3d-popup-btn");
  GtkWidget *child = gtk_button_get_child(GTK_BUTTON(btn));
  if (GTK_IS_LABEL(child)) {
    gtk_label_set_wrap(GTK_LABEL(child), TRUE);
    gtk_label_set_justify(GTK_LABEL(child), GTK_JUSTIFY_CENTER);
  }
  return btn;
}

static int slider_clamp_snap(GtkRange *sc, int raw) {
  GtkAdjustment *adj = gtk_range_get_adjustment(sc);
  int lo = (int)llround(gtk_adjustment_get_lower(adj));
  int hi = (int)llround(gtk_adjustment_get_upper(adj));
  int step = MAX(1, (int)llround(gtk_adjustment_get_step_increment(adj)));
  int val = CLAMP(raw, lo, hi);
  if (step > 1)
    val = CLAMP(lo + ((val - lo + step / 2) / step) * step, lo, hi);
  return val;
}

void x3d_slider_entry_set_bounds(GtkRange *range, double lo, double hi,
                                 double step) {
  if (!range || hi <= lo)
    return;
  GtkAdjustment *adj = gtk_range_get_adjustment(range);
  gtk_adjustment_set_lower(adj, lo);
  gtk_adjustment_set_upper(adj, hi);
  gtk_adjustment_set_step_increment(adj, step > 0.0 ? step : 1.0);
}

void x3d_slider_entry_set_value(GtkRange *range, double val) {
  if (!range)
    return;
  SliderEntryPair *p = g_object_get_data(G_OBJECT(range), "x3d-slider-pair");
  int clean = slider_clamp_snap(range, (int)llround(val));
  if (!p) {
    gtk_range_set_value(range, (double)clean);
    return;
  }
  p->syncing = TRUE;
  gtk_range_set_value(range, (double)clean);
  char buf[32];
  snprintf(buf, sizeof(buf), "%d", clean);
  gtk_editable_set_text(p->entry, buf);
  p->syncing = FALSE;
}

int x3d_slider_entry_get_int(GtkRange *range) {
  if (!range)
    return 0;
  SliderEntryPair *p = g_object_get_data(G_OBJECT(range), "x3d-slider-pair");
  int fallback = (int)llround(gtk_range_get_value(range));
  if (!p || !p->entry)
    return slider_clamp_snap(range, fallback);
  const char *txt = gtk_editable_get_text(p->entry);
  char *end = NULL;
  long parsed = (txt && txt[0]) ? strtol(txt, &end, 10) : (long)fallback;
  if (end == txt)
    parsed = (long)fallback;
  int clean = slider_clamp_snap(range, (int)parsed);
  x3d_slider_entry_set_value(range, (double)clean);
  return clean;
}

static void on_slider_range_changed(GtkRange *sc, gpointer data) {
  SliderEntryPair *p = (SliderEntryPair *)data;
  if (!p || p->syncing)
    return;
  int clean = slider_clamp_snap(sc, (int)llround(gtk_range_get_value(sc)));
  p->syncing = TRUE;
  if ((int)llround(gtk_range_get_value(sc)) != clean)
    gtk_range_set_value(sc, (double)clean);
  char buf[32];
  snprintf(buf, sizeof(buf), "%d", clean);
  gtk_editable_set_text(p->entry, buf);
  p->syncing = FALSE;
}

static void on_slider_entry_activate(GtkEntry *ent, gpointer data) {
  (void)ent;
  const SliderEntryPair *p = (const SliderEntryPair *)data;
  if (p && !p->syncing)
    (void)x3d_slider_entry_get_int(p->range);
}

static void on_slider_entry_focus_leave(GtkEventControllerFocus *f,
                                        gpointer data) {
  (void)f;
  const SliderEntryPair *p = (const SliderEntryPair *)data;
  if (p && !p->syncing)
    (void)x3d_slider_entry_get_int(p->range);
}

GtkWidget *x3d_slider_entry_new(double lo, double hi, double step, double init,
                                const char *unit, GtkRange **range_out) {
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

  GtkWidget *sc = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, lo, hi,
                                           step > 0.0 ? step : 1.0);
  gtk_scale_set_draw_value(GTK_SCALE(sc), FALSE);
  gtk_widget_set_size_request(sc, 150, -1);

  GtkWidget *ent = gtk_entry_new();
  gtk_editable_set_width_chars(GTK_EDITABLE(ent), 6);
  gtk_editable_set_max_width_chars(GTK_EDITABLE(ent), 6);
  gtk_entry_set_alignment(GTK_ENTRY(ent), 1.0f);
  gtk_entry_set_input_purpose(GTK_ENTRY(ent), GTK_INPUT_PURPOSE_DIGITS);

  SliderEntryPair *p = g_new0(SliderEntryPair, 1);
  p->range = GTK_RANGE(sc);
  p->entry = GTK_EDITABLE(ent);
  g_object_set_data_full(G_OBJECT(sc), "x3d-slider-pair", p, g_free);

  g_signal_connect(sc, "value-changed", G_CALLBACK(on_slider_range_changed), p);
  g_signal_connect(ent, "activate", G_CALLBACK(on_slider_entry_activate), p);

  GtkEventController *focus = gtk_event_controller_focus_new();
  g_signal_connect(focus, "leave", G_CALLBACK(on_slider_entry_focus_leave), p);
  gtk_widget_add_controller(ent, focus);

  x3d_slider_entry_set_value(GTK_RANGE(sc), init);

  gtk_box_append(GTK_BOX(box), sc);
  gtk_box_append(GTK_BOX(box), ent);
  if (unit && unit[0]) {
    GtkWidget *ulbl = gtk_label_new(unit);
    gtk_widget_add_css_class(ulbl, "x3d-gpu-subtitle");
    gtk_label_set_xalign(GTK_LABEL(ulbl), 0.0f);
    gtk_label_set_width_chars(GTK_LABEL(ulbl), 3);
    gtk_box_append(GTK_BOX(box), ulbl);
  }

  if (range_out)
    *range_out = GTK_RANGE(sc);
  return box;
}

GtkWidget *x3d_form_row_new(const char *title, const char *subtitle,
                            GtkWidget *control) {
  GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_add_css_class(row, "x3d-form-row");
  gtk_widget_set_hexpand(row, TRUE);

  GtkWidget *text_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_hexpand(text_col, TRUE);
  gtk_widget_set_valign(text_col, GTK_ALIGN_CENTER);

  GtkWidget *lbl_t = gtk_label_new(title);
  gtk_label_set_xalign(GTK_LABEL(lbl_t), 0.0f);
  gtk_label_set_wrap(GTK_LABEL(lbl_t), TRUE);
  gtk_box_append(GTK_BOX(text_col), lbl_t);

  if (subtitle && subtitle[0]) {
    GtkWidget *lbl_s = gtk_label_new(subtitle);
    gtk_label_set_xalign(GTK_LABEL(lbl_s), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(lbl_s), TRUE);
    gtk_widget_add_css_class(lbl_s, "x3d-gpu-subtitle");
    gtk_box_append(GTK_BOX(text_col), lbl_s);
  }

  gtk_box_append(GTK_BOX(row), text_col);
  if (control) {
    gtk_widget_set_valign(control, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(row), control);
  }
  return row;
}

static void sync_live_toggle_lbl(GtkToggleButton *tb, gpointer data) {
  (void)data;
  gtk_button_set_label(GTK_BUTTON(tb),
                       gtk_toggle_button_get_active(tb) ? "ON" : "OFF");
}

GtkWidget *x3d_live_toggle_row_new(const char *title, const char *subtitle,
                                   GtkWidget **toggle_out) {
  GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_add_css_class(row, "x3d-live-row");
  gtk_widget_set_hexpand(row, TRUE);

  GtkWidget *text_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_hexpand(text_col, TRUE);
  gtk_widget_set_valign(text_col, GTK_ALIGN_CENTER);

  GtkWidget *lbl_t = gtk_label_new(title);
  gtk_label_set_xalign(GTK_LABEL(lbl_t), 0.0f);
  gtk_widget_add_css_class(lbl_t, "x3d-accent");
  gtk_box_append(GTK_BOX(text_col), lbl_t);

  if (subtitle && subtitle[0]) {
    GtkWidget *lbl_s = gtk_label_new(subtitle);
    gtk_label_set_xalign(GTK_LABEL(lbl_s), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(lbl_s), TRUE);
    gtk_widget_add_css_class(lbl_s, "x3d-gpu-subtitle");
    gtk_box_append(GTK_BOX(text_col), lbl_s);
  }

  GtkWidget *tb = x3d_toggle_new("OFF");
  gtk_widget_set_size_request(tb, 76, -1);
  g_signal_connect(tb, "toggled", G_CALLBACK(sync_live_toggle_lbl), NULL);
  sync_live_toggle_lbl(GTK_TOGGLE_BUTTON(tb), NULL);
  gtk_widget_set_valign(tb, GTK_ALIGN_CENTER);

  gtk_box_append(GTK_BOX(row), text_col);
  gtk_box_append(GTK_BOX(row), tb);

  if (toggle_out)
    *toggle_out = tb;
  return row;
}

GtkWidget *x3d_subsection_label_new(const char *title) {
  GtkWidget *lbl = gtk_label_new(title);
  gtk_widget_add_css_class(lbl, "x3d-subsection-title");
  gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
  return lbl;
}

GtkWidget *x3d_page_header_new(const char *title) {
  GtkWidget *lbl = gtk_label_new(title);
  gtk_widget_add_css_class(lbl, "x3d-page-title");
  gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
  return lbl;
}

GtkBox *x3d_page_vbox_new(GtkStack *stack, const char *name,
                          const char *title) {
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
  gtk_widget_set_hexpand(box, TRUE);
  if (title)
    gtk_box_append(GTK_BOX(box), x3d_page_header_new(title));
  gtk_stack_add_named(stack, box, name);
  return GTK_BOX(box);
}

/* ── Recursive StagedWidget Tree Introspection ────────────────── */

gboolean x3d_widgets_tree_has_staged(GtkWidget *root) {
  if (!root || !gtk_widget_get_visible(root))
    return FALSE;
  if (X3D_IS_STAGED_WIDGET(root))
    return TRUE;
  for (GtkWidget *c = gtk_widget_get_first_child(root); c != NULL;
       c = gtk_widget_get_next_sibling(c)) {
    if (x3d_widgets_tree_has_staged(c))
      return TRUE;
  }
  return FALSE;
}

gboolean x3d_widgets_tree_has_dirty(GtkWidget *root) {
  if (!root || !gtk_widget_get_visible(root))
    return FALSE;
  if (X3D_IS_STAGED_WIDGET(root))
    return x3d_staged_widget_get_dirty(X3D_STAGED_WIDGET(root));
  for (GtkWidget *c = gtk_widget_get_first_child(root); c != NULL;
       c = gtk_widget_get_next_sibling(c)) {
    if (x3d_widgets_tree_has_dirty(c))
      return TRUE;
  }
  return FALSE;
}

gboolean x3d_widgets_tree_apply_dirty(GtkWidget *root) {
  if (!root || !gtk_widget_get_visible(root))
    return TRUE;
  if (X3D_IS_STAGED_WIDGET(root)) {
    if (x3d_staged_widget_get_dirty(X3D_STAGED_WIDGET(root)))
      return x3d_staged_widget_trigger_apply(X3D_STAGED_WIDGET(root));
    return TRUE;
  }
  gboolean all_ok = TRUE;
  for (GtkWidget *c = gtk_widget_get_first_child(root); c != NULL;
       c = gtk_widget_get_next_sibling(c)) {
    if (!x3d_widgets_tree_apply_dirty(c))
      all_ok = FALSE;
  }
  return all_ok;
}

void x3d_widgets_tree_cancel_dirty(GtkWidget *root) {
  if (!root || !gtk_widget_get_visible(root))
    return;
  if (X3D_IS_STAGED_WIDGET(root)) {
    if (x3d_staged_widget_get_dirty(X3D_STAGED_WIDGET(root)))
      x3d_staged_widget_trigger_cancel(X3D_STAGED_WIDGET(root));
    return;
  }
  for (GtkWidget *c = gtk_widget_get_first_child(root); c != NULL;
       c = gtk_widget_get_next_sibling(c)) {
    x3d_widgets_tree_cancel_dirty(c);
  }
}

void x3d_widgets_tree_default_all(GtkWidget *root) {
  if (!root || !gtk_widget_get_visible(root))
    return;
  if (X3D_IS_STAGED_WIDGET(root)) {
    x3d_staged_widget_trigger_default(X3D_STAGED_WIDGET(root));
    return;
  }
  for (GtkWidget *c = gtk_widget_get_first_child(root); c != NULL;
       c = gtk_widget_get_next_sibling(c)) {
    x3d_widgets_tree_default_all(c);
  }
}

void x3d_widgets_tree_connect_dirty_notify(GtkWidget *root, GCallback cb,
                                           gpointer user_data) {
  if (!root || !cb)
    return;
  if (X3D_IS_STAGED_WIDGET(root)) {
    g_signal_connect(root, "notify::dirty", cb, user_data);
    return;
  }
  for (GtkWidget *c = gtk_widget_get_first_child(root); c != NULL;
       c = gtk_widget_get_next_sibling(c)) {
    x3d_widgets_tree_connect_dirty_notify(c, cb, user_data);
  }
}

/* ── Desktop Widgets & Status Notifier Item Slice (Mounted in Extras) ── */

static struct {
  X3dPageCtx ctx;
  X3dStagedWidget *stg_desktop_widgets;
  GtkToggleButton *tb_widget_gtk_pin;
  GtkToggleButton *tb_widget_kde_plasmoid;
  GtkToggleButton *tb_kde_sni_enable;
} g_dw;

static void sync_kde_sni_toggle_lbl(GtkToggleButton *tb, gpointer data) {
  (void)data;
  gtk_button_set_label(GTK_BUTTON(tb),
                       gtk_toggle_button_get_active(tb)
                           ? "KDE6 SNI: Enabled"
                           : "KDE6 SNI: Disabled");
}

static void on_dw_default(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  x3d_stub_desktop_widgets_reset_defaults();
  x3d_stub_kde_sni_reset_defaults();
  gtk_toggle_button_set_active(g_dw.tb_widget_gtk_pin, FALSE);
  gtk_toggle_button_set_active(g_dw.tb_widget_kde_plasmoid, FALSE);
  gtk_toggle_button_set_active(g_dw.tb_kde_sni_enable, FALSE);
}

static void on_dw_cancel(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  gui_widgets_refresh_desktop_section();
}

static gboolean on_dw_apply(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  X3dStubState *st = x3d_stubs_get();
  st->widget_gtk_pinned = gtk_toggle_button_get_active(g_dw.tb_widget_gtk_pin);
  st->widget_kde_plasmoid =
      gtk_toggle_button_get_active(g_dw.tb_widget_kde_plasmoid);
  st->kde_sni_enabled = gtk_toggle_button_get_active(g_dw.tb_kde_sni_enable);
  x3d_stub_desktop_widgets_apply(g_dw.ctx.view->main_window, g_dw.ctx.ipc);
  x3d_stub_kde_sni_apply();
  return TRUE;
}

static void on_dw_config_clicked(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  gui_popups_show_desktop_widgets(g_dw.ctx.view->main_window, g_dw.ctx.ipc,
                                  g_dw.stg_desktop_widgets);
}

static void on_sni_style_clicked(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  gui_popups_show_sni_style(g_dw.ctx.view->main_window,
                            g_dw.stg_desktop_widgets);
}

void gui_widgets_refresh_desktop_section(void) {
  if (!g_dw.stg_desktop_widgets)
    return;
  const X3dStubState *st = x3d_stubs_get();
  x3d_staged_widget_set_syncing(g_dw.stg_desktop_widgets, TRUE);
  gtk_toggle_button_set_active(g_dw.tb_widget_gtk_pin, st->widget_gtk_pinned);
  gtk_toggle_button_set_active(g_dw.tb_widget_kde_plasmoid,
                               st->widget_kde_plasmoid);
  gtk_toggle_button_set_active(g_dw.tb_kde_sni_enable, st->kde_sni_enabled);
  x3d_staged_widget_set_syncing(g_dw.stg_desktop_widgets, FALSE);
}

GtkWidget *gui_widgets_build_desktop_section(const X3dPageCtx *ctx) {
  if (ctx)
    g_dw.ctx = *ctx;

  GtkWidget *stg_dw = x3d_staged_widget_new(
      "DESKTOP WIDGETS & STATUS NOTIFIER ITEM", FALSE);
  g_dw.stg_desktop_widgets = X3D_STAGED_WIDGET(stg_dw);

  GtkWidget *tb_gpin = x3d_toggle_new("Pin to Desktop (GTK4 Window)");
  GtkWidget *tb_plas = x3d_toggle_new("KDE6 Plasma Widget (Plasmoid)");
  GtkWidget *tb_sni = x3d_toggle_new("KDE6 SNI: Disabled");
  g_signal_connect(tb_sni, "toggled", G_CALLBACK(sync_kde_sni_toggle_lbl),
                   NULL);

  g_dw.tb_widget_gtk_pin = GTK_TOGGLE_BUTTON(tb_gpin);
  g_dw.tb_widget_kde_plasmoid = GTK_TOGGLE_BUTTON(tb_plas);
  g_dw.tb_kde_sni_enable = GTK_TOGGLE_BUTTON(tb_sni);

  GtkWidget *btn_cfg = x3d_popup_btn_new("Configure Widgets");
  GtkWidget *btn_sni = x3d_popup_btn_new("SNI Style");

  GtkWidget *dw_items[] = {tb_gpin, tb_plas, tb_sni, btn_cfg, btn_sni};
  x3d_staged_widget_set_grid_items(g_dw.stg_desktop_widgets, dw_items, 5);
  x3d_staged_widget_watch_control(g_dw.stg_desktop_widgets, tb_gpin);
  x3d_staged_widget_watch_control(g_dw.stg_desktop_widgets, tb_plas);
  x3d_staged_widget_watch_control(g_dw.stg_desktop_widgets, tb_sni);

  g_signal_connect(g_dw.stg_desktop_widgets, "default",
                   G_CALLBACK(on_dw_default), NULL);
  g_signal_connect(g_dw.stg_desktop_widgets, "cancel", G_CALLBACK(on_dw_cancel),
                   NULL);
  g_signal_connect(g_dw.stg_desktop_widgets, "apply", G_CALLBACK(on_dw_apply),
                   NULL);
  g_signal_connect(btn_cfg, "clicked", G_CALLBACK(on_dw_config_clicked), NULL);
  g_signal_connect(btn_sni, "clicked", G_CALLBACK(on_sni_style_clicked), NULL);

  return stg_dw;
}

static GtkWidget *mod_widgets_build(const X3dPageCtx *ctx) {
  (void)ctx;
  x3d_widgets_register_types();
  return NULL;
}

static void mod_widgets_refresh(const X3dPageCtx *ctx) {
  (void)ctx;
  gui_widgets_refresh_desktop_section();
}

static gboolean mod_widgets_has_dirty(void) {
  return g_dw.stg_desktop_widgets
             ? x3d_staged_widget_get_dirty(g_dw.stg_desktop_widgets)
             : FALSE;
}

static gboolean mod_widgets_has_staged(void) {
  return g_dw.stg_desktop_widgets != NULL;
}

static gboolean mod_widgets_apply_all(void) {
  if (g_dw.stg_desktop_widgets &&
      x3d_staged_widget_get_dirty(g_dw.stg_desktop_widgets)) {
    return x3d_staged_widget_trigger_apply(g_dw.stg_desktop_widgets);
  }
  return TRUE;
}

static void mod_widgets_cancel_all(void) {
  if (g_dw.stg_desktop_widgets &&
      x3d_staged_widget_get_dirty(g_dw.stg_desktop_widgets)) {
    x3d_staged_widget_trigger_cancel(g_dw.stg_desktop_widgets);
  }
}

static void mod_widgets_default_all(void) {
  if (g_dw.stg_desktop_widgets)
    x3d_staged_widget_trigger_default(g_dw.stg_desktop_widgets);
}

const X3dPageModule *gui_widgets_get_module(void) {
  static const X3dPageModule k_mod = {
      .id = NULL,
      .title = "Widgets",
      .is_subnav = FALSE,
      .initially_hidden = FALSE,
      .divider_after = FALSE,
      .build = mod_widgets_build,
      .refresh = mod_widgets_refresh,
      .activate = NULL,
      .deactivate = NULL,
      .has_dirty = mod_widgets_has_dirty,
      .has_staged = mod_widgets_has_staged,
      .apply_all = mod_widgets_apply_all,
      .cancel_all = mod_widgets_cancel_all,
      .default_all = mod_widgets_default_all,
      .on_interval_changed = NULL,
      .highlight_hw_mode = NULL,
  };
  return &k_mod;
}
