import fs from 'node:fs';
import {spawnSync} from 'node:child_process';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
// Actual external records enter only the original shared cache. No NPE,
// original program or snapshot is modified; type/target diagnostics are explicit.
const inventory=JSON.parse(fs.readFileSync('tmp/map-default-skill-candidates.json'));
const hero=inventory.find(r=>r.path.endsWith('英雄传奇.npe'));
const sword=inventory.find(r=>r.path.endsWith('新仙剑后传.npe'));
if(!hero||!sword)throw Error('missing actual default-type records');
const cases=[
 {name:'hero-single',source:hero,cached:6,skills:[6],keys:[16,16,16,64]},
 {name:'sword-single',source:sword,cached:6,skills:[6],keys:[16,16,16,64]},
 {name:'hero-fallen-target',source:hero,cached:6,members:2,fallenTarget:true,skills:[6],keys:[16,16,2,16,64]},
 {name:'hero-fee-reject',source:hero,cached:6,initialQi:79,ticks:240,skills:[6],keys:[16,16,64]},
];
const name=process.argv[2],output='tmp/native-map-default-skill.json';
if(!name){fs.writeFileSync(output,'[]\n');for(const c of cases){const p=spawnSync(process.execPath,[process.argv[1],c.name],{stdio:'inherit'});if(p.error)throw p.error;if(p.status)throw Error(c.name+' failed '+p.status);}process.exit(0);}
const c=cases.find(c=>c.name===name);if(!c)throw Error('unknown case');
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>rt.advance(mask).free();
const key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
try{
 rt.restoreBytes(fs.readFileSync('tmp/native-title-load-saved.snapshot'));rt.drainEventsBytes();
 const candidates=[];for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===1&&mem()[i+0x7935]===0&&mem()[i+0x7a58]===1&&mem()[i+0x7a66]===1&&mem()[i+0x7aec]===0xa9&&mem()[i+0x7aeb]===6)candidates.push(i);
 key(64);const live=candidates.filter(i=>mem()[i+0x7934]===6);if(live.length!==1)throw Error('RAM discovery');
 const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};key(64);
 putW(0x7a50,0);putB(0x7a32,0);
 for(let i=0;i<3;++i)putB(0x69f1+148*i,i<(c.members??1)?1:0);putB(0x7afa,c.members??1);putB(0x7ab0,c.members??1);
 putB(0x69de,99);putW(0x6a16,50);putW(0x6a14,200);putW(0x6a1a,c.initialQi??100);putW(0x6a18,120);
 if(c.signed){putW(0x6a16,65530);putW(0x6a22,32768);putW(0x6a24,65535);putW(0x6a12,65535);putW(0x6a1c,65535);putW(0x6a1e,32768);}
 for(let i=0;i<18;++i)putB(0x69df+i,c.skills[i]??0);
 const source=fs.readFileSync(c.source.path);
 const raw=Buffer.from(c.source.raw_hex,'hex');
 const section=source.readUInt32BE(0x3a+3*8);
 const start=section+source.readUInt32BE(section+4*(c.source.skill-1));
 const next=source.readUInt32BE(section+4*c.source.skill);
 const end=next===0xffffffff?section+source.readUInt32BE(0x3a+3*8+4):section+next;
 if(!source.subarray(start,end).equals(raw))throw Error('source record changed');
 const cacheBytes=Buffer.alloc(144);
 raw.copy(cacheBytes,0,0,12);cacheBytes[12]=0;raw.copy(cacheBytes,114,12,15);
 raw.copy(cacheBytes,117,37,39);raw.copy(cacheBytes,128,39,41);
 for(const [i,target] of [120,122,124,126,130,132,134,140,136,138,142].entries())raw.copy(cacheBytes,target,15+2*i,17+2*i);
 let desc=raw.subarray(41);const zero=desc.indexOf(0);if(zero>=0)desc=desc.subarray(0,zero+1);
 desc.copy(cacheBytes,13,0,Math.min(desc.length,101));
 putB(0x7a74,c.cached);for(let i=0;i<144;++i)putB(0x730e+i,cacheBytes[i]);
 if(c.members===2){
  const heroBytes=mem().slice(a(0x69c6)-1,a(0x69c6+148)-1);
  mem().set(heroBytes,a(0x69c6+148)-1);putB(0x69f1+148,1);
  if(c.fallenTarget)putW(0x6a16+148,0);
 }
 putW(0x7aba,77);putW(0x7abe,22);putW(0x7abc,37);
 const party=i=>{const d=i*148;return {hp:w(0x6a16+d),maxHp:w(0x6a14+d),qi:w(0x6a1a+d),maxQi:w(0x6a18+d),attack:w(0x6a22+d),defense:w(0x6a24+d),level:b(0x69de+d),experience:w(0x6a12+d),nextExperience:w(0x6a10+d),other:Array.from({length:3},(_,j)=>w(0x6a1c+d+2*j)),status:Array.from({length:4},(_,j)=>w(0x6a04+d+2*j))};};
 const state=()=>({mode:w(0x7934),menu:b(0x7a54),page:b(0x7a18),member:b(0x7adc),selected:b(0x7a44),row:b(0x7a20),col:b(0x7a22),first:b(0x7a24),
  loaded:b(0x7a74),bytes:Array.from({length:144},(_,i)=>b(0x730e+i)),length:w(0x7aba),start:w(0x7abe),end:w(0x7abc),
  party:party(0),party2:c.members===2?party(1):undefined});
 const seed=state();for(const mask of [64,2,16])key(mask);
 const before=state();fs.writeFileSync(`tmp/native-map-default-skill-${name}-before.idx`,rt.framePixelIndices());
 const phases=[];let index=0;
 for(const mask of c.keys){const commits=[];for(let t=0;t<(c.ticks??32);++t){tick(t===0?mask:0);
   for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){fs.writeFileSync(`tmp/native-map-default-skill-${name}-${index}.idx`,p);commits.push({index:index++,tick:t,state:state()});}}}
  phases.push({key:mask,commits,after:state()});}
 const rows=JSON.parse(fs.readFileSync(output)).filter(r=>r.name!==name);rows.push({...c,seed,before,phases,after:state()});
 fs.writeFileSync(output,JSON.stringify(rows,null,2)+'\n');console.log(JSON.stringify({name,before:before.menu,phases:phases.map(p=>({key:p.key,frames:p.commits.map(c=>c.state.menu),menu:p.after.menu,loaded:p.after.loaded,hp:p.after.party.hp,qi:p.after.party.qi,length:p.after.length,start:p.after.start,end:p.after.end,page:p.after.page,selected:p.after.selected,row:p.after.row,col:p.after.col,first:p.after.first}))}));
}finally{rt.free();}
