#include "../gpu_integration.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

static int test_gpu_init(void) {
  printf("\n=== Sprint 12.1: GPU Integration Initialization ===\n");

  gpu_integration_t gpu;
  int ret = termux_gpu_integration_init(&gpu);
  assert(ret == 0);
  assert(gpu.device_count == 0);
  assert(gpu.min_transfer_size == 4096);
  assert(gpu.offload_threshold == 1.5);

  printf("✓ GPU integration initialized\n");
  return 0;
}

static int test_gpu_device_detection(void) {
  printf("\n=== Sprint 12.2: GPU Device Detection ===\n");

  gpu_integration_t gpu;
  termux_gpu_integration_init(&gpu);

  int ret = termux_gpu_detect_devices(&gpu);
  assert(ret == 0);
  assert(gpu.device_count > 0);
  assert(gpu.device_count <= TERMUX_MAX_GPU_DEVICES);

  printf("  Detected %u GPU devices:\n", gpu.device_count);
  for (uint32_t i = 0; i < gpu.device_count; i++) {
    const gpu_device_t *dev = &gpu.devices[i];
    printf("    Device %u: %s (%.1f GFLOPS, %u MB)\n",
           dev->device_id, dev->device_name, dev->peak_gflops, dev->memory_mb);
  }

  printf("✓ GPU device detection working\n");
  return 0;
}

static int test_gpu_device_selection(void) {
  printf("\n=== Sprint 12.3: GPU Device Selection ===\n");

  gpu_integration_t gpu;
  termux_gpu_integration_init(&gpu);
  termux_gpu_detect_devices(&gpu);

  int ret = termux_gpu_select_device(&gpu, 0);
  assert(ret == 0);
  assert(gpu.active_device_id == 0);

  if (gpu.device_count > 1) {
    ret = termux_gpu_select_device(&gpu, 1);
    assert(ret == 0);
    assert(gpu.active_device_id == 1);
  }

  ret = termux_gpu_select_device(&gpu, 999);
  assert(ret == -1);

  printf("✓ GPU device selection working\n");
  return 0;
}

static int test_gpu_kernel_compilation(void) {
  printf("\n=== Sprint 12.4: GPU Kernel Compilation ===\n");

  gpu_integration_t gpu;
  termux_gpu_integration_init(&gpu);
  termux_gpu_detect_devices(&gpu);

  int ret = termux_gpu_compile_kernel(&gpu, GPU_KERNEL_CRC32C);
  assert(ret == 0);
  assert(gpu.kernels[GPU_KERNEL_CRC32C].compiled);

  ret = termux_gpu_compile_kernel(&gpu, GPU_KERNEL_REDUCTION);
  assert(ret == 0);
  assert(gpu.kernels[GPU_KERNEL_REDUCTION].compiled);

  ret = termux_gpu_compile_kernel(&gpu, GPU_KERNEL_COHERENCE);
  assert(ret == 0);

  printf("  Compiled %u GPU kernels\n", gpu.kernel_count);
  printf("✓ GPU kernel compilation working\n");
  return 0;
}

static int test_gpu_memory_management(void) {
  printf("\n=== Sprint 12.5: GPU Memory Management ===\n");

  gpu_integration_t gpu;
  termux_gpu_integration_init(&gpu);

  gpu_memory_t mem1, mem2;
  int ret = termux_gpu_allocate_memory(&gpu, 1024, &mem1);
  assert(ret == 0);
  assert(mem1.device_ptr != NULL);
  assert(mem1.size == 1024);

  ret = termux_gpu_allocate_memory(&gpu, 2048, &mem2);
  assert(ret == 0);
  assert(gpu.total_gpu_memory_used == 3072);

  ret = termux_gpu_free_memory(&gpu, &mem1);
  assert(ret == 0);
  assert(gpu.total_gpu_memory_used == 2048);

  ret = termux_gpu_free_memory(&gpu, &mem2);
  assert(ret == 0);
  assert(gpu.total_gpu_memory_used == 0);

  printf("✓ GPU memory management working\n");
  return 0;
}

static int test_gpu_memory_transfer(void) {
  printf("\n=== Sprint 12.6: GPU Memory Transfer ===\n");

  gpu_integration_t gpu;
  termux_gpu_integration_init(&gpu);

  gpu_memory_t device_mem;
  int ret = termux_gpu_allocate_memory(&gpu, 256, &device_mem);
  assert(ret == 0);

  uint8_t host_data[256];
  for (int i = 0; i < 256; i++) {
    host_data[i] = (uint8_t)(i % 256);
  }

  ret = termux_gpu_copy_to_device(&gpu, host_data, &device_mem, 256);
  assert(ret == 0);

  uint8_t result[256];
  ret = termux_gpu_copy_from_device(&gpu, &device_mem, result, 256);
  assert(ret == 0);

  assert(memcmp(host_data, result, 256) == 0);

  termux_gpu_free_memory(&gpu, &device_mem);
  printf("✓ GPU memory transfer working\n");
  return 0;
}

