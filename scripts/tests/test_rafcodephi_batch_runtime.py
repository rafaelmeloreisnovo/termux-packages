#!/usr/bin/env python3
"""Execute candidate Bash with offline command doubles; never run real APT.

SPDX-License-Identifier: Apache-2.0
Hosted test adapter, not a freestanding runtime or package/device proof.
"""
from pathlib import Path
import json
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
text = (ROOT / '.github/workflows/rafcodephi-batch-pipeline.yml').read_text()
for required in (
    '      - name: Materialize signed dual-architecture APT repository\n        id: signed_repository',
    '          REPOSITORY_OUTCOME: ${{ steps.signed_repository.outcome }}',
    "      - 'scripts/tests/test_rafcodephi_batch_runtime.py'",
    "      - 'scripts/rafcodephi/batch-guard/**'",
    '          bash scripts/rafcodephi/batch-guard/check.sh',
):
    assert required in text, ('MISSING_WORKFLOW_BINDING', required)
steps = {}
name = None
collect = False
for line in text.splitlines():
    if line.startswith('      - name: '):
        name = line.split(': ', 1)[1]
        collect = False
    elif line == '        run: |':
        collect = True
        steps[name] = []
    elif collect and (line.startswith('          ') or not line.strip()):
        steps[name].append(line[10:] if line else '')
    elif collect:
        collect = False

apt = '\n'.join(steps['Resolve isolated APT for one architecture'])
bundle = '\n'.join(steps['Bundle portable signed APT repository evidence'])
cases = 0
with tempfile.TemporaryDirectory(prefix='raf-batch-test-') as directory:
    root = Path(directory)
    bindir = root / 'bin'
    bindir.mkdir()
    stub = bindir / 'stub'
    stub.write_text('''#!/bin/bash
cmd=${0##*/}
printf '%s %s\\n' "$cmd" "$*" >> "$CALL_LOG"
if [[ "$cmd" == tee ]]; then
  if [[ "$FAIL_AT" == tee ]]; then /bin/cat >/dev/null; exit 74; fi
  exec /usr/bin/tee "$@"
fi
if [[ "$cmd:$*" == *"$FAIL_AT"* && -n "$FAIL_AT" ]]; then exit 73; fi
case "$cmd" in
  dpkg-deb)
    case "${@: -1}" in
      Package) printf 'fixture\\n' ;;
      Architecture) printf '%s\\n' "$TEST_ARCH" ;;
    esac ;;
esac
''')
    stub.chmod(0o755)
    for command in ('apt-get', 'apt-cache', 'dpkg-deb', 'tee'):
        (bindir / command).symlink_to(stub)
    env = dict(os.environ, PATH=str(bindir) + os.pathsep + os.environ['PATH'],
               CALL_LOG=str(root / 'calls'), RAFCODEPHI_APT_SIGNING_MODE='ephemeral-test',
               RAFCODEPHI_APT_SIGNING_FINGERPRINT='A' * 40)
    repo = root / 'publish/repo'
    for arch in ('arm', 'aarch64'):
        package_dir = repo / ('dists/stable/main/binary-' + arch)
        package_dir.mkdir(parents=True)
        (repo / ('pool/main/' + arch)).mkdir(parents=True)
        (repo / ('pool/main/' + arch + '/fixture.deb')).write_bytes(b'fixture-not-a-deb')
        (package_dir / 'Packages').write_text('Package: fixture\nArchitecture: ' + arch +
            '\nFilename: pool/main/' + arch + '/fixture.deb\n')
        for failure in ('', ' update', 'apt-cache:', ' install fixture', 'dpkg-deb:', 'tee'):
            env.update(FAIL_AT=failure, TEST_ARCH=arch)
            script = apt.replace('${{ matrix.arch }}', arch).replace(
                '/tmp/rafcodephi-apt-publish', str(root / 'publish')).replace(
                '/tmp/rafcodephi-apt-client-', str(root / 'client-'))
            result = subprocess.run(['bash', '-c', script], cwd=root, env=env,
                                    capture_output=True, text=True, timeout=10)
            report = json.loads((root / ('build/reports/rafcodephi-isolated-apt-' + arch + '-status.json')).read_text())
            # grep is the rightmost failing command in the dpkg-deb pipeline.
            expected = 74 if failure == 'tee' else 1 if failure == 'dpkg-deb:' else 73 if failure else 0
            assert result.returncode == expected, (arch, failure, result.returncode, result.stderr)
            assert report['exit_code'] == expected, (failure, report)
            assert report['state'] == ('BLOCKED' if failure else 'PASS'), (failure, report)
            assert report['claim_allowed_release'] is False
            cases += 1
    for outcome in ('failure', 'cancelled', 'skipped', '', 'success'):
        env['REPOSITORY_OUTCOME'] = outcome
        script = bundle.replace('/tmp/rafcodephi-apt-publish', str(root / 'publish'))
        result = subprocess.run(['bash', '-c', script], cwd=root, env=env,
                                capture_output=True, text=True, timeout=10)
        assert result.returncode == 0, result.stderr
        state = (root / 'build/reports/rafcodephi-dualarch-signed-apt-repository.state').read_text()
        archive = root / 'build/reports/rafcodephi-dualarch-signed-apt-repository.tar'
        assert ('state=PASS\n' in state) == (outcome == 'success'), (outcome, state)
        assert archive.exists() == (outcome == 'success'), outcome
        cases += 1
    # A later failed attempt must remove the previous successful bundle.
    env['REPOSITORY_OUTCOME'] = 'failure'
    result = subprocess.run(['bash', '-c', script], cwd=root, env=env,
                            capture_output=True, text=True, timeout=10)
    assert result.returncode == 0 and not archive.exists()
    cases += 1
print(f'BATCH_RUNTIME=PASS_SCOPED cases={cases}; APT=STUBBED; DEVICE=NOT_RUN')
