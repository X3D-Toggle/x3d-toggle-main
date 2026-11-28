/* Profile Manager Page Slice Implementation
 * `pages/gui_profile.c`
 */

#include "gui_profile.h"
#include "../gui_popups.h"
#include "../gui_stubs.h"
#include "gui_widgets.h"
#include <string.h>

extern int printf_sn(char *buf, size_t size, const char *fmt, ...);

static struct {
  X3dPageCtx ctx;
  GtkWidget *root;

  X3dStagedWidget *stg_proc_profile;
  GtkEditable *ent_proc_name;
  GtkDropDown *dd_proc_policy;
  GtkListBox *lb_proc_rules;

  X3dStagedWidget *stg_games_profile;
  GtkEditable *ent_game_name;
  GtkListBox *lb_games_usr;
  GtkWidget *lbl_games_sys_summary;
} g_prf;

static void clear_listbox(GtkListBox *lb) {
  GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(lb));
  while (child) {
    GtkWidget *next = gtk_widget_get_next_sibling(child);
    gtk_list_box_remove(lb, child);
    child = next;
  }
}

static void load_games_ipc(GPtrArray *sys_games, GPtrArray *usr_games) {
  g_ptr_array_set_size(sys_games, 0);
  g_ptr_array_set_size(usr_games, 0);

  char resp[4096] = {0};
  if (x3d_ipc_send("GAMES_LIST", resp, sizeof(resp)) != 0 || resp[0] == '\0' ||
      strcmp(resp, "ERR") == 0 || strcmp(resp, "EMPTY") == 0) {
    return;
  }

  gchar **lines = g_strsplit(resp, "\n", -1);
  for (int i = 0; lines[i]; i++) {
    const gchar *ln = g_strstrip(lines[i]);
    if (ln[0] == 'S' && ln[1] == ':' && ln[2] != '\0')
      g_ptr_array_add(sys_games, g_strdup(ln + 2));
    else if (ln[0] == 'U' && ln[1] == ':' && ln[2] != '\0')
      g_ptr_array_add(usr_games, g_strdup(ln + 2));
  }
  g_strfreev(lines);
}

static gboolean add_game_ipc(const char *name) {
  if (!name || !name[0])
    return FALSE;
  char cmd[256];
  printf_sn(cmd, sizeof(cmd), "GAME_ADD %s", name);
  return x3d_ipc_cmd_ok(cmd);
}

static gboolean remove_game_ipc(const char *name) {
  if (!name || !name[0])
    return FALSE;
  char cmd[256];
  printf_sn(cmd, sizeof(cmd), "GAME_REMOVE %s", name);
  return x3d_ipc_cmd_ok(cmd);
}

static void mod_profile_refresh(const X3dPageCtx *ctx) {
  if (ctx)
    g_prf.ctx = *ctx;

  clear_listbox(g_prf.lb_proc_rules);
  GPtrArray *pr = x3d_stubs_get()->process_rules;
  for (guint i = 0; pr && i < pr->len; i++) {
    GtkWidget *lbl = gtk_label_new(g_ptr_array_index(pr, i));
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_widget_set_margin_start(lbl, 10);
    gtk_widget_set_margin_top(lbl, 6);
    gtk_widget_set_margin_bottom(lbl, 6);
    gtk_list_box_append(g_prf.lb_proc_rules, lbl);
  }

  GPtrArray *sys_g = g_ptr_array_new_with_free_func(g_free);
  GPtrArray *usr_g = g_ptr_array_new_with_free_func(g_free);
  load_games_ipc(sys_g, usr_g);

  gchar *sum = g_strdup_printf(
      "System Curated Triggers ([GAMES_SYS]): %u active entries  •  User "
      "Triggers ([GAMES_USR]): %u",
      sys_g->len, usr_g->len);
  gtk_label_set_text(GTK_LABEL(g_prf.lbl_games_sys_summary), sum);
  g_free(sum);

  clear_listbox(g_prf.lb_games_usr);
  for (guint i = 0; i < usr_g->len; i++) {
    const char *nm = g_ptr_array_index(usr_g, i);
    GtkWidget *lbl = gtk_label_new(nm);
    gtk_widget_set_name(lbl, nm);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_widget_set_margin_start(lbl, 10);
    gtk_widget_set_margin_top(lbl, 6);
    gtk_widget_set_margin_bottom(lbl, 6);
    gtk_list_box_append(g_prf.lb_games_usr, lbl);
  }

  g_ptr_array_unref(sys_g);
  g_ptr_array_unref(usr_g);
}

