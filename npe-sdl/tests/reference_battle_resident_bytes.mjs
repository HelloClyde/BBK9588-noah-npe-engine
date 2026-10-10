import fs from 'node:fs';
import {spawnSync} from 'node:child_process';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const cases=[
 {name:'gap-zero',gapByte:0,learnPanels:2},
 {name:'gap-two',gapByte:2,learnPanels:3},
 {name:'gap-four',gapByte:4,learnPanels:2},
 {name:'pose-high-two',gapByte:2,pose0:512,learnPanels:4},
 {name:'pose-next-four',gapByte:2,pose0:514,pose1:1024,learnPanels:5},
 {name:'residue-tail-copy',gapByte:2,tail:[165,90],learnPanels:3},
 {name:'pose-high-two-late',gapByte:2,pose0:512,deferPose:true,learnPanels:4},
 {name:'pose-next-four-late',gapByte:2,pose0:514,pose1:1024,deferPose:true,learnPanels:5},
].map(c=>({item:159,throwing:true,cached:159,learn:true,skillCached:4,skills:[],activeByte:2,groupByte:2,...c}));
const name=process.argv[2],output='tmp/native-battle-resident-bytes.json';
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
  putW(0x649e+d,i<3?200:2000);putW(0x64a0+d,i<3?50:i===4?(cfg.enemyHp??1):0);
  putW(0x64a2+d,120);putW(0x64a4+d,100);
  putW(0x64a6+d,1);putW(0x64a8+d,i<3?(cfg.reverse?[997,999,998][i]:999-i):1);putW(0x64aa+d,1);
  putW(0x64ac+d,i<3?90:1);putW(0x64b0+d,i<3?90:1);
  putW(0x64ae+d,0);putW(0x64b2+d,0);putB(0x6468+d,99);
  for(let j=0;j<4;++j)putW(0x648e+d+j*2,i>=3&&j===2?-36:0);
  if(i>=3){for(let j=0;j<3;++j)putB(0x6469+d+j,0);putW(0x649c+d,i===4?1:0);putW(0x64b4+d,0);for(let j=0;j<2;++j)putB(0x64ca+d+j,cfg.drops?.[i-3]?.[j]??0);}
  else {putW(0x649a+d,10);putW(0x649c+d,cfg.learn&&i===0?9:0);putB(0x6468+d,cfg.learn&&i===0?1:99);for(let j=0;j<18;++j)putB(0x6469+d+j,cfg.skills?.[j]??0);}
 }
 for(let id=0;id<255;++id)putW(0x67c8+2*id,0);
 const stock=()=>[48,50,159,160].map(id=>[id,w(0x67c8+2*id)]);
 const cache=()=>({id:b(0x7a72),bytes:Array.from({length:142},(_,i)=>b(0x739e+i))});
 const state=()=>({mapRecords:Array.from({length:3},(_,i)=>Array.from({length:148},(_,j)=>b(0x69c6+148*i+j))),battleRecords:Array.from({length:6},(_,i)=>Array.from({length:148},(_,j)=>b(0x6450+148*i+j))),mapGroups:Array.from({length:3},(_,i)=>b(0x69f2+148*i)),battleGroups:Array.from({length:3},(_,i)=>b(0x647c+148*i)),gapBytes:Array.from({length:3},(_,i)=>b(0x647d+148*i)),mapFlags:Array.from({length:3},(_,i)=>b(0x69f1+148*i)),battleFlags:Array.from({length:3},(_,i)=>b(0x647b+148*i)),mode:w(0x7934),actor:b(0x7a9e),menu:b(0x7ac8),event:w(0x7936),cache:cache(),skillCache:{id:b(0x7a74),bytes:Array.from({length:144},(_,i)=>b(0x730e+i))},stock:stock(),
  commands:Array.from({length:3},(_,i)=>({kind:b(0x64cf+148*i),id:b(0x64d5+148*i),target:b(0x64ce+148*i),type:b(0x64d1+148*i),group:b(0x64d3+148*i),side:b(0x64d4+148*i),flag:b(0x64e2+148*i)})),
  records:Array.from({length:6},(_,i)=>{const d=i*148;return {hp:w(0x64a0+d),maxHp:w(0x649e+d),experience:w(0x649c+d),next:w(0x649a+d),qi:w(0x64a4+d),maxQi:w(0x64a2+d),attack:w(0x64b0+d),defense:w(0x64b2+d),level:b(0x6468+d),status:Array.from({length:4},(_,j)=>w(0x648e+d+j*2))};})});
 const item=(id)=>{
  // Only this item is positive at list entry; queued selection already debited it.
  putW(0x67c8+2*id,1);
  for(const mask of [4,2,16])key(mask);
  for(let n=0;b(0x7a9c)!==1&&n<6;++n)key(2);
  key(16);if(cfg.throwing && b(0x7aa2)!==1)key(2);key(16);
  if(b(0x7ac8)!==33)throw Error('list gate '+b(0x7ac8));
  key(16);if(b(0x7ac8)===37)key(16);
 };
 const prepareDefend=()=>{if(b(0x7ac8)!==30){key(4);key(2);}key(16);for(let n=0;b(0x7a9c)!==2&&n<6;++n)key(2);if(b(0x7ac8)!==31)throw Error('defend gate');};
 const initial=state();item(cfg.item);if(b(0x7a9e)!==1)throw Error('queue first');
 if(cfg.second)item(cfg.second);else{prepareDefend();key(16);}
 if(b(0x7a9e)!==2)throw Error('queue second');prepareDefend();
 putB(0x7a72,cfg.cached);for(let i=0;i<142;++i)putB(0x739e+i,0xa5);
 const title=Buffer.from('bbbac4e6b5c0bedf','hex');for(let i=0;i<12;++i)putB(0x739e+i,title[i]??0);putB(0x73aa,0);
 const desc=Buffer.from('bec9cbb5c3f700','hex');for(let i=0;i<desc.length;++i)putB(0x73ab+i,desc[i]);
 for(let i=114;i<142;++i)putB(0x739e+i,0);
 putB(0x7428,cfg.changedFlags?2:cfg.throwing?31:1);
 putB(0x742a,cfg.changedFlags?1:0);putB(0x742b,cfg.changedFlags?(cfg.throwing?1:0):cfg.throwing?0:1);
 putW(0x7410,7);putW(0x7412,3);putW(0x7414,17);
 putB(0x7a74,cfg.skillCached??4);for(let i=0;i<144;++i)putB(0x730e+i,0xa5);
 const skillTitle=Buffer.from('bbbac4e6cfc9caf5','hex');for(let i=0;i<12;++i)putB(0x730e+i,skillTitle[i]??0);putB(0x731a,0);
 for(let i=0;i<desc.length;++i)putB(0x731b+i,desc[i]);for(let i=114;i<144;++i)putB(0x730e+i,0);
 putB(0x7381,2);putB(0x7382,1);putB(0x7384,1);
 putB(0x647b,cfg.activeByte);putB(0x647c,cfg.groupByte);putB(0x647d,cfg.gapByte);
 if(!cfg.deferPose&&cfg.pose0!==undefined)putW(0x647e,cfg.pose0);
 if(!cfg.deferPose&&cfg.pose1!==undefined)putW(0x6480,cfg.pose1);
 if(cfg.tail){putB(0x64e2,cfg.tail[0]);putB(0x64e3,cfg.tail[1]);}
 const before=state(),frames=[];let poseInjection=null;
 for(let t=0;t<(cfg.maxTicks??400);++t){tick(t===0?16:0);for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(pixels){const index=frames.length;fs.writeFileSync(`tmp/native-battle-resident-bytes-${name}-${index}.idx`,pixels);frames.push({index,tick:t,batchState:state()});}}
  if(cfg.deferPose&&!poseInjection&&frames.length>=17&&b(0x6468)===2){
   poseInjection={tick:t,commits:frames.length,level:b(0x6468),skillId:b(0x7a74)};
   if(cfg.pose0!==undefined)putW(0x647e,cfg.pose0);if(cfg.pose1!==undefined)putW(0x6480,cfg.pose1);
  }
 }
 const after=state();if(after.mode!==5){fs.writeFileSync(`tmp/native-battle-resident-bytes-${name}-failed.json`,JSON.stringify({before,after,frames}));throw Error('victory end '+after.mode);}
 const rows=JSON.parse(fs.readFileSync(output)).filter(c=>c.cfg.name!==name);rows.push({cfg,initial,before,after,frames,poseInjection});fs.writeFileSync(output,JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name,frames:frames.length,before:before.cache.id,after:after.cache.id,skillCache:after.skillCache.id,stock:after.stock,party:after.records.slice(0,3).map(r=>[r.hp,r.qi]),enemy:after.records.slice(3).map(r=>[r.hp,r.qi])}));
}finally{rt.free();}
