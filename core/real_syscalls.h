#ifndef REAL_SYSCALLS_H
#define REAL_SYSCALLS_H

/*
 * REAL: Direct Linux syscalls, no libc, no wrapper, no CRT.
 *
 * Supported ABIs:
 *   x86_64  : nr=rax, args=rdi/rsi/rdx/r10/r8/r9, instruction=syscall
 *   AArch64 : nr=x8,  args=x0..x5,                  instruction=svc #0
 *   ARM EABI: nr=r7,  args=r0..r6,                  instruction=svc #0
 *
 * Errors are returned as negative errno values.
 */

/* Freestanding types (avoid libc headers). */
typedef unsigned char       real_u8;
typedef unsigned short      real_u16;
typedef unsigned int        real_u32;
typedef unsigned long long  real_u64;
typedef long                real_ssize_t;
typedef unsigned long       real_size_t;

#ifndef NULL
#  define NULL ((void *)0)
#endif

/* Architecture-specific Linux syscall numbers. */
#if defined(__x86_64__)
#  define SYS_read          0
#  define SYS_write         1
#  define SYS_close         3
#  define SYS_getdents64    217
#  define SYS_openat        257
#  define SYS_statx         332
#  define SYS_exit_group    231
#elif defined(__aarch64__)
#  define SYS_read          63
#  define SYS_write         64
#  define SYS_close         57
#  define SYS_getdents64    61
#  define SYS_openat        56
#  define SYS_statx         291
#  define SYS_exit_group    94
#elif defined(__arm__)
#  define SYS_read          3
#  define SYS_write         4
#  define SYS_close         6
#  define SYS_getdents64    217
#  define SYS_openat        322
#  define SYS_statx         397
#  define SYS_exit_group    248
#else
#  error "REAL freestanding syscall ABI unsupported on this architecture"
#endif

/* Open / *at flags. */
#define O_RDONLY             0
#define O_DIRECTORY          0200000
#define AT_FDCWD             -100
#define AT_SYMLINK_NOFOLLOW  0x100

/* statx request bits and file mode bits. */
#define STATX_TYPE           0x00000001U
#define STATX_MODE           0x00000002U
#define S_IFMT               0170000
#define S_IFDIR              0040000
#define S_IFREG              0100000
#define S_ISDIR(m)           (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m)           (((m) & S_IFMT) == S_IFREG)

/* Linux getdents64 dirent layout. */
struct real_linux_dirent64 {
  real_u64 d_ino;
  real_u64 d_off;
  real_u16 d_reclen;
  real_u8  d_type;
  char     d_name[];
};

#define DT_UNKNOWN 0
#define DT_DIR     4
#define DT_REG     8

/*
 * statx is a fixed UAPI structure.  The fields through stx_mode occupy the
 * first 32 bytes; reserve the remainder so the kernel can always write the
 * complete 256-byte ABI object without depending on libc's struct stat.
 */
struct real_statx {
  real_u32 stx_mask;
  real_u32 stx_blksize;
  real_u64 stx_attributes;
  real_u32 stx_nlink;
  real_u32 stx_uid;
  real_u32 stx_gid;
  real_u16 stx_mode;
  real_u16 __spare0;
  real_u8  __rest[224];
};

typedef char real_statx_must_be_256[(sizeof(struct real_statx) == 256) ? 1 : -1];

/* ---- direct syscall shims ---- */

#if defined(__x86_64__)

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

static inline long real_syscall5(long n, long a, long b, long c, long d,
                                 long e) {
  long ret;
  register long r10 __asm__("r10") = d;
  register long r8  __asm__("r8") = e;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "0"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8)
                   : "rcx", "r11", "memory");
  return ret;
}

#elif defined(__aarch64__)

static inline long real_syscall1(long n, long a) {
  register long x0 __asm__("x0") = a;
  register long x8 __asm__("x8") = n;
  __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory", "cc");
  return x0;
}

static inline long real_syscall2(long n, long a, long b) {
  register long x0 __asm__("x0") = a;
  register long x1 __asm__("x1") = b;
  register long x8 __asm__("x8") = n;
  __asm__ volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x8) : "memory", "cc");
  return x0;
}

static inline long real_syscall3(long n, long a, long b, long c) {
  register long x0 __asm__("x0") = a;
  register long x1 __asm__("x1") = b;
  register long x2 __asm__("x2") = c;
  register long x8 __asm__("x8") = n;
  __asm__ volatile("svc #0" : "+r"(x0)
                   : "r"(x1), "r"(x2), "r"(x8) : "memory", "cc");
  return x0;
}

