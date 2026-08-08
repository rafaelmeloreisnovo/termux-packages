#ifndef CRYPTO_CHACHA20_H
#define CRYPTO_CHACHA20_H

/*
 * Phase 9.21: ChaCha20-Poly1305 AEAD
 * High-performance authenticated encryption
 * Key: 32 bytes, Nonce: 12 bytes, Tag: 16 bytes
 */

#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * ChaCha20-Poly1305 Structures
 * ============================================================================ */

typedef struct {
  uint8_t key[32];   /* 256-bit key */
  uint8_t nonce[12]; /* 96-bit nonce */
} chacha20_cipher_t;

typedef struct {
  uint8_t ciphertext[4096];  /* Encrypted data (max 4KB) */
  size_t ciphertext_len;
  uint8_t tag[16];           /* Poly1305 authentication tag */
  uint64_t timestamp_ms;
} chacha20_sealed_t;

typedef struct {
  uint8_t plaintext[4096];  /* Decrypted data */
  size_t plaintext_len;
} chacha20_opened_t;

/* ============================================================================
 * Encryption & Decryption
 * ============================================================================ */

/* Initialize cipher with key and nonce */
int chacha20_init(chacha20_cipher_t *cipher, const uint8_t *key,
                  const uint8_t *nonce);

/* Seal (encrypt + authenticate) plaintext */
int chacha20_seal(const chacha20_cipher_t *cipher, const uint8_t *plaintext,
                  size_t plaintext_len, const uint8_t *aad, size_t aad_len,
                  chacha20_sealed_t *sealed);

/* Open (decrypt + verify) ciphertext */
int chacha20_open(const chacha20_cipher_t *cipher,
                  const chacha20_sealed_t *sealed, const uint8_t *aad,
                  size_t aad_len, chacha20_opened_t *opened);

/* ============================================================================
 * Key Management
 * ============================================================================ */

/* Generate random 256-bit key */
int chacha20_keygen(uint8_t *key);

/* Generate random 96-bit nonce */
int chacha20_noncegen(uint8_t *nonce);

/* Derive key from passphrase using HKDF-SHA256 */
int chacha20_derive_key(const char *passphrase, size_t passphrase_len,
                        const uint8_t *salt, size_t salt_len, uint8_t *key);

/* ============================================================================
 * Statistics & Monitoring
 * ============================================================================ */

typedef struct {
  uint32_t seal_count;
  uint32_t open_count;
  uint32_t open_failures;
  uint64_t total_bytes_encrypted;
  uint64_t total_bytes_decrypted;
  double mean_encryption_time_us;
  double mean_decryption_time_us;
} chacha20_stats_t;

/* Get ChaCha20 statistics */
int chacha20_get_stats(chacha20_stats_t *stats);

#endif /* CRYPTO_CHACHA20_H */
