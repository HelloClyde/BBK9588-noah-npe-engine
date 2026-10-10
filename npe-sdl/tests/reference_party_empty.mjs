import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
async function main(){
 const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
 const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
 const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot'),mem=()=>new Uint8Array(exp.memory.buffer);
 const stateAt=(base,m=mem())=>{const a=o=>base+o-(o<0x7000?8:0),b=o=>m[a(o)-1],w=o=>b(o)*256+m[a(o)];return{
  mode:w(0x7934),event:w(0x7936),context:w(0x7938),map:w(0x7a58),progress:w(0x7a66),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),money:(w(0x799e)*65536+w(0x79a0))>>>0,enabled:w(0x7a50),slot:b(0x7a04),periodCounter:b(0x7a32),menu:b(0x7a54),propertyChoice:b(0x7a18),member:b(0x7adc),equipmentSlot:b(0x7a2a),ui:b(0x7a4a),partyCount:b(0x7afa),menuPartyCount:b(0x7ab0),actors:Array.from({length:3},(_,i)=>{const o=148*i;return{id:b(0x69d2+o),active:b(0x69f1+o),portrait:w(0x6a02+o),level:b(0x69de+o),maxHp:w(0x6a14+o),hp:w(0x6a16+o),maxQi:w(0x6a18+o),qi:w(0x6a1a+o),attack:w(0x6a22+o),defense:w(0x6a24+o),experience:w(0x6a12+o),experienceToNext:w(0x6a10+o),otherStats:Array.from({length:3},(_,s)=>w(0x6a1c+o+s*2)),gear:Array.from({length:6},(_,s)=>b(0x69d8+o+s)),guards:Array.from({length:4},(_,s)=>w(0x6a04+o+s*2)),skills:Array.from({length:18},(_,s)=>b(0x69df+o+s)),raw:Array.from({length:148},(_,s)=>b(0x69c6+o+s))};}),gearSelected:b(0x7ab2),descLength:w(0x7aba),descStart:w(0x7abe),descEnd:w(0x7abc),
  npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};})};};

 const tick=mask=>{const out=rt.advance(mask);out.free();};
 const events=JSON.parse(fs.readFileSync('tmp/real-party-events-index.json'));
 const tests=[{name:'cold-unused-event3',order:[0,1,2],flags:[1,0,0],kind:'leave2'}];
 const rows=[];
 for(const test of tests){
  rt.restoreBytes(snapshot);rt.drainEventsBytes();let m=mem(),candidates=[];
  for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===1&&m[i+0x7935]===0&&m[i+0x7a58]===1&&m[i+0x7a66]===1&&m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6)candidates.push(i);
  for(let t=0;t<16;++t)tick(t===0?64:0);
  const live=candidates.filter(i=>stateAt(i).mode===6);if(live.length!==1)throw Error('party base '+live.length);
  const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=(o,v)=>{mem()[a(o)-1]=v;},w=(o,v)=>{b(o,v>>8);mem()[a(o)]=v&255;};
  for(let t=0;t<16;++t)tick(t===0?64:0);
  // Resolve live RAM via F4, then restore to the original saved map LCD.
  const baseline=rt.snapshotBytes(),pixels=rt.framePixelIndices();
  const original=stateAt(base).actors.map(v=>v.raw);
  for(let i=0;i<3;++i){for(let j=0;j<148;++j)b(0x69c6+148*i+j,original[test.order[i]][j]);b(0x69f1+148*i,test.flags[i]);}
  const character=Number(test.kind.slice(-1)),add=test.kind.startsWith('add')?1:0;
  const event=events.find(e=>e.character===character&&e.add===add&&e.nextMode===1);
  if(!event)throw Error('original event');
  // Supply an unchanged current-NPE group-6 record to the original cached service.
  b(0x78a2,event.add);b(0x78a3,event.character);b(0x78a4,event.nextMode);w(0x78a6,event.nextId);
  w(0x7934,10);w(0x7936,3);
  const before=stateAt(base),commits=[];fs.writeFileSync(`tmp/native-party-service-${test.name}-before.idx`,pixels);
  let finish=-1;
  for(let t=0;t<250;++t){tick(t%13===3?16:t%13===7?64:0);
   for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){const index=commits.length;fs.writeFileSync(`tmp/native-party-service-${test.name}-${index}.idx`,p);commits.push({tick:t,state:stateAt(base)});}}
   if([0,1].includes(stateAt(base).mode) && finish<0)finish=t;
   if(finish>=0&&t>=finish+3)break;
  }
  if(finish<0)throw Error('party no finish '+test.name);
  rows.push({...test,event,before,commits,after:stateAt(base),finish});
  fs.writeFileSync('tmp/native-party-empty.json',JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({name:test.name,commits:commits.map(c=>({tick:c.tick,mode:c.state.mode,actors:c.state.actors.map(a=>[a.id,a.active])})),finish,after:stateAt(base).actors.map(a=>[a.id,a.active])}));
 }
 rt.free();
}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
