/* GTK4 Frontend Core IPC Transport & Global Capability Implementation
 * `gui_ipc.c`
 * Pure UNIX domain socket transport (socket_send), reactive GObject capability
 * properties, desktop launchers, and user GUI theme preferences.
 * Zero GTK widget dependencies.
 */

#include "gui_ipc.h"
#include <stdio.h>
#include <unistd.h>

extern int socket_send(const char *cmd, char *response, size_t resp_len);
extern int printf_sn(char *buf, size_t size, const char *fmt, ...);

struct _X3dGuiIpc {
  GObject parent_instance;

  uint64_t caps;
  gboolean force_unsupported;
  gboolean adv_enabled;
  gboolean dev_enabled;
  gboolean aod_supported;
  gboolean aod_reboot_pending;
  guint update_interval_ms;
};

enum {
  PROP_0,
  PROP_CAPS,
  PROP_FORCE_UNSUPPORTED,
  PROP_ADV_ENABLED,
  PROP_DEV_ENABLED,
  PROP_AOD_SUPPORTED,
  PROP_AOD_REBOOT_PENDING,
  PROP_UPDATE_INTERVAL_MS,
  N_PROPS
};

static GParamSpec *obj_props[N_PROPS] = {NULL};

G_DEFINE_TYPE(X3dGuiIpc, x3d_gui_ipc, G_TYPE_OBJECT)

