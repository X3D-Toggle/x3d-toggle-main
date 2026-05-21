/* Internal Standard Library Implementation for the X3D Toggle Project
 * `libc.c`
 * This implementation provides the minimal types, constants, and
 * low-level bootstrap logic (_start) and essential syscall wrappers,
 * eliminating the need for external system libraries.
 */

#include "libc.h"
#include "error.h"

#ifndef F_GETFD
#define F_GETFD 3
#endif
#ifndef F_SETFD
#define F_SETFD 4
#endif

#ifndef GUI_BUILD

int errno = 0;
char **environ;

#ifndef LIBC_NO_BOOTSTRAP
__asm__(".section .text\n"
        ".global _start\n"
        "_start:\n"
        "   xor %rbp, %rbp\n"           /* Clear frame pointer */
        "   pop %rdi\n"                 /* Get argc */
        "   mov %rsp, %rsi\n"           /* Get argv */
        "   lea 8(%rsi,%rdi,8), %rdx\n" /* Get envp (8(%rsi + argc*8)) */
        "   mov %rdx, environ(%rip)\n"  /* Initialize environ pointer */
        "   and $-16, %rsp\n"           /* Align stack to 16 bytes */
        "   call daemon\n"              /* Execute program daemon */
        "   mov %rax, %rdi\n"           /* Return code to exit status */
        "   mov $60, %rax\n"            /* SYS_exit */
        "   syscall\n");
#endif

static unsigned int _next = 1;
void srand(unsigned int seed) { _next = seed; }
int rand(void) {
  _next = _next * 1103515245 + 12345;
  return (unsigned int)(_next / 65536) % 32768;
}

#define SYS_read 0
#define SYS_write 1
#define SYS_open 2
#define SYS_close 3
#define SYS_nanosleep 35
#define SYS_fork 57
#define SYS_execve 59
#define SYS_exit 60
#define SYS_wait4 61
#define SYS_getpid 39
#define SYS_getdents64 217
#define SYS_clock_gettime 228
#define SYS_access 21
#define SYS_geteuid 107
#define SYS_rt_sigaction 13
#define SYS_socket 41
#define SYS_connect 42
#define SYS_accept 43
#define SYS_sendto 44
#define SYS_recvfrom 45
#define SYS_bind 49
#define SYS_listen 50
#define SYS_select 23
#define SYS_fcntl 72
#define SYS_accept4 288
#define SYS_unlink 87
#define SYS_kill 62
#define SYS_dup2 33
#define SYS_chmod 90
#define SYS_mkdir 83
#define SYS_rmdir 84
#define SYS_affinity 203

char *strerror(int err) {
  return (char *)journal_strerror(err);
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
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_open), "D"(path), "S"(flags), "d"((long)mode)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

ssize_t read(int fd, void *buf, size_t n) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_read), "D"(fd), "S"(buf), "d"(n)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (ssize_t)ret;
}

ssize_t write(int fd, const void *buf, size_t n) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_write), "D"(fd), "S"(buf), "d"(n)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (ssize_t)ret;
}

int close(int fd) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_close), "D"(fd)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

int dup2(int oldfd, int newfd) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_dup2), "D"(oldfd), "S"(newfd)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

pid_t fork(void) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_fork)
                   : "rcx", "r11", "memory");
  return (pid_t)ret;
}

int execve(const char *path, char *const argv[], char *const envp[]) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_execve), "D"(path), "S"(argv), "d"(envp)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

void _exit(int status) {
  __asm__ volatile("syscall"
                   :
                   : "a"(SYS_exit), "D"(status)
                   : "rcx", "r11", "memory");
  while (1) {
  }
}

pid_t waitpid(pid_t pid, int *status, int options) {
  long ret;
  register long r10 __asm__("r10") = 0;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_wait4), "D"(pid), "S"(status), "d"(options),
                     "r"(r10)
                   : "rcx", "r11", "memory");
  return (pid_t)ret;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_nanosleep), "D"(req), "S"(rem)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

