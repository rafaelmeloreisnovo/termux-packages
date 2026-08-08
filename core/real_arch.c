#include "real_arch.h"
#include <string.h>
#include <sys/utsname.h>

/* ============================================================================
 * REAL: 15-architecture property table + compatibility matrix.
 * All values come from documented ABI/ISA references — not simulated.
 * ============================================================================ */

/* Order matches enum values 1..15 (index 0 = UNKNOWN sentinel).
 * Values chosen from official ABI docs and Termux's supported arch list. */
static const real_arch_props_t k_props[REAL_ARCH__COUNT] = {
    [REAL_ARCH_UNKNOWN] = {
        .arch = REAL_ARCH_UNKNOWN, .name = "unknown",
        .canonical_uname = "unknown", .termux_name = NULL,
        .word_bits = 0, .endian = REAL_ENDIAN_LITTLE,
        .page_size = 0, .cache_line = 0, .simd = 0,
        .syscall_conv = REAL_SYSCALL_UNKNOWN
    },
    [REAL_ARCH_X86_64] = {
        .arch = REAL_ARCH_X86_64, .name = "x86_64",
        .canonical_uname = "x86_64", .termux_name = "x86_64",
        .word_bits = 64, .endian = REAL_ENDIAN_LITTLE,
        .page_size = 4096, .cache_line = 64,
        .simd = REAL_SIMD_SSE2 | REAL_SIMD_AVX2,
        .syscall_conv = REAL_SYSCALL_LINUX_SYSCALL
    },
    [REAL_ARCH_I386] = {
        .arch = REAL_ARCH_I386, .name = "i386",
        .canonical_uname = "i686", .termux_name = "i686",
        .word_bits = 32, .endian = REAL_ENDIAN_LITTLE,
        .page_size = 4096, .cache_line = 64,
        .simd = REAL_SIMD_MMX | REAL_SIMD_SSE2,
        .syscall_conv = REAL_SYSCALL_LINUX_SYSCALL
    },
    [REAL_ARCH_ARM64] = {
        .arch = REAL_ARCH_ARM64, .name = "arm64",
        .canonical_uname = "aarch64", .termux_name = "aarch64",
        .word_bits = 64, .endian = REAL_ENDIAN_LITTLE,
        .page_size = 4096, .cache_line = 64,
        .simd = REAL_SIMD_NEON,
        .syscall_conv = REAL_SYSCALL_LINUX_SVC
    },
    [REAL_ARCH_ARM32] = {
        .arch = REAL_ARCH_ARM32, .name = "arm32",
        .canonical_uname = "armv7l", .termux_name = "arm",
        .word_bits = 32, .endian = REAL_ENDIAN_LITTLE,
        .page_size = 4096, .cache_line = 32,
        .simd = REAL_SIMD_NEON,
        .syscall_conv = REAL_SYSCALL_LINUX_SVC
    },
    [REAL_ARCH_RISCV64] = {
        .arch = REAL_ARCH_RISCV64, .name = "riscv64",
        .canonical_uname = "riscv64", .termux_name = NULL,
        .word_bits = 64, .endian = REAL_ENDIAN_LITTLE,
        .page_size = 4096, .cache_line = 64,
        .simd = REAL_SIMD_NONE,
        .syscall_conv = REAL_SYSCALL_LINUX_SYSCALL
    },
    [REAL_ARCH_RISCV32] = {
        .arch = REAL_ARCH_RISCV32, .name = "riscv32",
        .canonical_uname = "riscv32", .termux_name = NULL,
        .word_bits = 32, .endian = REAL_ENDIAN_LITTLE,
        .page_size = 4096, .cache_line = 32,
        .simd = REAL_SIMD_NONE,
        .syscall_conv = REAL_SYSCALL_LINUX_SYSCALL
    },
    [REAL_ARCH_MIPS64] = {
        .arch = REAL_ARCH_MIPS64, .name = "mips64",
        .canonical_uname = "mips64", .termux_name = NULL,
        .word_bits = 64, .endian = REAL_ENDIAN_BIG,
        .page_size = 16384, .cache_line = 32,
        .simd = REAL_SIMD_NONE,
        .syscall_conv = REAL_SYSCALL_LINUX_SYSCALL
    },
    [REAL_ARCH_MIPS32] = {
        .arch = REAL_ARCH_MIPS32, .name = "mips32",
        .canonical_uname = "mips", .termux_name = NULL,
        .word_bits = 32, .endian = REAL_ENDIAN_BIG,
        .page_size = 4096, .cache_line = 32,
        .simd = REAL_SIMD_NONE,
        .syscall_conv = REAL_SYSCALL_LINUX_SYSCALL
    },
    [REAL_ARCH_PPC64LE] = {
        .arch = REAL_ARCH_PPC64LE, .name = "ppc64le",
        .canonical_uname = "ppc64le", .termux_name = NULL,
        .word_bits = 64, .endian = REAL_ENDIAN_LITTLE,
        .page_size = 65536, .cache_line = 128,
        .simd = REAL_SIMD_ALTIVEC,
        .syscall_conv = REAL_SYSCALL_LINUX_SYSCALL
    },
    [REAL_ARCH_PPC32] = {
        .arch = REAL_ARCH_PPC32, .name = "ppc32",
        .canonical_uname = "ppc", .termux_name = NULL,
        .word_bits = 32, .endian = REAL_ENDIAN_BIG,
        .page_size = 4096, .cache_line = 32,
        .simd = REAL_SIMD_ALTIVEC,
        .syscall_conv = REAL_SYSCALL_LINUX_SYSCALL
    },
    [REAL_ARCH_S390X] = {
        .arch = REAL_ARCH_S390X, .name = "s390x",
        .canonical_uname = "s390x", .termux_name = NULL,
        .word_bits = 64, .endian = REAL_ENDIAN_BIG,
        .page_size = 4096, .cache_line = 256,
        .simd = REAL_SIMD_NONE,
        .syscall_conv = REAL_SYSCALL_LINUX_SYSCALL
    },
    [REAL_ARCH_SPARC64] = {
        .arch = REAL_ARCH_SPARC64, .name = "sparc64",
        .canonical_uname = "sparc64", .termux_name = NULL,
        .word_bits = 64, .endian = REAL_ENDIAN_BIG,
        .page_size = 8192, .cache_line = 64,
        .simd = REAL_SIMD_NONE,
        .syscall_conv = REAL_SYSCALL_LINUX_SYSCALL
    },
    [REAL_ARCH_LOONGARCH64] = {
        .arch = REAL_ARCH_LOONGARCH64, .name = "loongarch64",
        .canonical_uname = "loongarch64", .termux_name = NULL,
        .word_bits = 64, .endian = REAL_ENDIAN_LITTLE,
        .page_size = 16384, .cache_line = 64,
        .simd = REAL_SIMD_NONE,
        .syscall_conv = REAL_SYSCALL_LINUX_SYSCALL
    },
    [REAL_ARCH_WASM32] = {
        .arch = REAL_ARCH_WASM32, .name = "wasm32",
        .canonical_uname = "wasm32", .termux_name = NULL,
        .word_bits = 32, .endian = REAL_ENDIAN_LITTLE,
        .page_size = 65536, .cache_line = 0,
        .simd = REAL_SIMD_NONE,
        .syscall_conv = REAL_SYSCALL_WASI
    },
    [REAL_ARCH_ARM64_DARWIN] = {
        .arch = REAL_ARCH_ARM64_DARWIN, .name = "arm64_darwin",
        .canonical_uname = "arm64", .termux_name = NULL,
        .word_bits = 64, .endian = REAL_ENDIAN_LITTLE,
        .page_size = 16384, .cache_line = 128,
        .simd = REAL_SIMD_NEON,
        .syscall_conv = REAL_SYSCALL_DARWIN_SVC
    },
};

