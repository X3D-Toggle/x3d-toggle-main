/* Custom standalone C library layer for the X3D Toggle Project
 *
 * `libc.h` - Header only
 */

#ifndef LIBC_H
#define LIBC_H

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

typedef long long ssize_t;
typedef int pid_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef int sig_atomic_t;
typedef unsigned int mode_t;
typedef long time_t;
typedef unsigned long sigset_t;

#ifndef NULL
#define NULL ((void*)0)
#endif

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#define FILENO_STDIN  0
#define FILENO_STDOUT 1
#define FILENO_STDERR 2

#define ENOENT   2
#define EINTR    4
#define EAGAIN  11
#define EACCES  13

#define PART_DUAL  0
#define PART_CACHE 1
#define PART_FREQ  2

#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2
#define O_CREAT     0100
#define O_TRUNC     01000
#define O_APPEND    02000
#define O_NONBLOCK  04000
#define F_GETFL     3
#define F_SETFL     4

#define F_OK        0
#define X_OK        1
#define R_OK        4
#define W_OK        2

#define AF_UNIX      1
#define SOCK_STREAM  1
#define SOCK_DGRAM   2
#define SOCK_CLOEXEC 02000000
#define MSG_NOSIGNAL 0x4000

#define SIGTERM     15
#define SIGINT      2
#define SIGHUP      1
#define SIGKILL     9
#define SIGSEGV    11
#define SIGABRT     6
#define SIGILL      4
#define SIGFPE      8

#define SA_RESTART  0x10000000

#define WIFEXITED(s)    (((s) & 0x7f) == 0)
#define WIFSIGNALED(s)  (((s) & 0x7f) != 0 && ((s) & 0x7f) != 0x7f)
#define WEXITSTATUS(s)  (((s) & 0xff00) >> 8)
#define WTERMSIG(s)     ((s) & 0x7f)

#define LOG_PID     0x01
#define LOG_CONS    0x02
#define LOG_DAEMON  (3<<3)
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_INFO    6
#define LOG_CRIT    2

struct sockaddr {
    unsigned short sa_family;
    char sa_data[14];
};

struct sockaddr_un {
    unsigned short sun_family;
    char sun_path[108];
};

struct timespec {
    time_t tv_sec;
    long   tv_nsec;
};

struct timeval {
    time_t tv_sec;
    long   tv_usec;
};

typedef unsigned long fd_mask;
typedef struct {
    fd_mask fds_bits[1024 / (8 * sizeof(fd_mask))];
} fd_set;

#define FD_SET(n, p)   ((p)->fds_bits[(n) / (8 * sizeof(fd_mask))] |= (1UL << ((n) % (8 * sizeof(fd_mask)))))
#define FD_CLR(n, p)   ((p)->fds_bits[(n) / (8 * sizeof(fd_mask))] &= ~(1UL << ((n) % (8 * sizeof(fd_mask)))))
#define FD_ISSET(n, p) ((p)->fds_bits[(n) / (8 * sizeof(fd_mask))] & (1UL << ((n) % (8 * sizeof(fd_mask)))))
#define FD_ZERO(p)     memset((p), 0, sizeof(*(p)))

struct sigaction {
    void (*sa_handler)(int);
    unsigned long sa_flags;
    void (*sa_restorer)(void);
    unsigned long sa_mask;
};

struct linux_dirent64 {
    uint64_t        d_ino;
    int64_t         d_off;
    unsigned short  d_reclen;
    unsigned char   d_type;
    char            d_name[];
};

int open(const char *path, int flags, ...);
int close(int fd);
int dup2(int oldfd, int newfd);
ssize_t read(int fd, void *buf, size_t n);
ssize_t write(int fd, const void *buf, size_t n);
int socket(int domain, int type, int protocol);
int connect(int fd, const struct sockaddr *addr, unsigned int addrlen);
ssize_t send(int fd, const void *buf, size_t n, int flags);
ssize_t sendto(int fd, const void *buf, size_t n, int flags, const struct sockaddr *addr, unsigned int addrlen);
ssize_t recv(int fd, void *buf, size_t n, int flags);
int bind(int fd, const struct sockaddr *addr, unsigned int addrlen);
int listen(int fd, int backlog);
int accept(int fd, struct sockaddr *addr, unsigned int *addrlen);
int accept4(int fd, struct sockaddr *addr, unsigned int *addrlen, int flags);
int unlink(const char *path);
int chmod(const char *path, mode_t mode);
int mkdir(const char *path, mode_t mode);
int rmdir(const char *path);
int affinity_set(pid_t pid, size_t size, const void *mask);
int affinity_init(void);
int affinity_default(pid_t pid);
int affinity_swap(pid_t pid);
int affinity_auto(pid_t pid);
int affinity_dual(pid_t pid);
int affinity_partition(pid_t pid, int partition);
pid_t fork(void);
int execve(const char *path, char *const argv[], char *const envp[]);
pid_t waitpid(pid_t pid, int *status, int options);
void _exit(int status);
int nanosleep(const struct timespec *req, struct timespec *rem);
int system(const char *command);
int kill(pid_t pid, int sig);
int access(const char *path, int mode);
int fcntl(int fd, int cmd, ...);
int getdents64(unsigned int fd, struct linux_dirent64 *dirp, unsigned int count);
pid_t getpid(void);
uid_t geteuid(void);
#define CLOCK_REALTIME 0
int clock_gettime(int clk_id, struct timespec *tp);
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
int sigemptyset(sigset_t *set);
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);

size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcpy(char *d, const char *s);
char *strncpy(char *d, const char *s, size_t n);
char *strcat(char *d, const char *s);
char *strncat(char *d, const char *s, size_t n);
char *strchr(const char *s, int c);
char *strstr(const char *s1, const char *s2);
size_t strcspn(const char *s, const char *reject);
void *memset(void *s, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void srand(unsigned int seed);
int rand(void);
int atoi(const char *s);
double atof(const char *s);
unsigned long long strtoull(const char *p, char **e, int b);
int toupper(int c);
size_t strlcat_local(char *dst, const char *src, size_t size);
size_t scat(char *dest, const char *src, size_t dest_size);

char *getenv(const char *name);
void openlog(const char *ident, int option, int facility);
void syslog(int priority, const char *format, ...);
void closelog(void);
char *strerror(int err);

extern int errno;

#endif // LIBC_H
