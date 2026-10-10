import fs from 'node:fs';
import {spawnSync} from 'node:child_process';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
// Isolated original RAM fixtures; no game/resource/snapshot file is changed.
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const configs=[{name:'one',flags:[1,0,0],order:[0,1,2]},
 {name:'all',flags:[1,1,1],order:[0,1,2]},
 {name:'sparse',flags:[1,0,1],order:[2,1,0]}];
const output='tmp/native-character-metadata.json',selected=process.argv[2];
if(!selected){fs.writeFileSync(output,'[]\n');for(const c of configs){const p=spawnSync(process.execPath,[process.argv[1],c.name],{stdio:'inherit'});if(p.error)throw p.error;if(p.status)throw Error(c.name+' failed '+p.status);}process.exit(0);}
const chosen=configs.filter(c=>c.name===selected);if(chosen.length!==1)throw Error('unknown case');
const rows=[];
for(const config of chosen){
 rt.restoreBytes(fs.readFileSync('tmp/battle-fresh-start.bin'));
 const mem=()=>new Uint8Array(exp.memory.buffer),bank=Buffer.from(exp.memory.buffer),found=[];
 const signature=Buffer.from([0,3,0,1]);
 for(let p=bank.indexOf(signature);p>=0;p=bank.indexOf(signature,p+1)){
  const i=p-0x7934+1;if(i<0||i+0x8000>=bank.length)continue;
  if(bank[i+0x7a58]===9&&bank[i+0x7a66]===6&&bank[i+0x7a67]===0)found.push(i);
 }
 if(found.length!==1)throw Error('unique original RAM page '+found.length);
 const base=found[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 // Leave the previous battle via a real mode1 dispatch before entering
 // mode3 again. A mode3 redraw alone has a different presentation order.
 putW(0x7934,1);putW(0x7936,0);putW(0x7a50,0);putW(0x7ab4,1);
 for(let tick=0;tick<30;++tick){const out=rt.advance(0);out.free();}
 if(w(0x7934)!==1||w(0x7a58)!==9)throw Error('map dispatch before entry');
 const metadata=(o)=>({sex:b(o+17),poses:Array.from({length:8},(_,j)=>w(o+46+2*j)),animations:Array.from({length:3},(_,j)=>b(o+136+j)),gear:Array.from({length:6},(_,j)=>b(o+18+j))});
 const writeMetadata=(o,m)=>{putB(o+17,m.sex);m.poses.forEach((v,j)=>putW(o+46+2*j,v));m.animations.forEach((v,j)=>putB(o+136+j,v));};
 const defaults=[0,1,2].map(i=>metadata(0x69c6+148*i));
 const mapMetadata=()=>[0,1,2].map(i=>metadata(0x69c6+148*i));
 const battleMetadata=()=>Array.from({length:b(0x7afa)},(_,i)=>metadata(0x6450+148*i));
 const originals=Array.from({length:3},(_,i)=>mem().slice(a(0x69c6+148*i)-1,a(0x69c6+148*(i+1))-1));
 for(let i=0;i<3;++i){mem().set(originals[config.order[i]],a(0x69c6+148*i)-1);putB(0x69f1+148*i,config.flags[i]);putW(0x6a16+148*i,80+10*i);}
 putB(0x7afa,config.flags.filter(f=>f===1).length);
 putW(0x7934,3);putW(0x7936,1);putW(0x7a50,0);putW(0x7ab4,1);
 const state=()=>({mode:w(0x7934),event:w(0x7936),map:w(0x7a58),progress:w(0x7a66),
  flags:[0,1,2].map(i=>b(0x69f1+148*i)),mapIds:[0,1,2].map(i=>b(0x69d2+148*i)),party:b(0x7afa),actor:b(0x7a9e),target:b(0x7ac0),ids:[w(0x792e),w(0x7930),w(0x7932)],
  mapHp:Array.from({length:3},(_,i)=>w(0x6a16+148*i)),
  view:{x:b(0x7a2e),y:b(0x7a2c),sx:b(0x69d5),sy:b(0x69d6),direction:b(0x69d4)},
  records:Array.from({length:b(0x7afa)+3},(_,i)=>{const o=148*i;return {
   id:b(0x645c+o),target:b(0x64ce+o),name:Array.from({length:12},(_,j)=>b(0x6450+o+j)),hp:w(0x64a0+o),maxHp:w(0x649e+o),qi:w(0x64a4+o),maxQi:w(0x64a2+o),
   attack:w(0x64b0+o),defense:w(0x64b2+o),speed:w(0x64a8+o),
   status:Array.from({length:4},(_,s)=>w(0x648e+o+2*s))};})});
 // Distinct valid poses are selected from the loaded original records.
 for(let i=0;i<3;++i){const m=metadata(0x69c6+148*i);m.sex=[2,1,255][i];
  m.poses=[2,1,0,3,6,4,5,7].map(j=>m.poses[j]);
  m.animations=defaults[(i+1)%3].animations;writeMetadata(0x69c6+148*i,m);}
 const metadataBefore=mapMetadata();
 for(let i=0;i<3;++i)putB(0x69f2+148*i,[0,2,255][i]);
 const groups=()=>({map:[0,1,2].map(i=>b(0x69f2+148*i)),battle:Array.from({length:b(0x7afa)},(_,i)=>b(0x647c+148*i)),ids:[0,1,2].map(i=>b(0x69d2+148*i)),flags:[0,1,2].map(i=>b(0x69f1+148*i)),mode:w(0x7934)});
 const before=groups(),commits=[];
 for(let tick=0;tick<160;++tick){const out=rt.advance(0);out.free();
  for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
   fs.writeFileSync(`tmp/native-character-metadata-${config.name}-commit-${commits.length}.idx`,pixels);commits.push({tick});}}
 const after=groups();if(commits.length!==4)throw Error('entry commits '+commits.length);
 const metadataEntry=battleMetadata();
 for(let i=0;i<after.battle.length;++i){const m=metadata(0x6450+148*i);m.sex=[1,2,0][i];
  [m.poses[0],m.poses[1]]=[m.poses[1],m.poses[0]];
  m.animations=defaults[(i+2)%3].animations;writeMetadata(0x6450+148*i,m);}
 const metadataMutated=battleMetadata();
 for(let i=0;i<after.battle.length;++i)putB(0x647c+148*i,[1,255,2][i]);
 const mutated=groups();
 // The original random-battle flee service is reached through real keys.
 putB(0x7a9a,0);putW(0x7936,0);
 const key=mask=>{let out=rt.advance(mask);out.free();for(let t=0;t<15;++t){out=rt.advance(0);out.free();}};
 for(const mask of [4,2,16])key(mask);
 if(b(0x7ac8)!==31)throw Error('flee submenu '+b(0x7ac8));
 for(let tries=0;tries<4&&w(0x7934)===3;++tries){
  for(let t=0;b(0x7a9c)!==3&&t<5;++t)key(2);
  key(16);for(let t=0;t<200;++t){const out=rt.advance(0);out.free();}
  if(w(0x7934)===3)key(16);
 }
 if(w(0x7934)!==1)throw Error('original flee not completed');
 const exit=groups(),metadataExit=mapMetadata();
 if(exit.flags.some((f,i)=>f!==config.flags[i]))throw Error('map flags changed');

 let removed=null,metadataRemoved=null;
 if(config.name==='all'){
  putB(0x78a2,0);putB(0x78a3,2);putB(0x78a4,1);putW(0x78a6,0);putW(0x7934,10);putW(0x7936,1);
  for(let t=0;t<300;++t){const out=rt.advance(0);out.free();}
  if(w(0x7934)!==1)throw Error('leave2 followup');removed=groups();metadataRemoved=mapMetadata();
 }
 putW(0x7934,3);putW(0x7936,1);putW(0x7ab4,1);
 for(let t=0;t<160;++t){const out=rt.advance(0);out.free();}
 const reentry=groups(),metadataReentry=battleMetadata();
 rows.push({config,defaults,metadataBefore,metadataEntry,metadataMutated,metadataExit,metadataRemoved,metadataReentry,before,after,mutated,commits,exit,removed,reentry});console.log(JSON.stringify({name:config.name,after,exit,removed,reentry,commits:commits.length}));

}
const old=JSON.parse(fs.readFileSync(output));fs.writeFileSync(output,JSON.stringify([...old,...rows],null,2)+'\n');rt.free();
