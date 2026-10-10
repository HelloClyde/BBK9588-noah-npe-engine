import fs from 'node:fs';
import {spawnSync} from 'node:child_process';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const cases=['magic-retain','cancel-defend','cancel-throw','replace-back'];
const name=process.argv[2],output='tmp/native-battle-skill-resident.json';
if(!name){fs.writeFileSync(output,'[]\n');for(const c of cases){const p=spawnSync(process.execPath,[process.argv[1],c],{stdio:'inherit'});if(p.error)throw p.error;if(p.status)throw Error(c+' failed '+p.status);}process.exit(0);}
if(!cases.includes(name))throw Error('unknown case');
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
try{
 rt.restoreBytes(fs.readFileSync('tmp/battle-fresh-start.bin'));rt.drainEventsBytes();
 const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
 for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===3&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7a58]===9&&mem()[i+0x7a66]===6)found.push(i);
 if(found.length!==1)throw Error('RAM discovery');
 const base=found[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v&255;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 const tick=mask=>rt.advance(mask).free(),idle=n=>{for(let t=0;t<n;++t)tick(0);},key=mask=>{tick(mask);idle(15);};
 const hero=mem().slice(a(0x69c6)-1,a(0x69c6+148)-1);
 for(let i=1;i<3;++i)mem().set(hero,a(0x69c6+i*148)-1);
 putB(0x7afa,3);putB(0x7626,51);putB(0x7627,51);putW(0x7ab4,1);idle(30);
 for(let i=0;i<6;++i){const d=i*148;
  putW(0x649e+d,i<3?200:2000);putW(0x64a0+d,i<3?50:1000);putW(0x64a2+d,120);putW(0x64a4+d,100);
  putW(0x64a6+d,1);putW(0x64a8+d,i<3?999-i:1);putW(0x64aa+d,1);putW(0x64ac+d,i<3?90:1);putW(0x64b0+d,i<3?90:1);putW(0x64ae+d,0);putW(0x64b2+d,0);putB(0x6468+d,99);
  for(let j=0;j<4;++j)putW(0x648e+d+j*2,i>=3&&j===2?-36:0);
  for(let j=0;j<18;++j)putB(0x6469+d+j,i<3?(j===0?6:j===1?4:0):0);
  if(i>=3){putW(0x649c+d,0);putW(0x64b4+d,0);putB(0x64ca+d,0);putB(0x64cb+d,0);}
  else {putW(0x649c+d,0);putW(0x649a+d,10);putB(0x69de+d,99);for(let j=0;j<18;++j)putB(0x69df+d+j,j===0?6:j===1?4:0);}
 }
 for(let id=0;id<255;++id)putW(0x67c8+2*id,0);putW(0x67c8+2*46,1);putW(0x67c8+2*159,1);
 const state=()=>({mode:w(0x7934),menu:b(0x7ac8),actor:b(0x7a9e),execution:b(0x7acc),event:w(0x7936),
  skillCache:{id:b(0x7a74),bytes:Array.from({length:144},(_,i)=>b(0x730e+i))},stock:[46,159].map(id=>w(0x67c8+2*id)),cache:{id:b(0x7a72),bytes:Array.from({length:142},(_,i)=>b(0x739e+i))},
  commands:Array.from({length:3},(_,i)=>{const d=148*i;return {kind:b(0x64cf+d),target:b(0x64ce+d),item:b(0x64d5+d),skill:b(0x64d6+d),magicType:b(0x64d0+d),type:b(0x64d1+d),copy:b(0x64d2+d),group:b(0x64d3+d),side:b(0x64d4+d)};}),
  records:Array.from({length:6},(_,i)=>{const d=148*i;return {hp:w(0x64a0+d),maxHp:w(0x649e+d),qi:w(0x64a4+d),maxQi:w(0x64a2+d),attack:w(0x64b0+d),defense:w(0x64b2+d),level:b(0x6468+d),status:Array.from({length:4},(_,j)=>w(0x648e+d+j*2))};})});
 const phases=[];const mark=label=>{phases.push({label,state:state()});};
 const item=(id,cancel)=>{
  for(const mask of [4,2,16])key(mask);for(let n=0;b(0x7a9c)!==1&&n<6;++n)key(2);key(16);
  if(b(0x7aa2)!==(id===159?1:0))key(id===159?2:1);key(16);
  if(b(0x7ac8)!==33)throw Error('list gate '+JSON.stringify(state()));
  for(let n=0;w(0x7a4c)!==id&&n<4;++n)key(8);
  if(w(0x7a4c)!==id)throw Error('selected gate '+w(0x7a4c));
  key(16);if(b(0x7ac8)!==37)throw Error('target gate '+b(0x7ac8));mark('target-'+b(0x7a9e));
  if(cancel){key(64);mark('target-canceled-'+b(0x7a9e));for(let n=0;b(0x7ac8)!==30&&n<4;++n)key(64);if(b(0x7ac8)!==30)throw Error('base gate');mark('base-'+b(0x7a9e));}
  else key(16);
 };
 const attack=()=>{for(const mask of [1,8,16])key(mask);if(b(0x7ac8)!==17)throw Error('attack target gate '+JSON.stringify(state()));key(16);};
 const defend=confirm=>{if(b(0x7ac8)!==30){key(4);key(2);}key(16);for(let n=0;b(0x7a9c)!==2&&n<6;++n)key(2);if(b(0x7ac8)!==31)throw Error('defend gate');if(confirm)key(16);};
 const magic=(id,cancel)=>{
  for(const mask of [4,1,16])key(mask);
  if(b(0x7ac8)!==21)throw Error('magic list gate '+JSON.stringify(state()));
  if(id===4)key(8);key(16);mark('magic-target-'+id);
  if(b(0x7a9e)!==0 || b(0x7ac8)===21)throw Error('magic target gate '+JSON.stringify(state()));
  if(cancel){key(64);mark('magic-canceled-'+id);for(let n=0;b(0x7ac8)!==30&&n<4;++n)key(64);mark('magic-base-'+id);}
  else key(16);
 };
 const initial=state(),rounds=[],frames=[];
 const run=()=>{
  const before=state(),start=frames.length;
  for(let t=0;t<300;++t){tick(t===0?16:0);for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(pixels){const index=frames.length;fs.writeFileSync(`tmp/native-battle-skill-resident-${name}-${index}.idx`,pixels);frames.push({index,round:rounds.length,tick:t,batchState:state()});}}}
  if(w(0x7934)!==3 || b(0x7ac8)!==30 || b(0x7a9e)!==0)throw Error('round gate '+JSON.stringify(state()));
  rounds.push({before,after:state(),start,count:frames.length-start});
 };
 magic(6,name!=='magic-retain');
 if(name==='cancel-defend')defend(true);
 else if(name==='cancel-throw')item(159,false);
 else if(name==='replace-back'){
  magic(4,false);key(64);mark('back');defend(true);mark('back-defend');
 }
 defend(true);defend(false);run();mark('round-one');
 defend(true);mark('repeat-first');defend(true);mark('repeat-second');defend(false);run();
 const after=state(),rows=JSON.parse(fs.readFileSync(output)).filter(c=>c.name!==name);
 rows.push({name,initial,phases,rounds,after,frames});fs.writeFileSync(output,JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name,phases:phases.map(p=>[p.label,p.state.commands]),rounds:rounds.map(r=>({before:r.before.commands,after:r.after.commands,count:r.count}))}));
}finally{rt.free();}
