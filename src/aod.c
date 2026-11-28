/* AMD Overclocking (AOD) WMI Interface Module
 * `aod.c`
 * Implements detection, cached capability probing, firmware setup writes
 * (PPT, TDC, EDC, Mem Clock, Firmware SMT) for `amd-wmi-aod`, and live
 * PPT/TDC/EDC telemetry (ryzen_smu / hwmon / RAPL) independent of WMI.
 */

#include "aod.h"
#include "../build/xui.h"
#include "error.h"
#include "libc.h"

#ifdef GUI_BUILD
#include <dirent.h>
#endif

static int aod_read_str(const char *path, char *out, size_t out_sz) {
  if (!out || out_sz == 0)
    return -1;
  out[0] = '\0';
  int fd = open(path, O_RDONLY);
  if (fd < 0)
    return -1;
  ssize_t n = read(fd, out, out_sz - 1);
  close(fd);
  if (n <= 0) {
    out[0] = '\0';
    return -1;
  }
  out[n] = '\0';
  for (ssize_t i = 0; i < n; i++) {
    if (out[i] == '\r' || out[i] == '\n') {
      out[i] = '\0';
      break;
    }
  }
  return 0;
}

static int aod_read_ull(const char *dir, const char *attr,
                        unsigned long long *val_out) {
  char path[512];
  char buf[64];
  printf_sn(path, sizeof(path), "%s/%s", dir, attr);
  if (aod_read_str(path, buf, sizeof(buf)) != 0)
    return -1;
  char *endptr = NULL;
  errno = 0;
  unsigned long long v = strtoull(buf, &endptr, 10);
  if (endptr == buf || errno == ERANGE)
    return -1;
  *val_out = v;
  return 0;
}

static int aod_write_ull(const char *dir, const char *attr,
                         unsigned long long val) {
  char path[512];
  char buf[64];
  printf_sn(path, sizeof(path), "%s/%s", dir, attr);
  int len = printf_sn(buf, sizeof(buf), "%llu", val);
  if (len <= 0)
    return ERR_IO;
  if ((size_t)len >= sizeof(buf))
    len = (int)(sizeof(buf) - 1);
  int fd = open(path, O_WRONLY);
  if (fd < 0) {
    int err = errno;
    if (err == EACCES || err == EPERM)
      return ERR_PERMISSION;
    if (err == ENOENT || err == ENODEV)
      return ERR_UNSUPPORTED;
    return ERR_IO;
  }
  ssize_t written = write(fd, buf, (size_t)len);
  close(fd);
  return (written == (ssize_t)len) ? ERR_SUCCESS : ERR_IO;
}

static int aod_dir_has_driver_attrs(const char *dir) {
  static const char *const attrs[] = {
      "ppt_limit", "ppt",       "tdc_limit", "tdc",        "edc_limit",
      "edc",       "mem_clock", "memclk",    "smt_enable", "smt"};
  for (size_t i = 0; i < sizeof(attrs) / sizeof(attrs[0]); i++) {
    char p[512];
    printf_sn(p, sizeof(p), "%s/%s", dir, attrs[i]);
    if (access(p, F_OK) == 0)
      return 1;
  }
  return 0;
}

static int guid_prefix_match(const char *name) {
  size_t glen = strlen(AOD_WMI_GUID_UPPER);
  for (size_t i = 0; i < glen; i++) {
    if (!name[i])
      return 0;
    if (toupper((unsigned char)name[i]) !=
        (int)(unsigned char)AOD_WMI_GUID_UPPER[i])
      return 0;
  }
  /* Match exact GUID or instance-suffixed GUID (e.g. "...-0") */
  return (name[glen] == '\0' || name[glen] == '-');
}

