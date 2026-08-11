#include "dep_resolver.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#define GCD_SAFE(a, b) ({ uint32_t _a = (a), _b = (b); while (_b) { uint32_t _t = _b; _b = _a % _b; _a = _t; } _a; })

static uint32_t crc32c_byte(uint32_t crc, uint8_t byte) {
  const uint32_t poly = 0x82F63B78U;
  crc ^= byte;
  for (int i = 0; i < 8; i++) {
    crc = (crc >> 1) ^ ((crc & 1) ? poly : 0);
  }
  return crc;
}

static uint32_t crc32c_buffer(const void *buf, size_t len, uint32_t crc) {
  const uint8_t *bytes = (const uint8_t *)buf;
  for (size_t i = 0; i < len; i++) {
    crc = crc32c_byte(crc, bytes[i]);
  }
  return crc;
}

int termux_dep_graph_init(struct termux_dep_graph *graph, uint32_t pkg_count) {
  if (!graph || pkg_count == 0 || pkg_count > TERMUX_MAX_PACKAGES) return -1;

  graph->pkg_count = pkg_count;
  graph->total_edges = 0;
  graph->edges_finalized = 0;

  graph->deps_row = (uint16_t *)calloc(pkg_count + 1, sizeof(uint16_t));
  if (!graph->deps_row) return -2;

  graph->deps_col = (uint16_t *)calloc(TERMUX_MAX_DEPS_TOTAL, sizeof(uint16_t));
  if (!graph->deps_col) {
    free(graph->deps_row);
    return -2;
  }

  graph->depths = (uint8_t *)calloc(pkg_count, sizeof(uint8_t));
  if (!graph->depths) {
    free(graph->deps_row);
    free(graph->deps_col);
    return -2;
  }

  graph->edges_from = (uint16_t *)calloc(TERMUX_MAX_DEPS_TOTAL, sizeof(uint16_t));
  if (!graph->edges_from) {
    free(graph->deps_row);
    free(graph->deps_col);
    free(graph->depths);
    return -2;
  }

  graph->edges_to = (uint16_t *)calloc(TERMUX_MAX_DEPS_TOTAL, sizeof(uint16_t));
  if (!graph->edges_to) {
    free(graph->deps_row);
    free(graph->deps_col);
    free(graph->depths);
    free(graph->edges_from);
    return -2;
  }

  graph->crc32c_checksum = 0;
  return 0;
}

void termux_dep_graph_free(struct termux_dep_graph *graph) {
  if (!graph) return;
  if (graph->deps_row) free(graph->deps_row);
  if (graph->deps_col) free(graph->deps_col);
  if (graph->depths) free(graph->depths);
  if (graph->edges_from) free(graph->edges_from);
  if (graph->edges_to) free(graph->edges_to);
}

int termux_dep_graph_add_edge(struct termux_dep_graph *graph,
                               uint16_t from, uint16_t to) {
  if (!graph || from >= graph->pkg_count || to >= graph->pkg_count) return -1;
  if (graph->total_edges >= TERMUX_MAX_DEPS_TOTAL) return -2;
  if (graph->edges_finalized) return -3;

  graph->edges_from[graph->total_edges] = from;
  graph->edges_to[graph->total_edges] = to;
  graph->total_edges++;

  return 0;
}

int termux_dep_graph_finalize_csr(struct termux_dep_graph *graph) {
  if (!graph || graph->edges_finalized) return -1;
  if (graph->total_edges > TERMUX_MAX_DEPS_TOTAL) return -2;

  uint16_t *counts = (uint16_t *)calloc(graph->pkg_count, sizeof(uint16_t));
  if (!counts) return -3;

  for (uint32_t i = 0; i < graph->total_edges; i++) {
    counts[graph->edges_from[i]]++;
  }

  uint16_t offset = 0;
  for (uint32_t i = 0; i < graph->pkg_count; i++) {
    graph->deps_row[i] = offset;
    offset += counts[i];
    counts[i] = 0;
  }
  graph->deps_row[graph->pkg_count] = offset;

  for (uint32_t i = 0; i < graph->total_edges; i++) {
    uint16_t from = graph->edges_from[i];
    uint16_t pos = graph->deps_row[from] + counts[from];
    graph->deps_col[pos] = graph->edges_to[i];
    counts[from]++;
  }

  free(counts);
  graph->edges_finalized = 1;
  return 0;
}

