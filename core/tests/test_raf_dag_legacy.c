#include "../dep_resolver.h"
#include <stdio.h>
static struct termux_dep_resolver r;
static int failures;
#define CHECK(x) do {if(!(x)){++failures;printf("FAIL %d %s\n",__LINE__,#x);}}while(0)
int main(void) {
 CHECK(termux_dep_resolver_init(&r,5)==0);
 CHECK(termux_dep_graph_add_edge(&r.graph,0,2)==0);
 CHECK(termux_dep_graph_add_edge(&r.graph,0,1)==0);
 CHECK(termux_dep_graph_add_edge(&r.graph,1,3)==0);
 CHECK(termux_dep_graph_add_edge(&r.graph,2,3)==0);
 CHECK(termux_dep_graph_finalize_csr(&r.graph)==0);
 CHECK(r.graph.deps_row[1]==2&&r.graph.deps_row[4]==4);
 CHECK(termux_dep_resolver_build_layers(&r)==0);
 CHECK(r.graph.depths[3]==2);
 CHECK(termux_dep_graph_validate(&r.graph)==0);
 CHECK(termux_dep_graph_compute_crc32c(&r.graph)!=0);
 termux_dep_resolver_destroy(&r);
 CHECK(termux_dep_resolver_init(&r,2)==0);
 for(int i=0;i<300;++i)CHECK(termux_dep_graph_add_edge(&r.graph,0,1)==0);
 CHECK(termux_dep_graph_finalize_csr(&r.graph)==0);
 CHECK(termux_dep_resolver_build_layers(&r)==0);
 termux_dep_resolver_destroy(&r);
 CHECK(termux_dep_resolver_init(&r,3)==0);
 CHECK(termux_dep_graph_add_edge(&r.graph,0,1)==0);
 CHECK(termux_dep_graph_add_edge(&r.graph,1,0)==0);
 CHECK(termux_dep_graph_finalize_csr(&r.graph)==0);
 CHECK(termux_dep_resolver_build_layers(&r)==-3);
 termux_dep_resolver_destroy(&r);
 CHECK(termux_dep_resolver_init(&r,70)==0);
 CHECK(termux_dep_graph_finalize_csr(&r.graph)==0);
 CHECK(termux_dep_resolver_build_layers(&r)==-5);
 for (int layer=0;layer<TERMUX_TOROIDAL_LAYERS;++layer)
   CHECK(r.layers[layer].pkg_count==0);
 termux_dep_resolver_destroy(&r);
 CHECK(termux_dep_resolver_init(&r,257)==0);
 for(int i=0;i<256;++i)
   CHECK(termux_dep_graph_add_edge(&r.graph,(uint16_t)i,(uint16_t)(i+1))==0);
 CHECK(termux_dep_graph_finalize_csr(&r.graph)==0);
 CHECK(termux_dep_resolver_build_layers(&r)==-4);
 termux_dep_resolver_destroy(&r);
 if(!failures)puts("Legacy adapter PASS_SCOPED: CSR, indegree 300, cycle, layer bound");
 return failures!=0;
}
