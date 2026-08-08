#ifndef TERMUX_DEP_RESOLVER_H
#define TERMUX_DEP_RESOLVER_H

#include <stdint.h>
#include <stddef.h>

#define TERMUX_MAX_PACKAGES 2057
#define TERMUX_MAX_DEPS_TOTAL 14336
#define TERMUX_TOROIDAL_LAYERS 32
#define TERMUX_MAX_LAYER_SIZE 65

struct termux_dep_graph {
  uint16_t *deps_row;          // [2058] offsets into deps_col (size = pkg_count+1)
  uint16_t *deps_col;          // [~14K] dependency indices
  uint8_t *depths;             // [2057] topological depth
  uint16_t *edges_from;        // [~14K] temporary: edge source packages
  uint16_t *edges_to;          // [~14K] temporary: edge target packages
  uint32_t pkg_count;
  uint32_t total_edges;
  uint32_t edges_finalized;    // 1 if CSR format ready, 0 if edges stored
  uint32_t crc32c_checksum;    // graph integrity
};

struct termux_layer_batch {
  uint16_t pkg_indices[TERMUX_MAX_LAYER_SIZE];
  uint32_t pkg_count;
  uint32_t layer_idx;          // 0..31
  uint64_t coherence_phi;      // layer-level φ
};

struct termux_dep_resolver {
  struct termux_dep_graph graph;
  struct termux_layer_batch layers[TERMUX_TOROIDAL_LAYERS];
  uint32_t completion_map;     // bitfield progress
};

int termux_dep_graph_init(struct termux_dep_graph *graph, uint32_t pkg_count);
int termux_dep_graph_add_edge(struct termux_dep_graph *graph,
                               uint16_t from, uint16_t to);
int termux_dep_graph_finalize_csr(struct termux_dep_graph *graph);
void termux_dep_graph_free(struct termux_dep_graph *graph);
int termux_dep_graph_compute_depths(struct termux_dep_graph *graph);
int termux_dep_graph_validate(struct termux_dep_graph *graph);
uint32_t termux_dep_graph_compute_crc32c(struct termux_dep_graph *graph);

int termux_dep_resolver_init(struct termux_dep_resolver *resolver, uint32_t pkg_count);
int termux_dep_resolver_build_layers(struct termux_dep_resolver *resolver);
struct termux_layer_batch *termux_dep_resolver_get_layer(struct termux_dep_resolver *resolver,
                                                         uint32_t layer_idx);
int termux_dep_resolver_compute_layer_phi(struct termux_layer_batch *layer,
                                           struct termux_dep_graph *graph);
void termux_dep_resolver_destroy(struct termux_dep_resolver *resolver);

#endif // TERMUX_DEP_RESOLVER_H