int aod_resolve_dir(char *out, size_t out_sz) {
  if (!out || out_sz == 0)
    return -1;
  out[0] = '\0';

  /* Check common direct paths first before scanning /sys/bus/wmi/devices */
  static const char *const candidates[] = {
      AOD_WMI_BUS_DIR "/" AOD_WMI_GUID_UPPER "-0",
      AOD_WMI_BUS_DIR "/" AOD_WMI_GUID_UPPER,
      AOD_WMI_BUS_DIR "/" AOD_WMI_GUID_LOWER "-0",
      AOD_WMI_BUS_DIR "/" AOD_WMI_GUID_LOWER,
  };
  for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
    if (access(candidates[i], F_OK) == 0 &&
        aod_dir_has_driver_attrs(candidates[i])) {
      printf_sn(out, out_sz, "%s", candidates[i]);
      return 0;
    }
  }

#ifdef GUI_BUILD
  DIR *d = opendir(AOD_WMI_BUS_DIR);
  if (d) {
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
      if (guid_prefix_match(de->d_name)) {
        char cand[512];
        printf_sn(cand, sizeof(cand), "%s/%s", AOD_WMI_BUS_DIR, de->d_name);
        if (aod_dir_has_driver_attrs(cand)) {
          printf_sn(out, out_sz, "%s", cand);
          closedir(d);
          return 0;
        }
      }
    }
    closedir(d);
  }
#else
  int dfd = open(AOD_WMI_BUS_DIR, O_RDONLY);
  if (dfd >= 0) {
    char dbuf[2048];
    int nread;
    while ((nread = getdents64((unsigned int)dfd,
                               (struct linux_dirent64 *)dbuf,
                               sizeof(dbuf))) > 0) {
      int bpos = 0;
      while (bpos < nread) {
        struct linux_dirent64 *de = (struct linux_dirent64 *)(dbuf + bpos);
        if (guid_prefix_match(de->d_name)) {
          char cand[512];
          printf_sn(cand, sizeof(cand), "%s/%s", AOD_WMI_BUS_DIR, de->d_name);
          if (aod_dir_has_driver_attrs(cand)) {
            printf_sn(out, out_sz, "%s", cand);
            close(dfd);
            return 0;
          }
        }
        bpos += de->d_reclen;
      }
    }
    close(dfd);
  }
#endif

  return -1;
}

int aod_is_supported(void) {
  char dir[512];
  return (aod_resolve_dir(dir, sizeof(dir)) == 0) ? 1 : 0;
}

int aod_has_smt(void) {
  char dir[512];
  if (aod_resolve_dir(dir, sizeof(dir)) != 0)
    return 0;
  char p[560];
  printf_sn(p, sizeof(p), "%s/smt_enable", dir);
  if (access(p, F_OK) == 0)
    return 1;
  printf_sn(p, sizeof(p), "%s/smt", dir);
  return (access(p, F_OK) == 0) ? 1 : 0;
}

void aod_get_stock_defaults(int *ppt_w, int *tdc_a, int *edc_a, int *mem_mhz) {
  /* Default baseline: AM5 120W X3D spec */
  int ppt = 162;
  int tdc = 120;
  int edc = 180;
  int mem = 2600;

  char cpu_buf[4096];
  int fd = open("/proc/cpuinfo", O_RDONLY);
  if (fd >= 0) {
    ssize_t n = read(fd, cpu_buf, sizeof(cpu_buf) - 1);
    close(fd);
    if (n > 0) {
      cpu_buf[n] = '\0';
      if (strstr(cpu_buf, "9950X3D")) {
        ppt = 200;
        tdc = 160;
        edc = 225;
        mem = 2800;
      } else if (strstr(cpu_buf, "9900X3D") || strstr(cpu_buf, "9850X3D") ||
                 strstr(cpu_buf, "9800X3D") || strstr(cpu_buf, "9600X3D")) {
        ppt = 162;
        tdc = 120;
        edc = 180;
        mem = 2800;
      } else if (strstr(cpu_buf, "7950X3D") || strstr(cpu_buf, "7900X3D") ||
                 strstr(cpu_buf, "7800X3D")) {
        ppt = 162;
        tdc = 120;
        edc = 180;
        mem = 2600;
      } else if (strstr(cpu_buf, "7600X3D")) {
        ppt = 88;
        tdc = 75;
        edc = 150;
        mem = 2600;
      } else if (strstr(cpu_buf, "5800X3D") || strstr(cpu_buf, "5700X3D")) {
        ppt = 142;
        tdc = 95;
        edc = 140;
        mem = 1600;
      } else if (strstr(cpu_buf, "5600X3D")) {
        ppt = 142;
        tdc = 95;
        edc = 140;
        mem = 1600;
      }
    }
  }

  if (ppt_w)
    *ppt_w = ppt;
  if (tdc_a)
    *tdc_a = tdc;
  if (edc_a)
    *edc_a = edc;
  if (mem_mhz)
    *mem_mhz = mem;
}

