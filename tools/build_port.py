"""Rebuild a supplied ROM, profile real execution, then verify native code.

No ROM or generated game sources are downloaded. Each ROM has its own private
profile. Child tools run without console windows; all logs stay in build/.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

from import_rom import inspect, read_rom

ROOT = Path(__file__).resolve().parents[1]

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def run(argv, log, timeout=900):
    print(f'[{log.stem}]', flush=True)
    with log.open('w', encoding='utf-8') as output:
        result = subprocess.run([str(x) for x in argv], cwd=ROOT, stdout=output,
                                stderr=subprocess.STDOUT, timeout=timeout,
                                creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    text = log.read_text(encoding='utf-8', errors='replace')
    if result.returncode:
        raise RuntimeError(f'exit {result.returncode}: {log}\n{text[-4000:]}')
    return text

def compare(a, b, frames, with_hardware=True):
    left = Path(a).read_text().splitlines()
    right = Path(b).read_text().splitlines()
    if len(left) != frames or len(right) != frames:
        raise RuntimeError(f'Incomplete comparison: {len(left)}/{len(right)}; expected {frames}')
    for i, (x, y) in enumerate(zip(left, right)):
        if not x.startswith(f'{i} trace=') or not y.startswith(f'{i} trace='):
            raise RuntimeError(f'Nonconsecutive frame at {i}')
        if not with_hardware:
            # Independent machines have different internal representations.
            x, y = x.split(' hw=')[0], y.split(' hw=')[0]
        if x != y:
            raise RuntimeError(f'Divergence frame {i}:\n{x}\n{y}')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, required=True)
    parser.add_argument('--route', type=Path, default=ROOT/'tests/gameplay-route.txt')
    parser.add_argument('--frames', type=int, default=6000)
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--oracle', action='store_true')
    parser.add_argument('--reuse-profile', action='store_true',
                        help='Reuse matching completed interpreter traces and ROM-specific profiles')
    parser.add_argument('--align', type=int, nargs='+', choices=range(4), default=[0])
    args = parser.parse_args()
    if args.frames < 1 or args.jobs < 1:
        parser.error('frames and jobs must be positive')
    if not (ROOT/'external/nesrecomp/recompiler/CMakeLists.txt').is_file():
        parser.error('Run tools/setup.ps1 first')
    data = read_rom(args.rom.resolve())
    info = inspect(data)
    if info['mapper'] != 4:
        parser.error('Expected mapper 4/MMC3')
    folder = ROOT/'private/roms'/info['sha256']
    folder.mkdir(parents=True, exist_ok=True)
    rom = folder/'game.nes'
    rom.write_bytes(data)
    (folder/'manifest.json').write_text(json.dumps(info, indent=2)+'\n', encoding='utf-8')
    engine = (ROOT/'engine.lock').read_text().strip()
    profile = folder/'profiles'/engine
    profile.mkdir(parents=True, exist_ok=True)
    seeds, captures = profile/'seeds.txt', profile/'captures.txt'
    build = ROOT/'build/game'
    logs = ROOT/'build/verification'
    logs.mkdir(parents=True, exist_ok=True)
    # Revoke stale success before any operation that might fail.
    report = dict(status='running', rom_sha256=info['sha256'], engine=engine,
                  route_sha256=digest(args.route), frames=args.frames, alignments=args.align)
    report_path = logs/'result.json'
    previous = json.loads(report_path.read_text()) if report_path.exists() else {}
    reuse = args.reuse_profile and all(previous.get(key)==report[key]
                                      for key in ('rom_sha256','engine','route_sha256','frames','alignments'))
    if args.reuse_profile and not reuse:
        parser.error('Existing profile report does not match these inputs')
    report_path.write_text(json.dumps(report, indent=2)+'\n')
    configure = ['cmake', '-S', ROOT, '-B', build, '-G', 'MinGW Makefiles',
                 '-DCMAKE_C_COMPILER=clang', '-DCMAKE_BUILD_TYPE=Release',
                 f'-DPython3_EXECUTABLE={sys.executable}', f'-DROSE_ROM={rom}',
                 f'-DROSE_BUILD_ORACLE={"ON" if args.oracle else "OFF"}']
    if args.oracle:
        configure += ['-DCMAKE_CXX_COMPILER=clang++']
    try:
        # Bootstrap without importing a different ROM's cached profile.
        if not reuse:
            run(configure + ['-DROSE_SEEDS=', '-DROSE_CAPTURES='], logs/'configure-bootstrap.log')
            run(['cmake', '--build', build, '--parallel', args.jobs], logs/'build-bootstrap.log')
        exe = build/'RoseRecompiled.exe'
        common = [rom, '--frames', args.frames, '--no-save', '--input', args.route.resolve()]
        for align in args.align:
            if not reuse:
                run([exe, *common, '--align', align, '--interp-only',
                     '--miss-log', seeds, '--capture-log', captures,
                     '--hash-out', logs/f'a{align}-interp.txt'], logs/f'a{align}-profile.log')
            else:
                trace=logs/f'a{align}-interp.txt'
                compare(trace,trace,args.frames)
                if f'mode=interp-only frames={args.frames} cycles=' not in (logs/f'a{align}-profile.log').read_text():
                    raise RuntimeError('Cached interpreter run did not complete')
        run(configure + [f'-DROSE_SEEDS={seeds}', f'-DROSE_CAPTURES={captures}'], logs/'configure-native.log')
        run(['cmake', '--build', build, '--parallel', args.jobs], logs/'build-native.log')
        runs = []
        for align in args.align:
            output = run([exe, *common, '--align', align,
                          '--hash-out', logs/f'a{align}-native.txt',
                          '--screenshot', logs/f'a{align}-game.png', '--shot-every', 600,
                          '--wav-out', logs/f'a{align}-audio.wav'], logs/f'a{align}-native.log')
            compare(logs/f'a{align}-native.txt', logs/f'a{align}-interp.txt', args.frames)
            m = re.search(r'mode=native frames=(\d+) cycles=(\d+) native_cycles=(\d+)', output)
            if not m or int(m[1]) != args.frames:
                raise RuntimeError('Missing native completion summary')
            entry = dict(align=align, cycles=int(m[2]), native_cycles=int(m[3]), interpreter_match=True)
            if not 0 < entry['native_cycles'] <= entry['cycles']:
                raise RuntimeError('No native code executed, or invalid native cycle count')
            entry['native_percent'] = 100*entry['native_cycles']/entry['cycles']
            if args.oracle:
                # The oracle has no cartridge persistence and does not accept --no-save.
                oracle_common=[rom,'--frames',args.frames,'--input',args.route.resolve()]
                run([build/'rose_oracle.exe', *oracle_common, '--align', align,
                     '--hash-out', logs/f'a{align}-oracle.txt'], logs/f'a{align}-oracle.log')
                compare(logs/f'a{align}-native.txt', logs/f'a{align}-oracle.txt', args.frames, False)
                entry['independent_oracle_match'] = True
            runs.append(entry)
            print(f'Alignment {align}: {entry["native_percent"]:.6f}% native; frame hashes matched', flush=True)
        # Preserve completed gameplay evidence even if a later host-side test fails.
        report.update(runs=runs, executable_sha256=digest(exe))
        report_path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        run(['ctest', '--test-dir', build, '--output-on-failure'], logs/'tests.log')
        report.update(status='passed', runs=runs, executable_sha256=digest(exe),
                      seeds_sha256=digest(seeds), captures_sha256=digest(captures),
                      scope='Only the specified route, ROM and alignments; not the full game.')
        print(f'Build: {exe}\nReport: {report_path}', flush=True)
    except Exception as error:
        report.update(status='failed', error=str(error))
        raise
    finally:
        report_path.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')

if __name__ == '__main__':
    main()
