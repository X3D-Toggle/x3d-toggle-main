/* Internal Standard Library Implementation for the X3D Toggle Project
 *
 * `libc.c`
 *
 * This implementation provides the minimal types, constants, and 
 * low-level bootstrap logic (_start) and 
 * essential syscall wrappers.
 */

#include "libc.h"

int errno = 0;
char **environ;

__asm__(
    ".section .text\n"
    ".global _start\n"
    "_start:\n"
    "   xor %rbp, %rbp\n"            /* Clear frame pointer */
    "   pop %rdi\n"                  /* Get argc */
    "   mov %rsp, %rsi\n"            /* Get argv */
    "   lea 8(%rsi,%rdi,8), %rdx\n"  /* Get envp (8(%rsi + argc*8)) */
    "   mov %rdx, environ(%rip)\n"   /* Initialize environ pointer */
    "   and $-16, %rsp\n"            /* Align stack to 16 bytes */
    "   call daemon\n"               /* Execute program daemon */
    "   mov %rax, %rdi\n"            /* Return code to exit status */
    "   mov $60, %rax\n"             /* SYS_exit */
    "   syscall\n"
);

static unsigned int _next = 1;
void srand(unsigned int seed) { _next = seed; }
int rand(void) {
  _next = _next * 1103515245 + 12345;
  return (unsigned int)(_next / 65536) % 32768;
}

#define SYS_read          0
#define SYS_write         1
#define SYS_open          2
#define SYS_close         3
#define SYS_nanosleep    35
#define SYS_fork         57
#define SYS_execve       59
#define SYS_exit         60
#define SYS_wait4        61
#define SYS_getpid       39
#define SYS_getdents64  217
#define SYS_clock_gettime 228
#define SYS_access       21
#define SYS_geteuid     107
#define SYS_rt_sigaction 13
#define SYS_socket       41
#define SYS_connect      42
#define SYS_accept       43
#define SYS_sendto       44
#define SYS_recvfrom     45
#define SYS_bind         49
#define SYS_listen       50
#define SYS_select       23
#define SYS_fcntl        72
#define SYS_accept4     288
#define SYS_unlink       87
#define SYS_kill         62
#define SYS_dup2         33
#define SYS_chmod        90
#define SYS_mkdir        83
#define SYS_rmdir        84
#define SYS_affinity    203

char *strerror(int err) {
  static const char *msgs[] = {
    "Success", "Operation not permitted", "No such file or directory",
    "No such process", "Interrupted system call", "I/O error",
    "No such device or address", "Argument list too long",
    "Exec format error", "Bad file number", "No child processes",
    "Try again", "Out of memory", "Permission denied", "Bad address"
  };
  if (err < 0) err = -err;
  if (err >= 0 && err < 15) return (char *)msgs[err];
  return (char *)"Unknown error";
}

int open(const char *path, int flags, ...) {
  long ret;
  mode_t mode = 0;
  if (flags & O_CREAT) {
    va_list args;
    va_start(args, flags);
    mode = va_arg(args, mode_t);
    va_end(args);
  }
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_open), "D"(path), "S"(flags), "d"((long)mode) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

ssize_t read(int fd, void *buf, size_t n) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_read), "D"(fd), "S"(buf), "d"(n) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (ssize_t)ret;
}

ssize_t write(int fd, const void *buf, size_t n) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_write), "D"(fd), "S"(buf), "d"(n) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (ssize_t)ret;
}

int close(int fd) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_close), "D"(fd) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

int dup2(int oldfd, int newfd) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_dup2), "D"(oldfd), "S"(newfd) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

pid_t fork(void) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_fork) : "rcx", "r11", "memory");
  return (pid_t)ret;
}

int execve(const char *path, char *const argv[], char *const envp[]) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_execve), "D"(path), "S"(argv), "d"(envp) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

void _exit(int status) {
  __asm__ volatile ("syscall" : : "a"(SYS_exit), "D"(status) : "rcx", "r11", "memory");
  while(1) {}
}

pid_t waitpid(pid_t pid, int *status, int options) {
  long ret;
  register long r10 __asm__("r10") = 0;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_wait4), "D"(pid), "S"(status), "d"(options), "r"(r10) : "rcx", "r11", "memory");
  return (pid_t)ret;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_nanosleep), "D"(req), "S"(rem) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

int system(const char *cmd) {
  if (!cmd) return 1;
  pid_t pid = fork();
  if (pid == 0) {
    char *args[] = {(char*)"/usr/bin/sh", (char*)"-c", (char*)cmd, (char*)0};
    execve(args[0], args, environ);
    _exit(127);
  }
  if (pid < 0) return -1;
  int status;
  waitpid(pid, &status, 0);
  return status;
}

int getdents64(unsigned int fd, struct linux_dirent64 *dirp, unsigned int count) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_getdents64), "D"(fd), "S"(dirp), "d"(count) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

