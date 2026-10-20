"""Build a graphical starter and package the verified runtime without a ROM."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

from build_port import ROOT, digest, run
from import_rom import inspect, read_rom

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom',type=Path,required=True)
    parser.add_argument('--version',default='0.2.0-beta.1')
    args=parser.parse_args()
    if any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-' for c in args.version):
        parser.error('Invalid version')
    report=json.loads((ROOT/'build/verification/result.json').read_text())
    runtime=ROOT/'build/game/RoseRecompiled.exe'
    info=inspect(read_rom(args.rom.resolve()))
    if report.get('status')!='passed' or digest(runtime)!=report.get('executable_sha256') or info['sha256']!=report['rom_sha256']:
        parser.error('Runtime or ROM does not match the passed verification report')
    logs=ROOT/'build/packaging';logs.mkdir(parents=True,exist_ok=True)
    run(['cmake','-S',ROOT/'launcher','-B',ROOT/'build/launcher','-G','MinGW Makefiles',
         '-DCMAKE_CXX_COMPILER=clang++','-DCMAKE_BUILD_TYPE=Release',
         f'-DROSE_PAYLOAD_SHA256={info["payload_sha256"]}'],logs/'configure.log')
    run(['cmake','--build',ROOT/'build/launcher','--parallel','4'],logs/'build.log')
    name=f'RoseRecompiled-{args.version}-Windows-x64'
    stage=ROOT/'build/packages'/name
    # Never delete or merge an existing package: releases remain reviewable.
    stage.mkdir(parents=True,exist_ok=False)
    (stage/'runtime').mkdir();(stage/'LICENSES').mkdir()
    shutil.copy2(ROOT/'build/launcher/RoseRecompiled.exe',stage/'RoseRecompiled.exe')
    shutil.copy2(runtime,stage/'runtime/RoseRuntime.exe')
    shutil.copy2(ROOT/'build/game/SDL2.dll',stage/'runtime/SDL2.dll')
    for source,dest in [
        (ROOT/'external/nesrecomp/LICENSE','NESRecomp.txt'),
        (ROOT/'external/nesrecomp/runner/external/SDL2/COPYING.txt','SDL2.txt'),
        (ROOT/'external/nesrecomp/runner/cyc/vendor/emu2413/LICENSE','emu2413.txt')]:
        shutil.copy2(source,stage/'LICENSES'/dest)
    compiler=Path(shutil.which('clang++')).resolve()
    llvm_license=compiler.parent.parent/'LICENSE.TXT'
    if llvm_license.exists():shutil.copy2(llvm_license,stage/'LICENSES/LLVM-MinGW.txt')
    notes=stage/'data/system/.cache/.notes.txt';notes.parent.mkdir(parents=True)
    shutil.copy2(ROOT/'assets/system/.cache/.notes.txt',notes)
    (stage/'LEIA-ME.txt').write_text(
        f'ROSE RECOMPILED {args.version}\nWindows x64 - beta experimental\n\n'
        '1. Extraia o ZIP inteiro para uma pasta com permissao de escrita.\n'
        '2. Abra RoseRecompiled.exe por duplo clique.\n'
        '3. Na primeira vez, selecione sua ROM .nes compativel.\n'
        '4. Nas proximas vezes, a ROM selecionada sera lembrada.\n\n'
        'Esta beta foi compilada para a ROM USA fornecida pelo autor. Outras regioes/revisoes ainda nao sao compativeis com este pacote.\n'
        f'SHA256 do payload compativel (PRG+CHR, sem cabecalho/trainer):\n{info["payload_sha256"]}\n'
        'A ROM nao acompanha este pacote. Nao precisa instalar Python, CMake ou PowerShell.\n\n'
        'Controles padrao: setas para mover, Z = A, X = B, Enter = Start, barra invertida = Select.\n'
        'Escape abre/fecha o menu Rose Recompiled e pausa. No cabecalho, esquerda/direita troca abas; baixo entra nas opcoes.\n'
        'Para trocar a ROM lembrada, remova launcher.ini e abra novamente.\n'
        'Saves e config.ini ficam na pasta runtime. Mantenha o pacote extraido.\n'
        'Na aba VIDEO, FORMATO DA IMAGEM oferece ORIGINAL 4:3, 16:9 ESTICADO e 16:9 AMPLO.\n'
        'O modo AMPLO mostra cenario adicional e ativa inimigos comuns dentro da area 16:9 em salas horizontais compativeis.\n\n'
        'Ao relatar um problema, envie beta.log, versao, fase/local, passos e captura ou video. Nao envie a ROM.\n'
        'Validado: rota de 6000 frames da primeira fase, quatro alinhamentos CPU/PPU, comparacao com TriCNES e continuacao de save state.\n'
        'O modo amplo desta beta foi validado na rota inicial; salas especiais podem voltar ao enquadramento centralizado.\n'
        'O jogo inteiro, todos os controles fisicos e outras regioes/revisoes ainda precisam de testes.\n\n'
        'Dependencias e avisos em LICENSES. Framework: https://github.com/mstan/nesrecomp\n'
        f'Revisao do framework: {report["engine"]}\n',encoding='utf-8-sig')
    manifest=dict(version=args.version,platform='Windows x64',rom_included=False,
                  compatible_payload_sha256=info['payload_sha256'],
                  runtime_sha256=digest(stage/'runtime/RoseRuntime.exe'),
                  verified_gameplay_report=report)
    (stage/'build-info.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    archive=ROOT/'build/packages'/f'{name}.zip'
    with zipfile.ZipFile(archive,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as output:
        for file in sorted(stage.rglob('*')):
            if not file.is_file():continue
            entry=zipfile.ZipInfo.from_file(file,str(Path(name)/file.relative_to(stage)))
            if file==notes:entry.external_attr|=2  # DOS hidden bit for extractors that preserve it.
            output.writestr(entry,file.read_bytes(),compress_type=zipfile.ZIP_DEFLATED,compresslevel=9)
    (archive.with_suffix('.zip.sha256')).write_text(f'{digest(archive)}  {archive.name}\n')
    print(f'Package: {archive}\nExecutable: {stage / "RoseRecompiled.exe"}')

if __name__=='__main__':main()
