/* Profile Snapshot Management Popup Implementation
 * `popups/popup_profiles.c`
 */

#include "popup_profiles.h"

typedef struct {
  GtkEditable *ent_name;
  GtkDropDown *dd_saved;
} ProfMgmtCtx;

static void refresh_prof_mgmt_dd(ProfMgmtCtx *ctx) {
  GtkStringList *sl = gtk_string_list_new(NULL);
  GPtrArray *arr = x3d_stubs_get()->saved_profiles;
  for (guint i = 0; arr && i < arr->len; i++)
    gtk_string_list_append(sl, g_ptr_array_index(arr, i));
  gtk_drop_down_set_model(ctx->dd_saved, G_LIST_MODEL(sl));
  g_object_unref(sl);
}

static const char *get_selected_prof_name(ProfMgmtCtx *ctx) {
  const char *typed = gtk_editable_get_text(ctx->ent_name);
  if (typed && typed[0])
    return typed;
  GtkStringObject *so =
      GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(ctx->dd_saved));
  return so ? gtk_string_object_get_string(so) : "default";
}

static void on_prof_mgmt_btn(GtkButton *btn, gpointer data) {
  ProfMgmtCtx *ctx = (ProfMgmtCtx *)data;
  const char *act = g_object_get_data(G_OBJECT(btn), "x3d-prof-act");
  x3d_stub_profile_mgmt_action(act, get_selected_prof_name(ctx));
  refresh_prof_mgmt_dd(ctx);
}

void popup_profiles_show_mgmt(GtkWindow *parent) {
  GtkBox *body = NULL;
  GtkWindow *win = gui_popups_create_shell(parent, "Profile Management", 520,
                                           400, TRUE, &body);

  ProfMgmtCtx *ctx = g_new0(ProfMgmtCtx, 1);
  g_signal_connect_swapped(win, "destroy", G_CALLBACK(g_free), ctx);

  GtkWidget *card = x3d_current_widget_new("NAMED PROFILES & BACKUP", FALSE);
  GtkBox *cbody = x3d_current_widget_get_body(X3D_CURRENT_WIDGET(card));

  GtkWidget *ent = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(ent), "e.g. hardcore-gaming");
  ctx->ent_name = GTK_EDITABLE(ent);
  gtk_box_append(cbody,
                 x3d_form_row_new("Profile Name",
                                  "Target name for Save or Overwrite", ent));

  GtkWidget *dd = gtk_drop_down_new(NULL, NULL);
  ctx->dd_saved = GTK_DROP_DOWN(dd);
  refresh_prof_mgmt_dd(ctx);
  gtk_box_append(
      cbody, x3d_form_row_new("Saved Profiles",
                              "Select saved snapshot to Restore or Delete", dd));

  static const struct {
    const char *act;
    const char *lbl;
  } acts[] = {{"backup", "Backup"},
              {"save", "Save Configuration to Named Profile"},
              {"restore", "Restore Configuration from Saved Profile"},
              {"delete", "Delete"},
              {"overwrite", "Overwrite"}};

  GtkWidget *btns[5];
  for (int i = 0; i < 5; i++) {
    btns[i] = x3d_popup_btn_new(acts[i].lbl);
    g_object_set_data(G_OBJECT(btns[i]), "x3d-prof-act", (gpointer)acts[i].act);
    g_signal_connect(btns[i], "clicked", G_CALLBACK(on_prof_mgmt_btn), ctx);
  }
  GtkWidget *grid = x3d_centered_3col_grid_new();
  x3d_grid_pack_centered_3col(GTK_GRID(grid), btns, 5);
  gtk_widget_set_margin_top(grid, 8);
  gtk_box_append(cbody, grid);

  gtk_box_append(body, card);

  GtkWidget *close_btn = gtk_button_new_with_label("Close");
  gtk_widget_add_css_class(close_btn, "x3d-primary");
  gtk_widget_set_halign(close_btn, GTK_ALIGN_END);
  g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(gtk_window_close),
                           win);
  gtk_box_append(body, close_btn);

  gtk_window_present(win);
}
