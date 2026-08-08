#include "pkg_dag.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Static dependency projection — OBSERVED_LIMITED.
 * ============================================================================ */

REAL_ALWAYS_INLINE
static void trim_dep_name(const char *raw, char *out, size_t cap) {
  if (REAL_UNLIKELY(cap == 0)) return;
  size_t i = 0;
  while (*raw == ' ' || *raw == '\t') raw++;
  while (*raw && *raw != '(' && *raw != ',' && *raw != ' ' &&
         *raw != '\t' && *raw != '|' && i + 1 < cap) {
    out[i++] = *raw++;
  }
  out[i] = '\0';
}

REAL_PURE REAL_HOT
static int32_t inv_find_idx(const pkg_inventory_t *inv, const char *name) {
  if (REAL_UNLIKELY(!inv || !name)) return -1;
  for (uint32_t i = 0; i < inv->count; i++) {
    if (REAL_UNLIKELY(strcmp(inv->entries[i].name, name) == 0))
      return (int32_t)i;
  }
  return -1;
}

REAL_COLD REAL_NOINLINE
static int edges_grow(pkg_dag_t *dag) {
  if (dag->edge_capacity > UINT32_MAX / 2U) {
    dag->allocation_failures++;
    return -1;
  }
  uint32_t new_cap = dag->edge_capacity == 0 ? 4096U : dag->edge_capacity * 2U;
  if ((size_t)new_cap > SIZE_MAX / sizeof(*dag->edges)) {
    dag->allocation_failures++;
    return -1;
  }
  pkg_dag_edge_t *n =
      (pkg_dag_edge_t *)realloc(dag->edges, (size_t)new_cap * sizeof(*dag->edges));
  if (REAL_UNLIKELY(!n)) {
    dag->allocation_failures++;
    return -1;
  }
  dag->edges = n;
  dag->edge_capacity = new_cap;
  return 0;
}

REAL_COLD REAL_NOINLINE
static int unres_grow(pkg_dag_t *dag) {
  if (dag->unresolved_capacity > UINT32_MAX / 2U) {
    dag->allocation_failures++;
    return -1;
  }
  uint32_t new_cap =
      dag->unresolved_capacity == 0 ? 256U : dag->unresolved_capacity * 2U;
  if ((size_t)new_cap > SIZE_MAX / sizeof(*dag->unresolved)) {
    dag->allocation_failures++;
    return -1;
  }
  pkg_dag_unresolved_t *n = (pkg_dag_unresolved_t *)realloc(
      dag->unresolved, (size_t)new_cap * sizeof(*dag->unresolved));
  if (REAL_UNLIKELY(!n)) {
    dag->allocation_failures++;
    return -1;
  }
  dag->unresolved = n;
  dag->unresolved_capacity = new_cap;
  return 0;
}

static int add_edge(pkg_dag_t *dag, uint32_t from_idx, uint8_t is_build_dep,
                    const char *dep_name) {
  int32_t to = inv_find_idx(dag->inv, dep_name);
  if (to < 0) {
    if (dag->unresolved_count >= dag->unresolved_capacity && unres_grow(dag) < 0)
      return -1;
    pkg_dag_unresolved_t *u = &dag->unresolved[dag->unresolved_count++];
    u->pkg_idx = from_idx;
    strncpy(u->missing_dep, dep_name, sizeof(u->missing_dep) - 1U);
    u->missing_dep[sizeof(u->missing_dep) - 1U] = '\0';
    return 0;
  }

  if (dag->edge_count >= dag->edge_capacity && edges_grow(dag) < 0)
    return -1;
  pkg_dag_edge_t *e = &dag->edges[dag->edge_count++];
  e->from_idx = from_idx;
  e->to_idx = (uint32_t)to;
  e->is_build_dep = is_build_dep;
  if (is_build_dep)
    dag->total_build_dep_edges++;
  else
    dag->total_depends_edges++;
  return 0;
}

