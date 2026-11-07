#include "raf_dag.h"
#include <stdio.h>
/* Independent oracle for DAGs constructed from u<v edges, random insertions. */
static raf_dag g;
static raf_u16 expected[64],positions[64];
static raf_u32 seed=0xC001D00DU;
static unsigned errors;
static raf_u32 next_random(void){
 seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;
}
#define CK(c) do {if(!(c)){++errors;printf("line %d case %u\n",__LINE__,trial);}}while(0)
static void check_property(unsigned trial){
 raf_u32 n=2U+next_random()%62U,i,j;
 CK(raf_dag_init(&g,n)==0);
 for(i=0;i<n;++i)expected[i]=0U;
 for(i=0;i<n*4U;++i){
  raf_u32 a=next_random()%n,b=next_random()%n;
  if(a==b)continue;
  if(a>b){raf_u32 t=a;a=b;b=t;}
  CK(raf_dag_add(&g,a,b)==0);
  if((next_random()&7U)==0U)CK(raf_dag_add(&g,a,b)==0);
 }
 for(i=0;i<n;++i){
  for(j=0;j<g.edges;++j){
   if(g.from[j]==i){
    raf_u32 b=g.to[j];
    if(expected[b]<(raf_u16)(expected[i]+1U))expected[b]=(raf_u16)(expected[i]+1U);
   }
  }
 }
 CK(raf_dag_finalize(&g)==0);
 CK(raf_dag_solve(&g)==0);
 for(i=0;i<n;++i){
  CK(g.depth[i]==expected[i]);
  positions[g.order[i]]=(raf_u16)i;
 }
 for(i=0;i<g.edges;++i)CK(positions[g.from[i]]<positions[g.to[i]]);
 CK(raf_dag_solve(&g)==0);
 for(i=0;i<n;++i)CK(g.depth[i]==expected[i]);
}
static void check_corruption(unsigned trial){
 CK(raf_dag_init(&g,3)==0);
 CK(raf_dag_add(&g,0,1)==0);
 CK(raf_dag_add(&g,1,2)==0);
 CK(raf_dag_finalize(&g)==0);
 CK(raf_dag_solve(&g)==0);
 g.offsets[0]=1U;
 CK(raf_dag_solve(&g)==RAF_DAG_BAD_STATE&&g.solved==0);
 g.offsets[0]=0U;
 g.offsets[1]=3U;
 CK(raf_dag_solve(&g)==RAF_DAG_BAD_STATE);
 g.offsets[1]=1U;
 g.adjacency[0]=4U;
 CK(raf_dag_solve(&g)==RAF_DAG_BAD_STATE);
 g.adjacency[0]=1U;
 CK(raf_dag_solve(&g)==0);
}
int main(void){
 unsigned trial;
 for(trial=0;trial<512U;++trial)check_property(trial);
 trial=512U;check_corruption(trial);
 if(!errors)puts("RAFCODEPHI PROPERTY TEST PASS: 512 seeded DAGs, malformed CSR guards");
 return errors?1:0;
}
