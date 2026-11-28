/* AMD Overclocking (AOD) WMI Interface Module Header
 * `aod.h`
 * Modular support for the upstream Linux `amd-wmi-aod` driver
 * (WMI GUID 95F20B61-3D44-41F8-87E1-03CBF94C4E8F) plus hardware-agnostic
 * PPT / TDC / EDC live telemetry fallback (ryzen_smu / hwmon / RAPL).
 */

#ifndef AOD_H
#define AOD_H

#include "libc.h" // IWYU pragma: keep

#define AOD_WMI_GUID_UPPER "95F20B61-3D44-41F8-87E1-03CBF94C4E8F"
#define AOD_WMI_GUID_LOWER "95f20b61-3d44-41f8-87e1-03cbf94c4e8f"
#ifndef AOD_WMI_BUS_DIR
#define AOD_WMI_BUS_DIR    "/sys/bus/wmi/devices"
#endif

typedef struct {
  int supported;
  int has_ppt;
  int has_tdc;
  int has_edc;
  int has_mem;
  int has_smt;
  int ppt_writable;
  int tdc_writable;
  int edc_writable;
  int mem_writable;

  int ppt_min_w;
  int ppt_max_w;
  int ppt_step_w;
  int ppt_stock_w; /* AMD reference SKU TDP/PPT baseline */
  int ppt_bios_w;  /* Active board/BIOS limit if readable (0 if write-only) */

  int tdc_min_a;
  int tdc_max_a;
  int tdc_step_a;
  int tdc_stock_a; /* AMD reference SKU TDC baseline */
  int tdc_bios_a;  /* Active board/BIOS limit if readable (0 if write-only) */

  int edc_min_a;
  int edc_max_a;
  int edc_step_a;
  int edc_stock_a; /* AMD reference SKU EDC baseline */
  int edc_bios_a;  /* Active board/BIOS limit if readable (0 if write-only) */

  int mem_min_mhz;
  int mem_max_mhz;
  int mem_step_mhz;
  int mem_stock_mhz; /* AMD JEDEC reference baseline */
  int mem_cur_mhz;   /* Active board/BIOS memory clock if readable (0 if N/A) */

  int smt_cur; /* 0, 1, or -1 if unavailable */
} AodState;

typedef struct {
  int has_ppt;
  double ppt_val_w;
  double ppt_lim_w;

  int has_tdc;
  double tdc_val_a;
  double tdc_lim_a;

  int has_edc;
  double edc_val_a;
  double edc_lim_a;

  int has_mem;
  int mem_mhz;

  int wmi_online;
} AodLiveStats;

int aod_resolve_dir(char *out, size_t out_sz);
int aod_is_supported(void);
int aod_has_smt(void);

void aod_get_stock_defaults(int *ppt_w, int *tdc_a, int *edc_a, int *mem_mhz);
void aod_probe_state(AodState *st);
void aod_format_ipc_info(const AodState *st, char *buf, size_t buf_sz);

int aod_apply_limits(int ppt_w, int tdc_a, int edc_a, int mem_mhz);
int aod_apply_ppt_watts(int ppt_w);
int aod_set_smt(int enable);

void aod_read_live_telemetry(AodLiveStats *out, const AodState *cached_state);

#endif /* AOD_H */