void udelay(unsigned int usec) {
  struct timespec ts;
  ts.tv_sec = usec / 1000000;
  ts.tv_nsec = (usec % 1000000) * 1000;
  nanosleep(&ts, NULL);
}

int system(const char *cmd) {
  if (!cmd)
    return 1;
  pid_t pid = fork();
  if (pid == 0) {
    char *args[] = {(char *)"/bin/sh", (char *)"-c", (char *)cmd,
                    (char *)0};
    execve(args[0], args, environ);
    _exit(127);
  }
  if (pid < 0)
    return -1;
  int status;
  waitpid(pid, &status, 0);
  return status;
}

int getdents64(unsigned int fd, struct linux_dirent64 *dirp,
               unsigned int count) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_getdents64), "D"(fd), "S"(dirp), "d"(count)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

int clock_gettime(int clk_id, struct timespec *tp) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_clock_gettime), "D"(clk_id), "S"(tp)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

int access(const char *path, int mode) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_access), "D"(path), "S"(mode)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

pid_t getpid(void) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_getpid)
                   : "rcx", "r11", "memory");
  return (pid_t)ret;
}

uid_t geteuid(void) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_geteuid)
                   : "rcx", "r11", "memory");
  return (uid_t)ret;
}

int unlink(const char *path) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_unlink), "D"(path)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

int chmod(const char *path, mode_t mode) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_chmod), "D"(path), "S"(mode)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

int mkdir(const char *path, mode_t mode) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_mkdir), "D"(path), "S"(mode)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

int rmdir(const char *path) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_rmdir), "D"(path)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

int affinity_set(pid_t pid, size_t size, const void *mask) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_affinity), "D"(pid), "S"(size), "d"(mask)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

int kill(pid_t pid, int sig) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_kill), "D"(pid), "S"(sig)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

int sigaction(int signum, const struct sigaction *act,
              struct sigaction *oldact) {
  long ret;
  __asm__ volatile("movq %[rt_sig], %%r10\n"
                   "syscall"
                   : "=a"(ret)
                   : "a"(SYS_rt_sigaction), "D"(signum), "S"(act), "d"(oldact),
                     [rt_sig] "g"(8L)
                   : "rcx", "r11", "r10", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

int sigemptyset(sigset_t *set) {
  if (set)
    *set = 0;
  return 0;
}

char *getenv(const char *name) {
  if (!environ)
    return (void *)0;
  size_t len = strlen(name);
  for (char **e = environ; *e; e++) {
    if (strncmp(*e, name, len) == 0 && (*e)[len] == '=')
      return *e + len + 1;
  }
  return (void *)0;
}

int socket(int dom, int type, int pro) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_socket), "D"(dom), "S"(type), "d"(pro)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

int connect(int fd, const struct sockaddr *addr, unsigned int len) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_connect), "D"(fd), "S"(addr), "d"(len)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

int bind(int fd, const struct sockaddr *addr, unsigned int len) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_bind), "D"(fd), "S"(addr), "d"(len)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

int listen(int fd, int bl) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_listen), "D"(fd), "S"(bl)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

int accept(int fd, struct sockaddr *addr, unsigned int *len) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_accept), "D"(fd), "S"(addr), "d"(len)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

