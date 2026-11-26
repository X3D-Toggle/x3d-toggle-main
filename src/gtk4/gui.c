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
#include <dirent.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <stdio.h>

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

static GtkWidget *lbl_status_daemon = NULL;
static GtkWidget *lbl_status_hardware = NULL;
static GtkWidget *lbl_status_system = NULL;
static GtkEditable *g_cfg_server_address = NULL;
static GtkEditable *g_cfg_server_port = NULL;
static GtkDropDown *g_cfg_server_ssh = NULL;
static GtkSpinButton *g_cfg_journal_keep = NULL;
static GtkDropDown *g_cfg_journal_max_mb = NULL;

/* Dev mode nav rows (hidden by default) */
static GtkWidget *row_developer = NULL;
static GtkWidget *row_debug = NULL;
static GtkWidget *row_advanced = NULL;
static GtkListBox *g_sidebar = NULL;
static GtkStack *g_stack = NULL;
static GtkDropDown *g_lifecycle_dropdown = NULL;
static GtkDropDown *g_cfg_affinity_level = NULL;
static GtkGrid *g_grid_topology = NULL;
static GtkWidget *btn_cfg_apply = NULL;
static GtkWidget *btn_cfg_cancel = NULL;
static GtkDropDown *g_cfg_daemon_state = NULL;
static gboolean config_ignoring_changes = FALSE;
static GtkDropDown *g_cfg_fallback_profile = NULL;
static GtkRange *g_cfg_polling_interval = NULL;
static GtkRange *g_cfg_load_threshold = NULL;
static GtkDropDown *g_cfg_detection_level = NULL;
static GtkSwitch *g_cfg_ebpf_enable = NULL;
static GtkSwitch *g_cfg_dev_enable = NULL;
static GtkSwitch *g_cfg_advanced_enable = NULL;
static GtkSwitch *g_cfg_force_unsupported = NULL;

/* Advanced Configuration Widgets */
static GtkSwitch *g_cfg_sched_bore = NULL;
static GtkSpinButton *g_cfg_sched_bit_shift = NULL;
static GtkSwitch *g_cfg_sched_burst_fork = NULL;
static GtkSpinButton *g_cfg_sched_slice_us = NULL;
static GtkEditable *g_cfg_vm_max_map = NULL;
static GtkSwitch *g_cfg_split_lock_detect = NULL;
static GtkSwitch *g_cfg_nmi_watchdog = NULL;
static GtkDropDown *g_cfg_thp_mode = NULL;

/* Networking Configuration Widgets */
static GtkEditable *g_cfg_net_qdisc = NULL;
static GtkSpinButton *g_cfg_net_fastopen = NULL;
static GtkSpinButton *g_cfg_net_rp_filter = NULL;
static GtkSwitch *g_cfg_net_source_route = NULL;

static GtkWidget *row_sched_bore = NULL;
static GtkWidget *row_sched_bit_shift = NULL;
static GtkWidget *row_sched_burst_fork = NULL;
static GtkWidget *row_split_lock = NULL;
static GtkWidget *row_nmi_watchdog = NULL;
static GtkWidget *row_thp_mode = NULL;

static GtkSwitch *g_cfg_irq_enable = NULL;
static GtkSwitch *g_cfg_irq_gpu = NULL;
static GtkSwitch *g_cfg_irq_nvme = NULL;
static GtkSwitch *g_cfg_irq_usb = NULL;
static GtkSwitch *g_cfg_irq_nic = NULL;
static GtkSwitch *g_cfg_irq_audio = NULL;
static GtkSwitch *g_cfg_irq_coalesce = NULL;
static GtkSwitch *g_cfg_irq_watch = NULL;
static void set_config_dirty(gboolean dirty) {
  if (btn_cfg_apply)
    gtk_widget_set_sensitive(btn_cfg_apply, dirty);
  if (btn_cfg_cancel)
    gtk_widget_set_sensitive(btn_cfg_cancel, dirty);
}

static const char *g_selected_mode = NULL;
static GtkWidget *mode_btns[5]; /* Cache, Frequency, Dual, Default, Reset */

/* GPU Subpage State */
typedef struct {
  char hwmon_path[512];
  char card_name[32];
  char pci_slot[64];
  char label[160];
  gboolean is_discrete;
} AmdGpuInfo;

static GtkDropDown *g_gpu_dropdown = NULL;
static GtkWidget *lbl_gpu_telemetry = NULL;
static GtkWidget *lbl_gpu_device_meta = NULL;
static GtkWidget *row_gpu_lact = NULL;
static GtkWidget *btn_launch_lact = NULL;
static GtkWidget *row_gpu_corectrl = NULL;
static GtkWidget *btn_launch_corectrl = NULL;
static GtkWidget *lbl_gpu_control_empty = NULL;
static AmdGpuInfo *g_gpus = NULL;
static guint g_gpu_count = 0;
static guint g_gpu_timer_id = 0;
static gboolean g_gpu_updating_dropdown = FALSE;

/* ── Dashboard Polling ────────────────────────────────────────── */

static int daemon_retry_cooldown = 0;

static void status_sysfs(const char *path, char *out, size_t max_len) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    printf_sn(out, max_len, "N/A");
    return;
  }
  ssize_t n = read(fd, out, max_len - 1);
  if (n > 0) {
    out[n] = '\0';
    for (int i = 0; i < n; i++)
      if (out[i] == '\n' || out[i] == '\r')
        out[i] = '\0';
  } else {
    printf_sn(out, max_len, "N/A");
  }
  close(fd);
}

static void status_upper(char *str) {
  for (int idx = 0; str[idx]; idx++) {
    str[idx] = toupper((unsigned char)str[idx]);
  }
}

