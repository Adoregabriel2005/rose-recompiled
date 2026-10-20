"""Inspect a local cartridge, preserving each revision in its own SHA256 folder."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def inspect(data):
    if len(data) < 16 or data[:4] != b'NES\x1a':
        raise ValueError('Cabecalho iNES ausente ou truncado')
    h = data[:16]
    nes2 = h[7] & 12 == 8
    mapper = (h[6] >> 4) | (h[7] & 240)
    if nes2:
        mapper |= (h[8] & 15) << 8
        if h[9] != 0:
            raise ValueError('Tamanho NES 2.0 estendido ainda nao implementado')
    prg, chr_ = h[4] * 16384, h[5] * 8192
    offset = 16 + (512 if h[6] & 4 else 0)
    if prg == 0 or len(data) != offset + prg + chr_:
        raise ValueError('Tamanho da ROM nao corresponde ao cabecalho')
    timing = ['NTSC', 'PAL', 'MULTI', 'DENDY'][h[12] & 3] if nes2 else ('PAL' if h[9] & 1 else 'NTSC')
    return dict(sha256=hashlib.sha256(data).hexdigest(),
                payload_sha256=hashlib.sha256(data[offset:]).hexdigest(),
                format='NES 2.0' if nes2 else 'iNES', mapper=mapper,
                submapper=h[8] >> 4 if nes2 else None,
                prg_bytes=prg, chr_bytes=chr_, timing_header=timing,
                identity='unverified', region='unverified',
                compatibility='not-tested',
                note='Header timing is not a verified region or game identity.')

def read_rom(path):
    if path.suffix.lower() != '.zip':
        return path.read_bytes()
    with zipfile.ZipFile(path) as archive:
        entries = [e for e in archive.infolist() if e.filename.lower().endswith('.nes') and not e.is_dir()]
        if len(entries) != 1:
            raise ValueError('O ZIP deve conter exatamente uma ROM .nes')
        if entries[0].file_size > 8 * 1024 * 1024:
            raise ValueError('ROM excede limite de 8 MiB')
        return archive.read(entries[0])

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    args = parser.parse_args()
    data = read_rom(args.rom)
    report = inspect(data)
    if report['mapper'] != 4:
        raise ValueError('Este projeto espera mapper 4/MMC3; importacao recusada')
    folder = ROOT / 'private' / 'roms' / report['sha256']
    folder.mkdir(parents=True, exist_ok=True)
    (folder / 'game.nes').write_bytes(data)
    (folder / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(rom=str(folder / 'game.nes'), **report), indent=2))

if __name__ == '__main__':
    main()