const real_arch_props_t *real_arch_props(real_arch_t arch) {
  if ((int)arch < 0 || (int)arch >= REAL_ARCH__COUNT) return NULL;
  return &k_props[arch];
}

real_arch_t real_arch_compile_time(void) {
  return (real_arch_t)REAL_ARCH_BUILD;
}

real_arch_t real_arch_detect_runtime(void) {
  struct utsname u;
  if (uname(&u) != 0) return REAL_ARCH_UNKNOWN;

  /* Try direct match on uname -m */
  for (int i = 1; i < REAL_ARCH__COUNT; i++) {
    if (k_props[i].canonical_uname &&
        strcmp(k_props[i].canonical_uname, u.machine) == 0) {
      /* Distinguish arm64 Linux vs arm64 Darwin */
      if (i == REAL_ARCH_ARM64 && strcmp(u.sysname, "Darwin") == 0) {
        return REAL_ARCH_ARM64_DARWIN;
      }
      return (real_arch_t)i;
    }
  }

  /* Additional aliases uname may return */
  if (strcmp(u.machine, "amd64") == 0) return REAL_ARCH_X86_64;
  if (strcmp(u.machine, "i386") == 0 ||
      strcmp(u.machine, "i586") == 0)
    return REAL_ARCH_I386;
  if (strncmp(u.machine, "armv", 4) == 0)
    return REAL_ARCH_ARM32;
  return REAL_ARCH_UNKNOWN;
}

