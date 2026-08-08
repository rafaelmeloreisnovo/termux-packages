#ifndef CRYPTO_ED25519_H
#define CRYPTO_ED25519_H

/*
 * LEGACY TEST API — SIMULATED_TOY, NOT Ed25519/EdDSA.
 *
 * The symbol names are preserved temporarily for source compatibility with
 * historical Phase 9.21 tests. The implementation uses simplified XOR/hash
 * behavior and provides NO cryptographic security guarantee.
 *
 * Production inclusion is fail-closed by crypto_toy_guard.h.
 */

#include "crypto_toy_guard.h"
#include <stddef.h>
#include <stdint.h>

#define RAF_ED25519_API_IS_TOY 1

typedef struct {
  uint8_t seed[32];
  uint8_t public_key[32];
  uint8_t private_key[64];
} ed25519_keypair_t;

typedef struct {
  uint8_t public_key[32];
} ed25519_public_key_t;

typedef struct {
  uint8_t signature[64];
  uint64_t timestamp_ms;
  uint32_t message_len;
} ed25519_sig_t;

/* Toy key derivation from a caller-supplied seed. NOT Ed25519 keygen. */
int ed25519_keygen(ed25519_keypair_t *keypair, const uint8_t *seed);

/* Toy deterministic transform. NOT an Ed25519 signature. */
int ed25519_sign(const ed25519_keypair_t *keypair, const uint8_t *message,
                 size_t message_len, ed25519_sig_t *signature);

/* Toy consistency check. NOT Ed25519 verification. */
int ed25519_verify(const ed25519_public_key_t *public_key,
                   const uint8_t *message, size_t message_len,
                   const ed25519_sig_t *signature);

int ed25519_extract_public(const ed25519_keypair_t *keypair,
                           ed25519_public_key_t *public);

typedef struct {
  const ed25519_public_key_t *public_keys;
  const uint8_t *messages;
  const size_t *message_lens;
  const ed25519_sig_t *signatures;
  uint32_t count;
} ed25519_batch_verify_t;

/* Sequential toy verification; no constant-time claim is made. */
int ed25519_batch_verify(const ed25519_batch_verify_t *batch);

typedef struct {
  uint32_t sign_count;
  uint32_t verify_count;
  uint32_t batch_verify_count;
  uint32_t verify_failures;
  uint64_t total_bytes_signed;
  double mean_signature_time_us;
} ed25519_stats_t;

int ed25519_get_stats(ed25519_stats_t *stats);

#endif /* CRYPTO_ED25519_H */