static gboolean update_dashboard_cb(gpointer user_data) {
  (void)user_data;
  if (!lbl_status_daemon || !lbl_status_hardware || !lbl_status_system)
    return G_SOURCE_CONTINUE;

  char info[BUFF_INFO] = {0};
  int socket_ok = (socket_send("DAEMON_INFO", info, sizeof(info)) == 0);

  char ipc_status[64] = "SOCKET OFFLINE";
  if (socket_ok) {
    printf_sn(ipc_status, sizeof(ipc_status), "SOCKET ONLINE");
    daemon_retry_cooldown = 0;
  } else {
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
  }

  char daemon_state[64] = "STOPPED";
  char ebpf_status[64] = "INACTIVE";
  char sched_mode[64] = "INACTIVE (MANUAL OVERRIDE)";
  char ccd_state[64] = "AFFINITY PINNED";
  int override_val = 0;
  int affinity_active = 0;

  if (socket_ok) {
    char *st_val = strstr(info, "STATE=");
    const char *ov_val = strstr(info, "OVERRIDE=");
    const char *ba_val = strstr(info, "BPF_ACTIVE=");
    const char *mk = strstr(info, "MASK=");

    if (st_val) {
      printf_sn(daemon_state, sizeof(daemon_state), "%s", st_val + 6);
      char *sc = strchr(daemon_state, ';');
      if (!sc)
        sc = strchr(daemon_state, '|');
      if (sc)
        *sc = '\0';
    }
    if (ov_val)
      override_val = atoi(ov_val + 9);
    if (mk && strncmp(mk + 5, "none", 4) != 0)
      affinity_active = 1;
    if (ba_val) {
      printf_sn(ebpf_status, sizeof(ebpf_status), "%s",
                (atoi(ba_val + 11) ? "HEALTHY" : "POLLING (FALLBACK)"));
    }

    status_upper(daemon_state);
    if (strcmp(daemon_state, "DEFAULT") == 0) {
      printf_sn(daemon_state, sizeof(daemon_state), "ACTIVE");
      printf_sn(sched_mode, sizeof(sched_mode), "ACTIVE");
    } else if (strcmp(daemon_state, "HARD_RESET") == 0 ||
               strcmp(daemon_state, "AUTO") == 0) {
      printf_sn(daemon_state, sizeof(daemon_state), "CPPC NATIVE");
    } else {
      printf_sn(daemon_state, sizeof(daemon_state), "SUSPENDED");
      printf_sn(ebpf_status, sizeof(ebpf_status), "INACTIVE (MANUAL OVERRIDE)");
    }

    /* Capability Discovery Logic */
    const char *caps_val = strstr(info, "CAPS=");
    if (caps_val) {
      uint32_t caps = (uint32_t)strtoul(caps_val + 5, NULL, 16);
      gboolean force = g_cfg_force_unsupported
                           ? gtk_switch_get_active(g_cfg_force_unsupported)
                           : FALSE;

      if (row_sched_bore)
        gtk_widget_set_visible(row_sched_bore, force || (caps & CAP_BORE));
      if (row_sched_bit_shift)
        gtk_widget_set_visible(row_sched_bit_shift, force || (caps & CAP_BORE));
      if (row_sched_burst_fork)
        gtk_widget_set_visible(row_sched_burst_fork,
                               force || (caps & CAP_BORE));
      if (row_split_lock)
        gtk_widget_set_visible(row_split_lock,
                               force || (caps & CAP_SPLIT_LOCK));
      if (row_nmi_watchdog)
        gtk_widget_set_visible(row_nmi_watchdog,
                               force || (caps & CAP_NMI_WATCHDOG));
      if (row_thp_mode)
        gtk_widget_set_visible(row_thp_mode, force || (caps & CAP_THP));
    }
  }

  /* Fetch v-Cache mode via MODE IPC */
  char mode_str[BUFF_STATE] = "N/A";
  if (socket_ok && socket_send("MODE", mode_str, sizeof(mode_str)) == 0) {
    status_upper(mode_str);
  }

  /* CCD States */
  char ccd0_st[32] = "ONLINE";
  char ccd1_st[32] = "ONLINE";
  char path[256];
  char b0[8] = "1";
  printf_sn(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/online", 0);
  int fd = open(path, O_RDONLY);
  if (fd >= 0) {
    if (read(fd, b0, sizeof(b0) - 1) > 0 && b0[0] == '0')
      strcpy(ccd0_st, "PARKED");
    close(fd);
  }
#ifndef CCD1_START
#define CCD1_START 8
#endif
#ifndef CCD_COUNT
#define CCD_COUNT 2
#endif
  if (CCD_COUNT >= 2 && CCD1_START > 0) {
    printf_sn(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/online",
              CCD1_START);
    b0[0] = '1';
    fd = open(path, O_RDONLY);
    if (fd >= 0) {
      if (read(fd, b0, sizeof(b0) - 1) > 0 && b0[0] == '0')
        strcpy(ccd1_st, "PARKED");
      close(fd);
    }
  } else {
    strcpy(ccd1_st, "SINGLE-CCD");
  }

  if (affinity_active) {
    strcpy(ccd_state, "AFFINITY PINNED");
  } else if (strcmp(ccd1_st, "ONLINE") == 0) {
    strcpy(ccd_state, (override_val == 4) ? "INVERTED" : "FULL THROUGHPUT");
  } else {
    strcpy(ccd_state, "CCD ISOLATED");
  }

  char d_buff[64], epp[64], gov[64], smt[64], plat[64], bst_raw[64];
  status_sysfs("/sys/devices/system/cpu/amd_pstate/status", d_buff,
               sizeof(d_buff));
  status_sysfs(
      "/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference", epp,
      sizeof(epp));
  status_sysfs("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", gov,
               sizeof(gov));
  status_sysfs("/sys/devices/system/cpu/smt/control", smt, sizeof(smt));
  status_sysfs("/sys/firmware/acpi/platform_profile", plat, sizeof(plat));
  status_sysfs("/sys/devices/system/cpu/cpufreq/boost", bst_raw,
               sizeof(bst_raw));
  char boost_str[64];
  printf_sn(boost_str, sizeof(boost_str), "%s",
            (strcmp(bst_raw, "1") == 0) ? "ENABLED" : "DISABLED");

  status_upper(d_buff);
  status_upper(epp);
  status_upper(gov);
  status_upper(smt);
  status_upper(plat);

  char display_daemon[BUFF_LINE * 2] = {0};
  char display_hardware[BUFF_LINE * 2] = {0};
  char display_system[BUFF_LINE * 2] = {0};

  printf_sn(display_daemon, sizeof(display_daemon),
            "<span font_family=\"monospace\">"
            "  <b>Daemon Status:</b>  %s\n"
            "  <b>IPC Status:</b>     %s\n"
            "  <b>eBPF Health:</b>    %s\n"
            "  <b>Scheduler Mode:</b> %s"
            "</span>",
            daemon_state, ipc_status, ebpf_status, sched_mode);

  printf_sn(display_hardware, sizeof(display_hardware),
            "<span font_family=\"monospace\">"
            "  <b>v-Cache Mode:</b>   %s\n"
            "  <b>Boost Mode:</b>     %s\n"
            "  <b>CCD State:</b>      %s\n"
            "  <b>CCD0 Status:</b>    %s\n"
            "  <b>CCD1 Status:</b>    %s"
            "</span>",
            mode_str, boost_str, ccd_state, ccd0_st, ccd1_st);

  printf_sn(display_system, sizeof(display_system),
            "<span font_family=\"monospace\">"
            "  <b>Driver Mode:</b>    %s\n"
            "  <b>EPP Profile:</b>    %s\n"
            "  <b>Governor:</b>       %s\n"
            "  <b>SMT Status:</b>     %s\n"
            "  <b>Platform:</b>       %s"
            "</span>",
            d_buff, epp, gov, smt, plat);

  gtk_label_set_markup(GTK_LABEL(lbl_status_daemon), display_daemon);
  gtk_label_set_markup(GTK_LABEL(lbl_status_hardware), display_hardware);
  gtk_label_set_markup(GTK_LABEL(lbl_status_system), display_system);
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

/* Scripts launched here run inside a transient terminal window. The scripts
 * themselves pause with "Press [ENTER] to close window..." when they see
 * X3D_GUI_TERM=1, so the GUI must not add a second prompt. */
static void gui_spawn_terminal(const char *cmd_str) {
  static const char *const terms[] = {
      "ptyxis", "kgx",  "gnome-terminal", "konsole", "alacritty",
      "kitty",  "foot", "xfce4-terminal", "xterm",   NULL};
  const char *found = NULL;
  gchar *found_path = NULL;
  for (int i = 0; terms[i] != NULL; i++) {
    found_path = g_find_program_in_path(terms[i]);
    if (found_path) {
      found = terms[i];
      break;
    }
  }
  if (!found) {
    return;
  }
  g_free(found_path);

  char *env[] = {(char *)"X3D_EXEC=1", (char *)"X3D_GUI_TERM=1", NULL};
  if (strcmp(found, "gnome-terminal") == 0 || strcmp(found, "ptyxis") == 0) {
    char *args[] = {(char *)found, (char *)"--",    (char *)"/bin/sh",
                    (char *)"-c",  (char *)cmd_str, NULL};
    gui_spawn(args, env);
  } else if (strcmp(found, "xfce4-terminal") == 0) {
    gchar *xfce_cmd = g_strdup_printf("/bin/sh -c \"%s\"", cmd_str);
    char *args[] = {(char *)found, (char *)"-e", xfce_cmd, NULL};
    gui_spawn(args, env);
    g_free(xfce_cmd);
  } else {
    char *args[] = {(char *)found, (char *)"-e",    (char *)"/bin/sh",
                    (char *)"-c",  (char *)cmd_str, NULL};
    gui_spawn(args, env);
  }
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
  /* pkexec scrubs the environment, so the GUI-terminal hint is an argument */
  gui_spawn_terminal(
      "pkexec /usr/lib/x3d-toggle/scripts/tools/debug.sh --gui-term");
}

static void on_btn_analyze_coredump_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  (void)data;
  gui_spawn_terminal("/usr/lib/x3d-toggle/scripts/tools/coredump.sh");
}

static void on_btn_lint_clang_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  (void)data;
  gui_spawn_terminal("/usr/lib/x3d-toggle/scripts/tools/linter.sh clang");
}

static void on_btn_lint_cppcheck_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  (void)data;
  gui_spawn_terminal("/usr/lib/x3d-toggle/scripts/tools/linter.sh cppc");
}

static void on_btn_lint_valgrind_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  (void)data;
  gui_spawn_terminal("/usr/lib/x3d-toggle/scripts/tools/linter.sh valgrind");
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
  (void)btn;
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

  /* 1. Execute Hardware Mode via CLI Dispatch */
  if (g_selected_mode) {
    char *args[] = {(char *)"/usr/bin/x3d-toggle", (char *)g_selected_mode,
                    NULL};
    gui_spawn(args, NULL);
  }

  /* 2. Execute Lifecycle Action via IPC / CLI */
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
    case 4: { /* Enable (Soft Wake) */
      char *args[] = {(char *)"/usr/bin/x3d-toggle", (char *)"enable", NULL};
      gui_spawn(args, NULL);
      break;
    }
    case 5: { /* Autostart (Hard Wake) */
      char *args[] = {(char *)"/usr/bin/x3d-toggle", (char *)"start", NULL};
      gui_spawn(args, NULL);
      break;
    }
    default:
      break;
    }
    if (idx != 0)
      gtk_drop_down_set_selected(g_lifecycle_dropdown, 0);
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
    const char *hyphen = strchr(p, '-');
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

