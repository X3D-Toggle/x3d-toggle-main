/* GTK4 Frontend for X3D Toggle
 *
 * Wrapper: Loads declarative UI, binds button IDs to CLI commands.
 * `action_x3dtoggle_X` button -> executes `x3d-toggle X`
 */

#include "../../build/ccd.h"
#include "../../build/config.h"
#include "../../include/libc.h"
#include "error.h"
#include <adwaita.h>
#include <gtk/gtk.h>

extern int socket_send(const char *cmd, char *response, size_t resp_len);
extern int printf_sn(char *buf, size_t size, const char *fmt, ...);
extern char *strncpy(char *d, const char *s, size_t n);

#define BUFF_LINE 256
#define BUFF_INFO 128
#define BUFF_STATE 16
#define CONF_PATH DAEMON_CONF_PATH
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/* ── Config: Read settings.conf / IPC (forward-decl) ─────────── */
static void config_send(const char *key, const char *value);
static const char *config_get(const char *key);
static GtkListBoxRow *find_sidebar_row(GtkListBox *lb, const char *name);

static GtkWidget *lbl_status_dump = NULL;
static GtkEditable *g_cfg_server_ip = NULL;
static GtkEditable *g_cfg_server_port = NULL;
static GtkSpinButton *g_cfg_journal_keep = NULL;
static GtkDropDown *g_cfg_journal_max_mb = NULL;

/* Dev mode nav rows (hidden by default) */
static GtkWidget *row_developer = NULL;
static GtkWidget *row_debug = NULL;
static GtkWidget *row_advanced = NULL;
static GtkWidget *row_affinity = NULL;
static GtkListBox *g_sidebar = NULL;
static GtkStack *g_stack = NULL;
static GtkDropDown *g_lifecycle_dropdown = NULL;
static GtkDropDown *g_cfg_affinity_level = NULL;
static GtkGrid *g_grid_topology = NULL;
static GtkWidget *btn_cfg_apply = NULL;
static GtkWidget *btn_cfg_cancel = NULL;
static GtkDropDown *g_cfg_daemon_state = NULL;
static GtkDropDown *g_cfg_fallback_profile = NULL;
static GtkRange *g_cfg_polling_interval = NULL;
static GtkRange *g_cfg_load_threshold = NULL;
static GtkDropDown *g_cfg_detection_level = NULL;
static GtkSwitch *g_cfg_ebpf_enable = NULL;
static GtkSwitch *g_cfg_debug_enable = NULL;
static GtkSwitch *g_cfg_dev_enable = NULL;
static GtkSwitch *g_cfg_advanced_enable = NULL;

static gboolean config_dirty = FALSE;
static gboolean config_ignoring_changes = FALSE;

static void set_config_dirty(gboolean dirty) {
  config_dirty = dirty;
  if (btn_cfg_apply)
    gtk_widget_set_sensitive(btn_cfg_apply, dirty);
  if (btn_cfg_cancel)
    gtk_widget_set_sensitive(btn_cfg_cancel, dirty);
}

static const char *g_selected_mode = NULL;
static GtkWidget *mode_btns[5]; /* Cache, Frequency, Dual, Default, Reset */

/* ── Dashboard Polling ────────────────────────────────────────── */

static int daemon_retry_cooldown = 0;

static gboolean update_dashboard_cb(gpointer user_data) {
  (void)user_data;
  if (!lbl_status_dump)
    return G_SOURCE_CONTINUE;

  char info[BUFF_INFO] = {0};
  if (socket_send("DAEMON_INFO", info, sizeof(info)) != 0) {
    gtk_label_set_label(GTK_LABEL(lbl_status_dump),
                        "Daemon offline — attempting restart...");

    /* Auto-restart guardrail: try to revive daemon, with cooldown */
    if (daemon_retry_cooldown <= 0) {
      daemon_retry_cooldown = 10; /* wait 10 polls before retrying */
      char *argv[] = {(char *)"/usr/bin/systemctl", (char *)"start",
                      (char *)"x3d-toggle.service", NULL};
      g_spawn_async(NULL, argv, NULL,
                    G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                    NULL, NULL, NULL, NULL);
    } else {
      daemon_retry_cooldown--;
    }
    return G_SOURCE_CONTINUE;
  }
  daemon_retry_cooldown = 0;

  char display[BUFF_LINE * 2] = {0};
  char state_str[BUFF_STATE] = "Unknown";
  char active_str[BUFF_STATE] = "Inactive";

  char *st = strstr(info, "STATE=");
  char *ba = strstr(info, "BPF_ACTIVE=");

  if (st) {
    strncpy(state_str, st + 6, sizeof(state_str) - 1);
    state_str[sizeof(state_str) - 1] = '\0';
    char *sep = strchr(state_str, ';');
    if (!sep)
      sep = strchr(state_str, '|');
    if (sep)
      *sep = '\0';
  }
  if (ba && atoi(ba + 11))
    strncpy(active_str, "eBPF (Active)", sizeof(active_str) - 1);
  else
    strncpy(active_str, "Polling", sizeof(active_str) - 1);
  active_str[sizeof(active_str) - 1] = '\0';

  /* Fetch v-Cache mode via MODE IPC */
  char mode_str[BUFF_STATE] = "Unknown";
  if (socket_send("MODE", mode_str, sizeof(mode_str)) != 0) {
    strncpy(mode_str, "N/A", sizeof(mode_str) - 1);
    mode_str[sizeof(mode_str) - 1] = '\0';
  }

  printf_sn(display, sizeof(display),
            "<b>Daemon State:</b>  %s\n"
            "<b>Detection:</b>     %s\n"
            "<b>v-Cache Mode:</b>  %s",
            state_str, active_str, mode_str);

  gtk_label_set_markup(GTK_LABEL(lbl_status_dump), display);
  return G_SOURCE_CONTINUE;
}

/* ── Spawn Helper ─────────────────────────────────────────────── */

static void gui_spawn(char **args, char **env) {
  gchar **new_env = g_get_environ();
  if (env) {
    for (int i = 0; env[i] != NULL; i++) {
      char *eq = strchr(env[i], '=');
      if (eq) {
        gchar *key = g_strndup(env[i], eq - env[i]);
        new_env = g_environ_setenv(new_env, key, eq + 1, TRUE);
        g_free(key);
      }
    }
  }
  g_spawn_async(NULL, args, new_env,
                G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL |
                    G_SPAWN_SEARCH_PATH,
                NULL, NULL, NULL, NULL);
  g_strfreev(new_env);
}

