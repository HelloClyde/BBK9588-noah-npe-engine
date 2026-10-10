import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const prefix=process.argv[2]??'native-shop-text-settled';
const ascii=Array.from({length:100},(_,i)=>65+i%26),odd=[...ascii];odd[81]=0xb5;odd[82]=0xc0;
const cases=[
 {name:'ascii-pages',description:ascii,keys:[{key:0x86},{key:0x85},{key:0x84},{key:0x8e}]},
 {name:'odd-forward',description:odd,keys:[{key:0x86},{key:0x85},{key:0x84},{key:0x8e}]},
 {name:'right-word-wrap',description:ascii,keys:[{key:0x86},{key:0x85,patch:{end:65535}},{key:0x8e}]},
 {name:'right-past-string',description:ascii,keys:[{key:0x86},{key:0x85,patch:{length:65535,end:150}},{key:0x8e}]},
 {name:'left-signed-stop',description:ascii,keys:[{key:0x86},{key:0x84,patch:{start:32768}},{key:0x8e}]},
 {name:'left-past-string',description:ascii,keys:[{key:0x86},{key:0x84,patch:{start:101}},{key:0x8e}]},
 {name:'quantity-retains-text',description:ascii,keys:[{key:0x86},{key:0x82,patch:{length:0,start:82,end:99}},{key:0x8e}]},
 {name:'choice255',description:ascii,keys:[{key:0x86},{key:0x80,patch:{choice:255}},{key:0x8e}]},
];
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>rt.advance(mask).free(),rows=[];
try{for(const cfg of cases){
 rt.restoreBytes(fs.readFileSync('tmp/native-exit-dialogue-1-after.snapshot'));rt.drainEventsBytes();
 const candidates=[];for(let i=0;i+0x8000<mem().length;++i)
  if(mem()[i+0x7934]===4&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7938]===13&&mem()[i+0x7a58]===7)candidates.push(i);
 const selections=candidates.map(i=>mem()[i+0x7a4b]*256+mem()[i+0x7a4c]);
 if(candidates.length>1)for(let t=0;t<128;++t)tick(t===0?2:0);
 const found=candidates.length===1?candidates:candidates.filter((i,j)=>mem()[i+0x7a4b]*256+mem()[i+0x7a4c]!==selections[j]);
 if(found.length!==1)throw Error('RAM '+found.length);
 const base=found[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v&255;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 const state=()=>({mode:w(0x7934),event:w(0x7936),context:w(0x7938),ui:b(0x7a4a),selected:w(0x7a4c),
  quantity:b(0x7a48),choice:b(0x7a0c),length:w(0x7aba),start:w(0x7abe),end:w(0x7abc),loaded:b(0x7a72),
  bytes:Array.from({length:142},(_,i)=>b(0x739e+i)),sell:b(0x7ac2),first:w(0x7ac6),last:w(0x7ab6),row:b(0x7a46)});
 putW(0x799e,0);putW(0x79a0,1000);for(let id=1;id<255;++id)putW(0x67c8+id*2,0);
 putW(0x7938,13);putW(0x7ab4,1);putW(0x7a76,0);putW(0x7a50,0);
 for(let t=0;t<128;++t)tick(0);
 if(w(0x7a4c)!==45)throw Error('wrong selected item');
 putB(0x7a72,45);for(let i=0;i<142;++i)putB(0x739e+i,0);
 const name=Buffer.from('b5c0bedf','hex');for(let i=0;i<name.length;++i)putB(0x739e+i,name[i]);
 for(let i=0;i<cfg.description.length;++i)putB(0x73ab+i,cfg.description[i]);putW(0x741e,20);
 const before=state(),phases=[];let total=0;
 fs.writeFileSync(`tmp/${prefix}-${cfg.name}-before.idx`,rt.framePixelIndices());
 for(const phase of cfg.keys){
  for(const [name,v] of Object.entries(phase.patch??{})){
   const address={length:0x7aba,start:0x7abe,end:0x7abc,choice:0x7a0c}[name];
   if(name==='choice')putB(address,v);else putW(address,v);
  }
  const prior=state(),commits=[];
  for(let t=0;t<128;++t){putW(0x7a76,t===16?phase.key:0);tick(0);
   for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(pixels){
    fs.writeFileSync(`tmp/${prefix}-${cfg.name}-${total}.idx`,pixels);commits.push({index:total++,state:state()});
   }}
  }
  phases.push({...phase,before:prior,commits,after:state()});
 }
 rows.push({...cfg,before,phases,after:state(),commits:total});
 fs.writeFileSync(`tmp/${prefix}-service.json`,JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name:cfg.name,commits:total,phases:phases.map(p=>({key:p.key,commits:p.commits.length,
  length:p.after.length,start:p.after.start,end:p.after.end,quantity:p.after.quantity,choice:p.after.choice}))}));
 putW(0x7934,0);
}}finally{rt.free();}
