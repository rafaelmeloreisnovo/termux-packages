#!/usr/bin/env python3
"""Build and verify a signed development APT snapshot from recovered DEBs."""
import argparse
import datetime
import email.utils
import gzip
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def run(args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def build(source, output, signing, url):
    source, output, signing = map(Path, (source, output, signing))
    output.mkdir(parents=True, exist_ok=False)
    signing.mkdir(parents=True, exist_ok=True, mode=0o700)
    signing.chmod(0o700)
    env=os.environ.copy(); env['GNUPGHOME']=str(signing.resolve())
    run(['gpg','--batch','--pinentry-mode','loopback','--passphrase','','--quick-generate-key',
         'RAFCODEPHI development package snapshot 20261109','ed25519','sign','1y'],env=env)
    result=run(['gpg','--batch','--with-colons','--list-secret-keys'],env=env,capture_output=True,text=True)
    fingerprint=next(l.split(':')[9] for l in result.stdout.splitlines() if l.startswith('fpr:'))
    pub=run(['gpg','--batch','--export',fingerprint],env=env,capture_output=True).stdout
    (output/'rafcodephi-archive-key.gpg').write_bytes(pub)
    parents={}; counts={}; developer_commits=set(); parent_commits={}
    for arch in ('arm','aarch64'):
        receipt=json.loads((source/arch/'receipt.json').read_text())
        if receipt.get('schema') != 'rafcodephi.developer-handoff/v1':
            raise ValueError('developer handoff receipt missing for '+arch)
        if receipt.get('architecture') != arch or receipt.get('claim_allowed') is not False:
            raise ValueError('developer handoff boundary mismatch for '+arch)
        dest=output/'pool/main'/arch;dest.mkdir(parents=True)
        count=0
        for name,meta in receipt['files'].items():
            if not name.startswith('debs/') or not name.endswith('.deb'):continue
            p=source/arch/name
            with p.open('rb') as f:observed=hashlib.file_digest(f,'sha256').hexdigest()
            if observed!=meta['sha256']:raise ValueError('DEB SHA256 mismatch: '+name)
            if p.stat().st_size>95*1024*1024:raise ValueError('DEB exceeds Git blob bound')
            shutil.copyfile(p,dest/p.name);count+=1
        if count<1:raise ValueError('no DEBs for '+arch)
        parents[arch]=receipt['input_artifact_sha256'];counts[arch]=count
        developer_commits.add(receipt['developer_source_commit'])
        parent_commits[arch]=receipt['parent_source_commit']
        index=output/'dists/stable/main'/('binary-'+arch)/'Packages';index.parent.mkdir(parents=True)
        scan=run(['dpkg-scanpackages','-a',arch,str(Path('pool/main')/arch),'/dev/null'],cwd=output,capture_output=True)
        index.write_bytes(scan.stdout)
        index.with_name('Packages.gz').write_bytes(gzip.compress(scan.stdout,compresslevel=9,mtime=0))
    dist=output/'dists/stable'
    lines=['Origin: RAFCODEPHI Development Snapshot','Label: RAFCODEPHI','Suite: stable',
           'Codename: stable','Architectures: arm aarch64','Components: main',
           'Date: '+email.utils.format_datetime(datetime.datetime.now(datetime.timezone.utc)),'SHA256:']
    for p in sorted(dist.rglob('*')):
        if p.is_file():
            data=p.read_bytes();lines.append(f' {hashlib.sha256(data).hexdigest()} {len(data):16} {p.relative_to(dist).as_posix()}')
    (dist/'Release').write_text('\n'.join(lines)+'\n')
    for args in [ ['--clearsign','--output',str(dist/'InRelease'),str(dist/'Release')],
                  ['--detach-sign','--output',str(dist/'Release.gpg'),str(dist/'Release')]]:
        run(['gpg','--batch','--yes','--local-user',fingerprint,*args],env=env)
    run(['gpg','--batch','--verify',str(dist/'InRelease')],env=env)
    key_sha=hashlib.sha256(pub).hexdigest()
    if len(developer_commits) != 1:
        raise ValueError('developer source commit differs between architectures')
    receipt={'schema':'rafcodephi.development-apt-snapshot/v2',
             'source_run':int(os.environ.get('GITHUB_RUN_ID','0')),
             'source_commit':next(iter(developer_commits)),
             'parent_source_commit':parent_commits,'parent_artifact_sha256':parents,
             'package_counts':counts,'repository_url':url,'public_key_sha256':key_sha,
             'signing_fingerprint':fingerprint,'signing_mode':'development_snapshot',
             'profile':'rafcodephi-developer-profile',
             'install_command':'pkg install rafcodephi-developer-profile',
             'claim_allowed_release':False,'device_runtime':'TOKEN_VAZIO'}
    (output/'snapshot.json').write_text(json.dumps(receipt,indent=2)+'\n')
    (output/'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(output).as_posix()+'\n' for p in sorted(output.rglob('*')) if p.is_file()))
    print(json.dumps(receipt))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--source',required=True);p.add_argument('--output',required=True);p.add_argument('--signing',required=True);p.add_argument('--url',required=True);a=p.parse_args()
    build(a.source,a.output,a.signing,a.url)
