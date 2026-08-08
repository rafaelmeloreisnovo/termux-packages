#ifndef REAL_SHA256_H
#define REAL_SHA256_H

/*
 * REAL: SHA-256 (FIPS 180-4), self-contained.
 * Status: REAL — reference implementation, verified against RFC 6234
 * test vectors ("abc" and "" empty string).
 *
 * No OpenSSL, no libcrypto. Portable across every arch in real_arch.h.
 */

#include "real_attrs.h"
#include <stdint.h>
#include <stddef.h>

#define REAL_SHA256_BLOCK  64
#define REAL_SHA256_DIGEST 32   /* bytes */
#define REAL_SHA256_HEXLEN 65   /* 64 hex chars + NUL */

typedef struct {
  uint32_t state[8];
  uint64_t bitcount;
  uint8_t  buffer[REAL_SHA256_BLOCK];
  uint32_t buflen;
} real_sha256_ctx_t;

REAL_NONNULL_ALL
void real_sha256_init(real_sha256_ctx_t *ctx);

REAL_HOT REAL_NONNULL_ALL
void real_sha256_update(real_sha256_ctx_t *ctx, const uint8_t *data,
                        size_t len);

REAL_NONNULL_ALL
void real_sha256_final(real_sha256_ctx_t *ctx,
                       uint8_t out[REAL_SHA256_DIGEST]);

/* Convenience: hash a full byte buffer in one call. */
REAL_NONNULL(1, 3)
void real_sha256_buf(const uint8_t *data, size_t len,
                     uint8_t out[REAL_SHA256_DIGEST]);

/* Hash a file by path. Returns 0 on success, -1 on I/O error. */
REAL_WARN_UNUSED REAL_NONNULL_ALL
int real_sha256_file(const char *path,
                     uint8_t out[REAL_SHA256_DIGEST],
                     uint64_t *out_size);

/* Convert digest to lowercase hex (65 chars including NUL). */
REAL_NONNULL_ALL
void real_sha256_hex(const uint8_t digest[REAL_SHA256_DIGEST],
                     char out[REAL_SHA256_HEXLEN]);

#endif /* REAL_SHA256_H */
