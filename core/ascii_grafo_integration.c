#include "ascii_grafo.h"
#include "bitraf64_integration.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Integração ASCII-Grafo com BITRAF64
 *
 * Mapeia 2057 packages termux para células ASCII-Grafo
 * com topologia toroidal (10×10×10×6)
 */

typedef struct {
  ascii_grafo_graph_t grafo;
  uint32_t manifest_count;
} bitraf64_ascii_grafo_integration_t;

/* Instância global (para simplicidade) */
static bitraf64_ascii_grafo_integration_t g_integration = {0};

/*
 * Inicializar integração ASCII-Grafo + BITRAF64
 */
int bitraf64_ascii_grafo_init(uint32_t manifest_count) {
  if (manifest_count == 0 || manifest_count > 6000) return 0;

  /* Inicializar grafo */
  if (!ascii_grafo_graph_init(&g_integration.grafo, manifest_count)) {
    return 0;
  }

  g_integration.manifest_count = manifest_count;

  /* Inicializar células a partir de coordenadas BITRAF64 */
  for (uint32_t flat_idx = 0; flat_idx < manifest_count; flat_idx++) {
    /* Converter índice flat para coordenadas toroidais */
    uint32_t f = flat_idx / 1000;
    uint32_t remainder = flat_idx % 1000;
    uint32_t i = remainder / 100;
    remainder = remainder % 100;
    uint32_t j = remainder / 10;
    uint32_t k = remainder % 10;

    if (f >= 6 || i >= 10 || j >= 10 || k >= 10) {
      printf("Error: Invalid coordinates (f=%u, i=%u, j=%u, k=%u)\n", f, i, j, k);
      return 0;
    }

    ascii_grafo_cell_t *cell = &g_integration.grafo.cells[flat_idx];
    if (!ascii_grafo_cell_init(cell, i, j, k, f)) {
      printf("Error: Failed to initialize cell %u\n", flat_idx);
      return 0;
    }

    /* Renderizar sequência ASCII */
    ascii_grafo_render(cell);
  }

  /* Computar adjacências (vizinhos Moore) */
  if (!ascii_grafo_compute_neighbors(&g_integration.grafo)) {
    printf("Error: Failed to compute neighbors\n");
    return 0;
  }

  /* Validar integridade */
  if (!ascii_grafo_validate(&g_integration.grafo)) {
    printf("Error: Grafo validation failed\n");
    return 0;
  }

  return 1;
}

/*
 * Definir base¹ ativa para célula (por índice flat)
 */
int bitraf64_ascii_grafo_set_base1(uint32_t flat_idx, int active) {
  if (flat_idx >= g_integration.grafo.cell_count) return 0;

  ascii_grafo_cell_t *cell = &g_integration.grafo.cells[flat_idx];
  return ascii_grafo_set_base1(cell, active);
}

/*
 * Definir base² ativa para célula
 */
int bitraf64_ascii_grafo_set_base2(uint32_t flat_idx, int active) {
  if (flat_idx >= g_integration.grafo.cell_count) return 0;

  ascii_grafo_cell_t *cell = &g_integration.grafo.cells[flat_idx];
  return ascii_grafo_set_base2(cell, active);
}

/*
 * Definir bit central (compartilhado entre base¹ e base²)
 */
int bitraf64_ascii_grafo_set_central(uint32_t flat_idx, uint8_t value) {
  if (flat_idx >= g_integration.grafo.cell_count) return 0;

  ascii_grafo_cell_t *cell = &g_integration.grafo.cells[flat_idx];
  return ascii_grafo_set_central(cell, value);
}

/*
 * Obter célula ASCII-Grafo
 */
ascii_grafo_cell_t* bitraf64_ascii_grafo_get_cell(uint32_t flat_idx) {
  if (flat_idx >= g_integration.grafo.cell_count) return NULL;
  return &g_integration.grafo.cells[flat_idx];
}

