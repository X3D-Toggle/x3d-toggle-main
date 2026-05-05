/* IRQ Binding for the X3D Toggle Project
 * `irq.c`
 * Handles GPU/peripheral IRQ affinity binding, irqbalance lifecycle,
 * MSI verification, udev rule generation, coalescing hints, watch mode,
 * and graceful teardown. Reads configuration from the runtime daemon config.
 */

#include "xui.h"
#include "libc.h"
#include "error.h"
#include "irq.h"
#include "ccd.h"

#ifndef CONFIG_PATH
#define CONFIG_PATH "/etc/x3d-toggle.d/daemon.conf"
#endif

typedef struct {
  int target_core;
  int irq_enable;
  int irq_pinning;
  int irq_gpu;
  int irq_nvme;
  int irq_usb;
  int irq_nic;
  int irq_audio;
  int irq_coalesce;
  int irq_watch;
  int thread_control;
  int thread_cap;
  char thread_exempt[512];
} irq_config_t;

typedef struct {
  int irqs[IRQ_MAX_VECTORS];
  int count;
} irq_list_t;

static irq_config_t g_cfg;
static irq_list_t g_gpu_irqs;
static irq_list_t g_nvme_irqs;
static irq_list_t g_usb_irqs;
static irq_list_t g_nic_irqs;
static irq_list_t g_audio_irqs;
static char g_nic_names[8][32];
static int  g_nic_count;


static int read_file_line(const char *path, char *buf, size_t len) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) return -1;
  ssize_t n = read(fd, buf, len - 1);
  close(fd);
  if (n <= 0) return -1;
  buf[n] = '\0';
  for (ssize_t i = n - 1; i >= 0 && (buf[i] == '\n' || buf[i] == '\r'); i--)
    buf[i] = '\0';
  return 0;
}

static int write_file(const char *path, const char *val) {
  int fd = open(path, O_WRONLY);
  if (fd < 0) return -1;
  ssize_t n = write(fd, val, strlen(val));
  close(fd);
  return (n > 0) ? 0 : -1;
}

static int file_exists(const char *path) {
  return access(path, F_OK) == 0;
}

static int parse_conf_int(const char *haystack, const char *key) {
  char search[128];
  printf_sn(search, sizeof(search), "%s=", key);
  const char *p = strstr(haystack, search);
  if (!p) return 0;
  return atoi(p + strlen(search));
}

static void parse_conf_str(const char *haystack, const char *key,
                           char *out, size_t len) {
  char search[128];
  printf_sn(search, sizeof(search), "%s=", key);
  const char *p = strstr(haystack, search);
  if (!p) { out[0] = '\0'; return; }
  p += strlen(search);
  size_t i = 0;
  while (p[i] && p[i] != '\n' && p[i] != '\r' && i < len - 1) {
    out[i] = p[i];
    i++;
  }
  out[i] = '\0';
}

static int load_config(void) {
  char buf[4096] = {0};
  int fd = open(CONFIG_PATH, O_RDONLY);
  if (fd < 0) {
    journal_warn(ERR_CONF);
    return ERR_CONF;
  }
  ssize_t n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0) return ERR_CONF;
  buf[n] = '\0';

  g_cfg.irq_enable   = parse_conf_int(buf, "IRQ_ENABLE");
  g_cfg.irq_pinning  = parse_conf_int(buf, "IRQ_PINNING");
  g_cfg.irq_gpu      = parse_conf_int(buf, "IRQ_GPU");
  g_cfg.irq_nvme     = parse_conf_int(buf, "IRQ_NVME");
  g_cfg.irq_usb      = parse_conf_int(buf, "IRQ_USB");
  g_cfg.irq_nic      = parse_conf_int(buf, "IRQ_NIC");
  g_cfg.irq_audio    = parse_conf_int(buf, "IRQ_AUDIO");
  g_cfg.irq_coalesce = parse_conf_int(buf, "IRQ_COALESCE");
  g_cfg.irq_watch    = parse_conf_int(buf, "IRQ_WATCH");
  g_cfg.thread_control = parse_conf_int(buf, "THREAD_CONTROL");
  g_cfg.thread_cap   = parse_conf_int(buf, "THREAD_CAP");

  parse_conf_str(buf, "IRQ_CUSTOM", buf, sizeof(buf));
  g_cfg.target_core = atoi(buf);

  parse_conf_str(buf, "THREAD_EXEMPT", g_cfg.thread_exempt,
                 sizeof(g_cfg.thread_exempt));
  return ERR_SUCCESS;
}

