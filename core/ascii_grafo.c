#include "ascii_grafo.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Símbolos ASCII para Moore neighborhood */
static const char MOORE_SYMBOLS[] = "\\|/-*-/|\\";
static const char EMPTY_SYMBOL = '.';
static const char ACTIVE_SYMBOL = '*';
static const char NULL_SYMBOL = 'O';

/* Vetores de direção para Moore neighborhood */
static const int MOORE_DX[] = {-1,  0,  1, -1, 0, 1, -1, 0, 1};
static const int MOORE_DY[] = {-1, -1, -1,  0, 0, 0,  1, 1, 1};

/* ============================================================================
 * Operações em Célula Moore Elementar
 * ============================================================================ */

ascii_moore_cell_t ascii_moore_init(void) {
  ascii_moore_cell_t cell = {0};
  return cell;
}

int ascii_moore_set_bit(ascii_moore_cell_t *cell, moore_direction_t dir, int value) {
  if (!cell || dir >= MOORE_COUNT) return 0;

  if (value) {
    cell->neighborhood |= (1 << dir);
  } else {
    cell->neighborhood &= ~(1 << dir);
  }

  return 1;
}

int ascii_moore_get_bit(const ascii_moore_cell_t *cell, moore_direction_t dir) {
  if (!cell || dir >= MOORE_COUNT) return 0;
  return (cell->neighborhood >> dir) & 1;
}

int ascii_moore_orthogonal_sum(const ascii_moore_cell_t *cell) {
  if (!cell) return 0;

  int sum = 0;
  sum += (cell->neighborhood >> MOORE_N) & 1;   /* Norte */
  sum += (cell->neighborhood >> MOORE_S) & 1;   /* Sul */
  sum += (cell->neighborhood >> MOORE_E) & 1;   /* Leste */
  sum += (cell->neighborhood >> MOORE_W) & 1;   /* Oeste */

  return sum;
}

int ascii_moore_diagonal_sum(const ascii_moore_cell_t *cell) {
  if (!cell) return 0;

  int sum = 0;
  sum += (cell->neighborhood >> MOORE_NW) & 1;  /* Noroeste */
  sum += (cell->neighborhood >> MOORE_NE) & 1;  /* Nordeste */
  sum += (cell->neighborhood >> MOORE_SW) & 1;  /* Sudoeste */
  sum += (cell->neighborhood >> MOORE_SE) & 1;  /* Sudeste */

  return sum;
}

int ascii_moore_population(const ascii_moore_cell_t *cell) {
  if (!cell) return 0;

  uint16_t neighborhood_without_central = cell->neighborhood & 0x1EF;
  return __builtin_popcount(neighborhood_without_central);
}

/* ============================================================================
 * Operações em Célula ASCII-Grafo Completa
 * ============================================================================ */

int ascii_grafo_cell_init(
    ascii_grafo_cell_t *cell,
    uint32_t i, uint32_t j, uint32_t k, uint32_t f
) {
  if (!cell || i >= 10 || j >= 10 || k >= 10 || f >= 6) return 0;

  memset(cell, 0, sizeof(*cell));

  cell->i = i;
  cell->j = j;
  cell->k = k;
  cell->f = f;
  cell->layer = (i + j + k) % 32;
  cell->flat_idx = f * 1000 + i * 100 + j * 10 + k;

  cell->moore = ascii_moore_init();
  cell->central_bit = 0;  /* Inicia vazio */
  cell->base1_active = 0;
  cell->base2_active = 0;

  memset(cell->ascii_sequence, 0, sizeof(cell->ascii_sequence));

  return 1;
}

int ascii_grafo_set_base1(ascii_grafo_cell_t *cell, int active) {
  if (!cell) return 0;

  cell->base1_active = active ? 1 : 0;

  /* Atualizar bit central se necessário */
  if (cell->base1_active && cell->base2_active) {
    cell->central_bit = 1;  /* Conflito/compartilhamento */
  } else if (!cell->base1_active && !cell->base2_active) {
    cell->central_bit = 0;  /* Vazio */
  }

  return 1;
}

