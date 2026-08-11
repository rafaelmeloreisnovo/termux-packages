#include "gpu_integration.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/*
 * SIMULATED_FIXTURE GPU backend.
 *
 * This file does not enumerate a host GPU, compile a real GPU kernel, allocate
 * device memory, or measure GPU time. Historical deterministic behavior is
 * retained only when RAF_ENABLE_GPU_FIXTURE=1 is explicitly present.
 * Without that opt-in, all simulated device/runtime operations fail closed.
 */

static const char *gpu_fixture_names[] = {
  "SIMULATED_FIXTURE Qualcomm Adreno 660",
  "SIMULATED_FIXTURE ARM Mali-G78",
  "SIMULATED_FIXTURE Snapdragon Bionic GPU",
  "SIMULATED_FIXTURE Unknown GPU"
};

static int gpu_fixture_enabled(void) {
  const char *v = getenv("RAF_ENABLE_GPU_FIXTURE");
  return v != NULL && strcmp(v, "1") == 0;
}

int termux_gpu_integration_init(gpu_integration_t *gpu) {
  if (!gpu) return -1;
  memset(gpu, 0, sizeof(*gpu));
  gpu->min_transfer_size = 4096;
  gpu->offload_threshold = 1.5;
  return 0;
}

void termux_gpu_integration_destroy(gpu_integration_t *gpu) {
  if (!gpu) return;
  if (gpu->memory_allocations) {
    free(gpu->memory_allocations);
    gpu->memory_allocations = NULL;
  }
  memset(gpu, 0, sizeof(*gpu));
}

int termux_gpu_detect_devices(gpu_integration_t *gpu) {
  if (!gpu) return -1;
  gpu->device_count = 0;
  if (!gpu_fixture_enabled()) return -2;

  for (uint32_t i = 0; i < TERMUX_MAX_GPU_DEVICES && i < 3; i++) {
    gpu_device_t *dev = &gpu->devices[i];
    dev->device_id = i;
    dev->device_type = (gpu_device_type_t)i;
    dev->available = true;
    strncpy(dev->device_name, gpu_fixture_names[i], sizeof(dev->device_name) - 1);

    switch (i) {
      case GPU_DEVICE_ADRENO:
        dev->compute_units = 2;
        dev->memory_mb = 6144;
        dev->peak_gflops = 2560.0f;
        break;
      case GPU_DEVICE_MALI:
        dev->compute_units = 10;
        dev->memory_mb = 4096;
        dev->peak_gflops = 1310.0f;
        break;
      case GPU_DEVICE_BIONIC:
        dev->compute_units = 8;
        dev->memory_mb = 5120;
        dev->peak_gflops = 1536.0f;
        break;
      default:
        dev->available = false;
        break;
    }
    gpu->device_count++;
  }

  if (gpu->device_count > 0) gpu->active_device_id = 0;
  return gpu->device_count > 0 ? 0 : -1;
}

int termux_gpu_select_device(gpu_integration_t *gpu, uint32_t device_id) {
  if (!gpu) return -1;
  if (!gpu_fixture_enabled()) return -2;
  if (device_id >= gpu->device_count || !gpu->devices[device_id].available)
    return -1;
  gpu->active_device_id = device_id;
  return 0;
}

int termux_gpu_compile_kernel(gpu_integration_t *gpu, gpu_kernel_type_t kernel_type) {
  if (!gpu) return -1;
  if (!gpu_fixture_enabled()) return -2;
  if (gpu->device_count == 0 || kernel_type >= TERMUX_MAX_GPU_KERNELS) return -1;

  gpu_kernel_t *kernel = &gpu->kernels[kernel_type];
  kernel->kernel_type = kernel_type;
  kernel->compiled = true;
  kernel->kernel_handle = (uint64_t)(1000 + kernel_type); /* fixture token */

  switch (kernel_type) {
    case GPU_KERNEL_CRC32C:
      strncpy(kernel->kernel_name, "fixture_crc32c_cpu", sizeof(kernel->kernel_name) - 1);
      kernel->work_group_size = 256;
      break;
    case GPU_KERNEL_REDUCTION:
      strncpy(kernel->kernel_name, "fixture_reduction_cpu", sizeof(kernel->kernel_name) - 1);
      kernel->work_group_size = 256;
      break;
    case GPU_KERNEL_COHERENCE:
      strncpy(kernel->kernel_name, "fixture_coherence_cpu", sizeof(kernel->kernel_name) - 1);
      kernel->work_group_size = 128;
      break;
    case GPU_KERNEL_TRANSPOSE:
      strncpy(kernel->kernel_name, "fixture_transpose_cpu", sizeof(kernel->kernel_name) - 1);
      kernel->work_group_size = 128;
      break;
    default:
      return -1;
  }
  gpu->kernel_count++;
  return 0;
}