/* ── Journal Callbacks ────────────────────────────────────────── */

static gboolean reset_btn_lbl(gpointer data) {
  GtkWidget **w = (GtkWidget **)data;
  gtk_button_set_label(GTK_BUTTON(w[0]), (const char *)w[1]);
  gtk_widget_set_sensitive(w[0], TRUE);
  g_free(w[1]);
  g_free(w);
  return G_SOURCE_REMOVE;
}

static void btn_feedback(GtkButton *btn) {
  const char *orig = gtk_button_get_label(btn);
  gtk_button_set_label(btn, "Done!");
  gtk_widget_set_sensitive(GTK_WIDGET(btn), FALSE);
  GtkWidget **w = g_new(GtkWidget *, 2);
  w[0] = GTK_WIDGET(btn);
  w[1] = (GtkWidget *)g_strdup(orig);
  g_timeout_add(1500, reset_btn_lbl, w);
}

static void on_btn_launch_debug_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  (void)data;
  char *args[] = {
      (char *)"/bin/sh", (char *)"-c",
      (char *)"kgx -e '/usr/lib/x3d-toggle/scripts/tools/debug.sh' || "
              "gnome-terminal -- /usr/lib/x3d-toggle/scripts/tools/debug.sh || "
              "konsole -e /usr/lib/x3d-toggle/scripts/tools/debug.sh || xterm "
              "-e /usr/lib/x3d-toggle/scripts/tools/debug.sh",
      NULL};
  char *env[] = {(char *)"X3D_EXEC=1", NULL};
  gui_spawn(args, env);
}

static void on_btn_analyze_coredump_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  (void)data;
  char *args[] = {
      (char *)"/bin/sh", (char *)"-c",
      (char *)"kgx -e '/usr/lib/x3d-toggle/scripts/tools/coredump.sh' || "
              "gnome-terminal -- /usr/lib/x3d-toggle/scripts/tools/coredump.sh "
              "|| konsole -e /usr/lib/x3d-toggle/scripts/tools/coredump.sh || "
              "xterm -e /usr/lib/x3d-toggle/scripts/tools/coredump.sh",
      NULL};
  char *env[] = {(char *)"X3D_EXEC=1", NULL};
  gui_spawn(args, env);
}

static void on_btn_archive_journal_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  (void)data;
  char *args[] = {(char *)"/bin/sh",
                  (char *)"/usr/lib/x3d-toggle/scripts/tools/archive.sh", NULL};
  char *env[] = {(char *)"X3D_EXEC=1", NULL};
  gui_spawn(args, env);
  btn_feedback(btn);
}

static void on_btn_rotate_journal_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  (void)data;
  char *args[] = {(char *)"/bin/sh",
                  (char *)"/usr/lib/x3d-toggle/scripts/tools/rotate.sh",
                  (char *)"--all", NULL};
  char *env[] = {(char *)"X3D_EXEC=1", NULL};
  gui_spawn(args, env);
  btn_feedback(btn);
}

/* ── Mode Selection Callbacks ─────────────────────────────────── */

static void on_mode_card_clicked(GtkButton *btn, gpointer data) {
  const char *mode = (const char *)data;
  g_selected_mode = mode;

  /* Update visual state */
  for (int i = 0; i < 5; i++) {
    GtkWidget *card = mode_btns[i];
    if (card) {
      const char *name = gtk_widget_get_name(card);
      if (name && strstr(name, mode)) {
        gtk_widget_add_css_class(card, "selected");
      } else {
        gtk_widget_remove_css_class(card, "selected");
      }
    }
  }
}

static void on_mode_apply_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  (void)data;

  /* 1. Execute Hardware Mode via IPC */
  if (g_selected_mode) {
    char cmd[64] = {0};
    if (strcmp(g_selected_mode, "reset") == 0) {
      /* Reset: clear override and let daemon re-probe */
      socket_send("SET_MODE reset", NULL, 0);
    } else if (strcmp(g_selected_mode, "default") == 0) {
      /* Default: restore daemon defaults */
      socket_send("SET_DAEMON default", NULL, 0);
      socket_send("CONFIG_SYNC", NULL, 0);
    } else {
      /* cache / frequency / dual: set mode + suspend daemon */
      printf_sn(cmd, sizeof(cmd), "SET_MODE %s", g_selected_mode);
      socket_send("SET_DAEMON manual", NULL, 0);
      socket_send(cmd, NULL, 0);
    }
  }

  /* 2. Execute Lifecycle Action via IPC */
  if (g_lifecycle_dropdown) {
    guint idx = gtk_drop_down_get_selected(g_lifecycle_dropdown);
    /* 0=None, 1=Wake, 2=Sleep, 3=Stop, 4=Enable, 5=Autostart */
    switch (idx) {
    case 1: /* Wake */
      socket_send("CONFIG_SYNC", NULL, 0);
      socket_send("SET_DAEMON default", NULL, 0);
      socket_send("SET_MODE frequency", NULL, 0);
      break;
    case 2: /* Sleep */
      socket_send("SET_DAEMON manual", NULL, 0);
      break;
    case 3: /* Stop */
      socket_send("DAEMON_DISABLE", NULL, 0);
      break;
    default:
      break;
    }
  }

  /* Go back to Dashboard */
  if (g_stack)
    gtk_stack_set_visible_child_name(g_stack, "dashboard");
  if (g_sidebar) {
    GtkListBoxRow *r = find_sidebar_row(g_sidebar, "dashboard");
    if (r)
      gtk_list_box_select_row(g_sidebar, r);
  }
}
static void on_mode_cancel_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  (void)data;
  /* Go back to Dashboard */
  if (g_stack)
    gtk_stack_set_visible_child_name(g_stack, "dashboard");
  if (g_sidebar) {
    GtkListBoxRow *r = find_sidebar_row(g_sidebar, "dashboard");
    if (r)
      gtk_list_box_select_row(g_sidebar, r);
  }
}