int ascii_grafo_set_base2(ascii_grafo_cell_t *cell, int active) {
  if (!cell) return 0;

  cell->base2_active = active ? 1 : 0;

  /* Atualizar bit central se necessário */
  if (cell->base1_active && cell->base2_active) {
    cell->central_bit = 1;  /* Conflito/compartilhamento */
  } else if (!cell->base1_active && !cell->base2_active) {
    cell->central_bit = 0;  /* Vazio */
  }

  return 1;
}

int ascii_grafo_set_central(ascii_grafo_cell_t *cell, uint8_t value) {
  if (!cell) return 0;

  cell->central_bit = value;
  return 1;
}

uint8_t ascii_grafo_get_central(const ascii_grafo_cell_t *cell) {
  if (!cell) return 0;
  return cell->central_bit;
}

int ascii_grafo_has_conflict(const ascii_grafo_cell_t *cell) {
  if (!cell) return 0;
  return (cell->base1_active && cell->base2_active) ? 1 : 0;
}

/* ============================================================================
 * Renderização ASCII (33 caracteres)
 * ============================================================================ */

int ascii_grafo_render(ascii_grafo_cell_t *cell) {
  if (!cell) return 0;

  char buffer[34];
  int pos = 0;

  /* Construir vizinhança Moore (9 caracteres) */
  for (int dir = 0; dir < MOORE_COUNT; dir++) {
    int bit = ascii_moore_get_bit(&cell->moore, (moore_direction_t)dir);

    if (dir == MOORE_C) {
      /* Bit central: usar símbolo especial */
      if (cell->central_bit == 0) {
        buffer[pos++] = EMPTY_SYMBOL;
      } else if (cell->central_bit == 1) {
        buffer[pos++] = ACTIVE_SYMBOL;
      } else {
        buffer[pos++] = NULL_SYMBOL;
      }
    } else {
      buffer[pos++] = bit ? MOORE_SYMBOLS[dir] : EMPTY_SYMBOL;
    }
  }

  /* Base indicators (2 caracteres) */
  buffer[pos++] = cell->base1_active ? '1' : '.';
  buffer[pos++] = cell->base2_active ? '2' : '.';

  /* Coordenadas BITRAF64 (9 caracteres: i,j,k,f em hex) */
  pos += snprintf(buffer + pos, 34 - pos, "%01x%01x%01x%01x",
                  cell->i, cell->j, cell->k, cell->f);

  /* Layer (2 caracteres em hex) */
  pos += snprintf(buffer + pos, 34 - pos, "%02x", cell->layer);

  /* Estatísticas Moore (10 caracteres) */
  int orth = ascii_moore_orthogonal_sum(&cell->moore);
  int diag = ascii_moore_diagonal_sum(&cell->moore);
  int pop = ascii_moore_population(&cell->moore);

  pos += snprintf(buffer + pos, 34 - pos, "%d%d%d",
                  orth, diag, pop);

  /* Preencher resto com espaços até 33 caracteres */
  while (pos < 33) {
    buffer[pos++] = ' ';
  }

  buffer[33] = '\0';
  memcpy(cell->ascii_sequence, buffer, 34);

  /* Computar CRC32c simples (xor hash) */
  cell->ascii_hash = 0;
  for (int i = 0; i < 33; i++) {
    cell->ascii_hash ^= ((uint32_t)buffer[i] << (i % 4));
  }

  return 1;
}

const char* ascii_grafo_get_sequence(const ascii_grafo_cell_t *cell) {
  if (!cell) return "";
  return cell->ascii_sequence;
}

/* ============================================================================
 * Visualização ASCII Art
 * ============================================================================ */