static int resolve_target_core(void) {
  switch (g_cfg.irq_pinning) {
  case 0: return 0;
  case 1: {
    char buf[16] = {0};
    if (read_file_line(IRQ_STATE_DIR "/irq_target_core", buf, sizeof(buf)) == 0) {
        return atoi(buf);
    }
    return 0;
  }
  case 2: return g_cfg.target_core;
  case 3: return -1;
  default: return 0;
  }
}

static int disable_irqbalance(void) {
  int ret = system("systemctl is-active --quiet irqbalance 2>/dev/null");
  if (WIFEXITED(ret) && WEXITSTATUS(ret) == 0) {
    mkdir(IRQ_STATE_DIR, 0755);
    int fd = open(IRQ_BALANCER_STATE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
      write(fd, "1", 1);
      close(fd);
    }
    ret = system("systemctl stop irqbalance 2>/dev/null");
    if (WIFEXITED(ret) && WEXITSTATUS(ret) != 0) {
      journal_warn(ERR_IRQ_BALANCER, "stop", "systemctl returned non-zero");
    }
  }
  return ERR_SUCCESS;
}

static int restore_irqbalance(void) {
  if (file_exists(IRQ_BALANCER_STATE)) {
    int ret = system("systemctl start irqbalance 2>/dev/null");
    if (WIFEXITED(ret) && WEXITSTATUS(ret) != 0) {
      journal_warn(ERR_IRQ_BALANCER, "start", "systemctl returned non-zero");
    }
    unlink(IRQ_BALANCER_STATE);
  }
  return ERR_SUCCESS;
}

static void discover_irqs(const char *driver_token, irq_list_t *list) {
  list->count = 0;
  int fd = open("/proc/interrupts", O_RDONLY);
  if (fd < 0) return;

  char buf[8192];
  ssize_t n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0) return;
  buf[n] = '\0';

  char *line = buf;
  while (line && *line && list->count < IRQ_MAX_VECTORS) {
    char *nl = strchr(line, '\n');
    if (nl) *nl = '\0';

    if (strstr(line, driver_token)) {
      int irq = atoi(line);
      if (irq > 0) {
        list->irqs[list->count++] = irq;
      }
    }

    if (nl) line = nl + 1;
    else break;
  }
}

static int verify_msi(int irq) {
  char affinity_path[256];
  printf_sn(affinity_path, sizeof(affinity_path),
            "/proc/irq/%d/amdgpu", irq);
  char msi_check[512];
  printf_sn(msi_check, sizeof(msi_check),
            "grep -l 'PCI-MSI' /proc/irq/%d/../type 2>/dev/null", irq);
  int fd = open("/proc/interrupts", O_RDONLY);
  if (fd < 0) return 0;

  char buf[8192];
  ssize_t n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0) return 0;
  buf[n] = '\0';

  char irq_str[16];
  printf_sn(irq_str, sizeof(irq_str), "%d:", irq);
  char *line = strstr(buf, irq_str);
  if (line && (strstr(line, "PCI-MSI") || strstr(line, "PCI-MSIX")))
    return 1;

  return 0;
}

static int count_msix_vectors(void) {
  int count = 0;
  int fd = open("/proc/interrupts", O_RDONLY);
  if (fd < 0) return 0;

  char buf[8192];
  ssize_t n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0) return 0;
  buf[n] = '\0';

  char *p = buf;
  while ((p = strstr(p, "amdgpu")) != NULL) {
    if (strstr(p - 80 > buf ? p - 80 : buf, "PCI-MSIX"))
      count++;
    p++;
  }
  return count;
}

static int save_original_mask(int irq) {
  char path[256], orig_path[256], mask[64] = {0};
  printf_sn(path, sizeof(path), "/proc/irq/%d/smp_affinity_list", irq);
  printf_sn(orig_path, sizeof(orig_path),
            IRQ_STATE_DIR "/irq_orig_%d", irq);

  if (read_file_line(path, mask, sizeof(mask)) == 0) {
    mkdir(IRQ_STATE_DIR, 0755);
    int fd = open(orig_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
      write(fd, mask, strlen(mask));
      close(fd);
    }
  }
  return ERR_SUCCESS;
}

static int apply_affinity(int irq, int core) {
  save_original_mask(irq);

  char path[256], val[16];
  printf_sn(path, sizeof(path), "/proc/irq/%d/smp_affinity_list", irq);
  printf_sn(val, sizeof(val), "%d", core);

  if (write_file(path, val) < 0) {
    journal_warn(ERR_IRQ_BIND, irq, "write failed");
    return ERR_IRQ_BIND;
  }
  return ERR_SUCCESS;
}

