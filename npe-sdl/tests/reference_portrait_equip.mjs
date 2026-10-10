import fs from 'node:fs';
import {spawnSync} from 'node:child_process';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';

const cases=[
 {name:'rotated',portraits:[37,36,38]},
 {name:'small-zero',portraits:[36,8,0]},
 {name:'zero-first',portraits:[0,36,38]},
 {name:'word-wrap',portraits:[65535,36,38]},
 {name:'wide-second',portraits:[36,56,38]},
 {name:'wide-first',portraits:[56,8,38]},
 {name:'wide-nonempty',portraits:[56,8,38],gear:[85,85,85,85,85,85]},
 {name:'signed-fields',portraits:[36,56,38],signedFields:true},
].map(c=>({cached:85,stocks:[[85,4]],gear:[0,0,0,0,0,0],...c,
 members:2,gear2:[0,85,0,0,0,0],keys:[16,2,1,16,16,2,1,64,64]}));

const name=process.argv[2];
if(!name){fs.writeFileSync('tmp/native-portrait-equip.json','[]\n');
 for(const cfg of cases){const p=spawnSync(process.execPath,[process.argv[1],cfg.name],{stdio:'inherit'});if(p.error)throw p.error;if(p.status)throw Error(cfg.name+' failed '+p.status);}process.exit(0);}
