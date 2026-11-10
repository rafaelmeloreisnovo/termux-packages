#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Mutation/falsification tests: synthetic fixtures plus exact checkout source gate."""
import hashlib
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "validate_rafcodephi_profile_architecture.py"
spec = importlib.util.spec_from_file_location("profilegate", SCRIPT)
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)

class ProfileGateFalsifiers(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        profiles = {
            "bootstrap": ["apt","bash","busybox","dpkg","ca-certificates","coreutils",
                          "termux-tools","rafcodephi-federation-metadata"],
            "runtime": ["rafcodephi-bootstrap-profile","openssl","curl","git","python","procps","b3sum"],
            "build": ["rafcodephi-runtime-profile","clang","lld","cmake","make","ninja",
                      "binutils","file","patchelf","rafcodephi-rmr-vector-field",
                      "rafcodephi-rmr-vector-field-static"],
            "developer": ["nodejs-lts","npm","openjdk-21","android-tools","clang",
                          "make","ninja","ndk-sysroot"],
            "vectras": ["rafcodephi-build-profile","aria2","tar","pulseaudio","proot","proot-distro"],
            "full": ["rafcodephi-vectras-profile","termux-api"]
        }
        comp = []
        for name in ("termux-app-rafcodephi","blake3-reference-fork","vectras-vm-android",
                     "qemu-rafaelia","androidx-rmr","termux-api-rafcodephi"):
            c = dict(id=name, source_commit="a"*40, integration_mode="contract",
                     attribution_required=True, distribution_candidate=False,
                     copy_policy="no_copy")
            if name == "blake3-reference-fork":
                c["package_binding"] = dict(package="b3sum")
            if name == "qemu-rafaelia":
                c["package_binding"] = dict(package="qemu-system-x86-64-headless",
                                            arm32_x86_64_guest_runtime="TOKEN_VAZIO")
            if name == "termux-api-rafcodephi":
                c["package_binding"] = dict(package="termux-api")
            if name == "androidx-rmr":
                c["upstream_repository"] = "TOKEN_VAZIO"
            comp.append(c)
        self.manifest = dict(schema="rafcodephi.federated-components/v1",
            producer=dict(repository="rafaelmeloreisnovo/termux-packages"),
            policy=dict(relicense_upstream_work_allowed=False), claim_allowed=False,
            package_profiles={p:dict(metapackage="rafcodephi-"+p+"-profile",depends=v)
                              for p,v in profiles.items()}, components=comp)
        self.write(gate.MANIFEST, json.dumps(self.manifest))
        self.write(gate.GENERATOR,
                   "TERMUX_PKG_NO_STATICSPLIT; lib{,32}/**/*.a; TERMUX_PKG_NAME}-static.subpackage.sh\n")
        self.write(gate.DEFAULTS, "TERMUX_PKG_NO_STATICSPLIT=false\n")
        for p,deps in profiles.items():
            self.write("packages/rafcodephi-"+p+"-profile/build.sh",
                'TERMUX_PKG_LICENSE="Apache-2.0"\nTERMUX_PKG_SKIP_SRC_EXTRACT=true\n'
                'TERMUX_PKG_PLATFORM_INDEPENDENT=true\nTERMUX_PKG_DEPENDS="' +
                ", ".join(deps)+'"\n')
        for dep in set(sum(profiles.values(), [])):
            if dep.startswith("rafcodephi-") and dep.endswith("-profile"):
                continue
            if dep in ("clang","lld","curl","rafcodephi-rmr-vector-field-static"):
                continue
            self.write("packages/"+dep+"/build.sh",
                       'TERMUX_PKG_LICENSE="GPL-2.0-only"\n'
                       if dep=="rafcodephi-rmr-vector-field" else 'TERMUX_PKG_LICENSE="MIT"\n')
        self.write("packages/libllvm/build.sh",'TERMUX_PKG_LICENSE="Apache-2.0"\n')
        self.write("packages/libllvm/clang.subpackage.sh",'# clang output\n')
        self.write("packages/libllvm/lld.subpackage.sh",'# lld output\n')
        self.write("packages/libcurl/build.sh",'TERMUX_PKG_LICENSE="curl"\n')
        self.write("packages/libcurl/curl.subpackage.sh",'# curl output\n')
        self.write("packages/qemu-system-x86-64-headless/build.sh",
                   'TERMUX_PKG_LICENSE="GPL-2.0"\n')
        self.write("packages/rafcodephi-rmr-vector-field/build.sh",
                   'TERMUX_PKG_LICENSE="GPL-2.0-only"\n'
                   'install librafcodephi-rmr-vector-field.a "$TERMUX_PREFIX/lib/"\n')
        sources=[]
        for filename in ("rmr_vector_field.c","rmr_vector_field.h","rmr_types.h"):
            p="packages/rafcodephi-rmr-vector-field/files/"+filename
            self.write(p, "/* source fixture "+filename+" */\n")
            sources.append(dict(package_path=p, git_blob=gate.blob_sha(self.root/p)))
        self.write(gate.RMR,json.dumps(dict(schema="rafcodephi.freestanding-modules/v1",
                   module=dict(license_expression="GPL-2.0-only",source_files=sources))))

    def write(self,path,s):
        p=self.root/path
        p.parent.mkdir(parents=True,exist_ok=True)
        p.write_text(s,encoding="utf-8")

    def blocked(self, token):
        with self.assertRaises(gate.GateError) as e:
            gate.validate(self.root)
        self.assertIn(token,str(e.exception))

    def test_01_valid_source_graph(self):
        d=gate.validate(self.root)
        self.assertEqual(len(d["profiles"]),6)
        self.assertEqual(len(d["federated_bindings"]),3)
        self.assertTrue(any(x["mode"]=="CONDITIONAL_GENERATED_STATIC"
                            for x in d["output_producers"]))
        self.assertFalse(d["claim_allowed"])
        self.assertEqual(d["build_gate"],"NOT_RUN")

    def test_02_changed_profile_dependencies(self):
        p=self.root/"packages/rafcodephi-build-profile/build.sh"
        p.write_text(p.read_text().replace("clang, lld", "clang, bad-llvm"))
        self.blocked("RECIPE_DRIFT")

    def test_03_missing_recipe(self):
        (self.root/"packages/ninja/build.sh").unlink()
        self.blocked("UNRESOLVED_OUTPUT:ninja")

    def test_04_duplicate_dependency(self):
        self.manifest["package_profiles"]["build"]["depends"].append("ninja")
        self.write(gate.MANIFEST,json.dumps(self.manifest))
        self.blocked("PROFILE_DEPENDENCIES")

    def test_05_cycle(self):
        self.manifest["package_profiles"]["bootstrap"]["depends"].append("rafcodephi-full-profile")
        self.write(gate.MANIFEST,json.dumps(self.manifest))
        p=self.root/"packages/rafcodephi-bootstrap-profile/build.sh"
        p.write_text(p.read_text().replace('rafcodephi-federation-metadata"',
                                         'rafcodephi-federation-metadata, rafcodephi-full-profile"'))
        self.blocked("PROFILE_CYCLE")

    def test_06_static_generator_removed(self):
        self.write(gate.GENERATOR,"not a generator\n")
        self.blocked("STATIC_GENERATOR_MISSING")

    def test_07_static_disabled(self):
        self.write(gate.DEFAULTS,"TERMUX_PKG_NO_STATICSPLIT=true\n")
        self.blocked("STATIC_DEFAULT_DISABLED")

    def test_08_parent_optout(self):
        p=self.root/"packages/rafcodephi-rmr-vector-field/build.sh"
        p.write_text(p.read_text()+"TERMUX_PKG_NO_STATICSPLIT=true\n")
        self.blocked("STATIC_PARENT_OPTOUT")

    def test_09_missing_static_install(self):
        self.write("packages/rafcodephi-rmr-vector-field/build.sh",
                   'TERMUX_PKG_LICENSE="GPL-2.0-only"\n')
        self.blocked("RMR_STATIC_INSTALL_MISSING")

    def test_10_drifted_rmr_blob(self):
        p=self.root/"packages/rafcodephi-rmr-vector-field/files/rmr_vector_field.c"
        p.write_text(p.read_text()+"changed\n")
        self.blocked("RMR_BLOB_DRIFT")

    def test_11_claim_escalation(self):
        self.manifest["claim_allowed"]=True
        self.write(gate.MANIFEST,json.dumps(self.manifest))
        self.blocked("CLAIM_ESCALATION")

    def test_12_qemu_false_device_claim(self):
        self.manifest["components"][3]["package_binding"]["arm32_x86_64_guest_runtime"]="PASS"
        self.write(gate.MANIFEST,json.dumps(self.manifest))
        self.blocked("QEMU_ARM32_OVERCLAIM")

    def test_13_androidx_origin_invented(self):
        self.manifest["components"][4]["upstream_repository"]="invented/unknown"
        self.write(gate.MANIFEST,json.dumps(self.manifest))
        self.blocked("ANDROIDX_ORIGIN_UNVERIFIED_CHANGE")

    def test_14_duplicate_output_producers(self):
        self.write("packages/other/clang.subpackage.sh","fake duplicate\n")
        self.blocked("AMBIGUOUS_PRODUCER")

    def test_15_wrong_sha(self):
        with self.assertRaises(gate.GateError):
            gate.validate(self.root,"bad-sha")

if __name__=="__main__":
    unittest.main(verbosity=2)
