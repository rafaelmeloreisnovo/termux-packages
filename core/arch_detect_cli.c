/*
 * REAL: architecture detection + matrix CLI.
 * Status: REAL — reports compile-time + runtime arch + full matrix.
 *
 * Usage:
 *   arch-detect               — human report
 *   arch-detect --json        — JSON matrix
 *   arch-detect --compat A B  — print compat flags between A and B
 */

#include "real_arch.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
  if (argc >= 2 && strcmp(argv[1], "--json") == 0) {
    real_arch_write_json(stdout);
    return 0;
  }
  if (argc >= 4 && strcmp(argv[1], "--compat") == 0) {
    real_arch_t a = real_arch_from_name(argv[2]);
    real_arch_t b = real_arch_from_name(argv[3]);
    if (a == REAL_ARCH_UNKNOWN || b == REAL_ARCH_UNKNOWN) {
      fprintf(stderr, "unknown arch (use arch-detect to list names)\n");
      return 2;
    }
    uint32_t f = real_arch_compat(a, b);
    printf("compat(%s, %s) = 0x%02x\n", argv[2], argv[3], f);
    printf("  SAME       : %s\n", (f & REAL_COMPAT_SAME) ? "yes" : "no");
    printf("  ABI        : %s\n", (f & REAL_COMPAT_ABI) ? "yes" : "no");
    printf("  ENDIAN     : %s\n", (f & REAL_COMPAT_ENDIAN) ? "yes" : "no");
    printf("  WORDSIZE   : %s\n", (f & REAL_COMPAT_WORDSIZE) ? "yes" : "no");
    printf("  ISA_SUPER  : %s\n", (f & REAL_COMPAT_ISA_SUPER) ? "yes" : "no");
    printf("  EMULATABLE : %s\n", (f & REAL_COMPAT_EMULATABLE) ? "yes" : "no");
    return 0;
  }
  real_arch_report(stdout);
  return 0;
}
