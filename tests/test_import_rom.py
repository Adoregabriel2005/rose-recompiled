import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from import_rom import inspect

def rom(mapper=4, trainer=False):
    h = bytearray(b'NES\x1a' + bytes(12))
    h[4] = 1
    h[6] = (mapper << 4) | (4 if trainer else 0)
    return bytes(h) + bytes(512 if trainer else 0) + bytes(16384)

class ImportTests(unittest.TestCase):
    def test_header_and_mapper(self):
        self.assertEqual(inspect(rom())['mapper'], 4)
    def test_trainer_excluded_from_payload_identity(self):
        self.assertEqual(inspect(rom())['payload_sha256'], inspect(rom(trainer=True))['payload_sha256'])
    def test_truncated_rejected(self):
        with self.assertRaises(ValueError): inspect(rom()[:-1])
    def test_bad_signature_rejected(self):
        with self.assertRaises(ValueError): inspect(b'x'*64)
    def test_region_not_guessed(self):
        self.assertEqual(inspect(rom())['region'], 'unverified')
    def test_nes2_mapper_extension(self):
        data=bytearray(rom());data[7]=8;data[8]=1
        self.assertEqual(inspect(data)['mapper'],260)

if __name__ == '__main__': unittest.main()