typedef enum {
  AOD_UNIT_WATTS = 0,
  AOD_UNIT_AMPS = 1,
  AOD_UNIT_MHZ = 2
} AodKnobUnit;

typedef struct {
  const char *attr_name;
  int has_node;
  int writable;
  int has_step;
  unsigned long long raw_min;
  unsigned long long raw_max;
  unsigned long long raw_step;
  unsigned long long scale;
  int disp_min;
  int disp_max;
  int disp_step;
} AodKnobBounds;

typedef struct {
  const char *attr_name;
  unsigned long long raw_val;
} AodPendingWrite;

/* Dynamic runtime bounds guardrail:
 * - Reads <attr>_min and <attr>_max fresh from sysfs on every probe/write.
 * - Fails closed (writable = 0) if either *_min or *_max is missing, zero, or
 *   inverted, or if the target node lacks write permission — never falling back
 *   to hardcoded Ryzen defaults.
 * - Never opens the target knob for reading (ppt/tdc/edc are write-only in v1
 *   amd-aod-wmi).
 */
static void aod_probe_knob_bounds(const char *dir, const char *const *names,
                                  size_t n_names, AodKnobUnit unit,
                                  int default_disp_step, AodKnobBounds *out) {
  memset(out, 0, sizeof(*out));
  out->disp_step = (default_disp_step > 0) ? default_disp_step : 1;

  char path[560];
  const char *found = NULL;
  for (size_t i = 0; i < n_names; i++) {
    printf_sn(path, sizeof(path), "%s/%s", dir, names[i]);
    if (access(path, F_OK) == 0) {
      found = names[i];
      break;
    }
  }
  if (!found)
    return;

  out->attr_name = found;
  out->has_node = 1;

  if (access(path, W_OK) != 0) {
    journal_debug("AOD Guardrail: %s/%s is not writable; failing closed", dir,
                  found);
    return;
  }

  char min_attr[64], max_attr[64], step_attr[64];
  printf_sn(min_attr, sizeof(min_attr), "%s_min", found);
  printf_sn(max_attr, sizeof(max_attr), "%s_max", found);
  printf_sn(step_attr, sizeof(step_attr), "%s_step", found);

  unsigned long long rmin = 0, rmax = 0, rstep = 0;
  if (aod_read_ull(dir, min_attr, &rmin) != 0 ||
      aod_read_ull(dir, max_attr, &rmax) != 0 || rmin == 0 || rmax == 0 ||
      rmin > rmax) {
    journal_debug(
        "AOD Guardrail: Missing or invalid %s/%s for %s; failing closed",
        min_attr, max_attr, found);
    return;
  }

  unsigned long long scale = 1ULL;
  if (unit == AOD_UNIT_WATTS || unit == AOD_UNIT_AMPS) {
    if (rmax >= 1000000ULL)
      scale = 1000000ULL; /* uW / uA */
    else if (rmax >= 2000ULL)
      scale = 1000ULL; /* mW / mA */
    else
      scale = 1ULL; /* W / A */
  } else {
    /* MHz (or kHz if >= 1,000,000) */
    scale = (rmax >= 1000000ULL) ? 1000ULL : 1ULL;
  }

  int dmin = (int)((rmin + scale - 1ULL) / scale);
  int dmax = (int)(rmax / scale);
  if (dmin <= 0 || dmax < dmin) {
    journal_debug("AOD Guardrail: Scaled bounds [%d..%d] invalid for %s", dmin,
                  dmax, found);
    return;
  }

  int dstep = (default_disp_step > 0) ? default_disp_step : 1;
  if (aod_read_ull(dir, step_attr, &rstep) == 0 && rstep > 0) {
    int calc = (int)((rstep >= scale) ? (rstep / scale) : 1ULL);
    dstep = (calc > 0) ? calc : 1;
    out->has_step = 1;
  } else {
    rstep = (unsigned long long)dstep * scale;
  }

  out->raw_min = rmin;
  out->raw_max = rmax;
  out->raw_step = rstep;
  out->scale = scale;
  out->disp_min = dmin;
  out->disp_max = dmax;
  out->disp_step = dstep;
  out->writable = 1;
}

