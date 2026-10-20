"""Verify the extracted portable beta with no developer tools on PATH."""
import argparse
import ctypes
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import sys
import time
import zipfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from build_port import digest

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--zip',type=Path,required=True)
    parser.add_argument('--rom',type=Path,required=True)
    args=parser.parse_args()
    base=ROOT/'build/beta-package-test'
    base.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(args.zip) as archive:
        assert not any(Path(n).suffix.lower() in ('.nes','.zip','.sav','.cycstate') for n in archive.namelist())
        for name in archive.namelist():
            assert (base/name).resolve().is_relative_to(base.resolve())
        archive.extractall(base)
        folder=base/archive.namelist()[0].split('/')[0]
    exe=folder/'RoseRecompiled.exe'
    info=json.loads((folder/'build-info.json').read_text())
    assert digest(folder/'runtime/RoseRuntime.exe')==info['runtime_sha256']
    env=dict(os.environ,PATH='',SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
    flags=getattr(subprocess,'CREATE_NO_WINDOW',0)
    def command(argv,expected=0):
        result=subprocess.run([str(x) for x in argv],env=env,creationflags=flags,timeout=45)
        assert result.returncode==expected,(argv,result.returncode)
    command([exe,'--check-rom',args.rom.resolve()])
    wrong=base/'invalid.nes';wrong.write_bytes(b'not a ROM')
    command([exe,'--check-rom',wrong],2)
    command([exe,wrong,'--headless'],2)
    # Check the ZIP's exact bytes in a real launch -> runtime -> clean exit.
    command([exe,args.rom.resolve(),'--frames','2400','--no-save',
             '--input',ROOT/'tests/gameplay-route.txt',
             '--screenshot',base/'packaged-game.png'])
    assert (base/'packaged-game.png').stat().st_size>100
    assert (folder/'beta.log').stat().st_size>0
    # Test double-click's no-argument branch using a remembered ROM, entirely
    # offscreen, then close it through the runtime's own local control port.
    (folder/'launcher.ini').write_text('[Launcher]\nROM='+str(args.rom.resolve())+'\n',encoding='utf-16')
    with socket.socket() as reserve:
        reserve.bind(('127.0.0.1',0));port=reserve.getsockname()[1]
    local=dict(env,NESRECOMP_CYC_TCP=str(port))
    process=subprocess.Popen([str(exe)],env=local,creationflags=flags)
    try:
        deadline=time.monotonic()+12
        while True:
            try:
                conn=socket.create_connection(('127.0.0.1',port),timeout=1);break
            except OSError:
                if time.monotonic()>deadline or process.poll() is not None:raise RuntimeError('Remembered ROM did not start')
                time.sleep(.1)
        with conn,conn.makefile('rwb',buffering=0) as stream:
            def request(cmd,**fields):
                stream.write((json.dumps(dict(cmd=cmd,**fields))+'\n').encode())
                result=json.loads(stream.readline());assert result['ok'],result
                return result
            request('menu',open=True)
            time.sleep(.1);a=request('state');time.sleep(.1);b=request('state')
            assert a['menu_open'] and a['frame']==b['frame']
            request('quit')
        process.wait(timeout=5);assert process.returncode==0
    finally:
        if process.poll() is None:process.kill();process.wait()
    result=dict(status='passed',archive_sha256=digest(args.zip),runtime_byte_identical=True,
                no_rom_in_archive=True,empty_path_launch=True,compatible_rom_check=True,
                invalid_rom_rejected=True,remembered_rom_no_arguments=True,menu_pauses=True)
    (base/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