/* Parse a comma-separated list. A field containing A | B is counted as an
 * alternative constraint, while the first alternative is retained in this
 * projection. An overlong field is never silently truncated. */
static int split_deps(pkg_dag_t *dag, uint32_t from_idx, uint8_t is_build_dep,
                      const char *raw) {
  if (!raw || !*raw) return 0;
  const char *p = raw;
  while (*p) {
    while (*p == ',' || *p == ' ' || *p == '\t') p++;
    if (!*p) break;

    const char *start = p;
    while (*p && *p != ',') p++;
    size_t len = (size_t)(p - start);
    while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t')) len--;
    if (len == 0) continue;
    if (len >= 128U) {
      dag->dependency_field_overflows++;
      return -1;
    }

    char field[128];
    memcpy(field, start, len);
    field[len] = '\0';
    if (strchr(field, '|') != NULL) dag->alternative_dep_fields++;

    char clean[64];
    trim_dep_name(field, clean, sizeof(clean));
    if (clean[0] != '\0' && add_edge(dag, from_idx, is_build_dep, clean) < 0)
      return -1;
  }
  return 0;
}

int pkg_dag_build(pkg_dag_t *dag, const pkg_inventory_t *inv) {
  memset(dag, 0, sizeof(*dag));
  dag->inv = inv;
  if (inv->count == 0) return -1;

  dag->parsed =
      (pkg_parser_result_t *)calloc(inv->count, sizeof(pkg_parser_result_t));
  if (!dag->parsed) {
    dag->allocation_failures++;
    return -1;
  }
  dag->parsed_count = inv->count;

  for (uint32_t i = 0; i < inv->count; i++) {
    if (REAL_UNLIKELY(pkg_parser_parse_file(inv->entries[i].path,
                                             &dag->parsed[i]) < 0))
      dag->parse_failures++;
  }

  for (uint32_t i = 0; i < inv->count; i++) {
    if (split_deps(dag, i, 0, dag->parsed[i].depends_raw) < 0) return -1;
    if (split_deps(dag, i, 1, dag->parsed[i].build_depends_raw) < 0) return -1;
  }

  dag->adj = (uint32_t **)calloc(inv->count, sizeof(uint32_t *));
  dag->adj_len = (uint32_t *)calloc(inv->count, sizeof(uint32_t));
  if (!dag->adj || !dag->adj_len) {
    dag->allocation_failures++;
    return -1;
  }

  for (uint32_t e = 0; e < dag->edge_count; e++)
    dag->adj_len[dag->edges[e].from_idx]++;

  for (uint32_t i = 0; i < inv->count; i++) {
    if (dag->adj_len[i] == 0) continue;
    if ((size_t)dag->adj_len[i] > SIZE_MAX / sizeof(uint32_t)) {
      dag->allocation_failures++;
      return -1;
    }
    dag->adj[i] = (uint32_t *)malloc((size_t)dag->adj_len[i] * sizeof(uint32_t));
    if (!dag->adj[i]) {
      dag->allocation_failures++;
      return -1;
    }
  }

  uint32_t *pos = (uint32_t *)calloc(inv->count, sizeof(uint32_t));
  if (!pos) {
    dag->allocation_failures++;
    return -1;
  }
  for (uint32_t e = 0; e < dag->edge_count; e++) {
    uint32_t f = dag->edges[e].from_idx;
    dag->adj[f][pos[f]++] = dag->edges[e].to_idx;
  }
  free(pos);
  return 0;
}

static int has_self_loop(const pkg_dag_t *dag, uint32_t node) {
  for (uint32_t k = 0; k < dag->adj_len[node]; k++) {
    if (dag->adj[node][k] == node) return 1;
  }
  return 0;
}

typedef struct {
  pkg_dag_t *dag;
  int32_t *index;
  uint32_t *low;
  uint8_t *on_stack;
  uint32_t *stack;
  uint32_t stack_len;
  uint32_t next_index;
  uint32_t *component;
  int failed;
} tarjan_ctx_t;

