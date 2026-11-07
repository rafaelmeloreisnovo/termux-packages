#include "raf_dag.h"
int raf_dag_init(raf_dag *g,raf_u32 nodes){
 if(!g||nodes==0U||nodes>RAF_DAG_MAX_NODES)return RAF_DAG_BAD_INPUT;
 g->nodes=(raf_u16)nodes;g->edges=0U;g->sealed=0U;g->solved=0U;
 return RAF_DAG_OK;
}
int raf_dag_add(raf_dag *g,raf_u32 prerequisite,raf_u32 consumer){
 if(!g||!g->nodes||prerequisite>=g->nodes||consumer>=g->nodes)
  return RAF_DAG_BAD_INPUT;
 if(g->sealed)return RAF_DAG_BAD_STATE;
 if(g->edges==RAF_DAG_MAX_EDGES)return RAF_DAG_CAPACITY;
 g->from[g->edges]=(raf_u16)prerequisite;
 g->to[g->edges]=(raf_u16)consumer;
 ++g->edges;
 return RAF_DAG_OK;
}
/* Two-pass stable CSR: count, prefix sum, then scatter.
 * No pointer aliasing or heap allocation.
 */
int raf_dag_finalize(raf_dag *g){
 raf_u32 i,cursor,n;
 if(!g||!g->nodes||g->nodes>RAF_DAG_MAX_NODES||g->edges>RAF_DAG_MAX_EDGES)
  return RAF_DAG_BAD_INPUT;
 if(g->sealed)return RAF_DAG_BAD_STATE;
 n=g->nodes;
 for(i=0;i<=n;++i)g->offsets[i]=0U;
 for(i=0;i<g->edges;++i){
  if(g->from[i]>=n||g->to[i]>=n)return RAF_DAG_BAD_INPUT;
  ++g->offsets[(raf_u32)g->from[i]+1U];
 }
 cursor=0U;
 for(i=0;i<n;++i){
  cursor+=g->offsets[i+1U];
  g->offsets[i+1U]=(raf_u16)cursor;
  g->cursor[i]=g->offsets[i];
 }
 if(cursor!=g->edges)return RAF_DAG_BAD_INPUT;
 for(i=0;i<g->edges;++i)g->adjacency[g->cursor[g->from[i]]++]=g->to[i];
 g->sealed=1U;
 return RAF_DAG_OK;
}
/* Kahn traversal: duplicates are legal; 16-bit depths cover 2057 nodes.
 * A cycle fails closed; no silent truncation or geometric GCD rejection.
 */
int raf_dag_solve(raf_dag *g){
 raf_u32 i,head,tail,n;
 if(!g||!g->sealed)return RAF_DAG_BAD_STATE;
 n=g->nodes;
 for(i=0;i<n;++i){g->indegree[i]=0U;g->depth[i]=0U;}
 for(i=0;i<g->edges;++i)++g->indegree[g->to[i]];
 head=0U;tail=0U;
 for(i=0;i<n;++i)if(g->indegree[i]==0U)g->order[tail++]=(raf_u16)i;
 g->solved=0U;
 while(head<tail){
  raf_u32 u=g->order[head++];
  for(i=g->offsets[u];i<g->offsets[u+1U];++i){
   raf_u32 v=g->adjacency[i];
   raf_u16 candidate=(raf_u16)(g->depth[u]+1U);
   if(candidate>g->depth[v])g->depth[v]=candidate;
   --g->indegree[v];
   if(g->indegree[v]==0U)g->order[tail++]=(raf_u16)v;
  }
 }
 if(head!=n)return RAF_DAG_CYCLE;
 g->solved=1U;
 return RAF_DAG_OK;
}
/* Deterministic non-cryptographic fingerprint; not artifact custody. */
raf_u32 raf_dag_fingerprint(const raf_dag *g){
 raf_u32 h=2166136261U,i;
 if(!g||!g->sealed)return 0U;
 h=(h^g->nodes)*16777619U;h=(h^g->edges)*16777619U;
 for(i=0;i<g->edges;++i){
  h=(h^g->from[i])*16777619U;h=(h^g->to[i])*16777619U;
 }
 return h;
}