/* ── Sidebar Navigation ──────────────────────────────────────── */

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
static int is_core_in_mask(int core, const char *mask) {
  char buf[256];
  strncpy(buf, mask, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  char *p = buf;
  while (p && *p) {
    char *comma = strchr(p, ',');
    if (comma)
      *comma = '\0';
    char *hyphen = strchr(p, '-');
    if (hyphen) {
      int start = atoi(p);
      int end = atoi(hyphen + 1);
      if (core >= start && core <= end)
        return 1;
    } else {
      if (atoi(p) == core)
        return 1;
    }
    if (!comma)
      break;
    p = comma + 1;
  }
  return 0;
}

static void on_core_toggled(GtkToggleButton *btn, gpointer user_data) {
  (void)user_data;

  /* Auto-toggle to Manual mode (index 2) upon user interaction */
  if (g_cfg_affinity_level) {
    gtk_drop_down_set_selected(g_cfg_affinity_level, 2);
  }

  if (gtk_toggle_button_get_active(btn)) {
    gtk_widget_add_css_class(GTK_WIDGET(btn), "x3d-accent");
  } else {
    gtk_widget_remove_css_class(GTK_WIDGET(btn), "x3d-accent");
  }
}

void refresh_topology(void) {
  if (!g_grid_topology)
    return;
  char mask[128] = {0};
  int ccd1_start = 0;
  int total_cores = 0;
  if (ccd(mask, &ccd1_start, &total_cores) != 0)
    return;

  /* Priority: Use user-defined persistence from daemon.conf if available */
  const char *saved_mask = config_get("AFFINITY_MASK");
  if (saved_mask && saved_mask[0] && strcmp(saved_mask, "none") != 0) {
    strncpy(mask, saved_mask, sizeof(mask) - 1);
  }

  GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(g_grid_topology));
  while (child) {
    GtkWidget *next = gtk_widget_get_next_sibling(child);
    gtk_grid_remove(g_grid_topology, child);
    child = next;
  }

  for (int i = 0; i < total_cores; i++) {
    char label[8];
    printf_sn(label, sizeof(label), "%d", i);
    GtkWidget *btn = gtk_toggle_button_new_with_label(label);
    gtk_grid_attach(g_grid_topology, btn, i % 8, i / 8, 1, 1);
    if (is_core_in_mask(i, mask)) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn), TRUE);
      gtk_widget_add_css_class(btn, "x3d-accent");
    }
    g_signal_connect(btn, "toggled", G_CALLBACK(on_core_toggled), NULL);
  }
}

static void on_affinity_apply_clicked(GtkButton *btn, gpointer user_data) {
  (void)btn;
  (void)user_data;

  if (!g_grid_topology)
    return;

  char cache_mask[256] = {0};
  char freq_mask[256] = {0};
  int cache_pos = 0;
  int freq_pos = 0;

  GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(g_grid_topology));
  while (child) {
    if (GTK_IS_TOGGLE_BUTTON(child)) {
      GtkToggleButton *tb = GTK_TOGGLE_BUTTON(child);
      const char *label = gtk_button_get_label(GTK_BUTTON(tb));
      if (gtk_toggle_button_get_active(tb)) {
        cache_pos +=
            printf_sn(cache_mask + cache_pos, sizeof(cache_mask) - cache_pos,
                      "%s%s", (cache_pos > 0) ? "," : "", label);
      } else {
        freq_pos +=
            printf_sn(freq_mask + freq_pos, sizeof(freq_mask) - freq_pos,
                      "%s%s", (freq_pos > 0) ? "," : "", label);
      }
    }
    child = gtk_widget_get_next_sibling(child);
  }

  char cmd[512];
  printf_sn(cmd, sizeof(cmd), "SET_AFFINITY_CONFIG %s %s",
            (cache_mask[0]) ? cache_mask : "none",
            (freq_mask[0]) ? freq_mask : "none");
  int ret = socket_send(cmd, NULL, 0);
  if (atoi(config_get("DEBUG_ENABLE"))) {
    journal_debug("GUI: Sent affinity config: %s (result=%d)", cmd, ret);
  }

  /* Brief visual confirmation (Update status label if available) */
  if (lbl_status_dump) {
    gtk_label_set_label(GTK_LABEL(lbl_status_dump),
                        "Affinity Settings Applied");
  }

  /* Persistence */
  config_send("AFFINITY_MASK", cache_mask);
  config_send("AFFINITY_FREQ_MASK", freq_mask);

  if (g_cfg_affinity_level) {
    char lvl_str[4];
    printf_sn(lvl_str, sizeof(lvl_str), "%u",
              gtk_drop_down_get_selected(g_cfg_affinity_level));
    config_send("AFFINITY_LEVEL", lvl_str);
  }
}

static void on_affinity_cancel_clicked(GtkButton *btn, gpointer user_data) {
  (void)btn;
  (void)user_data;

  /* Reset dropdown to saved config */
  if (g_cfg_affinity_level) {
    const char *val = config_get("AFFINITY_LEVEL");
    gtk_drop_down_set_selected(g_cfg_affinity_level,
                               (guint)atoi(val ? val : "0"));
  }

  refresh_topology();
}
static void on_nav_row_selected(GtkListBox *box, GtkListBoxRow *row,
                                gpointer user_data) {
  (void)box;
  if (!row)
    return;
  GtkStack *stack = GTK_STACK(user_data);
  const char *id = gtk_widget_get_name(GTK_WIDGET(row));
  gtk_stack_set_visible_child_name(stack, id);

  if (id && strcmp(id, "affinity") == 0) {
    refresh_topology();
  }
}

/* ── Config: Read settings.conf ───────────────────────────────── */

/* ── Config: IPC-first lookup, file fallback ──────────────────── */

/* Sidebar row lookup by name (avoids fragile index arithmetic) */
static GtkListBoxRow *find_sidebar_row(GtkListBox *lb, const char *name) {
  if (!lb || !name) return NULL;
  for (int i = 0; ; i++) {
    GtkListBoxRow *r = gtk_list_box_get_row_at_index(lb, i);
    if (!r) break;
    const char *row_name = gtk_widget_get_name(GTK_WIDGET(r));
    if (row_name && strcmp(row_name, name) == 0)
      return r;
  }
  return NULL;
}

static const char *config_get(const char *key) {
  static char val[256];
  val[0] = '\0';

  /* 1. Try live IPC: GET_CONFIG <KEY> */
  char cmd[128];
  printf_sn(cmd, sizeof(cmd), "GET_CONFIG %s", key);
  char ipc_resp[256] = {0};
  if (socket_send(cmd, ipc_resp, sizeof(ipc_resp)) == 0 &&
      ipc_resp[0] != '\0' && strcmp(ipc_resp, "ERR") != 0) {
    strncpy(val, ipc_resp, sizeof(val) - 1);
    val[sizeof(val) - 1] = '\0';
    return val;
  }

  /* 2. Fallback: parse DAEMON_CONF_PATH directly (daemon offline) */
  FILE *f = fopen(CONF_PATH, "r");
  if (!f)
    return val;

  char line[512];
  size_t klen = strlen(key);
  while (fgets(line, sizeof(line), f)) {
    if (line[0] == '#' || line[0] == '\n')
      continue;
    if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
      char *v = line + klen + 1;
      v[strcspn(v, "\r\n")] = '\0';
      strncpy(val, v, sizeof(val) - 1);
      val[sizeof(val) - 1] = '\0';
      break;
    }
  }
  fclose(f);
  return val;
}


