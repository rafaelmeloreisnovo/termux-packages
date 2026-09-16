#!/usr/bin/env python3
import json
from pathlib import Path

P = Path('docs/assurance/package-provenance-handoff.v1.json')

def fail(msg):
    raise SystemExit(f'FAIL: {msg}')

m = json.loads(P.read_text(encoding='utf-8'))
if m.get('schema') != 'rafaelia.package-provenance-handoff.v1': fail('schema')
if m.get('claim_allowed') is not False: fail('claim_allowed')
if m['license'].get('single_license_flattening_allowed') is not False: fail('license flattening')
if not Path('LICENSE.md').is_file(): fail('LICENSE.md missing')
if m['handoff'].get('build_success_is_install_success') is not False: fail('build promoted to install')
if m['handoff'].get('install_success_is_runtime_success') is not False: fail('install promoted to runtime')
if m['handoff'].get('documentation_is_executable_handoff') is not False: fail('docs promoted to executable handoff')
required = set(m['handoff'].get('required_chain', []))
for name in ('source_digest','recipe_path_and_commit','artifact_digest','consumer_expected_identity','validator_result','provider_artifact_id','provider_artifact_digest','producer_receipt_digest','consumer_verified_byte_digests'):
    if name not in required: fail(f'missing handoff field {name}')
if m['security_privacy'].get('state') != 'FAIL_CLOSED': fail('security/privacy not fail-closed')
if m.get('signed_apt', {}).get('state') != 'IMPLEMENTED_UNTESTED_ACTIVATION_GATED': fail('signed apt state')
if m['signed_apt'].get('trust') != 'Signed-By embedded archive key': fail('signed apt trust')
if m['signed_apt'].get('force_push_allowed') is not False: fail('signed apt force-push boundary')
if m['signed_apt'].get('automatic_push_publication') is not False: fail('signed apt automatic publication boundary')
if not m['rollback'].get('available'): fail('rollback')
print('PASS: package licensing/provenance/handoff is path-aware; custody + signed APT are activation-gated and fail-closed')
