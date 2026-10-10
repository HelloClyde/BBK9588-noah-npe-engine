import fs from 'node:fs';
import {spawnSync} from 'node:child_process';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const cases=['group0','group1','group2','group1-defenses','group1-to-surround'];
const name=process.argv[2],output='tmp/native-character-attack-group.json';
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

 const surround=name==='group1-to-surround';
 const group=name==='group1-defenses'||surround?1:Number(name.slice(-1));putB(0x647c,group);
 if(name==='group1-defenses'){putW(0x64b2+4*148,15);putW(0x64b2+5*148,80);}
 const initial=state();key(16);
 if(b(0x7a9e)===0)key(16);
 if(b(0x7a9e)!==1)throw Error('ordinary group queue gate');
 const queued=state();
 const defend=confirm=>{if(b(0x7ac8)!==30){key(4);key(2);}key(16);for(let n=0;b(0x7a9c)!==2&&n<6;++n)key(2);if(confirm)key(16);};
 if(surround){key(4);key(2);key(16);if(b(0x7a9c)!==0)throw Error('surround row');}
 else {defend(true);defend(false);}
 const before=state(),frames=[];
 for(let t=0;t<300;++t){tick(t===0?16:0);for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(pixels){const index=frames.length;fs.writeFileSync(`tmp/native-character-attack-group-${name}-${index}.idx`,pixels);frames.push({index,tick:t});}}}
 const after=state();if(after.mode!==3)throw Error('continued gate');
 const rows=JSON.parse(fs.readFileSync(output));rows.push({name,group,surround,initial,queued,before,after,frames});fs.writeFileSync(output,JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name,group:queued.commands[0].group,hps:after.records.map(r=>r.hp),frames:frames.length}));
}finally{rt.free();}
