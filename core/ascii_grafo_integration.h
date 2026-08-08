#ifndef TERMUX_ASCII_GRAFO_INTEGRATION_H
#define TERMUX_ASCII_GRAFO_INTEGRATION_H

#include "ascii_grafo.h"
#include <stdint.h>

/*
 * Integração ASCII-Grafo com BITRAF64
 *
 * Fornece API simplificada para trabalhar com ASCII-Grafo
 * no contexto do sistema BITRAF64 termux-packages
 */

/* ============================================================================
 * Inicialização e Limpeza
 * ============================================================================ */

int bitraf64_ascii_grafo_init(uint32_t manifest_count);
int bitraf64_ascii_grafo_free(void);

/* ============================================================================
 * Operações em Células Individuais
 * ============================================================================ */

int bitraf64_ascii_grafo_set_base1(uint32_t flat_idx, int active);
int bitraf64_ascii_grafo_set_base2(uint32_t flat_idx, int active);
int bitraf64_ascii_grafo_set_central(uint32_t flat_idx, uint8_t value);

ascii_grafo_cell_t* bitraf64_ascii_grafo_get_cell(uint32_t flat_idx);

int bitraf64_ascii_grafo_get_neighbors(
    uint32_t flat_idx,
    uint32_t *out_neighbors,
    size_t max_neighbors
);

float bitraf64_ascii_grafo_coherence(uint32_t flat_idx);

void bitraf64_ascii_grafo_print_cell(uint32_t flat_idx);

/* ============================================================================
 * Operações em Grafo Completo
 * ============================================================================ */

int bitraf64_ascii_grafo_stats(ascii_grafo_stats_t *out_stats);

int bitraf64_ascii_grafo_report(char *buffer, size_t buffer_size);

int bitraf64_ascii_grafo_export_grid(
    char *buffer,
    size_t buffer_size,
    uint32_t start_idx,
    uint32_t grid_size
);

int bitraf64_ascii_grafo_validate(void);

/* ============================================================================
 * Listagens por Critério
 * ============================================================================ */

int bitraf64_ascii_grafo_list_base1_active(
    uint32_t *out_indices,
    size_t max_count
);

int bitraf64_ascii_grafo_list_base2_active(
    uint32_t *out_indices,
    size_t max_count
);

int bitraf64_ascii_grafo_list_conflicts(
    uint32_t *out_indices,
    size_t max_count
);

/* ============================================================================
 * Estatísticas por Camada Toroidal
 * ============================================================================ */

typedef struct {
  uint32_t layer;
  uint32_t cell_count;
  uint32_t base1_count;
  uint32_t base2_count;
  uint32_t conflict_count;
  float avg_moore_density;
  float avg_coherence;
} bitraf64_layer_stats_t;

int bitraf64_ascii_grafo_layer_stats(
    uint32_t layer,
    bitraf64_layer_stats_t *out_stats
);

int bitraf64_ascii_grafo_layer_report(
    uint32_t layer,
    char *buffer,
    size_t buffer_size
);

#endif /* TERMUX_ASCII_GRAFO_INTEGRATION_H */
