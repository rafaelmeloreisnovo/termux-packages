#ifndef TERMUX_GPU_INTEGRATION_H
#define TERMUX_GPU_INTEGRATION_H

/*
 * SIMULATED GPU FIXTURE API.
 *
 * This interface currently provides CPU-backed deterministic fixture behavior,
 * not GPU enumeration, GPU memory, GPU kernel compilation or GPU benchmarks.
 * Fixture behavior is available only with RAF_ENABLE_GPU_FIXTURE=1 in the
 * process environment; otherwise simulated runtime operations fail closed.
 *
 * claim_allowed=false
 * physical_gpu_verified=false
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TERMUX_MAX_GPU_DEVICES 4
#define TERMUX_MAX_GPU_KERNELS 16
#define TERMUX_GPU_MEMORY_LIMIT (512 * 1024 * 1024)

typedef enum {
  GPU_DEVICE_ADRENO = 0,
  GPU_DEVICE_MALI = 1,
  GPU_DEVICE_BIONIC = 2,
  GPU_DEVICE_UNKNOWN = 3
} gpu_device_type_t;

typedef enum {
  GPU_KERNEL_CRC32C = 0,
  GPU_KERNEL_REDUCTION = 1,
  GPU_KERNEL_COHERENCE = 2,
  GPU_KERNEL_TRANSPOSE = 3
} gpu_kernel_type_t;

typedef struct {
  uint32_t device_id;
  gpu_device_type_t device_type;
  char device_name[64];
  uint32_t compute_units;
  uint32_t memory_mb;
  float peak_gflops;
  bool available;
} gpu_device_t;

typedef struct {
  gpu_kernel_type_t kernel_type;
  char kernel_name[64];
  uint32_t work_group_size;
  bool compiled;
  uint64_t kernel_handle;
} gpu_kernel_t;

typedef struct {
  uint8_t *device_ptr; /* host-memory fixture pointer, not GPU memory */
  size_t size;
  bool pinned;
  uint32_t ref_count;
} gpu_memory_t;

typedef struct {
  gpu_device_t devices[TERMUX_MAX_GPU_DEVICES];
  uint32_t device_count;
  uint32_t active_device_id;

  gpu_kernel_t kernels[TERMUX_MAX_GPU_KERNELS];
  uint32_t kernel_count;

  gpu_memory_t *memory_allocations;
  uint32_t memory_count;
  uint64_t total_gpu_memory_used;
  uint64_t peak_gpu_memory;

  uint64_t total_gpu_kernel_calls;
  uint64_t gpu_operations_completed;
  double total_gpu_time_ms;

  size_t min_transfer_size;
  double offload_threshold;
} gpu_integration_t;

int termux_gpu_integration_init(gpu_integration_t *gpu);
void termux_gpu_integration_destroy(gpu_integration_t *gpu);
int termux_gpu_detect_devices(gpu_integration_t *gpu);
int termux_gpu_select_device(gpu_integration_t *gpu, uint32_t device_id);
int termux_gpu_compile_kernel(gpu_integration_t *gpu, gpu_kernel_type_t kernel_type);
int termux_gpu_allocate_memory(gpu_integration_t *gpu, size_t size, gpu_memory_t *mem);
int termux_gpu_free_memory(gpu_integration_t *gpu, gpu_memory_t *mem);
int termux_gpu_copy_to_device(gpu_integration_t *gpu, const void *host_ptr,
                              gpu_memory_t *device_mem, size_t size);
int termux_gpu_copy_from_device(gpu_integration_t *gpu, gpu_memory_t *device_mem,
                                void *host_ptr, size_t size);
uint32_t termux_gpu_crc32c_kernel(gpu_integration_t *gpu, const uint8_t *data,
                                  size_t len, uint32_t crc);
uint64_t termux_gpu_reduction_kernel(gpu_integration_t *gpu, const uint32_t *values,
                                     uint32_t count);
double termux_gpu_coherence_kernel(gpu_integration_t *gpu, const uint64_t *scores,
                                   uint32_t count);
bool termux_gpu_should_offload(gpu_integration_t *gpu, size_t data_size,
                               double expected_speedup);
double termux_gpu_efficiency(const gpu_integration_t *gpu);
void termux_gpu_print_stats(const gpu_integration_t *gpu);

#endif
