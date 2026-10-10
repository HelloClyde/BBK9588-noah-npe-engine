import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
// Original runtime only. No C output participates in these references.
const experimental=process.argv.includes('--title-experimental');
const dir=experimental?'tmp/role-bank-lifetime-native-title-experimental':'tmp/role-bank-lifetime-native-final';
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
const banks=()=>({battle:Array.from({length:888},(_,i)=>b(0x6450+i)),map:Array.from({length:444},(_,i)=>b(0x69c6+i))});
const state=()=>({mode:w(0x7934),event:w(0x7936),map:w(0x7a58),progress:w(0x7a66),partyCount:b(0x7afa)});
const seed=(salt)=>{
 for(let i=0;i<888;++i)putB(0x6450+i,(i*37+salt)%255+1);
 for(let i=0;i<444;++i)putB(0x69c6+i,(i*53+salt)%255+1);
};
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
  battleChanges:row.before.battle.filter((v,i)=>v!==row.after.battle[i]).length,
  mapChanges:row.before.map.filter((v,i)=>v!==row.after.map[i]).length,commits:row.commits}));
}
function mapMatches(saved){
 const needle=Uint8Array.from(saved),matches=[],data=mem();
 for(let i=0;i+needle.length<data.length;++i)if(data[i]===needle[0]&&data[i+1]===needle[1]){
  let j=2;while(j<needle.length&&data[i+j]===needle[j])++j;
  if(j===needle.length)matches.push(i);
 }return matches;
}
for(const route of ['new','empty'])for(const salt of [19,173]){
 locate('tmp/native-title-load-saved.snapshot',1,1,1);
 if(w(0x7934)!==6)throw Error('system entry');key(64);
 putW(0x7a50,0);putW(0x7934,7);putW(0x7ab4,1);idle(32);
 if(route==='empty')for(const mask of [2,16,2])key(mask);
 seed(salt);putB(0x7ac4,1);putW(0x7936,0);
 const row={route,salt,before:banks(),beforeState:state(),commits:0};
 for(let t=0;t<20;++t){tick(t===0?16:0);for(let j=0;j<rt.displayCommitCount();++j){
  const pixels=rt.displayCommitPixelIndices(j);if(pixels)fs.writeFileSync(`${dir}/${route}-${salt}-${row.commits++}.idx`,pixels);
 }}
 row.after=banks();row.afterState=state();
 if(row.afterState.mode!==1||row.afterState.map!==1||row.afterState.progress!==1)throw Error('new game outcome');
 freeze(row);
}
// Title diagnostics currently lack a verified lower-RAM address after loading.
// Keep them outside the accepted reference until that mapping is established.
for(const route of (experimental?['system-load','title-load']:['system-load']))for(const salt of [19,173]){
 locate('tmp/battle-fresh-start.bin',3,9,6);
 putW(0x7934,1);putW(0x7ab4,1);putW(0x7a66,1);putW(0x7a50,0);idle(35);
 putB(0x7ac4,1);
 const saved=banks();
 for(const mask of [64,2,2,2,16,2,16,16])key(mask);
 if(w(0x7934)!==1)throw Error('save did not return');
 putW(0x7a66,4);
 if(route==='title-load'){putW(0x7934,7);putW(0x7ab4,1);idle(35);}
 for(const mask of (route==='system-load'?[64,2,2,2,16,16]:[2,16]))key(mask);
 const beforeMapMatches=mapMatches(saved.map);
 if(route==='title-load'){
  if(beforeMapMatches.length!==1)throw Error('preload map copies '+JSON.stringify(beforeMapMatches));
  lowerBase=beforeMapMatches[0]-0x69c6+1;
 }
 seed(salt);putB(0x7ac4,1);
 const row={route,salt,saved,before:banks(),beforeState:state(),beforeLowerBase:lowerBase,beforeMapMatches,commits:0};
 for(let t=0;t<32;++t){tick(t===0?16:0);for(let j=0;j<rt.displayCommitCount();++j){
  const pixels=rt.displayCommitPixelIndices(j);if(pixels)fs.writeFileSync(`${dir}/${route}-${salt}-${row.commits++}.idx`,pixels);
 }}
 if(route==='title-load'){
  const matches=mapMatches(saved.map);
  if(matches.length!==1)throw Error('postload map copies '+JSON.stringify(matches));
  lowerBase=matches[0]-0x69c6+1;row.afterLowerBase=lowerBase;
  fs.writeFileSync(`${dir}/${route}-${salt}-state.bin`,rt.stateBytes());
 }
 row.after=banks();row.afterState=state();
 if(row.afterState.mode!==1||row.afterState.progress!==1)throw Error('load outcome '+JSON.stringify(row.afterState));
 freeze(row);
}
rt.free();