static void refresh_topology(void) {
  if (!g_grid_topology)
    return;
  char mask[128] = {0};
  int ccd1_start = 0;
  int total_cores = 0;
  if (ccd(mask, &ccd1_start, &total_cores) != 0)
    return;

  /* Priority: Use user-defined persistence from daemon.conf if available */
  const char *saved_mask = config_get("AFFINITY_MASK");
  if (saved_mask[0] && strcmp(saved_mask, "none") != 0) {
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

  guint level = g_cfg_affinity_level
                    ? gtk_drop_down_get_selected(g_cfg_affinity_level)
                    : 2;

  char cache_mask[256] = {0};
  char freq_mask[256] = {0};

  if (level == 0) {
    /* Auto: clear all masks, let daemon decide */
    printf_sn(cache_mask, sizeof(cache_mask), "none");
    printf_sn(freq_mask, sizeof(freq_mask), "none");
  } else if (level == 1) {
    /* By Die: auto-compute CCD0/CCD1 split from topology */
    char hw_mask[128] = {0};
    int ccd1_start = 0, total_cores = 0;
    if (ccd(hw_mask, &ccd1_start, &total_cores) == 0 && ccd1_start > 0) {
      int cp = 0, fp = 0;
      for (int i = 0; i < total_cores; i++) {
        if (i < ccd1_start)
          cp += printf_sn(cache_mask + cp, sizeof(cache_mask) - cp, "%s%d",
                          (cp > 0) ? "," : "", i);
        else
          fp += printf_sn(freq_mask + fp, sizeof(freq_mask) - fp, "%s%d",
                          (fp > 0) ? "," : "", i);
      }
    }
  } else {
    /* Manual: read toggle buttons */
    int cache_pos = 0, freq_pos = 0;
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
  }

  char cmd[512];
  printf_sn(cmd, sizeof(cmd), "SET_AFFINITY_CONFIG %s %s",
            (cache_mask[0]) ? cache_mask : "none",
            (freq_mask[0]) ? freq_mask : "none");
  int ret = socket_send(cmd, NULL, 0);
  if (atoi(config_get("DEBUG_ENABLE"))) {
    journal_debug("GUI: Sent affinity config: %s (result=%d)", cmd, ret);
  }

  /* Brief visual confirmation */
  if (lbl_status_daemon) {
    gtk_label_set_label(GTK_LABEL(lbl_status_daemon),
                        "Affinity Settings Applied");
  }

  /* Persistence */
  config_send("AFFINITY_MASK", (cache_mask[0]) ? cache_mask : "none");
  config_send("AFFINITY_FREQ_MASK", (freq_mask[0]) ? freq_mask : "none");

  char lvl_str[4];
  printf_sn(lvl_str, sizeof(lvl_str), "%u", level);
  config_send("AFFINITY_LEVEL", lvl_str);
  btn_feedback(btn);
}

static void on_affinity_cancel_clicked(GtkButton *btn, gpointer user_data) {
  (void)btn;
  (void)user_data;

  /* Reset dropdown to saved config */
  if (g_cfg_affinity_level) {
    const char *val = config_get("AFFINITY_LEVEL");
    gtk_drop_down_set_selected(g_cfg_affinity_level,
                               (guint)atoi(val[0] ? val : "0"));
  }

  refresh_topology();
}

/* ── GPU Subpage: Multi-GPU Sysfs Discovery, Telemetry & Launchers ── */

static gboolean is_digits_only(const char *s) {
  if (!s || !s[0])
    return FALSE;
  for (size_t i = 0; s[i] != '\0'; i++) {
    if (s[i] < '0' || s[i] > '9')
      return FALSE;
  }
  return TRUE;
}

static gboolean gpu_read_file(const char *path, char *out, size_t out_sz) {
  if (!out || out_sz == 0)
    return FALSE;
  out[0] = '\0';
  int fd = open(path, O_RDONLY);
  if (fd < 0)
    return FALSE;
  ssize_t n = read(fd, out, out_sz - 1);
  close(fd);
  if (n <= 0) {
    out[0] = '\0';
    return FALSE;
  }
  out[n] = '\0';
  return TRUE;
}

static gboolean gpu_read_attr_ll(const char *hwmon_dir, const char *attr,
                                 long long *val_out) {
  char path[640];
  char buf[64];
  snprintf(path, sizeof(path), "%s/%s", hwmon_dir, attr);
  if (!gpu_read_file(path, buf, sizeof(buf)))
    return FALSE;
  char *endptr = NULL;
  errno = 0;
  long long v = strtoll(buf, &endptr, 10);
  if (endptr == buf || errno == ERANGE)
    return FALSE;
  *val_out = v;
  return TRUE;
}

static void gpu_read_pci_slot(const char *card_name, char *slot_out,
                              size_t slot_sz) {
  char path[512];
  char buf[1024];
  snprintf(path, sizeof(path), "/sys/class/drm/%s/device/uevent", card_name);
  if (!gpu_read_file(path, buf, sizeof(buf))) {
    snprintf(slot_out, slot_sz, "unknown");
    return;
  }
  const char *p = buf;
  while (*p) {
    const char *nl = strchr(p, '\n');
    size_t len = nl ? (size_t)(nl - p) : strlen(p);
    if (len > 14 && strncmp(p, "PCI_SLOT_NAME=", 14) == 0) {
      size_t slen = len - 14;
      if (p[14 + slen - 1] == '\r')
        slen--;
      if (slen >= slot_sz)
        slen = slot_sz - 1;
      memcpy(slot_out, p + 14, slen);
      slot_out[slen] = '\0';
      return;
    }
    if (!nl)
      break;
    p = nl + 1;
  }
  snprintf(slot_out, slot_sz, "unknown");
}

static int cmp_amd_gpu_info(const void *a, const void *b) {
  const AmdGpuInfo *ga = (const AmdGpuInfo *)a;
  const AmdGpuInfo *gb = (const AmdGpuInfo *)b;
  int ca = atoi(ga->card_name + 4);
  int cb = atoi(gb->card_name + 4);
  if (ca != cb)
    return (ca < cb) ? -1 : 1;
  return strcmp(ga->hwmon_path, gb->hwmon_path);
}

static void gpu_deactivate(void) {
  if (g_gpu_timer_id != 0) {
    g_source_remove(g_gpu_timer_id);
    g_gpu_timer_id = 0;
  }
  g_free(g_gpus);
  g_gpus = NULL;
  g_gpu_count = 0;
}

static void gpu_refresh_telemetry(void) {
  if (!lbl_gpu_telemetry)
    return;

  if (!g_gpus || g_gpu_count == 0) {
    if (lbl_gpu_device_meta)
      gtk_widget_set_visible(lbl_gpu_device_meta, FALSE);
    gtk_label_set_text(GTK_LABEL(lbl_gpu_telemetry),
                       "  No supported AMD GPU detected");
    return;
  }

  guint idx = g_gpu_dropdown ? gtk_drop_down_get_selected(g_gpu_dropdown) : 0;
  if (idx == GTK_INVALID_LIST_POSITION || idx >= g_gpu_count)
    idx = 0;

  const AmdGpuInfo *gpu = &g_gpus[idx];

  if (lbl_gpu_device_meta) {
    char meta[192];
    snprintf(meta, sizeof(meta), "%s  •  PCI %s", gpu->card_name,
             gpu->pci_slot);
    gtk_label_set_text(GTK_LABEL(lbl_gpu_device_meta), meta);
    gtk_widget_set_visible(lbl_gpu_device_meta, TRUE);
  }

  char edge_str[32] = "N/A";
  char junc_str[32] = "N/A";
  char mem_str[32] = "N/A";
  char ppt_str[32] = "N/A";
  char cap_str[32] = "N/A";
  char fan_str[64] = "N/A";
  long long val = 0;

  if (gpu_read_attr_ll(gpu->hwmon_path, "temp1_input", &val))
    snprintf(edge_str, sizeof(edge_str), "%.1f °C", (double)val / 1000.0);

  if (gpu_read_attr_ll(gpu->hwmon_path, "temp2_input", &val))
    snprintf(junc_str, sizeof(junc_str), "%.1f °C", (double)val / 1000.0);

  if (gpu_read_attr_ll(gpu->hwmon_path, "temp3_input", &val))
    snprintf(mem_str, sizeof(mem_str), "%.1f °C", (double)val / 1000.0);

  if (gpu_read_attr_ll(gpu->hwmon_path, "power1_average", &val) ||
      gpu_read_attr_ll(gpu->hwmon_path, "power1_input", &val)) {
    snprintf(ppt_str, sizeof(ppt_str), "%.1f W", (double)val / 1000000.0);
  }

  if (gpu_read_attr_ll(gpu->hwmon_path, "power1_cap", &val))
    snprintf(cap_str, sizeof(cap_str), "%.1f W", (double)val / 1000000.0);

  long long fan_rpm = 0;
  long long pwm_raw = 0;
  gboolean has_fan = gpu_read_attr_ll(gpu->hwmon_path, "fan1_input", &fan_rpm);
  gboolean has_pwm = gpu_read_attr_ll(gpu->hwmon_path, "pwm1", &pwm_raw);

  if (has_fan && has_pwm) {
    long long pct = (pwm_raw * 100 + 127) / 255;
    if (pct < 0)
      pct = 0;
    if (pct > 100)
      pct = 100;
    snprintf(fan_str, sizeof(fan_str), "%lld RPM (%lld%%)", fan_rpm, pct);
  } else if (has_fan) {
    snprintf(fan_str, sizeof(fan_str), "%lld RPM", fan_rpm);
  } else if (has_pwm) {
    long long pct = (pwm_raw * 100 + 127) / 255;
    if (pct < 0)
      pct = 0;
    if (pct > 100)
      pct = 100;
    snprintf(fan_str, sizeof(fan_str), "%lld%%", pct);
  }

  gchar *markup = g_markup_printf_escaped("<span font_family=\"monospace\">"
                                          "  <b>Edge Temp:</b>      %s\n"
                                          "  <b>Junction Temp:</b>  %s\n"
                                          "  <b>Memory Temp:</b>    %s\n"
                                          "  <b>Power Draw:</b>     %s\n"
                                          "  <b>Power Cap:</b>      %s\n"
                                          "  <b>Fan Speed:</b>      %s"
                                          "</span>",
                                          edge_str, junc_str, mem_str, ppt_str,
                                          cap_str, fan_str);
  gtk_label_set_markup(GTK_LABEL(lbl_gpu_telemetry), markup);
  g_free(markup);
}

static gboolean gpu_poll_timer_cb(gpointer user_data) {
  (void)user_data;
  if (!g_stack) {
    g_gpu_timer_id = 0;
    return G_SOURCE_REMOVE;
  }
  const char *vis = gtk_stack_get_visible_child_name(g_stack);
  if (!vis || strcmp(vis, "gpu") != 0) {
    g_gpu_timer_id = 0;
    return G_SOURCE_REMOVE;
  }
  gpu_refresh_telemetry();
  return G_SOURCE_CONTINUE;
}

static void on_gpu_dropdown_changed(GObject *obj, GParamSpec *pspec,
                                    gpointer user_data) {
  (void)obj;
  (void)pspec;
  (void)user_data;
  if (g_gpu_updating_dropdown)
    return;
  gpu_refresh_telemetry();
}

static void gpu_discover_and_populate(void) {
  g_free(g_gpus);
  g_gpus = NULL;
  g_gpu_count = 0;

  DIR *drm_dir = opendir("/sys/class/drm");
  if (drm_dir) {
    const struct dirent *de;
    while ((de = readdir(drm_dir)) != NULL) {
      if (strncmp(de->d_name, "card", 4) != 0 ||
          !is_digits_only(de->d_name + 4))
        continue;

      char hwmon_base[512];
      snprintf(hwmon_base, sizeof(hwmon_base), "/sys/class/drm/%s/device/hwmon",
               de->d_name);
      DIR *hw_dir = opendir(hwmon_base);
      if (!hw_dir)
        continue;

      const struct dirent *he;
      while ((he = readdir(hw_dir)) != NULL) {
        if (strncmp(he->d_name, "hwmon", 5) != 0 ||
            !is_digits_only(he->d_name + 5))
          continue;

        char name_path[640];
        char drv_name[64];
        snprintf(name_path, sizeof(name_path), "%s/%s/name", hwmon_base,
                 he->d_name);
        if (!gpu_read_file(name_path, drv_name, sizeof(drv_name)))
          continue;
        drv_name[strcspn(drv_name, "\r\n")] = '\0';
        if (strcmp(drv_name, "amdgpu") != 0)
          continue;

        AmdGpuInfo info;
        memset(&info, 0, sizeof(info));
        snprintf(info.hwmon_path, sizeof(info.hwmon_path), "%s/%s", hwmon_base,
                 he->d_name);
        snprintf(info.card_name, sizeof(info.card_name), "%s", de->d_name);
        gpu_read_pci_slot(de->d_name, info.pci_slot, sizeof(info.pci_slot));

        char fan_path[640];
        char cap_path[640];
        snprintf(fan_path, sizeof(fan_path), "%s/fan1_input", info.hwmon_path);
        snprintf(cap_path, sizeof(cap_path), "%s/power1_cap", info.hwmon_path);
        info.is_discrete =
            (access(fan_path, R_OK) == 0 || access(cap_path, R_OK) == 0);

        snprintf(info.label, sizeof(info.label), "AMD GPU (%s - %s)",
                 info.pci_slot, info.is_discrete ? "Discrete" : "Integrated");

        g_gpus = g_renew(AmdGpuInfo, g_gpus, g_gpu_count + 1);
        g_gpus[g_gpu_count++] = info;
      }
      closedir(hw_dir);
    }
    closedir(drm_dir);
  }

  if (g_gpu_count > 1) {
    qsort(g_gpus, g_gpu_count, sizeof(AmdGpuInfo), cmp_amd_gpu_info);
  }

  if (g_gpu_dropdown) {
    if (g_gpu_count == 0) {
      gtk_widget_set_visible(GTK_WIDGET(g_gpu_dropdown), FALSE);
      g_gpu_updating_dropdown = TRUE;
      gtk_drop_down_set_model(g_gpu_dropdown, NULL);
      g_gpu_updating_dropdown = FALSE;
    } else {
      GtkStringList *str_list = gtk_string_list_new(NULL);
      guint default_idx = 0;
      gboolean found_discrete = FALSE;
      for (guint i = 0; i < g_gpu_count; i++) {
        gtk_string_list_append(str_list, g_gpus[i].label);
        if (!found_discrete && g_gpus[i].is_discrete) {
          default_idx = i;
          found_discrete = TRUE;
        }
      }
      g_gpu_updating_dropdown = TRUE;
      gtk_drop_down_set_model(g_gpu_dropdown, G_LIST_MODEL(str_list));
      g_object_unref(str_list);
      gtk_drop_down_set_selected(g_gpu_dropdown, default_idx);
      g_gpu_updating_dropdown = FALSE;
      gtk_widget_set_visible(GTK_WIDGET(g_gpu_dropdown), TRUE);
    }
  }

  gpu_refresh_telemetry();
}

static gboolean is_tool_available(const char *bin_name,
                                  const char *desktop_id) {
  gchar *bin = g_find_program_in_path(bin_name);
  if (bin) {
    g_free(bin);
    return TRUE;
  }
  GDesktopAppInfo *app = g_desktop_app_info_new(desktop_id);
  if (app) {
    g_object_unref(app);
    return TRUE;
  }
  return FALSE;
}

static void gpu_refresh_launchers(void) {
  gboolean has_lact =
      is_tool_available("lact", "io.github.ilya_zlobintsev.LACT.desktop");
  gboolean has_corectrl =
      is_tool_available("corectrl", "org.corectrl.CoreCtrl.desktop");

  if (row_gpu_lact)
    gtk_widget_set_visible(row_gpu_lact, has_lact);
  if (row_gpu_corectrl)
    gtk_widget_set_visible(row_gpu_corectrl, has_corectrl);
  if (lbl_gpu_control_empty)
    gtk_widget_set_visible(lbl_gpu_control_empty, !has_lact && !has_corectrl);
}

static void on_btn_launch_lact_clicked(GtkButton *btn, gpointer user_data) {
  (void)btn;
  (void)user_data;
  gchar *bin = g_find_program_in_path("lact");
  if (bin) {
    g_free(bin);
    char *argv[] = {(char *)"lact", (char *)"gui", NULL};
    g_spawn_async(NULL, argv, NULL,
                  G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
                      G_SPAWN_STDERR_TO_DEV_NULL,
                  NULL, NULL, NULL, NULL);
    return;
  }
  GDesktopAppInfo *app =
      g_desktop_app_info_new("io.github.ilya_zlobintsev.LACT.desktop");
  if (app) {
    g_app_info_launch(G_APP_INFO(app), NULL, NULL, NULL);
    g_object_unref(app);
  }
}

static void on_btn_launch_corectrl_clicked(GtkButton *btn, gpointer user_data) {
  (void)btn;
  (void)user_data;
  gchar *bin = g_find_program_in_path("corectrl");
  if (bin) {
    g_free(bin);
    char *argv[] = {(char *)"corectrl", NULL};
    g_spawn_async(NULL, argv, NULL,
                  G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
                      G_SPAWN_STDERR_TO_DEV_NULL,
                  NULL, NULL, NULL, NULL);
    return;
  }
  GDesktopAppInfo *app =
      g_desktop_app_info_new("org.corectrl.CoreCtrl.desktop");
  if (app) {
    g_app_info_launch(G_APP_INFO(app), NULL, NULL, NULL);
    g_object_unref(app);
  }
}

static void gpu_activate(void) {
  gpu_deactivate();
  gpu_discover_and_populate();
  gpu_refresh_launchers();
  if (g_gpu_count > 0) {
    g_gpu_timer_id = g_timeout_add_seconds(2, gpu_poll_timer_cb, NULL);
  }
}

static void on_gpu_page_destroy(GtkWidget *widget, gpointer user_data) {
  (void)widget;
  (void)user_data;
  gpu_deactivate();
  g_gpu_dropdown = NULL;
  lbl_gpu_telemetry = NULL;
  lbl_gpu_device_meta = NULL;
  row_gpu_lact = NULL;
  btn_launch_lact = NULL;
  row_gpu_corectrl = NULL;
  btn_launch_corectrl = NULL;
  lbl_gpu_control_empty = NULL;
}

static void on_nav_row_selected(GtkListBox *box, GtkListBoxRow *row,
                                gpointer user_data) {
  (void)box;
  if (!row)
    return;
  GtkStack *stack = GTK_STACK(user_data);
  const char *id = gtk_widget_get_name(GTK_WIDGET(row));
  gtk_stack_set_visible_child_name(stack, id);

  if (id && strcmp(id, "gpu") == 0) {
    gpu_activate();
  } else {
    gpu_deactivate();
  }

  if (id && strcmp(id, "affinity") == 0) {
    refresh_topology();
  }
}

/* ── Config: Read settings.conf ───────────────────────────────── */

/* ── Config: IPC-first lookup, file fallback ──────────────────── */

/* Sidebar row lookup by name (avoids fragile index arithmetic) */
static GtkListBoxRow *find_sidebar_row(GtkListBox *lb, const char *name) {
  if (!lb || !name)
    return NULL;
  for (int i = 0;; i++) {
    GtkListBoxRow *r = gtk_list_box_get_row_at_index(lb, i);
    if (!r)
      break;
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
  if (!f) {
    f = fopen("/etc/x3d-toggle.d/settings.conf", "r");
  }
  if (!f)
    return val;

  char line[512];
  size_t klen = strlen(key);
  while (fgets(line, sizeof(line), f)) {
    char *p = line;
    while (*p == ' ' || *p == '\t')
      p++;
    if (*p == '[')
      break;
    if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0')
      continue;
    if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
      char *v = p + klen + 1;
      v[strcspn(v, "\r\n")] = '\0';
      strncpy(val, v, sizeof(val) - 1);
      val[sizeof(val) - 1] = '\0';
      break;
    }
  }
  fclose(f);

  /* If key wasn't in daemon.conf, check settings.conf fallback */
  if (val[0] == '\0') {
    FILE *fs = fopen("/etc/x3d-toggle.d/settings.conf", "r");
    if (fs) {
      while (fgets(line, sizeof(line), fs)) {
        char *p = line;
        while (*p == ' ' || *p == '\t')
          p++;
        if (*p == '[')
          break;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0')
          continue;
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
          char *v = p + klen + 1;
          v[strcspn(v, "\r\n")] = '\0';
          strncpy(val, v, sizeof(val) - 1);
          val[sizeof(val) - 1] = '\0';
          break;
        }
      }
      fclose(fs);
    }
  }

  return val;
}