/*
 * Obter vizinhos Moore de uma célula (até 8 vizinhos)
 */
int bitraf64_ascii_grafo_get_neighbors(
    uint32_t flat_idx,
    uint32_t *out_neighbors,
    size_t max_neighbors
) {
  if (flat_idx >= g_integration.grafo.cell_count) return 0;

  return ascii_grafo_get_neighbors(&g_integration.grafo, flat_idx, out_neighbors, max_neighbors);
}

/*
 * Computar estatísticas do grafo ASCII-Grafo
 */
int bitraf64_ascii_grafo_stats(ascii_grafo_stats_t *out_stats) {
  if (!out_stats) return 0;
  return ascii_grafo_compute_stats(&g_integration.grafo, out_stats);
}

/*
 * Gerar relatório do grafo
 */
int bitraf64_ascii_grafo_report(char *buffer, size_t buffer_size) {
  if (!buffer || buffer_size < 512) return 0;
  return ascii_grafo_report(&g_integration.grafo, buffer, buffer_size);
}

/*
 * Exportar grid ASCII visual
 */
int bitraf64_ascii_grafo_export_grid(
    char *buffer,
    size_t buffer_size,
    uint32_t start_idx,
    uint32_t grid_size
) {
  if (!buffer || buffer_size < 1024) return 0;
  return ascii_grafo_export_grid(&g_integration.grafo, buffer, buffer_size, start_idx, grid_size);
}

/*
 * Listar todas as célula com base¹ ativa
 */
int bitraf64_ascii_grafo_list_base1_active(
    uint32_t *out_indices,
    size_t max_count
) {
  if (!out_indices) return 0;

  size_t count = 0;
  for (uint32_t i = 0; i < g_integration.grafo.cell_count && count < max_count; i++) {
    if (g_integration.grafo.cells[i].base1_active) {
      out_indices[count++] = i;
    }
  }

  return (int)count;
}

/*
 * Listar todas as células com base² ativa
 */
int bitraf64_ascii_grafo_list_base2_active(
    uint32_t *out_indices,
    size_t max_count
) {
  if (!out_indices) return 0;

  size_t count = 0;
  for (uint32_t i = 0; i < g_integration.grafo.cell_count && count < max_count; i++) {
    if (g_integration.grafo.cells[i].base2_active) {
      out_indices[count++] = i;
    }
  }

  return (int)count;
}

/*
 * Listar todas as células com conflito (base¹ ∩ base² ≠ ∅)
 */
int bitraf64_ascii_grafo_list_conflicts(
    uint32_t *out_indices,
    size_t max_count
) {
  if (!out_indices) return 0;

  size_t count = 0;
  for (uint32_t i = 0; i < g_integration.grafo.cell_count && count < max_count; i++) {
    if (ascii_grafo_has_conflict(&g_integration.grafo.cells[i])) {
      out_indices[count++] = i;
    }
  }

  return (int)count;
}

/*
 * Computar coerência combinada (ASCII-Grafo + BITRAF64)
 *
 * φ_grafo = (vizinhos_ativos / 8) × (base1 | base2) × (1 - conflito_rate)
 */
float bitraf64_ascii_grafo_coherence(uint32_t flat_idx) {
  if (flat_idx >= g_integration.grafo.cell_count) return 0.0f;

  ascii_grafo_cell_t *cell = &g_integration.grafo.cells[flat_idx];

  /* Fator de vizinhança (0-1) */
  int neighbors = ascii_moore_population(&cell->moore);
  float neighbor_factor = (float)neighbors / 8.0f;

  /* Fator de base (0 ou 1) */
  int base_active = cell->base1_active || cell->base2_active;

  /* Fator de conflito (0-1, penalidade se base¹ ∩ base² ≠ ∅) */
  float conflict_penalty = ascii_grafo_has_conflict(cell) ? 0.5f : 1.0f;

  /* Coerência final */
  float coherence = neighbor_factor * base_active * conflict_penalty;

  return coherence;
}

