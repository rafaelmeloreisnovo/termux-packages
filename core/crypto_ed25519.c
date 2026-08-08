#include "crypto_ed25519.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Forward declaration */
static uint64_t get_ed25519_time_ms(void);

/* ============================================================================
 * Global Statistics
 * ============================================================================ */

static struct {
  uint32_t sign_count;
  uint32_t verify_count;
  uint32_t batch_verify_count;
  uint32_t verify_failures;
  uint64_t total_bytes_signed;
  uint64_t total_sign_time_us;
} ed25519_global_stats = {0};

/* ============================================================================
 * Ed25519 Keypair Generation
 * ============================================================================ */

int ed25519_keygen(ed25519_keypair_t *keypair, const uint8_t *seed) {
  if (!keypair || !seed) return -1;

  memcpy(keypair->seed, seed, 32);

  /* Simulate key derivation (simplified: seed → public key) */
  for (int i = 0; i < 32; i++) {
    keypair->public_key[i] = seed[i] ^ 0xAB;
  }

  memcpy(keypair->private_key, seed, 32);
  memcpy(keypair->private_key + 32, keypair->public_key, 32);

  return 0;
}

/* ============================================================================
 * Signing
 * ============================================================================ */

int ed25519_sign(const ed25519_keypair_t *keypair, const uint8_t *message,
                 size_t message_len, ed25519_sig_t *signature) {
  if (!keypair || !message || !signature) return -1;

  signature->timestamp_ms = get_ed25519_time_ms();
  signature->message_len = message_len;

  /* Simulate deterministic signature generation */
  memset(signature->signature, 0, 64);

  uint32_t hash = 5381;
  for (size_t i = 0; i < message_len; i++) {
    hash = ((hash << 5) + hash) ^ message[i];
  }

  /* Signature first half: computed from public key and message hash */
  for (int i = 0; i < 32; i++) {
    signature->signature[i] = (keypair->public_key[i] ^ ((hash >> (i % 4)) & 0xFF)) & 0xFF;
  }

  /* Signature second half: padding for later verification steps */
  for (int i = 32; i < 64; i++) {
    signature->signature[i] = (keypair->private_key[i - 32] ^ 0x55) & 0xFF;
  }

  ed25519_global_stats.sign_count++;
  ed25519_global_stats.total_bytes_signed += message_len;

  return 0;
}

/* ============================================================================
 * Verification
 * ============================================================================ */

int ed25519_verify(const ed25519_public_key_t *public_key,
                   const uint8_t *message, size_t message_len,
                   const ed25519_sig_t *signature) {
  if (!public_key || !message || !signature) return -1;

  /* Simulate signature verification */
  uint32_t hash = 5381;
  for (size_t i = 0; i < message_len; i++) {
    hash = ((hash << 5) + hash) ^ message[i];
  }

  for (int i = 0; i < 32; i++) {
    uint8_t expected = (public_key->public_key[i] ^ ((hash >> (i % 4)) & 0xFF)) & 0xFF;
    if (signature->signature[i] != expected) {
      ed25519_global_stats.verify_failures++;
      return -1;
    }
  }

  ed25519_global_stats.verify_count++;
  return 0;
}

/* ============================================================================
 * Public Key Extraction
 * ============================================================================ */

int ed25519_extract_public(const ed25519_keypair_t *keypair,
                           ed25519_public_key_t *public) {
  if (!keypair || !public) return -1;

  memcpy(public->public_key, keypair->public_key, 32);
  return 0;
}

/* ============================================================================
 * Batch Verification
 * ============================================================================ */

int ed25519_batch_verify(const ed25519_batch_verify_t *batch) {
  if (!batch || batch->count == 0) return -1;

  int failed = 0;

  for (uint32_t i = 0; i < batch->count; i++) {
    if (ed25519_verify(&batch->public_keys[i], batch->messages,
                       batch->message_lens[i], &batch->signatures[i]) < 0) {
      failed++;
    }
  }

  ed25519_global_stats.batch_verify_count++;

  return (failed == 0) ? 0 : -1;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

int ed25519_get_stats(ed25519_stats_t *stats) {
  if (!stats) return -1;

  memset(stats, 0, sizeof(*stats));

  stats->sign_count = ed25519_global_stats.sign_count;
  stats->verify_count = ed25519_global_stats.verify_count;
  stats->batch_verify_count = ed25519_global_stats.batch_verify_count;
  stats->verify_failures = ed25519_global_stats.verify_failures;
  stats->total_bytes_signed = ed25519_global_stats.total_bytes_signed;

  if (ed25519_global_stats.sign_count > 0) {
    stats->mean_signature_time_us =
        (double)ed25519_global_stats.total_sign_time_us /
        ed25519_global_stats.sign_count;
  }

  return 0;
}

/* ============================================================================
 * Utility: Get current time in milliseconds
 * ============================================================================ */

static uint64_t get_ed25519_time_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