/* ── Config: IPC send helper ──────────────────────────────────── */

static void config_send(const char *key, const char *value) {
  char cmd[256], resp[64];
  printf_sn(cmd, sizeof(cmd), "SET_CONFIG %s %s", key, value);
  socket_send(cmd, resp, sizeof(resp));
}

/* ── Config: Widget change callbacks ──────────────────────────── */

typedef struct {
  const char *key;
  const char **values; /* NULL for numeric */
  int offset; /* index offset for numeric values (e.g., detection 1-based) */
} CfgDropData;

static void on_cfg_dropdown_changed(GObject *obj, GParamSpec *pspec,
                                    gpointer data) {
  (void)obj;
  (void)pspec;
  (void)data;
  if (config_ignoring_changes)
    return;
  set_config_dirty(TRUE);
}

static void on_cfg_switch_changed(GObject *obj, GParamSpec *pspec,
                                  gpointer data) {
  (void)obj;
  (void)pspec;
  (void)data;
  if (config_ignoring_changes)
    return;
  set_config_dirty(TRUE);
}

/* enabled/disabled style switches for linter settings */
static void on_cfg_switch_enabled(GObject *obj, GParamSpec *pspec,
                                  gpointer data) {
  (void)pspec;
  const char *key = (const char *)data;
  gboolean active = gtk_switch_get_active(GTK_SWITCH(obj));
  config_send(key, active ? "enabled" : "disabled");
}

static void on_cfg_scale_changed(GtkRange *range, gpointer data) {
  (void)range;
  (void)data;
  if (config_ignoring_changes)
    return;
  set_config_dirty(TRUE);
}

static void on_server_address_changed(GtkEditable *editable, gpointer data) {
  (void)editable;
  (void)data;
  if (config_ignoring_changes)
    return;
  if (!g_cfg_server_ip || !g_cfg_server_port)
    return;
  const char *ip = gtk_editable_get_text(g_cfg_server_ip);
  const char *port = gtk_editable_get_text(g_cfg_server_port);
  if (!ip || !ip[0] || !port || !port[0])
    return;
  /* char combined[128];
  printf_sn(combined, sizeof(combined), "%s:%s", ip, port);
  config_send("SERVER_ADDRESS", combined); */
  set_config_dirty(TRUE);
}

/* ── Dev mode visibility toggle ──────────────────────────────── */

static void on_advanced_enable_toggled(GObject *obj, GParamSpec *pspec,
                                       gpointer data) {
  (void)pspec;
  (void)data;
  gboolean active = gtk_switch_get_active(GTK_SWITCH(obj));
  config_send("ADVANCED_CONFIG_ENABLE", active ? "1" : "0");

  gtk_widget_set_visible(row_advanced, active);

  /* If hiding and currently viewing advanced page, switch back to config */
  if (!active && g_stack) {
    const char *vis = gtk_stack_get_visible_child_name(g_stack);
    if (vis && (strcmp(vis, "advanced") == 0)) {
      gtk_stack_set_visible_child_name(g_stack, "configuration");
      if (g_sidebar) {
        GtkListBoxRow *r = find_sidebar_row(g_sidebar, "configuration");
        if (r)
          gtk_list_box_select_row(g_sidebar, r);
      }
    }
  }
}

static void on_dev_enable_toggled(GObject *obj, GParamSpec *pspec,
                                  gpointer data) {
  (void)pspec;
  (void)data;
  gboolean active = gtk_switch_get_active(GTK_SWITCH(obj));
  config_send("DEV_ENABLE", active ? "1" : "0");

  gtk_widget_set_visible(row_developer, active);
  gtk_widget_set_visible(row_debug, active);

  /* If hiding and currently viewing a dev page, switch back to config */
  if (!active && g_stack) {
    const char *vis = gtk_stack_get_visible_child_name(g_stack);
    if (vis && (strcmp(vis, "developer") == 0 || strcmp(vis, "debug") == 0)) {
      gtk_stack_set_visible_child_name(g_stack, "configuration");
      /* Select config row in sidebar */
      if (g_sidebar) {
        GtkListBoxRow *r = find_sidebar_row(g_sidebar, "configuration");
        if (r)
          gtk_list_box_select_row(g_sidebar, r);
      }
    }
  }
}


/* ── Config: Bind all widgets ─────────────────────────────────── */

static const char *daemon_state_vals[] = {"default", "auto", "manual"};
static const char *fallback_vals[] = {"default", "cache", "frequency"};
// static const char *detection_vals[] = {"Strict", "Loose"};
// static const char *affinity_vals[] = {"Auto", "By Die", "Manual"};
static const char *valgrind_mode_vals[] = {"full", "summary", "disabled"};
static const char *valgrind_kinds_vals[] = {"all", "definite", "indirect",
                                            "possible", "reachable"};
static const char *journal_max_mb_vals[] = {"5", "10", "25", "50", "100"};

static CfgDropData dd_daemon_state = {"DAEMON_STATE", daemon_state_vals, 0};
static CfgDropData dd_fallback = {"FALLBACK_PROFILE", fallback_vals, 0};
static CfgDropData dd_detection = {"DETECTION_LEVEL", NULL, 1}; /* 1-based */
static CfgDropData dd_affinity = {"AFFINITY_LEVEL", NULL, 0};
static CfgDropData dd_valgrind_m = {"LINT_VALGRIND_MODE", valgrind_mode_vals,
                                    0};
static CfgDropData dd_valgrind_k = {"LINT_VALGRIND_KINDS", valgrind_kinds_vals,
                                    0};

static guint find_dropdown_idx(const char **values, int count,
                               const char *target) {
  for (int i = 0; i < count; i++)
    if (strcmp(values[i], target) == 0)
      return (guint)i;
  return 0;
}

