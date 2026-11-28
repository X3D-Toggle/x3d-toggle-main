/* Hysteresis Baselines & AOD Firmware Restart Notice Popups Implementation
 * `popups/popup_baselines.c`
 */

#include "popup_baselines.h"

static void on_hyst_up_changed(GtkSpinButton *sp, gpointer d) {
  x3d_stubs_get()->hyst_up_threshold = (int)gtk_spin_button_get_value(sp);
  if (d)
    x3d_staged_widget_set_dirty(X3D_STAGED_WIDGET(d), TRUE);
}
static void on_hyst_down_changed(GtkSpinButton *sp, gpointer d) {
  x3d_stubs_get()->hyst_down_threshold = (int)gtk_spin_button_get_value(sp);
  if (d)
    x3d_staged_widget_set_dirty(X3D_STAGED_WIDGET(d), TRUE);
}
static void on_hyst_cool_changed(GtkSpinButton *sp, gpointer d) {
  x3d_stubs_get()->hyst_cooldown_sec = (int)gtk_spin_button_get_value(sp);
  if (d)
    x3d_staged_widget_set_dirty(X3D_STAGED_WIDGET(d), TRUE);
}
static void on_hyst_samp_changed(GtkSpinButton *sp, gpointer d) {
  x3d_stubs_get()->hyst_sample_ms = (int)gtk_spin_button_get_value(sp);
  if (d)
    x3d_staged_widget_set_dirty(X3D_STAGED_WIDGET(d), TRUE);
}

void popup_baselines_show_hysteresis(GtkWindow *parent,
                                     X3dStagedWidget *owner_staged) {
  GtkBox *body = NULL;
  GtkWindow *win = gui_popups_create_shell(
      parent, "Load / Polling Hysteresis Baselines", 480, 360, TRUE, &body);

  const X3dStubState *st = x3d_stubs_get();
  GtkWidget *card = x3d_current_widget_new("HYSTERESIS BASELINES", FALSE);
  GtkBox *cbody = x3d_current_widget_get_body(X3D_CURRENT_WIDGET(card));

  GtkWidget *sp_up = gtk_spin_button_new_with_range(15, 95, 5);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(sp_up), st->hyst_up_threshold);
  g_signal_connect(sp_up, "value-changed", G_CALLBACK(on_hyst_up_changed),
                   owner_staged);

  GtkWidget *sp_dn = gtk_spin_button_new_with_range(5, 85, 5);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(sp_dn), st->hyst_down_threshold);
  g_signal_connect(sp_dn, "value-changed", G_CALLBACK(on_hyst_down_changed),
                   owner_staged);

  GtkWidget *sp_cd = gtk_spin_button_new_with_range(1, 60, 1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(sp_cd), st->hyst_cooldown_sec);
  g_signal_connect(sp_cd, "value-changed", G_CALLBACK(on_hyst_cool_changed),
                   owner_staged);

  GtkWidget *sp_sw = gtk_spin_button_new_with_range(50, 2000, 50);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(sp_sw), st->hyst_sample_ms);
  g_signal_connect(sp_sw, "value-changed", G_CALLBACK(on_hyst_samp_changed),
                   owner_staged);

  gtk_box_append(cbody,
                 x3d_form_row_new("Up-Threshold (%)",
                                  "Load ceiling to enter throughput mode",
                                  sp_up));
  gtk_box_append(cbody,
                 x3d_form_row_new("Down-Threshold (%)",
                                  "Load floor to relax back to baseline",
                                  sp_dn));
  gtk_box_append(cbody,
                 x3d_form_row_new("Cooldown Timer (s)",
                                  "Minimum dwell time between mode transitions",
                                  sp_cd));
  gtk_box_append(cbody,
                 x3d_form_row_new("Sample Window (ms)",
                                  "CPU stat integration window", sp_sw));
  gtk_box_append(body, card);

  GtkWidget *close_btn = gtk_button_new_with_label("Done");
  gtk_widget_add_css_class(close_btn, "x3d-primary");
  gtk_widget_set_halign(close_btn, GTK_ALIGN_END);
  g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(gtk_window_close),
                           win);
  gtk_box_append(body, close_btn);

  gtk_window_present(win);
}

void popup_baselines_show_aod_restart(GtkWindow *parent) {
  GtkWidget *win = adw_window_new();
  gtk_window_set_title(GTK_WINDOW(win), "AOD Firmware Settings");
  gtk_window_set_default_size(GTK_WINDOW(win), 380, 185);
  gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
  if (parent)
    gtk_window_set_transient_for(GTK_WINDOW(win), parent);
  gtk_window_set_modal(GTK_WINDOW(win), TRUE);
  gtk_window_set_destroy_with_parent(GTK_WINDOW(win), TRUE);

  GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *hb = adw_header_bar_new();
  GtkWidget *hb_lbl = gtk_label_new("AOD Firmware Settings");
  gtk_widget_add_css_class(hb_lbl, "x3d-accent");
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(hb), hb_lbl);
  gtk_box_append(GTK_BOX(outer), hb);

  GtkWidget *body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_set_margin_start(body, 22);
  gtk_widget_set_margin_end(body, 22);
  gtk_widget_set_margin_top(body, 18);
  gtk_widget_set_margin_bottom(body, 18);
  gtk_widget_set_vexpand(body, TRUE);
  gtk_widget_set_valign(body, GTK_ALIGN_CENTER);

  GtkWidget *msg = gtk_label_new("Please Restart to Apply Firmware Settings");
  gtk_widget_add_css_class(msg, "x3d-accent");
  gtk_label_set_wrap(GTK_LABEL(msg), TRUE);
  gtk_label_set_justify(GTK_LABEL(msg), GTK_JUSTIFY_CENTER);
  gtk_box_append(GTK_BOX(body), msg);

  GtkWidget *sub = gtk_label_new(
      "AMD Overclocking (AOD) setup values have been staged and take effect "
      "after a system restart.");
  gtk_widget_add_css_class(sub, "x3d-gpu-subtitle");
  gtk_label_set_wrap(GTK_LABEL(sub), TRUE);
  gtk_label_set_justify(GTK_LABEL(sub), GTK_JUSTIFY_CENTER);
  gtk_box_append(GTK_BOX(body), sub);

  GtkWidget *ok_btn = gtk_button_new_with_label("OK");
  gtk_widget_add_css_class(ok_btn, "x3d-primary");
  gtk_widget_set_halign(ok_btn, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top(ok_btn, 4);
  g_signal_connect_swapped(ok_btn, "clicked", G_CALLBACK(gtk_window_close),
                           win);
  gtk_box_append(GTK_BOX(body), ok_btn);

  gtk_box_append(GTK_BOX(outer), body);
  adw_window_set_content(ADW_WINDOW(win), outer);
  gtk_window_set_default_widget(GTK_WINDOW(win), ok_btn);

  gtk_window_present(GTK_WINDOW(win));
}
