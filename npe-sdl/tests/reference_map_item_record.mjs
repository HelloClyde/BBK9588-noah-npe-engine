import fs from 'node:fs';
import {spawnSync} from 'node:child_process';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';

const cases=[
 {name:'same-single-use',cached:45,stocks:[[45,2]],keys:[16,16,64,64]},
 {name:'fresh-single',cached:1,stocks:[[45,2]],keys:[64]},
 {name:'same-two',cached:45,stocks:[[45,2],[46,2]],keys:[8,4,4,64]},
 {name:'page-thirteen',cached:45,stocks:Array.from({length:13},(_,i)=>[45+i,1]),keys:[2,2,2,2,1,64]},
 {name:'long-description',cached:45,stocks:[[45,2]],descriptionHex:'41'+'d2bb'.repeat(49)+'42'+'00',keys:[256,256,512,512,64]},
 {name:'empty-retains',cached:45,stocks:[],keys:[16,4,64]},
];
const name=process.argv[2];
if(!name){fs.writeFileSync('tmp/native-map-item-record.json','[]\n');
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
 for(let i=0;i<3;++i)putB(0x69f1+i*148,i===0?1:0);putB(0x7afa,1);putB(0x7ab0,1);
 putW(0x6a16,50);putW(0x6a14,200);putW(0x6a1a,100);putW(0x6a18,120);
 putB(0x7a72,cfg.cached);for(let i=0;i<142;++i)putB(0x739e+i,0xa5);
 const title=Buffer.from('bbbac4e6b5c0bedf','hex');for(let i=0;i<12;++i)putB(0x739e+i,title[i]??0);putB(0x73aa,0);
 const desc=Buffer.from(cfg.descriptionHex??'bec9cbb5c3f700','hex');for(let i=0;i<desc.length;++i)putB(0x73ab+i,desc[i]);
 for(let i=114;i<142;++i)putB(0x739e+i,0);putW(0x7410,7);putW(0x7412,3);putW(0x741e,20);putB(0x7428,1);putB(0x742b,1);
 putW(0x7aba,77);putW(0x7abe,22);putW(0x7abc,37);
 const state=()=>({mode:w(0x7934),event:w(0x7936),menu:b(0x7a54),ui:b(0x7a4a),selected:w(0x7a4c),row:b(0x7a20),column:b(0x7a22),page:b(0x7a24),length:w(0x7aba),start:w(0x7abe),end:w(0x7abc),loaded:b(0x7a72),bytes:Array.from({length:142},(_,i)=>b(0x739e+i)),stock:cfg.stocks.map(([id])=>[id,w(0x67c8+id*2)]),party:{hp:w(0x6a16),maxHp:w(0x6a14),qi:w(0x6a1a),maxQi:w(0x6a18),attack:w(0x6a22),defense:w(0x6a24),level:b(0x69de),experience:w(0x6a12),nextExperience:w(0x6a10),other:Array.from({length:3},(_,i)=>w(0x6a1c+i*2)),status:Array.from({length:4},(_,i)=>w(0x6a04+i*2))}});
 const seed=state(),initial=[],phases=[],frames=[];
 const collect=(t,commits)=>{for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(!p)continue;const s=state(),index=frames.length;
  fs.writeFileSync(`tmp/native-map-item-record-${cfg.name}-${index}.idx`,p);const c={index,tick:t,state:s};frames.push(c);commits.push(c);}};
 for(const mask of [64,2,2,16])for(let t=0;t<16;++t)tick(t===0?mask:0);
 for(let t=0;t<32;++t){tick(t===0?16:0);collect(t,initial);}
 const before=state();if(before.mode!==4)throw Error('list entry failed '+JSON.stringify(before));
 for(const key of cfg.keys){const commits=[];let delivered=false;
  for(let t=0;t<32;++t){if(key>=256)putW(0x7a76,t>=3&&!delivered?(key===256?0x85:0x84):0);
   tick(key<256&&t===0?key:0);collect(t,commits);if(commits.length)delivered=true;}
  phases.push({key,delivered,commits,after:state()});}
 if(state().mode!==1)throw Error('route did not return to map');
 const row={...cfg,seed,initial,before,phases,after:state()};
 const rows=JSON.parse(fs.readFileSync('tmp/native-map-item-record.json')).filter(r=>r.name!==cfg.name);rows.push(row);
 fs.writeFileSync('tmp/native-map-item-record.json',JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name:cfg.name,initial:initial.map(c=>c.state.mode),before:{selected:before.selected,loaded:before.loaded,length:before.length,start:before.start,end:before.end},phases:phases.map(p=>({key:p.key,frames:p.commits.map(c=>c.state.mode),mode:p.after.mode,selected:p.after.selected,loaded:p.after.loaded,length:p.after.length,start:p.after.start,end:p.after.end,hp:p.after.party.hp}))}));
}finally{rt.free();}