static void tarjan_visit(tarjan_ctx_t *ctx, uint32_t v) {
  if (ctx->failed) return;
  ctx->index[v] = (int32_t)ctx->next_index;
  ctx->low[v] = ctx->next_index;
  ctx->next_index++;
  ctx->stack[ctx->stack_len++] = v;
  ctx->on_stack[v] = 1;

  for (uint32_t k = 0; k < ctx->dag->adj_len[v]; k++) {
    uint32_t w = ctx->dag->adj[v][k];
    if (ctx->index[w] < 0) {
      tarjan_visit(ctx, w);
      if (ctx->failed) return;
      if (ctx->low[w] < ctx->low[v]) ctx->low[v] = ctx->low[w];
    } else if (ctx->on_stack[w] && (uint32_t)ctx->index[w] < ctx->low[v]) {
      ctx->low[v] = (uint32_t)ctx->index[w];
    }
  }

  if (ctx->low[v] != (uint32_t)ctx->index[v]) return;

  uint32_t component_len = 0;
  uint32_t w;
  do {
    if (ctx->stack_len == 0) {
      ctx->failed = 1;
      return;
    }
    w = ctx->stack[--ctx->stack_len];
    ctx->on_stack[w] = 0;
    ctx->component[component_len++] = w;
  } while (w != v);

  int cyclic = component_len > 1U || has_self_loop(ctx->dag, v);
  if (!cyclic) return;

  pkg_dag_cycle_t *cycle = &ctx->dag->cycles[ctx->dag->cycle_count];
  cycle->nodes = (uint32_t *)malloc((size_t)component_len * sizeof(uint32_t));
  if (!cycle->nodes) {
    ctx->dag->allocation_failures++;
    ctx->failed = 1;
    return;
  }
  memcpy(cycle->nodes, ctx->component, (size_t)component_len * sizeof(uint32_t));
  cycle->length = component_len;
  ctx->dag->cycle_count++;
  ctx->dag->cycle_nodes += component_len;
}

static int record_cyclic_sccs(pkg_dag_t *dag) {
  uint32_t n = dag->inv->count;
  dag->cycles = (pkg_dag_cycle_t *)calloc(n, sizeof(pkg_dag_cycle_t));
  int32_t *index = (int32_t *)malloc((size_t)n * sizeof(int32_t));
  uint32_t *low = (uint32_t *)calloc(n, sizeof(uint32_t));
  uint8_t *on_stack = (uint8_t *)calloc(n, sizeof(uint8_t));
  uint32_t *stack = (uint32_t *)malloc((size_t)n * sizeof(uint32_t));
  uint32_t *component = (uint32_t *)malloc((size_t)n * sizeof(uint32_t));

  if (!dag->cycles || !index || !low || !on_stack || !stack || !component) {
    dag->allocation_failures++;
    free(index);
    free(low);
    free(on_stack);
    free(stack);
    free(component);
    return -1;
  }
  for (uint32_t i = 0; i < n; i++) index[i] = -1;

  tarjan_ctx_t ctx = {
      .dag = dag,
      .index = index,
      .low = low,
      .on_stack = on_stack,
      .stack = stack,
      .stack_len = 0,
      .next_index = 0,
      .component = component,
      .failed = 0,
  };

  for (uint32_t i = 0; i < n && !ctx.failed; i++) {
    if (index[i] < 0) tarjan_visit(&ctx, i);
  }

  free(index);
  free(low);
  free(on_stack);
  free(stack);
  free(component);
  return ctx.failed ? -1 : 0;
}

