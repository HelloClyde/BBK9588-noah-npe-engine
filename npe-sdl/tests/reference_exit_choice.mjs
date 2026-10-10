import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/empty-title-first-map-snapshot.bin');
const closing=process.argv.includes('--shop-close');
const dialogue=closing||process.argv.includes('--dialogue');
const output=closing?'native-shop-close':dialogue?'native-exit-dialogue':'native-exit-choice';
const configs=closing?[
 {name:'shop-close-space',keys:[128,16],talk:true},
 {name:'shop-close-enter',keys:[128,16],talk:true,shortcut:true},
]:dialogue?[
 {name:'space-over-prompt',keys:[16],talk:true},
 {name:'space-after-map-load',keys:[128,16],talk:true},
 {name:'shop-enter-shortcut',keys:[128,16],talk:true,shortcut:true},
]:[
 {name:'choose-first',keys:[128]},
 {name:'choose-second',keys:[2,128]},
 {name:'up-down-up',keys:[1,2,1,128]},
 {name:'repeat-directions',keys:[2,2,1,1,128]},
 {name:'ignored-horizontal-back',keys:[4,8,32,16,128]},
 {name:'wait-modal',keys:[],wait:300},
 {name:'second-range-denied',keys:[],secondFirst:2},
 {name:'primary-range-denied',keys:[],first:2},
 {name:'post-confirm-directions',keys:[128,2,1,4]},
 {name:'menu-roundtrip',keys:[64,64,2,128]},
];
const rows=[];
for(const config of configs){
 rt.restoreBytes(snapshot);for(let t=0;t<15;++t){const out=rt.advance(0);out.free();}
 const mem=()=>new Uint8Array(exp.memory.buffer),buf=Buffer.from(exp.memory.buffer),bases=[];
 for(let p=buf.indexOf(Buffer.from([0,1,0,7]));p>=0;p=buf.indexOf(Buffer.from([0,1,0,7]),p+1)){
  const i=p-0x6ca2+9;if(i<0||i+0x8000>=mem().length)continue;
  const w=o=>mem()[i+o-1]*256+mem()[i+o];
  if(w(0x7934)===1&&w(0x7a58)===1&&w(0x7a66)===1&&mem()[i+0x69d5-9]===7&&
     mem()[i+0x69d6-9]===1&&mem()[i+0x6cab-9]===32&&mem()[i+0x6cac-9]===13)bases.push(i);
 }
 if(bases.length!==1)throw Error('RAM discovery '+bases.length);
 const base=bases[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 const u32=(m,j)=>((m[j]*16777216+m[j+1]*65536+m[j+2]*256+m[j+3])>>>0);
 const oldMemory=mem().slice(),oldCounter=b(0x7a32);let changed=false;
 for(let t=0;t<300;++t){const out=rt.advance(0);out.free();if(b(0x7a32)!==oldCounter){changed=true;break;}}
 if(!changed)throw Error('period timeout');
 const nextMemory=mem(),candidates=new Set(),rng=[];
 for(let j=0;j<Math.min(oldMemory.length,nextMemory.length);++j)if(oldMemory[j]!==nextMemory[j])
  for(let k=Math.max(0,j-3);k<=j&&k+4<oldMemory.length;++k)candidates.add(k);
 for(const j of candidates){const old=u32(oldMemory,j),next=u32(nextMemory,j);if(old<=1||next<=1||old===next)continue;
  let v=old;for(let calls=1;calls<=128;++calls){v=(Math.imul(v,0x514f111f)+1)>>>0;if(v===next){rng.push({offset:j,calls});break;}}}
 if(rng.length!==1)throw Error('RNG discovery '+JSON.stringify(rng));const rngAddress=rng[0].offset;
 putW(0x7a50,0);putW(0x7a5c,0);putB(0x7ac4,0);
 for(let i=0;i<20;++i){putW(0x6ca2+i*14,0);putB(0x6ca2+i*14+11,0);}
 for(let i=0;i<2;++i)putW(0x783a+i*14+6,0);
 for(let i=0;i<4;++i){putW(0x761a+i*14+6,0);putB(0x761a+i*14+11,0);}
 putB(0x69d5,6);putB(0x69d6,1);putB(0x7a2e,36);putB(0x7a2c,1);
 for(let t=0;t<32;++t){const out=rt.advance(t===0?8:0);out.free();}
 const e=0x6b82+10*24;
 putB(e+2,1);putW(e+4,7);putW(e+6,config.first??1);putB(e+8,7);putB(e+9,13);
 putB(e+11,1);putW(e+12,1);putW(e+14,2);putW(e+16,config.secondFirst??1);
 putB(e+18,4);putB(e+19,13);putB(e+21,1);putW(e+22,1);
 putW(0x7a02,123);mem().set([0,0,0,11],rngAddress);
 const state=()=>({mode:w(0x7934),event:w(0x7936),context:w(0x7938),map:w(0x7a58),progress:w(0x7a66),
  direction:b(0x69d4),screenX:b(0x69d5),screenY:b(0x69d6),viewX:b(0x7a2e),viewY:b(0x7a2c),keycode:w(0x7a76),
  exitChoice:b(0x7ac4),exitSlot:w(0x7a84),moveKind:w(0x7a86),encounterSteps:w(0x7a02),
  redraw:w(0x7ab4),periodCounter:b(0x7a32),rng:u32(mem(),rngAddress),
  talkContext:b(0x7a68),mode2Flag:w(0x7a48),mode2Next:w(0x7a0a),mode2Event:w(0x7a82),mode2Word:w(0x7a0c),
  dialoguePointer:[w(0x797a),w(0x797c)],
  cache:Array.from({length:105},(_,i)=>w(0x70b4+i*2)),
  npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return {id:w(o),graphic:w(o+2),first:w(o+4),last:w(o+6),
   direction:b(o+8),x:b(o+9),y:b(o+10),moving:b(o+11)};}),
  exit:Array.from({length:24},(_,i)=>b(e+i))});
 const index=rows.length,prefix=`tmp/${output}-${index}`,before=state(),commits=[],phases=[];
 fs.writeFileSync(`${prefix}-before.idx`,rt.framePixelIndices());
 const phase=(key,ticks,label)=>{const start=commits.length,phaseBefore=state();
  for(let t=0;t<ticks;++t){
   /* The public web mask emits space for Confirm. Deliver the original
      resident Enter key once at tick3; subsequent ticks release it. */
   if((key===128||key===256)&&t>0)putW(0x7a76,t===3?0x86:0);
   const out=rt.advance(t===0?(key===128?16:key===256?0:key):0);out.free();
   for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
    fs.writeFileSync(`${prefix}-commit-${commits.length}.idx`,pixels);commits.push({label,tick:t,state:state()});}}
  phases.push({label,key,before:phaseBefore,commits:commits.length-start,after:state()});};
 phase(1,config.first===2||config.secondFirst===2?512:32,'enter');
 if(config.wait)phase(0,config.wait,'wait');
 for(const key of config.keys){
  if(config.talk&&key===16){
   const x=b(0x7a2e)+b(0x69d5),y=b(0x7a2c)+b(0x69d6)-1;
   putW(0x6ca2,1);putW(0x6ca2+4,0);putW(0x6ca2+6,0);
   putB(0x6ca2+9,x);putB(0x6ca2+10,y);putB(0x6ca2+11,0);
  }
  phase(key,key===128?512:32,'key-'+key);
 }
 if(config.talk){
  if(config.shortcut&&w(0x7934)===2)phase(256,32,'shop-enter');
  if(config.name==='space-over-prompt'&&w(0x7934)===2)phase(256,32,'ignored-enter');
  for(let page=0;page<24&&w(0x7934)===2;++page)phase(16,32,'page-'+page);
  if(w(0x7934)===2)throw Error('dialogue page limit');
  phase(2,32,'after-talk-down');
  if(closing)phase(64,32,'shop-close');
 }
 const after=state();fs.writeFileSync(`${prefix}-after.idx`,rt.framePixelIndices());
 if(dialogue)fs.writeFileSync(`${prefix}-after.snapshot`,rt.snapshotBytes());
 rows.push({name:config.name,config,before,after,commits,phases});
 fs.writeFileSync(`tmp/${output}.json`,JSON.stringify({rows},null,2)+'\n');
 console.log(config.name,JSON.stringify({commits:commits.length,phases:phases.map(p=>({key:p.key,commits:p.commits,
  code:p.after.keycode,choice:p.after.exitChoice,map:p.after.map,mode:p.after.mode,steps:p.after.encounterSteps,rng:p.after.rng}))}));
}
rt.free();
