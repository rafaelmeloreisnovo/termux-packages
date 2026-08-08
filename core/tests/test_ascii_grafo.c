#include "../ascii_grafo.h"
#include "../ascii_grafo_integration.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int test_moore_basic(void) {
  printf("Test 1: Moore Cell Basic Operations\n");

  ascii_moore_cell_t cell = ascii_moore_init();

  /* Verificar inicialização */
  if (ascii_moore_population(&cell) != 0) {
    printf("  ✗ Initial population should be 0\n");
    return 0;
  }

  /* Definir vizinhos */
  ascii_moore_set_bit(&cell, MOORE_N, 1);
  ascii_moore_set_bit(&cell, MOORE_S, 1);
  ascii_moore_set_bit(&cell, MOORE_E, 1);
  ascii_moore_set_bit(&cell, MOORE_W, 1);

  if (ascii_moore_orthogonal_sum(&cell) != 4) {
    printf("  ✗ Orthogonal sum should be 4\n");
    return 0;
  }

  /* Definir diagonais */
  ascii_moore_set_bit(&cell, MOORE_NE, 1);
  ascii_moore_set_bit(&cell, MOORE_SE, 1);

  if (ascii_moore_diagonal_sum(&cell) != 2) {
    printf("  ✗ Diagonal sum should be 2\n");
    return 0;
  }

  printf("  ✓ Moore cell operations working\n");
  return 1;
}

int test_ascii_grafo_cell(void) {
  printf("Test 2: ASCII-Grafo Cell Creation\n");

  ascii_grafo_cell_t cell;

  if (!ascii_grafo_cell_init(&cell, 3, 4, 5, 2)) {
    printf("  ✗ Cell initialization failed\n");
    return 0;
  }

  if (cell.i != 3 || cell.j != 4 || cell.k != 5 || cell.f != 2) {
    printf("  ✗ Cell coordinates incorrect\n");
    return 0;
  }

  if (cell.layer != (3 + 4 + 5) % 32) {
    printf("  ✗ Layer computation incorrect\n");
    return 0;
  }

  if (cell.flat_idx != 2 * 1000 + 3 * 100 + 4 * 10 + 5) {
    printf("  ✗ Flat index incorrect\n");
    return 0;
  }

  printf("  ✓ ASCII-Grafo cell created successfully\n");
  printf("    Coordinates: (%u,%u,%u,%u)\n", cell.i, cell.j, cell.k, cell.f);
  printf("    Layer: %u\n", cell.layer);
  printf("    Flat Index: %u\n", cell.flat_idx);

  return 1;
}

int test_base_operations(void) {
  printf("Test 3: Base¹/Base² Operations\n");

  ascii_grafo_cell_t cell;
  ascii_grafo_cell_init(&cell, 0, 0, 0, 0);

  /* Ativar base¹ */
  if (!ascii_grafo_set_base1(&cell, 1)) {
    printf("  ✗ Failed to set base1\n");
    return 0;
  }

  if (!cell.base1_active) {
    printf("  ✗ Base1 not active\n");
    return 0;
  }

  /* Ativar base² */
  if (!ascii_grafo_set_base2(&cell, 1)) {
    printf("  ✗ Failed to set base2\n");
    return 0;
  }

  /* Verificar conflito */
  if (!ascii_grafo_has_conflict(&cell)) {
    printf("  ✗ Conflict not detected\n");
    return 0;
  }

  /* Desativar base² */
  ascii_grafo_set_base2(&cell, 0);

  if (ascii_grafo_has_conflict(&cell)) {
    printf("  ✗ Conflict should not exist after deactivating base2\n");
    return 0;
  }

  printf("  ✓ Base operations working\n");
  return 1;
}