static int restore_affinity(int irq) {
  char orig_path[256], mask[64] = {0}, path[256];
  printf_sn(orig_path, sizeof(orig_path),
            IRQ_STATE_DIR "/irq_orig_%d", irq);
  printf_sn(path, sizeof(path), "/proc/irq/%d/smp_affinity_list", irq);

  if (read_file_line(orig_path, mask, sizeof(mask)) == 0) {
    if (write_file(path, mask) < 0) {
      journal_warn(ERR_IRQ_TEARDOWN, irq);
    }
    unlink(orig_path);
  }
  return ERR_SUCCESS;
}

static void apply_list(irq_list_t *list, int core) {
  for (int i = 0; i < list->count; i++) {
    apply_affinity(list->irqs[i], core);
  }
}

static void discover_nic_names(void) {
  g_nic_count = 0;
  int fd = open("/proc/interrupts", O_RDONLY);
  if (fd < 0) return;

  char buf[8192];
  ssize_t n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0) return;
  buf[n] = '\0';

  const char *tokens[] = {"eth", "enp", "eno", "wlan"};
  char *p = buf;
  while (p && *p && g_nic_count < 8) {
    char *nl = strchr(p, '\n');
    if (nl) *nl = '\0';
    for (int t = 0; t < 4; t++) {
      char *found = strstr(p, tokens[t]);
      if (found) {
        int j = 0;
        while (found[j] && found[j] != ' ' && found[j] != '\n' &&
               found[j] != '-' && j < 31) {
          g_nic_names[g_nic_count][j] = found[j];
          j++;
        }
        g_nic_names[g_nic_count][j] = '\0';
        g_nic_count++;
        break;
      }
    }
    if (nl) p = nl + 1;
    else break;
  }
}

static void apply_coalescing(int enable) {
  for (int i = 0; i < g_nic_count; i++) {
    char cmd[256];
    if (enable) {
      printf_sn(cmd, sizeof(cmd),
                "ethtool -C %s rx-usecs 0 tx-usecs 0 2>/dev/null",
                g_nic_names[i]);
    } else {
      printf_sn(cmd, sizeof(cmd),
                "ethtool -C %s rx-usecs 50 tx-usecs 50 2>/dev/null",
                g_nic_names[i]);
    }
    int ret = system(cmd);
    if (enable && WIFEXITED(ret) && WEXITSTATUS(ret) != 0) {
      journal_warn(ERR_IRQ_COALESCE, g_nic_names[i]);
    }
  }
}

int irq_init(void) {
  int rc = load_config();
  if (rc != ERR_SUCCESS) return rc;

  if (!g_cfg.irq_enable) return ERR_SUCCESS;
  if (g_cfg.irq_pinning == 3) return ERR_SUCCESS;

  g_cfg.target_core = resolve_target_core();
  if (g_cfg.target_core < 0) return ERR_SUCCESS;

  disable_irqbalance();

  memset(&g_gpu_irqs, 0, sizeof(g_gpu_irqs));
  memset(&g_nvme_irqs, 0, sizeof(g_nvme_irqs));
  memset(&g_usb_irqs, 0, sizeof(g_usb_irqs));
  memset(&g_nic_irqs, 0, sizeof(g_nic_irqs));
  memset(&g_audio_irqs, 0, sizeof(g_audio_irqs));

  return ERR_SUCCESS;
}

int irq_bind_gpu(void) {
  if (!g_cfg.irq_enable || g_cfg.irq_pinning == 3) return ERR_SUCCESS;
  if (!g_cfg.irq_gpu) return ERR_SUCCESS;

  discover_irqs("amdgpu", &g_gpu_irqs);
  if (g_gpu_irqs.count == 0) {
    discover_irqs("nouveau", &g_gpu_irqs);
  }

  if (g_gpu_irqs.count == 0) {
    journal_warn(ERR_IRQ_DISCOVER, "GPU");
    return ERR_SUCCESS; /* Non-fatal */
  }

  irq_list_t verified = {.count = 0};
  for (int i = 0; i < g_gpu_irqs.count; i++) {
    if (verify_msi(g_gpu_irqs.irqs[i])) {
      verified.irqs[verified.count++] = g_gpu_irqs.irqs[i];
    } else {
      journal_warn(ERR_IRQ_NO_MSI, "GPU IRQ (legacy APIC pin)");
    }
  }
  g_gpu_irqs = verified;

  apply_list(&g_gpu_irqs, g_cfg.target_core);
  return ERR_SUCCESS;
}

