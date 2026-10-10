import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';

const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rawKeys=[32,0x90,9,...'qQuUeEdDfF'.split('').map(c=>c.charCodeAt(0))];
const parent=[64,2,2,2,16],music=[...parent,2,2,16];
const cases=[
 {name:'title-cancel',title:true,keys:[2,16,2,2,2,1,1,1,64,2,1,16,1,64]},
 {name:'title-empty',title:true,keys:[2,16,1,16]},
 {name:'root',keys:[64,4,8,32,64]},
 {name:'submenu',keys:[...parent,4,8,32,64]},
 {name:'read-slots',keys:[...parent,16,4,8,32,64]},
 {name:'save-slots',keys:[...parent,2,16,2,1,4,8,64]},
 {name:'music-on',keys:[...music,1,4,8,32,16]},
 {name:'music-off',keys:[...music,16]},
 {name:'music-cancel',keys:[...music,1,64]},
];
const results=[];
fs.mkdirSync('tmp/system-ignored-original',{recursive:true});
for(const c of cases){
 const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
 try{
  const tick=mask=>rt.advance(mask).free();
  if(c.title){
   rt.installPersistentDataBytes(fs.readFileSync('tmp/native-title-load-persistent.bin'));
   rt.setBootTarget('title');rt.boot().free();for(let t=0;t<1000;++t)tick(0);
  }else rt.restoreBytes(fs.readFileSync('tmp/native-title-load-saved.snapshot'));
  const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
  for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===(c.title?7:1)&&mem()[i+0x7935]===0&&mem()[i+0x7aec]===0xa9&&mem()[i+0x7aeb]===6)found.push(i);
  if(found.length!==1)throw Error('live RAM '+c.name+' '+found.length);
  const base=found[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
  const putW=(o,v)=>{mem()[a(o)-1]=v>>>8;mem()[a(o)]=v&255;};
  const state=()=>({mode:w(0x7934),event:w(0x7936),init:w(0x7ab4),map:w(0x7a58),progress:w(0x7a66),
   slot:b(0x7a7a),selection:b(0x7a7c),stage:b(0x7a7e),menu:b(0x7a54),parent:b(0x7ae2),system:b(0x7a1e),music:b(0x7a8a),enabled:w(0x7a50)});
  const ignored=[],phases=[];let confirmations=0;
  for(let phase=0;phase<c.keys.length;++phase){
   if(state().mode===6||state().mode===7)for(const raw of rawKeys){
    const before=state(),pixels=Buffer.from(rt.framePixelIndices());let commits=0;
    for(let t=0;t<16;++t){putW(0x7a76,t>=3?raw:0);tick(0);commits+=rt.displayCommitCount();}
    putW(0x7a76,0);const after=state();
    if(commits||JSON.stringify(before)!==JSON.stringify(after)||!pixels.equals(Buffer.from(rt.framePixelIndices())))
     throw Error('original ignored key changed '+JSON.stringify({name:c.name,phase,raw,before,after,commits}));
    ignored.push({phase,raw,mode:before.mode,menu:before.menu,commits:0,statePreserved:true,pixelsPreserved:true});
   }
   const mask=c.keys[phase],frames=[];let delivered=false;
   for(let t=0;t<(c.title&&phase===c.keys.length-1&&c.name==='title-empty'?128:32);++t){
    if(mask===16)putW(0x7a76,t>=3&&!delivered?0x86:0);
    tick(mask!==16&&t===0?mask:0);
    for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
     const index=frames.length;fs.writeFileSync(`tmp/system-ignored-original/${c.name}-${phase}-${index}.idx`,pixels);
     frames.push({state:state(),index,tick:t});delivered=true;
    }
   }
   if(mask===16){if(!delivered)throw Error('raw confirmation not delivered '+c.name+' '+phase);++confirmations;}
   phases.push({mask,frames,after:state()});
  }
  results.push({...c,ignored,phases,rawConfirmations:confirmations});
  console.log(`${c.name}: ${ignored.length} original ignored inputs, ${confirmations} raw confirmations`);
 }finally{rt.free();}
}
fs.writeFileSync('tmp/system-ignored-original/report.json',JSON.stringify({passed:true,cases:results},null,2)+'\n');
