#include "raf_dag.h"
#include <stdio.h>
/* Hosted test ONLY. The production core does not include stdio/libc. */
static raf_dag g;
static int fails;
#define CK(x) do{if(!(x)){++fails;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)
int main(void){
 raf_u32 i;
 CK(raf_dag_init(&g,0)==RAF_DAG_BAD_INPUT);
 CK(raf_dag_init(&g,RAF_DAG_MAX_NODES+1U)==RAF_DAG_BAD_INPUT);
 CK(raf_dag_init(&g,5)==0);
 CK(raf_dag_solve(&g)==RAF_DAG_BAD_STATE);
 CK(raf_dag_add(&g,0,1)==0);
 CK(raf_dag_add(&g,2,4)==0);
 CK(raf_dag_add(&g,0,3)==0);
 CK(raf_dag_add(&g,0,1)==0);
 CK(raf_dag_add(&g,1,4)==0);
 CK(raf_dag_add(&g,5,0)==RAF_DAG_BAD_INPUT);
 CK(raf_dag_finalize(&g)==0);
 CK(g.offsets[0]==0&&g.offsets[1]==3&&g.offsets[2]==4);
 CK(g.offsets[3]==5&&g.offsets[5]==5);
 CK(g.adjacency[0]==1&&g.adjacency[1]==3&&g.adjacency[2]==1);
 CK(raf_dag_add(&g,0,1)==RAF_DAG_BAD_STATE);
 CK(raf_dag_finalize(&g)==RAF_DAG_BAD_STATE);
 CK(raf_dag_fingerprint(&g)!=0U);
 CK(raf_dag_solve(&g)==0&&g.depth[4]==2&&g.depth[3]==1);
 CK(raf_dag_solve(&g)==0);
 CK(raf_dag_init(&g,3)==0);
 CK(raf_dag_add(&g,0,1)==0&&raf_dag_add(&g,1,2)==0);
 CK(raf_dag_add(&g,2,0)==0);
 CK(raf_dag_finalize(&g)==0);
 CK(raf_dag_solve(&g)==RAF_DAG_CYCLE&&!g.solved);
 CK(raf_dag_init(&g,2)==0);
 for(i=0;i<RAF_DAG_MAX_EDGES;++i)CK(raf_dag_add(&g,0,1)==0);
 CK(raf_dag_add(&g,0,1)==RAF_DAG_CAPACITY);
 CK(raf_dag_finalize(&g)==0&&g.offsets[2]==RAF_DAG_MAX_EDGES);
 CK(raf_dag_solve(&g)==0&&g.depth[1]==1);
 CK(raf_dag_init(&g,RAF_DAG_MAX_NODES)==0);
 for(i=0;i+1U<RAF_DAG_MAX_NODES;++i)CK(raf_dag_add(&g,i,i+1U)==0);
 CK(raf_dag_finalize(&g)==0);
 CK(raf_dag_solve(&g)==0&&g.depth[RAF_DAG_MAX_NODES-1U]==RAF_DAG_MAX_NODES-1U);
 CK(raf_dag_init(&g,4)==0);
 CK(raf_dag_finalize(&g)==0);
 CK(raf_dag_solve(&g)==0&&g.order[0]==0&&g.order[3]==3);
 if(!fails)puts("RAFCODEPHI DAG PASS_SCOPED: CSR, cycles, duplicates, capacity, depth");
 return fails?1:0;
}
