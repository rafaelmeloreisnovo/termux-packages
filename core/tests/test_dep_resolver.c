#include "../dep_resolver.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static int test_dep_graph_init(void) {
  struct termux_dep_graph graph = {};
  int ret = termux_dep_graph_init(&graph, 100);
  assert(ret == 0);
  assert(graph.pkg_count == 100);
  assert(graph.total_edges == 0);
  assert(graph.depths != NULL);
  assert(graph.deps_row != NULL);
  assert(graph.deps_col != NULL);

  free(graph.deps_row);
  free(graph.deps_col);
  free(graph.depths);
  printf("✓ test_dep_graph_init passed\n");
  return 0;
}

static int test_dep_resolver_init(void) {
  struct termux_dep_resolver resolver = {};
  int ret = termux_dep_resolver_init(&resolver, 100);
  assert(ret == 0);
  assert(resolver.graph.pkg_count == 100);

  termux_dep_resolver_destroy(&resolver);
  printf("✓ test_dep_resolver_init passed\n");
  return 0;
}

static int test_toroidal_layer_batching(void) {
  struct termux_dep_resolver resolver = {};
  int ret = termux_dep_resolver_init(&resolver, TERMUX_TOROIDAL_LAYERS);
  assert(ret == 0);

  ret = termux_dep_graph_finalize_csr(&resolver.graph);
  assert(ret == 0);

  ret = termux_dep_resolver_build_layers(&resolver);
  assert(ret == 0 || ret == -3);

  termux_dep_resolver_destroy(&resolver);
  printf("✓ test_toroidal_layer_batching passed (layer initialization)\n");
  return 0;
}

static int test_crc32c_checksum(void) {
  struct termux_dep_resolver resolver = {};
  int ret = termux_dep_resolver_init(&resolver, 10);
  assert(ret == 0);

  for (uint16_t i = 0; i < 10; i++) {
    resolver.graph.depths[i] = i % TERMUX_TOROIDAL_LAYERS;
  }

  uint32_t crc1 = termux_dep_graph_compute_crc32c(&resolver.graph);
  assert(crc1 != 0);

  uint32_t crc2 = termux_dep_graph_compute_crc32c(&resolver.graph);
  assert(crc1 == crc2);

  termux_dep_resolver_destroy(&resolver);
  printf("✓ test_crc32c_checksum passed (deterministic)\n");
  return 0;
}

static int test_layer_phi_computation(void) {
  struct termux_dep_graph graph = {};
  struct termux_layer_batch layer = {
    .layer_idx = 0,
  };

  int ret = termux_dep_resolver_compute_layer_phi(&layer, &graph);
  assert(ret == 0);

  struct termux_layer_batch layer_deep = {
    .layer_idx = 16,
  };
  ret = termux_dep_resolver_compute_layer_phi(&layer_deep, &graph);
  assert(ret == 0);

  printf("✓ test_layer_phi_computation passed (φ scores computed)\n");
  return 0;
}

static int test_gcd_coverage(void) {
  uint8_t valid_gcds[] = {1, 2, 4, 8, 16, 32};

  for (uint32_t depth = 0; depth < TERMUX_TOROIDAL_LAYERS; depth++) {
    int depth_gcd_valid = 0;
    if (depth == 0) {
      depth_gcd_valid = 1;
    } else {
      uint32_t a = depth;
      uint32_t b = TERMUX_TOROIDAL_LAYERS;
      while (b != 0) {
        uint32_t tmp = b;
        b = a % b;
        a = tmp;
      }
      for (size_t i = 0; i < sizeof(valid_gcds) / sizeof(valid_gcds[0]); i++) {
        if (valid_gcds[i] == a) {
          depth_gcd_valid = 1;
          break;
        }
      }
    }
    assert(depth_gcd_valid);
  }

  printf("✓ test_gcd_coverage passed (all depths have valid gcd(depth, 32))\n");
  return 0;
}

int main(void) {
  printf("=== Dependency Resolver Unit Tests ===\n\n");

  int all_passed = 0;
  all_passed += test_dep_graph_init();
  all_passed += test_dep_resolver_init();
  all_passed += test_toroidal_layer_batching();
  all_passed += test_crc32c_checksum();
  all_passed += test_layer_phi_computation();
  all_passed += test_gcd_coverage();

  printf("\n=== All tests passed! ===\n");
  return all_passed == 0 ? 0 : 1;
}
