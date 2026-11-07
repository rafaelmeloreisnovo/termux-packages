#ifndef RAF_DAG_H
#define RAF_DAG_H
/* RAFCODEPhi: freestanding, caller-owned bounded directed graph.
 * No hosted headers, libc, heap, syscalls or mutable global state.
 * Edge means prerequisite -> consumer.
 */
#if !defined(__SIZEOF_SHORT__) || !defined(__SIZEOF_INT__) || !defined(__CHAR_BIT__)
#error "Unsupported integer model"
#endif
#if __SIZEOF_SHORT__ != 2 || __SIZEOF_INT__ != 4 || __CHAR_BIT__ != 8
#error "Requires 8-bit byte, 16-bit short, 32-bit int"
#endif
typedef unsigned short raf_u16;
typedef unsigned int raf_u32;
enum {
 RAF_DAG_MAX_NODES=2057, RAF_DAG_MAX_EDGES=14336,
 RAF_DAG_OK=0, RAF_DAG_BAD_INPUT=-1, RAF_DAG_BAD_STATE=-2,
 RAF_DAG_CAPACITY=-3, RAF_DAG_CYCLE=-4
};
typedef struct raf_dag {
 raf_u16 nodes,edges,sealed,solved;
 raf_u16 from[RAF_DAG_MAX_EDGES],to[RAF_DAG_MAX_EDGES];
 raf_u16 offsets[RAF_DAG_MAX_NODES+1],adjacency[RAF_DAG_MAX_EDGES];
 raf_u16 cursor[RAF_DAG_MAX_NODES],indegree[RAF_DAG_MAX_NODES];
 raf_u16 depth[RAF_DAG_MAX_NODES],order[RAF_DAG_MAX_NODES];
} raf_dag;
int raf_dag_init(raf_dag *g,raf_u32 nodes);
int raf_dag_add(raf_dag *g,raf_u32 prerequisite,raf_u32 consumer);
int raf_dag_finalize(raf_dag *g);
int raf_dag_solve(raf_dag *g);
raf_u32 raf_dag_fingerprint(const raf_dag *g);
#endif