int irq_bind_peripherals(void) {
  if (!g_cfg.irq_enable || g_cfg.irq_pinning == 3) return ERR_SUCCESS;

  if (g_cfg.irq_nvme) {
    discover_irqs("nvme", &g_nvme_irqs);
    if (g_nvme_irqs.count > 0)
      apply_list(&g_nvme_irqs, g_cfg.target_core);
  }

  if (g_cfg.irq_usb) {
    discover_irqs("xhci_hcd", &g_usb_irqs);
    if (g_usb_irqs.count > 0)
      apply_list(&g_usb_irqs, g_cfg.target_core);
  }

  if (g_cfg.irq_nic) {
    discover_irqs("eth", &g_nic_irqs);
    if (g_nic_irqs.count == 0)
      discover_irqs("enp", &g_nic_irqs);
    if (g_nic_irqs.count > 0) {
      apply_list(&g_nic_irqs, g_cfg.target_core);
      if (g_cfg.irq_coalesce) {
        discover_nic_names();
        apply_coalescing(1);
      }
    }
  }

  if (g_cfg.irq_audio) {
    discover_irqs("snd_hda", &g_audio_irqs);
    if (g_audio_irqs.count == 0)
      discover_irqs("snd-hda", &g_audio_irqs);
    if (g_audio_irqs.count > 0)
      apply_list(&g_audio_irqs, g_cfg.target_core);
  }

  return ERR_SUCCESS;
}

int irq_gen_udev(void) {
  if (!g_cfg.irq_enable || g_cfg.irq_pinning == 3) return ERR_SUCCESS;

  int fd = open(IRQ_UDEV_RULES, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    journal_error(ERR_IRQ_UDEV, "permission denied or path missing");
    return ERR_IRQ_UDEV;
  }

  char header[] = "# X3D Toggle IRQ Affinity Rules (auto-generated)\n"
                  "# Re-asserts IRQ affinity on device add/reset events\n\n";
  write(fd, header, strlen(header));

  char rule[256];
  int len;

  if (g_cfg.irq_gpu) {
    len = printf_sn(rule, sizeof(rule),
      "ACTION==\"add\", SUBSYSTEM==\"pci\", "
      "DRIVER==\"amdgpu|nouveau\", "
      "RUN+=\"/usr/bin/x3d-toggle irq-bind\"\n");
    write(fd, rule, len);
  }

  if (g_cfg.irq_nvme) {
    len = printf_sn(rule, sizeof(rule),
      "ACTION==\"add\", SUBSYSTEM==\"pci\", "
      "DRIVER==\"nvme\", "
      "RUN+=\"/usr/bin/x3d-toggle irq-bind\"\n");
    write(fd, rule, len);
  }

  if (g_cfg.irq_usb) {
    len = printf_sn(rule, sizeof(rule),
      "ACTION==\"add\", SUBSYSTEM==\"pci\", "
      "DRIVER==\"xhci_hcd\", "
      "RUN+=\"/usr/bin/x3d-toggle irq-bind\"\n");
    write(fd, rule, len);
  }

  close(fd);

  system("udevadm control --reload-rules 2>/dev/null");
  return ERR_SUCCESS;
}

int irq_teardown(void) {
  irq_list_t *lists[] = {
    &g_gpu_irqs, &g_nvme_irqs, &g_usb_irqs, &g_nic_irqs, &g_audio_irqs
  };
  for (int l = 0; l < 5; l++) {
    for (int i = 0; i < lists[l]->count; i++) {
      restore_affinity(lists[l]->irqs[i]);
    }
  }

  if (g_cfg.irq_coalesce && g_nic_count > 0) {
    apply_coalescing(0);
  }

  if (file_exists(IRQ_UDEV_RULES)) {
    unlink(IRQ_UDEV_RULES);
    system("udevadm control --reload-rules 2>/dev/null");
  }

  restore_irqbalance();

  return ERR_SUCCESS;
}

