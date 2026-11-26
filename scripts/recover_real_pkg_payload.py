#!/usr/bin/env python3
"""Recover exact existing bootstrap and DEBs without rebuilding or enabling APT."""
import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import tarfile
import zipfile


def safe_name(name):
    p = PurePosixPath(name)
    if p.is_absolute() or '..' in p.parts or '\\' in name:
        raise ValueError('unsafe archive member')
    return p


def recover(archive, destination, arch, expected):
    archive, destination = Path(archive), Path(destination)
    with archive.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    if digest != expected:
        raise ValueError('artifact SHA256 mismatch')
    destination.mkdir(parents=True, exist_ok=False)
    written = {}
    def emit(name, stream):
        if name in written:
            raise ValueError('duplicate selected output: ' + name)
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        h = hashlib.sha256()
        with target.open('xb') as out:
            while chunk := stream.read(1024 * 1024):
                h.update(chunk)
                out.write(chunk)
        written[name] = {'sha256': h.hexdigest(), 'bytes': target.stat().st_size}
    wanted = {f'rafcodephi-bootstrap-{arch}.zip', 'RAFCODEPHI_REAL_BOOTSTRAP_MANIFEST.txt'}
    with zipfile.ZipFile(archive) as z:
        members = z.infolist()
        if len({x.filename for x in members}) != len(members):
            raise ValueError('duplicate artifact member')
        for m in members:
            p = safe_name(m.filename)
            if p.name in wanted:
                if m.file_size > 512 * 1024 * 1024:
                    raise ValueError('bootstrap member exceeds bound')
                with z.open(m) as stream:
                    emit('bootstrap/' + p.name, stream)
        missing = wanted - {Path(k).name for k in written}
        if missing:
            raise ValueError('missing bootstrap members: ' + str(sorted(missing)))
        # The producer transports the same DEBs in two TARs. Select one,
        # preserving its directory identity; never extract links or paths.
        tars = [m for m in members if PurePosixPath(m.filename).name == 'rafcodephi-debs.tar']
        if len(tars) != 1:
            raise ValueError('missing or ambiguous DEB transport')
        with z.open(tars[0]) as raw, tarfile.open(fileobj=raw, mode='r|') as t:
            for m in t:
                p = safe_name(m.name)
                if m.isdir():
                    continue
                if not m.isfile():
                    raise ValueError('non-regular DEB transport member')
                if len(p.parts) != 3 or p.parts[:2] != ('debs', arch):
                    raise ValueError('unexpected DEB transport path: ' + str(p))
                if p.suffix != '.deb' and p.name != 'SHA256SUMS':
                    raise ValueError('unexpected DEB transport file')
                if m.size > 512 * 1024 * 1024:
                    raise ValueError('DEB member exceeds bound')
                emit('debs/' + p.name, t.extractfile(m))
    if not any(k.endswith('.deb') for k in written):
        raise ValueError('empty DEB set')
    receipt = {'schema': 'rafcodephi.recovered-handoff/v1', 'architecture': arch,
               'input_artifact_sha256': digest, 'files': written,
               'source_run': 41904631252,
               'source_commit': 'e7f23f46d039d04577c035457c920047f03dc2a1',
               'device_runtime': 'TOKEN_VAZIO', 'claim_allowed': False,
               'apt_repository_enabled': False}
    (destination / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps({'arch': arch, 'files': len(written), 'state': 'RECOVERED_UNVALIDATED'}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--archive', required=True)
    parser.add_argument('--destination', required=True)
    parser.add_argument('--arch', choices=('arm', 'aarch64'), required=True)
    parser.add_argument('--sha256', required=True)
    args = parser.parse_args()
    recover(args.archive, args.destination, args.arch, args.sha256)
