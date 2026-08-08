#include "pkg_dag.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * REAL: Dependency Graph
 * ============================================================================ */

/* Case-insensitive name comparison stripping version constraints.
 * termux deps look like "curl", "libc++", "coreutils (>=8.32)". */
static void trim_dep_name(const char *raw, char *out, size_t cap) {
  if (cap == 0) return;
  size_t i = 0;
  /* Skip leading whitespace */
  while (*raw == ' ' || *raw == '\t') raw++;
  /* Copy until '(' or ',' or whitespace */
  while (*raw && *raw != '(' && *raw != ',' && *raw != ' ' &&
         *raw != '\t' && *raw != '|' && i + 1 < cap) {
    out[i++] = *raw++;
  }
  out[i] = '\0';
  /* Trim trailing whitespace defensively */
  while (i > 0 && (out[i - 1] == ' ' || out[i - 1] == '\t')) {
    out[--i] = '\0';
  }
}

/* Split raw comma-separated dep list, calling cb for each name. */
static void split_deps(const char *raw,
                       void (*cb)(const char *name, void *ud), void *ud) {
  if (!raw || !cb) return;
  char buf[64];
  const char *p = raw;
  while (*p) {
    /* Skip commas and spaces */
    while (*p == ',' || *p == ' ' || *p == '\t') p++;
    if (!*p) break;
    /* Copy one field */
    size_t i = 0;
    while (*p && *p != ',' && i + 1 < sizeof(buf)) {
      buf[i++] = *p++;
    }
    buf[i] = '\0';
    /* Trim and extract name */
    char clean[64];
    trim_dep_name(buf, clean, sizeof(clean));
    if (clean[0]) cb(clean, ud);
    /* skip to next comma */
    while (*p && *p != ',') p++;
  }
}

/* Find inventory index by name; returns -1 if not found. */
static int32_t inv_find_idx(const pkg_inventory_t *inv, const char *name) {
  if (!inv || !name) return -1;
  for (uint32_t i = 0; i < inv->count; i++) {
    if (strcmp(inv->entries[i].name, name) == 0) return (int32_t)i;
  }
  return -1;
}

/* ---- Growable arrays for edges / unresolved ---- */

static int edges_grow(pkg_dag_t *dag) {
  uint32_t new_cap = dag->edge_capacity == 0 ? 4096 : dag->edge_capacity * 2;
  pkg_dag_edge_t *n = (pkg_dag_edge_t *)realloc(
      dag->edges, new_cap * sizeof(*dag->edges));
  if (!n) return -1;
  dag->edges = n;
  dag->edge_capacity = new_cap;
  return 0;
}

static int unres_grow(pkg_dag_t *dag) {
  uint32_t new_cap =
      dag->unresolved_capacity == 0 ? 256 : dag->unresolved_capacity * 2;
  pkg_dag_unresolved_t *n = (pkg_dag_unresolved_t *)realloc(
      dag->unresolved, new_cap * sizeof(*dag->unresolved));
  if (!n) return -1;
  dag->unresolved = n;
  dag->unresolved_capacity = new_cap;
  return 0;
}

/* Callback context for split_deps → adds edges */
typedef struct {
  pkg_dag_t *dag;
  uint32_t from_idx;
  uint8_t is_build_dep;
} split_ctx_t;

static void add_edge_cb(const char *dep_name, void *ud) {
  split_ctx_t *c = (split_ctx_t *)ud;
  int32_t to = inv_find_idx(c->dag->inv, dep_name);
  if (to < 0) {
    /* TOKEN_VAZIO: unresolved dep (external or missing) */
    if (c->dag->unresolved_count >= c->dag->unresolved_capacity) {
      if (unres_grow(c->dag) < 0) return;
    }
    pkg_dag_unresolved_t *u =
        &c->dag->unresolved[c->dag->unresolved_count++];
    u->pkg_idx = c->from_idx;
    strncpy(u->missing_dep, dep_name, sizeof(u->missing_dep) - 1);
    u->missing_dep[sizeof(u->missing_dep) - 1] = '\0';
    return;
  }
  if (c->dag->edge_count >= c->dag->edge_capacity) {
    if (edges_grow(c->dag) < 0) return;
  }
  pkg_dag_edge_t *e = &c->dag->edges[c->dag->edge_count++];
  e->from_idx = c->from_idx;
  e->to_idx = (uint32_t)to;
  e->is_build_dep = c->is_build_dep;
  if (c->is_build_dep) c->dag->total_build_dep_edges++;
  else c->dag->total_depends_edges++;
}

