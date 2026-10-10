/* Original public map input, resident RAM injection; no ROM or NPE edits. */
import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
rt.restoreBytes(fs.readFileSync('tmp/native-title-load-saved.snapshot'));rt.drainEventsBytes();
const mem=()=>new Uint8Array(exp.memory.buffer),candidates=[];
for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===1&&mem()[i+0x7935]===0&&mem()[i+0x7a58]===1&&mem()[i+0x7a66]===1&&mem()[i+0x7aec]===0xa9&&mem()[i+0x7aeb]===6)candidates.push(i);
const tick=mask=>{const out=rt.advance(mask);out.free();};
const key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
key(64);const live=candidates.filter(i=>mem()[i+0x7934]===6&&mem()[i+0x7935]===0);if(live.length!==1)throw Error('base '+live.length);
let base=live[0];const a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>8);mem()[a(o)]=v&255;};
key(64);putW(0x7a50,0);putW(0x7a58,80);putB(0x69d5,1);putB(0x69d6,1);putB(0x7a2e,0);putB(0x7a2c,0);
for(const mask of [64,2,2,2,16,2,16,16,64,2,2,2,16,16,16])key(mask);
if(w(0x7934)!==1||w(0x7a58)!==80||w(0x759e)!==6)throw Error('original map load failed');
const snapshot=rt.snapshotBytes(),configs=[
 ['flag200',0,0,3,0,200],['next-high',0,0,3,0,201],['next-high-taken',8,2,3,0,201],
 ['next-low',0,0,3,0,202],['progress-high',0,0,3,0,203],['progress-low',0,0,3,0,204],
 ['mode-byte',0,0,3,0,205],['padding-byte',0,0,3,0,206],['id255',0,0,3,0,255],
 ['id256',0,0,3,0,256],['id257',0,0,3,0,257],['last-padding',0,0,3,0,290],
 ['signed-minus-two-taken',8,1,3,0,65535]
];
let flagId=6;
const flagAddress=id=>0x742c+(((id-1)&65535)>=32768?((id-1)&65535)-65536:((id-1)&65535));
const resident=(o,n)=>Array.from({length:n},(_,i)=>b(o+i));
const state=()=>({mode:w(0x7934),event:w(0x7936),map:w(0x7a58),progress:w(0x7a66),direction:b(0x69d4),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),quantity:w(0x6852),flag:b(flagAddress(flagId))});
const rows=[];
for(const [name,quantity,flag,direction,slot,id] of configs){
 rt.restoreBytes(snapshot);rt.drainEventsBytes();
 const bases=[],m=mem();
 for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===1&&m[i+0x7933]===0&&m[i+0x7a58]===80&&m[i+0x7a57]===0&&m[i+0x759e]===6&&m[i+0x759d]===0&&m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6)bases.push(i);
 key(64);const current=bases.filter(i=>mem()[i+0x7934]===6&&mem()[i+0x7933]===0);
 if(current.length!==1)throw Error('restored live base '+current.length);base=current[0];key(64);
 putB(0x69d5,10);putB(0x69d6,3);putB(0x7a2e,17);putB(0x7a2c,17);putB(0x69d4,direction);
 flagId=id;
 for(let i=0;i<90;++i)putB(0x74f4+i,0);
 putW(0x759e,id);putW(0x6852,quantity);putB(flagAddress(id),flag);putW(0x7a50,0);
 for(let i=0;i<20;++i)putB(0x6ca2+i*14+11,0);
 for(let i=0;i<2;++i)putW(0x783a+i*14+6,0);
 if(slot===7){const original=resident(0x759e,8);for(let i=0;i<8;++i)putB(0x759e+7*8+i,original[i]);
  if(name==='zero-first-slot')putW(0x759e,0);
  if(name==='last-slot')for(let i=0;i<7;++i){const o=0x759e+i*8;putW(o,i+1);putB(o+6,0);putB(o+7,0);}}
 if(name==='opaque-word')putW(0x75a0,0x8123);
 const story=resident(0x74f4,90),itemAlias=b(0x742a);
 const before=state(),objects=resident(0x759e,64),flags=resident(0x742c,200),npc=resident(0x6ca2,280),monsters=resident(0x761a,56),chase=resident(0x783a,28),commits=[],phases=[];
 const phase=(kind)=>{const start=commits.length;
  for(let t=0;t<16;++t){if(kind!=='dismiss')putW(0x7a76,t===3?0x86:0);
   tick(kind==='dismiss'&&t===0?64:0);
   for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(pixels){
    fs.writeFileSync(`tmp/native-object-story-alias-${rows.length}-commit-${commits.length}.idx`,pixels);commits.push({kind,t,state:state()});}}}
  phases.push({kind,commits:commits.length-start,after:state()});};
 phase('pickup');
 fs.writeFileSync(`tmp/native-object-story-alias-${rows.length}-after-pickup.idx`,rt.framePixelIndices());
 phase('repeat');
 fs.writeFileSync(`tmp/native-object-story-alias-${rows.length}-after-repeat.idx`,rt.framePixelIndices());
 rows.push({name,id,story,itemAlias,afterStory:resident(0x74f4,90),before,after:state(),objects,flags,npc,monsters,chase,afterObjects:resident(0x759e,64),afterFlags:resident(0x742c,200),commits,phases});
 fs.writeFileSync('tmp/native-object-story-alias.json',JSON.stringify(rows,null,2)+'\n');console.log(name,JSON.stringify(state()),commits.length);
}
rt.free();
