import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const outputName='native-walk-continuation';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
rt.restoreBytes(fs.readFileSync('tmp/empty-title-first-map-snapshot.bin'));
const mem=()=>new Uint8Array(exp.memory.buffer),bases=[];
for(let i=0;i+0x8000<mem().length;++i){const w=o=>mem()[i+o-1]*256+mem()[i+o];
 if(w(0x7934)===1&&w(0x7a58)===1&&w(0x7a66)===1&&mem()[i+0x69d2-9]===1&&mem()[i+0x69d5-9]===7&&mem()[i+0x69d6-9]===1)bases.push(i);}
if(bases.length!==1)throw Error('RAM discovery '+bases.length);
const base=bases[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>mem()[a(o)-1]*256+mem()[a(o)];
const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
const npe=fs.readFileSync('tmp/tjfy-world-probe.npe');
const section=npe.readUInt32BE(0x3a+8*8),record=section+npe.readUInt32BE(section);
const width=npe[record+2];
const tile=(x,y)=>npe.readUInt16BE(record+17+(y*width+x)*2);
for(let y=0;y<7;++y)for(let x=0;x<15;++x)
 if(w(0x70b4+(y*15+x)*2)!==tile(36+x,1+y))throw Error('initial cache / NPE mismatch');
let rngAddress;
const u32=(m,j)=>((m[j]*16777216+m[j+1]*65536+m[j+2]*256+m[j+3])>>>0);
const state=()=>({map:w(0x7a58),encounters:w(0x7a5c),encounterSteps:w(0x7a02),counter:b(0x7a32),mode:w(0x7934),progress:w(0x7a66),heroY:b(0x69d6)+b(0x7a2c),
 heroX:b(0x69d5)+b(0x7a2e),screenX:b(0x69d5),screenY:b(0x69d6),
 viewX:b(0x7a2e),viewY:b(0x7a2c),direction:b(0x69d4),
 redraw:w(0x7ab4),cacheTail:Array.from({length:15},(_,i)=>w(0x70b4+(105+i)*2)),
 rng:rngAddress===undefined?null:u32(mem(),rngAddress),
 npc:Array.from({length:20},(_,j)=>{const o=0x6ca2+j*14;return{id:w(o),graphic:w(o+2),first:w(o+4),last:w(o+6),direction:b(o+8),x:b(o+9),y:b(o+10),moving:b(o+11)};}),
 chase:Array.from({length:2},(_,j)=>{const o=0x783a+j*14;return{graphic:w(o),first:w(o+2),last:w(o+4),enemy:w(o+6),x:w(o+8),y:w(o+10),cooldown:b(o+12)};})});
const period=prefix=>{const counter=b(0x7a32),commits=[];for(let ticks=0;ticks<300;++ticks){
 const out=rt.advance(0);out.free();
 if(prefix)for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
  fs.writeFileSync(`${prefix}-commit-${commits.length}.idx`,pixels);commits.push({tick:ticks});}
 if(b(0x7a32)!==counter)return {ticks,commits};}throw Error('period timeout');};
const initial=state(),rawBefore=mem().slice();period();const rawAfter=mem(),changes=new Set(),rng=[];
for(let j=0;j<Math.min(rawBefore.length,rawAfter.length);++j)if(rawBefore[j]!==rawAfter[j])
 for(let k=Math.max(0,j-3);k<=j&&k+4<rawBefore.length;++k)changes.add(k);
for(const j of changes){const old=u32(rawBefore,j),next=u32(rawAfter,j);if(old<=1||next<=1||old===next)continue;
 let v=old;for(let calls=1;calls<=128;++calls){v=(Math.imul(v,0x514f111f)+1)>>>0;if(v===next){rng.push({offset:j,before:old,after:next,calls});break;}}}
