/* bpf-user file for X3D Toggle Project
 * `bpf-user.c`
 * Logic for tracking active game count based on eBPF ringbuffer events.
 */

#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <stdint.h>
#include <stdlib.h>
#include <syslog.h>
#include "bpf-user.h"
#include "../../../include/error.h"
#include "../../../include/games.h"

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

static int handle_event(void *ctx, void *data, size_t data_sz) {
  (void)ctx;
  (void)data_sz;
  const struct process_event *e = data;

  if (games_match(&gl, e->comm)) {
    if (e->is_exit) {
      if (active_games > 0)
        active_games--;
      journal_log(BPF_HIT, e->comm, e->pid, "exited", active_games);
    } else {
      active_games++;
      journal_log(BPF_HIT, e->comm, e->pid, "launched", active_games);
    }
  }
  return 0;
}

bool bpf_init(void) {
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
