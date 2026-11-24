"""Host-only adversarial tests for API29 Python ELF gate, not device proof."""
import importlib.util
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "elf_gate", ROOT / "scripts/guard_rafcodephi_python_api29.py")
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


class ELFGateTests(unittest.TestCase):
    def test_only_undefined_dynamic_symbols_are_imports(self):
        sample = """Symbol table '.dynsym' contains 4 entries:
  Num:    Value  Size Type    Bind   Vis      Ndx Name
   0: 00000000     0 NOTYPE  LOCAL  DEFAULT  UND
   1: 00000000     0 FUNC    GLOBAL DEFAULT  UND sem_clockwait@LIBC (2)
   2: 00012345    18 FUNC    GLOBAL DEFAULT   12 sem_clockwait
   3: 00000000     0 FUNC    GLOBAL DEFAULT  UND pthread_cond_clockwait@LIBC (2)
"""
        self.assertEqual(gate.undefined_symbols(sample),
                         {"sem_clockwait", "pthread_cond_clockwait"})

    def test_raw_bytes_string_is_not_a_dynamic_import(self):
        self.assertEqual(gate.undefined_symbols("80872:sem_clockwait\n"), set())

    def test_missing_library_is_blocked(self):
        with tempfile.TemporaryDirectory() as td:
            result = gate.inspect(Path(td) / "libpython.so", "arm", 29)
        self.assertEqual(result["state"], "BLOCKED")
        self.assertFalse(result["claim_allowed"])
        self.assertEqual(result["device_execution"], "TOKEN_VAZIO")

    def test_invalid_elf_is_blocked(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "libpython.so"
            path.write_bytes(b"broken" * 30)
            result = gate.inspect(path, "arm", 29)
        self.assertEqual(result["state"], "BLOCKED")
        self.assertIn("NOT_ELF", " ".join(result["reasons"]))

    def test_api29_rejects_clockwait(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "libpython.so"
            path.write_bytes(b"\x7fELF\x01" + b"\x00" * 60)
            with patch.object(gate.subprocess, "run") as run:
                run.side_effect = [
                    type("R", (), {"returncode": 0,
                        "stdout": "Class: ELF32\nMachine: ARM\n"})(),
                    type("R", (), {"returncode": 0,
                        "stdout": "Symbol table '.dynsym' contains 1 entries:\n"
                            "  1: 0 0 FUNC GLOBAL DEFAULT UND sem_clockwait@LIBC (2)\n"})(),
                ]
                result = gate.inspect(path, "arm", 29)
        self.assertEqual(result["state"], "BLOCKED")
        self.assertIn("sem_clockwait", result["reasons"][0])

    def test_allowed_import_never_promotes_device(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "libpython.so"
            path.write_bytes(b"\x7fELF\x01" + b"\x00" * 60)
            with patch.object(gate.subprocess, "run") as run:
                run.side_effect = [
                    type("R", (), {"returncode": 0,
                        "stdout": "Class: ELF32\nMachine: ARM\n"})(),
                    type("R", (), {"returncode": 0,
                        "stdout": "Symbol table '.dynsym' contains 1 entries:\n"
                            "  1: 0 0 FUNC GLOBAL DEFAULT UND puts@LIBC (2)\n"})(),
                ]
                result = gate.inspect(path, "arm", 29)
        self.assertEqual(result["state"], "PASS_SCOPED_STATIC")
        self.assertFalse(result["claim_allowed"])
        self.assertEqual(result["device_execution"], "TOKEN_VAZIO")

    def test_recipe_enforces_api29_target_and_revision(self):
        recipe = (ROOT / "packages/python/build.sh").read_text(encoding="utf-8")
        self.assertIn("TERMUX_PKG_REVISION=2", recipe)
        self.assertIn('TERMUX_PKG_EXTRA_CONFIGURE_ARGS+=" ac_cv_func_sem_clockwait=no"', recipe)
        self.assertIn('TERMUX_PKG_EXTRA_CONFIGURE_ARGS+=" ac_cv_func_pthread_cond_clockwait=no"', recipe)
        self.assertIn("--max-api 29", recipe)
        self.assertIn('staged_root=$(cd -- "$TERMUX_PKG_MASSAGEDIR/$TERMUX_PREFIX_CLASSICAL" && pwd -P)', recipe)
        self.assertIn('current_root=$(pwd -P)', recipe)
        self.assertIn('[[ "$current_root" == "$staged_root" ]]', recipe)
        self.assertIn('--library "lib/libpython${_MAJOR_VERSION}.so"', recipe)
        self.assertIn('$TERMUX_SCRIPTDIR/scripts/guard_rafcodephi_python_api29.py', recipe)
        self.assertNotIn('--library "$TERMUX_PREFIX/lib/libpython${_MAJOR_VERSION}.so"', recipe)
        self.assertIn("guard_rafcodephi_python_api29.py", recipe)


if __name__ == "__main__":
    unittest.main()