static void on_proc_add_clicked(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  const char *nm = gtk_editable_get_text(g_prf.ent_proc_name);
  GtkStringObject *so =
      GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(g_prf.dd_proc_policy));
  if (nm && nm[0]) {
    x3d_stub_process_rule_add(nm, so ? gtk_string_object_get_string(so) : NULL);
    gtk_editable_set_text(g_prf.ent_proc_name, "");
    mod_profile_refresh(NULL);
    x3d_staged_widget_set_dirty(g_prf.stg_proc_profile, TRUE);
  }
}

static void on_proc_rem_clicked(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  GtkListBoxRow *r = gtk_list_box_get_selected_row(g_prf.lb_proc_rules);
  if (r) {
    x3d_stub_process_rule_remove((guint)gtk_list_box_row_get_index(r));
    mod_profile_refresh(NULL);
    x3d_staged_widget_set_dirty(g_prf.stg_proc_profile, TRUE);
  }
}

static gboolean on_proc_prof_apply(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  x3d_stubs_toast("Stub: Process Profile rules staged (Phase 6 IPC pending)");
  return TRUE;
}

static void on_game_add_clicked(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  const char *nm = gtk_editable_get_text(g_prf.ent_game_name);
  if (nm && nm[0]) {
    if (add_game_ipc(nm)) {
      gtk_editable_set_text(g_prf.ent_game_name, "");
      mod_profile_refresh(NULL);
      x3d_stubs_toast("Game trigger added and synced via daemon IPC");
    } else {
      x3d_stubs_toast("Failed to add game trigger (daemon IPC error)");
    }
  }
}

static void on_game_rem_clicked(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  GtkListBoxRow *r = gtk_list_box_get_selected_row(g_prf.lb_games_usr);
  if (r) {
    GtkWidget *ch = gtk_list_box_row_get_child(r);
    if (ch && GTK_IS_LABEL(ch)) {
      if (remove_game_ipc(gtk_label_get_text(GTK_LABEL(ch)))) {
        mod_profile_refresh(NULL);
        x3d_stubs_toast("Game trigger removed via daemon IPC");
      } else {
        x3d_stubs_toast("Failed to remove game trigger (daemon IPC error)");
      }
    }
  }
}

static gboolean on_games_prof_apply(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  if (!x3d_ipc_cmd_ok("CONFIG_SYNC")) {
    x3d_stubs_toast("Failed to sync Games Profile with daemon (IPC error)");
    return FALSE;
  }
  mod_profile_refresh(NULL);
  return TRUE;
}

static void on_prof_cancel(X3dStagedWidget *w, gpointer d) {
  (void)w;
  (void)d;
  mod_profile_refresh(NULL);
}

static void on_prof_mgmt_popup_clicked(GtkButton *b, gpointer d) {
  (void)b;
  (void)d;
  gui_popups_show_profile_mgmt(g_prf.ctx.view->main_window);
}

