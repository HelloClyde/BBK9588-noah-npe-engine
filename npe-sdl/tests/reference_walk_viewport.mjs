import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
// Original PRG execution. Inject viewport fields and their105 NPE cache words;
// preserve and capture the actual LCD, never generate reference pixels in C.
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const npe=fs.readFileSync('tmp/tjfy-world-probe.npe');
const section=npe.readUInt32BE(0x3a+8*8),record=section+npe.readUInt32BE(section);
const map={width:npe[record+2],height:npe[record+3]};
map.tiles=Array.from({length:map.width*map.height},(_,i)=>npe.readUInt16BE(record+17+i*2));
const snapshot=fs.readFileSync('tmp/empty-title-first-map-snapshot.bin');
// name, mask, viewX, viewY, screenX, screenY
const edges=process.argv.includes('--edges');
const stem=edges?'native-walk-edge':'native-walk-viewport';
const cases=edges?[
 ['up-top-row1',1,13,0,7,1],['up-top-row2',1,13,0,7,2],
 ['right-last-scroll',8,39,1,7,4],['right-edge-screen',8,40,1,7,4],
 ['left-first-scroll',4,1,6,7,4],['left-edge-screen',4,0,6,7,4],
]:[
 ['up-screen',1,36,1,12,5],['down-screen',2,36,1,12,3],
 ['up-scroll',1,36,5,12,4],['down-scroll',2,36,1,12,4],
 ['up-low-screen',1,36,5,12,2],['down-high-screen',2,36,1,12,6],
 ['right-low-screen',8,36,1,2,5],['right-high-screen',8,36,1,12,5],
 ['left-low-screen',4,34,1,6,5],['left-high-screen',4,36,1,12,5],
 ['down-bottom-center',2,34,23,7,4],['down-bottom-blocked',2,33,23,7,5],
 ['up-bottom-screen',1,34,23,7,6],
];
const rows=[],mem=()=>new Uint8Array(exp.memory.buffer);
for(const [name,mask,vx,vy,sx,sy] of cases){
 rt.restoreBytes(snapshot);for(let t=0;t<15;++t){const out=rt.advance(0);out.free();}
 const bases=[];
 const bankBytes=Buffer.from(exp.memory.buffer),signature=Buffer.from([0,1,0,7]);
 for(let p=bankBytes.indexOf(signature);p>=0;p=bankBytes.indexOf(signature,p+1)){
  const i=p-0x6ca2+9;if(i<0||i+0x8000>=mem().length)continue;
  const w=o=>mem()[i+o-1]*256+mem()[i+o];
  const low=o=>mem()[i+o-9]*256+mem()[i+o-8];
  if(w(0x7934)===1&&w(0x7a58)===1&&w(0x7a66)===1&&
     mem()[i+0x69d2-9]===1&&mem()[i+0x69d5-9]===7&&mem()[i+0x69d6-9]===1&&
     low(0x6ca2)===1&&low(0x6ca4)===7)bases.push(i);
 }
 if(bases.length!==1)throw Error('RAM discovery '+bases.length);
 const base=bases[0],a=o=>base+o-(o<0x7000?8:0);
 const b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 const state=()=>({mode:w(0x7934),progress:w(0x7a66),map:w(0x7a58),
  direction:b(0x69d4),screenX:b(0x69d5),screenY:b(0x69d6),viewX:b(0x7a2e),viewY:b(0x7a2c),
  moveKind:w(0x7a86),width:b(0x7a56),height:b(0x7a5a),
  npc:Array.from({length:20},(_,j)=>{const o=0x6ca2+j*14;return{id:w(o),graphic:w(o+2),first:w(o+4),last:w(o+6),direction:b(o+8),x:b(o+9),y:b(o+10),moving:b(o+11)};})});
 for(let y=0;y<7;++y)for(let x=0;x<15;++x)
  if(w(0x70b4+(y*15+x)*2)!==map.tiles[(b(0x7a2c)+y)*map.width+b(0x7a2e)+x])
   throw Error('initial original cache / NPE mismatch');
 putW(0x7a50,0);putW(0x7a5c,0);
 for(let j=0;j<20;++j){putW(0x6ca2+j*14,0);putB(0x6ca2+j*14+11,0);}
 putB(0x7a2e,vx);putB(0x7a2c,vy);putB(0x69d5,sx);putB(0x69d6,sy);
 for(let y=0;y<7;++y)for(let x=0;x<15;++x)
  putW(0x70b4+(y*15+x)*2,map.tiles[(vy+y)*map.width+vx+x]);
 const before=state(),index=rows.length,prefix=`tmp/${stem}-${index}`,commits=[];
 if(before.width!==map.width||before.height!==map.height)throw Error('map dimensions');
 fs.writeFileSync(`${prefix}-before.idx`,rt.framePixelIndices());
 for(let t=0;t<32;++t){const out=rt.advance(t===0?mask:0);out.free();
  for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
   fs.writeFileSync(`${prefix}-commit-${commits.length}.idx`,pixels);commits.push({tick:t});}
 }
 const after=state();fs.writeFileSync(`${prefix}-after.idx`,rt.framePixelIndices());
 if(after.mode!==1||after.map!==1)throw Error('unexpected mode/map '+name);
 rows.push({name,mask,before,after,commits});console.log(name,JSON.stringify({before:[vx,vy,sx,sy],
  after:[after.viewX,after.viewY,after.screenX,after.screenY],kind:after.moveKind,commits:commits.length}));
 fs.writeFileSync(`tmp/${stem}.json`,JSON.stringify({rows},null,2)+'\n');
}
rt.free();