int test_central_bit(void) {
  printf("Test 4: Central Bit Sharing\n");

  ascii_grafo_cell_t cell;
  ascii_grafo_cell_init(&cell, 1, 2, 3, 1);

  /* Inicialmente vazio */
  if (ascii_grafo_get_central(&cell) != 0) {
    printf("  ✗ Central bit should be 0 initially\n");
    return 0;
  }

  /* Definir para 1 (compartilhado) */
  ascii_grafo_set_central(&cell, 1);

  if (ascii_grafo_get_central(&cell) != 1) {
    printf("  ✗ Central bit should be 1\n");
    return 0;
  }

  printf("  ✓ Central bit operations working\n");
  return 1;
}

int test_ascii_render(void) {
  printf("Test 5: ASCII Sequence Rendering\n");

  ascii_grafo_cell_t cell;
  ascii_grafo_cell_init(&cell, 5, 6, 7, 3);

  /* Ativar alguns vizinhos */
  ascii_moore_set_bit(&cell.moore, MOORE_N, 1);
  ascii_moore_set_bit(&cell.moore, MOORE_E, 1);
  ascii_moore_set_bit(&cell.moore, MOORE_S, 1);

  /* Ativar bases */
  ascii_grafo_set_base1(&cell, 1);
  ascii_grafo_set_base2(&cell, 1);

  /* Renderizar */
  if (!ascii_grafo_render(&cell)) {
    printf("  ✗ Render failed\n");
    return 0;
  }

  if (strlen(cell.ascii_sequence) != 33) {
    printf("  ✗ ASCII sequence length should be 33, got %zu\n", strlen(cell.ascii_sequence));
    return 0;
  }

  printf("  ✓ ASCII rendering working\n");
  printf("    Sequence: %s\n", cell.ascii_sequence);

  return 1;
}

int test_grafo_graph(void) {
  printf("Test 6: ASCII-Grafo Graph (100 cells)\n");

  ascii_grafo_graph_t graph;

  if (!ascii_grafo_graph_init(&graph, 100)) {
    printf("  ✗ Graph initialization failed\n");
    return 0;
  }

  /* Inicializar 100 células (10×10 grid) */
  for (uint32_t i = 0; i < 100; i++) {
    uint32_t row = i / 10;
    uint32_t col = i % 10;

    ascii_grafo_cell_init(&graph.cells[i], row, col, 0, 0);
    ascii_grafo_render(&graph.cells[i]);
  }

  /* Computar vizinhos */
  if (!ascii_grafo_compute_neighbors(&graph)) {
    printf("  ✗ Neighbor computation failed\n");
    ascii_grafo_graph_free(&graph);
    return 0;
  }

  /* Validar */
  if (!ascii_grafo_validate(&graph)) {
    printf("  ✗ Graph validation failed\n");
    ascii_grafo_graph_free(&graph);
    return 0;
  }

  printf("  ✓ Graph created and validated\n");
  printf("    Cells: %u\n", graph.cell_count);

  ascii_grafo_graph_free(&graph);
  return 1;
}

int test_integration_init(void) {
  printf("Test 7: Integration Initialization (2057 packages)\n");

  /* Inicializar para 2057 packages termux */
  if (!bitraf64_ascii_grafo_init(2057)) {
    printf("  ✗ Integration initialization failed\n");
    return 0;
  }

  printf("  ✓ Integration initialized for 2057 packages\n");

  return 1;
}

int test_integration_operations(void) {
  printf("Test 8: Integration Operations\n");

  /* Verificar se inicializado */
  ascii_grafo_cell_t *cell = bitraf64_ascii_grafo_get_cell(0);
  if (!cell) {
    printf("  ✗ Failed to get cell 0\n");
    return 0;
  }

  /* Ativar base¹ em células selecionadas */
  bitraf64_ascii_grafo_set_base1(0, 1);
  bitraf64_ascii_grafo_set_base1(10, 1);
  bitraf64_ascii_grafo_set_base1(100, 1);

  /* Ativar base² em células diferentes */
  bitraf64_ascii_grafo_set_base2(5, 1);
  bitraf64_ascii_grafo_set_base2(15, 1);

  /* Ativar bit central */
  bitraf64_ascii_grafo_set_central(0, 1);

  /* Computar coerência */
  float coherence = bitraf64_ascii_grafo_coherence(0);
  printf("  Coherence φ(0): %.4f\n", coherence);

  /* Obter vizinhos */
  uint32_t neighbors[8];
  int neighbor_count = bitraf64_ascii_grafo_get_neighbors(0, neighbors, 8);
  printf("  Cell 0 neighbors: %d\n", neighbor_count);

  printf("  ✓ Integration operations working\n");

  return 1;
}

