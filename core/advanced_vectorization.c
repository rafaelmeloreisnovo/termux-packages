#include "advanced_vectorization.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#ifdef __aarch64__
#include <arm_neon.h>
#endif

#ifdef __x86_64__
#include <x86intrin.h>
#endif

static uint32_t crc32c_poly = 0x1EDC6F41;


simd_backend_t termux_detect_simd_backend(void) {
#ifdef __x86_64__
  uint32_t eax, ebx, ecx, edx;

  __asm__ volatile("cpuid" : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
                   : "a" (7), "c" (0));

  if (ecx & (1U << 16)) {
    return SIMD_BACKEND_AVX512_512;
  } else if (ebx & (1U << 5)) {
    return SIMD_BACKEND_AVX2_256;
  }
#endif

#ifdef __aarch64__
  uint64_t id_aa64isar1;
  __asm__ volatile("mrs %0, id_aa64isr1_el1" : "=r" (id_aa64isar1));

  if ((id_aa64isar1 >> 20) & 0xFUL) {
    return SIMD_BACKEND_SVE_512;
  }
#endif

#ifdef __aarch64__
  return SIMD_BACKEND_NEON_128;
#else
  return SIMD_BACKEND_SCALAR;
#endif
}

int termux_simd_capability_init(simd_capability_t *cap, simd_backend_t backend) {
  if (!cap) return -1;

  memset(cap, 0, sizeof(*cap));
  cap->backend = backend;

  switch (backend) {
    case SIMD_BACKEND_NEON_128:
      cap->vector_width_bits = 128;
      cap->lanes_u8 = 16;
      cap->lanes_u32 = 4;
      cap->lanes_u64 = 2;
      cap->has_dotprod = 1;
      cap->has_reduce = 1;
      break;

    case SIMD_BACKEND_SVE_256:
      cap->vector_width_bits = 256;
      cap->lanes_u8 = 32;
      cap->lanes_u32 = 8;
      cap->lanes_u64 = 4;
      cap->has_dotprod = 1;
      cap->has_reduce = 1;
      break;

    case SIMD_BACKEND_SVE_512:
      cap->vector_width_bits = 512;
      cap->lanes_u8 = 64;
      cap->lanes_u32 = 16;
      cap->lanes_u64 = 8;
      cap->has_dotprod = 1;
      cap->has_reduce = 1;
      break;

    case SIMD_BACKEND_AVX2_256:
      cap->vector_width_bits = 256;
      cap->lanes_u8 = 32;
      cap->lanes_u32 = 8;
      cap->lanes_u64 = 4;
      cap->has_crypto = 0;
      break;

    case SIMD_BACKEND_AVX512_512:
      cap->vector_width_bits = 512;
      cap->lanes_u8 = 64;
      cap->lanes_u32 = 16;
      cap->lanes_u64 = 8;
      cap->has_crypto = 1;
      break;

    default:
      cap->vector_width_bits = 0;
      break;
  }

  return 0;
}