/* ── Config: IPC send helper ──────────────────────────────────── */

static void config_send(const char *key, const char *value) {
  char cmd[256], resp[64];
  printf_sn(cmd, sizeof(cmd), "SET_CONFIG %s %s", key, value);
  socket_send(cmd, resp, sizeof(resp));
}

/* ── Config: Widget change callbacks ──────────────────────────── */

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

/* Immediate persistence for Developer / Journal controls (no tab Apply button)
 */
static void on_lint_switch_toggled(GObject *obj, GParamSpec *pspec,
                                   gpointer data) {
  (void)pspec;
  if (config_ignoring_changes || !data)
    return;
  config_send((const char *)data,
              gtk_switch_get_active(GTK_SWITCH(obj)) ? "enabled" : "disabled");
}

static void on_debug_enable_toggled(GObject *obj, GParamSpec *pspec,
                                    gpointer data) {
  (void)pspec;
  (void)data;
  if (config_ignoring_changes)
    return;
  config_send("DEBUG_ENABLE",
              gtk_switch_get_active(GTK_SWITCH(obj)) ? "1" : "0");
}

static void on_force_unsupported_toggled(GObject *obj, GParamSpec *pspec,
                                         gpointer data) {
  (void)pspec;
  (void)data;
  if (config_ignoring_changes)
    return;
  config_send("FORCE_UNSUPPORTED_DISPLAY",
              gtk_switch_get_active(GTK_SWITCH(obj)) ? "1" : "0");
  update_dashboard_cb(NULL);
}