int test_statistics(void) {
  printf("Test 9: Statistics Computation\n");

  ascii_grafo_stats_t stats;

  if (!bitraf64_ascii_grafo_stats(&stats)) {
    printf("  ✗ Statistics computation failed\n");
    return 0;
  }

  printf("  ✓ Statistics computed\n");
  printf("    Total Moore bits: %u\n", stats.total_bits);
  printf("    Orthogonal bits: %u\n", stats.orthogonal_bits);
  printf("    Diagonal bits: %u\n", stats.diagonal_bits);
  printf("    Central bits: %u\n", stats.central_bits);
  printf("    Conflicts: %u\n", stats.conflicts);
  printf("    Avg Moore density: %.2f\n", stats.avg_neighborhood_density);
  printf("    Base¹ coverage: %.2f%%\n", stats.base1_coverage * 100.0f);
  printf("    Base² coverage: %.2f%%\n", stats.base2_coverage * 100.0f);
  printf("    Conflict ratio: %.2f%%\n", stats.conflict_ratio * 100.0f);

  return 1;
}

int test_report(void) {
  printf("Test 10: Report Generation\n");

  char buffer[2048];

  if (!bitraf64_ascii_grafo_report(buffer, sizeof(buffer))) {
    printf("  ✗ Report generation failed\n");
    return 0;
  }

  printf("  ✓ Report generated\n");
  printf("%s\n", buffer);

  return 1;
}

int test_layer_stats(void) {
  printf("Test 11: Layer Statistics\n");

  /* Computar stats para layer 0 */
  bitraf64_layer_stats_t layer_stats;

  if (!bitraf64_ascii_grafo_layer_stats(0, &layer_stats)) {
    printf("  ✗ Layer stats computation failed\n");
    return 0;
  }

  printf("  ✓ Layer stats computed\n");
  printf("    Layer %u: %u cells\n", layer_stats.layer, layer_stats.cell_count);
  printf("    Base¹: %u cells\n", layer_stats.base1_count);
  printf("    Base²: %u cells\n", layer_stats.base2_count);
  printf("    Conflicts: %u\n", layer_stats.conflict_count);
  printf("    Avg coherence φ: %.4f\n", layer_stats.avg_coherence);

  return 1;
}

int test_validation(void) {
  printf("Test 12: Graph Validation\n");

  if (!bitraf64_ascii_grafo_validate()) {
    printf("  ✗ Validation failed\n");
    return 0;
  }

  printf("  ✓ Graph validation passed\n");

  return 1;
}

int main(void) {
  printf("=== ASCII-Grafo Toroidal Test Suite ===\n\n");

  int passed = 0;
  int total = 12;

  if (test_moore_basic()) passed++;
  printf("\n");

  if (test_ascii_grafo_cell()) passed++;
  printf("\n");

  if (test_base_operations()) passed++;
  printf("\n");

  if (test_central_bit()) passed++;
  printf("\n");

  if (test_ascii_render()) passed++;
  printf("\n");

  if (test_grafo_graph()) passed++;
  printf("\n");

  if (test_integration_init()) passed++;
  printf("\n");

  if (test_integration_operations()) passed++;
  printf("\n");

  if (test_statistics()) passed++;
  printf("\n");

  if (test_report()) passed++;
  printf("\n");

  if (test_layer_stats()) passed++;
  printf("\n");

  if (test_validation()) passed++;
  printf("\n");

  printf("=== Summary ===\n");
  printf("Passed: %d/%d tests\n", passed, total);

  bitraf64_ascii_grafo_free();

  return (passed == total) ? 0 : 1;
}