int termux_dep_graph_compute_depths(struct termux_dep_graph *graph) {
  if (!graph || !graph->depths || !graph->deps_row || !graph->edges_finalized) return -1;

  uint8_t *in_degree = (uint8_t *)calloc(graph->pkg_count, sizeof(uint8_t));
  if (!in_degree) return -2;

  uint16_t *queue = (uint16_t *)malloc(graph->pkg_count * sizeof(uint16_t));
  if (!queue) {
    free(in_degree);
    return -2;
  }

  for (uint32_t pkg = 0; pkg < graph->pkg_count; pkg++) {
    uint16_t start = graph->deps_row[pkg];
    uint16_t end = ((uint32_t)(pkg + 1) < graph->pkg_count) ? graph->deps_row[pkg + 1] : graph->total_edges;

    for (uint16_t i = start; i < end; i++) {
      if (graph->deps_col[i] < graph->pkg_count) {
        in_degree[graph->deps_col[i]]++;
      }
    }
  }

  uint32_t queue_head = 0, queue_tail = 0;
  for (uint32_t pkg = 0; pkg < graph->pkg_count; pkg++) {
    if (in_degree[pkg] == 0) {
      queue[queue_tail++] = pkg;
      graph->depths[pkg] = 0;
    }
  }

  uint32_t processed = 0;
  while (queue_head < queue_tail) {
    uint16_t pkg = queue[queue_head++];
    processed++;

    uint16_t start = graph->deps_row[pkg];
    uint16_t end = ((uint32_t)(pkg + 1) < graph->pkg_count) ? graph->deps_row[pkg + 1] : graph->total_edges;

    for (uint16_t i = start; i < end; i++) {
      uint16_t dep = graph->deps_col[i];
      if (dep < graph->pkg_count) {
        graph->depths[dep] = (graph->depths[dep] > graph->depths[pkg] + 1) ?
                             graph->depths[dep] : (graph->depths[pkg] + 1);

        in_degree[dep]--;
        if (in_degree[dep] == 0) {
          queue[queue_tail++] = dep;
        }
      }
    }
  }

  int ret = (processed == graph->pkg_count) ? 0 : -3;

  free(in_degree);
  free(queue);
  return ret;
}

int termux_dep_graph_validate(struct termux_dep_graph *graph) {
  if (!graph || !graph->depths || !graph->deps_col) return -1;

  for (uint32_t pkg = 0; pkg < graph->pkg_count; pkg++) {
    if (graph->depths[pkg] >= TERMUX_TOROIDAL_LAYERS) return -3;

    uint32_t gcd_depth_32 = GCD_SAFE(graph->depths[pkg], TERMUX_TOROIDAL_LAYERS);
    uint8_t valid_gcds[] = {1, 2, 4, 8, 16, 32};
    int valid = 0;
    for (size_t i = 0; i < 6; i++) {
      if (gcd_depth_32 == valid_gcds[i]) {
        valid = 1;
        break;
      }
    }
    if (!valid) return -2;
  }

  return 0;
}

uint32_t termux_dep_graph_compute_crc32c(struct termux_dep_graph *graph) {
  if (!graph) return 0;

  uint32_t crc = 0xFFFFFFFFU;

  crc = crc32c_buffer(&graph->pkg_count, sizeof(graph->pkg_count), crc);
  crc = crc32c_buffer(&graph->total_edges, sizeof(graph->total_edges), crc);
  crc = crc32c_buffer(graph->depths, graph->pkg_count * sizeof(uint8_t), crc);
  crc = crc32c_buffer(graph->deps_col, graph->total_edges * sizeof(uint16_t), crc);

  graph->crc32c_checksum = crc ^ 0xFFFFFFFFU;
  return graph->crc32c_checksum;
}