static void on_cfg_scale_changed(GtkRange *range, gpointer data) {
  (void)range;
  (void)data;
  if (config_ignoring_changes)
    return;
  set_config_dirty(TRUE);
}

/* ── Dev mode visibility toggle ──────────────────────────────── */

static void on_advanced_enable_toggled(GObject *obj, GParamSpec *pspec,
                                       gpointer data) {
  (void)pspec;
  (void)data;
  gboolean active = gtk_switch_get_active(GTK_SWITCH(obj));
  if (!config_ignoring_changes)
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
  if (!config_ignoring_changes)
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
static const char *valgrind_mode_vals[] = {"full", "summary", "disabled"};
static const char *valgrind_kinds_vals[] = {"all", "definite", "indirect",
                                            "possible", "reachable"};
static const char *journal_max_mb_vals[] = {"5", "10", "25", "50", "100"};
static const char *server_ssh_vals[] = {"local", "remote", "enabled"};

static void on_valgrind_mode_changed(GObject *obj, GParamSpec *pspec,
                                     gpointer data) {
  (void)pspec;
  (void)data;
  if (config_ignoring_changes)
    return;
  guint idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(obj));
  if (idx < ARRAY_SIZE(valgrind_mode_vals))
    config_send("LINT_VALGRIND_MODE", valgrind_mode_vals[idx]);
}

static void on_valgrind_kinds_changed(GObject *obj, GParamSpec *pspec,
                                      gpointer data) {
  (void)pspec;
  (void)data;
  if (config_ignoring_changes)
    return;
  guint idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(obj));
  if (idx < ARRAY_SIZE(valgrind_kinds_vals))
    config_send("LINT_VALGRIND_KINDS", valgrind_kinds_vals[idx]);
}

static guint find_dropdown_idx(const char **values, int count,
                               const char *target) {
  for (int i = 0; i < count; i++)
    if (strcmp(values[i], target) == 0)
      return (guint)i;
  return 0;
}

static void on_rotation_apply_clicked(GtkButton *btn, gpointer data) {
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
  btn_feedback(btn);
}

static void on_rotation_cancel_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  (void)data;
  if (g_cfg_journal_keep) {
    const char *val = config_get("JOURNAL_KEEP");
    if (val[0])
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

static void on_irq_apply_clicked(GtkButton *btn, gpointer data) {
  (void)data;

  gboolean irq_enabled =
      g_cfg_irq_enable ? gtk_switch_get_active(g_cfg_irq_enable) : FALSE;

  if (g_cfg_irq_enable)
    config_send("IRQ_ENABLE", irq_enabled ? "1" : "0");
  if (g_cfg_irq_gpu)
    config_send("IRQ_GPU", gtk_switch_get_active(g_cfg_irq_gpu) ? "1" : "0");
  if (g_cfg_irq_nvme)
    config_send("IRQ_NVME", gtk_switch_get_active(g_cfg_irq_nvme) ? "1" : "0");
  if (g_cfg_irq_usb)
    config_send("IRQ_USB", gtk_switch_get_active(g_cfg_irq_usb) ? "1" : "0");
  if (g_cfg_irq_nic)
    config_send("IRQ_NIC", gtk_switch_get_active(g_cfg_irq_nic) ? "1" : "0");
  if (g_cfg_irq_audio)
    config_send("IRQ_AUDIO",
                gtk_switch_get_active(g_cfg_irq_audio) ? "1" : "0");
  if (g_cfg_irq_coalesce)
    config_send("IRQ_COALESCE",
                gtk_switch_get_active(g_cfg_irq_coalesce) ? "1" : "0");
  if (g_cfg_irq_watch)
    config_send("IRQ_WATCH",
                gtk_switch_get_active(g_cfg_irq_watch) ? "1" : "0");

  /* Apply or reset IRQ bindings directly in the privileged daemon via IPC */
  char resp[64] = {0};
  socket_send(irq_enabled ? "IRQ_BIND" : "IRQ_RESET", resp, sizeof(resp));
  btn_feedback(btn);
}

/* ── Advanced Configuration Callbacks ────────────────────────── */

static void advanced_send(const char *sysctl_key, const char *value) {
  char cmd[256], resp[64];
  printf_sn(cmd, sizeof(cmd), "ADVANCED_SET %s %s", sysctl_key, value);
  socket_send(cmd, resp, sizeof(resp));
}

static void on_adv_apply_clicked(GtkButton *btn, gpointer data) {
  (void)data;
  if (g_cfg_sched_bore) {
    const char *v = gtk_switch_get_active(g_cfg_sched_bore) ? "1" : "0";
    config_send("SCHED_BORE", v);
    advanced_send("kernel.sched_bore", v);
  }
  if (g_cfg_sched_bit_shift) {
    char buf[16];
    printf_sn(buf, sizeof(buf), "%d",
              (int)gtk_spin_button_get_value(g_cfg_sched_bit_shift));
    config_send("SCHED_BIT_SHIFT", buf);
    advanced_send("kernel.sched_burst_penalty_offset", buf);
  }
  if (g_cfg_sched_burst_fork) {
    const char *v = gtk_switch_get_active(g_cfg_sched_burst_fork) ? "2" : "0";
    config_send("SCHED_BURST_FORK", v);
    advanced_send("kernel.sched_burst_inherit_type", v);
  }
  if (g_cfg_sched_slice_us) {
    char buf[16];
    printf_sn(buf, sizeof(buf), "%d",
              (int)gtk_spin_button_get_value(g_cfg_sched_slice_us));
    config_send("SCHED_SLICE_US", buf);
    advanced_send("kernel.sched_cfs_bandwidth_slice_us", buf);
  }
  if (g_cfg_vm_max_map) {
    const char *v = gtk_editable_get_text(g_cfg_vm_max_map);
    if (v && v[0]) {
      config_send("VM_MAX_MAP", v);
      advanced_send("vm.max_map_count", v);
    }
  }
  if (g_cfg_split_lock_detect) {
    const char *v = gtk_switch_get_active(g_cfg_split_lock_detect) ? "1" : "0";
    config_send("SPLIT_LOCK_DETECT", v);
    advanced_send("kernel.split_lock_mitigate", v);
  }
  if (g_cfg_nmi_watchdog) {
    const char *v = gtk_switch_get_active(g_cfg_nmi_watchdog) ? "1" : "0";
    config_send("NMI_WATCHDOG", v);
    advanced_send("kernel.nmi_watchdog", v);
  }
  if (g_cfg_thp_mode) {
    const char *modes[] = {"always", "madvise", "never"};
    guint idx = gtk_drop_down_get_selected(g_cfg_thp_mode);
    if (idx < ARRAY_SIZE(modes)) {
      const char *v = modes[idx];
      config_send("THP_MODE", v);
      advanced_send("target_thp_mode", v);
    }
  }
  btn_feedback(btn);
}

static void on_adv_cancel_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  (void)data;
  config_ignoring_changes = TRUE;
  if (g_cfg_sched_bore)
    gtk_switch_set_active(g_cfg_sched_bore, atoi(config_get("SCHED_BORE")));
  if (g_cfg_sched_bit_shift)
    gtk_spin_button_set_value(g_cfg_sched_bit_shift,
                              atoi(config_get("SCHED_BIT_SHIFT")));
  if (g_cfg_sched_burst_fork)
    gtk_switch_set_active(g_cfg_sched_burst_fork,
                          atoi(config_get("SCHED_BURST_FORK")));
  if (g_cfg_sched_slice_us)
    gtk_spin_button_set_value(g_cfg_sched_slice_us,
                              atoi(config_get("SCHED_SLICE_US")));
  if (g_cfg_vm_max_map)
    gtk_editable_set_text(g_cfg_vm_max_map, config_get("VM_MAX_MAP"));
  if (g_cfg_split_lock_detect)
    gtk_switch_set_active(g_cfg_split_lock_detect,
                          atoi(config_get("SPLIT_LOCK_DETECT")));
  if (g_cfg_nmi_watchdog)
    gtk_switch_set_active(g_cfg_nmi_watchdog, atoi(config_get("NMI_WATCHDOG")));
  if (g_cfg_thp_mode) {
    const char *val = config_get("THP_MODE");
    const char *modes[] = {"always", "madvise", "never"};
    gtk_drop_down_set_selected(g_cfg_thp_mode,
                               find_dropdown_idx(modes, 3, val));
  }
  config_ignoring_changes = FALSE;
}

/* ── Networking Configuration Callbacks ──────────────────────── */

