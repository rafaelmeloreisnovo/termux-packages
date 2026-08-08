#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>

#include "../crypto_ed25519.h"
#include "../crypto_chacha20.h"
#include "../rate_limiter.h"
#include "../crypto_pqc.h"

#define TEST_PASS(name) printf("✓ %s\n", name)
#define TEST_FAIL(name, msg) printf("✗ %s: %s\n", name, msg)

static int tests_passed = 0;
static int tests_failed = 0;

/* ============================================================================
 * Test 1: Ed25519 Key Generation
 * ============================================================================ */

void test_ed25519_keygen(void) {
  uint8_t seed[32];
  for (int i = 0; i < 32; i++) {
    seed[i] = i;
  }

  ed25519_keypair_t keypair;
  if (ed25519_keygen(&keypair, seed) == 0 && keypair.public_key[0] != 0) {
    TEST_PASS("Ed25519 Key Generation");
    tests_passed++;
  } else {
    TEST_FAIL("Ed25519 Key Generation", "keypair generation failed");
    tests_failed++;
  }
}

/* ============================================================================
 * Test 2: Ed25519 Signing & Verification
 * ============================================================================ */

void test_ed25519_sign_verify(void) {
  uint8_t seed[32];
  for (int i = 0; i < 32; i++) {
    seed[i] = i ^ 0xAA;
  }

  ed25519_keypair_t keypair;
  ed25519_keygen(&keypair, seed);

  const char *message = "Phase 9.21: Security Hardening";
  size_t message_len = strlen(message);

  ed25519_sig_t signature;
  if (ed25519_sign(&keypair, (const uint8_t *)message, message_len,
                   &signature) < 0) {
    TEST_FAIL("Ed25519 Signing", "signing failed");
    tests_failed++;
    return;
  }

  ed25519_public_key_t public_key;
  ed25519_extract_public(&keypair, &public_key);

  if (ed25519_verify(&public_key, (const uint8_t *)message, message_len,
                     &signature) == 0) {
    TEST_PASS("Ed25519 Sign & Verify");
    tests_passed++;
  } else {
    TEST_FAIL("Ed25519 Sign & Verify", "verification failed");
    tests_failed++;
  }
}

/* ============================================================================
 * Test 3: Ed25519 Statistics
 * ============================================================================ */

void test_ed25519_stats(void) {
  ed25519_stats_t stats;
  if (ed25519_get_stats(&stats) == 0 && stats.sign_count > 0 &&
      stats.verify_count > 0) {
    printf("  Signs: %u, Verifies: %u\n", stats.sign_count, stats.verify_count);
    TEST_PASS("Ed25519 Statistics");
    tests_passed++;
  } else {
    TEST_FAIL("Ed25519 Statistics", "stats invalid");
    tests_failed++;
  }
}

/* ============================================================================
 * Test 4: ChaCha20-Poly1305 Encryption
 * ============================================================================ */

void test_chacha20_seal_open(void) {
  uint8_t key[32], nonce[12];
  for (int i = 0; i < 32; i++) key[i] = i;
  for (int i = 0; i < 12; i++) nonce[i] = i ^ 0x55;

  chacha20_cipher_t cipher;
  if (chacha20_init(&cipher, key, nonce) < 0) {
    TEST_FAIL("ChaCha20 Initialization", "init failed");
    tests_failed++;
    return;
  }

  const char *plaintext = "Secure Message for Distribution";
  size_t plaintext_len = strlen(plaintext);

  chacha20_sealed_t sealed;
  if (chacha20_seal(&cipher, (const uint8_t *)plaintext, plaintext_len, NULL,
                    0, &sealed) < 0) {
    TEST_FAIL("ChaCha20 Seal", "encryption failed");
    tests_failed++;
    return;
  }

  chacha20_opened_t opened;
  if (chacha20_open(&cipher, &sealed, NULL, 0, &opened) == 0 &&
      opened.plaintext_len == plaintext_len &&
      memcmp(opened.plaintext, plaintext, plaintext_len) == 0) {
    TEST_PASS("ChaCha20 Seal & Open");
    tests_passed++;
  } else {
    TEST_FAIL("ChaCha20 Seal & Open", "decryption mismatch");
    tests_failed++;
  }
}