static int aod_validate_guarded_knob(const char *dir, const char *label,
                                     const char *const *names, size_t n_names,
                                     AodKnobUnit unit, int default_disp_step,
                                     int requested_val,
                                     AodPendingWrite *pending_out) {
  if (requested_val <= 0 || !pending_out)
    return ERR_INVALID_ARGS;

  AodKnobBounds b;
  aod_probe_knob_bounds(dir, names, n_names, unit, default_disp_step, &b);
  if (!b.has_node || !b.writable || !b.attr_name) {
    journal_debug(
        "AOD Guardrail: Refusing write to %s (missing writable node or "
        "*_min/*_max bounds)",
        label);
    return ERR_UNSUPPORTED;
  }

  unsigned long long raw_val = (unsigned long long)requested_val * b.scale;
  if (requested_val < b.disp_min || requested_val > b.disp_max ||
      raw_val < b.raw_min || raw_val > b.raw_max) {
    journal_debug(
        "AOD Guardrail: Rejected %s=%d (outside dynamic firmware bounds "
        "[%d..%d])",
        label, requested_val, b.disp_min, b.disp_max);
    return ERR_INVALID_ARGS;
  }

  if (b.has_step && b.raw_step > 1ULL &&
      ((raw_val - b.raw_min) % b.raw_step) != 0) {
    journal_debug(
        "AOD Guardrail: Rejected %s=%d (not aligned to firmware step %d from "
        "min %d)",
        label, requested_val, b.disp_step, b.disp_min);
    return ERR_INVALID_ARGS;
  }

  pending_out->attr_name = b.attr_name;
  pending_out->raw_val = raw_val;
  return ERR_SUCCESS;
}

