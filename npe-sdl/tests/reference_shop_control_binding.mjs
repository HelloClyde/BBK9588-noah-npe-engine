import fs from 'node:fs';
import {spawnSync} from 'node:child_process';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const cases=[
 {name:'buy-entry-sell-flag',cached:45,price:20,stocks:[[45,2]],afterFlag:1,keys:[0x86,0x80,0x86]},
 {name:'sell-entry-buy-flag',sell:true,cached:45,price:20,stocks:[[45,2],[46,2]],afterFlag:0,keys:[0x86,0x80,0x86]},
 {name:'buy-entry-nonboolean-flag',cached:45,price:20,stocks:[[45,2]],afterFlag:255,keys:[0x86,0x80,0x86]},
 {name:'buy-entry-kind-changed',cached:45,price:20,stocks:[[45,2]],afterKind:2,keys:[0x86,0x80,0x86]},
],prefix='native-shop-control-binding';
const selected=process.argv[2];
if(!selected){fs.writeFileSync(`tmp/${prefix}.json`,'[]\n');
 for(const t of cases){const p=spawnSync(process.execPath,[process.argv[1],t.name],{stdio:'inherit'});if(p.error)throw p.error;if(p.status)throw Error(t.name+' failed '+p.status);}process.exit(0);}
const cfg=cases.find(t=>t.name===selected);if(!cfg)throw Error('unknown test');
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>rt.advance(mask).free();
try{
 rt.restoreBytes(fs.readFileSync('tmp/native-exit-dialogue-1-after.snapshot'));rt.drainEventsBytes();
 const candidates=[];for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===4&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7938]===13&&mem()[i+0x7a58]===7)candidates.push(i);
 const selections=candidates.map(i=>mem()[i+0x7a4b]*256+mem()[i+0x7a4c]);
 if(candidates.length>1){for(let t=0;t<32;++t)tick(t===0?2:0);}
 const live=candidates.length===1?candidates:candidates.filter((i,j)=>mem()[i+0x7a4b]*256+mem()[i+0x7a4c]!==selections[j]);
 if(live.length!==1)throw Error('RAM discovery '+live.length);
 const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 const state=()=>({kind:b(0x7a0e),sellFlag:b(0x7ac2),mode:w(0x7934),context:w(0x7938),money:(w(0x799e)*65536+w(0x79a0))>>>0,selected:w(0x7a4c),first:w(0x7ac6),last:w(0x7ab6),row:b(0x7a46),ui:b(0x7a4a),quantity:b(0x7a48),choice:b(0x7a0c),length:w(0x7aba),start:w(0x7abe),end:w(0x7abc),loaded:b(0x7a72),inventoryPrefix:w(0x67c6),iconBase:w(0x7ae4),bytes:Array.from({length:142},(_,i)=>b(0x739e+i)),stock:cfg.stocks.map(([id])=>[id,w(0x67c8+id*2)])});
 putW(0x799e,0);putW(0x79a0,1000);for(let id=1;id<255;++id)putW(0x67c8+id*2,0);
 for(const [id,n] of cfg.stocks)putW(0x67c8+id*2,n);
 if(cfg.inventoryPrefix!==undefined)putW(0x67c6,cfg.inventoryPrefix);
 putW(0x7938,cfg.sell?10:13);putW(0x7ab4,1);putW(0x7a76,0);
 putB(0x7a72,cfg.cached);for(let i=0;i<142;++i)putB(0x739e+i,0xa5);
 const name=Buffer.from('bbbac4e6b5c0bedf','hex');for(let i=0;i<12;++i)putB(0x739e+i,name[i]??0);putB(0x73aa,0);
 const desc=Buffer.from(cfg.descriptionHex??'bec9cbb5c3f700','hex');for(let i=0;i<desc.length;++i)putB(0x73ab+i,desc[i]);putW(0x741e,cfg.price??20);
 const seed=state(),phases=[],initial=[],all=[];
 const collect=(t,list)=>{for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){const index=all.length;fs.writeFileSync(`tmp/${prefix}-${cfg.name}-${index}.idx`,p);const c={index,tick:t,state:state()};list.push(c);all.push(c);}}};
 for(let t=0;t<32;++t){tick(0);collect(t,initial);}
 if(cfg.afterFlag!==undefined)putB(0x7ac2,cfg.afterFlag);if(cfg.afterKind!==undefined)putB(0x7a0e,cfg.afterKind);
 const before=state();
 for(const key of cfg.keys){const forcedRow=cfg.beforeKeyRows?.[phases.length]??256;if(forcedRow<256)putB(0x7a46,forcedRow);
  const commits=[];let delivered=false;for(let t=0;t<32;++t){putW(0x7a76,t>=3&&!delivered?key:0);tick(0);collect(t,commits);if(commits.length)delivered=true;}
  phases.push({key,forcedRow,delivered,commits,after:state()});}
 const row={...cfg,seed,initial,before,phases,after:state()};
 const rows=JSON.parse(fs.readFileSync(`tmp/${prefix}.json`)).filter(r=>r.name!==cfg.name);rows.push(row);
 fs.writeFileSync(`tmp/${prefix}.json`,JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name:cfg.name,initial:initial.length,selected:before.selected,loaded:before.loaded,frames:all.length,phases:phases.map(p=>({key:p.key,frames:p.commits.length,loaded:p.after.loaded,selected:p.after.selected,first:p.after.first,last:p.after.last,money:p.after.money}))}));
}finally{rt.free();}
