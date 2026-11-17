#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("elfgate", root/"scripts/verify_pure_elf_object.py")
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)
ARM32, ARM64 = Path(sys.argv.pop(1)), Path(sys.argv.pop(1))


class ElfTests(unittest.TestCase):
    def test_real_objects(self):
        for path, abi in ((ARM32, "armeabi-v7a"), (ARM64, "arm64-v8a")):
            self.assertEqual(gate.verify(path, abi, "rafop_rbit32")["undefined_symbols"], 0)

    def test_wrong_abi(self):
        with self.assertRaises(ValueError):
            gate.verify(ARM32, "arm64-v8a", "rafop_rbit32")

    def test_missing_symbol(self):
        with self.assertRaises(ValueError):
            gate.verify(ARM64, "arm64-v8a", "missing")

    def test_header_mutations(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)/"mutated.o"
            for off, value in ((0, 0), (4, 2), (5, 2), (6, 0), (16, 2),
                               (18, 183), (20, 0), (32, 0), (39, 0), (40, 0), (46, 0)):
                data = bytearray(ARM32.read_bytes())
                data[off] = value
                path.write_bytes(data)
                with self.subTest(off=off), self.assertRaises(ValueError):
                    gate.verify(path, "armeabi-v7a", "rafop_rbit32")

    def test_truncated(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)/"truncated.o"
            for end in (0, 51, 63):
                path.write_bytes(ARM64.read_bytes()[:end])
                with self.subTest(end=end), self.assertRaises(ValueError):
                    gate.verify(path, "arm64-v8a", "rafop_rbit32")


if __name__ == "__main__":
    unittest.main()
