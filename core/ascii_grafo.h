#ifndef TERMUX_ASCII_GRAFO_H
#define TERMUX_ASCII_GRAFO_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/*
 * ASCII-Grafo Toroidal: Moore Neighborhood + Bit Central Compartilhado
 *
 * Modelo estrutural combinando:
 *   - Célula Moore 3×3 (8 vizinhos + 1 central)
 *   - Bit central compartilhado entre base¹ e base²
 *   - Representação ASCII de 33 caracteres
 *   - Integração com topologia BITRAF64 (10×10×10×6)
 */

/* Direções Moore (8-conectividade) */
typedef enum {
  MOORE_NW = 0,  /* Noroeste (canto superior-esquerdo) */
  MOORE_N  = 1,  /* Norte (superior) */
  MOORE_NE = 2,  /* Nordeste (canto superior-direito) */
  MOORE_W  = 3,  /* Oeste (esquerda) */
  MOORE_C  = 4,  /* Central (compartilhado) */
  MOORE_E  = 5,  /* Leste (direita) */
  MOORE_SW = 6,  /* Sudoeste (canto inferior-esquerdo) */
  MOORE_S  = 7,  /* Sul (inferior) */
  MOORE_SE = 8,  /* Sudeste (canto inferior-direito) */
  MOORE_COUNT = 9
} moore_direction_t;

/* Célula Moore elementar (9 bits + padding) */
typedef struct {
  uint16_t neighborhood : 9;  /* 9 bits Moore (0-8) */
  uint16_t _reserved    : 7;  /* Padding */
} ascii_moore_cell_t;

/* Duas bases sobrepostas (base¹ e base²) */
typedef struct {
  uint16_t base1 : 9;         /* Base¹: 9 bits Moore */
  uint16_t base2 : 9;         /* Base²: 9 bits Moore */
  uint16_t _reserved : 14;
  uint8_t central_shared;     /* 1 bit compartilhado: base1[C] ∩ base2[C] */
} ascii_grafo_dual_t;

/* Célula ASCII-Grafo integrada com BITRAF64 */
typedef struct {
  /* Célula Moore */
  ascii_moore_cell_t moore;

  /* Coordenadas toroidais BITRAF64 */
  uint32_t i, j, k;           /* Coordenadas espaciais (0-9) */
  uint32_t f;                 /* Fractal/domínio (0-5) */
  uint32_t layer;             /* Camada: (i+j+k) mod 32 */
  uint32_t flat_idx;          /* Índice flat: f*1000 + i*100 + j*10 + k */

  /* Bases dual */
  uint8_t base1_active;       /* Base¹ ativa (0 ou 1) */
  uint8_t base2_active;       /* Base² ativa (0 ou 1) */
  uint8_t central_bit;        /* Bit central: 1, 0, ou ∅ (255 = vazio) */

  /* Representação ASCII (33 caracteres) */
  char ascii_sequence[34];    /* null-terminated string */
  uint32_t ascii_hash;        /* CRC32c da sequência */
} ascii_grafo_cell_t;

/* Grafo estruturado (rede de células Moore) */
typedef struct {
  ascii_grafo_cell_t *cells;  /* Array de células (2057 para termux) */
  uint32_t cell_count;        /* Número total de células */

  /* Estatísticas */
  uint32_t total_moore_bits;  /* Total de bits ativos em vizinhanças */
  uint32_t total_central_bits;/* Total de bits centrais ativos */
  uint32_t conflicts;         /* Conflitos de base (base1 ∩ base2 ≠ ∅) */

  /* Cache de adjacências */
  uint32_t *neighbor_lists;   /* Índices de vizinhos por célula */
  uint32_t *neighbor_counts;  /* Contagem de vizinhos por célula */
} ascii_grafo_graph_t;

/* ============================================================================
 * API Principal
 * ============================================================================ */

/*
 * Inicializar célula Moore vazia
 */
ascii_moore_cell_t ascii_moore_init(void);

/*
 * Definir/obter bit em célula Moore
 */
int ascii_moore_set_bit(ascii_moore_cell_t *cell, moore_direction_t dir, int value);
int ascii_moore_get_bit(const ascii_moore_cell_t *cell, moore_direction_t dir);

/*
 * Operações em vizinhanças Moore
 */
int ascii_moore_orthogonal_sum(const ascii_moore_cell_t *cell);
int ascii_moore_diagonal_sum(const ascii_moore_cell_t *cell);
int ascii_moore_population(const ascii_moore_cell_t *cell);

/*
 * Inicializar célula ASCII-Grafo completa
 */
int ascii_grafo_cell_init(
    ascii_grafo_cell_t *cell,
    uint32_t i, uint32_t j, uint32_t k, uint32_t f
);

/*
 * Ativar/desativar bases
 */
int ascii_grafo_set_base1(ascii_grafo_cell_t *cell, int active);
int ascii_grafo_set_base2(ascii_grafo_cell_t *cell, int active);

/*
 * Bit central compartilhado (base¹ ∩ base²)
 */
int ascii_grafo_set_central(ascii_grafo_cell_t *cell, uint8_t value);
uint8_t ascii_grafo_get_central(const ascii_grafo_cell_t *cell);
int ascii_grafo_has_conflict(const ascii_grafo_cell_t *cell);

/*
 * Gerar sequência ASCII (33 caracteres)
 */
int ascii_grafo_render(ascii_grafo_cell_t *cell);
const char* ascii_grafo_get_sequence(const ascii_grafo_cell_t *cell);

/*
 * Gerar representação visual ASCII art
 */
void ascii_grafo_print_cell(const ascii_grafo_cell_t *cell);
void ascii_grafo_print_moore(const ascii_moore_cell_t *cell);

/*
 * Operações de grafo estruturado
 */
int ascii_grafo_graph_init(
    ascii_grafo_graph_t *graph,
    uint32_t cell_count
);

int ascii_grafo_graph_free(ascii_grafo_graph_t *graph);

/*
 * Computar adjacências (vizinhos diretos e diagonais)
 */
int ascii_grafo_compute_neighbors(ascii_grafo_graph_t *graph);

/*
 * Obter vizinhos de uma célula
 */
int ascii_grafo_get_neighbors(
    const ascii_grafo_graph_t *graph,
    uint32_t cell_idx,
    uint32_t *out_neighbors,
    size_t max_neighbors
);

/*
 * Validar integridade do grafo
 */
int ascii_grafo_validate(const ascii_grafo_graph_t *graph);

/*
 * Computar estatísticas
 */
typedef struct {
  uint32_t total_bits;
  uint32_t orthogonal_bits;
  uint32_t diagonal_bits;
  uint32_t central_bits;
  uint32_t conflicts;
  float avg_neighborhood_density;
  float base1_coverage;
  float base2_coverage;
  float conflict_ratio;
} ascii_grafo_stats_t;

int ascii_grafo_compute_stats(
    const ascii_grafo_graph_t *graph,
    ascii_grafo_stats_t *out_stats
);

/*
 * Gerar relatório ASCII-Grafo
 */
int ascii_grafo_report(
    const ascii_grafo_graph_t *graph,
    char *buffer,
    size_t buffer_size
);

/*
 * Exportar grafo em formato ASCII (3×3 grid visual)
 */
int ascii_grafo_export_grid(
    const ascii_grafo_graph_t *graph,
    char *buffer,
    size_t buffer_size,
    uint32_t start_idx,
    uint32_t grid_size  /* 3, 5, 7, etc */
);

#endif /* TERMUX_ASCII_GRAFO_H */