const cfg=cases.find(c=>c.name===name);if(!cfg)throw Error('unknown case');
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>rt.advance(mask).free();
try{
 rt.restoreBytes(fs.readFileSync('tmp/native-title-load-saved.snapshot'));rt.drainEventsBytes();
 const candidates=[];for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===1&&mem()[i+0x7935]===0&&mem()[i+0x7a58]===1&&mem()[i+0x7a66]===1&&mem()[i+0x7aec]===0xa9&&mem()[i+0x7aeb]===6)candidates.push(i);
 for(let t=0;t<16;++t)tick(t===0?64:0);
 const live=candidates.filter(i=>mem()[i+0x7934]===6);if(live.length!==1)throw Error('RAM discovery '+live.length);
 const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 for(let t=0;t<16;++t)tick(t===0?64:0);
 putW(0x7a50,0);putB(0x7a32,0);
 for(let id=0;id<255;++id)putW(0x67c8+id*2,0);for(const [id,n] of cfg.stocks)putW(0x67c8+id*2,n);
 for(let i=0;i<3;++i)putB(0x69f1+i*148,i<cfg.members?1:0);putB(0x7afa,cfg.members);putB(0x7ab0,cfg.members);
 for(let i=0;i<3;++i)putW(0x6a02+148*i,cfg.portraits[i]);
 for(let i=0;i<6;++i)putB(0x69d8+148+i,cfg.gear2[i]);
 for(let slot=0;slot<6;++slot)putB(0x69d8+slot,cfg.gear[slot]);
 putW(0x6a16,50);putW(0x6a14,200);putW(0x6a1a,100);putW(0x6a18,120);
 if(cfg.signedFields){
  putW(0x6a22+148,32768);putW(0x6a24+148,65535);
  for(const [j,v] of [65535,32768,32767].entries())putW(0x6a1c+148+2*j,v);
  for(const [j,v] of [65535,1,65534,2].entries())putW(0x6a04+148+2*j,v);
 }
 putB(0x7a72,cfg.cached);for(let i=0;i<142;++i)putB(0x739e+i,0xa5);
 const title=Buffer.from('bbbac4e6b5c0bedf','hex');for(let i=0;i<12;++i)putB(0x739e+i,title[i]??0);putB(0x73aa,0);
 const desc=Buffer.from(cfg.descriptionHex??'bec9cbb5c3f700','hex');for(let i=0;i<desc.length;++i)putB(0x73ab+i,desc[i]);
 for(let i=114;i<142;++i)putB(0x739e+i,0);putW(0x7410,7);putW(0x7412,3);putW(0x741e,20);putW(0x7414,9);putW(0x7416,11);putW(0x7418,2);putW(0x741a,4);putW(0x741c,6);putW(0x7420,5);putB(0x7428,cfg.type??29);putB(0x7429,cfg.flags??3);putB(0x742b,1);
 putW(0x7aba,77);putW(0x7abe,22);putW(0x7abc,37);
 const party=i=>{const d=i*148;return {hp:w(0x6a16+d),maxHp:w(0x6a14+d),qi:w(0x6a1a+d),maxQi:w(0x6a18+d),attack:w(0x6a22+d),defense:w(0x6a24+d),level:b(0x69de+d),experience:w(0x6a12+d),nextExperience:w(0x6a10+d),other:Array.from({length:3},(_,j)=>w(0x6a1c+d+j*2)),status:Array.from({length:4},(_,j)=>w(0x6a04+d+j*2))};};
 const state=()=>({portraits:Array.from({length:3},(_,i)=>w(0x6a02+148*i)),party2:party(1),gear2:Array.from({length:6},(_,i)=>b(0x69d8+148+i)),gear:Array.from({length:6},(_,i)=>b(0x69d8+i)),sex:b(0x69d7),mode:w(0x7934),event:w(0x7936),menu:b(0x7a54),ui:b(0x7a4a),selected:w(0x7a4c),row:b(0x7a20),column:b(0x7a22),page:b(0x7a24),length:w(0x7aba),start:w(0x7abe),end:w(0x7abc),loaded:b(0x7a72),bytes:Array.from({length:142},(_,i)=>b(0x739e+i)),stock:cfg.stocks.map(([id])=>[id,w(0x67c8+id*2)]),party:{hp:w(0x6a16),maxHp:w(0x6a14),qi:w(0x6a1a),maxQi:w(0x6a18),attack:w(0x6a22),defense:w(0x6a24),level:b(0x69de),experience:w(0x6a12),nextExperience:w(0x6a10),other:Array.from({length:3},(_,i)=>w(0x6a1c+i*2)),status:Array.from({length:4},(_,i)=>w(0x6a04+i*2))}});
 const seed=state(),initial=[],phases=[],frames=[];
 const collect=(t,commits)=>{for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(!p)continue;const s=state(),index=frames.length;
  fs.writeFileSync(`tmp/native-portrait-equip-${cfg.name}-${index}.idx`,p);const c={index,tick:t,state:s};frames.push(c);commits.push(c);}};
 for(const mask of [64,2,2,16,2])for(let t=0;t<16;++t)tick(t===0?mask:0);
 for(let t=0;t<32;++t){tick(t===0?16:0);collect(t,initial);}
 const before=state();if(before.mode!==4)throw Error('list entry failed '+JSON.stringify(before));
 for(const key of cfg.keys){const commits=[];let delivered=false;
  for(let t=0;t<(cfg.ticks??32);++t){if(key>=256)putW(0x7a76,t>=3&&!delivered?(key===256?0x85:0x84):0);
   tick(key<256&&t===0?key:0);collect(t,commits);if(commits.length)delivered=true;}
  phases.push({key,delivered,commits,after:state()});}
 if(state().mode!==1)throw Error('route did not return to map '+JSON.stringify({before,phases,after:state()}));
 const row={...cfg,seed,initial,before,phases,after:state()};
 const rows=JSON.parse(fs.readFileSync('tmp/native-portrait-equip.json')).filter(r=>r.name!==cfg.name);rows.push(row);
 fs.writeFileSync('tmp/native-portrait-equip.json',JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name:cfg.name,initial:initial.map(c=>c.state.mode),before:{selected:before.selected,loaded:before.loaded,length:before.length,start:before.start,end:before.end},phases:phases.map(p=>({key:p.key,frames:p.commits.map(c=>c.state.mode),mode:p.after.mode,selected:p.after.selected,loaded:p.after.loaded,length:p.after.length,start:p.after.start,end:p.after.end,hp:p.after.party.hp,attack:p.after.party.attack,stock:p.after.stock,gear:p.after.gear}))}));
}finally{rt.free();}