int irq_status(void) {
  char header[] = "\n  IRQ Affinity Status\n"
                  "  ─────────────────────────────────────────\n";
  write(FILENO_STDOUT, header, strlen(header));

  int msix = count_msix_vectors();
  char msix_line[128];
  int len = printf_sn(msix_line, sizeof(msix_line),
                      "  GPU MSI-X Vectors: %d\n\n", msix);
  write(FILENO_STDOUT, msix_line, len);

  irq_list_t *lists[] = {
    &g_gpu_irqs, &g_nvme_irqs, &g_usb_irqs, &g_nic_irqs, &g_audio_irqs
  };
  const char *labels[] = {"GPU", "NVMe", "USB/xHCI", "NIC", "Audio"};

  for (int l = 0; l < 5; l++) {
    if (lists[l]->count == 0) continue;
    char label_line[128];
    len = printf_sn(label_line, sizeof(label_line),
                    "  [%s] %d vectors\n", labels[l], lists[l]->count);
    write(FILENO_STDOUT, label_line, len);

    for (int i = 0; i < lists[l]->count; i++) {
      char path[256], mask[64] = {0};
      printf_sn(path, sizeof(path), "/proc/irq/%d/smp_affinity_list",
                lists[l]->irqs[i]);
      read_file_line(path, mask, sizeof(mask));
      char line[128];
      len = printf_sn(line, sizeof(line),
                      "    IRQ %3d → Core(s): %s\n",
                      lists[l]->irqs[i], mask[0] ? mask : "N/A");
      write(FILENO_STDOUT, line, len);
    }
  }

  char bal_line[128];
  len = printf_sn(bal_line, sizeof(bal_line),
                  "\n  irqbalance: %s\n",
                  file_exists(IRQ_BALANCER_STATE) ?
                  "Disabled (by X3D Toggle)" : "Active/Unmanaged");
  write(FILENO_STDOUT, bal_line, len);

  return ERR_SUCCESS;
}

int irq_watch(void) {
  if (!g_cfg.irq_enable || g_cfg.irq_pinning == 3) return ERR_SUCCESS;

  int core = g_cfg.target_core;
  char expected[16];
  printf_sn(expected, sizeof(expected), "%d", core);

  journal_file("INF", "IRQ Watch", "Watch mode started");

  char full_mask_str[32];
  int ccd1_start = 0, total_cores = 0;
  char mask_tmp[64] = {0};
  ccd(mask_tmp, &ccd1_start, &total_cores);
  if (total_cores > 0) {
    printf_sn(full_mask_str, sizeof(full_mask_str), "0-%d", total_cores - 1);
  } else {
    printf_sn(full_mask_str, sizeof(full_mask_str), "0");
  }

  while (1) {
    irq_list_t *lists[] = {
      &g_gpu_irqs, &g_nvme_irqs, &g_usb_irqs, &g_nic_irqs, &g_audio_irqs
    };
    for (int l = 0; l < 5; l++) {
      for (int i = 0; i < lists[l]->count; i++) {
        char path[256], current[64] = {0};
        printf_sn(path, sizeof(path), "/proc/irq/%d/smp_affinity_list", lists[l]->irqs[i]);
        if (read_file_line(path, current, sizeof(current)) == 0) {
          if (strcmp(current, expected) != 0) {
            write_file(path, expected);
            journal_debug("IRQ Watch: Re-asserted IRQ %d to core %d", lists[l]->irqs[i], core);
          }
        }
      }
    }

    if (g_cfg.thread_control) {
      char *exemptions = g_cfg.thread_exempt;
      char *ex = exemptions;
      char token[64];
      
      while (ex && *ex) {
        int j = 0;
        while (*ex && *ex != ',' && *ex != ' ' && j < 63) {
          token[j++] = *ex++;
        }
        token[j] = '\0';
        if (*ex == ',' || *ex == ' ') ex++;
        
        if (token[0]) {
          char cmd[512];
          printf_sn(cmd, sizeof(cmd), 
            "for p in $(pidof %s 2>/dev/null); do "
            "  taskset -p -c %s $p >/dev/null 2>&1; "
            "done", token, full_mask_str);
          system(cmd);
        }
      }
    }

    struct timespec ts = {.tv_sec = 5, .tv_nsec = 0};
    nanosleep(&ts, NULL);
  }

  return ERR_SUCCESS;
}

int cli_irq_bind(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  int rc = irq_init();
  if (rc != ERR_SUCCESS) return rc;
  rc = irq_bind_gpu();
  if (rc != ERR_SUCCESS) return rc;
  rc = irq_bind_peripherals();
  if (rc != ERR_SUCCESS) return rc;
  return irq_gen_udev();
}

int cli_irq_reset(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  return irq_teardown();
}

int cli_irq_status(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  return irq_status();
}

int cli_irq_udev(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  int rc = irq_init();
  if (rc != ERR_SUCCESS) return rc;
  return irq_gen_udev();
}

int cli_irq_watch(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  int rc = irq_init();
  if (rc != ERR_SUCCESS) return rc;
  return irq_watch();
}

/* end of IRQ.C */