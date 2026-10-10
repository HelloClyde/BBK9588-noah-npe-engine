import fs from 'node:fs';
import {spawnSync} from 'node:child_process';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
// Explicit wide-caster diagnostics cover initial draw, list movement and
// paging. Record payloads remain within the original101-byte description cache.
const cases=[
 {name:'wide-duplicate',skills:[6,6],keys:[{mask:16},{mask:8},{mask:4},{mask:64}]},
 {name:'wide-controls',descriptionHex:Buffer.from(Array.from({length:100},(_,i)=>i===4?13:i===5?10:65+i%26)).toString('hex')+'00',skills:[6],keys:[{mask:16},{mask:64}]},
 {name:'wide-window',skills:Array(10).fill(6),keys:[{mask:16},{mask:2},{mask:2},{mask:2},{mask:1},{mask:64}]},
 {name:'wide-pages',descriptionHex:Buffer.from(Array.from({length:100},(_,i)=>65+i%26)).toString('hex')+'00',
  skills:[6],keys:[{mask:16},{key:0x85,patch:{end:89}},{key:0x85},{key:0x84},{key:0x84},{mask:64}]},
].map(c=>({...c,portraits:[56,8,38],cached:6,members:1}));

const name=process.argv[2],output='tmp/native-magic-description-retain.json';
if(!name){fs.writeFileSync(output,'[]\n');for(const c of cases){const p=spawnSync(process.execPath,[process.argv[1],c.name],{stdio:'inherit'});if(p.error)throw p.error;if(p.status)throw Error(c.name+' failed '+p.status);}process.exit(0);}
const c=cases.find(c=>c.name===name);if(!c)throw Error('unknown case');
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>rt.advance(mask).free();
const key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
try{
 rt.restoreBytes(fs.readFileSync('tmp/native-title-load-saved.snapshot'));rt.drainEventsBytes();
 const candidates=[];for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===1&&mem()[i+0x7935]===0&&mem()[i+0x7a58]===1&&mem()[i+0x7a66]===1&&mem()[i+0x7aec]===0xa9&&mem()[i+0x7aeb]===6)candidates.push(i);
 key(64);const live=candidates.filter(i=>mem()[i+0x7934]===6);if(live.length!==1)throw Error('RAM discovery');
 const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};key(64);
 putW(0x7a50,0);putB(0x7a32,0);
 for(let i=0;i<3;++i)putB(0x69f1+148*i,i<(c.members??1)?1:0);putB(0x7afa,c.members??1);putB(0x7ab0,c.members??1);
 for(let i=0;i<3;++i)putW(0x6a02+148*i,c.portraits[i]);
 putB(0x69de,99);putW(0x6a16,50);putW(0x6a14,200);putW(0x6a1a,100);putW(0x6a18,120);
 if(c.signed){putW(0x6a16,65530);putW(0x6a22,32768);putW(0x6a24,65535);putW(0x6a12,65535);putW(0x6a1c,65535);putW(0x6a1e,32768);}
 for(let i=0;i<18;++i)putB(0x69df+i,c.skills[i]??0);
 putB(0x7a74,c.cached);for(let i=0;i<144;++i)putB(0x730e+i,0xa5);
 const title=Buffer.from('bbbac4e6cfc9caf5','hex');for(let i=0;i<12;++i)putB(0x730e+i,title[i]??0);putB(0x731a,0);
 const desc=Buffer.from(c.descriptionHex??'bec9cbb5c3f700','hex');for(let i=0;i<desc.length;++i)putB(0x731b+i,desc[i]);
 for(let i=114;i<144;++i)putB(0x730e+i,0);putB(0x7381,c.required??1);putB(0x7382,1);putB(0x7383,c.group??0);putB(0x7384,c.side??1);
 putW(0x7386,7);putW(0x7388,3);putW(0x738e,c.fee??5);
 putW(0x7aba,77);putW(0x7abe,22);putW(0x7abc,37);
 const party=i=>{const d=i*148;return {hp:w(0x6a16+d),maxHp:w(0x6a14+d),qi:w(0x6a1a+d),maxQi:w(0x6a18+d),attack:w(0x6a22+d),defense:w(0x6a24+d),level:b(0x69de+d),experience:w(0x6a12+d),nextExperience:w(0x6a10+d),other:Array.from({length:3},(_,j)=>w(0x6a1c+d+2*j)),status:Array.from({length:4},(_,j)=>w(0x6a04+d+2*j))};};
 const state=()=>({portraits:Array.from({length:3},(_,i)=>w(0x6a02+148*i)),mode:w(0x7934),menu:b(0x7a54),page:b(0x7a18),member:b(0x7adc),selected:b(0x7a44),row:b(0x7a20),col:b(0x7a22),first:b(0x7a24),
  loaded:b(0x7a74),bytes:Array.from({length:144},(_,i)=>b(0x730e+i)),length:w(0x7aba),start:w(0x7abe),end:w(0x7abc),
  party:party(0),party2:c.members===2?party(1):undefined});
 const seed=state();for(const mask of [64,2,16])key(mask);
 const before=state();fs.writeFileSync(`tmp/native-magic-description-retain-${name}-before.idx`,rt.framePixelIndices());
 const phases=[];let index=0;
 for(const phase of c.keys){
  for(const [field,value] of Object.entries(phase.patch??{}))putW({length:0x7aba,start:0x7abe,end:0x7abc}[field],value);
  const prior=state(),commits=[];
  for(let t=0;t<128;++t){
   if(phase.key)putW(0x7a76,t===16?phase.key:0);
   tick(t===0?(phase.mask??0):0);
   for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){fs.writeFileSync(`tmp/native-magic-description-retain-${name}-${index}.idx`,p);commits.push({index:index++,tick:t,state:state()});}}}
  phases.push({...phase,key:phase.key??phase.mask,before:prior,commits,after:state()});}
 if(state().mode!==1)throw Error('description retain route did not return to map');
 const rows=JSON.parse(fs.readFileSync(output)).filter(r=>r.name!==name);rows.push({...c,seed,before,phases,after:state()});
 fs.writeFileSync(output,JSON.stringify(rows,null,2)+'\n');console.log(JSON.stringify({name,before:before.menu,phases:phases.map(p=>({key:p.key,frames:p.commits.map(c=>c.state.menu),menu:p.after.menu,loaded:p.after.loaded,hp:p.after.party.hp,qi:p.after.party.qi,length:p.after.length,start:p.after.start,end:p.after.end,page:p.after.page,selected:p.after.selected,row:p.after.row,col:p.after.col,first:p.after.first}))}));
}finally{rt.free();}
