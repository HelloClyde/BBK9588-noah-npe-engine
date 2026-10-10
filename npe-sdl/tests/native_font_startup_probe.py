"""Run the production executable with isolated missing/corrupt font assets."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess

exe=Path('npe-sdl/build/npe-sdl.exe').resolve()
npe=Path('tmp/tjfy-world-probe.npe').resolve()
root=Path('tmp/native-font-startup-字库')
root.mkdir(exist_ok=True)
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
rows=[]
for name,missing,bad in [('missing12','gbk12-native.bin',False),
                         ('corrupt12','gbk12-native.bin',True),
                         ('missing16','gbk16-native.bin',False),
                         ('corrupt16','gbk16-native.bin',True)]:
    directory=root/name;assets=directory/'assets';assets.mkdir(parents=True,exist_ok=True)
    binary=directory/'npe-sdl.exe';shutil.copy2(exe,binary)
    # A valid generic table remains present to detect the old replacement.
    for font in ['gbk12.bin','gbk12-native.bin','gbk16-native.bin']:
        destination=assets/font
        if font==missing:
            if bad:destination.write_bytes(b'INVALID!')
            elif destination.exists():destination.unlink()
        else:shutil.copy2(Path('npe-sdl/assets')/font,destination)
    result=subprocess.run([str(binary.resolve()),'--new-game',str(npe)],env=env,
        stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=10)
    log=directory/'startup.log';log.write_bytes(result.stdout+b'\nSTDERR\n'+result.stderr)
    error=result.stderr.decode('utf8')
    assert result.returncode==1 and 'required native bitmap font unavailable:' in error,(name,result.returncode,error)
    assert missing in error and 'opening scene animation' not in error
    rows.append(dict(name=name,exit_code=result.returncode,missing_or_corrupt=missing,
        generic_table_present=True,utf8_asset_directory=True,log=log.as_posix(),
        log_sha256=hashlib.sha256(log.read_bytes()).hexdigest()))
report=dict(passed=True,cases=rows,executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
    scope='Production --new-game rejects missing/corrupt required native font before the game loop; valid generic font present in every isolated directory. Original build assets unchanged. Healthy production startup covered by title/NPC/walk short regressions.')
Path('tmp/native-font-startup-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print('4 production font startup cases rejected replacement/partial rendering; UTF-8 asset paths passed')