if(rng.length!==1)throw Error('RNG discovery '+JSON.stringify(rng));rngAddress=rng[0].offset;
const chase=(x,y,extra={})=>({graphic:1,first:0,last:0,enemy:89,x,y,cooldown:0,...extra});
const npc=(x,y,extra={})=>({id:1,graphic:14,first:0,last:0,direction:0,x,y,moving:0,...extra});
const cases=[
 ['wall-chase',2,[chase(38,4)]],
 ['wall-two-chases',2,[chase(38,4),chase(48,4,{graphic:2})]],
 ['wall-cooldown-first',2,[chase(38,4,{cooldown:2}),chase(48,4,{graphic:2})]],
 ['wall-cooldown-last-tick',2,[chase(38,4,{cooldown:1}),chase(48,4,{graphic:2})]],
 ['wall-cooldown-second',2,[chase(38,4),chase(48,4,{cooldown:1,graphic:2})]],
 ['wall-empty-first',2,[chase(38,4,{enemy:0}),chase(48,4,{graphic:2})]],
 ['wall-inactive-first',2,[chase(38,4,{first:9,last:9}),chase(48,4,{graphic:2})]],
 ['wall-adjacent-npc',2,[chase(38,4)],[npc(43,3)]],
 ['walk-chase',8,[chase(38,4)]],
 ['wall-encounter-roll',2,[chase(38,4)],[],1,0],
 ['wall-counter-minus-one',2,[chase(38,4)],[],1,65535],
 ['walk-counter-minus-one',8,[chase(38,4)],[],1,65535],
];
const rows=[];
for(const [name,mask,chases,npcs=[],encounters=0,encounterSteps=0] of cases){
 if(w(0x7934)!==1)throw Error('not in map before '+name);
 /* A real ordinary walk with no chase synchronizes the native retained LCD
    and committed front buffer. Periodic calls can leave LCD writes pending. */
 putW(0x7a5c,0);putW(0x7a50,0);
 for(let i=0;i<20;++i){putW(0x6ca2+i*14,0);putB(0x6ca2+i*14+11,0);}
 for(let i=0;i<2;++i)putW(0x783a+i*14+6,0);
 for(let i=0;i<4;++i){putW(0x761a+i*14+6,0);putB(0x761a+i*14+11,0);}
 putB(0x69d5,6);putB(0x69d6,1);putB(0x7a2e,36);putB(0x7a2c,1);
 for(let y=0;y<7;++y)for(let x=0;x<15;++x)putW(0x70b4+(y*15+x)*2,tile(36+x,1+y));
 for(let t=0;t<32;++t){const out=rt.advance(t===0?8:0);out.free();}
 putB(0x69d5,7);putB(0x69d6,1);putB(0x7a2e,36);putB(0x7a2c,1);
 putW(0x7a50,0);putW(0x7a5c,encounters);putW(0x7a02,encounterSteps);
 putB(0x7ac4,0);putW(0x7ab4,0);
 for(let i=0;i<20;++i){const n=npcs[i]??{...initial.npc[i],id:0,moving:0},o=0x6ca2+i*14;
  putW(o,n.id);putW(o+2,n.graphic);putW(o+4,n.first);putW(o+6,n.last);
  putB(o+8,n.direction);putB(o+9,n.x);putB(o+10,n.y);putB(o+11,n.moving);}
 for(let i=0;i<4;++i){const o=0x761a+i*14;putW(o+6,0);putB(o+11,0);}
 for(let i=0;i<2;++i){const n=chases[i]??chase(0,0,{enemy:0}),o=0x783a+i*14;
  putW(o,n.graphic);putW(o+2,n.first);putW(o+4,n.last);putW(o+6,n.enemy);
  putW(o+8,n.x);putW(o+10,n.y);putB(o+12,n.cooldown);}
 mem().set([0,0,0,11],rngAddress);
 const before=state(),index=rows.length,prefix=`tmp/${outputName}-${index}`,commits=[];
 fs.writeFileSync(`${prefix}-before.idx`,rt.framePixelIndices());
 for(let t=0;t<32;++t){const out=rt.advance(t===0?mask:0);out.free();
  for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
   fs.writeFileSync(`${prefix}-commit-${commits.length}.idx`,pixels);commits.push({tick:t});}}
 const after=state();fs.writeFileSync(`${prefix}-after.idx`,rt.framePixelIndices());
 if(after.mode!==1||after.map!==1)throw Error('unexpected mode/map '+name);
 rows.push({name,mask,before,after,commits});
 fs.writeFileSync(`tmp/${outputName}.json`,JSON.stringify({discovery:rng[0],rows},null,2)+'\n');
 console.log(name,JSON.stringify({commits:commits.length,chase:after.chase,rng:after.rng,encounterSteps:after.encounterSteps}));
}
rt.free();
