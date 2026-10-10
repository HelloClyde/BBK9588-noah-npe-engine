import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';

// Isolated original RAM arguments, unchanged PRG/NPE. No route replay.
const exp=await initWasm({module_or_path:fs.readFileSync(new URL('../../tmp/opennoah_wasm_bg.wasm',import.meta.url))});
const rt=new GameRuntime(fs.readFileSync(new URL('../../tmp/tianjiang-online.opennoah',import.meta.url)),'certified_original_v1');
const configs=[
 {name:'opening',group:1,x:0,y:0,draw:0,erase:0,frames:62},
 ...[0,1,2].flatMap(draw=>[0,1].map(erase=>({name:`local-${draw}-${erase}`,group:2,x:60,y:0,draw,erase,frames:12}))),
 {name:'ending',group:3,x:0,y:0,draw:0,erase:0,frames:37},
];
const rows=[];
for(const c of configs){
 rt.restoreBytes(fs.readFileSync(new URL('../../tmp/battle-fresh-start.bin',import.meta.url)));
 const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
 for(let i=0;i+0x8000<mem().length;++i)
  if(mem()[i+0x7934]===3&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7a58]===9&&mem()[i+0x7a66]===6)found.push(i);
 if(found.length!==1)throw Error('RAM '+found.length);
 const base=found[0],a=o=>base+o-(o<0x7000?8:0),w=o=>mem()[a(o)-1]*256+mem()[a(o)];
 const putW=(o,v)=>{mem()[a(o)-1]=v>>>8;mem()[a(o)]=v&255;},putB=(o,v)=>{mem()[a(o)-1]=v;};
 const tick=mask=>{const out=rt.advance(mask);out.free();};
 // Leave battle through the normal map dispatcher, retaining its rendered LCD.
 putW(0x7934,1);putW(0x7ab4,1);for(let i=0;i<35;++i)tick(0);
 fs.writeFileSync(new URL(`../../tmp/native-scene-${c.name}-before.idx`,import.meta.url),rt.framePixelIndices());
 // Mode15 takes these runtime argument fields loaded from the NPE record.
 putW(0x754e,0);putB(0x7550,c.group);putB(0x7551,0);
 putB(0x7552,c.draw);putB(0x7553,c.erase);putB(0x7554,7);
 putW(0x7556,c.x);putW(0x7558,c.y);putW(0x755a,0);putW(0x755c,0);
 putW(0x7934,15);putW(0x7936,1);
 const commits=[];
 for(let i=0;i<1800&&w(0x7934)!==7;++i){
  tick(0);
  for(let j=0;j<rt.displayCommitCount();++j){
   const pix=rt.displayCommitPixelIndices(j);if(!pix)continue;
   const index=commits.length;
   fs.writeFileSync(new URL(`../../tmp/native-scene-${c.name}-commit-${index}.idx`,import.meta.url),pix);
   commits.push({index,tick:Number(rt.logicalTick()),stepOffset:String(rt.displayCommitStepOffset(j)),stepSpan:String(rt.displayCommitStepSpan()),stepsPerSecond:rt.displayCommitStepsPerSecond()});
  }
 }
 if(w(0x7934)!==7||commits.length<c.frames)throw Error('scene did not finish '+JSON.stringify({c,mode:w(0x7934),event:w(0x7936),commits:commits.length,ram:Buffer.from(mem().slice(a(0x754e)-1,a(0x755e))).toString('hex')}));
 rows.push({...c,commits});console.log(c.name,commits.length);
 putW(0x7934,0);
}
fs.writeFileSync(new URL('../../tmp/native-scene-animation.json',import.meta.url),JSON.stringify(rows,null,2)+'\n');
rt.free();
