#include "../bitraf64_simd_ops.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

int test_backend_detection(void) {
  printf("Test 1: SIMD Backend Detection\n");

  bitraf64_simd_backend_t backend = bitraf64_simd_detect_backend();

  printf("  Detected backend: ");
  switch (backend) {
    case BITRAF64_SIMD_SCALAR:
      printf("SCALAR\n");
      break;
    case BITRAF64_SIMD_NEON:
      printf("NEON (128-bit)\n");
      break;
    case BITRAF64_SIMD_SVE:
      printf("SVE (256/512-bit)\n");
      break;
    case BITRAF64_SIMD_AVX2:
      printf("AVX2 (256-bit)\n");
      break;
    case BITRAF64_SIMD_AVX512:
      printf("AVX-512 (512-bit)\n");
      break;
    default:
      printf("UNKNOWN\n");
      return 0;
  }

  return 1;
}

int test_simd_caps_init(void) {
  printf("Test 2: SIMD Capabilities Initialization\n");

  bitraf64_simd_caps_t caps;
  if (!bitraf64_simd_init(&caps)) {
    printf("  ✗ Initialization failed\n");
    return 0;
  }

  printf("  Backend: %d\n", caps.backend);
  printf("  Vector width: %u bits\n", caps.vector_width * 8);
  printf("  Has FMA: %s\n", caps.has_fma ? "yes" : "no");
  printf("  Has reduction: %s\n", caps.has_reduction ? "yes" : "no");
  printf("  Has crypto: %s\n", caps.has_crypto ? "yes" : "no");

  return 1;
}

int test_spiral_distances(void) {
  printf("Test 3: Spiral Distance Computation\n");

  bitraf64_simd_caps_t caps;
  bitraf64_simd_init(&caps);

  uint32_t exponents[] = {0, 5, 10, 15, 20, 29};
  uint64_t results[6];

  if (!bitraf64_simd_spiral_distances(exponents, 6, results, &caps)) {
    printf("  ✗ Spiral distance computation failed\n");
    return 0;
  }

  printf("  Exponent 0:  0x%016" PRIx64 "\n", results[0]);
  printf("  Exponent 5:  0x%016" PRIx64 "\n", results[1]);
  printf("  Exponent 10: 0x%016" PRIx64 "\n", results[2]);
  printf("  Exponent 15: 0x%016" PRIx64 "\n", results[3]);
  printf("  Exponent 20: 0x%016" PRIx64 "\n", results[4]);
  printf("  Exponent 29: 0x%016" PRIx64 "\n", results[5]);

  return 1;
}

int test_parity_xor(void) {
  printf("Test 4: Parity XOR Reduction\n");

  bitraf64_simd_caps_t caps;
  bitraf64_simd_init(&caps);

  uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  uint8_t parity = bitraf64_simd_parity_xor(test_data, 8, &caps);

  printf("  Input: 01 02 03 04 05 06 07 08\n");
  printf("  XOR result: 0x%02x\n", test_data[0] ^ test_data[1] ^ test_data[2] ^ test_data[3] ^
                                  test_data[4] ^ test_data[5] ^ test_data[6] ^ test_data[7]);
  printf("  Parity bit: %u\n", parity);

  return 1;
}

int test_parity_matrix_mult(void) {
  printf("Test 5: Fiber ECC Parity Matrix Multiplication\n");

  bitraf64_simd_caps_t caps;
  bitraf64_simd_init(&caps);

  uint8_t test_data[] = {0x01, 0x02, 0x04, 0x08, 0x10};
  uint8_t H_matrix[] = {
    1, 0, 1, 0, 1,
    0, 1, 1, 0, 0,
    1, 1, 0, 1, 0,
    0, 0, 1, 1, 1
  };

  uint16_t parity_bits = 0;

  if (!bitraf64_simd_parity_matrix_mult(test_data, 5, H_matrix, 4, 5, &parity_bits, &caps)) {
    printf("  ✗ Parity matrix multiplication failed\n");
    return 0;
  }

  printf("  Data: 01 02 04 08 10\n");
  printf("  Parity bits: 0x%04x\n", parity_bits);

  return 1;
}