int pkg_dag_topo_sort(pkg_dag_t *dag) {
  if (REAL_UNLIKELY(!dag->inv)) return -1;
  uint32_t n = dag->inv->count;
  if (n == 0) return -1;

  dag->topo_order = (uint32_t *)calloc(n, sizeof(uint32_t));
  dag->topo_depth = (uint32_t *)calloc(n, sizeof(uint32_t));
  uint32_t *out_deg = (uint32_t *)calloc(n, sizeof(uint32_t));
  uint32_t *rev_len = (uint32_t *)calloc(n, sizeof(uint32_t));
  uint32_t **rev = (uint32_t **)calloc(n, sizeof(uint32_t *));
  uint32_t *rev_pos = (uint32_t *)calloc(n, sizeof(uint32_t));
  uint32_t *queue = (uint32_t *)malloc((size_t)n * sizeof(uint32_t));

  if (!dag->topo_order || !dag->topo_depth || !out_deg || !rev_len ||
      !rev || !rev_pos || !queue) {
    dag->allocation_failures++;
    free(out_deg);
    free(rev_len);
    free(rev);
    free(rev_pos);
    free(queue);
    return -1;
  }

  for (uint32_t i = 0; i < n; i++) out_deg[i] = dag->adj_len[i];
  for (uint32_t i = 0; i < n; i++) {
    for (uint32_t k = 0; k < dag->adj_len[i]; k++)
      rev_len[dag->adj[i][k]]++;
  }

  for (uint32_t i = 0; i < n; i++) {
    if (rev_len[i] == 0) continue;
    rev[i] = (uint32_t *)malloc((size_t)rev_len[i] * sizeof(uint32_t));
    if (!rev[i]) {
      dag->allocation_failures++;
      for (uint32_t j = 0; j < i; j++) free(rev[j]);
      free(out_deg);
      free(rev_len);
      free(rev);
      free(rev_pos);
      free(queue);
      return -1;
    }
  }

  for (uint32_t i = 0; i < n; i++) {
    for (uint32_t k = 0; k < dag->adj_len[i]; k++) {
      uint32_t v = dag->adj[i][k];
      rev[v][rev_pos[v]++] = i;
    }
  }

  uint32_t head = 0, tail = 0;
  for (uint32_t i = 0; i < n; i++) {
    if (out_deg[i] == 0) queue[tail++] = i;
  }

  while (head < tail) {
    uint32_t u = queue[head++];
    dag->topo_order[dag->topo_count++] = u;
    for (uint32_t k = 0; k < rev_len[u]; k++) {
      uint32_t v = rev[u][k];
      if (out_deg[v] == 0) continue;
      out_deg[v]--;
      uint32_t new_depth = dag->topo_depth[u] + 1U;
      if (new_depth > dag->topo_depth[v]) dag->topo_depth[v] = new_depth;
      if (out_deg[v] == 0) {
        queue[tail++] = v;
        if (dag->topo_depth[v] > dag->max_depth)
          dag->max_depth = dag->topo_depth[v];
      }
    }
  }

  int scc_rc = 0;
  if (dag->topo_count < n) scc_rc = record_cyclic_sccs(dag);

  for (uint32_t i = 0; i < n; i++) free(rev[i]);
  free(out_deg);
  free(rev_len);
  free(rev);
  free(rev_pos);
  free(queue);
  return scc_rc;
}

const pkg_parser_result_t *pkg_dag_parsed_at(const pkg_dag_t *dag,
                                             uint32_t idx) {
  if (REAL_UNLIKELY(idx >= dag->parsed_count)) return NULL;
  return &dag->parsed[idx];
}