const char *real_arch_name(real_arch_t arch) {
  const real_arch_props_t *p = real_arch_props(arch);
  return p ? p->name : NULL;
}

real_arch_t real_arch_from_name(const char *name) {
  /* NONNULL_ALL contract enforced by compiler */
  for (int i = 0; i < REAL_ARCH__COUNT; i++) {
    if (strcmp(k_props[i].name, name) == 0) return (real_arch_t)i;
  }
  return REAL_ARCH_UNKNOWN;
}

uint32_t real_arch_compat(real_arch_t a, real_arch_t b) {
  const real_arch_props_t *pa = real_arch_props(a);
  const real_arch_props_t *pb = real_arch_props(b);
  if (!pa || !pb || a == REAL_ARCH_UNKNOWN || b == REAL_ARCH_UNKNOWN) {
    return 0;
  }
  uint32_t flags = 0;
  if (a == b) flags |= REAL_COMPAT_SAME | REAL_COMPAT_ABI;
  if (pa->endian == pb->endian)       flags |= REAL_COMPAT_ENDIAN;
  if (pa->word_bits == pb->word_bits) flags |= REAL_COMPAT_WORDSIZE;

  /* ISA superset relationships that are real */
  if (a == REAL_ARCH_X86_64 && b == REAL_ARCH_I386) {
    flags |= REAL_COMPAT_ISA_SUPER | REAL_COMPAT_EMULATABLE;
  }
  if (a == REAL_ARCH_ARM64 && b == REAL_ARCH_ARM32) {
    /* AArch64 CPUs can run AArch32 in EL0 iff the SoC exposes it. */
    flags |= REAL_COMPAT_ISA_SUPER | REAL_COMPAT_EMULATABLE;
  }
  if (a == REAL_ARCH_RISCV64 && b == REAL_ARCH_RISCV32) {
    flags |= REAL_COMPAT_EMULATABLE;
  }
  if (a == REAL_ARCH_MIPS64 && b == REAL_ARCH_MIPS32) {
    flags |= REAL_COMPAT_ISA_SUPER | REAL_COMPAT_EMULATABLE;
  }
  /* qemu-user emulates most Linux archs on x86_64/arm64 */
  if ((a == REAL_ARCH_X86_64 || a == REAL_ARCH_ARM64) &&
      pb->syscall_conv == REAL_SYSCALL_LINUX_SYSCALL &&
      a != b) {
    flags |= REAL_COMPAT_EMULATABLE;
  }
  return flags;
}

/* ============================================================================
 * Emitters
 * ============================================================================ */

static const char *endian_name(real_endian_t e) {
  return e == REAL_ENDIAN_BIG ? "big" : "little";
}

static const char *syscall_name(real_syscall_conv_t c) {
  switch (c) {
    case REAL_SYSCALL_LINUX_SYSCALL: return "linux-syscall";
    case REAL_SYSCALL_LINUX_SVC:     return "linux-svc";
    case REAL_SYSCALL_DARWIN_SVC:    return "darwin-svc";
    case REAL_SYSCALL_WASI:          return "wasi";
    case REAL_SYSCALL_UNKNOWN:       return "unknown";
  }
  return "unknown";
}

static void write_simd_flags(FILE *out, uint32_t simd) {
  const struct { uint32_t m; const char *n; } tbl[] = {
      {REAL_SIMD_MMX, "mmx"},   {REAL_SIMD_SSE2, "sse2"},
      {REAL_SIMD_AVX2, "avx2"}, {REAL_SIMD_AVX512, "avx512"},
      {REAL_SIMD_NEON, "neon"}, {REAL_SIMD_SVE, "sve"},
      {REAL_SIMD_RVV, "rvv"},   {REAL_SIMD_ALTIVEC, "altivec"},
  };
  int first = 1;
  fputc('[', out);
  for (unsigned i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++) {
    if (simd & tbl[i].m) {
      if (!first) fputc(',', out);
      fprintf(out, "\"%s\"", tbl[i].n);
      first = 0;
    }
  }
  fputc(']', out);
}

