#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import subprocess
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "scripts/repair_rafcodephi_bootstrap_symlinks.py"
spec = importlib.util.spec_from_file_location("raf_repair", MODULE_PATH)
assert spec and spec.loader
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

PREFIX = "/data/data/com.termux.rafacodephi/files/usr"


def build_deb(
    root: Path, name: str, rel_symlink: str, target: str
) -> Path:
    pkg = root / f"pkg-{name}"
    (pkg / "DEBIAN").mkdir(parents=True)
    (pkg / "DEBIAN/control").write_text(
        f"Package: {name}\n"
        "Version: 1.0\n"
        "Architecture: arm\n"
        "Maintainer: RAFCODEPHI CI\n"
        "Description: fixture\n",
        encoding="utf-8",
    )
    prefix = pkg / PREFIX.lstrip("/")
    link = prefix / rel_symlink
    link.parent.mkdir(parents=True, exist_ok=True)
    target_file = link.parent / target
    target_file.write_text("fixture\n", encoding="utf-8")
    link.symlink_to(target)

    out = root / f"{name}_1.0_arm.deb"
    subprocess.run(
        [
            "dpkg-deb",
            "--build",
            "--root-owner-group",
            str(pkg),
            str(out),
        ],
        check=True,
        stdout=subprocess.DEVNULL,
    )
    return out


def make_bootstrap(path: Path) -> None:
    with zipfile.ZipFile(
        path, "w", compression=zipfile.ZIP_DEFLATED
    ) as zf:
        zf.writestr("SYMLINKS.txt", "existing←./bin/existing\n")
        zf.writestr("bin/dash", b"fixture")
        zf.writestr("libexec/termux-api-broadcast", b"fixture")


def main() -> int:
    with tempfile.TemporaryDirectory(
        prefix="rafcodephi-symlink-repair-test."
    ) as td:
        root = Path(td)
        debs = root / "debs"
        debs.mkdir()
        build_deb(debs, "dash", "bin/sh", "dash")
        build_deb(
            debs,
            "termux-api",
            "libexec/termux-api",
            "termux-api-broadcast",
        )

        bootstrap = root / "bootstrap-arm.zip"
        make_bootstrap(bootstrap)
        receipt = mod.repair(bootstrap, debs, PREFIX)
        assert receipt["state"] == "PASS"
        assert receipt["claim_allowed"] is False
        assert receipt["device_runtime_proof"] == "TOKEN_VAZIO"
        assert receipt["mutation_performed"] is True
        assert {item["path"] for item in receipt["repairs"]} == {
            "bin/sh",
            "libexec/termux-api",
        }

        with zipfile.ZipFile(bootstrap) as zf:
            mapping = mod.parse_symlinks(zf.read("SYMLINKS.txt"))
        assert mapping["bin/sh"] == "dash"
        assert (
            mapping["libexec/termux-api"]
            == "termux-api-broadcast"
        )

        # Falsifier: wrong package evidence must not authorize a repair.
        bad = root / "bad"
        bad.mkdir()
        build_deb(bad, "dash", "bin/sh", "not-dash")
        build_deb(
            bad,
            "termux-api",
            "libexec/termux-api",
            "termux-api-broadcast",
        )
        bad_bootstrap = root / "bootstrap-bad.zip"
        make_bootstrap(bad_bootstrap)
        try:
            mod.repair(bad_bootstrap, bad, PREFIX)
        except mod.RepairError as exc:
            assert "conflicting .deb symlink proof for bin/sh" in str(exc)
        else:
            raise AssertionError(
                "wrong .deb symlink target was not blocked"
            )

        # Missing global symlink metadata is a wider failure and is not rebuilt.
        missing_manifest = root / "bootstrap-missing-symlinks.zip"
        with zipfile.ZipFile(
            missing_manifest, "w", compression=zipfile.ZIP_DEFLATED
        ) as zf:
            zf.writestr("bin/dash", b"fixture")
        try:
            mod.repair(missing_manifest, debs, PREFIX)
        except mod.RepairError as exc:
            assert "exactly one SYMLINKS.txt" in str(exc)
        else:
            raise AssertionError(
                "missing global symlink manifest was partially reconstructed"
            )

        # Existing correct links are accepted without mutating the archive.
        stable = root / "bootstrap-stable.zip"
        with zipfile.ZipFile(
            stable, "w", compression=zipfile.ZIP_DEFLATED
        ) as zf:
            zf.writestr(
                "SYMLINKS.txt",
                "dash←./bin/sh\n"
                "termux-api-broadcast←./libexec/termux-api\n",
            )
        stable_receipt = mod.repair(stable, debs, PREFIX)
        assert stable_receipt["mutation_performed"] is False
        assert (
            stable_receipt["bootstrap_sha256_before"]
            == stable_receipt["bootstrap_sha256_after"]
        )

    print(
        "RAFCODEPHI_BOOTSTRAP_SYMLINK_REPAIR_TEST=PASS "
        "positive=true falsifier=true missing_manifest_blocked=true idempotent=true"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
