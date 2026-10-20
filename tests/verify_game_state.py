"""Check exact continuation after a real game save state (no RAM patches)."""
import argparse
import json
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from build_port import run

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe',type=Path,required=True)
    parser.add_argument('--rom',type=Path,required=True)
    parser.add_argument('--route',type=Path,default=ROOT/'tests/gameplay-route.txt')
    args=parser.parse_args()
    out=ROOT/'build/state-verification'
    out.mkdir(parents=True,exist_ok=True)
    report=out/'result.json'
    report.write_text('{"status":"running"}\n')
    try:
        common=[args.exe.resolve(),args.rom.resolve(),'--no-save','--frames','3600','--input',args.route.resolve()]
        state=out/'stage.cycstate'
        run([*common,'--hash-out',out/'continuous.txt','--save-state',f'2999:{state}'],out/'continuous.log')
        run([*common,'--load-state',state,'--hash-out',out/'resumed.txt'],out/'resumed.log')
        full=(out/'continuous.txt').read_text().splitlines()
        resumed=(out/'resumed.txt').read_text().splitlines()
        if len(full)!=3600 or len(resumed)!=600 or resumed!=full[3000:]:
            raise RuntimeError('Loaded-state continuation differs or is incomplete')
        result=dict(status='passed',saved_after_frame=2999,identical_continuation_frames=600)
        print('Save/load: all 600 continuation frames match, including hardware state')
    except Exception as error:
        result=dict(status='failed',error=str(error))
        raise
    finally:
        report.write_text(json.dumps(result,indent=2)+'\n')

if __name__=='__main__':main()
