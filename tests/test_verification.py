import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from build_port import compare

class VerificationTests(unittest.TestCase):
    def setUp(self):
        workspace = Path(__file__).resolve().parents[1]
        scratch = workspace/'build/test-temp'
        scratch.mkdir(parents=True, exist_ok=True)
        self.folder = tempfile.TemporaryDirectory(dir=scratch)
        # Cleanup is restricted to our own generated workspace test directory.
        assert Path(self.folder.name).resolve().is_relative_to(scratch.resolve())
        self.addCleanup(self.folder.cleanup)
        self.a = Path(self.folder.name)/'native.txt'
        self.b = Path(self.folder.name)/'reference.txt'

    def write(self, a, b):
        self.a.write_text(a)
        self.b.write_text(b)

    def test_native_requires_identical_hardware(self):
        self.write('0 trace=1 hw=A\n', '0 trace=1 hw=B\n')
        with self.assertRaises(RuntimeError): compare(self.a, self.b, 1)

    def test_oracle_compares_observations_not_internal_layout(self):
        self.write('0 trace=1 hw=A\n', '0 trace=1 hw=B\n')
        compare(self.a, self.b, 1, False)

    def test_oracle_still_rejects_visible_difference(self):
        self.write('0 trace=1 hw=A\n', '0 trace=2 hw=B\n')
        with self.assertRaises(RuntimeError): compare(self.a, self.b, 1, False)

    def test_incomplete_runs_cannot_pass(self):
        self.write('0 trace=1 hw=A\n', '0 trace=1 hw=A\n')
        with self.assertRaises(RuntimeError): compare(self.a, self.b, 2)

    def test_skipped_frames_cannot_pass(self):
        self.write('1 trace=1 hw=A\n', '1 trace=1 hw=A\n')
        with self.assertRaises(RuntimeError): compare(self.a, self.b, 1)

if __name__ == '__main__': unittest.main()