static void on_rotation_apply_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  (void)data;
  if (g_cfg_journal_keep) {
    int val = (int)gtk_spin_button_get_value(g_cfg_journal_keep);
    char val_str[16];
    printf_sn(val_str, sizeof(val_str), "%d", val);
    config_send("JOURNAL_KEEP", val_str);
  }
  if (g_cfg_journal_max_mb) {
    guint idx = gtk_drop_down_get_selected(g_cfg_journal_max_mb);
    if (idx < ARRAY_SIZE(journal_max_mb_vals)) {
      char val_str[16];
      printf_sn(val_str, sizeof(val_str), "%s", journal_max_mb_vals[idx]);
      config_send("JOURNAL_MAX_MB", val_str);
    }
  }
}

static void on_rotation_cancel_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  (void)data;
  if (g_cfg_journal_keep) {
    const char *val = config_get("JOURNAL_KEEP");
    if (val && val[0])
      gtk_spin_button_set_value(g_cfg_journal_keep, atoi(val));
  }
  if (g_cfg_journal_max_mb) {
    const char *val = config_get("JOURNAL_MAX_MB");
    gtk_drop_down_set_selected(
        g_cfg_journal_max_mb,
        find_dropdown_idx(journal_max_mb_vals, ARRAY_SIZE(journal_max_mb_vals),
                          val));
  }
}

static void on_cfg_apply_clicked(GtkButton *btn, gpointer user_data) {
  (void)btn;
  (void)user_data;
  
  if (btn_cfg_apply)
    gtk_button_set_label(GTK_BUTTON(btn_cfg_apply), "Applying...");

  if (g_cfg_daemon_state)
    config_send("DAEMON_STATE",
                daemon_state_vals[gtk_drop_down_get_selected(g_cfg_daemon_state)]);
  if (g_cfg_fallback_profile)
    config_send(
        "FALLBACK_PROFILE",
        fallback_vals[gtk_drop_down_get_selected(g_cfg_fallback_profile)]);
  if (g_cfg_polling_interval) {
    char buf[16];
    printf_sn(buf, sizeof(buf), "%d",
              (int)gtk_range_get_value(g_cfg_polling_interval));
    config_send("POLLING_INTERVAL", buf);
  }
  if (g_cfg_load_threshold) {
    char buf[16];
    printf_sn(buf, sizeof(buf), "%d",
              (int)gtk_range_get_value(g_cfg_load_threshold));
    config_send("LOAD_THRESHOLD", buf);
  }
  if (g_cfg_detection_level) {
    char buf[16];
    printf_sn(buf, sizeof(buf), "%d",
              (int)gtk_drop_down_get_selected(g_cfg_detection_level) + 1);
    config_send("DETECTION_LEVEL", buf);
  }
  if (g_cfg_ebpf_enable)
    config_send("EBPF_ENABLE",
                gtk_switch_get_active(g_cfg_ebpf_enable) ? "1" : "0");
  if (g_cfg_debug_enable)
    config_send("DEBUG_ENABLE",
                gtk_switch_get_active(g_cfg_debug_enable) ? "1" : "0");
  if (g_cfg_dev_enable)
    config_send("DEV_ENABLE",
                gtk_switch_get_active(g_cfg_dev_enable) ? "1" : "0");
  if (g_cfg_advanced_enable)
    config_send("ADVANCED_CONFIG_ENABLE",
                gtk_switch_get_active(g_cfg_advanced_enable) ? "1" : "0");

  if (g_cfg_server_ip && g_cfg_server_port) {
    const char *ip = gtk_editable_get_text(g_cfg_server_ip);
    const char *port = gtk_editable_get_text(g_cfg_server_port);
    if (ip && ip[0] && port && port[0]) {
      char combined[128];
      printf_sn(combined, sizeof(combined), "%s:%s", ip, port);
      config_send("SERVER_ADDRESS", combined);
    }
  }

  set_config_dirty(FALSE);
  if (btn_cfg_apply)
    gtk_button_set_label(GTK_BUTTON(btn_cfg_apply), "Apply");
}

static void on_cfg_cancel_clicked(GtkButton *btn, gpointer user_data) {
  (void)btn;
  (void)user_data;
  config_ignoring_changes = TRUE;

  if (g_cfg_daemon_state)
    gtk_drop_down_set_selected(
        g_cfg_daemon_state,
        find_dropdown_idx(daemon_state_vals, ARRAY_SIZE(daemon_state_vals),
                          config_get("DAEMON_STATE")));
  if (g_cfg_fallback_profile)
    gtk_drop_down_set_selected(
        g_cfg_fallback_profile,
        find_dropdown_idx(fallback_vals, ARRAY_SIZE(fallback_vals),
                          config_get("FALLBACK_PROFILE")));
  if (g_cfg_polling_interval)
    gtk_range_set_value(g_cfg_polling_interval,
                        atof(config_get("POLLING_INTERVAL")));
  if (g_cfg_load_threshold)
    gtk_range_set_value(g_cfg_load_threshold,
                        atof(config_get("LOAD_THRESHOLD")));
  if (g_cfg_detection_level)
    gtk_drop_down_set_selected(
        g_cfg_detection_level, (guint)(atoi(config_get("DETECTION_LEVEL")) - 1));
  if (g_cfg_ebpf_enable)
    gtk_switch_set_active(g_cfg_ebpf_enable,
                          atoi(config_get("EBPF_ENABLE")) != 0);
  if (g_cfg_debug_enable)
    gtk_switch_set_active(g_cfg_debug_enable,
                          atoi(config_get("DEBUG_ENABLE")) != 0);
  if (g_cfg_dev_enable)
    gtk_switch_set_active(g_cfg_dev_enable, atoi(config_get("DEV_ENABLE")) != 0);
  if (g_cfg_advanced_enable)
    gtk_switch_set_active(g_cfg_advanced_enable,
                          atoi(config_get("ADVANCED_CONFIG_ENABLE")) != 0);

  if (g_cfg_server_ip && g_cfg_server_port) {
    const char *val = config_get("SERVER_ADDRESS");
    if (val && val[0]) {
      char buf[128];
      strncpy(buf, val, sizeof(buf) - 1);
      buf[sizeof(buf) - 1] = '\0';
      char *colon = strchr(buf, ':');
      if (colon) {
        *colon = '\0';
        gtk_editable_set_text(g_cfg_server_ip, buf);
        gtk_editable_set_text(g_cfg_server_port, colon + 1);
      } else {
        gtk_editable_set_text(g_cfg_server_ip, buf);
      }
    }
  }

  config_ignoring_changes = FALSE;
  set_config_dirty(FALSE);
}