int pkg_dag_build(pkg_dag_t *dag, const pkg_inventory_t *inv) {
  if (!dag || !inv) return -1;
  memset(dag, 0, sizeof(*dag));
  dag->inv = inv;

  /* Parse every build.sh into parallel array */
  dag->parsed =
      (pkg_parser_result_t *)calloc(inv->count, sizeof(pkg_parser_result_t));
  if (!dag->parsed) return -1;
  dag->parsed_count = inv->count;

  for (uint32_t i = 0; i < inv->count; i++) {
    pkg_parser_parse_file(inv->entries[i].path, &dag->parsed[i]);
  }

  /* Build edges from parsed deps */
  for (uint32_t i = 0; i < inv->count; i++) {
    split_ctx_t ctx = {.dag = dag, .from_idx = i, .is_build_dep = 0};
    split_deps(dag->parsed[i].depends_raw, add_edge_cb, &ctx);
    ctx.is_build_dep = 1;
    split_deps(dag->parsed[i].build_depends_raw, add_edge_cb, &ctx);
  }

  /* Build adjacency lists */
  dag->adj = (uint32_t **)calloc(inv->count, sizeof(uint32_t *));
  dag->adj_len = (uint32_t *)calloc(inv->count, sizeof(uint32_t));
  if (!dag->adj || !dag->adj_len) return -1;

  /* First pass: count */
  for (uint32_t e = 0; e < dag->edge_count; e++) {
    dag->adj_len[dag->edges[e].from_idx]++;
  }
  /* Allocate */
  for (uint32_t i = 0; i < inv->count; i++) {
    if (dag->adj_len[i] > 0) {
      dag->adj[i] = (uint32_t *)malloc(dag->adj_len[i] * sizeof(uint32_t));
      if (!dag->adj[i]) return -1;
    }
  }
  /* Second pass: fill */
  uint32_t *pos = (uint32_t *)calloc(inv->count, sizeof(uint32_t));
  if (!pos) return -1;
  for (uint32_t e = 0; e < dag->edge_count; e++) {
    uint32_t f = dag->edges[e].from_idx;
    dag->adj[f][pos[f]++] = dag->edges[e].to_idx;
  }
  free(pos);

  return 0;
}

/* Kahn's algorithm with cycle detection.
 * Depth of a node = 1 + max(depth of any dep). */
