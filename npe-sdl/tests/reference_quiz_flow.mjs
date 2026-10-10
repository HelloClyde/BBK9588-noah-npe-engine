import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
// Offline original-engine reference. Only diagnostic RAM is changed.
async function main(){
 const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
 const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
 const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>{const o=rt.advance(mask);o.free();};
 const key=mask=>{for(let i=0;i<16;++i)tick(i?0:mask);};
 const rows=[];
 const tests=[
  {name:'fixed-counter-cap',event:13,count:7,fixedSeed:65535,keys:[0x83,0x86]},
  {name:'category-counter-cap',event:0,correctSeed:65535,keys:['correct']},
  {name:'category-wrong-cap',event:0,count:5,wrong:2,wrongSeed:65535,keys:['wrong']},
  {name:'choice-wrong',event:13,keys:[0x81,0x83,0x80,0x82,0x8e,0x86]},
  {name:'choice-correct',event:13,count:7,keys:[0x82,0x80,0x83,0x83,0x86]},
  {name:'choice-letters',event:14,keys:[0x61,0x64,0x69,0x6c,0x6d,0x6a,0x6b]},
  {name:'choice-long',event:410,keys:[0x81,0x86]},
  {name:'fill-correct',event:1035,count:7,keys:[0x65,0x77,0x86]},
  {name:'fill-case',event:1035,keys:[0x45,0x77,0x86]},
  {name:'fill-edit',event:2000,keys:[0x89,0x6f,0x6e,0x78,0x89,0x65,0x8e,0x86]},
  {name:'fill-space',event:2000,keys:[0x6f,0x6e,0x65,0x20,0x86]},
  {name:'fill-limit',event:1035,keys:[...Array(34).fill(0x61),0x2c,0x2e,0x89,0x86]},
  {name:'category-correct',event:0,keys:['correct']},
  {name:'category-third-wrong',event:0,count:5,wrong:2,keys:['wrong']},
  {name:'category-fill-correct',event:1,keys:['correct']},
  {name:'category-fill-third-wrong',event:1,count:5,wrong:2,keys:['wrong']},
  {name:'category-timeout',event:0,keys:['timeout']},
  {name:'category-exit',event:0,count:1,wrong:3,keys:[0x8e]},
  {name:'category-fill-exit',event:1,count:5,wrong:1,keys:[0x8e]},
 ];
 for(const test of tests){
  rt.restoreBytes(fs.readFileSync('tmp/native-title-load-saved.snapshot'));
  const initial=mem(),candidates=[];
  for(let i=0;i+0x8000<initial.length;++i)if(initial[i+0x7934]===1&&initial[i+0x7935]===0&&initial[i+0x7a58]===1&&initial[i+0x7a66]===1&&initial[i+0x7aec]===0xa9&&initial[i+0x7aeb]===6)candidates.push(i);
  key(64);const live=candidates.filter(i=>mem()[i+0x7934]===6);if(live.length!==1)throw Error('Quiz RAM '+live.length);
  const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
  const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>8);mem()[a(o)]=v&255;};
  const string=o=>{const out=[];for(let i=0;i<33&&b(o+i);++i)out.push(b(o+i));return out;};
  const state=()=>({mode:w(0x7934),event:w(0x7936),context:w(0x7938),category:w(0x7a52),length:b(0x7ad8),type:b(0x7ae0),correct:b(0x7a96),previous:w(0x7ade),count:w(0x7ad0),wrong:b(0x7ada),timer:b(0x7a32),key:w(0x7a76),background:b(0x7ace),money:(w(0x799e)*65536+w(0x79a0))>>>0,answer:string(0x77dc),canonical:string(0x77bc),stats:Array.from({length:22},(_,i)=>w(0x7684+i*2))});
  key(64);for(let i=0;i<22;++i)putW(0x7684+2*i,100+7*i);
  putW(0x7934,8);putW(0x7936,test.event);putW(0x7ab4,1);putW(0x7ad0,test.count||0);putB(0x7ada,test.wrong||0);putW(0x7684,test.correctSeed||0);putW(0x7686,test.wrongSeed||0);putW(0x76ac,test.fixedSeed||0);rt.drainEventsBytes();
  const stages=[],run=(label,code,advances=code===undefined?32:(test.event<10?240:32))=>{
   const commits=[];let mapCommitted=false;
   for(let t=0;t<advances;++t){if(code!==undefined)putW(0x7a76,t===3?code:0);tick(0);
    for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){fs.writeFileSync(`tmp/native-quiz-${test.name}-${stages.length}-${commits.length}.idx`,p);commits.push({tick:t,state:state()});if(state().mode===1&&p[0]===0)mapCommitted=true;}}
    // End at the first map initialization commit. Subsequent NPC periods
    // belong to the map controller, not this service reference.
    if(mapCommitted)break;
   }
   stages.push({label,code,commits,after:state()});
  };
  run('entry');for(const code of test.keys){
   if(code==='timeout'){putB(0x7a32,8);run('timeout',undefined,240);}
   else if(code==='correct'||code==='wrong'){
    const q=state();if(q.type){if(code==='correct')for(const c of q.canonical)run('typed',c);}
    else {const c=code==='correct'?q.correct:(q.correct+1)%4;if(c&1)run('right',0x81);if(c&2)run('down',0x83);}
    run('submit',0x86);
   }else run('key',code);
  }
  rows.push({...test,stages});fs.writeFileSync('tmp/native-quiz-flow.json',JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({name:test.name,stages:stages.map(s=>({code:s.code,commits:s.commits.length,mode:s.after.mode,context:s.after.context,count:s.after.count,wrong:s.after.wrong,length:s.after.length,answer:s.after.answer,money:s.after.money,stats:s.after.stats}))}));
 }
 rt.free();
}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