static void on_net_apply_clicked(GtkButton *btn, gpointer data) {
  (void)data;
  if (g_cfg_net_qdisc) {
    const char *qdisc = gtk_editable_get_text(g_cfg_net_qdisc);
    if (qdisc && qdisc[0]) {
      config_send("NET_QDISC", qdisc);
      advanced_send("net.core.default_qdisc", qdisc);
    }
  }
  if (g_cfg_net_fastopen) {
    char buf[16];
    printf_sn(buf, sizeof(buf), "%d",
              (int)gtk_spin_button_get_value(g_cfg_net_fastopen));
    config_send("NET_FASTOPEN", buf);
    advanced_send("net.ipv4.tcp_fastopen", buf);
  }
  if (g_cfg_net_rp_filter) {
    char buf[16];
    printf_sn(buf, sizeof(buf), "%d",
              (int)gtk_spin_button_get_value(g_cfg_net_rp_filter));
    config_send("NET_RP_FILTER", buf);
    advanced_send("net.ipv4.conf.all.rp_filter", buf);
  }
  if (g_cfg_net_source_route) {
    const char *v = gtk_switch_get_active(g_cfg_net_source_route) ? "1" : "0";
    config_send("NET_SOURCE_ROUTE", v);
    advanced_send("net.ipv4.conf.all.accept_source_route", v);
  }
  if (g_cfg_server_address) {
    const char *addr = gtk_editable_get_text(g_cfg_server_address);
    if (addr && addr[0])
      config_send("SERVER_ADDRESS", addr);
  }
  if (g_cfg_server_port) {
    const char *port = gtk_editable_get_text(g_cfg_server_port);
    if (port && port[0])
      config_send("SERVER_PORT", port);
  }
  if (g_cfg_server_ssh) {
    guint idx = gtk_drop_down_get_selected(g_cfg_server_ssh);
    if (idx < ARRAY_SIZE(server_ssh_vals))
      config_send("SERVER_SSH", server_ssh_vals[idx]);
  }
  btn_feedback(btn);
}

static void on_net_cancel_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  (void)data;
  config_ignoring_changes = TRUE;
  if (g_cfg_net_qdisc)
    gtk_editable_set_text(g_cfg_net_qdisc, config_get("NET_QDISC"));
  if (g_cfg_net_fastopen)
    gtk_spin_button_set_value(g_cfg_net_fastopen,
                              atoi(config_get("NET_FASTOPEN")));
  if (g_cfg_net_rp_filter)
    gtk_spin_button_set_value(g_cfg_net_rp_filter,
                              atoi(config_get("NET_RP_FILTER")));
  if (g_cfg_net_source_route)
    gtk_switch_set_active(g_cfg_net_source_route,
                          atoi(config_get("NET_SOURCE_ROUTE")));
  if (g_cfg_server_address) {
    const char *val = config_get("SERVER_ADDRESS");
    if (val[0] && strcmp(val, "ERR") != 0)
      gtk_editable_set_text(g_cfg_server_address, val);
  }
  if (g_cfg_server_port) {
    const char *val = config_get("SERVER_PORT");
    if (val[0] && strcmp(val, "ERR") != 0)
      gtk_editable_set_text(g_cfg_server_port, val);
  }
  if (g_cfg_server_ssh) {
    const char *val = config_get("SERVER_SSH");
    gtk_drop_down_set_selected(
        g_cfg_server_ssh,
        find_dropdown_idx(server_ssh_vals, ARRAY_SIZE(server_ssh_vals), val));
  }
  config_ignoring_changes = FALSE;
}

static void on_irq_status_clicked(GtkButton *btn, gpointer data) {
  (void)btn;
  (void)data;
  gui_spawn_terminal("watch -n1 x3d-toggle irq-status");
}

static gboolean reset_cfg_apply_lbl(gpointer data) {
  (void)data;
  if (btn_cfg_apply)
    gtk_button_set_label(GTK_BUTTON(btn_cfg_apply), "Apply");
  return G_SOURCE_REMOVE;
}

static void on_cfg_apply_clicked(GtkButton *btn, gpointer user_data) {
  (void)btn;
  (void)user_data;

  if (g_cfg_daemon_state) {
    guint idx = gtk_drop_down_get_selected(g_cfg_daemon_state);
    if (idx < ARRAY_SIZE(daemon_state_vals)) {
      char cmd[64], resp[32];
      config_send("DAEMON_STATE", daemon_state_vals[idx]);
      printf_sn(cmd, sizeof(cmd), "SET_DAEMON %s", daemon_state_vals[idx]);
      socket_send(cmd, resp, sizeof(resp));
    }
  }
  if (g_cfg_fallback_profile) {
    guint idx = gtk_drop_down_get_selected(g_cfg_fallback_profile);
    if (idx < ARRAY_SIZE(fallback_vals))
      config_send("FALLBACK_PROFILE", fallback_vals[idx]);
  }
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
    guint sel = gtk_drop_down_get_selected(g_cfg_detection_level);
    char buf[16];
    printf_sn(buf, sizeof(buf), "%d", (sel == 1) ? 2 : 1);
    config_send("DETECTION_LEVEL", buf);
  }
  if (g_cfg_ebpf_enable)
    config_send("EBPF_ENABLE",
                gtk_switch_get_active(g_cfg_ebpf_enable) ? "1" : "0");
  if (g_cfg_dev_enable)
    config_send("DEV_ENABLE",
                gtk_switch_get_active(g_cfg_dev_enable) ? "1" : "0");
  if (g_cfg_advanced_enable)
    config_send("ADVANCED_CONFIG_ENABLE",
                gtk_switch_get_active(g_cfg_advanced_enable) ? "1" : "0");

  set_config_dirty(FALSE);
  if (btn_cfg_apply) {
    gtk_button_set_label(GTK_BUTTON(btn_cfg_apply), "Applied!");
    g_timeout_add(1500, reset_cfg_apply_lbl, NULL);
  }
}

