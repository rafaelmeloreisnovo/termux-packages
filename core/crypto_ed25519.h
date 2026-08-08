#ifndef CRYPTO_ED25519_H
#define CRYPTO_ED25519_H

/*
 * Phase 9.21: Ed25519 Signatures
 * Deterministic EDDSA signing for message authentication
 * Key format: 32-byte seed, 64-byte key pair
 */

#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * Ed25519 Key Structures
 * ============================================================================ */

typedef struct {
  uint8_t seed[32];      /* Private seed */
  uint8_t public_key[32]; /* Public key */
  uint8_t private_key[64]; /* Private key (seed + public) */
} ed25519_keypair_t;

typedef struct {
  uint8_t public_key[32]; /* Verification key only */
} ed25519_public_key_t;

typedef struct {
  uint8_t signature[64]; /* Ed25519 signature */
  uint64_t timestamp_ms;
  uint32_t message_len;
} ed25519_sig_t;

/* ============================================================================
 * Ed25519 Operations
 * ============================================================================ */

/* Generate keypair from random seed */
int ed25519_keygen(ed25519_keypair_t *keypair, const uint8_t *seed);

/* Sign message with private key */
int ed25519_sign(const ed25519_keypair_t *keypair, const uint8_t *message,
                 size_t message_len, ed25519_sig_t *signature);

/* Verify signature with public key */
int ed25519_verify(const ed25519_public_key_t *public_key,
                   const uint8_t *message, size_t message_len,
                   const ed25519_sig_t *signature);

/* Extract public key from keypair */
int ed25519_extract_public(const ed25519_keypair_t *keypair,
                           ed25519_public_key_t *public);

/* ============================================================================
 * Batch Verification
 * ============================================================================ */

typedef struct {
  const ed25519_public_key_t *public_keys;
  const uint8_t *messages;
  const size_t *message_lens;
  const ed25519_sig_t *signatures;
  uint32_t count;
} ed25519_batch_verify_t;

/* Verify multiple signatures in batch (constant-time) */
int ed25519_batch_verify(const ed25519_batch_verify_t *batch);

/* ============================================================================
 * Statistics & Monitoring
 * ============================================================================ */

typedef struct {
  uint32_t sign_count;
  uint32_t verify_count;
  uint32_t batch_verify_count;
  uint32_t verify_failures;
  uint64_t total_bytes_signed;
  double mean_signature_time_us;
} ed25519_stats_t;

/* Get Ed25519 statistics */
int ed25519_get_stats(ed25519_stats_t *stats);

#endif /* CRYPTO_ED25519_H */