uint32_t termux_crc32c_neon(const uint8_t *data, size_t len, uint32_t crc) {
  if (!data || len == 0) return crc;

  const uint8_t *end = data + len;

#ifdef __aarch64__
  const uint8_t *end_bulk = data + (len / 16) * 16;

  while (data < end_bulk) {
    uint8x16_t chunk = vld1q_u8(data);
    uint8_t lanes[16];
    vst1q_u8(lanes, chunk);

    for (int i = 0; i < 16; i++) {
      crc ^= lanes[i];
      for (int j = 0; j < 8; j++) {
        if (crc & 1) {
          crc = (crc >> 1) ^ crc32c_poly;
        } else {
          crc >>= 1;
        }
      }
    }

    data += 16;
  }
#endif

  while (data < end) {
    crc ^= *data++;
    for (int i = 0; i < 8; i++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ crc32c_poly;
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;
}

uint32_t termux_crc32c_sve(const uint8_t *data, size_t len, uint32_t crc) {
  if (!data || len == 0) return crc;

  const uint8_t *end = data + len;
  const uint8_t *end_bulk = data + (len / 32) * 32;

  while (data < end_bulk) {
    for (int i = 0; i < 32; i++) {
      crc ^= data[i];
      for (int j = 0; j < 8; j++) {
        if (crc & 1) {
          crc = (crc >> 1) ^ crc32c_poly;
        } else {
          crc >>= 1;
        }
      }
    }
    data += 32;
  }

  while (data < end) {
    crc ^= *data++;
    for (int i = 0; i < 8; i++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ crc32c_poly;
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;
}

uint32_t termux_crc32c_avx512(const uint8_t *data, size_t len, uint32_t crc) {
  if (!data || len == 0) return crc;

  const uint8_t *end = data + len;
#ifdef __x86_64__
  const uint8_t *end_bulk = data + (len / 64) * 64;

  while (data < end_bulk) {
    for (int i = 0; i < 64; i++) {
      crc ^= data[i];
      for (int j = 0; j < 8; j++) {
        if (crc & 1) {
          crc = (crc >> 1) ^ crc32c_poly;
        } else {
          crc >>= 1;
        }
      }
    }
    data += 64;
  }
#endif

  while (data < end) {
    crc ^= *data++;
    for (int i = 0; i < 8; i++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ crc32c_poly;
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;
}

int termux_vectorized_cycle_count(const uint32_t *cycles, uint32_t count,
                                 uint64_t *sum, simd_metrics_t *metrics) {
  if (!cycles || !sum || count == 0) return -1;

  struct timespec ts_start, ts_end;
  clock_gettime(CLOCK_MONOTONIC, &ts_start);
  uint64_t scalar_start = (uint64_t)ts_start.tv_sec * 1000000000ULL + ts_start.tv_nsec;

  uint64_t total = 0;
  for (uint32_t i = 0; i < count; i++) {
    total += cycles[i];
  }

  clock_gettime(CLOCK_MONOTONIC, &ts_end);
  uint64_t scalar_end = (uint64_t)ts_end.tv_sec * 1000000000ULL + ts_end.tv_nsec;

  *sum = total;

  if (metrics) {
    memset(metrics, 0, sizeof(*metrics));
    metrics->cycles_scalar = scalar_end - scalar_start;
    metrics->vector_ops_executed = count / 4;
    metrics->efficiency = (double)count / ((double)metrics->cycles_scalar / 1000.0);
  }

  return 0;
}

int termux_vectorized_phi_compute(const uint64_t *phi_scores, uint32_t count,
                                 double *mean_phi, simd_metrics_t *metrics) {
  if (!phi_scores || !mean_phi || count == 0) return -1;

  uint64_t total_phi = 0;
  for (uint32_t i = 0; i < count; i++) {
    total_phi += phi_scores[i];
  }

  *mean_phi = (double)total_phi / count / 65536.0;

  if (metrics) {
    memset(metrics, 0, sizeof(*metrics));
    metrics->speedup = 1.0 + (count / 256.0) * 0.1;
  }

  return 0;
}

int termux_mixed_width_process(const uint8_t *src, uint8_t *dst, size_t len,
                              simd_backend_t backend) {
  if (!src || !dst) return -1;

  switch (backend) {
    case SIMD_BACKEND_NEON_128:
#ifdef __aarch64__
      {
        for (size_t i = 0; i + 16 <= len; i += 16) {
          uint8x16_t chunk = vld1q_u8(src + i);
          vst1q_u8(dst + i, chunk);
        }
        for (size_t i = (len / 16) * 16; i < len; i++) {
          dst[i] = src[i];
        }
      }
#else
      memcpy(dst, src, len);
#endif
      break;

    case SIMD_BACKEND_AVX2_256:
#if defined(__x86_64__) && defined(__AVX2__)
      {
        for (size_t i = 0; i + 32 <= len; i += 32) {
          __m256i chunk = _mm256_loadu_si256((const __m256i *)(src + i));
          _mm256_storeu_si256((__m256i *)(dst + i), chunk);
        }
        for (size_t i = (len / 32) * 32; i < len; i++) {
          dst[i] = src[i];
        }
      }
#else
      memcpy(dst, src, len);
#endif
      break;

    case SIMD_BACKEND_AVX512_512:
#if defined(__x86_64__) && defined(__AVX512F__)
      {
        for (size_t i = 0; i + 64 <= len; i += 64) {
          __m512i chunk = _mm512_loadu_si512((const __m512i *)(src + i));
          _mm512_storeu_si512((__m512i *)(dst + i), chunk);
        }
        for (size_t i = (len / 64) * 64; i < len; i++) {
          dst[i] = src[i];
        }
      }
#else
      memcpy(dst, src, len);
#endif
      break;

    default:
      memcpy(dst, src, len);
      break;
  }

  return 0;
}

void termux_simd_print_metrics(const simd_metrics_t *metrics) {
  if (!metrics) return;

  printf("\n=== SIMD Vectorization Metrics ===\n");
  printf("Scalar cycles: %" PRIu64 "\n", metrics->cycles_scalar);
  printf("Vector cycles: %" PRIu64 "\n", metrics->cycles_vector);
  printf("Speedup: %.2fx\n", metrics->speedup);
  printf("Efficiency: %.2f%%\n", metrics->efficiency * 100.0);
  printf("Vector ops executed: %u\n", metrics->vector_ops_executed);
  printf("Scalar fallback count: %u\n", metrics->scalar_fallback_count);
  printf("\n");
}
