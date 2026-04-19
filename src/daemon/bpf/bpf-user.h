/* bpf-user header file for X3D Toggle
 *
 * `bpf-user.h`
 */

#ifndef BPF_USER_H
#define BPF_USER_H

#include <stdbool.h>

bool bpf_init(void);
void bpf_poll(int timeout_ms);
bool bpf_game(void);
void bpf_cleanup(void);

#endif // BPF_USER_H
