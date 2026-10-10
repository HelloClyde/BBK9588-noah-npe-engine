import fs from 'node:fs';
import {spawnSync} from 'node:child_process';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const down=n=>Array(n).fill(0x83),up=n=>Array(n).fill(0x82);
const tests=[
 {name:'buy-stale',cached:45,price:20,stocks:[[45,0]],keys:[0x86,0x80,0x86]},
 {name:'buy-fresh',cached:1,stocks:[[45,0]],keys:[0x83,0x86,0x86,0x82,0x86,0x86]},
 {name:'sell-price-zero',sell:true,cached:45,price:0,stocks:[[45,1]],keys:[]},
 {name:'sell-rejected-first',sell:true,cached:45,price:0,stocks:[[45,1],[46,2]],keys:[0x82,0x86,0x86]},
 {name:'sell-window',sell:true,cached:45,price:17,stocks:[45,46,47,48,49,53,54,55,56,57].map(id=>[id,2]),keys:[...down(7),...up(8)]},
 {name:'sell-remove',sell:true,cached:1,stocks:[[45,1],[46,2],[47,1]],keys:[0x86,0x80,0x86,0x83,0x86,0x80,0x86]},
];
const selectionTests=[
 {name:'zero-resource-only',sell:true,cached:146,price:20,stocks:[[146,1]],keys:[0x86,0x80,0x86]},
 {name:'zero-resource-later',sell:true,cached:146,price:20,inventoryPrefix:321,stocks:[[146,2],[148,2]],keys:[0x86,0x80,0x86,0x83,0x82,0x86,0x86]},
 {name:'zero-resource-before',sell:true,cached:146,price:20,stocks:[[145,1],[146,1]],keys:[0x83,0x86,0x86]},
 {name:'zero-resource-window',sell:true,cached:146,price:20,stocks:[146,148,149,151,152,159,160,161,162].map(id=>[id,2]),keys:[...down(7),...up(8)]},
 {name:'zero-resource-paging',sell:true,cached:146,price:20,stocks:[[146,1]],descriptionHex:'d2bb'.repeat(50)+'00',keys:[0x86,0x85,0x84,0x80,0x86]},
 {name:'zero-resource-negative-cache',sell:true,cached:146,price:65535,stocks:[[146,1]],keys:[]},
 {name:'zero-resource-row-underflow',sell:true,cached:146,price:20,inventoryPrefix:321,stocks:[[146,2],[148,2]],beforeKeyRows:[null,0],keys:[0x83,0x82]},
 {name:'zero-resource-partial-cursor',sell:true,cached:146,price:20,inventoryPrefix:321,stocks:[[146,2],[148,2]],beforeKeyRows:[7,null],keys:[0x83,0x82]},
];
const selectionMatrix=process.argv[2]==='--selection';
const cases=selectionMatrix?selectionTests:tests,prefix=selectionMatrix?'native-shop-selection':'native-shop-item-cache';
const selected=process.argv[selectionMatrix?3:2];
if(!selected){fs.writeFileSync(`tmp/${prefix}.json`,'[]\n');
 for(const t of cases){const args=[process.argv[1],...(selectionMatrix?['--selection']:[]),t.name];const p=spawnSync(process.execPath,args,{stdio:'inherit'});if(p.error)throw p.error;if(p.status)throw Error(t.name+' failed '+p.status);}process.exit(0);}
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
 const state=()=>({mode:w(0x7934),context:w(0x7938),money:(w(0x799e)*65536+w(0x79a0))>>>0,selected:w(0x7a4c),first:w(0x7ac6),last:w(0x7ab6),row:b(0x7a46),ui:b(0x7a4a),quantity:b(0x7a48),choice:b(0x7a0c),length:w(0x7aba),start:w(0x7abe),end:w(0x7abc),loaded:b(0x7a72),inventoryPrefix:w(0x67c6),iconBase:w(0x7ae4),bytes:Array.from({length:142},(_,i)=>b(0x739e+i)),stock:cfg.stocks.map(([id])=>[id,w(0x67c8+id*2)])});
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
 const before=state();
 for(const key of cfg.keys){const forcedRow=cfg.beforeKeyRows?.[phases.length]??256;if(forcedRow<256)putB(0x7a46,forcedRow);
  const commits=[];let delivered=false;for(let t=0;t<32;++t){putW(0x7a76,t>=3&&!delivered?key:0);tick(0);collect(t,commits);if(commits.length)delivered=true;}
  phases.push({key,forcedRow,delivered,commits,after:state()});}
 const row={...cfg,seed,initial,before,phases,after:state()};
 const rows=JSON.parse(fs.readFileSync(`tmp/${prefix}.json`)).filter(r=>r.name!==cfg.name);rows.push(row);
 fs.writeFileSync(`tmp/${prefix}.json`,JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name:cfg.name,initial:initial.length,selected:before.selected,loaded:before.loaded,frames:all.length,phases:phases.map(p=>({key:p.key,frames:p.commits.length,loaded:p.after.loaded,selected:p.after.selected,first:p.after.first,last:p.after.last,money:p.after.money}))}));
}finally{rt.free();}
