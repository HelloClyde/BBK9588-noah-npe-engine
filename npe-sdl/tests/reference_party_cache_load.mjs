import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
async function main(){
 const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
 const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
 const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot'),mem=()=>new Uint8Array(exp.memory.buffer);
 const stateAt=(base,m=mem())=>{const a=o=>base+o-(o<0x7000?8:0),b=o=>m[a(o)-1],w=o=>b(o)*256+m[a(o)];return{
  mode:w(0x7934),event:w(0x7936),context:w(0x7938),map:w(0x7a58),progress:w(0x7a66),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),money:(w(0x799e)*65536+w(0x79a0))>>>0,enabled:w(0x7a50),slot:b(0x7a04),periodCounter:b(0x7a32),menu:b(0x7a54),propertyChoice:b(0x7a18),member:b(0x7adc),equipmentSlot:b(0x7a2a),ui:b(0x7a4a),partyCount:b(0x7afa),menuPartyCount:b(0x7ab0),actors:Array.from({length:3},(_,i)=>{const o=148*i;return{id:b(0x69d2+o),active:b(0x69f1+o),portrait:w(0x6a02+o),level:b(0x69de+o),maxHp:w(0x6a14+o),hp:w(0x6a16+o),maxQi:w(0x6a18+o),qi:w(0x6a1a+o),attack:w(0x6a22+o),defense:w(0x6a24+o),experience:w(0x6a12+o),experienceToNext:w(0x6a10+o),otherStats:Array.from({length:3},(_,s)=>w(0x6a1c+o+s*2)),gear:Array.from({length:6},(_,s)=>b(0x69d8+o+s)),guards:Array.from({length:4},(_,s)=>w(0x6a04+o+s*2)),skills:Array.from({length:18},(_,s)=>b(0x69df+o+s)),raw:Array.from({length:148},(_,s)=>b(0x69c6+o+s))};}),gearSelected:b(0x7ab2),descLength:w(0x7aba),descStart:w(0x7abe),descEnd:w(0x7abc),
  npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};})};};


 const tick=mask=>{const out=rt.advance(mask);out.free();};
 const key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
 const events=JSON.parse(fs.readFileSync('tmp/real-party-events-index.json'));
 const extraOnly=process.argv.includes('--extra-only');
 const rows=extraOnly?JSON.parse(fs.readFileSync('tmp/native-party-cache-load.json')).filter(r=>r.targetMap!==37):[];
 for(const targetMap of extraOnly?[37]:[1,20,64,2,37])for(const entry of ['system','title']){
  rt.restoreBytes(snapshot);rt.drainEventsBytes();const m=mem(),candidates=[];
  for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===1&&m[i+0x7935]===0&&m[i+0x7a58]===1&&m[i+0x7a66]===1&&m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6)candidates.push(i);
  key(64);const live=candidates.filter(i=>stateAt(i).mode===6);if(live.length!==1)throw Error('cache base '+live.length);
  const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)],putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>8);mem()[a(o)]=v&255;};
  const cache=()=>Array.from({length:3},(_,i)=>{const o=0x78a2+i*6;return{add:b(o),character:b(o+1),nextMode:b(o+2),padding:b(o+3),nextId:w(o+4)};});
  const bootCache=cache();key(64);
  putW(0x7a58,targetMap);putB(0x69d5,1);putB(0x69d6,1);putB(0x7a2e,0);putB(0x7a2c,0);
  for(const mask of [64,2,2,2,16,2,16,16])key(mask);
  if(w(0x7934)!==1)throw Error('cache save');
  if(entry==='title'){putW(0x7934,7);putW(0x7ab4,1);for(let t=0;t<32;++t)tick(0);}
  for(const mask of entry==='system'?[64,2,2,2,16,16]:[2,16])key(mask);
  const seeds=[events.find(e=>e.map===120&&e.event===1),events.find(e=>e.map===20&&e.event===1),events.find(e=>e.map===64&&e.event===1)];
  for(let i=0;i<3;++i){const o=0x78a2+i*6,e=seeds[i];putB(o,e.add);putB(o+1,e.character);putB(o+2,e.nextMode);putB(o+3,0x80+i);putW(o+4,e.nextId);}
  const beforeCache=cache(),before=stateAt(base);key(16);
  if(w(0x7934)!==1||w(0x7a58)!==targetMap)throw Error('cache load');
  const afterCache=cache(),after=stateAt(base);
  rows.push({entry,targetMap,bootCache,beforeCache,afterCache,before,after});fs.writeFileSync('tmp/native-party-cache-load.json',JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({entry,targetMap,bootCache,beforeCache,afterCache}));
 }
 rt.free();
}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
