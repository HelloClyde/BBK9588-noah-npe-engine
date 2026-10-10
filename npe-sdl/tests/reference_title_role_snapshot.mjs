import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const dir='tmp/title-role-snapshot-native';fs.mkdirSync(dir,{recursive:true});
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
let base;
const mem=()=>new Uint8Array(exp.memory.buffer),a=o=>base+o-(o<0x7000?8:0);
const b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
const putB=(o,v)=>{mem()[a(o)-1]=v&255;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
const tick=mask=>{const o=rt.advance(mask);o.free();},idle=n=>{for(let i=0;i<n;++i)tick(0);};
const key=mask=>{tick(mask);idle(15);};
function locate(mode,progress,transition=false){
 const data=mem(),found=[];
 for(let i=0;i+0x8000<data.length;++i)if(data[i+0x7934]===mode&&data[i+0x7935]===0&&
  data[i+0x7936]===1&&data[i+0x7a58]===9&&data[i+0x7a66]===progress)found.push(i);
 if(transition)key(64);
 const live=transition?found.filter(i=>{base=i;return w(0x7934)===6;}):found;
 if(live.length!==1)throw Error('live RAM '+live.length);base=live[0];
}
const banks=()=>({battle:Array.from({length:888},(_,i)=>b(0x6450+i)),map:Array.from({length:444},(_,i)=>b(0x69c6+i))});
const rows=[];
for(const route of ['system','title'])for(const salt of [19,173]){
 rt.restoreBytes(fs.readFileSync('tmp/battle-fresh-start.bin'));locate(3,6);
 putW(0x7934,1);putW(0x7ab4,1);putW(0x7a66,1);putW(0x7a50,0);idle(35);putB(0x7ac4,1);
 const saved=banks();
 for(const mask of [64,2,2,2,16,2,16,16])key(mask);
 if(w(0x7934)!==1)throw Error('save');putW(0x7a66,4);
 if(route==='title'){putW(0x7934,7);putW(0x7ab4,1);idle(35);}
 for(const mask of (route==='system'?[64,2,2,2,16,16]:[2,16]))key(mask);
 for(let i=0;i<888;++i)putB(0x6450+i,(i*37+salt)%255+1);
 for(let i=0;i<444;++i)putB(0x69c6+i,(i*53+salt)%255+1);
 putB(0x7ac4,1);const before=banks();key(16);idle(16);
 const snapshot=rt.snapshotBytes();fs.writeFileSync(`${dir}/${route}-${salt}.snapshot`,snapshot);
 const direct=banks(),pixels=rt.framePixelIndices();
 rt.restoreBytes(snapshot);locate(1,1,true);
 const after=banks();
 rows.push({route,salt,saved,before,direct,after,base});
 fs.writeFileSync(`${dir}/report.json`,JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({route,salt,base,
  battleDiff:after.battle.filter((v,i)=>v!==before.battle[i]).length,
  mapDiff:after.map.filter((v,i)=>v!==saved.map[i]).length,
  mapStart:after.map.slice(0,16),battleStart:after.battle.slice(0,16),
  savedStart:saved.map.slice(0,16),snapshotFrameDiff:rt.framePixelIndices().filter((v,i)=>v!==pixels[i]).length}));
}
rt.free();
