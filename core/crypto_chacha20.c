#include "crypto_chacha20.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

/* Forward declaration */
static uint64_t get_chacha20_time_ms(void);

/* ============================================================================
 * Global Statistics
 * ============================================================================ */

static struct {
  uint32_t seal_count;
  uint32_t open_count;
  uint32_t open_failures;
  uint64_t total_bytes_encrypted;
  uint64_t total_bytes_decrypted;
  uint64_t total_encrypt_time_us;
  uint64_t total_decrypt_time_us;
} chacha20_global_stats = {0};

/* ============================================================================
 * Cipher Initialization
 * ============================================================================ */

int chacha20_init(chacha20_cipher_t *cipher, const uint8_t *key,
                  const uint8_t *nonce) {
  if (!cipher || !key || !nonce) return -1;

  memcpy(cipher->key, key, 32);
  memcpy(cipher->nonce, nonce, 12);

  return 0;
}

/* ============================================================================
 * Encryption & Authentication
 * ============================================================================ */

int chacha20_seal(const chacha20_cipher_t *cipher, const uint8_t *plaintext,
                  size_t plaintext_len, const uint8_t *aad, size_t aad_len,
                  chacha20_sealed_t *sealed) {
  if (!cipher || !plaintext || !sealed) return -1;
  if (plaintext_len > 4096) return -1;

  sealed->timestamp_ms = get_chacha20_time_ms();
  sealed->ciphertext_len = plaintext_len;

  /* Simulate ChaCha20 encryption (XOR with keystream) */
  for (size_t i = 0; i < plaintext_len; i++) {
    uint32_t keystream = (cipher->key[i % 32] ^ cipher->nonce[i % 12]);
    sealed->ciphertext[i] = plaintext[i] ^ (keystream & 0xFF);
  }

  /* Simulate Poly1305 authentication tag */
  memset(sealed->tag, 0, 16);
  uint32_t tag_hash = 0;

  for (size_t i = 0; i < plaintext_len; i++) {
    tag_hash ^= sealed->ciphertext[i];
  }

  if (aad && aad_len > 0) {
    for (size_t i = 0; i < aad_len; i++) {
      tag_hash ^= aad[i];
    }
  }

  for (int i = 0; i < 16; i++) {
    sealed->tag[i] = (tag_hash >> ((i % 4) * 8)) & 0xFF;
  }

  chacha20_global_stats.seal_count++;
  chacha20_global_stats.total_bytes_encrypted += plaintext_len;

  return 0;
}

/* ============================================================================
 * Decryption & Verification
 * ============================================================================ */

int chacha20_open(const chacha20_cipher_t *cipher,
                  const chacha20_sealed_t *sealed, const uint8_t *aad,
                  size_t aad_len, chacha20_opened_t *opened) {
  if (!cipher || !sealed || !opened) return -1;
  if (sealed->ciphertext_len > 4096) return -1;

  /* Verify authentication tag */
  uint32_t tag_hash = 0;

  for (size_t i = 0; i < sealed->ciphertext_len; i++) {
    tag_hash ^= sealed->ciphertext[i];
  }

  if (aad && aad_len > 0) {
    for (size_t i = 0; i < aad_len; i++) {
      tag_hash ^= aad[i];
    }
  }

  for (int i = 0; i < 16; i++) {
    uint8_t expected = (tag_hash >> ((i % 4) * 8)) & 0xFF;
    if (sealed->tag[i] != expected) {
      chacha20_global_stats.open_failures++;
      return -1;
    }
  }

  /* Decrypt (XOR with keystream) */
  opened->plaintext_len = sealed->ciphertext_len;

  for (size_t i = 0; i < sealed->ciphertext_len; i++) {
    uint32_t keystream = (cipher->key[i % 32] ^ cipher->nonce[i % 12]);
    opened->plaintext[i] = sealed->ciphertext[i] ^ (keystream & 0xFF);
  }

  chacha20_global_stats.open_count++;
  chacha20_global_stats.total_bytes_decrypted += sealed->ciphertext_len;

  return 0;
}

/* ============================================================================
 * Key Management
 * ============================================================================ */

int chacha20_keygen(uint8_t *key) {
  if (!key) return -1;

  for (int i = 0; i < 32; i++) {
    key[i] = (rand() >> (i % 3)) & 0xFF;
  }

  return 0;
}

int chacha20_noncegen(uint8_t *nonce) {
  if (!nonce) return -1;

  for (int i = 0; i < 12; i++) {
    nonce[i] = (rand() >> (i % 2)) & 0xFF;
  }

  return 0;
}

int chacha20_derive_key(const char *passphrase, size_t passphrase_len,
                        const uint8_t *salt, size_t salt_len, uint8_t *key) {
  if (!passphrase || !salt || !key) return -1;

  /* Simulate HKDF-SHA256 key derivation */
  memset(key, 0, 32);

  uint32_t hash = 5381;

  for (size_t i = 0; i < passphrase_len; i++) {
    hash = ((hash << 5) + hash) ^ (unsigned char)passphrase[i];
  }

  for (size_t i = 0; i < salt_len; i++) {
    hash ^= salt[i];
  }

  for (int i = 0; i < 32; i++) {
    key[i] = (hash >> ((i % 4) * 8)) & 0xFF;
    hash = ((hash << 7) | (hash >> 25)) ^ key[i];
  }

  return 0;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

int chacha20_get_stats(chacha20_stats_t *stats) {
  if (!stats) return -1;

  memset(stats, 0, sizeof(*stats));

  stats->seal_count = chacha20_global_stats.seal_count;
  stats->open_count = chacha20_global_stats.open_count;
  stats->open_failures = chacha20_global_stats.open_failures;
  stats->total_bytes_encrypted = chacha20_global_stats.total_bytes_encrypted;
  stats->total_bytes_decrypted = chacha20_global_stats.total_bytes_decrypted;

  if (chacha20_global_stats.seal_count > 0) {
    stats->mean_encryption_time_us =
        (double)chacha20_global_stats.total_encrypt_time_us /
        chacha20_global_stats.seal_count;
  }

  if (chacha20_global_stats.open_count > 0) {
    stats->mean_decryption_time_us =
        (double)chacha20_global_stats.total_decrypt_time_us /
        chacha20_global_stats.open_count;
  }

  return 0;
}

/* ============================================================================
 * Utility: Get current time in milliseconds
 * ============================================================================ */

static uint64_t get_chacha20_time_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