/*
 * Validar integridade do grafo
 */
int bitraf64_ascii_grafo_validate(void) {
  return ascii_grafo_validate(&g_integration.grafo);
}

/*
 * Liberar recursos
 */
int bitraf64_ascii_grafo_free(void) {
  return ascii_grafo_graph_free(&g_integration.grafo);
}

/*
 * Imprimir célula detalhada (debug)
 */
void bitraf64_ascii_grafo_print_cell(uint32_t flat_idx) {
  if (flat_idx >= g_integration.grafo.cell_count) return;

  ascii_grafo_cell_t *cell = &g_integration.grafo.cells[flat_idx];
  ascii_grafo_print_cell(cell);

  printf("  Coherence φ: %.4f\n", bitraf64_ascii_grafo_coherence(flat_idx));
}

/*
 * Estatísticas por camada toroidal
 */
typedef struct {
  uint32_t layer;
  uint32_t cell_count;
  uint32_t base1_count;
  uint32_t base2_count;
  uint32_t conflict_count;
  float avg_moore_density;
  float avg_coherence;
} layer_stats_t;

int bitraf64_ascii_grafo_layer_stats(
    uint32_t layer,
    layer_stats_t *out_stats
) {
  if (layer >= 32 || !out_stats) return 0;

  memset(out_stats, 0, sizeof(*out_stats));
  out_stats->layer = layer;

  float total_coherence = 0.0f;
  int cell_count = 0;

  for (uint32_t i = 0; i < g_integration.grafo.cell_count; i++) {
    ascii_grafo_cell_t *cell = &g_integration.grafo.cells[i];

    if (cell->layer != layer) continue;

    out_stats->cell_count++;
    if (cell->base1_active) out_stats->base1_count++;
    if (cell->base2_active) out_stats->base2_count++;
    if (ascii_grafo_has_conflict(cell)) out_stats->conflict_count++;

    int pop = ascii_moore_population(&cell->moore);
    out_stats->avg_moore_density += (float)pop / 8.0f;

    total_coherence += bitraf64_ascii_grafo_coherence(i);
    cell_count++;
  }

  if (cell_count > 0) {
    out_stats->avg_moore_density /= cell_count;
    out_stats->avg_coherence = total_coherence / cell_count;
  }

  return out_stats->cell_count > 0 ? 1 : 0;
}

/*
 * Gerar relatório detalhado por camada
 */
int bitraf64_ascii_grafo_layer_report(
    uint32_t layer,
    char *buffer,
    size_t buffer_size
) {
  if (layer >= 32 || !buffer || buffer_size < 512) return 0;

  layer_stats_t stats;
  if (!bitraf64_ascii_grafo_layer_stats(layer, &stats)) {
    return 0;
  }

  int offset = 0;

  offset += snprintf(buffer + offset, buffer_size - offset,
                     "Layer %u Statistics\n", layer);
  offset += snprintf(buffer + offset, buffer_size - offset,
                     "  Cells: %u\n", stats.cell_count);
  offset += snprintf(buffer + offset, buffer_size - offset,
                     "  Base¹: %u (%.1f%%)\n", stats.base1_count,
                     100.0f * stats.base1_count / stats.cell_count);
  offset += snprintf(buffer + offset, buffer_size - offset,
                     "  Base²: %u (%.1f%%)\n", stats.base2_count,
                     100.0f * stats.base2_count / stats.cell_count);
  offset += snprintf(buffer + offset, buffer_size - offset,
                     "  Conflicts: %u (%.1f%%)\n", stats.conflict_count,
                     100.0f * stats.conflict_count / stats.cell_count);
  offset += snprintf(buffer + offset, buffer_size - offset,
                     "  Avg Moore Density: %.2f\n", stats.avg_moore_density);
  offset += snprintf(buffer + offset, buffer_size - offset,
                     "  Avg Coherence φ: %.4f\n", stats.avg_coherence);

  return offset;
}