static void on_cfg_cancel_clicked(GtkButton *btn, gpointer user_data) {
  (void)btn;
  (void)user_data;
  config_ignoring_changes = TRUE;

  if (g_cfg_daemon_state)
    gtk_drop_down_set_selected(g_cfg_daemon_state,
                               find_dropdown_idx(daemon_state_vals,
                                                 ARRAY_SIZE(daemon_state_vals),
                                                 config_get("DAEMON_STATE")));
  if (g_cfg_fallback_profile)
    gtk_drop_down_set_selected(
        g_cfg_fallback_profile,
        find_dropdown_idx(fallback_vals, ARRAY_SIZE(fallback_vals),
                          config_get("FALLBACK_PROFILE")));
  if (g_cfg_polling_interval) {
    const char *pv = config_get("POLLING_INTERVAL");
    gtk_range_set_value(g_cfg_polling_interval, atof(pv[0] ? pv : "3"));
  }
  if (g_cfg_load_threshold) {
    const char *lv = config_get("LOAD_THRESHOLD");
    gtk_range_set_value(g_cfg_load_threshold, atof(lv[0] ? lv : "50"));
  }
  if (g_cfg_detection_level) {
    int det = atoi(config_get("DETECTION_LEVEL"));
    gtk_drop_down_set_selected(g_cfg_detection_level, (det >= 2) ? 1U : 0U);
  }
  if (g_cfg_ebpf_enable)
    gtk_switch_set_active(g_cfg_ebpf_enable,
                          atoi(config_get("EBPF_ENABLE")) != 0);
  if (g_cfg_dev_enable)
    gtk_switch_set_active(g_cfg_dev_enable,
                          atoi(config_get("DEV_ENABLE")) != 0);
  if (g_cfg_advanced_enable)
    gtk_switch_set_active(g_cfg_advanced_enable,
                          atoi(config_get("ADVANCED_CONFIG_ENABLE")) != 0);

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
                     NULL);
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
                     NULL);
  }

  /* Scale: POLLING_INTERVAL */
  w = gtk_builder_get_object(builder, "cfg_polling_interval");
  if (w) {
    g_cfg_polling_interval = GTK_RANGE(w);
    val = config_get("POLLING_INTERVAL");
    gtk_range_set_value(GTK_RANGE(w), atof(val[0] ? val : "3"));
    g_signal_connect(w, "value-changed", G_CALLBACK(on_cfg_scale_changed),
                     (gpointer) "POLLING_INTERVAL");
  }

  /* Scale: LOAD_THRESHOLD */
  w = gtk_builder_get_object(builder, "cfg_load_threshold");
  if (w) {
    g_cfg_load_threshold = GTK_RANGE(w);
    val = config_get("LOAD_THRESHOLD");
    gtk_range_set_value(GTK_RANGE(w), atof(val[0] ? val : "50"));
    g_signal_connect(w, "value-changed", G_CALLBACK(on_cfg_scale_changed),
                     (gpointer) "LOAD_THRESHOLD");
  }

  /* Dropdown: DETECTION_LEVEL (1=Strict idx0, 2=Loose idx1) */
  w = gtk_builder_get_object(builder, "cfg_detection_level");
  if (w) {
    g_cfg_detection_level = GTK_DROP_DOWN(w);
    int det = atoi(config_get("DETECTION_LEVEL"));
    gtk_drop_down_set_selected(GTK_DROP_DOWN(w), (det >= 2) ? 1U : 0U);
    g_signal_connect(w, "notify::selected", G_CALLBACK(on_cfg_dropdown_changed),
                     NULL);
  }

  /* Switch: EBPF_ENABLE */
  w = gtk_builder_get_object(builder, "cfg_ebpf_enable");
  if (w) {
    g_cfg_ebpf_enable = GTK_SWITCH(w);
    val = config_get("EBPF_ENABLE");
    gtk_switch_set_active(GTK_SWITCH(w), val[0] ? (atoi(val) != 0) : TRUE);
    g_signal_connect(w, "notify::active", G_CALLBACK(on_cfg_switch_changed),
                     (gpointer) "EBPF_ENABLE");
  }

  /* Switch: ADVANCED_CONFIG_ENABLE (controls advanced tab visibility) */
  w = gtk_builder_get_object(builder, "cfg_advanced_enable");
  if (w) {
    g_cfg_advanced_enable = GTK_SWITCH(w);
    val = config_get("ADVANCED_CONFIG_ENABLE");
    gboolean adv = atoi(val) != 0;
    gtk_switch_set_active(GTK_SWITCH(w), adv);
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
    g_signal_connect(w, "notify::active", G_CALLBACK(on_dev_enable_toggled),
                     NULL);
    gtk_widget_set_visible(row_developer, dev);
    gtk_widget_set_visible(row_debug, dev);
  }

  /* Switch: DEBUG_ENABLE (System Journal tab - immediate persistence) */
  w = gtk_builder_get_object(builder, "cfg_debug_enable");
  if (w) {
    val = config_get("DEBUG_ENABLE");
    gtk_switch_set_active(GTK_SWITCH(w), atoi(val) != 0);
    g_signal_connect(w, "notify::active", G_CALLBACK(on_debug_enable_toggled),
                     NULL);
  }

  /* ── IRQ Subsystem Switches (applied via btn_irq_apply) ── */
  struct {
    const char *id;
    const char *key;
    GtkSwitch **ptr;
  } irq_switches[] = {{"cfg_irq_enable", "IRQ_ENABLE", &g_cfg_irq_enable},
                      {"cfg_irq_gpu", "IRQ_GPU", &g_cfg_irq_gpu},
                      {"cfg_irq_nvme", "IRQ_NVME", &g_cfg_irq_nvme},
                      {"cfg_irq_usb", "IRQ_USB", &g_cfg_irq_usb},
                      {"cfg_irq_nic", "IRQ_NIC", &g_cfg_irq_nic},
                      {"cfg_irq_audio", "IRQ_AUDIO", &g_cfg_irq_audio},
                      {"cfg_irq_coalesce", "IRQ_COALESCE", &g_cfg_irq_coalesce},
                      {"cfg_irq_watch", "IRQ_WATCH", &g_cfg_irq_watch},
                      {NULL, NULL, NULL}};

  for (int i = 0; irq_switches[i].id; i++) {
    w = gtk_builder_get_object(builder, irq_switches[i].id);
    if (w) {
      if (irq_switches[i].ptr)
        *(irq_switches[i].ptr) = GTK_SWITCH(w);
      val = config_get(irq_switches[i].key);
      gtk_switch_set_active(GTK_SWITCH(w), atoi(val) != 0);
    }
  }

  /* ── Developer tab: Linter switches (immediate persistence) ── */
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
      g_signal_connect(w, "notify::active", G_CALLBACK(on_lint_switch_toggled),
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
    g_signal_connect(w, "notify::selected",
                     G_CALLBACK(on_valgrind_mode_changed), NULL);
  }

  /* Dropdown: VALGRIND_KINDS */
  w = gtk_builder_get_object(builder, "cfg_lint_valgrind_kinds");
  if (w) {
    val = config_get("LINT_VALGRIND_KINDS");
    gtk_drop_down_set_selected(
        GTK_DROP_DOWN(w),
        find_dropdown_idx(valgrind_kinds_vals, ARRAY_SIZE(valgrind_kinds_vals),
                          val));
    g_signal_connect(w, "notify::selected",
                     G_CALLBACK(on_valgrind_kinds_changed), NULL);
  }

  /* Linter Run Buttons */
  w = gtk_builder_get_object(builder, "btn_lint_clang_run");
  if (w)
    g_signal_connect(w, "clicked", G_CALLBACK(on_btn_lint_clang_clicked), NULL);

  w = gtk_builder_get_object(builder, "btn_lint_cppcheck_run");
  if (w)
    g_signal_connect(w, "clicked", G_CALLBACK(on_btn_lint_cppcheck_clicked),
                     NULL);

  w = gtk_builder_get_object(builder, "btn_lint_valgrind_run");
  if (w)
    g_signal_connect(w, "clicked", G_CALLBACK(on_btn_lint_valgrind_clicked),
                     NULL);

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
    if (val[0])
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

  /* Entry: SERVER_ADDRESS, SERVER_PORT, SERVER_SSH (Networking tab) */
  w = gtk_builder_get_object(builder, "cfg_server_address");
  if (w) {
    g_cfg_server_address = GTK_EDITABLE(w);
  }
  w = gtk_builder_get_object(builder, "cfg_server_port");
  if (w) {
    g_cfg_server_port = GTK_EDITABLE(w);
  }
  val = config_get("SERVER_ADDRESS");
  if (val[0] && strcmp(val, "ERR") != 0 && g_cfg_server_address) {
    gtk_editable_set_text(g_cfg_server_address, val);
  }
  val = config_get("SERVER_PORT");
  if (val[0] && strcmp(val, "ERR") != 0 && g_cfg_server_port) {
    gtk_editable_set_text(g_cfg_server_port, val);
  }
  w = gtk_builder_get_object(builder, "cfg_server_ssh");
  if (w) {
    g_cfg_server_ssh = GTK_DROP_DOWN(w);
  }
  val = config_get("SERVER_SSH");
  if (val[0] && strcmp(val, "ERR") != 0 && g_cfg_server_ssh) {
    gtk_drop_down_set_selected(
        g_cfg_server_ssh,
        find_dropdown_idx(server_ssh_vals, ARRAY_SIZE(server_ssh_vals), val));
  }

  /* Config Apply/Cancel */
  w = gtk_builder_get_object(builder, "btn_cfg_apply");
  if (w) {
    btn_cfg_apply = GTK_WIDGET(w);
    gtk_widget_set_sensitive(btn_cfg_apply, FALSE);
    g_signal_connect(w, "clicked", G_CALLBACK(on_cfg_apply_clicked), NULL);
  }

  w = gtk_builder_get_object(builder, "btn_irq_apply");
  if (w) {
    g_signal_connect(w, "clicked", G_CALLBACK(on_irq_apply_clicked), NULL);
  }

  w = gtk_builder_get_object(builder, "btn_irq_status");
  if (w) {
    g_signal_connect(w, "clicked", G_CALLBACK(on_irq_status_clicked), NULL);
  }
  w = gtk_builder_get_object(builder, "btn_cfg_cancel");
  if (w) {
    btn_cfg_cancel = GTK_WIDGET(w);
    gtk_widget_set_sensitive(btn_cfg_cancel, FALSE);
    g_signal_connect(w, "clicked", G_CALLBACK(on_cfg_cancel_clicked), NULL);
  }

  /* Advanced Configuration Tab Bindings */
  struct {
    const char *id;
    GtkSwitch **ptr;
    const char *key;
  } adv_switches[] = {
      {"cfg_sched_bore", &g_cfg_sched_bore, "SCHED_BORE"},
      {"cfg_sched_burst_fork", &g_cfg_sched_burst_fork, "SCHED_BURST_FORK"},
      {"cfg_split_lock_detect", &g_cfg_split_lock_detect, "SPLIT_LOCK_DETECT"},
      {"cfg_nmi_watchdog", &g_cfg_nmi_watchdog, "NMI_WATCHDOG"},
      {NULL, NULL, NULL}};
  for (int i = 0; adv_switches[i].id; i++) {
    w = gtk_builder_get_object(builder, adv_switches[i].id);
    if (w) {
      *adv_switches[i].ptr = GTK_SWITCH(w);
      gtk_switch_set_active(GTK_SWITCH(w),
                            atoi(config_get(adv_switches[i].key)));
    }
  }
  w = gtk_builder_get_object(builder, "cfg_sched_bit_shift");
  if (w) {
    g_cfg_sched_bit_shift = GTK_SPIN_BUTTON(w);
    gtk_spin_button_set_value(g_cfg_sched_bit_shift,
                              atoi(config_get("SCHED_BIT_SHIFT")));
  }
  w = gtk_builder_get_object(builder, "cfg_sched_slice_us");
  if (w) {
    g_cfg_sched_slice_us = GTK_SPIN_BUTTON(w);
    gtk_spin_button_set_value(g_cfg_sched_slice_us,
                              atoi(config_get("SCHED_SLICE_US")));
  }
  w = gtk_builder_get_object(builder, "cfg_vm_max_map");
  if (w) {
    g_cfg_vm_max_map = GTK_EDITABLE(w);
    gtk_editable_set_text(g_cfg_vm_max_map, config_get("VM_MAX_MAP"));
  }
  w = gtk_builder_get_object(builder, "cfg_thp_mode");
  if (w) {
    g_cfg_thp_mode = GTK_DROP_DOWN(w);
    const char *modes[] = {"always", "madvise", "never"};
    gtk_drop_down_set_selected(
        g_cfg_thp_mode, find_dropdown_idx(modes, 3, config_get("THP_MODE")));
  }
  w = gtk_builder_get_object(builder, "btn_adv_apply");
  if (w)
    g_signal_connect(w, "clicked", G_CALLBACK(on_adv_apply_clicked), NULL);
  w = gtk_builder_get_object(builder, "btn_adv_cancel");
  if (w)
    g_signal_connect(w, "clicked", G_CALLBACK(on_adv_cancel_clicked), NULL);

  /* Networking Tab Bindings */
  w = gtk_builder_get_object(builder, "cfg_net_qdisc");
  if (w) {
    g_cfg_net_qdisc = GTK_EDITABLE(w);
    gtk_editable_set_text(g_cfg_net_qdisc, config_get("NET_QDISC"));
  }
  w = gtk_builder_get_object(builder, "cfg_net_fastopen");
  if (w) {
    g_cfg_net_fastopen = GTK_SPIN_BUTTON(w);
    gtk_spin_button_set_value(g_cfg_net_fastopen,
                              atoi(config_get("NET_FASTOPEN")));
  }
  w = gtk_builder_get_object(builder, "cfg_net_rp_filter");
  if (w) {
    g_cfg_net_rp_filter = GTK_SPIN_BUTTON(w);
    gtk_spin_button_set_value(g_cfg_net_rp_filter,
                              atoi(config_get("NET_RP_FILTER")));
  }
  w = gtk_builder_get_object(builder, "cfg_net_source_route");
  if (w) {
    g_cfg_net_source_route = GTK_SWITCH(w);
    gtk_switch_set_active(g_cfg_net_source_route,
                          atoi(config_get("NET_SOURCE_ROUTE")));
  }
  w = gtk_builder_get_object(builder, "btn_net_apply");
  if (w)
    g_signal_connect(w, "clicked", G_CALLBACK(on_net_apply_clicked), NULL);
  w = gtk_builder_get_object(builder, "btn_net_cancel");
  if (w)
    g_signal_connect(w, "clicked", G_CALLBACK(on_net_cancel_clicked), NULL);

  /* Capability Rows */
  row_sched_bore =
      GTK_WIDGET(gtk_builder_get_object(builder, "row_sched_bore"));
  row_sched_bit_shift =
      GTK_WIDGET(gtk_builder_get_object(builder, "row_sched_bit_shift"));
  row_sched_burst_fork =
      GTK_WIDGET(gtk_builder_get_object(builder, "row_sched_burst_fork"));
  row_split_lock =
      GTK_WIDGET(gtk_builder_get_object(builder, "row_split_lock"));
  row_nmi_watchdog =
      GTK_WIDGET(gtk_builder_get_object(builder, "row_nmi_watchdog"));
  row_thp_mode = GTK_WIDGET(gtk_builder_get_object(builder, "row_thp_mode"));

  /* UI Override Switch (Developer tab - immediate persistence) */
  w = gtk_builder_get_object(builder, "cfg_force_unsupported");
  if (w) {
    g_cfg_force_unsupported = GTK_SWITCH(w);
    gtk_switch_set_active(g_cfg_force_unsupported,
                          atoi(config_get("FORCE_UNSUPPORTED_DISPLAY")) != 0);
    g_signal_connect(w, "notify::active",
                     G_CALLBACK(on_force_unsupported_toggled), NULL);
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

/* ── Theme Management & Persistence ───────────────────────────── */

static GtkCssProvider *g_theme_provider = NULL;
static gboolean g_theme_updating = FALSE;

static const struct {
  const char *id;
  const char *resource_path;
} g_themes[] = {
    {"default", "/org/x3d-toggle/gui/theme.css"},
    {"imperial", "/org/x3d-toggle/gui/theme-imperial.css"},
};

static guint gui_pref_load_theme(void) {
  guint idx = 0;
  gchar *conf_path =
      g_build_filename(g_get_user_config_dir(), "x3d-toggle", "gui.conf", NULL);
  gchar *contents = NULL;
  if (g_file_get_contents(conf_path, &contents, NULL, NULL) && contents) {
    const char *p = contents;
    while (*p) {
      const char *nl = strchr(p, '\n');
      size_t len = nl ? (size_t)(nl - p) : strlen(p);
      if (len > 6 && strncmp(p, "THEME=", 6) == 0) {
        size_t vlen = len - 6;
        if (p[6 + vlen - 1] == '\r')
          vlen--;
        for (guint i = 0; i < ARRAY_SIZE(g_themes); i++) {
          if (strlen(g_themes[i].id) == vlen &&
              strncmp(p + 6, g_themes[i].id, vlen) == 0) {
            idx = i;
            break;
          }
        }
        break;
      }
      if (!nl)
        break;
      p = nl + 1;
    }
    g_free(contents);
  }
  g_free(conf_path);
  return idx;
}

static void gui_pref_save_theme(guint idx) {
  if (idx >= ARRAY_SIZE(g_themes))
    return;
  gchar *conf_dir =
      g_build_filename(g_get_user_config_dir(), "x3d-toggle", NULL);
  if (g_mkdir_with_parents(conf_dir, 0755) == 0) {
    gchar *conf_path = g_build_filename(conf_dir, "gui.conf", NULL);
    gchar *line = g_strdup_printf("THEME=%s\n", g_themes[idx].id);
    g_file_set_contents(conf_path, line, -1, NULL);
    g_free(line);
    g_free(conf_path);
  }
  g_free(conf_dir);
}

static void apply_gui_theme(guint idx) {
  if (idx >= ARRAY_SIZE(g_themes) || !g_theme_provider)
    return;
  adw_style_manager_set_color_scheme(adw_style_manager_get_default(),
                                     (idx == 1) ? ADW_COLOR_SCHEME_FORCE_DARK
                                                : ADW_COLOR_SCHEME_DEFAULT);
  gtk_css_provider_load_from_resource(g_theme_provider,
                                      g_themes[idx].resource_path);
}

static void on_theme_dropdown_selected(GObject *dropdown, GParamSpec *pspec,
                                       gpointer user_data) {
  (void)pspec;
  (void)user_data;
  if (g_theme_updating)
    return;
  guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(dropdown));
  if (selected < ARRAY_SIZE(g_themes)) {
    apply_gui_theme(selected);
    gui_pref_save_theme(selected);
  }
}

