import json
import argparse
import os
from pathlib import Path
import subprocess

parser=argparse.ArgumentParser();parser.add_argument('--base',default='/workspace/work/pkg-recovery');parser.add_argument('--repo');parser.add_argument('--url');args=parser.parse_args()
base=Path(args.base).resolve();repo=Path(args.repo).resolve() if args.repo else base/'apt-repo';results={}
for arch in ('arm','aarch64'):
 root=base/('apt-client-'+arch)
 for name in ('etc/apt/sources.list.d','state/lists/partial','cache/archives/partial','log'):(root/name).mkdir(parents=True,exist_ok=True)
 (root/'state/status').touch()
 (root/'etc/apt/sources.list').write_text(f'deb [arch={arch} signed-by={repo}/rafcodephi-archive-key.gpg] {args.url or 'file:'+str(repo)} stable main\n')
 opts=['-o',f'Dir::Etc::sourcelist={root}/etc/apt/sources.list','-o',f'Dir::Etc::sourceparts={root}/etc/apt/sources.list.d','-o','Dir::Etc::main=/dev/null','-o','Dir::Etc::parts=/nonexistent',
       '-o',f'Dir::State={root}/state','-o',f'Dir::State::status={root}/state/status','-o',f'Dir::State::lists={root}/state/lists',
       '-o',f'Dir::Cache={root}/cache','-o',f'Dir::Cache::archives={root}/cache/archives','-o',f'Dir::Log={root}/log',
       '-o',f'APT::Architecture={arch}','-o',f'APT::Architectures={arch}','-o','Acquire::Languages=none','-o','APT::Sandbox::User='+os.environ.get('USER','runner')]
 with (base/('apt-resolution-'+arch+'.log')).open('w') as log:
  update=subprocess.run(['apt-get',*opts,'-o','APT::Update::Error-Mode=any','update'],stdout=log,stderr=subprocess.STDOUT)
  install=subprocess.run(['apt-get',*opts,'--download-only','--no-install-recommends','-y','install','nano','ninja','python'],stdout=log,stderr=subprocess.STDOUT) if update.returncode==0 else None
 results[arch]={'signed_update_exit':update.returncode,'resolve_and_download_exit':None if install is None else install.returncode,'packages':['nano','ninja','python'],'scope':'HOST_APT_ANDROID_PACKAGES_DOWNLOAD_ONLY','device_runtime':'TOKEN_VAZIO'}
 print(arch,results[arch],flush=True)
(base/'apt-resolution.json').write_text(json.dumps(results,indent=2)+'\n')
raise SystemExit(0 if all(x['resolve_and_download_exit']==0 for x in results.values()) else 1)
