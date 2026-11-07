#ifndef RAF_PKG_DAG_SHADOW_H
#define RAF_PKG_DAG_SHADOW_H
/* Hosted adapter contract. The underlying raf_dag.c remains freestanding. */
#include "pkg_dag.h"
#include "raf_dag/raf_dag.h"
enum {
 RAF_SHADOW_PASS=0,
 RAF_SHADOW_BLOCKED_CYCLE=2,
 RAF_SHADOW_BLOCKED_CAPACITY=3,
 RAF_SHADOW_INVALID=4,
 RAF_SHADOW_DIVERGENCE=5
};
typedef struct raf_dag_shadow_result {
 raf_u32 nodes,edges,max_depth,fingerprint;
 int status;
} raf_dag_shadow_result;
/* pkg_dag edges: consumer -> dependency; raf_dag: prerequisite -> consumer.
 * Scratch is caller-owned. The comparator allocates nothing.
 */
int raf_dag_shadow_compare(const pkg_dag_t *source,raf_dag *scratch,
                           raf_dag_shadow_result *out);
#endif
