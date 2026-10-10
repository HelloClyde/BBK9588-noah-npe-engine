import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const cases=[];
for(const key of [0x71,0x51])for(const more of [0,1])for(const context of [9,10])cases.push({name:`q-${key}-more${more}-context${context}`,npc:1,progress:3,patch:{more,context},key,ignored:true});
const bytes=fs.readFileSync('tmp/tjfy-world-probe.npe'),section=bytes.readUInt32BE(0x3a+10*8);
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>rt.advance(mask).free();
const selected=process.argv[2],rows=selected?JSON.parse(fs.readFileSync('tmp/native-npc-ignored-service.json')).filter(r=>r.name!==selected):[];
try{for(const cfg of cases){
 if(selected&&cfg.name!==selected)continue;
 rt.restoreBytes(fs.readFileSync('tmp/native-title-load-saved.snapshot'));rt.drainEventsBytes();
 const key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
 const candidates=[];for(let i=0;i+0x8000<mem().length;++i)
  if(mem()[i+0x7934]===1&&mem()[i+0x7a58]===1&&mem()[i+0x7a66]===1&&mem()[i+0x7aec]===0xa9&&mem()[i+0x7aeb]===6)candidates.push(i);
 key(64);const found=candidates.filter(i=>mem()[i+0x7934]===6);
 if(found.length!==1)throw Error('live RAM '+found.length);const base=found[0];key(64);
 const a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v&255;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 const state=()=>({mode:w(0x7934),event:w(0x7936),progress:w(0x7a66),context:w(0x7938),npcContext:b(0x7a68),
  npcState:b(0x6fba+cfg.npc),followup:b(0x7a0a),next:w(0x7a82),mapInit:w(0x7ab4),
  pointer:w(0x797a)*65536+w(0x797c),offset:w(0x7a1a),more:b(0x7a1c),style:b(0x7ace),
  rect:[0x791e,0x7920,0x7922,0x7924].map(w)});
 if(cfg.rect)for(let i=0;i<4;++i)putW(0x791e+2*i,cfg.rect[i]);
 if(cfg.style!==undefined)putB(0x7ace,cfg.style);
 putW(0x7a50,0);putW(0x7a5c,0);putW(0x7a66,cfg.progress);putB(0x6fba+cfg.npc,cfg.npcState??0);
 putW(0x7934,2);putW(0x7936,cfg.npc);putW(0x7ab4,1);
 const before=state();fs.writeFileSync(`tmp/native-npc-ignored-${cfg.name}-before.idx`,rt.framePixelIndices());
 for(let t=0;t<35;++t)tick(0);
 if(w(0x7934)!==2)throw Error('NPC entry '+cfg.name);
 const pages=[];
 const capture=()=>{const n=pages.length;fs.writeFileSync(`tmp/native-npc-ignored-${cfg.name}-page-${n}.idx`,rt.framePixelIndices());pages.push(state());};
 capture();const patched=[];
 if(cfg.patch){const p=cfg.patch;
  if(p.story||p.zero){let pointer=p.zero?bytes.indexOf(Buffer.alloc(64)):section+bytes.readUInt32BE(section+4*(p.story-1))+7;
   if(pointer<0)throw Error('no zero resource span');putW(0x797a,pointer>>>16);putW(0x797c,pointer&65535);putW(0x7a1a,p.offset??0);putB(0x7a1c,p.more??1);}
  if(p.more!==undefined)putB(0x7a1c,p.more);
  if(p.style!==undefined)putB(0x7ace,p.style);
  if(p.rect)for(let i=0;i<4;++i)putW(0x791e+2*i,p.rect[i]);
  if(p.context!==undefined)putB(0x7a68,p.context);
  const phaseBefore=state(),commits=[];
  for(let t=0;t<24;++t){putW(0x7a76,t===3?(cfg.key??0x20):0);tick(0);
   for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(pixels){
    fs.writeFileSync(`tmp/native-npc-ignored-${cfg.name}-continue-${commits.length}.idx`,pixels);commits.push(state());
   }}
  }
  if(!cfg.close&&!cfg.shortcut&&!cfg.ignored){if(commits.length!==1||w(0x7934)!==2)throw Error('continuation '+cfg.name);}
  if(cfg.ignored&&commits.length)throw Error('ignored input drew');
  patched.push({before:phaseBefore,commits,after:state()});
 }else{
  while(b(0x7a1c)===1&&pages.length<25){key(16);if(w(0x7934)!==2)throw Error('premature close');capture();}
  if(pages.length===25)throw Error('page limit');
 }
 rows.push({...cfg,before,pages,patched,after:state()});fs.writeFileSync('tmp/native-npc-ignored-service.json',JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name:cfg.name,before,pages:pages.length,first:pages[0],patched:patched.map(p=>({commits:p.commits.length,after:p.after}))}));
 putW(0x7934,0);
}}finally{rt.free();}
