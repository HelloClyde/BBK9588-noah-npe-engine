import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
// Seed only resident text controls; read original warm constructors and normal-title loads.
{
// Original runtime only. No C output participates in these references.
const dir='tmp/story-text-restart-native';
fs.mkdirSync(dir,{recursive:true});
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const mem=()=>new Uint8Array(exp.memory.buffer);
const rows=[];
let base,lowerBase;
const a=o=>(o<0x7000?lowerBase:base)+o,b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
const putB=(o,v)=>{mem()[a(o)-1]=v&255;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
const tick=mask=>{const out=rt.advance(mask);out.free();};
const idle=n=>{for(let i=0;i<n;++i)tick(0);};
const key=mask=>{tick(mask);idle(15);};
const banks=()=>({pointer:w(0x797a)*65536+w(0x797c),rect:[0x791e,0x7920,0x7922,0x7924].map(w),offset:w(0x7a1a),more:b(0x7a1c),style:b(0x7ace)});
const state=()=>({mode:w(0x7934),event:w(0x7936),map:w(0x7a58),progress:w(0x7a66),partyCount:b(0x7afa)});
const seed=salt=>{for(let i=0;i<4;++i){putB(0x797a+i,(i*37+salt)&255);putW(0x791e+2*i,(0x8123+i*97+salt)&65535);}putW(0x7a1a,65535-salt);putB(0x7a1c,salt);putB(0x7ace,255-salt);};
function locate(snapshot,mode,map,progress){
 rt.restoreBytes(fs.readFileSync(snapshot));rt.drainEventsBytes();
 const m=mem(),found=[];
 for(let i=0;i+0x8000<m.length;++i)
  if(m[i+0x7934]===mode&&m[i+0x7935]===0&&m[i+0x7a58]===map&&m[i+0x7a66]===progress&&
   (mode===1?m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6:m[i+0x7936]===1))found.push(i);
 let live=found;
 if(mode===1){key(64);live=found.filter(i=>{base=i;return w(0x7934)===6;});}
 if(live.length!==1)throw Error('live RAM candidates '+live.length);
 base=live[0];lowerBase=base-8;
}
function freeze(row){
 rows.push(row);fs.writeFileSync(`${dir}/report.json`,JSON.stringify({original_only:true,rows},null,2)+'\n');
 console.log(JSON.stringify({route:row.route,salt:row.salt,before:row.beforeState,after:row.afterState,
  textChanges:JSON.stringify(row.before)!==JSON.stringify(row.after),commits:row.commits}));
}
for(const route of ['new','empty'])for(const salt of [19,173]){
 locate('tmp/native-title-load-saved.snapshot',1,1,1);
 if(w(0x7934)!==6)throw Error('system entry');key(64);
 putW(0x7a50,0);putW(0x7934,7);putW(0x7ab4,1);idle(32);
 if(route==='empty')for(const mask of [2,16,2])key(mask);
 seed(salt);putB(0x7ac2,salt);putB(0x7ac4,1);putW(0x7936,0);
 const row={route,salt,before:banks(),beforeState:state(),commits:0};
 for(let t=0;t<20;++t){tick(t===0?16:0);for(let j=0;j<rt.displayCommitCount();++j){
  const pixels=rt.displayCommitPixelIndices(j);if(pixels)fs.writeFileSync(`${dir}/${route}-${salt}-${row.commits++}.idx`,pixels);
 }}
 row.after=banks();row.afterState=state();
 if(row.afterState.mode!==1||row.afterState.map!==1||row.afterState.progress!==1)throw Error('new game outcome');
 freeze(row);
}
rt.free();

}
{
const dir='tmp/story-text-title-boot-native';fs.mkdirSync(dir,{recursive:true});
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const pack=fs.readFileSync('tmp/tianjiang-online.opennoah'),persistent=fs.readFileSync('tmp/native-title-load-persistent.bin');
const rows=[];
for(const salt of [19,173]){
 const rt=new GameRuntime(pack,'certified_original_v1');
 rt.installPersistentDataBytes(persistent);rt.setBootTarget('title');const start=rt.boot();start.free();
 const tick=mask=>{const o=rt.advance(mask);o.free();},idle=n=>{for(let i=0;i<n;++i)tick(0);};
 const key=mask=>{tick(mask);idle(15);};idle(1000);
 const mem=()=>new Uint8Array(exp.memory.buffer),data=mem(),found=[];
 for(let i=0;i+0x8000<data.length;++i)if(data[i+0x7934]===7&&data[i+0x7935]===0&&
  data[i+0x7aec]===0xa9&&data[i+0x7aeb]===6)found.push(i);
 if(found.length!==1)throw Error('cold title RAM '+found.length);
 const base=found[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v&255;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 const banks=()=>({pointer:w(0x797a)*65536+w(0x797c),rect:[0x791e,0x7920,0x7922,0x7924].map(w),offset:w(0x7a1a),more:b(0x7a1c),style:b(0x7ace)});
 const state=()=>({mode:w(0x7934),event:w(0x7936),map:w(0x7a58),progress:w(0x7a66),partyCount:b(0x7afa)});
 key(2);key(16);
 for(let i=0;i<4;++i){putB(0x797a+i,(i*37+salt)&255);putW(0x791e+2*i,(0x8123+i*97+salt)&65535);}putW(0x7a1a,65535-salt);putB(0x7a1c,salt);putB(0x7ace,255-salt);putB(0x7ac4,1);
 const row={salt,base,before:banks(),beforeState:state(),commits:[]};
 for(let t=0;t<32;++t){tick(t===0?16:0);for(let j=0;j<rt.displayCommitCount();++j){
  const p=rt.displayCommitPixelIndices(j);if(p){const index=row.commits.length;
   fs.writeFileSync(`${dir}/${salt}-${index}.idx`,p);row.commits.push({tick:t,state:state()});
  }
 }}
 row.after=banks();row.afterState=state();
 fs.writeFileSync(`${dir}/${salt}.snapshot`,rt.snapshotBytes());
 rows.push(row);fs.writeFileSync(`${dir}/report.json`,JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({salt,before:row.beforeState,after:row.afterState,
  textChanges:JSON.stringify(row.before)!==JSON.stringify(row.after),commits:row.commits.length}));
 rt.free();
}

}
