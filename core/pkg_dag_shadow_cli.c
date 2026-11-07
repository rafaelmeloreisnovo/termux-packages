/* Hosted file I/O adapter only; the DAG kernel has no libc or syscalls. */
#include "pkg_dag_shadow.h"
#include "pkg_scanner.h"
#include <stdio.h>
static raf_dag scratch;
int main(int argc,char **argv){
 pkg_inventory_t inventory={0};
 pkg_dag_t dag={0};
 raf_dag_shadow_result r;
 int ret=2;
 int built=0;
 if(argc!=2){
  fprintf(stderr,"Usage: pkg-real-dag-shadow <repo-root>\n");
  return 64;
 }
 if(pkg_inventory_init(&inventory,16)<0){
  fprintf(stderr,"BLOCKED_INVENTORY_INIT\n");
  return ret;
 }
 if(pkg_inventory_scan_all(&inventory,argv[1])<0||
    !pkg_inventory_is_complete(&inventory)){
  fprintf(stderr,"BLOCKED_INCOMPLETE_INVENTORY\n");
  goto cleanup;
 }
 built=1;
 if(pkg_dag_build(&dag,&inventory)<0){
  fprintf(stderr,"BLOCKED_DAG_BUILD\n");
  goto cleanup;
 }
 if(dag.parse_failures||dag.allocation_failures||
    dag.dependency_field_overflows){
  fprintf(stderr,"BLOCKED_STATIC_PARSE_ERRORS=%u\n",
   dag.parse_failures+dag.allocation_failures+
   dag.dependency_field_overflows);
  goto cleanup;
 }
 if(pkg_dag_topo_sort(&dag)<0){
  fprintf(stderr,"BLOCKED_HOSTED_TOPO\n");
  goto cleanup;
 }
 int result=raf_dag_shadow_compare(&dag,&scratch,&r);
 fprintf(stdout,"schema=rafcodephi.dag-shadow/v1\n");
 fprintf(stdout,"projection=OBSERVED_LIMITED\n");
 fprintf(stdout,"nodes=%u edges=%u unresolved=%u alternatives=%u\n",
  r.nodes,r.edges,dag.unresolved_count,dag.alternative_dep_fields);
 fprintf(stdout,"max_depth=%u fingerprint_noncrypto=%08x\n",
  r.max_depth,r.fingerprint);
 fprintf(stdout,"comparison_state=%s\n",
  result==RAF_SHADOW_PASS?"PASS_SCOPED":
  result==RAF_SHADOW_BLOCKED_CYCLE?"BLOCKED_CYCLE":
  result==RAF_SHADOW_BLOCKED_CAPACITY?"BLOCKED_CAPACITY":
  result==RAF_SHADOW_DIVERGENCE?"DIVERGENCE":"INVALID");
 fprintf(stdout,"claim_allowed_device_runtime=false\n");
 ret=(result==RAF_SHADOW_PASS)?0:3;
cleanup:
 if(built)pkg_dag_free(&dag);
 pkg_inventory_free(&inventory);
 return ret;
}