void ascii_grafo_print_moore(const ascii_moore_cell_t *cell) {
  if (!cell) return;

  printf("┌─────┐\n");
  printf("│%c %c %c│\n",
         ascii_moore_get_bit(cell, MOORE_NW) ? MOORE_SYMBOLS[MOORE_NW] : EMPTY_SYMBOL,
         ascii_moore_get_bit(cell, MOORE_N) ? MOORE_SYMBOLS[MOORE_N] : EMPTY_SYMBOL,
         ascii_moore_get_bit(cell, MOORE_NE) ? MOORE_SYMBOLS[MOORE_NE] : EMPTY_SYMBOL);
  printf("│%c⊙%c│\n",
         ascii_moore_get_bit(cell, MOORE_W) ? MOORE_SYMBOLS[MOORE_W] : EMPTY_SYMBOL,
         ascii_moore_get_bit(cell, MOORE_E) ? MOORE_SYMBOLS[MOORE_E] : EMPTY_SYMBOL);
  printf("│%c %c %c│\n",
         ascii_moore_get_bit(cell, MOORE_SW) ? MOORE_SYMBOLS[MOORE_SW] : EMPTY_SYMBOL,
         ascii_moore_get_bit(cell, MOORE_S) ? MOORE_SYMBOLS[MOORE_S] : EMPTY_SYMBOL,
         ascii_moore_get_bit(cell, MOORE_SE) ? MOORE_SYMBOLS[MOORE_SE] : EMPTY_SYMBOL);
  printf("└─────┘\n");
}

void ascii_grafo_print_cell(const ascii_grafo_cell_t *cell) {
  if (!cell) return;

  printf("ASCII-Grafo Cell [%u,%u,%u,%u]\n", cell->i, cell->j, cell->k, cell->f);
  printf("  Layer: %u\n", cell->layer);
  printf("  Base¹: %s, Base²: %s\n",
         cell->base1_active ? "active" : "inactive",
         cell->base2_active ? "active" : "inactive");
  printf("  Central Bit: %u\n", cell->central_bit);
  printf("  Conflict: %s\n", ascii_grafo_has_conflict(cell) ? "YES" : "NO");
  printf("  Moore Neighborhood:\n");

  printf("    Ortho: %u, Diag: %u, Pop: %u\n",
         ascii_moore_orthogonal_sum(&cell->moore),
         ascii_moore_diagonal_sum(&cell->moore),
         ascii_moore_population(&cell->moore));

  printf("  ASCII Sequence: %s\n", cell->ascii_sequence);

  printf("  Visual:\n");
  for (int i = 0; i < 4; i++) printf("    ");
  ascii_grafo_print_moore(&cell->moore);
}

/* ============================================================================
 * Operações de Grafo Estruturado
 * ============================================================================ */

int ascii_grafo_graph_init(
    ascii_grafo_graph_t *graph,
    uint32_t cell_count
) {
  if (!graph || cell_count == 0) return 0;

  graph->cells = (ascii_grafo_cell_t *)malloc(cell_count * sizeof(ascii_grafo_cell_t));
  if (!graph->cells) return 0;

  graph->neighbor_lists = (uint32_t *)malloc(cell_count * 9 * sizeof(uint32_t));
  graph->neighbor_counts = (uint32_t *)malloc(cell_count * sizeof(uint32_t));

  if (!graph->neighbor_lists || !graph->neighbor_counts) {
    free(graph->cells);
    free(graph->neighbor_lists);
    free(graph->neighbor_counts);
    return 0;
  }

  memset(graph->cells, 0, cell_count * sizeof(ascii_grafo_cell_t));
  memset(graph->neighbor_lists, 0, cell_count * 9 * sizeof(uint32_t));
  memset(graph->neighbor_counts, 0, cell_count * sizeof(uint32_t));

  graph->cell_count = cell_count;
  graph->total_moore_bits = 0;
  graph->total_central_bits = 0;
  graph->conflicts = 0;

  return 1;
}

int ascii_grafo_graph_free(ascii_grafo_graph_t *graph) {
  if (!graph) return 0;

  if (graph->cells) free(graph->cells);
  if (graph->neighbor_lists) free(graph->neighbor_lists);
  if (graph->neighbor_counts) free(graph->neighbor_counts);

  memset(graph, 0, sizeof(*graph));
  return 1;
}

