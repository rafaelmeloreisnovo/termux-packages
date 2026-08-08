#ifndef REAL_MEM_H
#define REAL_MEM_H

/*
 * REAL: freestanding mem/str primitives, no libc.
 * Status: REAL — hand-rolled implementations.
 *
 * The compiler may still LOWER our loops to memcpy/memset calls
 * (a known -O2 idiom). We name our functions with `real_` prefix so
 * they never collide with the intrinsic ones. Where the compiler would
 * insert a memcpy call, we compile with -fno-builtin-memcpy etc.
 *
 * Only what's actually used by the freestanding pipeline.
 */

#include "real_syscalls.h"

static inline void real_memset(void *dst, int c, real_size_t n) {
  unsigned char *d = (unsigned char *)dst;
  for (real_size_t i = 0; i < n; i++) d[i] = (unsigned char)c;
}

static inline void real_memcpy(void *dst, const void *src, real_size_t n) {
  unsigned char *d = (unsigned char *)dst;
  const unsigned char *s = (const unsigned char *)src;
  for (real_size_t i = 0; i < n; i++) d[i] = s[i];
}

static inline real_size_t real_strlen(const char *s) {
  real_size_t n = 0;
  while (s[n]) n++;
  return n;
}

static inline int real_strcmp(const char *a, const char *b) {
  while (*a && *a == *b) { a++; b++; }
  return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static inline int real_strncmp(const char *a, const char *b, real_size_t n) {
  for (real_size_t i = 0; i < n; i++) {
    unsigned char ca = (unsigned char)a[i];
    unsigned char cb = (unsigned char)b[i];
    if (ca != cb) return (int)ca - (int)cb;
    if (ca == 0) return 0;
  }
  return 0;
}

/* Copy at most cap-1 chars from src to dst; NUL-terminate. */
static inline void real_strncpy_z(char *dst, real_size_t cap, const char *src) {
  if (cap == 0) return;
  real_size_t i;
  for (i = 0; i + 1 < cap && src[i]; i++) dst[i] = src[i];
  dst[i] = '\0';
}

/* Append `path/name` into `out[cap]`; returns length or -1 if overflow. */
static inline int real_join_path(char *out, real_size_t cap, const char *dir,
                                 const char *name) {
  real_size_t dl = real_strlen(dir);
  real_size_t nl = real_strlen(name);
  if (dl + 1 + nl + 1 > cap) return -1;
  real_memcpy(out, dir, dl);
  out[dl] = '/';
  real_memcpy(out + dl + 1, name, nl);
  out[dl + 1 + nl] = '\0';
  return (int)(dl + 1 + nl);
}

/* Write an unsigned integer as decimal to out; returns bytes written.
 * out must be at least 21 bytes. Not NUL-terminated. */
static inline int real_u64_to_dec(char *out, real_u64 v) {
  char tmp[21];
  int n = 0;
  if (v == 0) { out[0] = '0'; return 1; }
  while (v) { tmp[n++] = '0' + (int)(v % 10); v /= 10; }
  for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
  return n;
}

/* Write NUL-terminated string via write(2). Best-effort, single call. */
static inline void real_writes(int fd, const char *s) {
  (void)real_write(fd, s, real_strlen(s));
}

#endif /* REAL_MEM_H */
