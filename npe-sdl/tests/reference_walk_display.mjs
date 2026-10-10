import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const mem=()=>new Uint8Array(exp.memory.buffer);
const snapshot=fs.readFileSync('tmp/empty-title-first-map-snapshot.bin');
const cases=[
 ['up-empty',1,[]],['down-empty',2,[]],['left-empty',4,[]],['right-empty',8,[]],
 ['down-npc',2,[{x:43,y:3}]],['up-npc',1,[{x:43,y:1}]],
 ['right-npc',8,[{x:44,y:2}]],['left-npc',4,[{x:42,y:2}]],
 ['right-above-npc',8,[{x:44,y:1}]],
 ['right-below-npc',8,[{x:44,y:3}]],
 ['right-hidden-below',8,[{x:44,y:3,first:9,last:9}]],
];
const exitOnly=process.argv.includes('--exit-full');
const outputStem=exitOnly?'native-walk-exit':'native-walk';
const rows=[];
for(const [name,mask,npcs] of (exitOnly?cases.slice(0,1):cases)){
 rt.restoreBytes(snapshot);
 for(let t=0;t<15;++t){const out=rt.advance(0);out.free();}
 // A restore may relocate the bank backing; locate it anew for every case.
 const bases=[];
 for(let i=0;i+0x8000<mem().length;++i){const w=o=>mem()[i+o-1]*256+mem()[i+o];
  const lowerWord=o=>mem()[i+o-9]*256+mem()[i+o-8];
  if(w(0x7934)===1&&w(0x7a58)===1&&w(0x7a66)===1&&
     mem()[i+0x69d2-9]===1&&mem()[i+0x69d5-9]===7&&mem()[i+0x69d6-9]===1&&
     lowerWord(0x6ca2)===1&&lowerWord(0x6ca4)===7)bases.push(i);}
 if(bases.length!==1)throw Error('RAM discovery '+bases.length);
 const base=bases[0],a=o=>base+o-(o<0x7000?8:0);
 const b=o=>mem()[a(o)-1],w=o=>mem()[a(o)-1]*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;};
 const putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 const state=()=>({mode:w(0x7934),progress:w(0x7a66),map:w(0x7a58),
  direction:b(0x69d4),screenX:b(0x69d5),screenY:b(0x69d6),
  viewX:b(0x7a2e),viewY:b(0x7a2c),moveKind:w(0x7a86),counter:b(0x7a32),
  npc:Array.from({length:20},(_,j)=>{const o=0x6ca2+j*14;
   return{id:w(o),graphic:w(o+2),first:w(o+4),last:w(o+6),direction:b(o+8),x:b(o+9),y:b(o+10),moving:b(o+11)};})});
 putW(0x7a50,0);putW(0x7a5c,0);
 for(let j=0;j<20;++j){const o=0x6ca2+j*14,n=npcs[j];
  putW(o,n?1:0);putB(o+11,0);
  if(n){putW(o+2,14);putW(o+4,n.first??0);putW(o+6,n.last??0);
   putB(o+8,0);putB(o+9,n.x);putB(o+10,n.y);}}
 const index=rows.length,prefix=`tmp/${outputStem}-${index}`,before=state(),commits=[];
 fs.writeFileSync(`${prefix}-before.idx`,rt.framePixelIndices());
 for(let t=0;t<(exitOnly?512:32);++t){const out=rt.advance(t===0?mask:0);out.free();
  for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
   fs.writeFileSync(`${prefix}-commit-${commits.length}.idx`,pixels);
   commits.push({tick:t,stepOffset:String(rt.displayCommitStepOffset(j)),state:state()});}
  if(exitOnly && t>0 && state().mode===1 && state().map!==before.map){
   fs.writeFileSync(`${prefix}-after-snapshot.bin`,rt.snapshotBytes());break;
  }
 }
 const after=state();fs.writeFileSync(`${prefix}-after.idx`,rt.framePixelIndices());
 rows.push({name,mask,before,after,commits});
 console.log(name,JSON.stringify({before:[before.screenX,before.screenY,before.viewX,before.viewY],
  after:[after.screenX,after.screenY,after.viewX,after.viewY],mode:after.mode,direction:after.direction,
  kind:after.moveKind,commits:commits.length}));
}
fs.writeFileSync(`tmp/${outputStem}.json`,JSON.stringify({rows},null,2)+'\n');rt.free();
