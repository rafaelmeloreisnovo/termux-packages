#include "pkg_dag_shadow.h"
/* Read-only verification of the same observed static package projection.
 * Does not read files, allocate memory or invoke OS/platform services.
 */
int raf_dag_shadow_compare(const pkg_dag_t *source,raf_dag *scratch,
                           raf_dag_shadow_result *out){
 raf_u32 n,i,max_depth;
 int rc;
 if(!out)return RAF_SHADOW_INVALID;
 out->nodes=0U;out->edges=0U;out->max_depth=0U;
 out->fingerprint=0U;out->status=RAF_SHADOW_INVALID;
 if(!source||!scratch||!source->inv)return out->status;
 n=source->inv->count;
 out->nodes=n;out->edges=source->edge_count;
 if(!n||source->topo_count>n||
    (source->edge_count&&!source->edges)||
    !source->topo_order||!source->topo_depth)return out->status;
 if(n>RAF_DAG_MAX_NODES||source->edge_count>RAF_DAG_MAX_EDGES){
  out->status=RAF_SHADOW_BLOCKED_CAPACITY;
  return out->status;
 }
 for(i=0;i<source->edge_count;++i)
  if(source->edges[i].from_idx>=n||source->edges[i].to_idx>=n)
   return out->status;
 if(raf_dag_init(scratch,n)!=RAF_DAG_OK)return out->status;
 for(i=0;i<source->edge_count;++i){
  const pkg_dag_edge_t *e=&source->edges[i];
  if(raf_dag_add(scratch,e->to_idx,e->from_idx)!=RAF_DAG_OK)
   return out->status;
 }
 if(raf_dag_finalize(scratch)!=RAF_DAG_OK)return out->status;
 out->fingerprint=raf_dag_fingerprint(scratch);
 rc=raf_dag_solve(scratch);
 if(rc==RAF_DAG_CYCLE){
  out->status=(source->topo_count<n)?RAF_SHADOW_BLOCKED_CYCLE:
                                       RAF_SHADOW_DIVERGENCE;
  return out->status;
 }
 if(rc!=RAF_DAG_OK||source->topo_count!=n){
  out->status=RAF_SHADOW_DIVERGENCE;
  return out->status;
 }
 /* Validate legacy order as a permutation and valid prerequisite order.
  * Reuse scratch indegree after solve; it is not needed for depth proof.
  */
 for(i=0;i<n;++i)scratch->indegree[i]=0xffffU;
 for(i=0;i<n;++i){
  raf_u32 node=source->topo_order[i];
  if(node>=n||scratch->indegree[node]!=0xffffU){
   out->status=RAF_SHADOW_DIVERGENCE;
   return out->status;
  }
  scratch->indegree[node]=(raf_u16)i;
 }
 for(i=0;i<source->edge_count;++i){
  const pkg_dag_edge_t *e=&source->edges[i];
  if(scratch->indegree[e->to_idx]>=scratch->indegree[e->from_idx]){
   out->status=RAF_SHADOW_DIVERGENCE;
   return out->status;
  }
 }
 max_depth=0U;
 for(i=0;i<n;++i){
  if(scratch->depth[i]!=source->topo_depth[i]){
   out->status=RAF_SHADOW_DIVERGENCE;
   return out->status;
  }
  if(scratch->depth[i]>max_depth)max_depth=scratch->depth[i];
 }
 out->max_depth=max_depth;
 out->status=(max_depth==source->max_depth)?
              RAF_SHADOW_PASS:RAF_SHADOW_DIVERGENCE;
 return out->status;
}
