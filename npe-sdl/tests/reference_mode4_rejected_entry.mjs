/* Original mode4 initialization for invalid shop contexts. The original NPE
   and engine are unchanged; only live input controls are diagnostic seeds. */
import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot');
const out='tmp/native-mode4-rejected-entry';fs.mkdirSync(out,{recursive:true});
const configs=[
 {name:'cold-zero',context:0,ui:0,kind:0},
 {name:'cold-nine',context:9,ui:0,kind:0},
 {name:'signed-negative',context:65535,ui:0,kind:2},
 {name:'retained-list',context:0,ui:1,kind:2},
 {name:'retained-quantity',context:0,ui:2,kind:1},
];
const rows=[];
try{for(const cfg of configs){
 rt.restoreBytes(snapshot);rt.drainEventsBytes();
 const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>rt.advance(mask).free(),candidates=[];
 for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===1&&mem()[i+0x7935]===0&&mem()[i+0x7a58]===1&&mem()[i+0x7a66]===1&&mem()[i+0x7aec]===0xa9&&mem()[i+0x7aeb]===6)candidates.push(i);
 for(let t=0;t<16;++t)tick(t===0?64:0);
 const live=candidates.filter(i=>mem()[i+0x7934]===6&&mem()[i+0x7935]===0);
 if(live.length!==1)throw Error('mode4 RAM '+live.length);
 const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 for(let t=0;t<16;++t)tick(t===0?64:0);
 const state=()=>({mode:w(0x7934),event:w(0x7936),context:w(0x7938),init:w(0x7ab4),kind:b(0x7a0e),ui:b(0x7a4a),
 selected:w(0x7a4c),first:w(0x7ac6),last:w(0x7ab6),row:b(0x7a46),quantity:b(0x7a48),choice:b(0x7a0c),
 scanFirst:b(0x7a10),scanLast:b(0x7a12),inventoryOnly:b(0x7a14),sell:b(0x7ac2),
 descStart:w(0x7abe),descEnd:w(0x7abc),map:w(0x7a58),progress:w(0x7a66),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),
 npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};})});
 putW(0x7934,4);putW(0x7936,1796);putW(0x7938,cfg.context);putW(0x7ab4,1);putW(0x7a76,0);
 putB(0x7a0e,cfg.kind);putB(0x7a4a,cfg.ui);putW(0x7a4c,65535);putW(0x7ac6,9);putW(0x7ab6,37);
 putB(0x7a46,6);putB(0x7a48,99);putB(0x7a0c,0);putB(0x7ac2,1);
 putB(0x7a10,10);putB(0x7a12,20);putB(0x7a14,255);putW(0x7abe,82);putW(0x7abc,97);putW(0x7a50,0);
 const before=state(),beforePixels=rt.framePixelIndices();fs.writeFileSync(`${out}/${cfg.name}-before.idx`,beforePixels);
 const commits=[],collect=t=>{const captured=[];for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(pixels){const index=commits.length;fs.writeFileSync(`${out}/${cfg.name}-${index}.idx`,pixels);const c={index,tick:t,state:state()};commits.push(c);captured.push(c);}}return captured;};
 const init=[];for(let t=0;t<32;++t){tick(0);init.push(...collect(t));}
 const afterInit=state(),phases=[];
 for(let press=0;press<(cfg.ui===1?1:2);++press){let delivered=false;const p=[],phaseBefore=state();
  for(let t=0;t<32;++t){putW(0x7a76,t>=3&&!delivered?0x8e:0);tick(0);const c=collect(t);p.push(...c);if(c.length||state().ui!==phaseBefore.ui||state().mode===1)delivered=true;}
  phases.push({key:0x8e,commits:p,after:state()});
 }
 if(init.length||afterInit.init!==0||phases.at(-1).after.mode!==1)throw Error('incomplete mode4 entry '+cfg.name);
 const row={...cfg,before,afterInit,init,phases,commits};rows.push(row);fs.writeFileSync(`${out}/report.json`,JSON.stringify({diagnostic_controls:true,original:true,rows},null,2)+'\n');
 console.log(JSON.stringify({name:cfg.name,init:init.length,afterInit,phases:phases.map(p=>({frames:p.commits.length,mode:p.after.mode,ui:p.after.ui}))}));
}}
finally{rt.free();}
