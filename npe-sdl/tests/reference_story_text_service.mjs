import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
// Continue a real, unchanged original mode5 reader after patching resident
// text controls. Resource bytes, formatter, allocator and reader are untouched.
const cases=[
 {name:'window-pointer',id:1,offset:1,style:0,rect:[0,84,240,118]},
 {name:'full-pointer',id:63,offset:1,style:1,rect:[0,12,240,131]},
 {name:'style2-retained',id:1,offset:1,style:2,rect:[0,12,240,131]},
 {name:'style255-retained',id:3,offset:1,style:255,rect:[8,10,168,115]},
 {name:'window-rect',id:4,offset:1,style:0,rect:[8,30,168,100]},
 {name:'full-rect',id:4,offset:1,style:1,rect:[16,8,208,119]},
 {name:'empty-first-block',id:1,offset:0,style:0,rect:[0,84,240,118]},
 {name:'three-block-boundary',id:23,offset:1,style:1,rect:[0,12,240,131]},
 {name:'offset-word-wrap',id:4,terminal:true,offset:65535,style:0,rect:[0,84,240,118]},
 {name:'window-lcd-clip',id:4,offset:1,style:0,rect:[0,96,240,131]},
 {name:'more2-closes',id:4,offset:1,style:0,more:2,rect:[0,84,240,118]},
];
const bytes=fs.readFileSync('tmp/tjfy-world-probe.npe'),section=bytes.readUInt32BE(0x3a+10*8);
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>rt.advance(mask).free();
const selected=process.argv[2];
const rows=selected?JSON.parse(fs.readFileSync("tmp/native-story-text-service.json")).filter(r=>r.name!==selected):[];
try{for(const cfg of cases){
 if(selected&&cfg.name!==selected)continue;
 rt.restoreBytes(fs.readFileSync('tmp/battle-fresh-start.bin'));rt.drainEventsBytes();
 const found=[];for(let i=0;i+0x8000<mem().length;++i)
  if(mem()[i+0x7934]===3&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7a58]===9&&mem()[i+0x7a66]===6)found.push(i);
 if(found.length!==1)throw Error('RAM '+found.length);
 const base=found[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 const state=()=>({mode:w(0x7934),event:w(0x7936),pointer:w(0x797a)*65536+w(0x797c),
  offset:w(0x7a1a),more:b(0x7a1c),style:b(0x7ace),rect:[0x791e,0x7920,0x7922,0x7924].map(w)});
 putW(0x7934,5);putW(0x7936,4);putW(0x7ab4,1);
 for(let t=0;t<35;++t)tick(0);
 if(w(0x7934)!==5||b(0x7a1c)!==0)throw Error('entry');
 fs.writeFileSync(`tmp/native-story-text-${cfg.name}-before.idx`,rt.framePixelIndices());
 let pointer=section+bytes.readUInt32BE(section+4*(cfg.id-1))+7;
 if(cfg.terminal)pointer=bytes.indexOf(0x40,pointer)-cfg.offset;
 putW(0x797a,pointer>>>16);putW(0x797c,pointer&65535);
 putW(0x7a1a,cfg.offset);putB(0x7a1c,cfg.more??1);putB(0x7ace,cfg.style);
 for(let i=0;i<4;++i)putW(0x791e+2*i,cfg.rect[i]);
 const before=state(),commits=[];
 for(let t=0;t<16;++t){tick(t===0?16:0);
  for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){
   fs.writeFileSync(`tmp/native-story-text-${cfg.name}-${commits.length}.idx`,p);
   commits.push({tick:t,...state()});
  }}
 }
 if(cfg.more===2){if(w(0x7934)===5)throw Error('more2 did not close');}
 else if(commits.length!==1||w(0x7934)!==5)throw Error('reader '+cfg.name+' '+commits.length+' '+w(0x7934));
 rows.push({...cfg,before,commits,after:state()});
 fs.writeFileSync('tmp/native-story-text-service.json',JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name:cfg.name,commits:commits.length,after:state()}));
 putW(0x7934,0);
}}finally{rt.free();}
