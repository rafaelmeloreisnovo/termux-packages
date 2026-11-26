#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Read-only RAFCODEPHI profile/provenance graph gate. NOT a package/device build."""
from __future__ import annotations
import argparse
import hashlib
import json
import re
from pathlib import Path

MANIFEST = "docs/assurance/rafcodephi-federated-components.v1.json"
RMR = "docs/assurance/rafcodephi-freestanding-modules.v1.json"
GENERATOR = "scripts/build/termux_create_debian_subpackages.sh"
DEFAULTS = "scripts/build/termux_step_setup_variables.sh"
PROFILES = ("bootstrap", "runtime", "build", "developer", "vectras", "full")
SHA40 = re.compile(r"^[0-9a-f]{40}$")
PKG = re.compile(r"^[a-z0-9][a-z0-9+.-]*$")

class GateError(Exception):
    pass

def must(test, token):
    if not test:
        raise GateError(token)

def read(root, path):
    p = root / path
    must(p.is_file(), "MISSING_SOURCE:" + path)
    return p.read_text(encoding="utf-8")

def literal(src, name):
    v = re.findall(r'(?m)^\s*' + re.escape(name) + r'="([^"\n]*)"\s*$', src)
    must(len(v) == 1, "DYNAMIC_OR_AMBIGUOUS:" + name)
    return v[0]

def blob_sha(path):
    raw = path.read_bytes()
    return hashlib.sha1(b"blob " + str(len(raw)).encode() + b"\0" + raw).hexdigest()

def resolve(root, name, gen, defaults):
    must(bool(PKG.fullmatch(name)), "BAD_PACKAGE:" + name)
    matches = []
    if (root / "packages" / name / "build.sh").is_file():
        matches.append((name, "DIRECT_RECIPE"))
    for p in sorted((root / "packages").glob("*/" + name + ".subpackage.sh")):
        matches.append((p.parent.name, "DECLARED_SUBPACKAGE"))
    must(len(matches) <= 1, "AMBIGUOUS_PRODUCER:" + name)
    if matches:
        return dict(output=name, producer=matches[0][0], mode=matches[0][1],
                    artifact="TOKEN_VAZIO_DEB")
    if name.endswith("-static"):
        parent = name[:-7]
        recipe = root / "packages" / parent / "build.sh"
        if recipe.is_file():
            must("TERMUX_PKG_NAME}-static.subpackage.sh" in gen,
                 "STATIC_GENERATOR_MISSING")
            must("TERMUX_PKG_NO_STATICSPLIT" in gen and
                 "lib{,32}/**/*.a" in gen, "STATIC_GENERATOR_DRIFT")
            must(bool(re.search(r"(?m)^\s*TERMUX_PKG_NO_STATICSPLIT=false\s*$", defaults)),
                 "STATIC_DEFAULT_DISABLED")
            src = recipe.read_text(encoding="utf-8")
            must(not re.search(r'(?m)^\s*TERMUX_PKG_NO_STATICSPLIT\s*=\s*["' + "'" +
                               r']?true["' + "'" + r']?\s*(?:#.*)?$', src),
                 "STATIC_PARENT_OPTOUT")
            if parent == "rafcodephi-rmr-vector-field":
                must("librafcodephi-rmr-vector-field.a" in src,
                     "RMR_STATIC_INSTALL_MISSING")
            return dict(output=name, producer=parent, mode="CONDITIONAL_GENERATED_STATIC",
                        artifact="TOKEN_VAZIO_DEB", precondition="staged .a and enabled split")
    raise GateError("UNRESOLVED_OUTPUT:" + name)

def check_dag(edges):
    names = {"rafcodephi-" + p + "-profile" for p in PROFILES}
    links = {n: [] for n in names}
    for a, b in edges:
        if b in names:
            links[a].append(b)
    state = {}
    def walk(n):
        must(state.get(n, 0) != 1, "PROFILE_CYCLE:" + n)
        if state.get(n, 0) == 2:
            return
        state[n] = 1
        for v in links[n]:
            walk(v)
        state[n] = 2
    for node in sorted(names):
        walk(node)

