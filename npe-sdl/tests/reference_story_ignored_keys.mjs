import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';

// Diagnostic text controls; the original dispatcher, reader and NPE bytes
// remain unchanged. Raw RAM key words avoid the host action-mask mapping.
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const bytes=fs.readFileSync('tmp/tjfy-world-probe.npe');
const section=bytes.readUInt32BE(0x3a+10*8);
const pointer=section+bytes.readUInt32BE(section+4*3)+7;
const rows=[];
fs.mkdirSync('tmp/story-ignored-original',{recursive:true});
for(const raw of [0x86,0x6b])for(const more of [0,1,2,255]){
 const name=`key${raw.toString(16)}-more${more}`;
 const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
 try{
  const tick=mask=>rt.advance(mask).free();
  rt.restoreBytes(fs.readFileSync('tmp/battle-fresh-start.bin'));rt.drainEventsBytes();
  const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
  for(let i=0;i+0x8000<mem().length;++i)
   if(mem()[i+0x7934]===3&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&
      mem()[i+0x7a58]===9&&mem()[i+0x7a66]===6)found.push(i);
  if(found.length!==1)throw Error('live RAM '+name+' '+found.length);
  const base=found[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
  const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
  const state=()=>({mode:w(0x7934),event:w(0x7936),pointer:w(0x797a)*65536+w(0x797c),
   offset:w(0x7a1a),more:b(0x7a1c),style:b(0x7ace),rect:[0x791e,0x7920,0x7922,0x7924].map(w)});
  putW(0x7934,5);putW(0x7936,4);putW(0x7ab4,1);
  for(let t=0;t<35;++t)tick(0);
  if(w(0x7934)!==5||b(0x7a1c)!==0)throw Error('story entry '+name);
  const initial=Buffer.from(rt.framePixelIndices());
  if(!initial.equals(fs.readFileSync('tmp/native-story-4-page-0.idx')))throw Error('initial oracle '+name);
  putW(0x797a,pointer>>>16);putW(0x797c,pointer&65535);
  putW(0x7a1a,1);putB(0x7a1c,more);
  const before=state();let ignoredCommits=0;
  for(let t=0;t<16;++t){putW(0x7a76,t>=3?raw:0);tick(0);ignoredCommits+=rt.displayCommitCount();}
  putW(0x7a76,0);
  if(ignoredCommits||JSON.stringify(before)!==JSON.stringify(state())||
     !initial.equals(Buffer.from(rt.framePixelIndices())))throw Error('ignored key changed '+name);
  const commits=[];let delivered=false;
  for(let t=0;t<32;++t){
   putW(0x7a76,t>=3&&!delivered?0x90:0);tick(0);
   for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
    fs.writeFileSync(`tmp/story-ignored-original/${name}-${commits.length}.idx`,pixels);
    commits.push({tick:t,...state()});delivered=true;
   }
  }
  putW(0x7a76,0);const after=state();
  if(!delivered)throw Error('raw Space not delivered '+name);
  if(more===1){if(commits.length!==1||after.mode!==5||after.more!==0||after.offset!==120)
   throw Error('continuation '+JSON.stringify({name,commits,after}));
  }else if(after.mode!==1||after.event!==1||after.pointer!==before.pointer||
           after.offset!==before.offset||after.more!==before.more)throw Error('close '+name);
  rows.push({name,raw,more,before,after,ignoredCommits,rawSpaceDelivered:delivered,commits,closes:more!==1});
  console.log(`${name}: raw unsupported key retained text/LCD, raw Space ${more===1?'read':'closed'}`);
 }finally{rt.free();}
}
fs.writeFileSync('tmp/story-ignored-original/report.json',JSON.stringify({passed:true,cases:rows},null,2)+'\n');