int accept4(int fd, struct sockaddr *addr, unsigned int *len, int fl) {
  long ret;
  __asm__ volatile("movq %[fl], %%r10\n"
                   "syscall"
                   : "=a"(ret)
                   : "a"(SYS_accept4), "D"(fd), "S"(addr), "d"(len),
                     [fl] "g"((long)fl)
                   : "rcx", "r11", "r10", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

ssize_t send(int fd, const void *buf, size_t len, int fl) {
  long ret;
  register long r10 __asm__("r10") = fl;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_sendto), "D"(fd), "S"(buf), "d"(len), "r"(r10),
                     "r"(0), "r"(0)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (ssize_t)ret;
}

ssize_t sendto(int fd, const void *buf, size_t len, int fl,
               const struct sockaddr *addr, unsigned int addrlen) {
  long ret;
  __asm__ volatile("movq %[fl], %%r10\n"
                   "movq %[addr], %%r8\n"
                   "movq %[addrlen], %%r9\n"
                   "syscall"
                   : "=a"(ret)
                   : "a"(SYS_sendto), "D"(fd), "S"(buf), "d"(len),
                     [fl] "g"((long)fl), [addr] "g"((long)addr),
                     [addrlen] "g"((long)addrlen)
                   : "rcx", "r11", "r10", "r8", "r9", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (ssize_t)ret;
}

ssize_t recv(int fd, void *buf, size_t len, int fl) {
  long ret;
  register long r10 __asm__("r10") = fl;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_recvfrom), "D"(fd), "S"(buf), "d"(len), "r"(r10),
                     "r"(0), "r"(0)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (ssize_t)ret;
}

int select(int n, fd_set *rf, fd_set *wf, fd_set *ef, struct timeval *tv) {
  long ret;
  __asm__ volatile("movq %[ef], %%r10\n"
                   "movq %[tv], %%r8\n"
                   "syscall"
                   : "=a"(ret)
                   : "a"(SYS_select), "D"(n), "S"(rf), "d"(wf),
                     [ef] "g"((long)ef), [tv] "g"((long)tv)
                   : "rcx", "r11", "r10", "r8", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

int fcntl(int fd, int cmd, ...) {
  long ret, arg = 0;
  if (cmd == F_SETFL || cmd == F_GETFL || cmd == F_SETFD ||
      cmd == F_GETFD) {
    va_list args;
    va_start(args, cmd);
    arg = va_arg(args, long);
    va_end(args);
  }
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(SYS_fcntl), "D"(fd), "S"(cmd), "d"(arg)
                   : "rcx", "r11", "memory");
  if (ret < 0 && ret > -4096) {
    errno = -(int)ret;
    return -1;
  }
  return (int)ret;
}

size_t strlen(const char *s) {
  if (!s)
    return 0;
  size_t i = 0;
  while (s[i])
    i++;
  return i;
}
int strcmp(const char *s1, const char *s2) {
  if (!s1 || !s2)
    return !s1 && !s2 ? 0 : (s1 ? 1 : -1);
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  return *(unsigned char *)s1 - *(unsigned char *)s2;
}
int strncmp(const char *s1, const char *s2, size_t n) {
  if (!n)
    return 0;
  if (!s1 || !s2)
    return !s1 && !s2 ? 0 : (s1 ? 1 : -1);
  while (n && *s1 && (*s1 == *s2)) {
    s1++;
    s2++;
    n--;
  }
  if (n == 0)
    return 0;
  return *(unsigned char *)s1 - *(unsigned char *)s2;
}
char *strcpy(char *d, const char *s) {
  if (!d || !s)
    return d;
  char *r = d;
  while ((*d++ = *s++)) {
  }
  return r;
}
char *strncpy(char *d, const char *s, size_t n) {
  if (!d || !s || !n)
    return d;
  char *r = d;
  while (n && (*d++ = *s++))
    n--;
  while (n--)
    *d++ = 0;
  return r;
}
char *strcat(char *d, const char *s) {
  if (!d || !s)
    return d;
  char *r = d;
  while (*d)
    d++;
  while ((*d++ = *s++)) {
  }
  return r;
}
char *strncat(char *d, const char *s, size_t n) {
  if (!d || !s)
    return d;
  char *r = d;
  while (*d)
    d++;
  while (n-- && (*d++ = *s++)) {
  }
  *d = 0;
  return r;
}
char *strchr(const char *s, int c) {
  if (!s)
    return (void *)0;
  while (*s && *s != (char)c)
    s++;
  return (*s == (char)c) ? (char *)s : (void *)0;
}
char *strstr(const char *h, const char *n) {
  if (!h || !n)
    return (void *)0;
  if (!*n)
    return (char *)h;
  for (; *h; h++) {
    if (*h == *n) {
      const char *h1 = h, *n1 = n;
      while (*h1 && *n1 && *h1 == *n1) {
        h1++;
        n1++;
      }
      if (!*n1)
        return (char *)h;
    }
  }
  return (void *)0;
}
size_t strcspn(const char *s, const char *reject) {
  if (!s || !reject)
    return 0;
  size_t n = 0;
  while (s[n]) {
    if (strchr(reject, s[n]))
      break;
    n++;
  }
  return n;
}
void *memcpy(void *d, const void *s, size_t n) {
  char *d1 = d;
  const char *s1 = s;
  while (n--)
    *d1++ = *s1++;
  return d;
}
void *memset(void *s, int c, size_t n) {
  char *p = s;
  while (n--)
    *p++ = (char)c;
  return s;
}
int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *a = s1, *b = s2;
  while (n--) {
    if (*a != *b)
      return *a - *b;
    a++;
    b++;
  }
  return 0;
}
void *memmove(void *dest, const void *src, size_t n) {
  unsigned char *d = dest;
  const unsigned char *s = src;
  if (d < s) {
    while (n--)
      *d++ = *s++;
  } else {
    d += n;
    s += n;
    while (n--)
      *--d = *--s;
  }
  return dest;
}

int atoi(const char *p) {
  int n = 0, neg = 0;
  while (*p == ' ' || *p == '\t')
    p++;
  if (*p == '-') {
    neg = 1;
    p++;
  }
  while (*p >= '0' && *p <= '9')
    n = n * 10 + (*p++ - '0');
  return neg ? -n : n;
}
double atof(const char *p) {
  double res = 0.0, frac = 1.0;
  int neg = 0, state = 0;
  while (*p == ' ' || *p == '\t')
    p++;
  if (*p == '-') {
    neg = 1;
    p++;
  }
  while (*p) {
    if (*p >= '0' && *p <= '9') {
      if (state == 0)
        res = res * 10.0 + (*p - '0');
      else {
        frac *= 0.1;
        res += (*p - '0') * frac;
      }
    } else if (*p == '.')
      state = 1;
    else
      break;
    p++;
  }
  return neg ? -res : res;
}
int toupper(int c) {
  if (c >= 'a' && c <= 'z')
    return c - ('a' - 'A');
  return c;
}

int isspace(int c) {
  return (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
          c == '\f' || c == '\v');
}

long strtol(const char *p, char **e, int b) {
  unsigned long r = 0;
  int neg = 0;
  while (*p == ' ' || *p == '\t')
    p++;
  if (*p == '-') { neg = 1; p++; }
  else if (*p == '+') { p++; }
  if (b == 0) {
    if (*p == '0' && (*(p+1) == 'x' || *(p+1) == 'X')) { b = 16; p += 2; }
    else if (*p == '0') { b = 8; p++; }
    else b = 10;
  } else if (b == 16 && *p == '0' && (*(p+1) == 'x' || *(p+1) == 'X')) {
    p += 2;
  }
  const char *digits_start = p;
  int overflow = 0;
  unsigned long limit = neg ? ((unsigned long)LONG_MAX + 1UL) : (unsigned long)LONG_MAX;
  while (*p) {
    int v;
    if (*p >= '0' && *p <= '9') v = *p - '0';
    else if (*p >= 'a' && *p <= 'z') v = *p - 'a' + 10;
    else if (*p >= 'A' && *p <= 'Z') v = *p - 'A' + 10;
    else break;
    if (v >= b) break;
    if (!overflow && (r > (limit - (unsigned long)v) / (unsigned long)b)) {
      overflow = 1;
    }
    if (!overflow) r = r * (unsigned long)b + (unsigned long)v;
    p++;
  }
  if (p == digits_start) r = 0;
  if (overflow) {
    errno = ERANGE;
    return neg ? LONG_MIN : LONG_MAX;
  }
  if (e) *e = (char *)p;
  return neg ? -(long)r : (long)r;
}

unsigned long long strtoull(const char *p, char **e, int b) {
  unsigned long long r = 0;
  while (*p == ' ' || *p == '\t')
    p++;
  while (*p) {
    int v = 0;
    if (*p >= '0' && *p <= '9')
      v = *p - '0';
    else if (*p >= 'a' && *p <= 'f')
      v = *p - 'a' + 10;
    else if (*p >= 'A' && *p <= 'F')
      v = *p - 'A' + 10;
    else
      break;
    if (v >= b)
      break;
    r = r * b + v;
    p++;
  }
  if (e)
    *e = (char *)p;
  return r;
}

int sysctl_read(const char *path, char *buf, size_t size) {
  int fd = open(path, 0); /* O_RDONLY = 0 */
  if (fd < 0) return -1;
  ssize_t n = read(fd, buf, size - 1);
  close(fd);
  if (n < 0) return -1;
  buf[n] = '\0';
  /* Trim trailing newline */
  while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == ' ')) {
    buf[--n] = '\0';
  }
  return 0;
}

