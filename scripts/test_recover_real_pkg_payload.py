import hashlib
import importlib.util
import io
from pathlib import Path
import tarfile
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location('recover_payload', Path(__file__).with_name('recover_real_pkg_payload.py'))
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

class RecoveryTests(unittest.TestCase):
    def fixture(self, root, member='debs/arm/apt_1_arm.deb', symlink=False):
        stream = io.BytesIO()
        with tarfile.open(fileobj=stream, mode='w') as t:
            info = tarfile.TarInfo(member)
            if symlink:
                info.type = tarfile.SYMTYPE; info.linkname = '/tmp/other'
                t.addfile(info)
            else:
                info.size = 3; t.addfile(info, io.BytesIO(b'deb'))
        path = root / 'input.zip'
        with zipfile.ZipFile(path, 'w') as z:
            z.writestr('artifacts/rafcodephi-bootstrap/rafcodephi-bootstrap-arm.zip', b'fixture')
            z.writestr('artifacts/rafcodephi-bootstrap/RAFCODEPHI_REAL_BOOTSTRAP_MANIFEST.txt', b'fixture')
            z.writestr('artifacts/rafcodephi-bootstrap/rafcodephi-debs.tar', stream.getvalue())
        return path, hashlib.sha256(path.read_bytes()).hexdigest()
    def test_recover_is_scoped_and_bytes_preserved(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); archive, digest = self.fixture(root)
            m.recover(archive, root/'out', 'arm', digest)
            self.assertEqual((root/'out/debs/apt_1_arm.deb').read_bytes(), b'deb')
            self.assertIn('"claim_allowed": false', (root/'out/receipt.json').read_text())
    def test_digest_mismatch_before_destination(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d); archive,_=self.fixture(root)
            with self.assertRaisesRegex(ValueError,'SHA256'):m.recover(archive,root/'out','arm','0'*64)
            self.assertFalse((root/'out').exists())
    def test_traversal_blocked(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d); archive,digest=self.fixture(root,'debs/arm/../../outside.deb')
            with self.assertRaisesRegex(ValueError,'unsafe'):m.recover(archive,root/'out','arm',digest)
    def test_symlink_blocked(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d); archive,digest=self.fixture(root,symlink=True)
            with self.assertRaisesRegex(ValueError,'non-regular'):m.recover(archive,root/'out','arm',digest)
    def test_other_architecture_blocked(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d); archive,digest=self.fixture(root,'debs/aarch64/apt_1_aarch64.deb')
            with self.assertRaisesRegex(ValueError,'unexpected'):m.recover(archive,root/'out','arm',digest)

if __name__ == '__main__':unittest.main()