/* ============================================================================
 * Test 5: ChaCha20 Statistics
 * ============================================================================ */

void test_chacha20_stats(void) {
  chacha20_stats_t stats;
  if (chacha20_get_stats(&stats) == 0 && stats.seal_count > 0 &&
      stats.open_count > 0) {
    printf("  Encryptions: %u, Decryptions: %u\n", stats.seal_count,
           stats.open_count);
    TEST_PASS("ChaCha20 Statistics");
    tests_passed++;
  } else {
    TEST_FAIL("ChaCha20 Statistics", "stats invalid");
    tests_failed++;
  }
}

/* ============================================================================
 * Test 6: Rate Limiter Initialization
 * ============================================================================ */

void test_rate_limiter_init(void) {
  rate_limiter_t limiter;
  if (rate_limiter_init(&limiter, 100, 1000) == 0) {
    TEST_PASS("Rate Limiter Initialization");
    tests_passed++;
    rate_limiter_free(&limiter);
  } else {
    TEST_FAIL("Rate Limiter Initialization", "init failed");
    tests_failed++;
  }
}

/* ============================================================================
 * Test 7: Rate Limiter Token Bucket
 * ============================================================================ */

void test_rate_limiter_allow(void) {
  rate_limiter_t limiter;
  rate_limiter_init(&limiter, 10, 100);

  const char *client_id = "test-client-001";
  int allowed = 0, denied = 0;

  for (int i = 0; i < 150; i++) {
    if (rate_limiter_allow(&limiter, client_id, 1) == 0) {
      allowed++;
    } else {
      denied++;
    }
  }

  if (allowed > 0 && denied >= 0) {
    printf("  Allowed: %d, Denied: %d\n", allowed, denied);
    TEST_PASS("Rate Limiter Token Bucket");
    tests_passed++;
  } else {
    TEST_FAIL("Rate Limiter Token Bucket", "rate limiting failed");
    tests_failed++;
  }

  rate_limiter_free(&limiter);
}

/* ============================================================================
 * Test 8: Rate Limiter Statistics
 * ============================================================================ */

void test_rate_limiter_stats(void) {
  rate_limiter_t limiter;
  rate_limiter_init(&limiter, 10, 100);

  rate_limiter_allow(&limiter, "client-A", 1);
  rate_limiter_allow(&limiter, "client-B", 1);

  rate_limiter_stats_t stats;
  if (rate_limiter_get_stats(&limiter, &stats) == 0 && stats.total_clients > 0) {
    printf("  Active clients: %u, Denial rate: %.2f%%\n", stats.active_clients,
           stats.denial_rate * 100);
    TEST_PASS("Rate Limiter Statistics");
    tests_passed++;
  } else {
    TEST_FAIL("Rate Limiter Statistics", "stats invalid");
    tests_failed++;
  }

  rate_limiter_free(&limiter);
}

/* ============================================================================
 * Test 9: PQC KEM (Key Encapsulation)
 * ============================================================================ */

void test_pqc_kem(void) {
  pqc_kem_keypair_t keypair;
  if (pqc_kem_keygen(&keypair) < 0) {
    TEST_FAIL("PQC KEM Keygen", "keygen failed");
    tests_failed++;
    return;
  }

  pqc_kem_encapsulation_t encap;
  if (pqc_kem_encapsulate(keypair.public_key, &encap) < 0) {
    TEST_FAIL("PQC KEM Encapsulate", "encapsulation failed");
    tests_failed++;
    return;
  }

  uint8_t shared_secret[32];
  if (pqc_kem_decapsulate(keypair.secret_key, encap.ciphertext,
                          shared_secret) == 0 &&
      memcmp(shared_secret, encap.shared_secret, 32) == 0) {
    TEST_PASS("PQC KEM");
    tests_passed++;
  } else {
    TEST_FAIL("PQC KEM", "shared secret mismatch");
    tests_failed++;
  }
}

