import fs from 'node:fs';
import {spawnSync} from 'node:child_process';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const cases=[
 {name:'cancel-same',cached:46,items:[46],keys:[64]},
 {name:'same-party-target',cached:46,items:[46],keys:[16,2,64,64]},
 {name:'same-group-queue',cached:46,group:1,items:[46],keys:[16]},
 {name:'fresh',cached:1,items:[46],keys:[64]},
 {name:'two-items',cached:46,items:[46,55],keys:[8,4,64]},
 {name:'wrong-side',cached:46,side:0,items:[46],keys:[16,64],ticks:240},
 {name:'throw-target',cached:159,side:0,type:31,throwing:true,items:[159],keys:[16,2,64,64]},
 {name:'empty-cancel',cached:46,items:[],keys:[64]},
 {name:'empty-target-cancel',cached:255,items:[],keys:[16,64,64]},
 {name:'multi-target-cancel',cached:46,items:[46,55],keys:[16,2,64,64]},
 {name:'page-return',cached:46,items:[46,47,48,49,50,51,52,53,54,55,56,57,58],keys:[2,2,2,2,1,64]},
];
const typeWords=process.argv.includes('--type-words');
const selectorTarget=process.argv.includes('--selector-target')||typeWords;
const resources=process.argv.includes('--resources')||selectorTarget;
if(resources)cases.push(
 {name:'repeat-party-target',cached:46,items:[46],keys:[16,2,64,16,1,64,64]},
 {name:'party-confirm',cached:46,items:[46],keys:[16,2,16]},
 {name:'throw-confirm',cached:159,side:0,type:31,throwing:true,items:[159],keys:[16,2,16]},
);
if(typeWords)cases.push({name:'seeded-party-target',cached:46,items:[46],seedTypes:true,keys:[16,2,64,64]});
if(typeWords)cases.push({name:'seeded-next-actor',cached:46,items:[46],group:1,nextTypes:[0x91,0x83,0xad],seedTypes:true,keys:[16]});
const name=process.argv.slice(2).find(v=>!v.startsWith('--'));
const prefix=typeWords?'tmp/native-battle-item-type-words':selectorTarget?'tmp/native-battle-item-selector-target':resources?'tmp/native-battle-item-resources':'tmp/native-battle-item-record',output=prefix+'.json';
if(!name){fs.writeFileSync(output,'[]\n');for(const c of cases){const p=spawnSync(process.execPath,[process.argv[1],c.name,...(typeWords?['--type-words']:selectorTarget?['--selector-target']:resources?['--resources']:[])],{stdio:'inherit'});if(p.error)throw p.error;if(p.status)throw Error(c.name+' failed '+p.status);}process.exit(0);}
const c=cases.find(c=>c.name===name);if(!c)throw Error('unknown case');
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>rt.advance(mask).free();
const key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
try{
 rt.restoreBytes(fs.readFileSync('tmp/battle-fresh-start.bin'));rt.drainEventsBytes();
 const found=[];for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===3&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7a58]===9&&mem()[i+0x7a66]===6)found.push(i);
 if(found.length!==1)throw Error('RAM discovery');
 const base=found[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 const hero=mem().slice(a(0x69c6)-1,a(0x69c6+148)-1);
 for(let i=0;i<3;++i)mem().set(hero,a(0x69c6+i*148)-1);
 putB(0x7afa,3);putB(0x7626,51);putB(0x7627,51);putW(0x7ab4,1);
 for(let t=0;t<30;++t)tick(0);
 for(let i=0;i<6;++i){const d=i*148;
  putW(0x649e+d,200);putW(0x64a0+d,50);putW(0x64a2+d,120);putW(0x64a4+d,100);
  putB(0x6468+d,99);for(let s=0;s<4;++s)putW(0x648e+d+2*s,0);
 }
 for(let id=0;id<255;++id)putW(0x67c8+id*2,0);for(const id of c.items)putW(0x67c8+id*2,1);
 putB(0x7a9e,0);putB(0x7a72,c.cached);for(let i=0;i<142;++i)putB(0x739e+i,0xa5);
 const title=Buffer.from('bbbac4e6b5c0bedf','hex');for(let i=0;i<12;++i)putB(0x739e+i,title[i]??0);putB(0x73aa,0);
 const desc=Buffer.from('bec9cbb5c3f700','hex');for(let i=0;i<desc.length;++i)putB(0x73ab+i,desc[i]);
 for(let i=114;i<142;++i)putB(0x739e+i,0);
 putB(0x7428,c.type??1);putB(0x742a,c.group??0);putB(0x742b,c.side??1);
 putW(0x7410,7);putW(0x7412,3);
 putW(0x7aba,77);putW(0x7abe,22);putW(0x7abc,37);
 if(resources)putW(0x7aa4,0x01a6);
 if(c.seedTypes)for(const [i,address] of [0x7aa0,0x7aa6,0x7aaa].entries())putW(address,[0x81a6,0xcbf0,0x153a][i]);
 if(c.nextTypes)for(const [i,address] of [0x64d1,0x64d0,0x64d2].entries())putB(address+148,c.nextTypes[i]);
 const state=()=>({mode:w(0x7934),menu:b(0x7ac8),actor:b(0x7a9e),page:b(0x7a18),member:b(0x7adc),loaded:b(0x7a72),
  ...(resources?{resourceFlags:w(0x7aa4)}:{}),
  ...(selectorTarget?{selectorTarget:b(0x7ac0)}:{}),
  ...(typeWords?{typeWords:[w(0x7aa0),w(0x7aa6),w(0x7aaa)]}:{}),
  length:w(0x7aba),start:w(0x7abe),end:w(0x7abc),bytes:Array.from({length:142},(_,i)=>b(0x739e+i)),
  selected:w(0x7a4c),row:b(0x7a20),column:b(0x7a22),stock:c.items.map(id=>[id,w(0x67c8+2*id)]),alias:w(0x69c6),
  commands:Array.from({length:3},(_,i)=>({kind:b(0x64cf+148*i),target:b(0x64ce+148*i),id:b(0x64d5+148*i),type:b(0x64d1+148*i),group:b(0x64d3+148*i),side:b(0x64d4+148*i)})),
  parties:Array.from({length:3},(_,i)=>{const d=i*148;return {hp:w(0x64a0+d),maximum:w(0x649e+d),qi:w(0x64a4+d),maxQi:w(0x64a2+d),level:b(0x6468+d)};})});
 const seed=state();for(const mask of [4,2,16])key(mask);for(let n=0;b(0x7a9c)!==1&&n<6;++n)key(2);key(16);if(c.throwing)key(2);key(16);
 const before=state();if(before.menu!==33)throw Error('list entry '+JSON.stringify(before));
 fs.writeFileSync(`${prefix}-${name}-before.idx`,rt.framePixelIndices());
 const phases=[];let index=0;
 for(const mask of c.keys){const commits=[];for(let t=0;t<(c.ticks??32);++t){tick(t===0?mask:0);
  for(let j=0;j<rt.displayCommitCount();++j){const pix=rt.displayCommitPixelIndices(j);if(pix){fs.writeFileSync(`${prefix}-${name}-${index}.idx`,pix);commits.push({index:index++,tick:t,state:state()});}}}
  phases.push({key:mask,commits,after:state()});
 }
 const rows=JSON.parse(fs.readFileSync(output)).filter(r=>r.name!==name);rows.push({...c,seed,before,phases,after:state()});
 fs.writeFileSync(output,JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name,before:{menu:before.menu,page:before.page,loaded:before.loaded,length:before.length},phases:phases.map(p=>({key:p.key,frames:p.commits.map(c=>[c.state.menu,c.state.actor,c.state.loaded]),menu:p.after.menu,actor:p.after.actor,loaded:p.after.loaded,command:p.after.commands[0]}))}));
}finally{rt.free();}