static void config_bind(GtkBuilder *builder) {
  const char *val;
  GObject *w;

  config_ignoring_changes = TRUE;

  /* Dropdown: DAEMON_STATE */
  w = gtk_builder_get_object(builder, "cfg_daemon_state");
  if (w) {
    g_cfg_daemon_state = GTK_DROP_DOWN(w);
    val = config_get("DAEMON_STATE");
    gtk_drop_down_set_selected(GTK_DROP_DOWN(w),
                               find_dropdown_idx(daemon_state_vals,
                                                 ARRAY_SIZE(daemon_state_vals),
                                                 val));
    g_signal_connect(w, "notify::selected", G_CALLBACK(on_cfg_dropdown_changed),
                     &dd_daemon_state);
  }

  /* Dropdown: FALLBACK_PROFILE */
  w = gtk_builder_get_object(builder, "cfg_fallback_profile");
  if (w) {
    g_cfg_fallback_profile = GTK_DROP_DOWN(w);
    val = config_get("FALLBACK_PROFILE");
    gtk_drop_down_set_selected(
        GTK_DROP_DOWN(w),
        find_dropdown_idx(fallback_vals, ARRAY_SIZE(fallback_vals), val));
    g_signal_connect(w, "notify::selected", G_CALLBACK(on_cfg_dropdown_changed),
                     &dd_fallback);
  }

  /* Scale: POLLING_INTERVAL */
  w = gtk_builder_get_object(builder, "cfg_polling_interval");
  if (w) {
    g_cfg_polling_interval = GTK_RANGE(w);
    val = config_get("POLLING_INTERVAL");
    gtk_range_set_value(GTK_RANGE(w), atof(val));
    g_signal_connect(w, "value-changed", G_CALLBACK(on_cfg_scale_changed),
                     (gpointer) "POLLING_INTERVAL");
  }

  /* Scale: LOAD_THRESHOLD */
  w = gtk_builder_get_object(builder, "cfg_load_threshold");
  if (w) {
    g_cfg_load_threshold = GTK_RANGE(w);
    val = config_get("LOAD_THRESHOLD");
    gtk_range_set_value(GTK_RANGE(w), atof(val));
    g_signal_connect(w, "value-changed", G_CALLBACK(on_cfg_scale_changed),
                     (gpointer) "LOAD_THRESHOLD");
  }

  /* Dropdown: DETECTION_LEVEL (1=Strict idx0, 2=Loose idx1) */
  w = gtk_builder_get_object(builder, "cfg_detection_level");
  if (w) {
    g_cfg_detection_level = GTK_DROP_DOWN(w);
    val = config_get("DETECTION_LEVEL");
    gtk_drop_down_set_selected(GTK_DROP_DOWN(w), (guint)(atoi(val) - 1));
    g_signal_connect(w, "notify::selected", G_CALLBACK(on_cfg_dropdown_changed),
                     &dd_detection);
  }

  /* Switch: EBPF_ENABLE */
  w = gtk_builder_get_object(builder, "cfg_ebpf_enable");
  if (w) {
    g_cfg_ebpf_enable = GTK_SWITCH(w);
    val = config_get("EBPF_ENABLE");
    gtk_switch_set_active(GTK_SWITCH(w), atoi(val) != 0);
    g_signal_connect(w, "notify::active", G_CALLBACK(on_cfg_switch_changed),
                     (gpointer) "EBPF_ENABLE");
  }

  /* Dropdown: AFFINITY_LEVEL */
  w = gtk_builder_get_object(builder, "cfg_affinity_level");
  if (w) {
    val = config_get("AFFINITY_LEVEL");
    gtk_drop_down_set_selected(GTK_DROP_DOWN(w), (guint)atoi(val));
    g_signal_connect(w, "notify::selected", G_CALLBACK(on_cfg_dropdown_changed),
                     &dd_affinity);
  }

  /* Switch: ADVANCED_CONFIG_ENABLE (controls advanced tab visibility) */
  w = gtk_builder_get_object(builder, "cfg_advanced_enable");
  if (w) {
    g_cfg_advanced_enable = GTK_SWITCH(w);
    val = config_get("ADVANCED_CONFIG_ENABLE");
    gboolean adv = atoi(val) != 0;
    gtk_switch_set_active(GTK_SWITCH(w), adv);
    /* Use the real visibility-toggling handler, not the generic dirty-bit one */
    g_signal_connect(w, "notify::active",
                     G_CALLBACK(on_advanced_enable_toggled), NULL);
    gtk_widget_set_visible(row_advanced, adv);
  }

  /* Switch: DEV_ENABLE (also controls dev tab visibility) */
  w = gtk_builder_get_object(builder, "cfg_dev_enable");
  if (w) {
    g_cfg_dev_enable = GTK_SWITCH(w);
    val = config_get("DEV_ENABLE");
    gboolean dev = atoi(val) != 0;
    gtk_switch_set_active(GTK_SWITCH(w), dev);
    /* Use the real visibility-toggling handler, not the generic dirty-bit one */
    g_signal_connect(w, "notify::active", G_CALLBACK(on_dev_enable_toggled),
                     NULL);
    /* Set initial visibility */
    gtk_widget_set_visible(row_developer, dev);
    gtk_widget_set_visible(row_debug, dev);
  }


  /* Switch: DEBUG_ENABLE */
  w = gtk_builder_get_object(builder, "cfg_debug_enable");
  if (w) {
    g_cfg_debug_enable = GTK_SWITCH(w);
    val = config_get("DEBUG_ENABLE");
    gtk_switch_set_active(GTK_SWITCH(w), atoi(val) != 0);
    g_signal_connect(w, "notify::active", G_CALLBACK(on_cfg_switch_changed),
                     (gpointer) "DEBUG_ENABLE");
  }

  /* ── Developer tab: Clang-Tidy switches ── */
  struct {
    const char *id;
    const char *key;
  } lint_switches[] = {
      {"cfg_lint_clang_diagnostic", "LINT_CLANG_DIAGNOSTIC"},
      {"cfg_lint_clang_bugprone", "LINT_CLANG_BUGPRONE"},
      {"cfg_lint_clang_modernize", "LINT_CLANG_MODERNIZE"},
      {"cfg_lint_clang_readability", "LINT_CLANG_READABILITY"},
      {"cfg_lint_clang_performance", "LINT_CLANG_PERFORMANCE"},
      {"cfg_lint_clang_portability", "LINT_CLANG_PORTABILITY"},
      {"cfg_lint_clang_analyzer", "LINT_CLANG_ANALYZER"},
      {"cfg_lint_cppcheck_all", "LINT_CPPCHECK_ALL"},
      {"cfg_lint_cppcheck_warning", "LINT_CPPCHECK_WARNING"},
      {"cfg_lint_cppcheck_style", "LINT_CPPCHECK_STYLE"},
      {"cfg_lint_cppcheck_performance", "LINT_CPPCHECK_PERFORMANCE"},
      {"cfg_lint_cppcheck_portability", "LINT_CPPCHECK_PORTABILITY"},
      {"cfg_lint_cppcheck_information", "LINT_CPPCHECK_INFORMATION"},
      {"cfg_lint_cppcheck_unused", "LINT_CPPCHECK_UNUSED"},
      {"cfg_lint_valgrind_origins", "LINT_VALGRIND_ORIGINS"},
      {NULL, NULL}};

  for (int i = 0; lint_switches[i].id; i++) {
    w = gtk_builder_get_object(builder, lint_switches[i].id);
    if (w) {
      val = config_get(lint_switches[i].key);
      gtk_switch_set_active(GTK_SWITCH(w), strcmp(val, "enabled") == 0);
      g_signal_connect(w, "notify::active", G_CALLBACK(on_cfg_switch_enabled),
                       (gpointer)lint_switches[i].key);
    }
  }

  /* Dropdown: VALGRIND_MODE */
  w = gtk_builder_get_object(builder, "cfg_lint_valgrind_mode");
  if (w) {
    val = config_get("LINT_VALGRIND_MODE");
    gtk_drop_down_set_selected(GTK_DROP_DOWN(w),
                               find_dropdown_idx(valgrind_mode_vals,
                                                 ARRAY_SIZE(valgrind_mode_vals),
                                                 val));
    g_signal_connect(w, "notify::selected", G_CALLBACK(on_cfg_dropdown_changed),
                     &dd_valgrind_m);
  }

  /* Dropdown: VALGRIND_KINDS */
  w = gtk_builder_get_object(builder, "cfg_lint_valgrind_kinds");
  if (w) {
    val = config_get("LINT_VALGRIND_KINDS");
    gtk_drop_down_set_selected(
        GTK_DROP_DOWN(w),
        find_dropdown_idx(valgrind_kinds_vals, ARRAY_SIZE(valgrind_kinds_vals),
                          val));
    g_signal_connect(w, "notify::selected", G_CALLBACK(on_cfg_dropdown_changed),
                     &dd_valgrind_k);
  }

  /* ── System Journal tab: Buttons & Settings ── */
  w = gtk_builder_get_object(builder, "btn_launch_debug");
  if (w)
    g_signal_connect(w, "clicked", G_CALLBACK(on_btn_launch_debug_clicked),
                     NULL);

  w = gtk_builder_get_object(builder, "btn_analyze_coredump");
  if (w)
    g_signal_connect(w, "clicked", G_CALLBACK(on_btn_analyze_coredump_clicked),
                     NULL);

  w = gtk_builder_get_object(builder, "btn_archive_journal");
  if (w)
    g_signal_connect(w, "clicked", G_CALLBACK(on_btn_archive_journal_clicked),
                     NULL);

  w = gtk_builder_get_object(builder, "btn_rotate_journal");
  if (w)
    g_signal_connect(w, "clicked", G_CALLBACK(on_btn_rotate_journal_clicked),
                     NULL);

  /* SpinButton: JOURNAL_KEEP */
  w = gtk_builder_get_object(builder, "cfg_journal_keep");
  if (w) {
    g_cfg_journal_keep = GTK_SPIN_BUTTON(w);
    val = config_get("JOURNAL_KEEP");
    if (val && val[0])
      gtk_spin_button_set_value(g_cfg_journal_keep, atoi(val));
  }

  /* Dropdown: JOURNAL_MAX_MB */
  w = gtk_builder_get_object(builder, "cfg_journal_max_mb");
  if (w) {
    g_cfg_journal_max_mb = GTK_DROP_DOWN(w);
    val = config_get("JOURNAL_MAX_MB");
    gtk_drop_down_set_selected(
        g_cfg_journal_max_mb,
        find_dropdown_idx(journal_max_mb_vals, ARRAY_SIZE(journal_max_mb_vals),
                          val));
  }

  /* Rotation Settings Apply/Cancel */
  w = gtk_builder_get_object(builder, "btn_rotation_apply");
  if (w)
    g_signal_connect(w, "clicked", G_CALLBACK(on_rotation_apply_clicked), NULL);
  w = gtk_builder_get_object(builder, "btn_rotation_cancel");
  if (w)
    g_signal_connect(w, "clicked", G_CALLBACK(on_rotation_cancel_clicked),
                     NULL);

  /* Entry: SERVER_ADDRESS */
  w = gtk_builder_get_object(builder, "cfg_server_ip");
  GObject *w2 = gtk_builder_get_object(builder, "cfg_server_port");
  if (w && w2) {
    g_cfg_server_ip = GTK_EDITABLE(w);
    g_cfg_server_port = GTK_EDITABLE(w2);
    val = config_get("SERVER_ADDRESS");
    if (val && val[0]) {
      char buf[128] = {0};
      strncpy(buf, val, sizeof(buf) - 1);
      buf[sizeof(buf) - 1] = '\0';
      char *colon = strchr(buf, ':');
      if (colon) {
        *colon = '\0';
        gtk_editable_set_text(g_cfg_server_ip, buf);
        gtk_editable_set_text(g_cfg_server_port, colon + 1);
      } else {
        gtk_editable_set_text(g_cfg_server_ip, buf);
      }
    }
    g_signal_connect(w, "changed", G_CALLBACK(on_server_address_changed), NULL);
    g_signal_connect(w2, "changed", G_CALLBACK(on_server_address_changed),
                     NULL);
  }

  /* Config Apply/Cancel */
  w = gtk_builder_get_object(builder, "btn_cfg_apply");
  if (w) {
    btn_cfg_apply = GTK_WIDGET(w);
    gtk_widget_set_sensitive(btn_cfg_apply, FALSE);
    g_signal_connect(w, "clicked", G_CALLBACK(on_cfg_apply_clicked), builder);
  }
  w = gtk_builder_get_object(builder, "btn_cfg_cancel");
  if (w) {
    btn_cfg_cancel = GTK_WIDGET(w);
    gtk_widget_set_sensitive(btn_cfg_cancel, FALSE);
    g_signal_connect(w, "clicked", G_CALLBACK(on_cfg_cancel_clicked), builder);
  }

  config_ignoring_changes = FALSE;
  set_config_dirty(FALSE);
}

