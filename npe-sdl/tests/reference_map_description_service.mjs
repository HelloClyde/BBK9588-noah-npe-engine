import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>rt.advance(mask).free();
const rows=process.argv[2]?JSON.parse(fs.readFileSync('tmp/native-map-description-service.json')):[];
try{for(const kind of (process.argv[2]&&process.argv[2]!=='all'?[process.argv[2]]:['item','skill']))for(const test of ['pages','odd-forward','controls','right-word-wrap','right-past-string','left-past-string','left-signed-stop']){
 if(process.argv[3]&&test!==process.argv[3])continue;
 const cfg={kind,test,name:kind+'-'+test},id=kind==='item'?45:6,limit=kind==='item'?90:112,length=100;
 rt.restoreBytes(fs.readFileSync('tmp/native-title-load-saved.snapshot'));rt.drainEventsBytes();
 const candidates=[];for(let i=0;i+0x8000<mem().length;++i)
  if(mem()[i+0x7934]===1&&mem()[i+0x7935]===0&&mem()[i+0x7a58]===1&&mem()[i+0x7a66]===1&&mem()[i+0x7aec]===0xa9&&mem()[i+0x7aeb]===6)candidates.push(i);
 for(let t=0;t<128;++t)tick(t===0?64:0);
 const live=candidates.filter(i=>mem()[i+0x7934]===6);if(live.length!==1)throw Error('RAM discovery');
 const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v&255;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 const key=mask=>{for(let t=0;t<128;++t)tick(t===0?mask:0);};key(64);
 putW(0x7a50,0);putB(0x7a32,0);
 for(let i=0;i<3;++i)putB(0x69f1+148*i,i===0?1:0);putB(0x7afa,1);putB(0x7ab0,1);
 putB(0x69de,99);putW(0x6a16,50);putW(0x6a14,200);putW(0x6a1a,100);putW(0x6a18,120);
 for(let item=0;item<255;++item)putW(0x67c8+item*2,0);putW(0x67c8+45*2,2);
 for(let i=0;i<18;++i)putB(0x69df+i,i===0?6:0);
 const cache=kind==='item'?0x739e:0x730e,size=kind==='item'?142:144;
 putB(kind==='item'?0x7a72:0x7a74,id);for(let i=0;i<size;++i)putB(cache+i,0);
 const name=Buffer.from('b5c0bedf','hex');for(let i=0;i<4;++i)putB(cache+i,name[i]);
 const description=Array.from({length},(_,i)=>65+i%26);
 if(test==='odd-forward'){description[89]=0xb5;description[90]=0xc0;}
 if(test==='controls'){description[4]=13;description[5]=10;}
 for(let i=0;i<description.length;++i)putB(cache+13+i,description[i]);
 if(kind==='item'){putW(0x741e,20);putB(0x7428,1);putB(0x742b,1);}
 else{putB(cache+115,1);putB(cache+116,1);putB(cache+118,1);putW(cache+128,5);}
 putW(0x7aba,77);putW(0x7abe,22);putW(0x7abc,37);
 const state=()=>({mode:w(0x7934),event:w(0x7936),menu:b(0x7a54),page:b(0x7a18),member:b(0x7adc),selected:w(0x7a4c),
  length:w(0x7aba),start:w(0x7abe),end:w(0x7abc),itemLoaded:b(0x7a72),skillLoaded:b(0x7a74),
  itemBytes:Array.from({length:142},(_,i)=>b(0x739e+i)),skillBytes:Array.from({length:144},(_,i)=>b(0x730e+i)),
  npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};}),
  party:{hp:w(0x6a16),maxHp:w(0x6a14),qi:w(0x6a1a),maxQi:w(0x6a18),attack:w(0x6a22),defense:w(0x6a24),level:b(0x69de),experience:w(0x6a12),next:w(0x6a10),other:Array.from({length:3},(_,i)=>w(0x6a1c+i*2)),status:Array.from({length:4},(_,i)=>w(0x6a04+i*2))}});
 const seed=state();
 for(const mask of kind==='item'?[64,2,2,16]:[64,2,16])key(mask);
 const before=state();fs.writeFileSync(`tmp/native-map-description-${cfg.name}-setup.json`,JSON.stringify({seed,before}));fs.writeFileSync(`tmp/native-map-description-${cfg.name}-before.idx`,rt.framePixelIndices());
 const keys=test==='pages'||test==='odd-forward'||test==='controls'?[{mask:16},{key:0x85,...(kind==='skill'?{patch:{end:test==='odd-forward'?88:89}}:{})},{key:0x85},{key:0x84}]:
  [{mask:16},{key:test.startsWith('right')?0x85:0x84,patch:test==='right-word-wrap'?{end:65535}:test==='right-past-string'?{length:65535,end:150}:test==='left-past-string'?{start:length+1}:{start:32768}}];
 const phases=[];let total=0;
 for(const phase of keys){
  for(const [field,value] of Object.entries(phase.patch??{}))putW({length:0x7aba,start:0x7abe,end:0x7abc}[field],value);
  const prior=state(),commits=[];
  for(let t=0;t<128;++t){if(phase.key)putW(0x7a76,t===16?phase.key:0);tick(t===0?(phase.mask??0):0);
   for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(pixels){
    fs.writeFileSync(`tmp/native-map-description-${cfg.name}-${total}.idx`,pixels);commits.push({index:total++,state:state()});
   }}
  }
  phases.push({...phase,before:prior,commits,after:state()});
 }
 rows.push({...cfg,id,limit,length,description,seed,before,phases,after:state(),commits:total});
 fs.writeFileSync('tmp/native-map-description-service.json',JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name:cfg.name,commits:total,phases:phases.map(p=>({commits:p.commits.length,menu:p.after.menu,start:p.after.start,end:p.after.end,length:p.after.length}))}));
 putW(0x7934,0);
}}finally{rt.free();}