int termux_gpu_allocate_memory(gpu_integration_t *gpu, size_t size, gpu_memory_t *mem) {
  if (!gpu || !mem || size == 0) return -1;
  if (!gpu_fixture_enabled()) return -2;
  if (gpu->total_gpu_memory_used + size > TERMUX_GPU_MEMORY_LIMIT) return -3;

  /* Host malloc used only as a deterministic fixture; not GPU device memory. */
  mem->device_ptr = (uint8_t *)malloc(size);
  if (!mem->device_ptr) return -1;
  mem->size = size;
  mem->pinned = false;
  mem->ref_count = 1;
  gpu->total_gpu_memory_used += size;
  if (gpu->total_gpu_memory_used > gpu->peak_gpu_memory)
    gpu->peak_gpu_memory = gpu->total_gpu_memory_used;
  gpu->memory_count++;
  return 0;
}

int termux_gpu_free_memory(gpu_integration_t *gpu, gpu_memory_t *mem) {
  if (!gpu || !mem || !mem->device_ptr) return -1;
  if (!gpu_fixture_enabled()) return -2;
  if (mem->ref_count > 1) {
    mem->ref_count--;
    return 0;
  }
  gpu->total_gpu_memory_used -= mem->size;
  free(mem->device_ptr);
  mem->device_ptr = NULL;
  mem->size = 0;
  mem->ref_count = 0;
  gpu->memory_count--;
  return 0;
}

int termux_gpu_copy_to_device(gpu_integration_t *gpu, const void *host_ptr,
                               gpu_memory_t *device_mem, size_t size) {
  if (!gpu || !host_ptr || !device_mem || !device_mem->device_ptr || size == 0)
    return -1;
  if (!gpu_fixture_enabled()) return -2;
  if (size > device_mem->size) return -1;
  memcpy(device_mem->device_ptr, host_ptr, size);
  return 0;
}

int termux_gpu_copy_from_device(gpu_integration_t *gpu, gpu_memory_t *device_mem,
                                 void *host_ptr, size_t size) {
  if (!gpu || !host_ptr || !device_mem || !device_mem->device_ptr || size == 0)
    return -1;
  if (!gpu_fixture_enabled()) return -2;
  if (size > device_mem->size) return -1;
  memcpy(host_ptr, device_mem->device_ptr, size);
  return 0;
}

uint32_t termux_gpu_crc32c_kernel(gpu_integration_t *gpu, const uint8_t *data,
                                   size_t len, uint32_t crc) {
  if (!gpu || !data || len == 0 || !gpu_fixture_enabled()) return crc;
  gpu->total_gpu_kernel_calls++;
  uint32_t result = crc;
  for (size_t i = 0; i < len; i++) {
    result ^= data[i];
    for (int j = 0; j < 8; j++)
      result = (result & 1) ? ((result >> 1) ^ 0x1EDC6F41) : (result >> 1);
  }
  gpu->gpu_operations_completed++;
  return result;
}

uint64_t termux_gpu_reduction_kernel(gpu_integration_t *gpu, const uint32_t *values,
                                      uint32_t count) {
  if (!gpu || !values || count == 0 || !gpu_fixture_enabled()) return 0;
  gpu->total_gpu_kernel_calls++;
  uint64_t sum = 0;
  for (uint32_t i = 0; i < count; i++) sum += values[i];
  gpu->gpu_operations_completed++;
  return sum;
}

double termux_gpu_coherence_kernel(gpu_integration_t *gpu, const uint64_t *scores,
                                    uint32_t count) {
  if (!gpu || !scores || count == 0 || !gpu_fixture_enabled()) return 0.0;
  gpu->total_gpu_kernel_calls++;
  uint64_t sum = 0;
  for (uint32_t i = 0; i < count; i++) sum += scores[i];
  gpu->gpu_operations_completed++;
  return (double)sum / count / 65536.0;
}

bool termux_gpu_should_offload(gpu_integration_t *gpu, size_t data_size,
                               double expected_speedup) {
  if (!gpu || !gpu_fixture_enabled() || gpu->device_count == 0) return false;
  if (data_size < gpu->min_transfer_size) return false;
  if (expected_speedup < gpu->offload_threshold) return false;
  return true;
}

double termux_gpu_efficiency(const gpu_integration_t *gpu) {
  (void)gpu;
  /* No fake throughput/efficiency value is emitted for the CPU fixture. */
  return 0.0;
}

void termux_gpu_print_stats(const gpu_integration_t *gpu) {
  if (!gpu) return;
  printf("\n================================================================================\n");
  printf("        SIMULATED GPU FIXTURE STATISTICS — NOT PHYSICAL GPU EVIDENCE\n");
  printf("================================================================================\n");
  printf("fixture_enabled=%s\n", gpu_fixture_enabled() ? "true" : "false");
  printf("fixture_devices=%u\n", gpu->device_count);
  printf("fixture_kernel_calls=%" PRIu64 "\n", gpu->total_gpu_kernel_calls);
  printf("claim_allowed=false\n");
  printf("physical_gpu_verified=false\n");
  printf("================================================================================\n\n");
}