static void on_app_activate(GtkApplication *app, gpointer user_data) {
  (void)user_data;
  GObject *w;

  gtk_window_set_default_icon_name("x3d-toggle");

  /* Initialize CSS provider and restore saved theme */
  if (!g_theme_provider) {
    g_theme_provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(g_theme_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  }
  guint initial_theme = gui_pref_load_theme();
  apply_gui_theme(initial_theme);

  /* Load UI */
  GtkBuilder *builder =
      gtk_builder_new_from_resource("/org/x3d-toggle/gui/x3d-toggle.ui");

  GtkWindow *window =
      GTK_WINDOW(gtk_builder_get_object(builder, "main_window"));
  gtk_window_set_application(window, app);

  lbl_status_daemon =
      GTK_WIDGET(gtk_builder_get_object(builder, "lbl_status_daemon"));
  lbl_status_hardware =
      GTK_WIDGET(gtk_builder_get_object(builder, "lbl_status_hardware"));
  lbl_status_system =
      GTK_WIDGET(gtk_builder_get_object(builder, "lbl_status_system"));

  /* Bind Navigation */
  GtkListBox *sidebar =
      GTK_LIST_BOX(gtk_builder_get_object(builder, "sidebar_list"));
  GtkStack *stack = GTK_STACK(gtk_builder_get_object(builder, "content_stack"));

  g_sidebar = sidebar;
  g_stack = stack;

  GObject *dropdown_theme = gtk_builder_get_object(builder, "dropdown_theme");
  if (dropdown_theme) {
    g_theme_updating = TRUE;
    gtk_drop_down_set_selected(GTK_DROP_DOWN(dropdown_theme), initial_theme);
    g_theme_updating = FALSE;
    g_signal_connect(dropdown_theme, "notify::selected",
                     G_CALLBACK(on_theme_dropdown_selected), NULL);
  }

  GObject *dropdown_extras = gtk_builder_get_object(builder, "dropdown_extras");
  GObject *txt_extras_view = gtk_builder_get_object(builder, "txt_extras_view");
  g_signal_connect(dropdown_extras, "notify::selected",
                   G_CALLBACK(on_extras_dropdown_selected), txt_extras_view);
  update_extras_view(GTK_TEXT_VIEW(txt_extras_view), 0);

  add_nav_row(sidebar, "dashboard", "Dashboard");
  add_nav_row(sidebar, "cpu", "CPU");
  add_nav_row(sidebar, "gpu", "GPU");

  add_nav_row(sidebar, "affinity", "Threaded Affinity Settings");
  add_nav_row(sidebar, "irq", "IRQ Vector Binding");

  add_nav_row(sidebar, "configuration", "Configuration");
  row_advanced = add_nav_row(sidebar, "advanced", "Advanced Configuration");
  add_nav_row(sidebar, "networking", "Networking Settings");
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
                               (guint)atoi(val[0] ? val : "0"));
  }

  /* Bind Lifecycle Dropdown */
  g_lifecycle_dropdown =
      GTK_DROP_DOWN(gtk_builder_get_object(builder, "lifecycle_dropdown"));

  /* GPU Subpage Bindings */
  w = gtk_builder_get_object(builder, "gpu_selector_dropdown");
  if (w) {
    g_gpu_dropdown = GTK_DROP_DOWN(w);
    g_signal_connect(w, "notify::selected", G_CALLBACK(on_gpu_dropdown_changed),
                     NULL);
  }
  lbl_gpu_telemetry =
      GTK_WIDGET(gtk_builder_get_object(builder, "lbl_gpu_telemetry"));
  lbl_gpu_device_meta =
      GTK_WIDGET(gtk_builder_get_object(builder, "lbl_gpu_device_meta"));
  row_gpu_lact = GTK_WIDGET(gtk_builder_get_object(builder, "row_gpu_lact"));
  btn_launch_lact =
      GTK_WIDGET(gtk_builder_get_object(builder, "btn_launch_lact"));
  row_gpu_corectrl =
      GTK_WIDGET(gtk_builder_get_object(builder, "row_gpu_corectrl"));
  btn_launch_corectrl =
      GTK_WIDGET(gtk_builder_get_object(builder, "btn_launch_corectrl"));
  lbl_gpu_control_empty =
      GTK_WIDGET(gtk_builder_get_object(builder, "lbl_gpu_control_empty"));

  if (btn_launch_lact)
    g_signal_connect(btn_launch_lact, "clicked",
                     G_CALLBACK(on_btn_launch_lact_clicked), NULL);
  if (btn_launch_corectrl)
    g_signal_connect(btn_launch_corectrl, "clicked",
                     G_CALLBACK(on_btn_launch_corectrl_clicked), NULL);

  w = gtk_builder_get_object(builder, "gpu_page_box");
  if (w)
    g_signal_connect(w, "destroy", G_CALLBACK(on_gpu_page_destroy), NULL);

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
  gpu_deactivate();
  g_clear_object(&g_theme_provider);
  g_object_unref(app);
  return status;
}