def validate(root, source_sha="TOKEN_VAZIO_REF"):
    root = Path(root).resolve()
    must(source_sha == "TOKEN_VAZIO_REF" or bool(SHA40.fullmatch(source_sha)),
         "BAD_SOURCE_SHA")
    m = json.loads(read(root, MANIFEST))
    must(m.get("schema") == "rafcodephi.federated-components/v1", "MANIFEST_SCHEMA")
    must(m.get("claim_allowed") is False, "CLAIM_ESCALATION")
    must(m.get("policy", {}).get("relicense_upstream_work_allowed") is False,
         "LICENSE_ESCALATION")
    profiles = m.get("package_profiles", {})
    must(set(profiles) == set(PROFILES), "PROFILE_SET")
    components = m.get("components", [])
    must(len(components) == 6 and len({x.get("id") for x in components}) == 6,
         "COMPONENT_SET")
    gen, defaults = read(root, GENERATOR), read(root, DEFAULTS)
    edges, profile_edges, bindings, output = [], [], {}, []
    for profile in PROFILES:
        pkg = "rafcodephi-" + profile + "-profile"
        d = profiles[profile]
        must(d.get("metapackage") == pkg, "PROFILE_NAME:" + profile)
        deps = d.get("depends")
        must(isinstance(deps, list) and bool(deps) and
             len(deps) == len(set(deps)) and
             all(isinstance(v, str) and PKG.fullmatch(v) for v in deps),
             "PROFILE_DEPENDENCIES:" + profile)
        src = read(root, "packages/" + pkg + "/build.sh")
        must(literal(src, "TERMUX_PKG_DEPENDS") == ", ".join(deps),
             "RECIPE_DRIFT:" + profile)
        must(literal(src, "TERMUX_PKG_LICENSE") == "Apache-2.0",
             "PROFILE_LICENSE:" + profile)
        must("TERMUX_PKG_SKIP_SRC_EXTRACT=true" in src and
             "TERMUX_PKG_PLATFORM_INDEPENDENT=true" in src, "NOT_METAPACKAGE:" + profile)
        output.append(dict(name=profile, package=pkg, depends=deps))
        for dep in deps:
            bindings[dep] = resolve(root, dep, gen, defaults)
            edges.append(dict(source=pkg, target=dep, type="DEPENDS_ON",
                              evidence="packages/" + pkg + "/build.sh"))
            profile_edges.append((pkg, dep))
    check_dag(profile_edges)
    for p, parent in (("runtime", "bootstrap"), ("build", "runtime"),
                      ("vectras", "build"), ("full", "vectras")):
        must("rafcodephi-" + parent + "-profile" in profiles[p]["depends"],
             "MISSING_PARENT_EDGE:" + p)
    must(bindings["clang"]["producer"] == "libllvm" and
         bindings["lld"]["producer"] == "libllvm" and
         bindings["curl"]["producer"] == "libcurl", "SUBPACKAGE_PRODUCER_DRIFT")
    must(bindings["rafcodephi-rmr-vector-field-static"]["mode"] ==
         "CONDITIONAL_GENERATED_STATIC", "RMR_STATIC_SOURCE_DRIFT")
    rm = json.loads(read(root, RMR))
    must(rm.get("schema") == "rafcodephi.freestanding-modules/v1", "RMR_SCHEMA")
    must(rm.get("module", {}).get("license_expression") == "GPL-2.0-only" and
         literal(read(root, "packages/rafcodephi-rmr-vector-field/build.sh"),
                 "TERMUX_PKG_LICENSE") == "GPL-2.0-only", "RMR_LICENSE")
    sources = []
    for item in rm["module"]["source_files"]:
        path = item["package_path"]
        must(path.startswith("packages/rafcodephi-rmr-vector-field/files/") and
             ".." not in Path(path).parts, "UNSAFE_SOURCE_PATH")
        must((root / path).is_file() and blob_sha(root / path) == item["git_blob"],
             "RMR_BLOB_DRIFT:" + path)
        sources.append({"path": path, "git_blob": item["git_blob"]})
    ce = []
    for c in components:
        cid = c.get("id")
        must(c.get("attribution_required") is True and
             c.get("distribution_candidate") is False and
             bool(SHA40.fullmatch(c.get("source_commit", ""))),
             "COMPONENT_AUTHORITY:" + str(cid))
        binding = c.get("package_binding")
        if binding:
            name = binding.get("package", "")
            must(bool(PKG.fullmatch(name)), "INVALID_BINDING")
            must(name in bindings or name == "qemu-system-x86-64-headless",
                 "UNEXPECTED_BINDING:" + name)
            if name == "qemu-system-x86-64-headless":
                must((root / "packages/qemu-system-x86-64-headless/build.sh").is_file(),
                     "QEMU_SOURCE_ABSENT")
            ce.append(dict(source=cid, target=name, type=c["integration_mode"],
                           copy_policy=c["copy_policy"]))
    by_id = {c["id"]: c for c in components}
    must(by_id["qemu-rafaelia"]["package_binding"]["arm32_x86_64_guest_runtime"] ==
         "TOKEN_VAZIO", "QEMU_ARM32_OVERCLAIM")
    must(by_id["androidx-rmr"]["upstream_repository"] == "TOKEN_VAZIO",
         "ANDROIDX_ORIGIN_UNVERIFIED_CHANGE")
    return dict(schema="rafcodephi.profile-architecture-evidence/v1",
                source_repository="rafaelmeloreisnovo/termux-packagesMy",
                source_commit=source_sha,
                manifest_producer_lineage=m.get("producer", {}).get("repository", "TOKEN_VAZIO"),
                profiles=output, dependency_edges=edges,
                output_producers=[bindings[k] for k in sorted(bindings)],
                federated_bindings=ce, rmr_source_blobs=sources,
                gaps=dict(static_deb="TOKEN_VAZIO_ARTIFACT", android_runtime="TOKEN_VAZIO_DEVICE",
                          arm32_qemu_x86_64="TOKEN_VAZIO", signed_apt="TOKEN_VAZIO_IN_THIS_GATE",
                          global_licenses="TOKEN_VAZIO"),
                source_gate="PASS_SCOPED", build_gate="NOT_RUN",
                device_gate="NOT_RUN", release_allowed=False, claim_allowed=False)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=str(Path(__file__).resolve().parents[1]))
    ap.add_argument("--source-sha", default="TOKEN_VAZIO_REF")
    ap.add_argument("--out")
    args = ap.parse_args()
    try:
        doc = validate(Path(args.root), args.source_sha)
    except (GateError, OSError, ValueError, KeyError, TypeError) as ex:
        print("BLOCKED_PROFILE_ARCHITECTURE:", ex)
        return 1
    if args.out:
        p = Path(args.out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(doc, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("PASS_SOURCE_PROFILE_ARCHITECTURE profiles=6 edges=" +
          str(len(doc["dependency_edges"])) + " outputs=" +
          str(len(doc["output_producers"])) +
          " DEB=NOT_RUN DEVICE=NOT_RUN CLAIM_ALLOWED=false")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