static void x3d_gui_ipc_get_property(GObject *object, guint prop_id,
                                     GValue *value, GParamSpec *pspec) {
  const X3dGuiIpc *self = X3D_GUI_IPC(object);
  switch (prop_id) {
  case PROP_CAPS:
    g_value_set_uint64(value, self->caps);
    break;
  case PROP_FORCE_UNSUPPORTED:
    g_value_set_boolean(value, self->force_unsupported);
    break;
  case PROP_ADV_ENABLED:
    g_value_set_boolean(value, self->adv_enabled);
    break;
  case PROP_DEV_ENABLED:
    g_value_set_boolean(value, self->dev_enabled);
    break;
  case PROP_AOD_SUPPORTED:
    g_value_set_boolean(value, self->aod_supported);
    break;
  case PROP_AOD_REBOOT_PENDING:
    g_value_set_boolean(value, self->aod_reboot_pending);
    break;
  case PROP_UPDATE_INTERVAL_MS:
    g_value_set_uint(value, self->update_interval_ms);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static void x3d_gui_ipc_set_property(GObject *object, guint prop_id,
                                     const GValue *value, GParamSpec *pspec) {
  X3dGuiIpc *self = X3D_GUI_IPC(object);
  switch (prop_id) {
  case PROP_CAPS:
    x3d_gui_ipc_set_caps(self, g_value_get_uint64(value));
    break;
  case PROP_FORCE_UNSUPPORTED:
    x3d_gui_ipc_set_force_unsupported(self, g_value_get_boolean(value));
    break;
  case PROP_ADV_ENABLED:
    x3d_gui_ipc_set_adv_enabled(self, g_value_get_boolean(value));
    break;
  case PROP_DEV_ENABLED:
    x3d_gui_ipc_set_dev_enabled(self, g_value_get_boolean(value));
    break;
  case PROP_AOD_SUPPORTED:
    x3d_gui_ipc_set_aod_supported(self, g_value_get_boolean(value));
    break;
  case PROP_AOD_REBOOT_PENDING:
    x3d_gui_ipc_set_aod_reboot_pending(self, g_value_get_boolean(value));
    break;
  case PROP_UPDATE_INTERVAL_MS:
    x3d_gui_ipc_set_update_interval_ms(self, g_value_get_uint(value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static void x3d_gui_ipc_class_init(X3dGuiIpcClass *klass) {
  GObjectClass *gcls = G_OBJECT_CLASS(klass);
  gcls->get_property = x3d_gui_ipc_get_property;
  gcls->set_property = x3d_gui_ipc_set_property;

  obj_props[PROP_CAPS] = g_param_spec_uint64(
      "caps", "Capabilities",
      "Daemon-reported hardware and kernel capability bitmask", 0, G_MAXUINT64,
      0, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
  obj_props[PROP_FORCE_UNSUPPORTED] = g_param_spec_boolean(
      "force-unsupported", "Force Unsupported",
      "Bypass capability discovery logic", FALSE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
  obj_props[PROP_ADV_ENABLED] = g_param_spec_boolean(
      "adv-enabled", "Advanced Enabled", "Expose Advanced Configuration", FALSE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
  obj_props[PROP_DEV_ENABLED] = g_param_spec_boolean(
      "dev-enabled", "Developer Enabled", "Expose Developer Options", FALSE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
  obj_props[PROP_AOD_SUPPORTED] = g_param_spec_boolean(
      "aod-supported", "AOD Supported", "Host exposes amd-wmi-aod", FALSE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
  obj_props[PROP_AOD_REBOOT_PENDING] = g_param_spec_boolean(
      "aod-reboot-pending", "AOD Reboot Pending",
      "Firmware AOD settings staged for next reboot", FALSE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
  obj_props[PROP_UPDATE_INTERVAL_MS] = g_param_spec_uint(
      "update-interval-ms", "Update Interval (ms)",
      "Live telemetry polling interval in milliseconds", 250, 5000, 500,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties(gcls, N_PROPS, obj_props);
}

static void x3d_gui_ipc_init(X3dGuiIpc *self) {
  self->caps = 0;
  self->update_interval_ms = 500;
}

X3dGuiIpc *x3d_gui_ipc_new(void) {
  X3dGuiIpc *self = g_object_new(X3D_TYPE_GUI_IPC, NULL);
  X3dGuiThemePrefs prefs;
  x3d_ipc_load_theme_prefs(&prefs);
  self->update_interval_ms = prefs.update_interval_ms;

  self->adv_enabled = atoi(x3d_ipc_config_get("ADVANCED_CONFIG_ENABLE")) != 0;
  self->dev_enabled = atoi(x3d_ipc_config_get("DEV_ENABLE")) != 0;
  self->force_unsupported =
      atoi(x3d_ipc_config_get("FORCE_UNSUPPORTED_DISPLAY")) != 0;
  return self;
}

uint64_t x3d_gui_ipc_get_caps(const X3dGuiIpc *self) {
  return self ? self->caps : 0;
}

void x3d_gui_ipc_set_caps(X3dGuiIpc *self, uint64_t caps) {
  if (!self || self->caps == caps)
    return;
  self->caps = caps;
  g_object_notify_by_pspec(G_OBJECT(self), obj_props[PROP_CAPS]);
}

gboolean x3d_gui_ipc_get_force_unsupported(const X3dGuiIpc *self) {
  return self ? self->force_unsupported : FALSE;
}

void x3d_gui_ipc_set_force_unsupported(X3dGuiIpc *self, gboolean force) {
  if (!self || self->force_unsupported == force)
    return;
  self->force_unsupported = force;
  g_object_notify_by_pspec(G_OBJECT(self), obj_props[PROP_FORCE_UNSUPPORTED]);
}

gboolean x3d_gui_ipc_get_adv_enabled(const X3dGuiIpc *self) {
  return self ? self->adv_enabled : FALSE;
}

void x3d_gui_ipc_set_adv_enabled(X3dGuiIpc *self, gboolean enabled) {
  if (!self || self->adv_enabled == enabled)
    return;
  self->adv_enabled = enabled;
  g_object_notify_by_pspec(G_OBJECT(self), obj_props[PROP_ADV_ENABLED]);
}

gboolean x3d_gui_ipc_get_dev_enabled(const X3dGuiIpc *self) {
  return self ? self->dev_enabled : FALSE;
}

void x3d_gui_ipc_set_dev_enabled(X3dGuiIpc *self, gboolean enabled) {
  if (!self || self->dev_enabled == enabled)
    return;
  self->dev_enabled = enabled;
  g_object_notify_by_pspec(G_OBJECT(self), obj_props[PROP_DEV_ENABLED]);
}

gboolean x3d_gui_ipc_get_aod_supported(const X3dGuiIpc *self) {
  return self ? self->aod_supported : FALSE;
}

void x3d_gui_ipc_set_aod_supported(X3dGuiIpc *self, gboolean supported) {
  if (!self || self->aod_supported == supported)
    return;
  self->aod_supported = supported;
  g_object_notify_by_pspec(G_OBJECT(self), obj_props[PROP_AOD_SUPPORTED]);
}

gboolean x3d_gui_ipc_get_aod_reboot_pending(const X3dGuiIpc *self) {
  return self ? self->aod_reboot_pending : FALSE;
}

void x3d_gui_ipc_set_aod_reboot_pending(X3dGuiIpc *self, gboolean pending) {
  if (!self || self->aod_reboot_pending == pending)
    return;
  self->aod_reboot_pending = pending;
  g_object_notify_by_pspec(G_OBJECT(self), obj_props[PROP_AOD_REBOOT_PENDING]);
}

guint x3d_gui_ipc_get_update_interval_ms(const X3dGuiIpc *self) {
  return self ? self->update_interval_ms : 500;
}

void x3d_gui_ipc_set_update_interval_ms(X3dGuiIpc *self, guint ms) {
  if (!self)
    return;
  guint clamped = (guint)CLAMP((int)ms, 250, 5000);
  if (self->update_interval_ms == clamped)
    return;
  self->update_interval_ms = clamped;
  g_object_notify_by_pspec(G_OBJECT(self), obj_props[PROP_UPDATE_INTERVAL_MS]);
}

gboolean x3d_gui_ipc_has_cap(const X3dGuiIpc *self, uint64_t cap_mask) {
  if (!self || cap_mask == 0)
    return TRUE;
  if (self->force_unsupported)
    return TRUE;
  return (self->caps & cap_mask) == cap_mask;
}

/* ── IPC Framing & Transport ──────────────────────────────────── */

gboolean x3d_ipc_kv_get(const char *haystack, const char *key, char sep,
                        char *out, size_t out_sz) {
  if (!haystack || !key || !out || out_sz == 0)
    return FALSE;
  size_t klen = strlen(key);
  const char *p = haystack;
  while (*p) {
    if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
      const char *val = p + klen + 1;
      const char *end = strchr(val, sep);
      size_t len = end ? (size_t)(end - val) : strlen(val);
      if (len >= out_sz)
        len = out_sz - 1;
      memcpy(out, val, len);
      out[len] = '\0';
      return TRUE;
    }
    const char *next = strchr(p, sep);
    if (!next)
      break;
    p = next + 1;
  }
  return FALSE;
}

int x3d_ipc_send(const char *cmd, char *resp, size_t resp_len) {
  return socket_send(cmd, resp, resp_len);
}

gboolean x3d_ipc_cmd_ok(const char *cmd) {
  if (!cmd || !cmd[0])
    return FALSE;
  char resp[64] = {0};
  return socket_send(cmd, resp, sizeof(resp)) == 0 &&
         strncmp(resp, "OK", 2) == 0;
}

const char *x3d_ipc_config_get(const char *key) {
  static char val[256];
  val[0] = '\0';
  if (!key)
    return val;

  char cmd[128];
  printf_sn(cmd, sizeof(cmd), "GET_CONFIG %s", key);
  char ipc_resp[256] = {0};
  if (socket_send(cmd, ipc_resp, sizeof(ipc_resp)) == 0 &&
      ipc_resp[0] != '\0' && strcmp(ipc_resp, "ERR") != 0) {
    snprintf(val, sizeof(val), "%s", ipc_resp);
  }
  return val;
}

gboolean x3d_ipc_config_set(const char *key, const char *value) {
  if (!key || !value)
    return FALSE;
  char cmd[256];
  printf_sn(cmd, sizeof(cmd), "SET_CONFIG %s %s", key, value);
  return x3d_ipc_cmd_ok(cmd);
}

gboolean x3d_ipc_advanced_get(const char *sysctl_key, char *out,
                              size_t out_sz) {
  if (!sysctl_key || !out || out_sz == 0)
    return FALSE;
  out[0] = '\0';
  char cmd[256];
  printf_sn(cmd, sizeof(cmd), "ADVANCED_GET %s", sysctl_key);
  if (socket_send(cmd, out, out_sz) == 0 && out[0] != '\0' &&
      strcmp(out, "ERR") != 0) {
    return TRUE;
  }
  out[0] = '\0';
  return FALSE;
}

gboolean x3d_ipc_advanced_set(const char *sysctl_key, const char *value) {
  if (!sysctl_key || !value)
    return FALSE;
  char cmd[256];
  printf_sn(cmd, sizeof(cmd), "ADVANCED_SET %s %s", sysctl_key, value);
  return x3d_ipc_cmd_ok(cmd);
}

void x3d_ipc_read_aod_live(AodLiveStats *out) {
  if (!out)
    return;
  memset(out, 0, sizeof(*out));
  char resp[256] = {0};
  if (socket_send("AOD_LIVE", resp, sizeof(resp)) != 0 || resp[0] == '\0' ||
      strcmp(resp, "ERR") == 0) {
    return;
  }
  char v[64];
  if (x3d_ipc_kv_get(resp, "WMI", '|', v, sizeof(v)))
    out->wmi_online = atoi(v);
  if (x3d_ipc_kv_get(resp, "PPT", '|', v, sizeof(v)))
    (void)sscanf(v, "%d,%lf,%lf", &out->has_ppt, &out->ppt_val_w,
                 &out->ppt_lim_w);
  if (x3d_ipc_kv_get(resp, "TDC", '|', v, sizeof(v)))
    (void)sscanf(v, "%d,%lf,%lf", &out->has_tdc, &out->tdc_val_a,
                 &out->tdc_lim_a);
  if (x3d_ipc_kv_get(resp, "EDC", '|', v, sizeof(v)))
    (void)sscanf(v, "%d,%lf,%lf", &out->has_edc, &out->edc_val_a,
                 &out->edc_lim_a);
  if (x3d_ipc_kv_get(resp, "MEM", '|', v, sizeof(v)))
    (void)sscanf(v, "%d,%d", &out->has_mem, &out->mem_mhz);
}

int x3d_ipc_read_topology_masks(char *cache_out, size_t cache_sz,
                                char *freq_out, size_t freq_sz,
                                int *ccd1_start, int *total_cores) {
  if (cache_out && cache_sz > 0)
    cache_out[0] = '\0';
  if (freq_out && freq_sz > 0)
    freq_out[0] = '\0';
  if (ccd1_start)
    *ccd1_start = 0;
  if (total_cores)
    *total_cores = 0;

  char resp[512] = {0};
  if (socket_send("GET_TOPOLOGY", resp, sizeof(resp)) != 0 || resp[0] == '\0' ||
      strcmp(resp, "ERR") == 0) {
    return -1;
  }

  if (cache_out && cache_sz > 0)
    x3d_ipc_kv_get(resp, "CACHE", ';', cache_out, cache_sz);
  if (freq_out && freq_sz > 0)
    x3d_ipc_kv_get(resp, "FREQ", ';', freq_out, freq_sz);

  char num[32] = {0};
  if (ccd1_start && x3d_ipc_kv_get(resp, "CCD1", ';', num, sizeof(num)))
    *ccd1_start = atoi(num);
  if (total_cores && x3d_ipc_kv_get(resp, "TOTAL", ';', num, sizeof(num)))
    *total_cores = atoi(num);
  return 0;
}

int x3d_ipc_read_topology(char *mask_out, size_t mask_sz, int *ccd1_start,
                          int *total_cores) {
  if (!mask_out || mask_sz == 0)
    return -1;
  char hw_cache[128] = {0}, hw_freq[128] = {0};
  if (x3d_ipc_read_topology_masks(hw_cache, sizeof(hw_cache), hw_freq,
                                  sizeof(hw_freq), ccd1_start,
                                  total_cores) != 0) {
    return -1;
  }
  const char *saved = x3d_ipc_config_get("AFFINITY_MASK");
  if (saved[0] && strcmp(saved, "none") != 0 && strcmp(saved, "ERR") != 0)
    snprintf(mask_out, mask_sz, "%s", saved);
  else
    snprintf(mask_out, mask_sz, "%s", hw_cache);
  return 0;
}

/* ── Desktop-Session Process & Tool Launchers ─────────────────── */

void x3d_ipc_spawn(char **args, char **env) {
  gchar **new_env = g_get_environ();
  if (env) {
    for (int i = 0; env[i] != NULL; i++) {
      char *eq = strchr(env[i], '=');
      if (eq) {
        gchar *key = g_strndup(env[i], (gsize)(eq - env[i]));
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

void x3d_ipc_spawn_terminal(const char *cmd_str) {
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
  if (!found)
    return;
  g_free(found_path);

  char *env[] = {(char *)"X3D_EXEC=1", (char *)"X3D_GUI_TERM=1", NULL};
  if (strcmp(found, "gnome-terminal") == 0 || strcmp(found, "ptyxis") == 0) {
    char *args[] = {(char *)found, (char *)"--",    (char *)"/bin/sh",
                    (char *)"-c",  (char *)cmd_str, NULL};
    x3d_ipc_spawn(args, env);
  } else if (strcmp(found, "xfce4-terminal") == 0) {
    gchar *xfce_cmd = g_strdup_printf("/bin/sh -c \"%s\"", cmd_str);
    char *args[] = {(char *)found, (char *)"-e", xfce_cmd, NULL};
    x3d_ipc_spawn(args, env);
    g_free(xfce_cmd);
  } else {
    char *args[] = {(char *)found, (char *)"-e",    (char *)"/bin/sh",
                    (char *)"-c",  (char *)cmd_str, NULL};
    x3d_ipc_spawn(args, env);
  }
}

gboolean x3d_ipc_is_tool_available(const char *bin_name,
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

void x3d_ipc_launch_desktop_tool(const char *bin_name, const char *arg1,
                                 const char *desktop_id) {
  gchar *bin = g_find_program_in_path(bin_name);
  if (bin) {
    g_free(bin);
    char *argv[3];
    argv[0] = (char *)bin_name;
    argv[1] = (char *)arg1;
    argv[2] = NULL;
    g_spawn_async(NULL, argv, NULL,
                  G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
                      G_SPAWN_STDERR_TO_DEV_NULL,
                  NULL, NULL, NULL, NULL);
    return;
  }
  GDesktopAppInfo *app = g_desktop_app_info_new(desktop_id);
  if (app) {
    g_app_info_launch(G_APP_INFO(app), NULL, NULL, NULL);
    g_object_unref(app);
  }
}

/* ── User GUI Preferences (~/.config/x3d-toggle/gui.conf) ─────── */

void x3d_ipc_load_theme_prefs(X3dGuiThemePrefs *prefs) {
  if (!prefs)
    return;
  prefs->theme_idx = 0;
  prefs->color_idx = 0;
  prefs->font_idx = 0;
  prefs->font_size_pt = 11;
  prefs->update_interval_ms = 500;

  gchar *conf_path =
      g_build_filename(g_get_user_config_dir(), "x3d-toggle", "gui.conf", NULL);
  gchar *contents = NULL;
  if (g_file_get_contents(conf_path, &contents, NULL, NULL) && contents) {
    gchar **lines = g_strsplit(contents, "\n", -1);
    for (int i = 0; lines[i]; i++) {
      const gchar *ln = g_strstrip(lines[i]);
      if (g_str_has_prefix(ln, "THEME=")) {
        const char *tv = ln + 6;
        if (strcmp(tv, "imperial") == 0)
          prefs->theme_idx = 1;
        else if (strcmp(tv, "lact") == 0)
          prefs->theme_idx = 2;
        else if (strcmp(tv, "light") == 0)
          prefs->theme_idx = 3;
        else if (strcmp(tv, "dark") == 0)
          prefs->theme_idx = 4;
        else if (strcmp(tv, "system") == 0)
          prefs->theme_idx = 5;
        else
          prefs->theme_idx = 0;
      } else if (g_str_has_prefix(ln, "COLOR_IDX=")) {
        prefs->color_idx = (guint)CLAMP(atoi(ln + 10), 0, 5);
      } else if (g_str_has_prefix(ln, "FONT_IDX=")) {
        prefs->font_idx = (guint)CLAMP(atoi(ln + 9), 0, 4);
      } else if (g_str_has_prefix(ln, "FONT_SIZE=")) {
        prefs->font_size_pt = CLAMP(atoi(ln + 10), 9, 18);
      } else if (g_str_has_prefix(ln, "UPDATE_INTERVAL_MS=")) {
        prefs->update_interval_ms = (guint)CLAMP(atoi(ln + 19), 250, 5000);
      }
    }
    g_strfreev(lines);
    g_free(contents);
  }
  g_free(conf_path);
}

gboolean x3d_ipc_save_theme_prefs(const X3dGuiThemePrefs *prefs) {
  if (!prefs)
    return FALSE;
  static const char *const k_theme_ids[] = {"obsidian", "imperial", "lact",
                                            "light",    "dark",     "system"};
  const char *tid = (prefs->theme_idx < G_N_ELEMENTS(k_theme_ids))
                        ? k_theme_ids[prefs->theme_idx]
                        : "obsidian";
  guint interval_ms = (guint)CLAMP((int)prefs->update_interval_ms, 250, 5000);
  gboolean ok = FALSE;
  gchar *conf_dir =
      g_build_filename(g_get_user_config_dir(), "x3d-toggle", NULL);
  if (g_mkdir_with_parents(conf_dir, 0755) == 0) {
    gchar *conf_path = g_build_filename(conf_dir, "gui.conf", NULL);
    gchar *body = g_strdup_printf(
        "THEME=%s\nCOLOR_IDX=%u\nFONT_IDX=%u\nFONT_SIZE=%d\n"
        "UPDATE_INTERVAL_MS=%u\n",
        tid, prefs->color_idx, prefs->font_idx, prefs->font_size_pt,
        interval_ms);
    ok = g_file_set_contents(conf_path, body, -1, NULL);
    g_free(body);
    g_free(conf_path);
  }
  g_free(conf_dir);
  return ok;
}
