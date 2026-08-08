#include "../gpu_integration.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void enable_fixture(void) {
  int ret = setenv("RAF_ENABLE_GPU_FIXTURE", "1", 1);
  assert(ret == 0);
}

static int test_gpu_default_fail_closed(void) {
  printf("\n=== Sprint 12.0: Production Default Gate ===\n");
  unsetenv("RAF_ENABLE_GPU_FIXTURE");
  gpu_integration_t gpu;
  assert(termux_gpu_integration_init(&gpu) == 0);
  int ret = termux_gpu_detect_devices(&gpu);
  assert(ret == -2);
  assert(gpu.device_count == 0);
  printf("✓ simulated GPU enumeration blocked without explicit fixture opt-in\n");
  return 0;
}

static int test_gpu_init(void) {
  printf("\n=== Sprint 12.1: Fixture Initialization ===\n");
  enable_fixture();
  gpu_integration_t gpu;
  int ret = termux_gpu_integration_init(&gpu);
  assert(ret == 0);
  assert(gpu.device_count == 0);
  assert(gpu.min_transfer_size == 4096);
  assert(gpu.offload_threshold == 1.5);
  printf("✓ CPU-backed GPU fixture initialized\n");
  return 0;
}

static int test_gpu_device_detection(void) {
  printf("\n=== Sprint 12.2: Simulated Device Fixture ===\n");
  enable_fixture();
  gpu_integration_t gpu;
  termux_gpu_integration_init(&gpu);
  int ret = termux_gpu_detect_devices(&gpu);
  assert(ret == 0);
  assert(gpu.device_count == 3);
  for (uint32_t i = 0; i < gpu.device_count; i++)
    assert(strstr(gpu.devices[i].device_name, "SIMULATED_FIXTURE") != NULL);
  printf("  Fixture devices: %u (not physical detection)\n", gpu.device_count);
  printf("✓ fixture device population PASSED\n");
  return 0;
}

static int test_gpu_device_selection(void) {
  printf("\n=== Sprint 12.3: Fixture Device Selection ===\n");
  enable_fixture();
  gpu_integration_t gpu;
  termux_gpu_integration_init(&gpu);
  termux_gpu_detect_devices(&gpu);
  assert(termux_gpu_select_device(&gpu, 0) == 0);
  assert(gpu.active_device_id == 0);
  assert(termux_gpu_select_device(&gpu, 999) == -1);
  printf("✓ fixture selection PASSED\n");
  return 0;
}

static int test_gpu_kernel_compilation(void) {
  printf("\n=== Sprint 12.4: Fixture Kernel Tokens ===\n");
  enable_fixture();
  gpu_integration_t gpu;
  termux_gpu_integration_init(&gpu);
  termux_gpu_detect_devices(&gpu);
  assert(termux_gpu_compile_kernel(&gpu, GPU_KERNEL_CRC32C) == 0);
  assert(termux_gpu_compile_kernel(&gpu, GPU_KERNEL_REDUCTION) == 0);
  assert(termux_gpu_compile_kernel(&gpu, GPU_KERNEL_COHERENCE) == 0);
  assert(gpu.kernel_count == 3);
  assert(strstr(gpu.kernels[GPU_KERNEL_CRC32C].kernel_name, "fixture_") != NULL);
  printf("✓ fixture kernel-token creation PASSED (no GPU compiler claimed)\n");
  return 0;
}

static int test_gpu_memory_management(void) {
  printf("\n=== Sprint 12.5: Host-Memory Fixture ===\n");
  enable_fixture();
  gpu_integration_t gpu;
  termux_gpu_integration_init(&gpu);
  gpu_memory_t mem1 = {}, mem2 = {};
  assert(termux_gpu_allocate_memory(&gpu, 1024, &mem1) == 0);
  assert(termux_gpu_allocate_memory(&gpu, 2048, &mem2) == 0);
  assert(gpu.total_gpu_memory_used == 3072);
  assert(termux_gpu_free_memory(&gpu, &mem1) == 0);
  assert(termux_gpu_free_memory(&gpu, &mem2) == 0);
  assert(gpu.total_gpu_memory_used == 0);
  printf("✓ host-memory fixture PASSED (not GPU memory)\n");
  return 0;
}

