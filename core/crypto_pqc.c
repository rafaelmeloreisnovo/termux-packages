#include "crypto_pqc.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ============================================================================
 * Global Statistics
 * ============================================================================ */

static struct {
  uint32_t kem_encap_count;
  uint32_t kem_decap_count;
  uint32_t sig_sign_count;
  uint32_t sig_verify_count;
  uint32_t hybrid_sign_count;
  uint32_t hybrid_verify_count;
  uint32_t verify_failures;
  pqc_scheme_t current_scheme;
} pqc_global_stats = {.current_scheme = PQC_SCHEME_HYBRID};

/* ============================================================================
 * PQC KEM (Key Encapsulation Mechanism)
 * ============================================================================ */

int pqc_kem_keygen(pqc_kem_keypair_t *keypair) {
  if (!keypair) return -1;

  /* Simulate KYBER keypair generation - derive public from secret */
  for (int i = 0; i < 3168; i++) {
    keypair->secret_key[i] = (rand() >> (i % 5)) & 0xFF;
  }

  for (int i = 0; i < KYBER_PUBLICKEY_BYTES; i++) {
    keypair->public_key[i] = (keypair->secret_key[i % 3168] ^ 0xAA) & 0xFF;
  }

  return 0;
}

int pqc_kem_encapsulate(const uint8_t *public_key,
                        pqc_kem_encapsulation_t *encap) {
  if (!public_key || !encap) return -1;

  /* Simulate KYBER encapsulation */
  for (int i = 0; i < KYBER_CIPHERTEXT_BYTES; i++) {
    encap->ciphertext[i] = (public_key[i % KYBER_PUBLICKEY_BYTES] ^ ((i * 7) & 0xFF)) & 0xFF;
  }

  for (int i = 0; i < KYBER_SHAREDSECRET_BYTES; i++) {
    encap->shared_secret[i] = (public_key[i % KYBER_PUBLICKEY_BYTES]) ^ 0x55;
  }

  pqc_global_stats.kem_encap_count++;
  return 0;
}

int pqc_kem_decapsulate(const uint8_t *secret_key, const uint8_t *ciphertext,
                        uint8_t *shared_secret) {
  if (!secret_key || !ciphertext || !shared_secret) return -1;

  /* Simulate KYBER decapsulation - extract from secret key */
  /* In real KYBER, we'd decrypt the ciphertext with the secret key */
  /* For this simulation, the public key was derived from secret_key */
  for (int i = 0; i < KYBER_SHAREDSECRET_BYTES; i++) {
    uint8_t public_byte = (secret_key[i % 3168] ^ 0xAA) & 0xFF;
    shared_secret[i] = public_byte ^ 0x55;
  }

  pqc_global_stats.kem_decap_count++;
  return 0;
}

/* ============================================================================
 * PQC Signatures (DILITHIUM-like)
 * ============================================================================ */

int pqc_sig_keygen(pqc_sig_keypair_t *keypair) {
  if (!keypair) return -1;

  /* Simulate DILITHIUM keypair generation - derive public from secret */
  for (int i = 0; i < 2544; i++) {
    keypair->secret_key[i] = (rand() >> (i % 4)) & 0xFF;
  }

  for (int i = 0; i < DILITHIUM_PUBLICKEY_BYTES; i++) {
    keypair->public_key[i] = (keypair->secret_key[i % 2544] ^ 0xAA) & 0xFF;
  }

  return 0;
}

int pqc_sig_sign(const uint8_t *secret_key, const uint8_t *message,
                 size_t message_len, pqc_sig_t *signature) {
  if (!secret_key || !message || !signature) return -1;

  /* Simulate DILITHIUM signing */
  memset(signature->signature, 0, DILITHIUM_SIGNATURE_BYTES);

  uint32_t hash = 5381;
  for (size_t i = 0; i < message_len; i++) {
    hash = ((hash << 5) + hash) ^ message[i];
  }

  /* Derive public key from secret key */
  uint8_t public_key[DILITHIUM_PUBLICKEY_BYTES];
  for (int i = 0; i < DILITHIUM_PUBLICKEY_BYTES; i++) {
    public_key[i] = (secret_key[i % 2544] ^ 0xAA) & 0xFF;
  }

  /* Sign: encode public key + message hash */
  for (int i = 0; i < DILITHIUM_SIGNATURE_BYTES && i < DILITHIUM_PUBLICKEY_BYTES; i++) {
    signature->signature[i] = ((public_key[i] ^ ((hash >> (i % 4)) & 0xFF)) & 0xFF);
  }

  pqc_global_stats.sig_sign_count++;
  return 0;
}

int pqc_sig_verify(const uint8_t *public_key, const uint8_t *message,
                   size_t message_len, const pqc_sig_t *signature) {
  if (!public_key || !message || !signature) return -1;

  /* Simulate DILITHIUM verification */
  uint32_t hash = 5381;
  for (size_t i = 0; i < message_len; i++) {
    hash = ((hash << 5) + hash) ^ message[i];
  }

  /* Verify signature matches public key and message */
  for (int i = 0; i < DILITHIUM_SIGNATURE_BYTES && i < (DILITHIUM_PUBLICKEY_BYTES / 2); i++) {
    uint8_t expected = ((public_key[i] ^ ((hash >> (i % 4)) & 0xFF)) & 0xFF);
    if (signature->signature[i] != expected) {
      pqc_global_stats.verify_failures++;
      return -1;
    }
  }

  pqc_global_stats.sig_verify_count++;
  return 0;
}

/* ============================================================================
 * Hybrid Scheme (Classical + PQC)
 * ============================================================================ */