int ascii_grafo_compute_neighbors(ascii_grafo_graph_t *graph) {
  if (!graph || !graph->cells || !graph->neighbor_lists) return 0;

  /* Para células em grid BITRAF64 (10×10×10×6) */
  for (uint32_t idx = 0; idx < graph->cell_count; idx++) {
    ascii_grafo_cell_t *cell = &graph->cells[idx];

    int neighbor_count = 0;
    uint32_t *neighbors = &graph->neighbor_lists[idx * 9];

    /* Iterar sobre direções Moore */
    for (int dir = 0; dir < MOORE_COUNT; dir++) {
      if (dir == MOORE_C) continue;  /* Pular central */

      int di = MOORE_DY[dir];
      int dj = MOORE_DX[dir];

      int ni = (cell->i + di + 10) % 10;  /* Toroidal wrap */
      int nj = (cell->j + dj + 10) % 10;
      int nk = cell->k;                   /* Sem wrap em k para este exemplo */
      int nf = cell->f;

      if (nk >= 10) continue;

      uint32_t neighbor_flat = nf * 1000 + ni * 100 + nj * 10 + nk;

      if (neighbor_flat < graph->cell_count) {
        neighbors[neighbor_count++] = neighbor_flat;
      }
    }

    graph->neighbor_counts[idx] = neighbor_count;
  }

  return 1;
}

int ascii_grafo_get_neighbors(
    const ascii_grafo_graph_t *graph,
    uint32_t cell_idx,
    uint32_t *out_neighbors,
    size_t max_neighbors
) {
  if (!graph || !out_neighbors || cell_idx >= graph->cell_count) return 0;

  uint32_t count = graph->neighbor_counts[cell_idx];
  if (count > max_neighbors) count = max_neighbors;

  memcpy(out_neighbors, &graph->neighbor_lists[cell_idx * 9], count * sizeof(uint32_t));

  return count;
}

int ascii_grafo_validate(const ascii_grafo_graph_t *graph) {
  if (!graph || !graph->cells) return 0;

  for (uint32_t i = 0; i < graph->cell_count; i++) {
    ascii_grafo_cell_t *cell = &graph->cells[i];

    /* Validar coordenadas */
    if (cell->i >= 10 || cell->j >= 10 || cell->k >= 10 || cell->f >= 6) {
      return 0;
    }

    /* Validar layer */
    if (cell->layer != (cell->i + cell->j + cell->k) % 32) {
      return 0;
    }

    /* Validar flat_idx */
    if (cell->flat_idx != cell->f * 1000 + cell->i * 100 + cell->j * 10 + cell->k) {
      return 0;
    }

    /* Validar sequência ASCII */
    if (strlen(cell->ascii_sequence) != 33) {
      return 0;
    }
  }

  return 1;
}

int ascii_grafo_compute_stats(
    const ascii_grafo_graph_t *graph,
    ascii_grafo_stats_t *out_stats
) {
  if (!graph || !graph->cells || !out_stats) return 0;

  memset(out_stats, 0, sizeof(*out_stats));

  uint32_t total_bits = 0;
  uint32_t orthogonal_bits = 0;
  uint32_t diagonal_bits = 0;
  uint32_t central_bits = 0;
  uint32_t conflicts = 0;
  uint32_t base1_count = 0;
  uint32_t base2_count = 0;

  for (uint32_t i = 0; i < graph->cell_count; i++) {
    ascii_grafo_cell_t *cell = &graph->cells[i];

    orthogonal_bits += ascii_moore_orthogonal_sum(&cell->moore);
    diagonal_bits += ascii_moore_diagonal_sum(&cell->moore);
    total_bits += ascii_moore_population(&cell->moore);

    if (cell->central_bit == 1) {
      central_bits++;
    }

    if (ascii_grafo_has_conflict(cell)) {
      conflicts++;
    }

    if (cell->base1_active) base1_count++;
    if (cell->base2_active) base2_count++;
  }

  out_stats->total_bits = total_bits;
  out_stats->orthogonal_bits = orthogonal_bits;
  out_stats->diagonal_bits = diagonal_bits;
  out_stats->central_bits = central_bits;
  out_stats->conflicts = conflicts;

  out_stats->avg_neighborhood_density = (float)total_bits / graph->cell_count;
  out_stats->base1_coverage = (float)base1_count / graph->cell_count;
  out_stats->base2_coverage = (float)base2_count / graph->cell_count;
  out_stats->conflict_ratio = (float)conflicts / graph->cell_count;

  return 1;
}

