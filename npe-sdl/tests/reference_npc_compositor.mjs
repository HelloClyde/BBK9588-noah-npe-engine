import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
rt.restoreBytes(fs.readFileSync('tmp/empty-title-first-map-snapshot.bin'));
const mem=()=>new Uint8Array(exp.memory.buffer),bases=[];
for(let i=0;i+0x8000<mem().length;++i){const w=o=>mem()[i+o-1]*256+mem()[i+o];
 if(w(0x7934)===1&&w(0x7a58)===1&&w(0x7a66)===1&&mem()[i+0x69d2-9]===1&&mem()[i+0x69d5-9]===7&&mem()[i+0x69d6-9]===1)bases.push(i);}
if(bases.length!==1)throw Error('RAM discovery '+bases.length);
const base=bases[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>mem()[a(o)-1]*256+mem()[a(o)];
const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
const state=()=>({counter:b(0x7a32),mode:w(0x7934),progress:w(0x7a66),heroY:b(0x69d6)+b(0x7a2c),
 npc:Array.from({length:20},(_,j)=>{const o=0x6ca2+j*14;return{id:w(o),graphic:w(o+2),first:w(o+4),last:w(o+6),direction:b(o+8),x:b(o+9),y:b(o+10),moving:b(o+11)};})});
const period=()=>{const counter=b(0x7a32);for(let ticks=0;ticks<300;++ticks){const out=rt.advance(0);out.free();if(b(0x7a32)!==counter)return ticks;}throw Error('period timeout');};
const original=state().npc,rows=[];
const actor=(x,y,extra={})=>({id:1,graphic:7,first:0,last:0,direction:0,x,y,moving:0,...extra});
const cases=[
 ['above', [actor(43,1)]], ['below',[actor(43,3)]],
 ['above-first',[actor(43,1),actor(43,3,{graphic:14})]],
 ['below-first',[actor(43,3,{graphic:14}),actor(43,1)]],
 ['hidden-above-first',[actor(43,1,{first:9,last:9}),actor(43,3,{graphic:14})]],
 ['same-cell',[actor(43,2),actor(43,2,{graphic:14})]],
 ['overlap',[actor(45,3),actor(45,4,{graphic:14})]],
 ['overlap-reversed',[actor(45,4,{graphic:14}),actor(45,3)]],
 ['left-edge',[actor(36,2)]], ['right-edge',[actor(50,2)]],
 ['top-clipped',[actor(44,0)]], ['bottom-clipped',[actor(44,8)]],
 ['moving-above',[actor(43,1,{moving:1})]],
 ['moving-below',[actor(43,3,{moving:1})]],
 ['moving-overlap',[actor(45,3,{moving:1}),actor(45,4,{graphic:14,moving:1})]],
 ['moving-left',[actor(36,2,{moving:1})]],
 ['moving-right',[actor(50,2,{moving:1})]],
 ['moving-top-row',[actor(44,1,{moving:1})]],
 ['moving-bottom-row',[actor(44,7,{moving:1})]],
 ['center-above-first',[actor(43,4),actor(43,6,{graphic:14})],5],
 ['center-below-first',[actor(43,6,{graphic:14}),actor(43,4)],5],
 ['center-hidden-above',[actor(43,4,{first:9,last:9}),actor(43,6,{graphic:14})],5],
 ['center-moving-above',[actor(43,4,{moving:1}),actor(43,6,{graphic:14})],5],
 ['center-moving-below',[actor(43,6,{graphic:14,moving:1}),actor(43,4)],5],
];
for(const [name,actors,heroY=2] of cases){
 putB(0x69d6,heroY-b(0x7a2c));
 // Keep the live LCD: field injection does not redraw it. Capture that exact
 // input surface, then wait for the real firmware periodic erase/draw event.
 for(let i=0;i<20;++i){const n=actors[i]??{...original[i],id:0,moving:0},o=0x6ca2+i*14;
  putW(o,n.id);putW(o+2,n.graphic);putW(o+4,n.first);putW(o+6,n.last);
  putB(o+8,n.direction);putB(o+9,n.x);putB(o+10,n.y);putB(o+11,n.moving);}
 const before=state(),index=rows.length;
 fs.writeFileSync(`tmp/native-npc-compositor-${index}-before.idx`,rt.framePixelIndices());
 const ticks=period(),after=state();
 if(after.mode!==1)throw Error('unexpected mode '+after.mode);
 fs.writeFileSync(`tmp/native-npc-compositor-${index}-after.idx`,rt.framePixelIndices());
 rows.push({name,before,after,ticks});console.log(name,JSON.stringify(after.npc.slice(0,actors.length)));
}
fs.writeFileSync('tmp/native-npc-compositor.json',JSON.stringify({rows},null,2)+'\n');rt.free();
