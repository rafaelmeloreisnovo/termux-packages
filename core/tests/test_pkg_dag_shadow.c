#include "../pkg_dag_shadow.h"
#include <stdio.h>
static raf_dag scratch;
static pkg_inventory_t inventory;
static pkg_dag_t dag;
static pkg_dag_edge_t edges[4];
static uint32_t depths[4],order[4];
static int failures;
#define CHECK(x) do{if(!(x)){++failures;printf("FAIL:%u:%s\n",(unsigned)__LINE__,#x);}}while(0)
static void setup(void){
 inventory.count=4;
 dag.inv=&inventory;dag.edges=edges;dag.edge_count=3;
 dag.topo_order=order;dag.topo_depth=depths;
 dag.topo_count=4;dag.max_depth=2;
 /* Hosted: consumer -> dependency. */
 edges[0].from_idx=0;edges[0].to_idx=1;edges[0].is_build_dep=0;
 edges[1].from_idx=1;edges[1].to_idx=2;edges[1].is_build_dep=0;
 edges[2].from_idx=0;edges[2].to_idx=3;edges[2].is_build_dep=1;
 depths[0]=2;depths[1]=1;depths[2]=0;depths[3]=0;
 order[0]=2;order[1]=3;order[2]=1;order[3]=0;
}
int main(void){
 raf_dag_shadow_result r;
 setup();
 CHECK(raf_dag_shadow_compare(&dag,&scratch,&r)==RAF_SHADOW_PASS);
 CHECK(r.nodes==4&&r.edges==3&&r.max_depth==2);
 CHECK(r.fingerprint!=0U);
 /* Independent prerequisites can appear in either order. */
 order[0]=3;order[1]=2;
 CHECK(raf_dag_shadow_compare(&dag,&scratch,&r)==RAF_SHADOW_PASS);
 setup();depths[0]=1;
 CHECK(raf_dag_shadow_compare(&dag,&scratch,&r)==RAF_SHADOW_DIVERGENCE);
 setup();order[0]=0;
 CHECK(raf_dag_shadow_compare(&dag,&scratch,&r)==RAF_SHADOW_DIVERGENCE);
 setup();edges[0].to_idx=99;
 CHECK(raf_dag_shadow_compare(&dag,&scratch,&r)==RAF_SHADOW_INVALID);
 setup();dag.edge_count=RAF_DAG_MAX_EDGES+1U;
 CHECK(raf_dag_shadow_compare(&dag,&scratch,&r)==RAF_SHADOW_BLOCKED_CAPACITY);
 setup();inventory.count=RAF_DAG_MAX_NODES+1U;
 CHECK(raf_dag_shadow_compare(&dag,&scratch,&r)==RAF_SHADOW_BLOCKED_CAPACITY);
 setup();inventory.count=2;dag.edge_count=2;
 edges[0].from_idx=0;edges[0].to_idx=1;
 edges[1].from_idx=1;edges[1].to_idx=0;
 dag.topo_count=0;
 CHECK(raf_dag_shadow_compare(&dag,&scratch,&r)==RAF_SHADOW_BLOCKED_CYCLE);
 dag.topo_count=2;
 CHECK(raf_dag_shadow_compare(&dag,&scratch,&r)==RAF_SHADOW_DIVERGENCE);
 if(!failures)puts("RAF_SHADOW_TEST_PASS: direction, depth, order, overflow, cycles");
 return failures?1:0;
}
