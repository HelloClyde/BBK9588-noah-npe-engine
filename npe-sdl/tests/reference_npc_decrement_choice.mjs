// Original mode22 service: exact UI and signed-fee paths. Diagnostic inputs
// change service entry, amounts and role stats only; original NPE/program stay.
import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot'),mem=()=>new Uint8Array(exp.memory.buffer);
const tick=mask=>{const out=rt.advance(mask);out.free();};
const key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
const tests=[0,1,2].map(value=>({name:'npc-choice60-'+value,mode:24,event:60,value}));
const rows=[];
try{
 for(const cfg of tests){
  const {name,mode,event}=cfg;
  rt.restoreBytes(snapshot);rt.drainEventsBytes();const m=mem(),candidates=[];
  for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===1&&m[i+0x7935]===0&&m[i+0x7a58]===1&&m[i+0x7a66]===1&&m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6)candidates.push(i);
  key(64);const live=candidates.filter(i=>mem()[i+0x7934]===6);if(live.length!==1)throw Error('healing base '+live.length);
  const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)],putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>8);mem()[a(o)]=v&255;};
  const state=()=>({mode:w(0x7934),event:w(0x7936),context:w(0x7938),map:w(0x7a58),progress:w(0x7a66),money:(w(0x799e)*65536+w(0x79a0))>>>0,init:w(0x7ab4),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),enabled:w(0x7a50),slot:b(0x7a04),level:b(0x69de),experience:w(0x6a12),threshold:w(0x6a10),attack:w(0x6a22),defense:w(0x6a24),other:[w(0x6a1c),w(0x6a1e),w(0x6a20)],npcState:Array.from({length:250},(_,i)=>b(0x6fba+i)),maxHp:Array.from({length:3},(_,i)=>w(0x6a14+i*148)),hp:Array.from({length:3},(_,i)=>w(0x6a16+i*148)),maxQi:Array.from({length:3},(_,i)=>w(0x6a18+i*148)),qi:Array.from({length:3},(_,i)=>w(0x6a1a+i*148)),npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};})});
  key(64);const pixels=rt.framePixelIndices();
  if(mode===23){
   putB(0x69de,cfg.level??1);putW(0x6a10,cfg.threshold??10);putW(0x6a12,cfg.experience??0);
   if(cfg.stats!==undefined)for(const o of [0x6a14,0x6a18,0x6a22,0x6a24,0x6a1c,0x6a1e,0x6a20])putW(o,cfg.stats);
   putW(0x6a16,1);putW(0x6a1a,1);
  }else putB(0x6fba+(event>=1&&event<250?event:60),cfg.value);
  const choiceBytes=Array.from({length:6},(_,i)=>b(0x7822+i));
  if(choiceBytes[1]!==24 || w(0x7826)!==60)throw Error('actual NPE cancel choice missing '+choiceBytes);
  putW(0x7934,17);putW(0x7936,2);putW(0x7938,0);putW(0x7ab4,1);putW(0x7a50,0);putB(0x7a32,0);
  fs.writeFileSync(`tmp/native-simple-events-${name}-before.records`,Uint8Array.from({length:444},(_,i)=>b(0x69c6+i)));
  const before=state(),commits=[];fs.writeFileSync(`tmp/native-simple-events-${name}-before.idx`,pixels);
  let finish=-1;
  for(const mask of [0,16]){
   for(let t=0;t<(mask===0?64:16);++t){tick(t===0?mask:0);
    for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){const index=commits.length;fs.writeFileSync(`tmp/native-simple-events-${name}-${index}.idx`,p);commits.push({tick:t,state:state()});}}
    if(state().mode===1&&state().init===0){finish=t;break;}
   }
   if(finish>=0)break;
  }
  if(finish<0)throw Error('NPC choice no finish '+name+' '+JSON.stringify({mode:state().mode,event:state().event,init:state().init,context:state().context,commits:commits.length,npc60:state().npcState[60]}));
  fs.writeFileSync(`tmp/native-simple-events-${name}-after.records`,Uint8Array.from({length:444},(_,i)=>b(0x69c6+i)));
  rows.push({name,mode,event,cfg,choiceBytes,before,commits,after:state(),finish});
  fs.writeFileSync('tmp/native-simple-events-choice.json',JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({name,mode,event,commits:commits.length,level:state().level,experience:state().experience,hp:state().hp,attack:state().attack,npcState:state().npcState[event>=1&&event<250?event:60]}));
 }
}finally{rt.free();}
