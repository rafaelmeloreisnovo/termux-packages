#include "dep_resolver.h"

/* Legacy ABI shim. Fixed-size backing store, no allocation or syscalls.
 * Unlike raf_dag, this API has one global workspace and is NOT reentrant.
 * Caller must destroy the current graph before initializing another.
 */
struct termux_static_storage {
  uint16_t row[TERMUX_MAX_PACKAGES + 1];
  uint16_t col[TERMUX_MAX_DEPS_TOTAL];
  uint8_t depth[TERMUX_MAX_PACKAGES];
  uint16_t from[TERMUX_MAX_DEPS_TOTAL];
  uint16_t to[TERMUX_MAX_DEPS_TOTAL];
  uint16_t cursor[TERMUX_MAX_PACKAGES];
  uint16_t indegree[TERMUX_MAX_PACKAGES];
  uint16_t queue[TERMUX_MAX_PACKAGES];
};
static struct termux_static_storage storage;
static struct termux_dep_graph *owner;

static void zero_bytes(void *p, size_t count) {
  uint8_t *b = (uint8_t *)p;
  for (size_t i = 0; i < count; ++i) b[i] = 0;
}
static uint32_t crc32c_bytes(uint32_t c, const void *data, size_t count) {
  const uint8_t *b = (const uint8_t *)data;
  for (size_t j = 0; j < count; ++j) {
    c ^= b[j];
    for (uint32_t k = 0; k < 8; ++k)
      c = (c >> 1) ^ (0x82F63B78U & (0U - (c & 1U)));
  }
  return c;
}
static int is_owner(const struct termux_dep_graph *g) {
  return g && g == owner && g->deps_row == storage.row &&
         g->deps_col == storage.col && g->depths == storage.depth;
}
int termux_dep_graph_init(struct termux_dep_graph *g, uint32_t nodes) {
  if (!g || !nodes || nodes > TERMUX_MAX_PACKAGES) return -1;
  if (owner && owner != g) return -4;
  zero_bytes(g, sizeof(*g));
  g->pkg_count = nodes;
  g->deps_row = storage.row;
  g->deps_col = storage.col;
  g->depths = storage.depth;
  g->edges_from = storage.from;
  g->edges_to = storage.to;
  zero_bytes(storage.row, (nodes + 1U) * sizeof(uint16_t));
  zero_bytes(storage.depth, nodes);
  owner = g;
  return 0;
}
void termux_dep_graph_free(struct termux_dep_graph *g) {
  if (!is_owner(g)) return;
  owner = (struct termux_dep_graph *)0;
  zero_bytes(g, sizeof(*g));
}
int termux_dep_graph_add_edge(struct termux_dep_graph *g, uint16_t from, uint16_t to) {
  if (!is_owner(g) || from >= g->pkg_count || to >= g->pkg_count) return -1;
  if (g->edges_finalized) return -3;
  if (g->total_edges >= TERMUX_MAX_DEPS_TOTAL) return -2;
  g->edges_from[g->total_edges] = from;
  g->edges_to[g->total_edges] = to;
  ++g->total_edges;
  return 0;
}
int termux_dep_graph_finalize_csr(struct termux_dep_graph *g) {
  if (!is_owner(g)) return -1;
  if (g->edges_finalized) return -3;
  for (uint32_t i = 0; i <= g->pkg_count; ++i) g->deps_row[i] = 0;
  for (uint32_t i = 0; i < g->total_edges; ++i)
    ++g->deps_row[(uint32_t)g->edges_from[i] + 1U];
  uint32_t sum = 0;
  for (uint32_t i = 0; i < g->pkg_count; ++i) {
    sum += g->deps_row[i + 1U];
    g->deps_row[i + 1U] = (uint16_t)sum;
    storage.cursor[i] = g->deps_row[i];
  }
  if (sum != g->total_edges) return -2;
  for (uint32_t i = 0; i < g->total_edges; ++i)
    g->deps_col[storage.cursor[g->edges_from[i]]++] = g->edges_to[i];
  g->edges_finalized = 1U;
  return 0;
}
int termux_dep_graph_compute_depths(struct termux_dep_graph *g) {
  if (!is_owner(g) || !g->edges_finalized) return -1;
  zero_bytes(storage.indegree, g->pkg_count * sizeof(uint16_t));
  zero_bytes(g->depths, g->pkg_count);
  for (uint32_t i = 0; i < g->total_edges; ++i) ++storage.indegree[g->edges_to[i]];
  uint32_t head = 0, tail = 0;
  for (uint32_t i = 0; i < g->pkg_count; ++i)
    if (storage.indegree[i] == 0) storage.queue[tail++] = (uint16_t)i;
  while (head < tail) {
    uint32_t u = storage.queue[head++];
    for (uint32_t i = g->deps_row[u]; i < g->deps_row[u + 1U]; ++i) {
      uint32_t v = g->deps_col[i];
      if (g->depths[u] == 255U) return -4;
      uint8_t next = (uint8_t)(g->depths[u] + 1U);
      if (next > g->depths[v]) g->depths[v] = next;
      --storage.indegree[v];
      if (storage.indegree[v] == 0) storage.queue[tail++] = (uint16_t)v;
    }
  }
  return head == g->pkg_count ? 0 : -3;
}
int termux_dep_graph_validate(struct termux_dep_graph *g) {
  if (!is_owner(g) || !g->edges_finalized) return -1;
  if (g->deps_row[0] != 0 || g->deps_row[g->pkg_count] != g->total_edges) return -2;
  for (uint32_t u = 0; u < g->pkg_count; ++u) {
    if (g->deps_row[u] > g->deps_row[u + 1U]) return -2;
    for (uint32_t i = g->deps_row[u]; i < g->deps_row[u + 1U]; ++i)
      if (g->deps_col[i] >= g->pkg_count || g->depths[g->deps_col[i]] <= g->depths[u])
        return -3;
  }
  return 0;
}
uint32_t termux_dep_graph_compute_crc32c(struct termux_dep_graph *g) {
  if (!is_owner(g) || !g->edges_finalized) return 0;
  uint32_t c = 0xFFFFFFFFU;
  c = crc32c_bytes(c, &g->pkg_count, sizeof(g->pkg_count));
  c = crc32c_bytes(c, &g->total_edges, sizeof(g->total_edges));
  c = crc32c_bytes(c, g->depths, g->pkg_count);
  c = crc32c_bytes(c, g->deps_col, g->total_edges * sizeof(uint16_t));
  g->crc32c_checksum = c ^ 0xFFFFFFFFU;
  return g->crc32c_checksum;
}
int termux_dep_resolver_init(struct termux_dep_resolver *r, uint32_t nodes) {
  if (!r || !nodes || nodes > TERMUX_MAX_PACKAGES) return -1;
  if (owner && owner != &r->graph) return -4;
  zero_bytes(r, sizeof(*r));
  return termux_dep_graph_init(&r->graph, nodes);
}
int termux_dep_resolver_build_layers(struct termux_dep_resolver *r) {
  if (!r || !is_owner(&r->graph)) return -1;
  int status = termux_dep_graph_compute_depths(&r->graph);
  if (status) return status;
  status = termux_dep_graph_validate(&r->graph);
  if (status) return status;
  /* Preflight layer capacity: no partially populated batches on failure. */
  uint16_t layer_counts[TERMUX_TOROIDAL_LAYERS];
  for (uint32_t i = 0; i < TERMUX_TOROIDAL_LAYERS; ++i) {
    r->layers[i].layer_idx = i;
    r->layers[i].pkg_count = 0U;
    layer_counts[i] = 0U;
  }
  for (uint32_t i = 0; i < r->graph.pkg_count; ++i) {
    uint32_t idx = (uint32_t)r->graph.depths[i] % TERMUX_TOROIDAL_LAYERS;
    if (++layer_counts[idx] > TERMUX_MAX_LAYER_SIZE) return -5;
  }
  for (uint32_t i = 0; i < r->graph.pkg_count; ++i) {
    struct termux_layer_batch *layer = &r->layers[r->graph.depths[i] % TERMUX_TOROIDAL_LAYERS];
    layer->pkg_indices[layer->pkg_count++] = (uint16_t)i;
  }
  (void)termux_dep_graph_compute_crc32c(&r->graph);
  return 0;
}
struct termux_layer_batch *termux_dep_resolver_get_layer(struct termux_dep_resolver *r, uint32_t idx) {
  if (!r || idx >= TERMUX_TOROIDAL_LAYERS) return (struct termux_layer_batch *)0;
  return &r->layers[idx];
}
int termux_dep_resolver_compute_layer_phi(struct termux_layer_batch *l, struct termux_dep_graph *g) {
  if (!l || !is_owner(g) || l->layer_idx >= TERMUX_TOROIDAL_LAYERS) return -1;
  uint32_t scale = TERMUX_TOROIDAL_LAYERS;
  /* gcd(k,32) is lowbit(k) for 1 <= k <= 32: no division helper on ARM32. */
  uint32_t k = l->layer_idx + 1U;
  uint32_t gcd = k & (0U - k);
  uint64_t score = ((uint64_t)(scale - l->layer_idx) * 65536U) / scale;
  l->coherence_phi = (score * (uint64_t)gcd) / scale;
  return 0;
}
void termux_dep_resolver_destroy(struct termux_dep_resolver *r) {
  if (!r) return;
  termux_dep_graph_free(&r->graph);
  zero_bytes(r, sizeof(*r));
}
