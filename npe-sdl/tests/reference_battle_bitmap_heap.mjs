import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const cases=[
 {name:'resident-29',gapByte:4,pose0:29,learnPanels:2},
 {name:'extended-30',gapByte:4,pose0:30,learnPanels:2},
 {name:'extended-31',gapByte:4,pose0:31,learnPanels:2},
 {name:'short-header-34',gapByte:4,pose0:34,learnPanels:2},
 {name:'displacement-wrap-10952',gapByte:4,pose0:10952,learnPanels:2},
].map(c=>({item:159,throwing:true,cached:159,learn:true,skillCached:4,skills:[],activeByte:2,groupByte:2,...c}));
const mode=process.argv[3]??'plain';if(!['plain','trace'].includes(mode))throw Error('mode must be plain or trace');
const name=process.argv[2];
if(!name)throw Error('usage: node reference_battle_bitmap_heap.mjs extended-31|short-header-34 plain|trace');
if(!['extended-31','short-header-34'].includes(name))throw Error('unsupported pointer case');
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

 const u32=o=>w(o)*65536+w(o+2);
 const observe=()=>({heapHandle:w(0x7a88),packed:u32(0x79a6),
  trace:[w(0x7e60),w(0x7e62),w(0x7e64),w(0x7e66)],fields:state(),rawRAM:Buffer.from(mem().slice(base-0x2000,base+0x10000)).toString('base64')});

 // This optional trace changes only isolated guest instruction memory.
 // Plain mode executes the original reference without instrumentation.
 // Never use a traced run as an unmodified reference fixture.
 if(mode==='trace'){
 const all=Buffer.from(exp.memory.buffer),program=fs.readFileSync('tmp/NP560.PRG');
 const pc=0x459096,tramp=0x45f000;
 const words=[0x1f10,0x58f2,0x6810,0x00a4,
  0xa8f0,0x7e60,0xa8f1,0x7e62,
  0x782b,0xffee,0xa8f2,0x7e64,
  0x88f2,0x7e66,0x5021,0xa8f2,0x7e66,
  0x382f,0x1f90,0x6a0b,0xfffc,0x6a1b,0xfffe,0xc620];
 const raw=Buffer.alloc(words.length*2);words.forEach((v,i)=>raw.writeUInt16LE(v,2*i));
 const encode=(data,width,endian)=>{
  const out=Buffer.alloc(data.length/2*width);
  for(let i=0;i<data.length/2;++i){const v=data.readUInt16LE(i*2);
   if(width===2){if(endian==='le')out.writeUInt16LE(v,i*width);else out.writeUInt16BE(v,i*width);}
   else if(endian==='le')out.writeUInt32LE(v,i*width);else out.writeUInt32BE(v,i*width);
  }return out;
 };
 const sites=[],search=[];
 for(const width of [2,4])for(const endian of ['le','be']){
  const sig=encode(program.subarray(pc-0x200000,pc-0x200000+24),width,endian);
  const target=encode(program.subarray(tramp-0x200000,tramp-0x200000+raw.length),width,endian);
  const code=encode(raw,width,endian),jump=Buffer.from([0x28,0xbd,0,0xf0,0,0xc7,0,0xc7]);
  const matches=[];
  for(let p=all.indexOf(sig);p>=0;p=all.indexOf(sig,p+1)){
   const dst=p+(tramp-pc)/2*width;
   const valid=dst>=0&&dst+code.length<=all.length&&all.subarray(dst,dst+target.length).equals(target);
   matches.push({p,dst,valid});if(valid)sites.push({p,dst,width,endian,code,jump:encode(jump,width,endian)});
  }search.push({width,endian,matches});
 }

 const maps=[];
 for(let addr=0x458000;addr<0x460000;addr+=0x1000){
  const sig=encode(program.subarray(addr-0x200000,addr-0x200000+48),2,'be'),matches=[];
  for(let p=all.indexOf(sig);p>=0;p=all.indexOf(sig,p+1))matches.push(p);
  maps.push({addr,matches});
 }
 const targetBE=encode(program.subarray(tramp-0x200000,tramp-0x200000+raw.length),2,'be'),tramps=[];
 for(let p=all.indexOf(targetBE);p>=0;p=all.indexOf(targetBE,p+1))tramps.push(p);
 console.log(JSON.stringify({name,search,maps,tramps}));
 const codeBE=encode(raw,2,'be'),jumpBE=encode(Buffer.from([0x28,0xbd,0,0xf0,0,0xc7,0,0xc7]),2,'be');
 const instructionSites=search.find(s=>s.width===2&&s.endian==='be').matches;
 // Guest mappings may be page allocations; only patch an isolated diagnostic
 // when a complete original target exists in the nearby address region.
 if(instructionSites.length!==2)throw Error('expected two mapped instruction copies');
 const targets=tramps.filter(p=>p>instructionSites[1].p&&p-instructionSites[0].p<0x30000);
 if(targets.length!==2)throw Error('expected two mapped trampoline copies');
 for(let i=0;i<instructionSites.length;++i){
  codeBE.copy(all,targets[i]);jumpBE.copy(all,instructionSites[i].p);
 }
 for(let o=0x7e60;o<=0x7e66;o+=2)putW(o,0);
 }
 const before=observe(),samples=[];
 for(let t=0;t<4;++t){
  tick(t===0?16:0);const frames=[];
  for(let j=0;j<rt.displayCommitCount();++j){
   const pixels=rt.displayCommitPixelIndices(j);if(!pixels)throw Error('missing commit');
   const path=`tmp/native-battle-bitmap-heap-${mode}-${name}-${t}-${j}.idx`;fs.writeFileSync(path,pixels);frames.push(path);
  }
  samples.push({tick:t,commits:rt.displayCommitCount(),frames,...observe()});
 }
 const wire=Buffer.from(rt.stateBytes());
 fs.writeFileSync(`tmp/native-battle-bitmap-heap-${mode}-${name}-state.bin`,wire);
 fs.writeFileSync(`tmp/native-battle-bitmap-heap-${mode}-${name}.json`,JSON.stringify({mode,name,base,before,samples},null,2));
 console.log(JSON.stringify({name,mode,base,before:before.heapHandle,after:samples.map(s=>[s.tick,s.heapHandle,s.commits,s.trace])}));
}finally{rt.free();}
