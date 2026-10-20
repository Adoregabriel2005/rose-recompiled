"""Recheck completed run artifacts after fixing host-side tests, without
rerunning an unchanged game binary. Fails if executable or route is newer than
the runs, traces are incomplete, a comparison fails, or host tests fail.
"""
import json
from pathlib import Path
import re

from build_port import ROOT, compare, digest, run

def main():
    logs=ROOT/'build/verification'
    path=logs/'result.json'
    report=json.loads(path.read_text())
    exe=ROOT/'build/game/RoseRecompiled.exe'
    route=ROOT/'tests/gameplay-route.txt'
    if digest(route)!=report['route_sha256']:
        raise RuntimeError('Recorded route changed')
    if (ROOT/'engine.lock').read_text().strip()!=report['engine']:
        raise RuntimeError('Framework revision changed')
    if report.get('executable_sha256') and digest(exe)!=report['executable_sha256']:
        raise RuntimeError('Recorded executable changed')
    runs=[]
    for align in report['alignments']:
        native=logs/f'a{align}-native.txt'
        oracle=logs/f'a{align}-oracle.txt'
        interp=logs/f'a{align}-interp.txt'
        for trace in (native,oracle,interp):
            if trace.stat().st_mtime<max(exe.stat().st_mtime,route.stat().st_mtime):
                # Bootstrap interpreter traces intentionally predate the final
                # native binary, but must still postdate the unchanged route.
                if trace!=interp or trace.stat().st_mtime<route.stat().st_mtime:
                    raise RuntimeError(f'Artifact predates executable or route: {trace}')
        compare(native,interp,report['frames'])
        compare(native,oracle,report['frames'],False)
        text=(logs/f'a{align}-native.log').read_text()
        m=re.search(r'mode=native frames=(\d+) cycles=(\d+) native_cycles=(\d+)',text)
        if not m or int(m[1])!=report['frames']:
            raise RuntimeError('Missing native completion')
        if f'mode=oracle frames={report["frames"]} cycles=' not in (logs/f'a{align}-oracle.log').read_text():
            raise RuntimeError('Missing oracle completion')
        cycles,native_cycles=int(m[2]),int(m[3])
        if not 0<native_cycles<=cycles:
            raise RuntimeError('No valid native execution')
        runs.append(dict(align=align,cycles=cycles,native_cycles=native_cycles,
                         native_percent=100*native_cycles/cycles,
                         interpreter_match=True,independent_oracle_match=True))
    run(['ctest','--test-dir',ROOT/'build/game','--output-on-failure'],logs/'tests.log')
    profile=ROOT/'private/roms'/report['rom_sha256']/'profiles'/report['engine']
    report.pop('error',None)
    report.update(status='passed',runs=runs,executable_sha256=digest(exe),
                  seeds_sha256=digest(profile/'seeds.txt'),captures_sha256=digest(profile/'captures.txt'),
                  scope='Only the specified route, ROM and alignments; not the full game.',
                  finalization='Completed run artifacts rechecked; host tests rerun after workspace temp-path fix.')
    path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,indent=2))

if __name__=='__main__':main()