/* ── Application Activate ─────────────────────────────────────── */

static void update_extras_view(GtkTextView *text_view, guint index) {
  const char *files[] = {"/org/x3d-toggle/gui/README.md",
                         "/org/x3d-toggle/gui/ARCHITECTURE.md",
                         "/org/x3d-toggle/gui/x3d-toggle.1.md"};

  GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);

  if (index == 3) {
    extern const char *const insults[];
    extern const int insults_count;

    GString *str =
        g_string_new("📜 X3D Toggle - The Great Book of Insults 📜\n\n");
    for (int i = 0; i < insults_count; i++) {
      g_string_append_printf(str, "• %s\n", insults[i]);
    }

    gtk_text_buffer_set_text(buffer, str->str, -1);
    g_string_free(str, TRUE);
    return;
  }

  if (index >= (sizeof(files) / sizeof(files[0])))
    return;

  GError *err = NULL;
  GBytes *bytes = g_resources_lookup_data(files[index], 0, &err);

  if (bytes) {
    const gchar *data = g_bytes_get_data(bytes, NULL);
    gsize size = g_bytes_get_size(bytes);
    gtk_text_buffer_set_text(buffer, data, size);
    g_bytes_unref(bytes);
  } else {
    if (err) {
      gtk_text_buffer_set_text(buffer, err->message, -1);
      g_error_free(err);
    } else {
      gtk_text_buffer_set_text(buffer, "Error loading document.", -1);
    }
  }
}