int pkg_dag_topo_sort(pkg_dag_t *dag) {
  if (!dag || !dag->inv) return -1;

  uint32_t n = dag->inv->count;
  dag->topo_order = (uint32_t *)calloc(n, sizeof(uint32_t));
  dag->topo_depth = (uint32_t *)calloc(n, sizeof(uint32_t));
  if (!dag->topo_order || !dag->topo_depth) return -1;

  /* Compute in-degree (# of packages that depend on this) */
  uint32_t *in_deg = (uint32_t *)calloc(n, sizeof(uint32_t));
  if (!in_deg) return -1;

  /* Edge u->v means u depends on v; we want to build v first.
   * So in the topological orientation, v has an "in-edge" from u.
   * We reverse: to build order, count how many things point TO each node. */
  for (uint32_t e = 0; e < dag->edge_count; e++) {
    /* v = to_idx is dependency; count things pointing at v means
     * v is depended on by many. But for build order we want leaves first,
     * so we treat edge as dependency-direction (u needs v).
     * Kahn: process nodes with no *outgoing* edges (i.e., no deps) first. */
    (void)e;
  }
  /* Use out-degree instead: adj_len[i] = # deps of i.
   * A node with 0 deps is a leaf → build first. */
  uint32_t *out_deg = (uint32_t *)calloc(n, sizeof(uint32_t));
  if (!out_deg) { free(in_deg); return -1; }
  for (uint32_t i = 0; i < n; i++) out_deg[i] = dag->adj_len[i];

  /* Reverse adjacency (who depends on i) */
  uint32_t *rev_len = (uint32_t *)calloc(n, sizeof(uint32_t));
  if (!rev_len) { free(in_deg); free(out_deg); return -1; }
  for (uint32_t i = 0; i < n; i++) {
    for (uint32_t k = 0; k < dag->adj_len[i]; k++) {
      rev_len[dag->adj[i][k]]++;
    }
  }
  uint32_t **rev = (uint32_t **)calloc(n, sizeof(uint32_t *));
  if (!rev) { free(in_deg); free(out_deg); free(rev_len); return -1; }
  for (uint32_t i = 0; i < n; i++) {
    if (rev_len[i] > 0) {
      rev[i] = (uint32_t *)malloc(rev_len[i] * sizeof(uint32_t));
    }
  }
  uint32_t *rev_pos = (uint32_t *)calloc(n, sizeof(uint32_t));
  for (uint32_t i = 0; i < n; i++) {
    for (uint32_t k = 0; k < dag->adj_len[i]; k++) {
      uint32_t v = dag->adj[i][k];
      rev[v][rev_pos[v]++] = i;
    }
  }
  free(rev_pos);

  /* Queue of leaves (out_deg == 0) */
  uint32_t *queue = (uint32_t *)malloc(n * sizeof(uint32_t));
  if (!queue) { goto cleanup_fail; }
  uint32_t head = 0, tail = 0;
  for (uint32_t i = 0; i < n; i++) {
    if (out_deg[i] == 0) {
      queue[tail++] = i;
      dag->topo_depth[i] = 0;
    }
  }

  while (head < tail) {
    uint32_t u = queue[head++];
    dag->topo_order[dag->topo_count++] = u;

    for (uint32_t k = 0; k < rev_len[u]; k++) {
      uint32_t v = rev[u][k];
      if (out_deg[v] > 0) {
        out_deg[v]--;
        /* Update depth: v's depth is max(v's current, u's + 1) */
        uint32_t new_d = dag->topo_depth[u] + 1;
        if (new_d > dag->topo_depth[v]) dag->topo_depth[v] = new_d;
        if (out_deg[v] == 0) {
          queue[tail++] = v;
          if (dag->topo_depth[v] > dag->max_depth) {
            dag->max_depth = dag->topo_depth[v];
          }
        }
      }
    }
  }

  /* Any node still with out_deg > 0 is part of a cycle */
  uint32_t cycle_nodes = 0;
  for (uint32_t i = 0; i < n; i++) {
    if (out_deg[i] > 0) cycle_nodes++;
  }
  if (cycle_nodes > 0) {
    /* Record as one aggregate "cycle" for now (item #5) */
    dag->cycles =
        (pkg_dag_cycle_t *)calloc(1, sizeof(pkg_dag_cycle_t));
    if (dag->cycles) {
      dag->cycles[0].nodes =
          (uint32_t *)malloc(cycle_nodes * sizeof(uint32_t));
      dag->cycles[0].length = 0;
      if (dag->cycles[0].nodes) {
        for (uint32_t i = 0; i < n; i++) {
          if (out_deg[i] > 0) {
            dag->cycles[0].nodes[dag->cycles[0].length++] = i;
          }
        }
        dag->cycle_count = 1;
      }
    }
  }

  free(queue);
  for (uint32_t i = 0; i < n; i++) free(rev[i]);
  free(rev);
  free(rev_len);
  free(out_deg);
  free(in_deg);
  return 0;

cleanup_fail:
  for (uint32_t i = 0; i < n; i++) free(rev[i]);
  free(rev);
  free(rev_len);
  free(out_deg);
  free(in_deg);
  return -1;
}

