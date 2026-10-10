import fs from 'node:fs';
import {spawnSync} from 'node:child_process';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const candidates=JSON.parse(fs.readFileSync('tmp/other-npe-skill-gate-candidates.json'));
const cases=candidates.map((c,i)=>({name:['dragon-enemy','roc-unknown','soul-zero'][i],source:c.path,sourceSkill:c.skill}));
cases[0].reject=true;
cases.push({...cases[0],name:'dragon-enemy-fallen',reject:false,fallBeforeConfirm:true});
const name=process.argv[2],output='tmp/native-other-npe-skill.json';
if(!name){fs.writeFileSync(output,'[]\n');for(const c of cases){const p=spawnSync(process.execPath,[process.argv[1],c.name],{stdio:'inherit'});if(p.error)throw p.error;if(p.status)throw Error(c.name+' failed '+p.status);}process.exit(0);}
const cfg=cases.find(c=>c.name===name);if(!cfg)throw Error('unknown case');
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
try{
 rt.restoreBytes(fs.readFileSync('tmp/battle-fresh-start.bin'));rt.drainEventsBytes();
 const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
 for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===3&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7a58]===9&&mem()[i+0x7a66]===6)found.push(i);
 if(found.length!==1)throw Error('RAM discovery');
 const base=found[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v&255;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 const tick=mask=>rt.advance(mask).free(),idle=n=>{for(let t=0;t<n;++t)tick(0);},key=mask=>{tick(mask);idle(15);};
 const hero=mem().slice(a(0x69c6)-1,a(0x69c6+148)-1);
 for(let i=1;i<3;++i)mem().set(hero,a(0x69c6+i*148)-1);
 putB(0x7afa,3);putB(0x7626,51);putB(0x7627,51);putW(0x7ab4,1);idle(30);
 for(let i=0;i<6;++i){const d=i*148;
  putW(0x649e+d,i<3?200:2000);putW(0x64a0+d,i<3?50:1000);
  putW(0x64a2+d,120);putW(0x64a4+d,i===0?(cfg.qi??100):100);
  putW(0x64a6+d,1);putW(0x64a8+d,i<3?999-i:1);putW(0x64aa+d,1);
  putW(0x64ac+d,i<3?90:1);putW(0x64b0+d,i<3?90:1);
  putW(0x64ae+d,0);putW(0x64b2+d,0);putB(0x6468+d,99);
  for(let j=0;j<4;++j)putW(0x648e+d+j*2,i>=3&&j===2?-36:0);
  if(i>=3)for(let j=0;j<3;++j)putB(0x6469+d+j,0);
 }
 for(let i=0;i<3;++i){putB(0x69de+148*i,99);for(let j=0;j<18;++j)putB(0x69df+148*i+j,j===0?(i===1?cfg.second??6:6):0);}
 const cache=()=>({id:b(0x7a74),bytes:Array.from({length:144},(_,i)=>b(0x730e+i))});
 const state=()=>({mode:w(0x7934),actor:b(0x7a9e),menu:b(0x7ac8),cache:cache(),
  commands:Array.from({length:3},(_,i)=>({kind:b(0x64cf+148*i),id:b(0x64d6+148*i),target:b(0x64ce+148*i),type:b(0x64d0+148*i),group:b(0x64d3+148*i),side:b(0x64d4+148*i),flag:b(0x64e2+148*i)})),
  records:Array.from({length:6},(_,i)=>{const d=i*148;return {hp:w(0x64a0+d),maxHp:w(0x649e+d),qi:w(0x64a4+d),maxQi:w(0x64a2+d),attack:w(0x64b0+d),defense:w(0x64b2+d),level:b(0x6468+d),status:Array.from({length:4},(_,j)=>w(0x648e+d+j*2))};})});
 // Read the complete record from the named other NPE. The original program
 // and resource file remain intact; only its shared RAM cache is substituted.
 const source=fs.readFileSync(cfg.source),section=source.readUInt32BE(0x3a+3*8);
 const start=section+source.readUInt32BE(section+4*(cfg.sourceSkill-1));
 const endWord=source.readUInt32BE(section+4*cfg.sourceSkill);
 const end=endWord===0xffffffff?section+source.readUInt32BE(0x3a+3*8+4):section+endWord;
 const raw=source.subarray(start,end),cacheBytes=Buffer.alloc(144);
 raw.copy(cacheBytes,0,0,12);cacheBytes[12]=0;raw.copy(cacheBytes,114,12,15);
 raw.copy(cacheBytes,117,37,39);raw.copy(cacheBytes,128,39,41);
 for(const [i,target] of [120,122,124,126,130,132,134,140,136,138,142].entries())raw.copy(cacheBytes,target,15+2*i,17+2*i);
 let desc=raw.subarray(41);const zero=desc.indexOf(0);if(zero>=0)desc=desc.subarray(0,zero+1);
 desc.copy(cacheBytes,13,0,Math.min(desc.length,101));
 putB(0x7a74,6);for(let i=0;i<144;++i)putB(0x730e+i,cacheBytes[i]);
 const initial=state();cfg.raw=Array.from(raw);cfg.cache=Array.from(cacheBytes);
 const gateSteps=[];const checkpoint=label=>{gateSteps.push({label,state:state()});fs.writeFileSync(`tmp/native-other-npe-skill-${name}-gates.json`,JSON.stringify(gateSteps,null,2));fs.writeFileSync(`tmp/native-other-npe-skill-${name}-gate-${gateSteps.length-1}.idx`,rt.framePixelIndices());};
 const magic=()=>{
  for(const mask of [4,1,16]){key(mask);checkpoint(`input${mask}`);}
  if(b(0x7a74)!==6 || b(0x7382)!==raw[14] || b(0x7383)!==raw[37] || b(0x7384)!==raw[38])throw Error('cache did not survive prefix/list');
  key(16);checkpoint("confirm-list");
  if(cfg.fallBeforeConfirm){const target=b(0x64ce)-1;putW(0x64a0+148*target,0);checkpoint('target-fallen');}
  if(raw[37]===0){key(16);checkpoint("confirm-target");}
 };
 const prepareDefend=()=>{if(b(0x7ac8)!==30){key(4);key(2);}key(16);for(let n=0;b(0x7a9c)!==2&&n<6;++n)key(2);if(b(0x7ac8)!==31)throw Error('defend gate');};
 magic();
 if(cfg.reject){
  if(b(0x7a9e)!==0||b(0x7ac8)!==27)throw Error('expected living-target refusal');
  const after=state(),rows=JSON.parse(fs.readFileSync(output)).filter(c=>c.name!==name);
  rows.push({cfg,initial,after,gates:gateSteps,frames:[]});fs.writeFileSync(output,JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({name,refusedLivingTarget:true,command:after.commands[0]}));process.exitCode=0;
 }else{
 if(b(0x7a9e)!==1)throw Error('queue first actor='+b(0x7a9e)+' menu='+b(0x7ac8));
 if(cfg.second)magic();else{prepareDefend();key(16);}
 if(b(0x7a9e)!==2)throw Error('queue second');
 prepareDefend();
 const before=state();const frames=[];
 for(let t=0;t<300;++t){tick(t===0?16:0);for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(pixels){const index=frames.length;fs.writeFileSync(`tmp/native-other-npe-skill-${name}-${index}.idx`,pixels);frames.push({index,tick:t});}}}
 const after=state();if(after.actor!==0||after.mode!==3||![20,30].includes(after.menu)){fs.writeFileSync(`tmp/native-other-npe-skill-${name}-failed.json`,JSON.stringify({before,after,frames}));throw Error('round end '+after.actor+'/'+after.menu);}
 const rows=JSON.parse(fs.readFileSync(output)).filter(c=>c.name!==name);rows.push({cfg,initial,before,after,frames});fs.writeFileSync(output,JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name,kind:raw[14],group:raw[37],side:raw[38],frames:frames.length,before:before.commands[0],after:after.records,cacheUnchanged:JSON.stringify(before.cache)===JSON.stringify(after.cache)}));
 }
}finally{rt.free();}