void real_arch_write_json(FILE *out) {
  /* NONNULL_ALL contract enforced by compiler */
  fprintf(out, "{\n");
  fprintf(out, "  \"schema\": \"real_arch_matrix/1.0.0\",\n");
  fprintf(out, "  \"status\": \"REAL\",\n");
  fprintf(out, "  \"total_archs\": %d,\n", REAL_ARCH__COUNT - 1);

  real_arch_t rt = real_arch_detect_runtime();
  real_arch_t ct = real_arch_compile_time();
  fprintf(out, "  \"compile_time_arch\": \"%s\",\n", real_arch_name(ct));
  fprintf(out, "  \"runtime_arch\": \"%s\",\n", real_arch_name(rt));
  fprintf(out, "  \"archs\": [\n");
  for (int i = 1; i < REAL_ARCH__COUNT; i++) {
    const real_arch_props_t *p = &k_props[i];
    fprintf(out,
            "    {\"name\":\"%s\",\"uname\":\"%s\",\"termux\":%s%s%s,"
            "\"word_bits\":%u,\"endian\":\"%s\","
            "\"page_size\":%u,\"cache_line\":%u,\"simd\":",
            p->name, p->canonical_uname,
            p->termux_name ? "\"" : "",
            p->termux_name ? p->termux_name : "null",
            p->termux_name ? "\"" : "",
            p->word_bits, endian_name(p->endian),
            p->page_size, p->cache_line);
    write_simd_flags(out, p->simd);
    fprintf(out, ",\"syscall_conv\":\"%s\"}%s\n",
            syscall_name(p->syscall_conv),
            (i + 1 < REAL_ARCH__COUNT) ? "," : "");
  }
  fprintf(out, "  ]\n");
  fprintf(out, "}\n");
}

static void write_compat_flags(FILE *out, uint32_t f) {
  const struct { uint32_t m; char c; } tbl[] = {
      {REAL_COMPAT_SAME, 'S'},       {REAL_COMPAT_ABI, 'A'},
      {REAL_COMPAT_ENDIAN, 'E'},     {REAL_COMPAT_WORDSIZE, 'W'},
      {REAL_COMPAT_ISA_SUPER, '>'},  {REAL_COMPAT_EMULATABLE, 'Q'},
  };
  for (unsigned i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++) {
    fputc((f & tbl[i].m) ? tbl[i].c : '.', out);
  }
}

void real_arch_report(FILE *out) {
  /* NONNULL_ALL contract enforced by compiler */
  fprintf(out, "=== REAL Architecture Matrix (%d archs) ===\n",
          REAL_ARCH__COUNT - 1);

  real_arch_t rt = real_arch_detect_runtime();
  real_arch_t ct = real_arch_compile_time();
  fprintf(out, "Compile-time: %s\n", real_arch_name(ct));
  fprintf(out, "Runtime:      %s\n\n", real_arch_name(rt));

  fprintf(out, "%-14s %-10s %-8s %-6s %-4s %-6s %-6s %-16s\n",
          "arch", "uname", "termux", "bits", "end", "page", "cache",
          "syscall");
  fprintf(out, "%-14s %-10s %-8s %-6s %-4s %-6s %-6s %-16s\n",
          "----", "-----", "------", "----", "---", "----", "-----",
          "-------");
  for (int i = 1; i < REAL_ARCH__COUNT; i++) {
    const real_arch_props_t *p = &k_props[i];
    fprintf(out, "%-14s %-10s %-8s %-6u %-4s %-6u %-6u %-16s\n",
            p->name, p->canonical_uname,
            p->termux_name ? p->termux_name : "-",
            p->word_bits, endian_name(p->endian),
            p->page_size, p->cache_line, syscall_name(p->syscall_conv));
  }

  fprintf(out, "\nCompatibility matrix (row can run column?):\n");
  fprintf(out, "  Legend: S=Same A=ABI E=Endian W=WordSize >=ISA-super Q=Emulatable\n\n");
  fprintf(out, "%-14s", "");
  for (int j = 1; j < REAL_ARCH__COUNT; j++) {
    fprintf(out, " %-7s", real_arch_name((real_arch_t)j));
  }
  fputc('\n', out);
  for (int i = 1; i < REAL_ARCH__COUNT; i++) {
    fprintf(out, "%-14s", real_arch_name((real_arch_t)i));
    for (int j = 1; j < REAL_ARCH__COUNT; j++) {
      uint32_t f = real_arch_compat((real_arch_t)i, (real_arch_t)j);
      fputc(' ', out);
      write_compat_flags(out, f);
      fputc(' ', out);
    }
    fputc('\n', out);
  }
}