static int test_gpu_memory_transfer(void) {
  printf("\n=== Sprint 12.6: Host memcpy Fixture ===\n");
  enable_fixture();
  gpu_integration_t gpu;
  termux_gpu_integration_init(&gpu);
  gpu_memory_t mem = {};
  assert(termux_gpu_allocate_memory(&gpu, 256, &mem) == 0);
  uint8_t src[256], dst[256];
  for (int i = 0; i < 256; i++) src[i] = (uint8_t)i;
  assert(termux_gpu_copy_to_device(&gpu, src, &mem, sizeof(src)) == 0);
  assert(termux_gpu_copy_from_device(&gpu, &mem, dst, sizeof(dst)) == 0);
  assert(memcmp(src, dst, sizeof(src)) == 0);
  assert(termux_gpu_free_memory(&gpu, &mem) == 0);
  printf("✓ host memcpy fixture PASSED (not PCI/GPU transfer)\n");
  return 0;
}

static int test_gpu_crc32c_kernel(void) {
  printf("\n=== Sprint 12.7: CPU CRC Fixture ===\n");
  enable_fixture();
  gpu_integration_t gpu;
  termux_gpu_integration_init(&gpu);
  termux_gpu_detect_devices(&gpu);
  uint8_t data[] = {1, 2, 3, 4, 5};
  (void)termux_gpu_crc32c_kernel(&gpu, data, sizeof(data), 0);
  assert(gpu.total_gpu_kernel_calls == 1);
  printf("✓ CPU fixture computation PASSED\n");
  return 0;
}

static int test_gpu_reduction_kernel(void) {
  printf("\n=== Sprint 12.8: CPU Reduction Fixture ===\n");
  enable_fixture();
  gpu_integration_t gpu;
  termux_gpu_integration_init(&gpu);
  termux_gpu_detect_devices(&gpu);
  uint32_t values[] = {100, 200, 300, 400, 500};
  assert(termux_gpu_reduction_kernel(&gpu, values, 5) == 1500);
  printf("✓ CPU reduction fixture PASSED\n");
  return 0;
}

static int test_gpu_coherence_kernel(void) {
  printf("\n=== Sprint 12.9: CPU Coherence Fixture ===\n");
  enable_fixture();
  gpu_integration_t gpu;
  termux_gpu_integration_init(&gpu);
  termux_gpu_detect_devices(&gpu);
  uint64_t scores[] = {65536, 65536, 65536, 65536};
  double mean = termux_gpu_coherence_kernel(&gpu, scores, 4);
  assert(mean == 1.0);
  printf("✓ CPU coherence fixture PASSED\n");
  return 0;
}

static int test_gpu_offload_decision(void) {
  printf("\n=== Sprint 12.10: Fixture Offload Policy ===\n");
  enable_fixture();
  gpu_integration_t gpu;
  termux_gpu_integration_init(&gpu);
  termux_gpu_detect_devices(&gpu);
  assert(termux_gpu_should_offload(&gpu, 4096, 2.0));
  assert(!termux_gpu_should_offload(&gpu, 1024, 2.0));
  assert(!termux_gpu_should_offload(&gpu, 4096, 1.2));
  printf("✓ fixture policy logic PASSED (no speedup measured)\n");
  return 0;
}

int main(void) {
  printf("\n================================================================================\n");
  printf("        SPRINT 12: SIMULATED GPU FIXTURE — NOT PHYSICAL GPU VALIDATION\n");
  printf("================================================================================\n");
  printf("claim_allowed=false\nphysical_gpu_verified=false\n");

  int all_passed = 0;
  all_passed += test_gpu_default_fail_closed();
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
    printf("✓ ALL SPRINT 12 FIXTURE/FAIL-CLOSED TESTS PASSED\n");
    printf("  GPU_RUNTIME=TOKEN_VAZIO_UNLESS_REAL_BACKEND_RECEIPT\n");
    printf("  PRODUCT_READINESS=NOT_CLAIMED\n");
  } else {
    printf("✗ SOME FIXTURE TESTS FAILED\n");
  }
  printf("================================================================================\n\n");
  unsetenv("RAF_ENABLE_GPU_FIXTURE");
  return all_passed == 0 ? 0 : 1;
}