static int test_gpu_crc32c_kernel(void) {
  printf("\n=== Sprint 12.7: GPU CRC32C Kernel ===\n");

  gpu_integration_t gpu;
  termux_gpu_integration_init(&gpu);
  termux_gpu_detect_devices(&gpu);
  termux_gpu_compile_kernel(&gpu, GPU_KERNEL_CRC32C);

  uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
  uint32_t crc = termux_gpu_crc32c_kernel(&gpu, data, sizeof(data), 0);

  printf("  CRC32C of data: 0x%08x\n", crc);
  assert(gpu.total_gpu_kernel_calls > 0);
  assert(gpu.gpu_operations_completed > 0);

  printf("✓ GPU CRC32C kernel working\n");
  return 0;
}

static int test_gpu_reduction_kernel(void) {
  printf("\n=== Sprint 12.8: GPU Reduction Kernel ===\n");

  gpu_integration_t gpu;
  termux_gpu_integration_init(&gpu);
  termux_gpu_detect_devices(&gpu);
  termux_gpu_compile_kernel(&gpu, GPU_KERNEL_REDUCTION);

  uint32_t values[] = {100, 200, 300, 400, 500};
  uint64_t sum = termux_gpu_reduction_kernel(&gpu, values, 5);

  printf("  Sum of values: %lu\n", sum);
  assert(sum == 1500);
  assert(gpu.total_gpu_kernel_calls > 0);

  printf("✓ GPU reduction kernel working\n");
  return 0;
}

static int test_gpu_coherence_kernel(void) {
  printf("\n=== Sprint 12.9: GPU Coherence Kernel ===\n");

  gpu_integration_t gpu;
  termux_gpu_integration_init(&gpu);
  termux_gpu_detect_devices(&gpu);
  termux_gpu_compile_kernel(&gpu, GPU_KERNEL_COHERENCE);

  uint64_t scores[] = {65536, 65536, 65536, 65536};
  double mean_phi = termux_gpu_coherence_kernel(&gpu, scores, 4);

  printf("  Mean coherence φ: %.4f\n", mean_phi);
  assert(mean_phi > 0.0);
  assert(gpu.total_gpu_kernel_calls > 0);

  printf("✓ GPU coherence kernel working\n");
  return 0;
}

static int test_gpu_offload_decision(void) {
  printf("\n=== Sprint 12.10: GPU Offload Decision ===\n");

  gpu_integration_t gpu;
  termux_gpu_integration_init(&gpu);
  termux_gpu_detect_devices(&gpu);

  bool should_offload = termux_gpu_should_offload(&gpu, 4096, 2.0);
  assert(should_offload == true);

  should_offload = termux_gpu_should_offload(&gpu, 1024, 2.0);
  assert(should_offload == false);

  should_offload = termux_gpu_should_offload(&gpu, 4096, 1.2);
  assert(should_offload == false);

  printf("✓ GPU offload decision working\n");
  return 0;
}

int main(void) {
  printf("\n================================================================================\n");
  printf("                    SPRINT 12: GPU INTEGRATION\n");
  printf("================================================================================\n");

  int all_passed = 0;
  all_passed += test_gpu_init();
  all_passed += test_gpu_device_detection();
  all_passed += test_gpu_device_selection();
  all_passed += test_gpu_kernel_compilation();
  all_passed += test_gpu_memory_management();
  all_passed += test_gpu_memory_transfer();
  all_passed += test_gpu_crc32c_kernel();
  all_passed += test_gpu_reduction_kernel();
  all_passed += test_gpu_coherence_kernel();
  all_passed += test_gpu_offload_decision();

  printf("\n================================================================================\n");
  if (all_passed == 0) {
    printf("✓ ALL SPRINT 12 TESTS PASSED\n");
    printf("  GPU integration initialization: ✓\n");
    printf("  GPU device detection: ✓\n");
    printf("  GPU device selection: ✓\n");
    printf("  GPU kernel compilation: ✓\n");
    printf("  GPU memory management: ✓\n");
    printf("  GPU memory transfer: ✓\n");
    printf("  GPU CRC32C kernel: ✓\n");
    printf("  GPU reduction kernel: ✓\n");
    printf("  GPU coherence kernel: ✓\n");
    printf("  GPU offload decision: ✓\n");
  } else {
    printf("✗ SOME TESTS FAILED\n");
  }
  printf("================================================================================\n\n");

  return all_passed == 0 ? 0 : 1;
}