void aod_probe_state(AodState *st) {
  if (!st)
    return;
  memset(st, 0, sizeof(*st));
  st->smt_cur = -1;

  aod_get_stock_defaults(&st->ppt_stock_w, &st->tdc_stock_a, &st->edc_stock_a,
                         &st->mem_stock_mhz);

  /* Snapshot active board/BIOS limits from SMU telemetry when available */
  AodLiveStats live;
  aod_read_live_telemetry(&live, st);
  if (live.has_ppt && live.ppt_lim_w > 0.5)
    st->ppt_bios_w = (int)(live.ppt_lim_w + 0.5);
  if (live.has_tdc && live.tdc_lim_a > 0.5)
    st->tdc_bios_a = (int)(live.tdc_lim_a + 0.5);
  if (live.has_edc && live.edc_lim_a > 0.5)
    st->edc_bios_a = (int)(live.edc_lim_a + 0.5);
  if (live.has_mem && live.mem_mhz > 0)
    st->mem_cur_mhz = live.mem_mhz;

  char dir[512];
  if (aod_resolve_dir(dir, sizeof(dir)) != 0)
    return;

  st->supported = 1;
  static const char *const ppt_names[] = {"ppt_limit", "ppt"};
  static const char *const tdc_names[] = {"tdc_limit", "tdc"};
  static const char *const edc_names[] = {"edc_limit", "edc"};
  static const char *const mem_names[] = {"mem_clock", "memclk"};
  static const char *const smt_names[] = {"smt_enable", "smt"};

  AodKnobBounds kb;

  /* PPT (write-only in v1 amd-aod-wmi; bounds read fresh from *_min / *_max) */
  aod_probe_knob_bounds(dir, ppt_names, 2, AOD_UNIT_WATTS, 1, &kb);
  st->has_ppt = kb.has_node;
  st->ppt_writable = kb.writable;
  st->ppt_min_w = kb.disp_min;
  st->ppt_max_w = kb.disp_max;
  st->ppt_step_w = kb.disp_step;

  /* TDC (write-only in v1 amd-aod-wmi; bounds read fresh from *_min / *_max) */
  aod_probe_knob_bounds(dir, tdc_names, 2, AOD_UNIT_AMPS, 1, &kb);
  st->has_tdc = kb.has_node;
  st->tdc_writable = kb.writable;
  st->tdc_min_a = kb.disp_min;
  st->tdc_max_a = kb.disp_max;
  st->tdc_step_a = kb.disp_step;

  /* EDC (write-only in v1 amd-aod-wmi; bounds read fresh from *_min / *_max) */
  aod_probe_knob_bounds(dir, edc_names, 2, AOD_UNIT_AMPS, 1, &kb);
  st->has_edc = kb.has_node;
  st->edc_writable = kb.writable;
  st->edc_min_a = kb.disp_min;
  st->edc_max_a = kb.disp_max;
  st->edc_step_a = kb.disp_step;

  /* Memory Clock (read current value only if node exposes read permission) */
  aod_probe_knob_bounds(dir, mem_names, 2, AOD_UNIT_MHZ, 50, &kb);
  st->has_mem = kb.has_node;
  st->mem_writable = kb.writable;
  st->mem_min_mhz = kb.disp_min;
  st->mem_max_mhz = kb.disp_max;
  st->mem_step_mhz = kb.disp_step;
  if (kb.has_node && kb.attr_name) {
    char mpath[560];
    unsigned long long mv = 0;
    printf_sn(mpath, sizeof(mpath), "%s/%s", dir, kb.attr_name);
    if (access(mpath, R_OK) == 0 && aod_read_ull(dir, kb.attr_name, &mv) == 0 &&
        mv > 0) {
      st->mem_cur_mhz =
          (int)((kb.scale > 1ULL && mv >= kb.scale) ? (mv / kb.scale) : mv);
    }
  }

  /* Firmware SMT (0/1) */
  for (size_t i = 0; i < 2; i++) {
    char spath[560];
    unsigned long long sv = 0;
    printf_sn(spath, sizeof(spath), "%s/%s", dir, smt_names[i]);
    if (access(spath, F_OK) == 0) {
      st->has_smt = (access(spath, W_OK) == 0) ? 1 : 0;
      if (access(spath, R_OK) == 0 &&
          aod_read_ull(dir, smt_names[i], &sv) == 0) {
        st->smt_cur = (sv != 0) ? 1 : 0;
      }
      break;
    }
  }
}

void aod_format_ipc_info(const AodState *st, char *buf, size_t buf_sz) {
  if (!st || !buf || buf_sz == 0)
    return;
  printf_sn(
      buf, buf_sz,
      "SUPPORTED=%d|PPT=%d,%d,%d,%d,%d,%d,%d|TDC=%d,%d,%d,%d,%d,%d,%d|"
      "EDC=%d,%d,%d,%d,%d,%d,%d|MEM=%d,%d,%d,%d,%d,%d,%d|SMT=%d,%d",
      st->supported, st->has_ppt, st->ppt_writable, st->ppt_min_w,
      st->ppt_max_w, st->ppt_step_w, st->ppt_stock_w, st->ppt_bios_w,
      st->has_tdc, st->tdc_writable, st->tdc_min_a, st->tdc_max_a,
      st->tdc_step_a, st->tdc_stock_a, st->tdc_bios_a, st->has_edc,
      st->edc_writable, st->edc_min_a, st->edc_max_a, st->edc_step_a,
      st->edc_stock_a, st->edc_bios_a, st->has_mem, st->mem_writable,
      st->mem_min_mhz, st->mem_max_mhz, st->mem_step_mhz, st->mem_stock_mhz,
      st->mem_cur_mhz, st->has_smt, st->smt_cur);
}

