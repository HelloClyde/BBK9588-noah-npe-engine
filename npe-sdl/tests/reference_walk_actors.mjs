import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/empty-title-first-map-snapshot.bin');
const npc=(x,y,extra={})=>({id:1,graphic:14,first:0,last:0,direction:0,x,y,moving:0,...extra});
const monster=(x,y,extra={})=>({enemy:1,npc:1,graphic:14,first:0,last:0,x,y,left:0,right:0,...extra});
const cases=[
 ['right-monster-above',8,[],[monster(44,1)]],
 ['right-monster-below',8,[],[monster(44,3)]],
 ['right-monsters-above-first',8,[],[monster(44,1),monster(44,3,{graphic:7})]],
 ['right-monsters-below-first',8,[],[monster(44,3,{graphic:7}),monster(44,1)]],
 ['right-npc-above-monster-below',8,[npc(44,1)],[monster(44,3,{graphic:7})]],
 ['right-npc-below-monster-above',8,[npc(44,3)],[monster(44,1,{graphic:7})]],
 ['right-hidden-npc-first',8,[npc(44,1,{first:9,last:9}),npc(44,3)],[monster(44,3,{graphic:7})]],
 ['right-npcs-above-first',8,[npc(44,1),npc(44,3,{graphic:7})],[]],
 ['right-npcs-below-first',8,[npc(44,3,{graphic:7}),npc(44,1)],[]],
 ['right-overlap',8,[npc(46,3),npc(46,4,{graphic:7})],[monster(46,3,{graphic:7})]],
 ['right-overlap-reversed',8,[npc(46,4,{graphic:7}),npc(46,3)],[monster(46,4,{graphic:7})]],
 ['down-block-monster-above',2,[],[monster(43,1)]],
 ['down-block-monster-below',2,[],[monster(43,3)]],
 ['down-block-npc-first',2,[npc(43,1)],[monster(43,3,{graphic:7})]],
 ['right-monster-collision',8,[],[monster(44,2)]],
 ['right-monster-hidden-collision',8,[],[monster(44,2,{first:9,last:9})]],
 ['right-monster-zero-enemy',8,[],[monster(44,2,{enemy:0})]],
 ['right-monster-zero-graphic',8,[],[monster(44,2,{graphic:0})]],
 ['right-monster-zero-npc',8,[],[monster(44,2,{npc:0})]],
 ['right-monster-zero-enemy-below',8,[],[monster(44,3,{enemy:0})]],
 ['right-monster-top-row',8,[],[monster(46,1)]],
 ['right-monster-bottom-row',8,[],[monster(46,8)]],
 ['right-monster-last-visible-row',8,[],[monster(46,7)]],
 ['down-block-hidden-monster-below',2,[],[monster(43,3,{first:9,last:9})]],
];
const rows=[],mem=()=>new Uint8Array(exp.memory.buffer);
for(const [name,mask,npcs,monsters] of cases){
 rt.restoreBytes(snapshot);for(let t=0;t<15;++t){const out=rt.advance(0);out.free();}
 const bankBytes=Buffer.from(exp.memory.buffer),sig=Buffer.from([0,1,0,7]),bases=[];
 for(let p=bankBytes.indexOf(sig);p>=0;p=bankBytes.indexOf(sig,p+1)){
  const i=p-0x6ca2+9;if(i<0||i+0x8000>=mem().length)continue;
  const w=o=>mem()[i+o-1]*256+mem()[i+o];
  if(w(0x7934)===1&&w(0x7a58)===1&&w(0x7a66)===1&&
     mem()[i+0x69d2-9]===1&&mem()[i+0x69d5-9]===7&&mem()[i+0x69d6-9]===1&&
     mem()[i+0x7a2e-1]===36&&mem()[i+0x7a2c-1]===1&&
     mem()[i+0x6cab-9]===32&&mem()[i+0x6cac-9]===13)bases.push(i);
 }
 if(bases.length!==1)throw Error('RAM discovery '+bases.length);
 const base=bases[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 const state=()=>({mode:w(0x7934),progress:w(0x7a66),map:w(0x7a58),direction:b(0x69d4),
  screenX:b(0x69d5),screenY:b(0x69d6),viewX:b(0x7a2e),viewY:b(0x7a2c),moveKind:w(0x7a86),
  npc:Array.from({length:20},(_,j)=>{const o=0x6ca2+j*14;return{id:w(o),graphic:w(o+2),first:w(o+4),last:w(o+6),direction:b(o+8),x:b(o+9),y:b(o+10),moving:b(o+11)};}),
  monsters:Array.from({length:4},(_,j)=>{const o=0x761a+j*14;return{graphic:w(o),first:w(o+2),last:w(o+4),npc:w(o+6),x:b(o+9),y:b(o+10),enemy:b(o+11),left:b(o+12),right:b(o+13)};})});
 putW(0x7a50,0);putW(0x7a5c,0);
 for(let j=0;j<20;++j){const o=0x6ca2+j*14,n=npcs[j];putW(o,n?.id??0);putB(o+11,0);
  if(n){putW(o+2,n.graphic);putW(o+4,n.first);putW(o+6,n.last);putB(o+8,n.direction);putB(o+9,n.x);putB(o+10,n.y);}}
 for(let j=0;j<4;++j){const o=0x761a+j*14,n=monsters[j]??monster(0,0,{enemy:0,npc:0,graphic:0});
  putW(o,n.graphic);putW(o+2,n.first);putW(o+4,n.last);putW(o+6,n.npc);putB(o+8,0);
  putB(o+9,n.x);putB(o+10,n.y);putB(o+11,n.enemy);putB(o+12,n.left);putB(o+13,n.right);}
 const before=state(),index=rows.length,prefix=`tmp/native-walk-actors-${index}`,commits=[];
 fs.writeFileSync(`${prefix}-before.idx`,rt.framePixelIndices());
 for(let t=0;t<32;++t){const out=rt.advance(t===0?mask:0);out.free();
  for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
   fs.writeFileSync(`${prefix}-commit-${commits.length}.idx`,pixels);commits.push({tick:t});}}
 const after=state();fs.writeFileSync(`${prefix}-after.idx`,rt.framePixelIndices());
 if(after.mode!==1||after.map!==1)throw Error('unexpected mode/map '+name);
 rows.push({name,mask,before,after,commits});fs.writeFileSync('tmp/native-walk-actors.json',JSON.stringify({rows},null,2)+'\n');
 console.log(name,JSON.stringify({after:[after.viewX,after.viewY,after.screenX,after.screenY],commits:commits.length}));
}
rt.free();
