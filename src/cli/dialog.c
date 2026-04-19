/* Master CLI Dialog Controller for the X3D Toggle Project
 *
 * `dialog.c`
 *
 * Implements the MVC-compliant View (Monitor Loop) and Controller (Dispatcher)
 * for the standalone status window.
 */

#include "../../build/xui.h"
#include "../../include/cli.h"
#include "../../build/config.h"
#include "../../include/status.h"

static int dialog_open(void) {
    int fd = open("/proc", O_RDONLY);
    if (fd < 0) return 0;
    
    char buf[4096];
    int nread;
    int found = 0;
    struct linux_dirent64 *d;

    while ((nread = getdents64(fd, (struct linux_dirent64 *)buf, sizeof(buf))) > 0) {
        for (int bpos = 0; bpos < nread;) {
            d = (struct linux_dirent64 *)(buf + bpos);
            if (d->d_name[0] >= '0' && d->d_name[0] <= '9') {
                char path[256], cmdline[256];
                printf_sn(path, sizeof(path), "/proc/%s/cmdline", d->d_name);
                int cfd = open(path, O_RDONLY);
                if (cfd >= 0) {
                    ssize_t n = read(cfd, cmdline, sizeof(cmdline) - 1);
                    if (n > 0) {
                        cmdline[n] = '\0';
                        for (int i = 0; i < n - 1; i++) if (cmdline[i] == '\0') cmdline[i] = ' ';
                        if (strstr(cmdline, "x3d-toggle monitor")) found = 1;
                    }
                    close(cfd);
                }
            }
            if (found) break;
            bpos += d->d_reclen;
        }
        if (found) break;
    }
    close(fd);
    return found;
}

int cli_status_monitor(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    DaemonConfig cfg;
    config_load(&cfg);

    Status st;
    
    write(1, "\x1b[?25l", 6);

    while (1) {
        if (daemon_status(&st) != 0) {
            break;
        }

        write(1, "\x1b[H\x1b[2J", 7);

        printf_br();
        const char *bpf_health = diag_ebpf();

        printf_step("${COLOR_CYAN}%s${COLOR_RESET}  Daemon Status:  ${COLOR_CYAN}%s", st.d_icon, st.d_mode);
        printf_step("${TOOLS}  IPC Status:     ${COLOR_CYAN}%s", st.ipc_status);
        printf_step("${SCHED}  eBPF Health:    ${COLOR_CYAN}%s", bpf_health);
        printf_step("${SCHED}  Scheduler Mode: ${COLOR_CYAN}%s", st.ebpf_status);
        printf_step("===========================================");
        printf_step("${COLOR_CYAN}%s  v-Cache Mode:   ${COLOR_CYAN}%s", st.c_icon, st.c_mode);
        printf_step("${BOOST}  Boost Mode:     ${COLOR_CYAN}%s", st.st_buff);
        printf_step("${COLOR_CYAN}%s  CCD State:      ${COLOR_CYAN}%s", st.ccd_icon, st.ccd_state);
        printf_br();
        printf_step("${CCD0}  CCD0 Status:    ${COLOR_CYAN}%s", st.st_buff_ccd0);
        printf_step("${CCD1}  CCD1 Status:    ${COLOR_CYAN}%s", st.st_buff_ccd1);
        printf_step("===========================================");
        printf_step("${DRIVER}  Driver Mode:    ${COLOR_CYAN}%s", st.d_buff);
        printf_step("${EPP}  EPP Profile:    ${COLOR_CYAN}%s", st.epp);
        printf_step("${GOV}  Governor:       ${COLOR_CYAN}%s", st.gov);
        printf_br();
        printf_step("${SMT}  SMT Status:     ${COLOR_CYAN}%s", st.smt);
        printf_step("${PLAT}  Platform:       ${COLOR_CYAN}%s", st.plat);
        printf_br();
        printf_step("2,${COLOR_DIM}(Heartbeat: %.1fs)", st.refresh_interval);
        
        long ns = (long)(st.refresh_interval * 1000000000.0);
        struct timespec ts;
        ts.tv_sec = (time_t)(st.refresh_interval);
        ts.tv_nsec = ns % 1000000000L;
        nanosleep(&ts, NULL);
    }

    write(1, "\x1b[?25h", 6);
    return 0;
}

