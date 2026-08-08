#ifndef REAL_SYSCALLS_H
#define REAL_SYSCALLS_H

/*
 * REAL: Direct Linux syscalls, no libc.
 * Status: REAL — inline asm, no wrapper, no CRT.
 *
 * ABI: x86_64 Linux (System V AMD64).
 *   Syscall number in rax; args in rdi, rsi, rdx, r10, r8, r9;
 *   syscall instruction; return in rax.
 *   Errors returned as negative errno (do NOT compare to -1).
 *
 * Also provides the freestanding types and constants normally
 * pulled from libc headers.
 */

/* Freestanding types (avoid <stdint.h>, <stddef.h>). */
typedef unsigned char       real_u8;
typedef unsigned short      real_u16;
typedef unsigned int        real_u32;
typedef unsigned long       real_u64;
typedef long                real_ssize_t;
typedef unsigned long       real_size_t;

#ifndef NULL
#  define NULL ((void *)0)
#endif

/* Syscall numbers (linux/x86_64) — arch/x86/entry/syscalls/syscall_64.tbl */
#define SYS_read          0
#define SYS_write         1
#define SYS_open          2
#define SYS_close         3
#define SYS_stat          4
#define SYS_fstat         5
#define SYS_lstat         6
#define SYS_getdents      78
#define SYS_getdents64    217
#define SYS_openat        257
#define SYS_newfstatat    262
#define SYS_exit_group    231
#define SYS_exit          60

/* Open flags */
#define O_RDONLY          0
#define O_DIRECTORY       0200000

/* fstatat flags */
#define AT_FDCWD          -100
#define AT_SYMLINK_NOFOLLOW 0x100

/* stat mode bits */
#define S_IFMT            0170000
#define S_IFDIR           0040000
#define S_IFREG           0100000
#define S_ISDIR(m)        (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m)        (((m) & S_IFMT) == S_IFREG)

/* Linux getdents64 dirent layout (kernel) */
struct real_linux_dirent64 {
  real_u64      d_ino;
  real_u64      d_off;
  real_u16      d_reclen;
  real_u8       d_type;
  char          d_name[];
};

#define DT_UNKNOWN 0
#define DT_DIR     4
#define DT_REG     8

/* Kernel stat struct (x86_64 Linux) — see arch/x86/include/uapi/asm/stat.h */
struct real_kstat {
  real_u64 st_dev;
  real_u64 st_ino;
  real_u64 st_nlink;
  real_u32 st_mode;
  real_u32 st_uid;
  real_u32 st_gid;
  real_u32 __pad0;
  real_u64 st_rdev;
  long     st_size;
  long     st_blksize;
  long     st_blocks;
  real_u64 st_atime_sec;
  real_u64 st_atime_nsec;
  real_u64 st_mtime_sec;
  real_u64 st_mtime_nsec;
  real_u64 st_ctime_sec;
  real_u64 st_ctime_nsec;
  long     __unused[3];
};

/* ---- inline asm syscalls (x86_64) ---- */

static inline long real_syscall1(long n, long a) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "0"(n), "D"(a)
                   : "rcx", "r11", "memory");
  return ret;
}

static inline long real_syscall2(long n, long a, long b) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "0"(n), "D"(a), "S"(b)
                   : "rcx", "r11", "memory");
  return ret;
}

static inline long real_syscall3(long n, long a, long b, long c) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "0"(n), "D"(a), "S"(b), "d"(c)
                   : "rcx", "r11", "memory");
  return ret;
}

static inline long real_syscall4(long n, long a, long b, long c, long d) {
  long ret;
  register long r10 __asm__("r10") = d;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "0"(n), "D"(a), "S"(b), "d"(c), "r"(r10)
                   : "rcx", "r11", "memory");
  return ret;
}

/* ---- Thin wrappers ---- */

static inline long real_read(int fd, void *buf, real_size_t n) {
  return real_syscall3(SYS_read, fd, (long)buf, (long)n);
}
static inline long real_write(int fd, const void *buf, real_size_t n) {
  return real_syscall3(SYS_write, fd, (long)buf, (long)n);
}
static inline long real_open(const char *path, int flags) {
  return real_syscall2(SYS_open, (long)path, flags);
}
static inline long real_close(int fd) { return real_syscall1(SYS_close, fd); }
static inline long real_getdents64(int fd, void *buf, real_size_t n) {
  return real_syscall3(SYS_getdents64, fd, (long)buf, (long)n);
}
static inline long real_newfstatat(int dirfd, const char *path,
                                   struct real_kstat *st, int flags) {
  return real_syscall4(SYS_newfstatat, dirfd, (long)path, (long)st, flags);
}

__attribute__((noreturn))
static inline void real_exit(int code) {
  (void)real_syscall1(SYS_exit_group, code);
  __builtin_unreachable();
}

#endif /* REAL_SYSCALLS_H */