/* ============================================================================
 * Test 10: PQC Signatures
 * ============================================================================ */

void test_pqc_signatures(void) {
  pqc_sig_keypair_t keypair;
  if (pqc_sig_keygen(&keypair) < 0) {
    TEST_FAIL("PQC Signatures Keygen", "keygen failed");
    tests_failed++;
    return;
  }

  const char *message = "Post-Quantum Cryptography Test";
  size_t message_len = strlen(message);

  pqc_sig_t signature;
  if (pqc_sig_sign(keypair.secret_key, (const uint8_t *)message, message_len,
                   &signature) < 0) {
    TEST_FAIL("PQC Signatures Sign", "signing failed");
    tests_failed++;
    return;
  }

  if (pqc_sig_verify(keypair.public_key, (const uint8_t *)message, message_len,
                     &signature) == 0) {
    TEST_PASS("PQC Signatures");
    tests_passed++;
  } else {
    TEST_FAIL("PQC Signatures", "verification failed");
    tests_failed++;
  }
}

/* ============================================================================
 * Test 11: PQC Hybrid Scheme
 * ============================================================================ */

void test_pqc_hybrid(void) {
  pqc_hybrid_private_key_t priv;
  pqc_hybrid_public_key_t pub;

  if (pqc_hybrid_keygen(&priv, &pub) < 0) {
    TEST_FAIL("PQC Hybrid Keygen", "keygen failed");
    tests_failed++;
    return;
  }

  const char *message = "Hybrid Ed25519 + DILITHIUM Signature";
  size_t message_len = strlen(message);

  uint8_t hybrid_sig[2500];
  if (pqc_hybrid_sign(&priv, (const uint8_t *)message, message_len,
                      hybrid_sig) < 0) {
    TEST_FAIL("PQC Hybrid Sign", "signing failed");
    tests_failed++;
    return;
  }

  if (pqc_hybrid_verify(&pub, (const uint8_t *)message, message_len,
                        hybrid_sig) == 0) {
    TEST_PASS("PQC Hybrid Scheme");
    tests_passed++;
  } else {
    TEST_FAIL("PQC Hybrid Scheme", "verification failed");
    tests_failed++;
  }
}

/* ============================================================================
 * Test 12: PQC Statistics
 * ============================================================================ */

void test_pqc_stats(void) {
  pqc_stats_t stats;
  if (pqc_get_stats(&stats) == 0 && stats.current_scheme == PQC_SCHEME_HYBRID) {
    printf("  KEM: %u encap, %u decap\n", stats.kem_encap_count,
           stats.kem_decap_count);
    printf("  Signatures: %u sign, %u verify\n", stats.sig_sign_count,
           stats.sig_verify_count);
    printf("  Hybrid: %u sign, %u verify\n", stats.hybrid_sign_count,
           stats.hybrid_verify_count);
    TEST_PASS("PQC Statistics");
    tests_passed++;
  } else {
    TEST_FAIL("PQC Statistics", "stats invalid");
    tests_failed++;
  }
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(void) {
  printf("\n=== Phase 9.21: Security Hardening Test Suite ===\n");
  printf("Testing Ed25519, ChaCha20-Poly1305, Rate Limiting, and PQC\n\n");

  test_ed25519_keygen();
  test_ed25519_sign_verify();
  test_ed25519_stats();

  test_chacha20_seal_open();
  test_chacha20_stats();

  test_rate_limiter_init();
  test_rate_limiter_allow();
  test_rate_limiter_stats();

  test_pqc_kem();
  test_pqc_signatures();
  test_pqc_hybrid();
  test_pqc_stats();

  printf("\n=== Summary ===\n");
  printf("Passed: %d/12\n", tests_passed);
  printf("Failed: %d/12\n", tests_failed);

  if (tests_failed == 0) {
    printf("\n✓✓✓ All tests PASSED\n");
    return 0;
  } else {
    printf("\n✗✗✗ Some tests FAILED\n");
    return 1;
  }
}