int ascii_grafo_report(
    const ascii_grafo_graph_t *graph,
    char *buffer,
    size_t buffer_size
) {
  if (!graph || !buffer || buffer_size < 512) return 0;

  ascii_grafo_stats_t stats;
  ascii_grafo_compute_stats(graph, &stats);

  int offset = 0;

  offset += snprintf(buffer + offset, buffer_size - offset,
                     "=== ASCII-Grafo Toroidal Report ===\n\n");

  offset += snprintf(buffer + offset, buffer_size - offset,
                     "Total Cells: %u\n", graph->cell_count);

  offset += snprintf(buffer + offset, buffer_size - offset,
                     "Moore Bits: %u total (%u ortho, %u diag)\n",
                     stats.total_bits, stats.orthogonal_bits, stats.diagonal_bits);

  offset += snprintf(buffer + offset, buffer_size - offset,
                     "Central Bits: %u (%.2f%%)\n",
                     stats.central_bits, stats.central_bits * 100.0f / graph->cell_count);

  offset += snprintf(buffer + offset, buffer_size - offset,
                     "Base¹ Coverage: %.2f%%\n", stats.base1_coverage * 100.0f);

  offset += snprintf(buffer + offset, buffer_size - offset,
                     "Base² Coverage: %.2f%%\n", stats.base2_coverage * 100.0f);

  offset += snprintf(buffer + offset, buffer_size - offset,
                     "Conflicts (Base¹ ∩ Base²): %u (%.2f%%)\n",
                     stats.conflicts, stats.conflict_ratio * 100.0f);

  offset += snprintf(buffer + offset, buffer_size - offset,
                     "Avg Moore Density: %.2f bits/cell\n",
                     stats.avg_neighborhood_density);

  return offset;
}

int ascii_grafo_export_grid(
    const ascii_grafo_graph_t *graph,
    char *buffer,
    size_t buffer_size,
    uint32_t start_idx,
    uint32_t grid_size
) {
  if (!graph || !buffer || buffer_size < 1024 || start_idx >= graph->cell_count) return 0;

  if (grid_size != 3 && grid_size != 5 && grid_size != 7 && grid_size != 9) return 0;

  int offset = 0;

  offset += snprintf(buffer + offset, buffer_size - offset,
                     "ASCII-Grafo Grid (%u×%u starting at cell %u)\n\n",
                     grid_size, grid_size, start_idx);

  /* Renderizar grid */
  for (uint32_t row = 0; row < grid_size; row++) {
    for (uint32_t col = 0; col < grid_size; col++) {
      uint32_t idx = start_idx + row * grid_size + col;

      if (idx >= graph->cell_count) {
        offset += snprintf(buffer + offset, buffer_size - offset, "[     ] ");
        continue;
      }

      ascii_grafo_cell_t *cell = &graph->cells[idx];

      char central_char = cell->central_bit == 1 ? '*' :
                         cell->central_bit == 0 ? '.' : 'O';

      offset += snprintf(buffer + offset, buffer_size - offset,
                        "[%c%u%u] ",
                        central_char,
                        cell->base1_active ? 1 : 0,
                        cell->base2_active ? 1 : 0);
    }

    offset += snprintf(buffer + offset, buffer_size - offset, "\n");
  }

  return offset;
}
