/*
 * REAL: Freestanding package counter — no libc, no CRT.
 * Status: REAL — direct syscalls, hand-rolled runtime.
 *
 * Counts real build.sh files across the termux repo layout using ONLY
 * Linux syscalls. No printf, no malloc, no fopen, no CRT, no dyld.
 *
 * Build:
 *   cc -nostdlib -nostartfiles -ffreestanding -static \
 *      -fno-builtin -fno-stack-protector -no-pie \
 *      -Wall -Wextra -Wshadow -Werror -O2 \
 *      pkg_count_freestanding.c -o pkg-count-freestanding
 *
 * Usage: pkg-count-freestanding <base_dir>
 * Exit code: 0 if any packages found, non-zero on syscall failure.
 * Output format (stdout):
 *   packages=<n>
 *   root-packages=<n>
 *   x11-packages=<n>
 *   disabled-packages=<n>
 *   total_build_sh=<n>
 *   total_subpackage_sh=<n>
 */

#include "real_mem.h"
#include "real_syscalls.h"

#define GETDENTS_BUF 8192
#define PATH_MAX_R  1024
#define STDOUT 1
#define STDERR 2

static real_u32 count_build_sh_in(const char *pkgs_dir,
                                  real_u32 *out_subpackage_count) {
  int fd = (int)real_open(pkgs_dir, O_RDONLY | O_DIRECTORY);
  if (fd < 0) return 0;

  char buf[GETDENTS_BUF] __attribute__((aligned(8)));
  real_u32 build_sh_count = 0;
  real_u32 subpkg_count = 0;

  for (;;) {
    long n = real_getdents64(fd, buf, sizeof(buf));
    if (n <= 0) break;

    long off = 0;
    while (off < n) {
      struct real_linux_dirent64 *ent =
          (struct real_linux_dirent64 *)(buf + off);
      off += ent->d_reclen;

      /* Skip . and .. */
      if (ent->d_name[0] == '.' &&
          (ent->d_name[1] == '\0' ||
           (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) {
        continue;
      }
      /* We only care about directories (packages) */
      if (ent->d_type != DT_DIR && ent->d_type != DT_UNKNOWN) continue;

      /* Check <pkgs_dir>/<pkg>/build.sh exists and is a regular file. */
      char pkg_dir[PATH_MAX_R];
      if (real_join_path(pkg_dir, sizeof(pkg_dir), pkgs_dir, ent->d_name) < 0)
        continue;

      char build_sh[PATH_MAX_R];
      if (real_join_path(build_sh, sizeof(build_sh), pkg_dir, "build.sh") < 0)
        continue;

      struct real_statx st;
      real_memset(&st, 0, sizeof(st));
      if (real_statx(AT_FDCWD, build_sh, 0, STATX_TYPE | STATX_MODE, &st) == 0 &&
          S_ISREG(st.stx_mode)) {
        build_sh_count++;
      }

      /* Count *.subpackage.sh siblings */
      int sub_fd = (int)real_open(pkg_dir, O_RDONLY | O_DIRECTORY);
      if (sub_fd < 0) continue;
      char sbuf[GETDENTS_BUF] __attribute__((aligned(8)));
      for (;;) {
        long m = real_getdents64(sub_fd, sbuf, sizeof(sbuf));
        if (m <= 0) break;
        long soff = 0;
        while (soff < m) {
          struct real_linux_dirent64 *sent =
              (struct real_linux_dirent64 *)(sbuf + soff);
          soff += sent->d_reclen;
          real_size_t nl = real_strlen(sent->d_name);
          const char *suf = ".subpackage.sh";
          real_size_t sl = 14;
          if (nl > sl &&
              real_strncmp(sent->d_name + nl - sl, suf, sl) == 0) {
            subpkg_count++;
          }
        }
      }
      (void)real_close(sub_fd);
    }
  }

  (void)real_close(fd);
  if (out_subpackage_count) *out_subpackage_count += subpkg_count;
  return build_sh_count;
}

static void emit_pair(const char *key, real_u32 value) {
  real_writes(STDOUT, key);
  real_writes(STDOUT, "=");
  char num[24];
  int n = real_u64_to_dec(num, value);
  (void)real_write(STDOUT, num, (real_size_t)n);
  real_writes(STDOUT, "\n");
}

/* Regular C main-like function called from the architecture entrypoint. */
__attribute__((used, noreturn))
static void real_main(long argc, char **argv) {
  const char *base = ".";
  if (argc >= 2) base = argv[1];

  const char *repos[] = {"packages", "root-packages", "x11-packages",
                         "disabled-packages"};

  real_u32 totals = 0;
  real_u32 subpkgs = 0;

  for (int i = 0; i < 4; i++) {
    char path[PATH_MAX_R];
    if (real_join_path(path, sizeof(path), base, repos[i]) < 0) continue;
    real_u32 n = count_build_sh_in(path, &subpkgs);
    emit_pair(repos[i], n);
    totals += n;
  }
  emit_pair("total_build_sh", totals);
  emit_pair("total_subpackage_sh", subpkgs);

  real_exit(totals > 0 ? 0 : 1);
}

/*
 * Kernel process entrypoints.  Capture argc/argv directly from the initial
 * stack before any compiler-generated prologue can move SP, then enter the
 * normal C ABI for real_main().  Linux supplies an ABI-aligned initial stack.
 */
#if defined(__x86_64__)
__asm__(
    ".global _start\n\t"
    ".type _start, @function\n\t"
    "_start:\n\t"
    "  xor  %rbp, %rbp\n\t"
    "  mov  (%rsp), %rdi\n\t"
    "  lea  8(%rsp), %rsi\n\t"
    "  and  $-16, %rsp\n\t"
    "  call real_main\n\t"
    "  ud2\n\t"
);
#elif defined(__aarch64__)
__asm__(
    ".global _start\n\t"
    ".type _start, %function\n\t"
    "_start:\n\t"
    "  mov x29, xzr\n\t"
    "  ldr x0, [sp]\n\t"
    "  add x1, sp, #8\n\t"
    "  bl real_main\n\t"
    "1: b 1b\n\t"
);
#elif defined(__arm__)
__asm__(
    ".global _start\n\t"
    ".type _start, %function\n\t"
    "_start:\n\t"
    "  mov r11, #0\n\t"
    "  ldr r0, [sp]\n\t"
    "  add r1, sp, #4\n\t"
    "  bl real_main\n\t"
    "1: b 1b\n\t"
);
#else
#error "pkg-count-freestanding has no process entrypoint for this architecture"
#endif