int clock_gettime(int clk_id, struct timespec *tp) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_clock_gettime), "D"(clk_id), "S"(tp) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

int access(const char *path, int mode) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_access), "D"(path), "S"(mode) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

pid_t getpid(void) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_getpid) : "rcx", "r11", "memory");
  return (pid_t)ret;
}

uid_t geteuid(void) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_geteuid) : "rcx", "r11", "memory");
  return (uid_t)ret;
}

int unlink(const char *path) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_unlink), "D"(path) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

int chmod(const char *path, mode_t mode) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_chmod), "D"(path), "S"(mode) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

int mkdir(const char *path, mode_t mode) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_mkdir), "D"(path), "S"(mode) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

int rmdir(const char *path) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_rmdir), "D"(path) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

int affinity_set(pid_t pid, size_t size, const void *mask) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_affinity), "D"(pid), "S"(size), "d"(mask) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

int kill(pid_t pid, int sig) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_kill), "D"(pid), "S"(sig) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
  long ret;
  register long r10 __asm__("r10") = 8;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_rt_sigaction), "D"(signum), "S"(act), "d"(oldact), "r"(r10) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

int sigemptyset(sigset_t *set) { if (set) *set = 0; return 0; }

char *getenv(const char *name) {
  if (!environ) return (void*)0;
  size_t len = strlen(name);
  for (char **e = environ; *e; e++) {
    if (strncmp(*e, name, len) == 0 && (*e)[len] == '=') return *e + len + 1;
  }
  return (void*)0;
}

int socket(int dom, int type, int pro) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_socket), "D"(dom), "S"(type), "d"(pro) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

int connect(int fd, const struct sockaddr *addr, unsigned int len) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_connect), "D"(fd), "S"(addr), "d"(len) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

int bind(int fd, const struct sockaddr *addr, unsigned int len) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_bind), "D"(fd), "S"(addr), "d"(len) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

int listen(int fd, int bl) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_listen), "D"(fd), "S"(bl) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

int accept(int fd, struct sockaddr *addr, unsigned int *len) {
  long ret;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_accept), "D"(fd), "S"(addr), "d"(len) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

int accept4(int fd, struct sockaddr *addr, unsigned int *len, int fl) {
  long ret;
  register long r10 __asm__("r10") = fl;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_accept4), "D"(fd), "S"(addr), "d"(len), "r"(r10) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

ssize_t send(int fd, const void *buf, size_t len, int fl) {
  long ret;
  register long r10 __asm__("r10") = fl;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_sendto), "D"(fd), "S"(buf), "d"(len), "r"(r10), "r"(0), "r"(0) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (ssize_t)ret;
}

ssize_t sendto(int fd, const void *buf, size_t len, int fl, const struct sockaddr *addr, unsigned int addrlen) {
  long ret;
  register long r10 __asm__("r10") = fl;
  register long r8 __asm__("r8") = (long)addr;
  register long r9 __asm__("r9") = (long)addrlen;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_sendto), "D"(fd), "S"(buf), "d"(len), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (ssize_t)ret;
}

ssize_t recv(int fd, void *buf, size_t len, int fl) {
  long ret;
  register long r10 __asm__("r10") = fl;
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_recvfrom), "D"(fd), "S"(buf), "d"(len), "r"(r10), "r"(0), "r"(0) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (ssize_t)ret;
}