static inline long real_syscall4(long n, long a, long b, long c, long d) {
  register long x0 __asm__("x0") = a;
  register long x1 __asm__("x1") = b;
  register long x2 __asm__("x2") = c;
  register long x3 __asm__("x3") = d;
  register long x8 __asm__("x8") = n;
  __asm__ volatile("svc #0" : "+r"(x0)
                   : "r"(x1), "r"(x2), "r"(x3), "r"(x8) : "memory", "cc");
  return x0;
}

static inline long real_syscall5(long n, long a, long b, long c, long d,
                                 long e) {
  register long x0 __asm__("x0") = a;
  register long x1 __asm__("x1") = b;
  register long x2 __asm__("x2") = c;
  register long x3 __asm__("x3") = d;
  register long x4 __asm__("x4") = e;
  register long x8 __asm__("x8") = n;
  __asm__ volatile("svc #0" : "+r"(x0)
                   : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x8)
                   : "memory", "cc");
  return x0;
}

#elif defined(__arm__)

static inline long real_syscall1(long n, long a) {
  register long r0 __asm__("r0") = a;
  register long r7 __asm__("r7") = n;
  __asm__ volatile("svc #0" : "+r"(r0) : "r"(r7) : "memory", "cc");
  return r0;
}

static inline long real_syscall2(long n, long a, long b) {
  register long r0 __asm__("r0") = a;
  register long r1 __asm__("r1") = b;
  register long r7 __asm__("r7") = n;
  __asm__ volatile("svc #0" : "+r"(r0) : "r"(r1), "r"(r7) : "memory", "cc");
  return r0;
}

static inline long real_syscall3(long n, long a, long b, long c) {
  register long r0 __asm__("r0") = a;
  register long r1 __asm__("r1") = b;
  register long r2 __asm__("r2") = c;
  register long r7 __asm__("r7") = n;
  __asm__ volatile("svc #0" : "+r"(r0)
                   : "r"(r1), "r"(r2), "r"(r7) : "memory", "cc");
  return r0;
}

static inline long real_syscall4(long n, long a, long b, long c, long d) {
  register long r0 __asm__("r0") = a;
  register long r1 __asm__("r1") = b;
  register long r2 __asm__("r2") = c;
  register long r3 __asm__("r3") = d;
  register long r7 __asm__("r7") = n;
  __asm__ volatile("svc #0" : "+r"(r0)
                   : "r"(r1), "r"(r2), "r"(r3), "r"(r7) : "memory", "cc");
  return r0;
}

static inline long real_syscall5(long n, long a, long b, long c, long d,
                                 long e) {
  register long r0 __asm__("r0") = a;
  register long r1 __asm__("r1") = b;
  register long r2 __asm__("r2") = c;
  register long r3 __asm__("r3") = d;
  register long r4 __asm__("r4") = e;
  register long r7 __asm__("r7") = n;
  __asm__ volatile("svc #0" : "+r"(r0)
                   : "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r7)
                   : "memory", "cc");
  return r0;
}

#endif

/* ---- Thin wrappers ---- */

static inline long real_read(int fd, void *buf, real_size_t n) {
  return real_syscall3(SYS_read, fd, (long)buf, (long)n);
}

static inline long real_write(int fd, const void *buf, real_size_t n) {
  return real_syscall3(SYS_write, fd, (long)buf, (long)n);
}

static inline long real_open(const char *path, int flags) {
  return real_syscall4(SYS_openat, AT_FDCWD, (long)path, flags, 0);
}

static inline long real_close(int fd) {
  return real_syscall1(SYS_close, fd);
}

static inline long real_getdents64(int fd, void *buf, real_size_t n) {
  return real_syscall3(SYS_getdents64, fd, (long)buf, (long)n);
}

static inline long real_statx(int dirfd, const char *path, int flags,
                              unsigned int mask, struct real_statx *st) {
  return real_syscall5(SYS_statx, dirfd, (long)path, flags, (long)mask,
                       (long)st);
}

__attribute__((noreturn))
static inline void real_exit(int code) {
  (void)real_syscall1(SYS_exit_group, code);
  __builtin_unreachable();
}

#endif /* REAL_SYSCALLS_H */