static void on_extras_dropdown_selected(GObject *dropdown, GParamSpec *pspec,
                                        gpointer user_data) {
  (void)pspec;
  GtkTextView *text_view = GTK_TEXT_VIEW(user_data);
  guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(dropdown));
  update_extras_view(text_view, selected);
}

static void on_app_activate(GtkApplication *app, gpointer user_data) {
  (void)user_data;
  GObject *w;

  gtk_window_set_default_icon_name("x3d-toggle");

  /* Follow the system color scheme (light/dark/default) via the XDG
   * settings portal. Must be called after GTK is initialized (i.e. inside
   * the activate signal), not before g_application_run(). */
  adw_style_manager_set_color_scheme(adw_style_manager_get_default(),
                                     ADW_COLOR_SCHEME_DEFAULT);

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

  g_sidebar = sidebar;
  g_stack = stack;

  GObject *dropdown_extras = gtk_builder_get_object(builder, "dropdown_extras");
  GObject *txt_extras_view = gtk_builder_get_object(builder, "txt_extras_view");
  g_signal_connect(dropdown_extras, "notify::selected",
                   G_CALLBACK(on_extras_dropdown_selected), txt_extras_view);
  update_extras_view(GTK_TEXT_VIEW(txt_extras_view), 0);

  add_nav_row(sidebar, "dashboard", "Dashboard");
  add_nav_row(sidebar, "modes", "Modes");

  row_affinity = add_nav_row(sidebar, "affinity", "Threaded Affinity Settings");

  add_nav_row(sidebar, "configuration", "Configuration");
  row_advanced = add_nav_row(sidebar, "advanced", "Advanced Configuration");
  row_developer = add_nav_row(sidebar, "developer", "Developer Options");
  row_debug = add_nav_row(sidebar, "debug", "System Journal");
  add_nav_row(sidebar, "extras", "Extras");

  /* Dev tabs hidden by default */
  gtk_widget_set_visible(row_developer, FALSE);
  gtk_widget_set_visible(row_debug, FALSE);
  gtk_widget_set_visible(row_advanced, FALSE);

  g_signal_connect(sidebar, "row-selected", G_CALLBACK(on_nav_row_selected),
                   stack);

  /* Select Dashboard by default */
  GtkListBoxRow *first_row = gtk_list_box_get_row_at_index(sidebar, 0);
  if (first_row) {
    gtk_list_box_select_row(sidebar, first_row);
    g_signal_emit_by_name(sidebar, "row-selected", first_row);
  }

  /* Bind Mode Selection */
  const char *mode_ids[] = {"cache", "frequency", "dual", "default", "reset"};
  for (int i = 0; i < 5; i++) {
    char btn_id[64];
    printf_sn(btn_id, sizeof(btn_id), "mode_card_%s", mode_ids[i]);
    mode_btns[i] = GTK_WIDGET(gtk_builder_get_object(builder, btn_id));
    if (mode_btns[i]) {
      gtk_widget_set_name(mode_btns[i], btn_id);
      g_signal_connect(mode_btns[i], "clicked",
                       G_CALLBACK(on_mode_card_clicked), (gpointer)mode_ids[i]);
    }
  }

  w = (GObject *)gtk_builder_get_object(builder, "btn_mode_apply");
  if (w)
    g_signal_connect(w, "clicked", G_CALLBACK(on_mode_apply_clicked), NULL);

  w = (GObject *)gtk_builder_get_object(builder, "btn_mode_cancel");
  if (w)
    g_signal_connect(w, "clicked", G_CALLBACK(on_mode_cancel_clicked), NULL);

  /* Affinity Tab Bindings */
  w = gtk_builder_get_object(builder, "grid_topology");
  if (w)
    g_grid_topology = GTK_GRID(w);

  w = gtk_builder_get_object(builder, "btn_affinity_apply");
  if (w)
    g_signal_connect(w, "clicked", G_CALLBACK(on_affinity_apply_clicked), NULL);

  w = gtk_builder_get_object(builder, "btn_affinity_cancel");
  if (w)
    g_signal_connect(w, "clicked", G_CALLBACK(on_affinity_cancel_clicked),
                     NULL);

  w = gtk_builder_get_object(builder, "cfg_affinity_level");
  if (w) {
    g_cfg_affinity_level = GTK_DROP_DOWN(w);
    const char *val = config_get("AFFINITY_LEVEL");
    gtk_drop_down_set_selected(g_cfg_affinity_level,
                               (guint)atoi(val ? val : "0"));
  }

  /* Bind Lifecycle Dropdown */
  g_lifecycle_dropdown =
      GTK_DROP_DOWN(gtk_builder_get_object(builder, "lifecycle_dropdown"));

  /* Bind config widgets */
  config_bind(builder);

  g_object_unref(builder);
  gtk_window_present(window);

  /* Start Live Dashboard Polling */
  g_timeout_add_seconds(1, update_dashboard_cb, NULL);
  update_dashboard_cb(NULL); /* Initial fetch */
}

int main(int argc, char **argv) {
  g_set_prgname("x3d-toggle");
  AdwApplication *app =
      adw_application_new("org.x3d.toggle", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(on_app_activate), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