int select(int n, fd_set *rf, fd_set *wf, fd_set *ef, struct timeval *tv) {
  long ret;
  register long r10 __asm__("r10") = (long)ef;
  register long r8  __asm__("r8")  = (long)tv;
  __asm__ volatile (
      "syscall"
      : "=a"(ret)
      : "a"(SYS_select), "D"(n), "S"(rf), "d"(wf), "r"(r10), "r"(r8)
      : "rcx", "r11", "memory"
  );
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

int fcntl(int fd, int cmd, ...) {
  long ret, arg = 0;
  if (cmd == F_SETFL || cmd == F_GETFL || cmd == 4 /* F_SETFD */ || cmd == 3 /* F_GETFD */) {
    va_list args;
    va_start(args, cmd);
    arg = va_arg(args, long);
    va_end(args);
  }
  __asm__ volatile ("syscall" : "=a"(ret) : "a"(SYS_fcntl), "D"(fd), "S"(cmd), "d"(arg) : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) { errno = -(int)ret; return -1; }
  return (int)ret;
}

size_t strlen(const char *s) { if (!s) return 0; size_t i = 0; while (s[i]) i++; return i; }
int strcmp(const char *s1, const char *s2) { if (!s1 || !s2) return !s1 && !s2 ? 0 : (s1 ? 1 : -1); while (*s1 && (*s1 == *s2)) { s1++; s2++; } return *(unsigned char *)s1 - *(unsigned char *)s2; }
int strncmp(const char *s1, const char *s2, size_t n) { if (!n) return 0; if (!s1 || !s2) return !s1 && !s2 ? 0 : (s1 ? 1 : -1); while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; } if (n == 0) return 0; return *(unsigned char *)s1 - *(unsigned char *)s2; }
char *strcpy(char *d, const char *s) { if (!d || !s) return d; char *r = d; while ((*d++ = *s++)) {} return r; }
char *strncpy(char *d, const char *s, size_t n) { if (!d || !s || !n) return d; char *r = d; while (n && (*d++ = *s++)) n--; while (n--) *d++ = 0; return r; }
char *strcat(char *d, const char *s) { if (!d || !s) return d; char *r = d; while (*d) d++; while ((*d++ = *s++)) {} return r; }
char *strncat(char *d, const char *s, size_t n) { if (!d || !s) return d; char *r = d; while (*d) d++; while (n-- && (*d++ = *s++)) {} *d = 0; return r; }
char *strchr(const char *s, int c) { if (!s) return (void*)0; while (*s && *s != (char)c) s++; return (*s == (char)c) ? (char *)s : (void*)0; }
char *strstr(const char *h, const char *n) { if (!h || !n) return (void*)0; if (!*n) return (char *)h; for (; *h; h++) { if (*h == *n) { const char *h1 = h, *n1 = n; while (*h1 && *n1 && *h1 == *n1) { h1++; n1++; } if (!*n1) return (char *)h; } } return (void*)0; }
size_t strcspn(const char *s, const char *reject) { if (!s || !reject) return 0; size_t n = 0; while (s[n]) { if (strchr(reject, s[n])) break; n++; } return n; }
void *memcpy(void *d, const void *s, size_t n) { char *d1 = d; const char *s1 = s; while (n--) *d1++ = *s1++; return d; }
void *memset(void *s, int c, size_t n) { char *p = s; while (n--) *p++ = (char)c; return s; }
int memcmp(const void *s1, const void *s2, size_t n) { const unsigned char *a = s1, *b = s2; while (n--) { if (*a != *b) return *a - *b; a++; b++; } return 0; }
void *memmove(void *dest, const void *src, size_t n) { unsigned char *d = dest; const unsigned char *s = src; if (d < s) { while (n--) *d++ = *s++; } else { d += n; s += n; while (n--) *--d = *--s; } return dest; }

int atoi(const char *p) { int n = 0, neg = 0; while (*p == ' ' || *p == '\t') p++; if (*p == '-') { neg = 1; p++; } while (*p >= '0' && *p <= '9') n = n * 10 + (*p++ - '0'); return neg ? -n : n; }
double atof(const char *p) { double res = 0.0, frac = 1.0; int neg = 0, state = 0; while (*p == ' ' || *p == '\t') p++; if (*p == '-') { neg = 1; p++; } while (*p) { if (*p >= '0' && *p <= '9') { if (state == 0) res = res * 10.0 + (*p - '0'); else { frac *= 0.1; res += (*p - '0') * frac; } } else if (*p == '.') state = 1; else break; p++; } return neg ? -res : res; }
int toupper(int c) { if (c >= 'a' && c <= 'z') return c - ('a' - 'A'); return c; }

unsigned long long strtoull(const char *p, char **e, int b) { unsigned long long r = 0; while (*p == ' ' || *p == '\t') p++; while (*p) { int v = 0; if (*p >= '0' && *p <= '9') v = *p - '0'; else if (*p >= 'a' && *p <= 'f') v = *p - 'a' + 10; else if (*p >= 'A' && *p <= 'F') v = *p - 'A' + 10; else break; if (v >= b) break; r = r * b + v; p++; } if (e) *e = (char *)p; return r; }

size_t strlcat_local(char *dst, const char *src, size_t size) {
  size_t dlen = strlen(dst);
  size_t slen = strlen(src);
  if (dlen >= size) return size + slen;
  size_t res = dlen + slen;
  size_t copy = (res >= size) ? (size - dlen - 1) : slen;
  memcpy(dst + dlen, src, copy);
  dst[dlen + copy] = '\0';
  return res;
}

size_t scat(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) return 0;
    size_t i = 0;
    while (src[i] != '\0' && src[i] != '\n' && src[i] != '\r' && i < (dest_size - 1)) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
    while (i > 0 && (dest[i - 1] == ' ' || dest[i - 1] == '\t')) {
        i--;
        dest[i] = '\0';
    }
    return i;
}

extern void journal_syslog(int priority, const char *summary, const char *ctx);
extern int printf_vsn(char *buf, size_t size, const char *fmt, va_list args);

void openlog(const char *id, int o, int f) { (void)id; (void)o; (void)f; }
void closelog(void) {}

void syslog(int priority, const char *fmt, ...) {
  if (!fmt) return;
  char body[1024];
  va_list args;
  va_start(args, fmt);
  printf_vsn(body, sizeof(body), fmt, args);
  va_end(args);

  journal_syslog(priority, "System Event", body);
}