int termux_dep_resolver_init(struct termux_dep_resolver *resolver, uint32_t pkg_count) {
  if (!resolver || pkg_count == 0) return -1;

  memset(resolver, 0, sizeof(*resolver));
  int ret = termux_dep_graph_init(&resolver->graph, pkg_count);
  if (ret != 0) return ret;

  return 0;
}

int termux_dep_resolver_build_layers(struct termux_dep_resolver *resolver) {
  if (!resolver || !resolver->graph.depths) return -1;

  int ret = termux_dep_graph_compute_depths(&resolver->graph);
  if (ret != 0) return ret;

  ret = termux_dep_graph_validate(&resolver->graph);
  if (ret != 0) return ret;

  for (uint32_t layer = 0; layer < TERMUX_TOROIDAL_LAYERS; layer++) {
    resolver->layers[layer].layer_idx = layer;
    resolver->layers[layer].pkg_count = 0;
  }

  for (uint32_t pkg = 0; pkg < resolver->graph.pkg_count; pkg++) {
    uint8_t depth = resolver->graph.depths[pkg];
    uint32_t layer_idx = depth % TERMUX_TOROIDAL_LAYERS;

    if (layer_idx >= TERMUX_TOROIDAL_LAYERS) layer_idx = TERMUX_TOROIDAL_LAYERS - 1;

    struct termux_layer_batch *layer = &resolver->layers[layer_idx];
    if (layer->pkg_count < TERMUX_MAX_LAYER_SIZE) {
      layer->pkg_indices[layer->pkg_count++] = pkg;
    }
  }

  termux_dep_graph_compute_crc32c(&resolver->graph);

  return 0;
}

struct termux_layer_batch *termux_dep_resolver_get_layer(struct termux_dep_resolver *resolver,
                                                         uint32_t layer_idx) {
  if (!resolver || layer_idx >= TERMUX_TOROIDAL_LAYERS) return NULL;
  return &resolver->layers[layer_idx];
}

int termux_dep_resolver_compute_layer_phi(struct termux_layer_batch *layer,
                                           struct termux_dep_graph *graph) {
  if (!layer || !graph) return -1;

  uint64_t depth_score = TERMUX_TOROIDAL_LAYERS - layer->layer_idx;
  uint64_t gcd_val = GCD_SAFE(layer->layer_idx + 1, TERMUX_TOROIDAL_LAYERS);
  uint64_t phi = ((depth_score * 65536) / TERMUX_TOROIDAL_LAYERS) *
                 ((gcd_val * 65536) / TERMUX_TOROIDAL_LAYERS) / 65536;

  layer->coherence_phi = phi;
  return 0;
}

void termux_dep_resolver_destroy(struct termux_dep_resolver *resolver) {
  if (!resolver) return;
  termux_dep_graph_free(&resolver->graph);
  memset(resolver, 0, sizeof(*resolver));
}

void termux_dep_resolver_print_stats(struct termux_dep_resolver *resolver) {
  if (!resolver) return;

  printf("=== Dependency Graph Statistics ===\n");
  printf("Packages: %u\n", resolver->graph.pkg_count);
  printf("Total edges: %u\n", resolver->graph.total_edges);
  printf("CRC32c checksum: 0x%08x\n", resolver->graph.crc32c_checksum);
  printf("Toroidal layers: %u\n", TERMUX_TOROIDAL_LAYERS);

  for (uint32_t layer = 0; layer < TERMUX_TOROIDAL_LAYERS; layer++) {
    if (resolver->layers[layer].pkg_count > 0) {
      printf("  Layer %2u: %u packages, φ=%" PRIu64 "\n",
             layer, resolver->layers[layer].pkg_count,
             resolver->layers[layer].coherence_phi);
    }
  }
}