void pkg_dag_write_json(FILE *out, const pkg_dag_t *dag) {
  fputs("{\n", out);
  fputs("  \"schema\": \"pkg_dag/2.0.0\",\n", out);
  fputs("  \"status\": \"OBSERVED_LIMITED\",\n", out);
  fputs("  \"claim_allowed\": false,\n", out);
  fputs("  \"projection\": \"first_alternative_static_dependency_projection\",\n", out);
  fputs("  \"cycle_semantics\": \"cyclic_scc_count\",\n", out);
  fprintf(out, "  \"node_count\": %u,\n", dag->inv ? dag->inv->count : 0);
  fprintf(out, "  \"edge_count\": %u,\n", dag->edge_count);
  fprintf(out, "  \"depends_edges\": %u,\n", dag->total_depends_edges);
  fprintf(out, "  \"build_dep_edges\": %u,\n", dag->total_build_dep_edges);
  fprintf(out, "  \"unresolved_count\": %u,\n", dag->unresolved_count);
  fprintf(out, "  \"parse_failures\": %u,\n", dag->parse_failures);
  fprintf(out, "  \"alternative_dep_fields\": %u,\n", dag->alternative_dep_fields);
  fprintf(out, "  \"dependency_field_overflows\": %u,\n", dag->dependency_field_overflows);
  fprintf(out, "  \"allocation_failures\": %u,\n", dag->allocation_failures);
  fprintf(out, "  \"cycle_count\": %u,\n", dag->cycle_count);
  fprintf(out, "  \"cycle_nodes\": %u,\n", dag->cycle_nodes);
  fprintf(out, "  \"topo_ordered\": %u,\n", dag->topo_count);
  fprintf(out, "  \"max_depth\": %u\n", dag->max_depth);
  fputs("}\n", out);
}

void pkg_dag_report(FILE *out, const pkg_dag_t *dag) {
  fprintf(out, "=== Dependency Projection — OBSERVED_LIMITED ===\n");
  fprintf(out, "Claim allowed:             false\n");
  fprintf(out, "Projection:                first alternative of static fields\n");
  fprintf(out, "Nodes:                     %u\n", dag->inv ? dag->inv->count : 0);
  fprintf(out, "Edges total:               %u\n", dag->edge_count);
  fprintf(out, "  DEPENDS edges:           %u\n", dag->total_depends_edges);
  fprintf(out, "  BUILD_DEPENDS edges:     %u\n", dag->total_build_dep_edges);
  fprintf(out, "Unresolved/external names: %u\n", dag->unresolved_count);
  fprintf(out, "Parse failures:            %u\n", dag->parse_failures);
  fprintf(out, "Alternative fields:        %u\n", dag->alternative_dep_fields);
  fprintf(out, "Field overflows:           %u\n", dag->dependency_field_overflows);
  fprintf(out, "Allocation failures:       %u\n", dag->allocation_failures);
  fprintf(out, "Cyclic SCCs:               %u (%u nodes)\n",
          dag->cycle_count, dag->cycle_nodes);
  fprintf(out, "Topological ordered:       %u\n", dag->topo_count);
  fprintf(out, "Max depth:                 %u\n", dag->max_depth);

  for (uint32_t c = 0; c < dag->cycle_count && c < 3U; c++) {
    fprintf(out, "\nCyclic SCC %u (%u nodes):\n", c, dag->cycles[c].length);
    uint32_t show = dag->cycles[c].length > 10U ? 10U : dag->cycles[c].length;
    for (uint32_t i = 0; i < show; i++) {
      uint32_t idx = dag->cycles[c].nodes[i];
      fprintf(out, "  - %s\n", dag->inv->entries[idx].name);
    }
  }

  if (dag->unresolved_count > 0) {
    fprintf(out, "\nFirst 10 unresolved/external names:\n");
    uint32_t show = dag->unresolved_count > 10U ? 10U : dag->unresolved_count;
    for (uint32_t i = 0; i < show; i++) {
      fprintf(out, "  %s -> %s\n",
              dag->inv->entries[dag->unresolved[i].pkg_idx].name,
              dag->unresolved[i].missing_dep);
    }
  }
}

void pkg_dag_free(pkg_dag_t *dag) {
  if (!dag) return;
  free(dag->parsed);
  free(dag->edges);
  if (dag->adj) {
    for (uint32_t i = 0; dag->inv && i < dag->inv->count; i++)
      free(dag->adj[i]);
    free(dag->adj);
  }
  free(dag->adj_len);
  free(dag->topo_order);
  free(dag->topo_depth);
  free(dag->unresolved);
  if (dag->cycles) {
    for (uint32_t i = 0; i < dag->cycle_count; i++) free(dag->cycles[i].nodes);
    free(dag->cycles);
  }
  memset(dag, 0, sizeof(*dag));
}