int pqc_hybrid_keygen(pqc_hybrid_private_key_t *private_key,
                      pqc_hybrid_public_key_t *public_key) {
  if (!private_key || !public_key) return -1;

  /* Generate Ed25519 keys */
  for (int i = 0; i < 32; i++) {
    private_key->ed25519_private[i] = (rand() >> (i % 3)) & 0xFF;
    public_key->ed25519_public[i] =
        (private_key->ed25519_private[i] ^ 0x7F) & 0xFF;
  }

  /* Generate DILITHIUM keys */
  for (int i = 0; i < 2544; i++) {
    private_key->pqc_sig_private[i] = (rand() >> (i % 4)) & 0xFF;
  }

  for (int i = 0; i < DILITHIUM_PUBLICKEY_BYTES; i++) {
    public_key->pqc_sig_public[i] =
        (private_key->pqc_sig_private[i % 2544] ^ 0xAA) & 0xFF;
  }

  /* Generate KYBER keys */
  for (int i = 0; i < 3168; i++) {
    private_key->kyber_private[i] = (rand() >> (i % 5)) & 0xFF;
  }

  for (int i = 0; i < KYBER_PUBLICKEY_BYTES; i++) {
    public_key->kyber_public[i] =
        (private_key->kyber_private[i % 3168] ^ 0x55) & 0xFF;
  }

  return 0;
}

int pqc_hybrid_sign(const pqc_hybrid_private_key_t *private_key,
                    const uint8_t *message, size_t message_len,
                    uint8_t *hybrid_signature) {
  if (!private_key || !message || !hybrid_signature) return -1;

  /* Sign with both Ed25519 and DILITHIUM */
  uint32_t hash = 5381;
  for (size_t i = 0; i < message_len; i++) {
    hash = ((hash << 5) + hash) ^ message[i];
  }

  /* Derive public keys from private keys */
  uint8_t ed_public[32];
  uint8_t pqc_public[DILITHIUM_PUBLICKEY_BYTES];

  for (int i = 0; i < 32; i++) {
    ed_public[i] = (private_key->ed25519_private[i] ^ 0x7F) & 0xFF;
  }

  for (int i = 0; i < DILITHIUM_PUBLICKEY_BYTES; i++) {
    pqc_public[i] = (private_key->pqc_sig_private[i % 2544] ^ 0xAA) & 0xFF;
  }

  /* Ed25519 signature (first 32 bytes) */
  for (int i = 0; i < 32; i++) {
    hybrid_signature[i] = (ed_public[i] ^ ((hash >> (i % 4)) & 0xFF)) & 0xFF;
  }

  /* DILITHIUM signature (next 2420 bytes) */
  for (int i = 0; i < DILITHIUM_SIGNATURE_BYTES && i < DILITHIUM_PUBLICKEY_BYTES; i++) {
    hybrid_signature[32 + i] =
        ((pqc_public[i] ^ ((hash >> (i % 4)) & 0xFF)) & 0xFF);
  }

  pqc_global_stats.hybrid_sign_count++;
  return 0;
}

int pqc_hybrid_verify(const pqc_hybrid_public_key_t *public_key,
                      const uint8_t *message, size_t message_len,
                      const uint8_t *hybrid_signature) {
  if (!public_key || !message || !hybrid_signature) return -1;

  uint32_t hash = 5381;
  for (size_t i = 0; i < message_len; i++) {
    hash = ((hash << 5) + hash) ^ message[i];
  }

  /* Verify Ed25519 part (first 32 bytes) */
  for (int i = 0; i < 32; i++) {
    uint8_t expected = (public_key->ed25519_public[i] ^ ((hash >> (i % 4)) & 0xFF)) & 0xFF;
    if (hybrid_signature[i] != expected) {
      pqc_global_stats.verify_failures++;
      return -1;
    }
  }

  /* Verify DILITHIUM part (next portion) */
  for (int i = 0; i < DILITHIUM_PUBLICKEY_BYTES && i < DILITHIUM_SIGNATURE_BYTES;
       i++) {
    uint8_t expected =
        ((public_key->pqc_sig_public[i] ^ ((hash >> (i % 4)) & 0xFF)) & 0xFF);
    if (hybrid_signature[32 + i] != expected) {
      pqc_global_stats.verify_failures++;
      return -1;
    }
  }

  pqc_global_stats.hybrid_verify_count++;
  return 0;
}

/* ============================================================================
 * Scheme Management
 * ============================================================================ */

pqc_scheme_t pqc_get_scheme(void) {
  return pqc_global_stats.current_scheme;
}

int pqc_set_scheme(pqc_scheme_t scheme) {
  if (scheme < PQC_SCHEME_CLASSICAL || scheme > PQC_SCHEME_PQC_ONLY)
    return -1;

  pqc_global_stats.current_scheme = scheme;
  return 0;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

int pqc_get_stats(pqc_stats_t *stats) {
  if (!stats) return -1;

  memset(stats, 0, sizeof(*stats));

  stats->kem_encap_count = pqc_global_stats.kem_encap_count;
  stats->kem_decap_count = pqc_global_stats.kem_decap_count;
  stats->sig_sign_count = pqc_global_stats.sig_sign_count;
  stats->sig_verify_count = pqc_global_stats.sig_verify_count;
  stats->hybrid_sign_count = pqc_global_stats.hybrid_sign_count;
  stats->hybrid_verify_count = pqc_global_stats.hybrid_verify_count;
  stats->verify_failures = pqc_global_stats.verify_failures;
  stats->current_scheme = pqc_global_stats.current_scheme;

  return 0;
}
