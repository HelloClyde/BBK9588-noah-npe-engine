import fs from 'node:fs';
import {spawnSync} from 'node:child_process';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const cases=[
 {name:'same',cached:6}, {name:'fresh',cached:1},
 {name:'queued-flags',cached:6,changedFlags:true},
 {name:'forgotten-prefix',cached:6,forget:true},
 {name:'fee-after-cap',cached:6,qi:119},
 {name:'two-same',cached:6,second:6},
 {name:'two-different',cached:6,second:4},
];
const preparedRecords=process.argv.includes('--prepared-records');
if(preparedRecords)cases.push({name:'two-status',cached:6,second:20},{name:'two-group',cached:6,second:10},{name:'two-own-group',cached:6,second:9},{name:'zero-numeric',cached:6,zeroNumeric:true});
const prefix=preparedRecords?'tmp/native-battle-prepared-skill-record':'tmp/native-battle-execution-skill-record';
const name=process.argv.slice(2).find(v=>!v.startsWith('--')),output=prefix+'.json';
if(!name){fs.writeFileSync(output,'[]\n');for(const c of cases.filter(c=>!preparedRecords||['same','two-different','two-status','two-group','two-own-group','zero-numeric'].includes(c.name))){const p=spawnSync(process.execPath,[process.argv[1],c.name,...(preparedRecords?['--prepared-records']:[])],{stdio:'inherit'});if(p.error)throw p.error;if(p.status)throw Error(c.name+' failed '+p.status);}process.exit(0);}
const cfg=cases.find(c=>c.name===name);if(!cfg)throw Error('unknown case');
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
  putW(0x649e+d,i<3?200:2000);putW(0x64a0+d,i<3?50:1000);
  putW(0x64a2+d,120);putW(0x64a4+d,i===0?(cfg.qi??100):100);
  putW(0x64a6+d,1);putW(0x64a8+d,i<3?999-i:1);putW(0x64aa+d,1);
  putW(0x64ac+d,i<3?90:1);putW(0x64b0+d,i<3?90:1);
  putW(0x64ae+d,0);putW(0x64b2+d,0);putB(0x6468+d,99);
  for(let j=0;j<4;++j)putW(0x648e+d+j*2,i>=3&&j===2?-36:0);
  if(i>=3)for(let j=0;j<3;++j)putB(0x6469+d+j,0);
 }
 for(let i=0;i<3;++i){putB(0x69de+148*i,99);for(let j=0;j<18;++j)putB(0x69df+148*i+j,j===0?(i===1?cfg.second??6:6):0);}
 const cache=()=>({id:b(0x7a74),bytes:Array.from({length:144},(_,i)=>b(0x730e+i))});
 const state=()=>({mode:w(0x7934),actor:b(0x7a9e),menu:b(0x7ac8),cache:cache(),
  prepared:preparedRecords?Array.from({length:6},(_,i)=>Array.from({length:12},(_,j)=>b(0x64b6+148*i+j))):undefined,
  commands:Array.from({length:3},(_,i)=>({kind:b(0x64cf+148*i),id:b(0x64d6+148*i),target:b(0x64ce+148*i),type:b(0x64d0+148*i),group:b(0x64d3+148*i),side:b(0x64d4+148*i),flag:b(0x64e2+148*i)})),
  records:Array.from({length:6},(_,i)=>{const d=i*148;return {hp:w(0x64a0+d),maxHp:w(0x649e+d),qi:w(0x64a4+d),maxQi:w(0x64a2+d),attack:w(0x64b0+d),defense:w(0x64b2+d),level:b(0x6468+d),status:Array.from({length:4},(_,j)=>w(0x648e+d+j*2))};})});
 const initial=state();
 const magic=()=>{const actor=b(0x7a9e);for(const mask of [4,1,16,16])key(mask);
  if(b(0x7a9e)===actor){
   if(preparedRecords&&actor===1&&[4,20].includes(cfg.second)){key(2);if(b(0x7ac0)!==5)throw Error('prepared spell target '+b(0x7ac0));}
   key(16);
  }};
 const prepareDefend=()=>{if(b(0x7ac8)!==30){key(4);key(2);}key(16);for(let n=0;b(0x7a9c)!==2&&n<6;++n)key(2);if(b(0x7ac8)!==31)throw Error('defend gate');};
 magic();if(b(0x7a9e)!==1)throw Error('queue first');
 if(cfg.second)magic();else{prepareDefend();key(16);}
 if(b(0x7a9e)!==2)throw Error('queue second');
 prepareDefend();
 // Change only resident RAM after the native controller has saved metadata.
 putB(0x7a74,cfg.cached);for(let i=0;i<144;++i)putB(0x730e+i,0xa5);
 const title=Buffer.from('bbbac4e6cfc9caf5','hex');for(let i=0;i<12;++i)putB(0x730e+i,title[i]??0);putB(0x731a,0);
 const desc=Buffer.from('bec9cbb5c3f700','hex');for(let i=0;i<desc.length;++i)putB(0x731b+i,desc[i]);
 for(let i=114;i<144;++i)putB(0x730e+i,0);
 putB(0x7381,1);putB(0x7382,cfg.changedFlags?2:1);putB(0x7383,cfg.changedFlags?1:0);putB(0x7384,cfg.changedFlags?0:1);
 putW(0x7386,cfg.zeroNumeric?0:7);putW(0x7388,cfg.zeroNumeric?0:3);putW(0x738e,5);
 if(cfg.forget){for(let i=0;i<18;++i)putB(0x69df+i,0);putB(0x69de,0);putB(0x6468,0);}
 if(preparedRecords)for(let i=0;i<6;++i)for(let j=0;j<6;++j)putW(0x64b6+148*i+2*j,100*(i+1)+j+1);
 const before=state();const frames=[];
 for(let t=0;t<300;++t){tick(t===0?16:0);for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(pixels){const index=frames.length;fs.writeFileSync(`${prefix}-${name}-${index}.idx`,pixels);frames.push({index,tick:t});}}}
 const after=state();if(after.actor!==0||after.mode!==3||![20,30].includes(after.menu)){fs.writeFileSync(`${prefix}-${name}-failed.json`,JSON.stringify({before,after,frames}));throw Error('round end '+after.actor+'/'+after.menu);}
 const rows=JSON.parse(fs.readFileSync(output)).filter(c=>c.cfg.name!==name);rows.push({cfg,initial,before,after,frames});fs.writeFileSync(output,JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name,frames:frames.length,before:before.cache.id,after:after.cache.id,party:after.records.slice(0,3).map(r=>[r.hp,r.qi]),enemy:after.records.slice(3).map(r=>r.hp)}));
}finally{rt.free();}