static GtkWidget *mod_profile_build(const X3dPageCtx *ctx) {
  g_prf.ctx = *ctx;

  GtkBox *page = x3d_page_vbox_new(ctx->view->content_stack, "profiles",
                                   "Profile Manager");
  g_prf.root = GTK_WIDGET(page);

  GtkWidget *stg_proc =
      x3d_staged_widget_new("PROCESS PROFILE (STUB)", FALSE);
  g_prf.stg_proc_profile = X3D_STAGED_WIDGET(stg_proc);
  GtkBox *pbody = x3d_staged_widget_get_body(g_prf.stg_proc_profile);

  GtkWidget *prow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *pent = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(pent),
                                 "Process executable (e.g. blender)");
  gtk_widget_set_hexpand(pent, TRUE);
  const char *pols[] = {"Cache CCD (CCD0)", "Frequency CCD (CCD1)",
                        "Dual Unparked", "Exempt / Unrestricted", NULL};
  GtkWidget *pdd = gtk_drop_down_new_from_strings(pols);
  GtkWidget *btn_padd = gtk_button_new_with_label("Add");
  GtkWidget *btn_prem = gtk_button_new_with_label("Remove Selected");
  gtk_box_append(GTK_BOX(prow), pent);
  gtk_box_append(GTK_BOX(prow), pdd);
  gtk_box_append(GTK_BOX(prow), btn_padd);
  gtk_box_append(GTK_BOX(prow), btn_prem);
  gtk_box_append(pbody, prow);

  g_prf.ent_proc_name = GTK_EDITABLE(pent);
  g_prf.dd_proc_policy = GTK_DROP_DOWN(pdd);

  GtkWidget *plb = gtk_list_box_new();
  gtk_widget_add_css_class(plb, "x3d-extras-frame");
  g_prf.lb_proc_rules = GTK_LIST_BOX(plb);
  gtk_box_append(pbody, plb);
  gtk_box_append(page, stg_proc);

  g_signal_connect(btn_padd, "clicked", G_CALLBACK(on_proc_add_clicked), NULL);
  g_signal_connect(btn_prem, "clicked", G_CALLBACK(on_proc_rem_clicked), NULL);
  g_signal_connect(g_prf.stg_proc_profile, "apply",
                   G_CALLBACK(on_proc_prof_apply), NULL);
  g_signal_connect(g_prf.stg_proc_profile, "cancel",
                   G_CALLBACK(on_prof_cancel), NULL);

  GtkWidget *stg_games = x3d_staged_widget_new("GAMES PROFILE", FALSE);
  g_prf.stg_games_profile = X3D_STAGED_WIDGET(stg_games);
  GtkBox *gbody = x3d_staged_widget_get_body(g_prf.stg_games_profile);

  g_prf.lbl_games_sys_summary = gtk_label_new("System Trigger List: loading...");
  gtk_widget_add_css_class(g_prf.lbl_games_sys_summary, "x3d-gpu-subtitle");
  gtk_label_set_xalign(GTK_LABEL(g_prf.lbl_games_sys_summary), 0.0f);
  gtk_box_append(gbody, g_prf.lbl_games_sys_summary);

  GtkWidget *grow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *gent = gtk_entry_new();
  gtk_entry_set_placeholder_text(
      GTK_ENTRY(gent), "Game executable substring (e.g. cyberpunk2077)");
  gtk_widget_set_hexpand(gent, TRUE);
  GtkWidget *btn_gadd = gtk_button_new_with_label("Add Trigger");
  GtkWidget *btn_grem = gtk_button_new_with_label("Remove Selected");
  gtk_box_append(GTK_BOX(grow), gent);
  gtk_box_append(GTK_BOX(grow), btn_gadd);
  gtk_box_append(GTK_BOX(grow), btn_grem);
  gtk_box_append(gbody, grow);

  g_prf.ent_game_name = GTK_EDITABLE(gent);
  GtkWidget *glb = gtk_list_box_new();
  gtk_widget_add_css_class(glb, "x3d-extras-frame");
  g_prf.lb_games_usr = GTK_LIST_BOX(glb);
  gtk_box_append(gbody, glb);
  gtk_box_append(page, stg_games);

  g_signal_connect(btn_gadd, "clicked", G_CALLBACK(on_game_add_clicked), NULL);
  g_signal_connect(btn_grem, "clicked", G_CALLBACK(on_game_rem_clicked), NULL);
  g_signal_connect(g_prf.stg_games_profile, "apply",
                   G_CALLBACK(on_games_prof_apply), NULL);
  g_signal_connect(g_prf.stg_games_profile, "cancel",
                   G_CALLBACK(on_prof_cancel), NULL);

  GtkWidget *mgmt_imm = x3d_immediate_widget_new("PROFILE SNAPSHOT MANAGEMENT");
  GtkWidget *btn_mgmt = x3d_popup_btn_new("Management");
  GtkWidget *mitems[] = {btn_mgmt};
  x3d_immediate_widget_set_grid_items(X3D_IMMEDIATE_WIDGET(mgmt_imm), mitems,
                                      1);
  gtk_box_append(page, mgmt_imm);
  g_signal_connect(btn_mgmt, "clicked", G_CALLBACK(on_prof_mgmt_popup_clicked),
                   NULL);

  x3d_widgets_tree_connect_dirty_notify(g_prf.root, ctx->notify_dirty_cb,
                                        ctx->notify_dirty_data);
  return g_prf.root;
}

static gboolean mod_profile_has_dirty(void) {
  return x3d_widgets_tree_has_dirty(g_prf.root);
}
static gboolean mod_profile_has_staged(void) {
  return x3d_widgets_tree_has_staged(g_prf.root);
}
static gboolean mod_profile_apply_all(void) {
  return x3d_widgets_tree_apply_dirty(g_prf.root);
}
static void mod_profile_cancel_all(void) {
  x3d_widgets_tree_cancel_dirty(g_prf.root);
}
static void mod_profile_default_all(void) {
  x3d_widgets_tree_default_all(g_prf.root);
}

const X3dPageModule *gui_profile_get_module(void) {
  static const X3dPageModule k_mod = {
      .id = "profiles",
      .title = "Profile Manager",
      .is_subnav = FALSE,
      .initially_hidden = FALSE,
      .divider_after = FALSE,
      .build = mod_profile_build,
      .refresh = mod_profile_refresh,
      .activate = NULL,
      .deactivate = NULL,
      .has_dirty = mod_profile_has_dirty,
      .has_staged = mod_profile_has_staged,
      .apply_all = mod_profile_apply_all,
      .cancel_all = mod_profile_cancel_all,
      .default_all = mod_profile_default_all,
      .on_interval_changed = NULL,
      .highlight_hw_mode = NULL,
  };
  return &k_mod;
}