int aod_apply_ppt_watts(int ppt_w) {
  if (ppt_w <= 0)
    return ERR_INVALID_ARGS;
  char dir[512];
  if (aod_resolve_dir(dir, sizeof(dir)) != 0)
    return ERR_UNSUPPORTED;
  static const char *const ppt_names[] = {"ppt_limit", "ppt"};
  AodPendingWrite pw;
  int r = aod_validate_guarded_knob(dir, "PPT", ppt_names, 2, AOD_UNIT_WATTS, 1,
                                    ppt_w, &pw);
  if (r != ERR_SUCCESS)
    return r;
  /* Write-only commit: never attempt read-back on write-only power node. */
  return aod_write_ull(dir, pw.attr_name, pw.raw_val);
}

int aod_apply_limits(int ppt_w, int tdc_a, int edc_a, int mem_mhz) {
  if (ppt_w <= 0 && tdc_a <= 0 && edc_a <= 0 && mem_mhz <= 0)
    return ERR_INVALID_ARGS;

  char dir[512];
  if (aod_resolve_dir(dir, sizeof(dir)) != 0)
    return ERR_UNSUPPORTED;

  static const char *const ppt_names[] = {"ppt_limit", "ppt"};
  static const char *const tdc_names[] = {"tdc_limit", "tdc"};
  static const char *const edc_names[] = {"edc_limit", "edc"};
  static const char *const mem_names[] = {"mem_clock", "memclk"};

  /* Phase 1: Probe and validate ALL requested knobs before any sysfs write
   * so an invalid/unaligned TDC, EDC, or Mem Clock never leaves PPT mutated. */
  AodPendingWrite pending[4];
  size_t pending_count = 0;

  if (ppt_w > 0) {
    int r = aod_validate_guarded_knob(dir, "PPT", ppt_names, 2, AOD_UNIT_WATTS,
                                      1, ppt_w, &pending[pending_count]);
    if (r != ERR_SUCCESS)
      return r;
    pending_count++;
  }

  if (tdc_a > 0) {
    int r = aod_validate_guarded_knob(dir, "TDC", tdc_names, 2, AOD_UNIT_AMPS,
                                      1, tdc_a, &pending[pending_count]);
    if (r != ERR_SUCCESS)
      return r;
    pending_count++;
  }

  if (edc_a > 0) {
    int r = aod_validate_guarded_knob(dir, "EDC", edc_names, 2, AOD_UNIT_AMPS,
                                      1, edc_a, &pending[pending_count]);
    if (r != ERR_SUCCESS)
      return r;
    pending_count++;
  }

  if (mem_mhz > 0) {
    int r = aod_validate_guarded_knob(dir, "MEM_CLOCK", mem_names, 2,
                                      AOD_UNIT_MHZ, 50, mem_mhz,
                                      &pending[pending_count]);
    if (r != ERR_SUCCESS)
      return r;
    pending_count++;
  }

  if (pending_count == 0)
    return ERR_UNSUPPORTED;

  /* Phase 2: Commit all validated writes (O_WRONLY, no read-back). */
  for (size_t i = 0; i < pending_count; i++) {
    int r = aod_write_ull(dir, pending[i].attr_name, pending[i].raw_val);
    if (r != ERR_SUCCESS)
      return r;
  }

  return ERR_SUCCESS;
}