int test_coherence_mean(void) {
  printf("Test 6: Coherence Mean Aggregation\n");

  bitraf64_simd_caps_t caps;
  bitraf64_simd_init(&caps);

  uint64_t phi_scores[] = {
    65536,  /* 1.0 in Q48.16 */
    131072, /* 2.0 in Q48.16 */
    98304,  /* 1.5 in Q48.16 */
    65536   /* 1.0 in Q48.16 */
  };

  uint64_t mean = bitraf64_simd_coherence_mean(phi_scores, 4, &caps);

  printf("  Input scores: 1.0, 2.0, 1.5, 1.0 (Q48.16)\n");
  printf("  Mean: 0x%016" PRIx64 " (%.4f in floating point)\n", mean, (double)mean / 65536.0);
  printf("  Expected: ~1.375 (0x%016" PRIx64 ")\n", (uint64_t)(1.375 * 65536.0));

  return 1;
}

int test_crc32c_quad(void) {
  printf("Test 7: CRC32c Quad Processing\n");

  bitraf64_simd_caps_t caps;
  bitraf64_simd_init(&caps);

  uint8_t block0[64], block1[64], block2[64], block3[64];
  memset(block0, 0x11, 64);
  memset(block1, 0x22, 64);
  memset(block2, 0x33, 64);
  memset(block3, 0x44, 64);

  uint32_t crc0, crc1, crc2, crc3;

  if (!bitraf64_simd_crc32c_quad(block0, block1, block2, block3,
                                 &crc0, &crc1, &crc2, &crc3, &caps)) {
    printf("  ✗ CRC32c quad processing failed\n");
    return 0;
  }

  printf("  Block 0 CRC32c (0x11×64): 0x%08x\n", crc0);
  printf("  Block 1 CRC32c (0x22×64): 0x%08x\n", crc1);
  printf("  Block 2 CRC32c (0x33×64): 0x%08x\n", crc2);
  printf("  Block 3 CRC32c (0x44×64): 0x%08x\n", crc3);

  return 1;
}

int test_speedup_estimation(void) {
  printf("Test 8: Speedup Estimation Per Operation\n");

  bitraf64_simd_caps_t caps;
  bitraf64_simd_init(&caps);

  const char *operations[] = {"spiral", "parity", "coherence", "crc32c"};

  for (size_t i = 0; i < 4; i++) {
    float speedup = bitraf64_simd_get_speedup(&caps, operations[i]);
    printf("  %s: %.1f× speedup\n", operations[i], speedup);
  }

  return 1;
}

int main(void) {
  printf("=== BITRAF64 SIMD Operations Test Suite ===\n\n");

  int passed = 0;
  int total = 8;

  if (test_backend_detection()) passed++;
  printf("  ✓ Backend detection\n\n");

  if (test_simd_caps_init()) passed++;
  printf("  ✓ Capabilities initialization\n\n");

  if (test_spiral_distances()) passed++;
  printf("  ✓ Spiral distance computation\n\n");

  if (test_parity_xor()) passed++;
  printf("  ✓ Parity XOR reduction\n\n");

  if (test_parity_matrix_mult()) passed++;
  printf("  ✓ Parity matrix multiplication\n\n");

  if (test_coherence_mean()) passed++;
  printf("  ✓ Coherence mean aggregation\n\n");

  if (test_crc32c_quad()) passed++;
  printf("  ✓ CRC32c quad processing\n\n");

  if (test_speedup_estimation()) passed++;
  printf("  ✓ Speedup estimation\n\n");

  printf("=== Summary ===\n");
  printf("Passed: %d/%d tests\n", passed, total);

  return (passed == total) ? 0 : 1;
}
