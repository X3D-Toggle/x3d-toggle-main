/* IRQ Binding Header for the X3D Toggle Project
 * `irq.h` - Header only
 */

#ifndef IRQ_H
#define IRQ_H

#include "libc.h" // IWYU pragma: keep

#define IRQ_STATE_DIR "/run/x3d-toggle"
#define IRQ_UDEV_RULES "/etc/udev/rules.d/99-x3d-irq.rules"
#define IRQ_BALANCER_STATE IRQ_STATE_DIR "/irqbalance_was_active"

#define IRQ_MAX_VECTORS 256

int irq_init(void);
int irq_bind_gpu(void);
int irq_bind_peripherals(void);
int irq_gen_udev(void);
int irq_teardown(void);
int irq_status(void);
int irq_format_summary(char *buf, size_t buf_sz);
int irq_watch(void);

#endif /* IRQ_H */
