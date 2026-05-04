/* bpf-user file for X3D Toggle Project
 * `bpf-user.c`
 * Logic for tracking active game count based on eBPF ringbuffer events
 * and GameMode D-Bus signals.
 */

#include "bpf-user.h"
#include "../../../include/error.h"
#include "../../../include/games.h"

#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <systemd/sd-bus.h>

extern int printf_sn(char *buf, size_t size, const char *fmt, ...);
#include "../steam.h"

#define _POSIX_C_SOURCE 202405L

struct process_event {
  int pid;
  char comm[16];
  uint8_t is_exit;
};

static struct ring_buffer *rb = NULL;
static struct bpf_object *obj = NULL;
static struct bpf_link *link_exec = NULL;
static struct bpf_link *link_exit = NULL;

static int active_games = 0;
extern gamelist gl;

/* --- eBPF Process Event Handler --- */
static int handle_event(void *ctx, void *data, size_t data_sz) {
  (void)ctx;
  (void)data_sz;
  const struct process_event *e = data;

  if (games_match(&gl, e->comm)) {
    /* Curated game list match */
    if (e->is_exit) {
      if (active_games > 0)
        active_games--;
      journal_log(BPF_HIT, e->comm, e->pid, "exited", active_games);
    } else {
      active_games++;
      journal_log(BPF_HIT, e->comm, e->pid, "launched", active_games);
    }
  } else if (!e->is_exit && (is_game_engine_comm(e->comm) || scan_pid_steam_by_pid(e->pid))) {
    /* Steam game detected via SteamAppId environ or common game engine thread names.
     * Only check on exec events — for exit we rely on the active_games
     * decrement path via the Steam-detected launch counter below. */
    active_games++;
    journal_log(BPF_HIT, e->comm, e->pid, "launched [Steam]", active_games);
  } else if (e->is_exit && active_games > 0) {
    /* Conservative: decrement on any exit if we have active games tracked.
     * This avoids leaking the counter when a Steam game exits but was
     * detected via the environ path (comm may not match anything). */
    active_games--;
    journal_log(BPF_HIT, e->comm, e->pid, "exited [unmatched]", active_games);
  }
  return 0;
}


static int on_gamemode_signal(sd_bus_message *m, void *userdata,
                              sd_bus_error *ret_error) {
  (void)userdata;
  (void)ret_error;

  int32_t pid;
  const char *member = sd_bus_message_get_member(m);

  /* GameMode passes an integer (PID) */
  if (sd_bus_message_read(m, "i", &pid) >= 0) {
    if (strcmp(member, "GameRegistered") == 0) {
      active_games++;
      journal_log(BPF_HIT, "GameMode [D-Bus]", pid, "launched", active_games);
    } else if (strcmp(member, "GameUnregistered") == 0) {
      if (active_games > 0)
        active_games--;
      journal_log(BPF_HIT, "GameMode [D-Bus]", pid, "exited", active_games);
    }
  }
  return 0;
}

static void *gamemode_listener_thread(void *arg) {
  (void)arg;
  sd_bus *bus = NULL;
  char bus_path[128];
  bool connected = false;

  for (int uid = 1000; uid < 1010; uid++) {
    printf_sn(bus_path, sizeof(bus_path), "unix:path=/run/user/%d/bus", uid);
    if (sd_bus_new(&bus) >= 0) {
      sd_bus_set_address(bus, bus_path);
      if (sd_bus_start(bus) >= 0) {
        connected = true;
        break;
      }
      sd_bus_unref(bus);
      bus = NULL;
    }
  }

  if (connected && bus) {
    sd_bus_match_signal(bus, NULL, NULL, /* Match any sender */
                        "/com/feralinteractive/GameMode",
                        "com.feralinteractive.GameMode", "GameRegistered",
                        on_gamemode_signal, NULL);

    sd_bus_match_signal(bus, NULL, NULL, "/com/feralinteractive/GameMode",
                        "com.feralinteractive.GameMode", "GameUnregistered",
                        on_gamemode_signal, NULL);

    while (true) {
      sd_bus_process(bus, NULL);
      sd_bus_wait(bus, (uint64_t)-1);
    }
    sd_bus_unref(bus);
  }
  return NULL;
}

bool bpf_init(void) {
  if (obj)
    return true;

  pthread_t tid;
  if (pthread_create(&tid, NULL, gamemode_listener_thread, NULL) == 0) {
    pthread_detach(tid);
  }

  const char *bpf_path = "/usr/lib/x3d-toggle/bpf.o";
  obj = bpf_object__open_file(bpf_path, NULL);
  if (!obj || (intptr_t)obj < 0) {
    journal_log(ERR_LOAD, bpf_path);
    obj = NULL;
    return false;
  }

  if (bpf_object__load(obj) < 0) {
    journal_log(ERR_LOAD, "kernel verifier rejection or permission denied");
    bpf_object__close(obj);
    obj = NULL;
    return false;
  }

  struct bpf_program *p_exec =
      bpf_object__find_program_by_name(obj, "handle_exec");
  if (p_exec) {
    link_exec = bpf_program__attach(p_exec);
    if (!link_exec || (intptr_t)link_exec < 0) {
      journal_log(ERR_LOAD, "failed to attach handle_exec tracepoint");
      link_exec = NULL;
    }
  }

  struct bpf_program *p_exit =
      bpf_object__find_program_by_name(obj, "handle_exit");
  if (p_exit) {
    link_exit = bpf_program__attach(p_exit);
    if (!link_exit || (intptr_t)link_exit < 0) {
      journal_log(ERR_LOAD, "failed to attach handle_exit tracepoint");
      link_exit = NULL;
    }
  }

  if (link_exec && link_exit) {
    struct bpf_map *map = bpf_object__find_map_by_name(obj, "events");
    int map_fd = map ? bpf_map__fd(map) : -1;
    if (map_fd >= 0) {
      rb = ring_buffer__new(map_fd, handle_event, NULL, NULL);
      if (rb) {
        journal_log(BPF_HIT, "N/A", 0, "initialized [BPF_ACTIVE]", 0);
        return true;
      }
    }
  }

  journal_log(ERR_LOAD, "falling back to polling mode");
  return false;
}

void bpf_poll(int timeout_ms) {
  if (rb)
    ring_buffer__poll(rb, timeout_ms);
}

bool bpf_game(void) { return (active_games > 0); }

void bpf_cleanup(void) {
  if (rb)
    ring_buffer__free(rb);
  if (link_exec)
    bpf_link__destroy(link_exec);
  if (link_exit)
    bpf_link__destroy(link_exit);
  if (obj)
    bpf_object__close(obj);
}

/* end of BPF_USER.C */