int cli_status_dialog(int argc, char *argv[]) {
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--loop") == 0) {
            return cli_status_monitor(argc, argv);
        }
    }

    if (dialog_open()) {
        journal_info(STATE_ACTIVE);
        return ERR_SUCCESS;
    }

    const char *terminals[] = {"konsole", "gnome-terminal", "xfce4-terminal", "alacritty", "kitty", "xterm", NULL};
    const char *term = NULL;

    for (int i = 0; terminals[i] != NULL; i++) {
        char path[128];
        const char *search_paths[] = {"/usr/bin/", "/bin/", "/usr/local/bin/"};
        for (int j = 0; j < 3; j++) {
            printf_sn(path, sizeof(path), "%s%s", search_paths[j], terminals[i]);
            if (open(path, O_RDONLY) >= 0) { // Simple check since we don't have access() in internal libc
                term = terminals[i];
                break;
            }
        }
        if (term) break;
    }

    if (!term) {
        return cli_diag_status(argc, argv);
    }

    pid_t pid = fork();
    if (pid == 0) {
        char *env[] = {NULL};
        char *a[16];
        int idx = 0;
        char full_path[128];
        printf_sn(full_path, 128, "/usr/bin/%s", term);

        a[idx++] = (char *)term;
        if (strcmp(term, "konsole") == 0) {
            a[idx++] = (char *)"--qwindowgeometry"; a[idx++] = (char *)"500x400";
            a[idx++] = (char *)"--title"; a[idx++] = (char *)"X3D Status";
            a[idx++] = (char *)"-e"; a[idx++] = (char *)"x3d-toggle"; a[idx++] = (char *)"monitor"; a[idx++] = (char *)"--loop";
        } else if (strcmp(term, "gnome-terminal") == 0) {
             a[idx++] = (char *)"--title=X3D Status"; a[idx++] = (char *)"--geometry=50x15";
             a[idx++] = (char *)"--"; a[idx++] = (char *)"x3d-toggle"; a[idx++] = (char *)"monitor"; a[idx++] = (char *)"--loop";
        } else {
             a[idx++] = (char *)"-T"; a[idx++] = (char *)"X3D Status"; a[idx++] = (char *)"-e";
             a[idx++] = (char *)"x3d-toggle"; a[idx++] = (char *)"monitor"; a[idx++] = (char *)"--loop";
        }
        a[idx] = NULL;
        execve(full_path, a, env);
        _exit(1);
    }

    printf_step("${ALRIGHT} Launching dynamic status monitor via %s...", term);
    return 0;
}

int cli_diag_status(int argc, char *argv[]) {
  int is_gui = 0;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--gui") == 0)
      is_gui = 1;
  }

  buffer_set();

  Status st;
  if (daemon_status(&st) != ERR_SUCCESS) {
    journal_error(ERR_STATUS);
    return ERR_STATUS;
  }

  char color_cyan[BUFF_COLOR] = "";
  char color_reset[BUFF_RESET] = "";
  char br[BUFF_BR] = "";

  if (is_gui) {
    scat(color_cyan, HTML_CYAN, 32);
    scat(color_reset, HTML_RESET, 32);
    scat(br, "<br>", 16);
  } else {
    scat(color_cyan, COLOR_CYAN, 32);
    scat(color_reset, COLOR_RESET, 32);
  }

  if (is_gui)
    printf_string("<html><pre>");

  const char *bpf_health = diag_ebpf();

  printf_step("%s  Daemon Status:  %s%s%s", st.d_icon, color_cyan, st.d_mode, br);
  printf_step("%s  IPC Status:     %s%s%s", TOOLS, color_cyan, st.ipc_status, br);
  printf_step("%s  eBPF Health:    %s%s%s", SCHED, color_cyan, bpf_health, br);
  printf_step("%s  Scheduler Mode: %s%s%s", SCHED, color_cyan, st.ebpf_status, br);
  printf_step("========================================");
  printf_step("%s  v-Cache Mode:   %s%s%s", st.c_icon, color_cyan, st.c_mode, br);
  printf_step("%s  Boost Mode:     %s%s%s", BOOST, color_cyan, st.st_buff, br);
  printf_step("%s  CCD State:      %s%s%s", st.ccd_icon, color_cyan, st.ccd_state, br);
  printf_step("%s", br);
  printf_step("%s  CCD0 Status:    %s%s%s", CCD0, color_cyan, st.st_buff_ccd0, br);
  printf_step("%s  CCD1 Status:    %s%s%s", CCD1, color_cyan, st.st_buff_ccd1, br);
  printf_step("========================================");
  printf_step("%s  Driver Mode:    %s%s%s", DRIVER, color_cyan, st.d_buff, br);
  printf_step("%s  EPP Profile:    %s%s%s", EPP, color_cyan, st.epp, br);
  printf_step("%s  Governor:       %s%s%s", GOV, color_cyan, st.gov, br);

  printf_step("%s", br);
  printf_step("%s  SMT Status:     %s%s%s", SMT, color_cyan, st.smt, br);
  printf_step("%s  Platform:       %s%s%s", PLAT, color_cyan, st.plat, br);

  buffer_flush();

  if (is_gui)
    printf_string("</pre></html>");

  return ERR_SUCCESS;
}

int cli_status_owl(int argc, char *argv[]) {
    return cli_status_dialog(argc, argv);
}

/* end of DIALOG.C */
