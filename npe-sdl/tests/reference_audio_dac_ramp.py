"""Execute the unmodified PRG DAC ramps in an isolated CPU diagnostic.

This measures word transitions/write counts, not their physical timing or a
codec. It never loads NAND, writes guest RAM from the host, or joins a game.
"""
import argparse, hashlib, json, re, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path('scripts').resolve()))
from web.np560_web import Emulator

parser=argparse.ArgumentParser()
parser.add_argument('--qemu',type=Path,required=True)
args=parser.parse_args()
out=Path('tmp/audio-dac-ramp-native');out.mkdir(exist_ok=True)
original=Path('tmp/NP560.PRG').read_bytes()
resident=bytearray(b'\x00\x0f'*(0x0834c4//2))
resident[0x0833fa:0x0834c4]=original[0x0533fa:0x0534c4]
rom=out/'original-ramp.rom';rom.write_bytes(resident)
cases=[]
for name,left,right,target_left,target_right,entry in [
 ('stop-opposite',3,-3,0,0,0x3462),
 ('stop-left-only',-4,0,0,0,0x3462),
 ('stop-right-only',0,4,0,0,0x3462),
 ('stop-zero',0,0,0,0,0x3462),
 ('same-positive-target',-2,4,2,2,0x3462),
 ('same-negative-target',2,-4,-2,-2,0x3462),
 ('start-opposite',0,0,3,-3,0x33fa),
 ('start-left-only',0,-3,3,-3,0x33fa),
]:
 code=[0x6800,2,0x68ff,0x6000,0x6810,0,0x6870,left&65535,0x6871,right&65535,0x6872,1]
 # Ramp arguments: repeat count then target(s), reverse stack order.
 for value in ([95,target_left] if entry==0x3462 else [95,target_right,target_left]):
  code.extend([0x68f0,value&65535,0x58f0])
 code.extend([0xbd08,entry,0x50f4 if entry==0x3462 else 0x50f6])
 pc=len(code)*2;code.append(0xc701)
 firmware=out/(name+'.prg');firmware.write_bytes(struct.pack('<%dH'%len(code),*code))
 emulator=None
 try:
  emulator=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,soc_rom_skeleton=True,start_paused=True)
  emulator.qmp.execute('cont');deadline=time.monotonic()+5;state=''
  while time.monotonic()<deadline:
   state=emulator.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
   if f'PC=20:{pc:04x}'.lower() in state.lower():break
   time.sleep(.01)
  emulator.qmp.execute('stop')
  assert f'PC=20:{pc:04x}'.lower() in state.lower(),state[:500]
  (out/(name+'.cpu.txt')).write_text(state,encoding='utf-8')
  for marker in ['ROM_MISSING=none','INVALID_CONTROL=none']:
   assert marker in state,(name,marker,state[:500])
  packet=emulator.audio()
  assert packet['registers']==[target_left&65535,target_right&65535,1],packet['registers']
  expected=[1+abs(left-target_left),1+abs(right-target_right),1]
  assert packet['writes']==expected,(name,packet['writes'],expected)
  regs={int(k):int(v,16) for k,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]+)','\n'.join(state.splitlines()[:5]),re.I)}
  assert regs[15]==0x6000,regs
  transitions=[]
  direct=next(line for line in state.splitlines() if line.startswith('DIRECT_SFR_WRITE='))
  for bank,pc,channel,old,new in re.findall(r'/([0-9a-f]+):([0-9a-f]+)/r(70|71)=([0-9a-f]+)>([0-9a-f]+)',direct,re.I):
   if int(bank,16)!=8:continue
   signed=lambda v: v if v<32768 else v-65536
   transitions.append({'channel':int(channel,16)-0x70,'old':signed(int(old,16)),
                       'new':signed(int(new,16)),'pc':f'08:{int(pc,16):04X}'})
  assert len(transitions)==abs(left-target_left)+abs(right-target_right),transitions
  row={'name':name,'entry':f'08:{entry:04X}','input':[left,right],
       'target':[target_left,target_right],'repeat_nops':95,'writes':packet['writes'],
       'registers':packet['registers'],'transitions':transitions,
       'host_guest_memory_writes':0,'nand_used':False,'passed':True}
  cases.append(row);print(json.dumps(row),flush=True)
 finally:
  if emulator:emulator.close()
report={'passed':len(cases)==8,'original_prg_sha256':hashlib.sha256(original).hexdigest(),
        'qemu_sha256':hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
        'scope':'original signed word steps and register-write counts; no timing/codec proof','cases':cases}
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