const pkg_parser_result_t *pkg_dag_parsed_at(const pkg_dag_t *dag,
                                             uint32_t idx) {
  if (!dag || idx >= dag->parsed_count) return NULL;
  return &dag->parsed[idx];
}

void pkg_dag_write_json(FILE *out, const pkg_dag_t *dag) {
  if (!out || !dag) return;
  fputs("{\n", out);
  fputs("  \"schema\": \"pkg_dag_v1\",\n", out);
  fputs("  \"status\": \"REAL\",\n", out);
  fprintf(out, "  \"node_count\": %u,\n",
          dag->inv ? dag->inv->count : 0);
  fprintf(out, "  \"edge_count\": %u,\n", dag->edge_count);
  fprintf(out, "  \"depends_edges\": %u,\n", dag->total_depends_edges);
  fprintf(out, "  \"build_dep_edges\": %u,\n", dag->total_build_dep_edges);
  fprintf(out, "  \"unresolved_count\": %u,\n", dag->unresolved_count);
  fprintf(out, "  \"cycle_count\": %u,\n", dag->cycle_count);
  fprintf(out, "  \"topo_ordered\": %u,\n", dag->topo_count);
  fprintf(out, "  \"max_depth\": %u\n", dag->max_depth);
  fputs("}\n", out);
}

void pkg_dag_report(FILE *out, const pkg_dag_t *dag) {
  if (!out || !dag) return;
  fprintf(out, "=== REAL Dependency Graph ===\n");
  fprintf(out, "Nodes (packages):         %u\n",
          dag->inv ? dag->inv->count : 0);
  fprintf(out, "Edges total:              %u\n", dag->edge_count);
  fprintf(out, "  DEPENDS edges:          %u\n", dag->total_depends_edges);
  fprintf(out, "  BUILD_DEPENDS edges:    %u\n", dag->total_build_dep_edges);
  fprintf(out, "Unresolved deps:          %u  [TOKEN_VAZIO]\n",
          dag->unresolved_count);
  fprintf(out, "Cycles detected:          %u  %s\n", dag->cycle_count,
          dag->cycle_count > 0 ? "[TOKEN_VAZIO]" : "");
  fprintf(out, "Topological ordered:      %u\n", dag->topo_count);
  fprintf(out, "Max depth:                %u\n", dag->max_depth);

  if (dag->cycle_count > 0 && dag->cycles && dag->cycles[0].length > 0) {
    fprintf(out, "\nCycle members (first %u):\n",
            dag->cycles[0].length > 10 ? 10 : dag->cycles[0].length);
    uint32_t show = dag->cycles[0].length > 10 ? 10 : dag->cycles[0].length;
    for (uint32_t i = 0; i < show; i++) {
      uint32_t idx = dag->cycles[0].nodes[i];
      fprintf(out, "  - %s\n", dag->inv->entries[idx].name);
    }
  }

  if (dag->unresolved_count > 0) {
    fprintf(out, "\nFirst 10 unresolved deps:\n");
    uint32_t show =
        dag->unresolved_count > 10 ? 10 : dag->unresolved_count;
    for (uint32_t i = 0; i < show; i++) {
      fprintf(out, "  %s → %s (missing)\n",
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
    for (uint32_t i = 0; dag->inv && i < dag->inv->count; i++) {
      free(dag->adj[i]);
    }
    free(dag->adj);
  }
  free(dag->adj_len);
  free(dag->topo_order);
  free(dag->topo_depth);
  free(dag->unresolved);
  if (dag->cycles) {
    for (uint32_t i = 0; i < dag->cycle_count; i++) {
      free(dag->cycles[i].nodes);
    }
    free(dag->cycles);
  }
  memset(dag, 0, sizeof(*dag));
}