int sysctl_write(const char *path, const char *val) {
  int fd = open(path, 1); /* O_WRONLY = 1 */
  if (fd < 0) return -1;
  size_t len = strlen(val);
  ssize_t n = write(fd, val, len);
  close(fd);
  return (n == (ssize_t)len) ? 0 : -1;
}

void priority(int enable) {
  const char *targets[] = {
      "/sys/class/amd_x3d/vcache_mode",
      "/sys/bus/platform/devices/AMDI0101:00/amd_x3d_mode",
      "/sys/devices/platform/AMDI0101:00/amd_x3d_mode",
      "/sys/bus/pci/drivers/amd_x3d_vcache/vcache_mode",
      "/sys/bus/platform/drivers/amd_x3d_vcache/AMDI0101:00/amd_x3d_mode"};

  const char *mode = enable ? "cache" : "frequency";
  size_t len = strlen(mode);

  for (size_t i = 0; i < sizeof(targets) / sizeof(targets[0]); i++) {
    int fd = open(targets[i], 1); /* O_WRONLY = 1 */
    if (fd >= 0) {
      write(fd, mode, len);
      close(fd);
      return;
    }
  }
}

int execvp(const char *file, char *const argv[]) {
  if (strchr(file, '/')) {
    return execve(file, argv, environ);
  }

  char *path = getenv("PATH");
  if (!path)
    path = (char *)"/bin:/usr/bin";

  static char buf[4096];
  const char *p = path;
  while (*p) {
    const char *start = p;
    while (*p && *p != ':')
      p++;

    size_t dlen = p - start;
    if (dlen + strlen(file) + 2 > sizeof(buf)) {
      journal_error(ERR_LONG, file);
      if (*p == ':')
        p++;
      continue;
    }

    memcpy(buf, start, dlen);
    buf[dlen] = '/';
    strcpy(buf + dlen + 1, file);

    execve(buf, argv, environ);

    if (*p == ':')
      p++;
  }
  return -1;
}

#endif

size_t strlcat_local(char *dst, const char *src, size_t size) {
  size_t dlen = strlen(dst);
  size_t slen = strlen(src);
  if (dlen >= size)
    return size + slen;
  size_t res = dlen + slen;
  size_t copy = (res >= size) ? (size - dlen - 1) : slen;
  memcpy(dst + dlen, src, copy);
  dst[dlen + copy] = '\0';
  return res;
}


extern void journal_syslog(int priority, const char *summary, const char *ctx);
extern int printf_vsn(char *buf, size_t size, const char *fmt, va_list args);

void openlog(const char *id, int o, int f) {
  (void)id;
  (void)o;
  (void)f;
}
void closelog(void) {}

void syslog(int priority, const char *fmt, ...) {
  if (!fmt)
    return;
  char body[1024];
  va_list args;
  va_start(args, fmt);
  printf_vsn(body, sizeof(body), fmt, args);
  va_end(args);

  journal_syslog(priority, "System Event", body);
}
