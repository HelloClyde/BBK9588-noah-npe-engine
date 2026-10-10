import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const dir='tmp/shop-catalog-title-boot-native';fs.mkdirSync(dir,{recursive:true});
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
 const putB=(o,v)=>{mem()[a(o)-1]=v&255;};
 const banks=()=>({catalog:Array.from({length:255},(_,i)=>b(0x6dba+i)),scanFirst:b(0x7a10),scanLast:b(0x7a12),inventoryOnly:b(0x7a14),battle:Array.from({length:888},(_,i)=>b(0x6450+i)),map:Array.from({length:444},(_,i)=>b(0x69c6+i))});
 const state=()=>({mode:w(0x7934),event:w(0x7936),map:w(0x7a58),progress:w(0x7a66),partyCount:b(0x7afa)});
 key(2);key(16);
 for(let i=0;i<888;++i)putB(0x6450+i,(i*37+salt)%255+1);
 for(let i=0;i<444;++i)putB(0x69c6+i,(i*53+salt)%255+1);
 for(let i=0;i<255;++i)putB(0x6dba+i,(i*37+salt)%256);putB(0x7a10,salt);putB(0x7a12,255-salt);putB(0x7a14,255);putB(0x7ac4,1);
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
  battleDiff:row.after.battle.filter((v,i)=>v!==row.before.battle[i]).length,
  mapStart:row.after.map.slice(0,16),battleStart:row.after.battle.slice(0,16),commits:row.commits.length}));
 rt.free();
}
