#include "dep_resolver.h"

#define TERMUX_STATIC_GRAPH_STORAGE 1

struct termux_static_storage {
  uint16_t deps_row_buf[TERMUX_MAX_PACKAGES + 1];
  uint16_t deps_col_buf[TERMUX_MAX_DEPS_TOTAL];
  uint8_t depths_buf[TERMUX_MAX_PACKAGES];
  uint8_t in_degree_buf[TERMUX_MAX_PACKAGES];
  uint16_t queue_buf[TERMUX_MAX_PACKAGES];
};

static struct termux_static_storage static_storage = {};

static inline void memzero(void *p, size_t len) {
  uint8_t *b = (uint8_t *)p;
  for (size_t i = 0; i < len; i++) b[i] = 0;
}

static inline uint32_t gcd_compute(uint32_t a, uint32_t b) {
  while (b) {
    uint32_t t = b;
    b = a % b;
    a = t;
  }
  return a;
}

static inline uint32_t crc32c_byte_branchless(uint32_t crc, uint8_t byte) {
  const uint32_t poly = 0x82F63B78U;
  crc ^= byte;

  for (int i = 0; i < 8; i++) {
    uint32_t mask = -(crc & 1);
    crc = (crc >> 1) ^ (poly & mask);
  }
  return crc;
}

static inline uint32_t crc32c_buffer(const void *buf, size_t len, uint32_t crc) {
  const uint8_t *bytes = (const uint8_t *)buf;
  for (size_t i = 0; i < len; i++) {
    crc = crc32c_byte_branchless(crc, bytes[i]);
  }
  return crc;
}

int termux_dep_graph_init(struct termux_dep_graph *graph, uint32_t pkg_count) {
  if (!graph || pkg_count == 0 || pkg_count > TERMUX_MAX_PACKAGES) return -1;

  graph->pkg_count = pkg_count;
  graph->total_edges = 0;
  graph->crc32c_checksum = 0;

  graph->deps_row = static_storage.deps_row_buf;
  graph->deps_col = static_storage.deps_col_buf;
  graph->depths = static_storage.depths_buf;

  memzero(graph->deps_row, (pkg_count + 1) * sizeof(uint16_t));
  memzero(graph->deps_col, TERMUX_MAX_DEPS_TOTAL * sizeof(uint16_t));
  memzero(graph->depths, pkg_count * sizeof(uint8_t));

  return 0;
}

int termux_dep_graph_add_edge(struct termux_dep_graph *graph,
                               uint16_t from, uint16_t to) {
  if (!graph || from >= graph->pkg_count || to >= graph->pkg_count) return -1;
  if (graph->total_edges >= TERMUX_MAX_DEPS_TOTAL) return -2;

  graph->deps_col[graph->total_edges] = to;
  graph->total_edges++;

  return 0;
}

int termux_dep_graph_compute_depths(struct termux_dep_graph *graph) {
  if (!graph || !graph->depths || !graph->deps_row) return -1;

  uint8_t *in_degree = static_storage.in_degree_buf;
  uint16_t *queue = static_storage.queue_buf;

  memzero(in_degree, graph->pkg_count);

  for (uint32_t pkg = 0; pkg < graph->pkg_count; pkg++) {
    uint16_t start = graph->deps_row[pkg];
    uint16_t end = ((uint32_t)(pkg + 1) < graph->pkg_count) ?
                   graph->deps_row[pkg + 1] : graph->total_edges;

    for (uint16_t i = start; i < end; i++) {
      if (graph->deps_col[i] < graph->pkg_count) {
        in_degree[graph->deps_col[i]]++;
      }
    }
  }

  uint32_t queue_head = 0, queue_tail = 0;
  for (uint32_t pkg = 0; pkg < graph->pkg_count; pkg++) {
    if (in_degree[pkg] == 0) {
      if (queue_tail >= graph->pkg_count) return -2;
      queue[queue_tail++] = pkg;
      graph->depths[pkg] = 0;
    }
  }

  uint32_t processed = 0;
  while (queue_head < queue_tail) {
    uint16_t pkg = queue[queue_head++];
    processed++;

    uint16_t start = graph->deps_row[pkg];
    uint16_t end = ((uint32_t)(pkg + 1) < graph->pkg_count) ?
                   graph->deps_row[pkg + 1] : graph->total_edges;

    for (uint16_t i = start; i < end; i++) {
      uint16_t dep = graph->deps_col[i];
      if (dep < graph->pkg_count) {
        uint8_t new_depth = graph->depths[pkg] + 1;
        if (graph->depths[dep] < new_depth) {
          graph->depths[dep] = new_depth;
        }

        in_degree[dep]--;
        if (in_degree[dep] == 0) {
          if (queue_tail >= graph->pkg_count) return -2;
          queue[queue_tail++] = dep;
        }
      }
    }
  }

  return (processed == graph->pkg_count) ? 0 : -3;
}

int termux_dep_graph_validate(struct termux_dep_graph *graph) {
  if (!graph || !graph->depths) return -1;

  for (uint32_t pkg = 0; pkg < graph->pkg_count; pkg++) {
    uint32_t gcd_val = gcd_compute(graph->depths[pkg], TERMUX_TOROIDAL_LAYERS);
    uint8_t valid_gcds[] = {1, 2, 3, 6, 7, 14, 21, 42};
    int is_valid = 0;
    for (size_t i = 0; i < 8; i++) {
      if (gcd_val == valid_gcds[i]) {
        is_valid = 1;
        break;
      }
    }
    if (!is_valid) return -2;
  }

  return 0;
}

uint32_t termux_dep_graph_compute_crc32c(struct termux_dep_graph *graph) {
  if (!graph) return 0;

  uint32_t crc = 0xFFFFFFFFU;

  crc = crc32c_buffer(&graph->pkg_count, sizeof(graph->pkg_count), crc);
  crc = crc32c_buffer(&graph->total_edges, sizeof(graph->total_edges), crc);
  crc = crc32c_buffer(graph->depths, graph->pkg_count, crc);
  crc = crc32c_buffer(graph->deps_col, graph->total_edges * sizeof(uint16_t), crc);

  graph->crc32c_checksum = crc ^ 0xFFFFFFFFU;
  return graph->crc32c_checksum;
}

int termux_dep_resolver_init(struct termux_dep_resolver *resolver, uint32_t pkg_count) {
  if (!resolver || pkg_count == 0) return -1;

  memzero(resolver, sizeof(*resolver));
  return termux_dep_graph_init(&resolver->graph, pkg_count);
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
  if (!resolver || layer_idx >= TERMUX_TOROIDAL_LAYERS) return 0;
  return &resolver->layers[layer_idx];
}

int termux_dep_resolver_compute_layer_phi(struct termux_layer_batch *layer,
                                           struct termux_dep_graph *graph) {
  if (!layer || !graph) return -1;

  uint64_t depth_score = TERMUX_TOROIDAL_LAYERS - layer->layer_idx;
  uint32_t gcd_val = gcd_compute(layer->layer_idx + 1, TERMUX_TOROIDAL_LAYERS);
  uint64_t phi = ((depth_score * 65536) / TERMUX_TOROIDAL_LAYERS) *
                 ((((uint64_t)gcd_val) * 65536) / TERMUX_TOROIDAL_LAYERS) / 65536;

  layer->coherence_phi = phi;
  return 0;
}

void termux_dep_resolver_destroy(struct termux_dep_resolver *resolver) {
  if (!resolver) return;
  memzero(resolver, sizeof(*resolver));
}
