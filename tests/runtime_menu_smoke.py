"""Exercise the actual SDL host on invisible dummy video/audio devices."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import time

exe, rom, image = map(lambda x: str(Path(x).resolve()), sys.argv[1:4])
with socket.socket() as reserve:
    reserve.bind(('127.0.0.1', 0))
    port = reserve.getsockname()[1]
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
log = Path(image).with_suffix('.log')
with log.open('w', encoding='utf-8') as output:
    process = subprocess.Popen([exe, rom, '--hidden', '--no-save', '--tcp', str(port)],
                               env=env, stdout=output, stderr=output,
                               creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
    try:
        deadline=time.monotonic()+15
        while True:
            try:
                conn=socket.create_connection(('127.0.0.1',port),timeout=1)
                break
            except OSError:
                if process.poll() is not None or time.monotonic()>deadline:
                    raise RuntimeError(log.read_text())
                time.sleep(.1)
        conn.settimeout(5)
        with conn, conn.makefile('rwb',buffering=0) as stream:
            def command(cmd, **fields):
                stream.write((json.dumps(dict(cmd=cmd,**fields))+'\n').encode())
                result=json.loads(stream.readline())
                assert result.get('ok'), result
                return result
            command('menu',open=True)
            time.sleep(.1)
            first=command('state')
            time.sleep(.2)
            second=command('state')
            assert first['menu_open'] and second['frame']==first['frame'], (first,second)
            command('screenshot',layer='ui',path=image)
            for tab in ('som', 'video', 'controles', 'sistema', 'inicio'):
                command('menu',nav='right')
                shot = str(Path(image).with_name(Path(image).stem + '-' + tab + '.png'))
                command('screenshot',layer='ui',path=shot)
                assert command('state')['frame']==second['frame']
            command('menu',nav='down')
            command('menu',open=False)
            time.sleep(.2)
            resumed=command('state')
            assert not resumed['menu_open'] and resumed['frame']>second['frame']
            command('quit')
        process.wait(timeout=5)
        assert process.returncode==0
        print('Runtime menu: pause, navigation, screenshot and resume passed')
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