int aod_set_smt(int enable) {
  char dir[512];
  if (aod_resolve_dir(dir, sizeof(dir)) != 0)
    return ERR_UNSUPPORTED;
  static const char *const smt_names[] = {"smt_enable", "smt"};
  for (size_t i = 0; i < 2; i++) {
    char p[560];
    printf_sn(p, sizeof(p), "%s/%s", dir, smt_names[i]);
    if (access(p, F_OK) == 0) {
      if (access(p, W_OK) != 0)
        return ERR_UNSUPPORTED;
      return aod_write_ull(dir, smt_names[i], enable ? 1ULL : 0ULL);
    }
  }
  return ERR_UNSUPPORTED;
}

void aod_read_live_telemetry(AodLiveStats *out, const AodState *cached_state) {
  if (!out)
    return;
  memset(out, 0, sizeof(*out));
  if (cached_state) {
    out->wmi_online = cached_state->supported;
  } else {
    static int cached_wmi = -1;
    if (cached_wmi < 0)
      cached_wmi = aod_is_supported();
    out->wmi_online = cached_wmi;
  }

  /* 1. Primary: AMD SMU Power Metrics Table (/sys/kernel/ryzen_smu_drv) */
  int vfd = open("/sys/kernel/ryzen_smu_drv/pm_table_version", O_RDONLY);
  int tfd = open("/sys/kernel/ryzen_smu_drv/pm_table", O_RDONLY);
  if (vfd >= 0 && tfd >= 0) {
    uint32_t pm_ver = 0;
    float f[96];
    memset(f, 0, sizeof(f));
    ssize_t vn = read(vfd, &pm_ver, sizeof(pm_ver));
    ssize_t tn = read(tfd, f, sizeof(f));
    if (vn == 4 && tn >= (ssize_t)(24 * sizeof(float))) {
      uint32_t fam = (pm_ver >> 16) & 0xFFU;
      if (fam == 0x62U) {
        /* Zen 5 Granite Ridge (9000 / 9000X3D) */
        out->has_ppt = 1;
        out->ppt_lim_w = (double)f[2];
        out->ppt_val_w = (double)f[3];
        out->has_tdc = 1;
        out->tdc_lim_a = (double)f[8];
        out->tdc_val_a = (double)f[9];
        out->has_edc = 1;
        out->edc_val_a = (double)f[20];
        if (tn >= (ssize_t)(64 * sizeof(float)) && f[63] > 0.0f)
          out->edc_lim_a = (double)f[63];
        if (tn >= (ssize_t)(76 * sizeof(float)) && f[75] >= 800.0f &&
            f[75] <= 6000.0f) {
          out->has_mem = 1;
          out->mem_mhz = (int)(f[75] + 0.5f);
        }
      } else if (fam == 0x54U) {
        /* Zen 4 Raphael (7000 / 7000X3D) */
        out->has_ppt = 1;
        out->ppt_lim_w = (double)f[2];
        out->ppt_val_w = (double)f[3];
        out->has_tdc = 1;
        out->tdc_lim_a = (double)f[4];
        out->tdc_val_a = (double)f[5];
        out->has_edc = 1;
        out->edc_lim_a = (double)f[6];
        out->edc_val_a = (double)f[7];
      } else if (fam == 0x38U || fam == 0x24U) {
        /* Zen 3 Vermeer / Zen 2 Matisse (5800X3D / 5700X3D) */
        out->has_ppt = 1;
        out->ppt_lim_w = (double)f[0];
        out->ppt_val_w = (double)f[1];
        out->has_tdc = 1;
        out->tdc_lim_a = (double)f[2];
        out->tdc_val_a = (double)f[3];
        out->has_edc = 1;
        out->edc_lim_a = (double)f[8];
        out->edc_val_a = (double)f[9];
      }
    }
  }
  if (vfd >= 0)
    close(vfd);
  if (tfd >= 0)
    close(tfd);

  /* 2. Fallback: zenpower / zenergy hwmon CPU sensors (cached path scan) */
  if (!out->has_ppt) {
    static int hwmon_state = 0; /* 0 = unscanned, 1 = found, -1 = none */
    static char hwmon_dir[128] = {0};

    if (hwmon_state == 0) {
      hwmon_state = -1;
      for (int i = 0; i < 16; i++) {
        char hdir[128], name[64], npath[160];
        printf_sn(hdir, sizeof(hdir), "/sys/class/hwmon/hwmon%d", i);
        printf_sn(npath, sizeof(npath), "%s/name", hdir);
        if (aod_read_str(npath, name, sizeof(name)) != 0)
          continue;
        if (strcmp(name, "zenpower") != 0 && strcmp(name, "zenpower3") != 0 &&
            strcmp(name, "zenergy") != 0 && strcmp(name, "k10temp") != 0)
          continue;

        unsigned long long probe_v = 0;
        int has_pwr = (aod_read_ull(hdir, "power1_input", &probe_v) == 0 ||
                       aod_read_ull(hdir, "power1_average", &probe_v) == 0);
        int has_cur = (aod_read_ull(hdir, "curr1_input", &probe_v) == 0 ||
                       aod_read_ull(hdir, "curr2_input", &probe_v) == 0);
        if (has_pwr) {
          printf_sn(hwmon_dir, sizeof(hwmon_dir), "%s", hdir);
          hwmon_state = 1;
          break;
        }
        if (has_cur && hwmon_state < 0) {
          printf_sn(hwmon_dir, sizeof(hwmon_dir), "%s", hdir);
          hwmon_state = 1;
        }
      }
    }

    if (hwmon_state == 1) {
      unsigned long long uw = 0;
      if (aod_read_ull(hwmon_dir, "power1_input", &uw) == 0 ||
          aod_read_ull(hwmon_dir, "power1_average", &uw) == 0) {
        out->has_ppt = 1;
        out->ppt_val_w = (double)uw / 1000000.0;
      }
      unsigned long long ma = 0;
      if (!out->has_tdc && aod_read_ull(hwmon_dir, "curr1_input", &ma) == 0) {
        out->has_tdc = 1;
        out->tdc_val_a = (double)ma / 1000.0;
      }
      if (!out->has_edc && aod_read_ull(hwmon_dir, "curr2_input", &ma) == 0) {
        out->has_edc = 1;
        out->edc_val_a = (double)ma / 1000.0;
      }
    }
  }

  /* 3. Fallback: RAPL package energy delta (/sys/class/powercap/intel-rapl:0) */
  if (!out->has_ppt) {
    unsigned long long cur_uj = 0;
    if (aod_read_ull("/sys/class/powercap/intel-rapl:0", "energy_uj",
                     &cur_uj) == 0) {
      static unsigned long long prev_uj = 0;
      static struct timespec prev_ts = {0, 0};
      struct timespec now = {0, 0};
      clock_gettime(CLOCK_MONOTONIC, &now);
      if (prev_uj == 0 || cur_uj < prev_uj) {
        /* Initial seed or counter wrap-around */
        prev_uj = cur_uj;
        prev_ts = now;
      } else {
        static double last_ppt_w = -1.0;
        double dt = (double)(now.tv_sec - prev_ts.tv_sec) +
                    (double)(now.tv_nsec - prev_ts.tv_nsec) / 1e9;
        if (dt >= 0.1) {
          last_ppt_w = ((double)(cur_uj - prev_uj) / 1000000.0) / dt;
          out->has_ppt = 1;
          out->ppt_val_w = last_ppt_w;
          prev_uj = cur_uj;
          prev_ts = now;
        } else if (last_ppt_w >= 0.0) {
          /* Preserve last valid reading without starving prev_uj/prev_ts */
          out->has_ppt = 1;
          out->ppt_val_w = last_ppt_w;
        }
      }
    }
  }
}

/* end of AOD.C */
