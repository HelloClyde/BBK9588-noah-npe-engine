import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const marginMatrix=process.argv.includes('--margin-matrix');
const outputName=marginMatrix?'native-chase-margin':'native-chase-compositor';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
rt.restoreBytes(fs.readFileSync('tmp/empty-title-first-map-snapshot.bin'));
const mem=()=>new Uint8Array(exp.memory.buffer),bases=[];
for(let i=0;i+0x8000<mem().length;++i){const w=o=>mem()[i+o-1]*256+mem()[i+o];
 if(w(0x7934)===1&&w(0x7a58)===1&&w(0x7a66)===1&&mem()[i+0x69d2-9]===1&&mem()[i+0x69d5-9]===7&&mem()[i+0x69d6-9]===1)bases.push(i);}
if(bases.length!==1)throw Error('RAM discovery '+bases.length);
const base=bases[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>mem()[a(o)-1]*256+mem()[a(o)];
const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
let rngAddress;
const u32=(m,j)=>((m[j]*16777216+m[j+1]*65536+m[j+2]*256+m[j+3])>>>0);
const state=()=>({counter:b(0x7a32),mode:w(0x7934),progress:w(0x7a66),heroY:b(0x69d6)+b(0x7a2c),
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
const ordinaryCases=[
 ['left-diagonal',[chase(38,3)]],['right-diagonal',[chase(48,3,{graphic:2})]],
 ['above-aligned',[chase(44,2)]],['lower-left',[chase(38,6)]],
 ['right-edge',[chase(50,3)]],['left-offscreen',[chase(35,3)]],
 ['top-row',[chase(39,1)]],['bottom-row',[chase(39,8)]],
 ['bottom-visited-high',[chase(39,8)],[],[6,1]],
 ['bottom-visited-low',[chase(39,8)],[],[7,1]],
 ['two-overlap',[chase(38,3),chase(39,3,{graphic:2})]],
 ['two-overlap-reversed',[chase(39,3,{graphic:2}),chase(38,3)]],
 ['cooldown-first',[chase(38,3,{cooldown:2}),chase(48,3,{graphic:2})]],
 ['cooldown-last-tick',[chase(38,3,{cooldown:1}),chase(48,3,{graphic:2})]],
 ['cooldown-second',[chase(38,3),chase(48,3,{graphic:2,cooldown:1})]],
 ['inactive-first',[chase(38,3,{first:9,last:9}),chase(48,3,{graphic:2})]],
 ['empty-first',[chase(38,3,{enemy:0}),chase(48,3,{graphic:2})]],
 ['npc-over-new-chase',[chase(42,3)],[npc(43,4)]],
 ['moving-npc-over-chase',[chase(42,3)],[npc(43,4,{moving:1})]],
 ['moving-npc-erases-chase',[chase(38,3)],[npc(39,4,{moving:1})]],
 ['hidden-moving-npc',[chase(38,3)],[npc(39,4,{first:9,last:9,moving:1})]],
 ['contact-first',[chase(44,5),chase(48,3,{graphic:2})],[npc(43,4,{moving:1})]],
];
/* Recreate the dirty margin before every target. A shared live instance
   alone would let the first restoration conceal later cases. Capture
   actual LCD inputs; do not synthesize them from the patched fields. */
const marginTargets=[
 ['empty',[]],['far-visible',[npc(46,4)]],
 ['above',[npc(43,4)]],['below',[npc(43,6)]],
 ['same-cell',[npc(43,5)]],
 ['hidden-above',[npc(43,4,{first:9,last:9})]],
 ['offscreen',[npc(34,4)]],
 ['other-graphic',[npc(43,4,{graphic:7})]],
];
const cases=marginMatrix?marginTargets.flatMap(([name,actors])=>[
 ['dirty-'+name,[chase(39,8)]],
 [name,[chase(42,3)],actors]
]):ordinaryCases;
const rows=[];
for(const [name,chases,npcs=[],visited] of cases){
 if(w(0x7934)!==1)throw Error('not in map before '+name);
 putB(0x69d6,4);putW(0x7a50,0);
 for(let i=0;i<30;++i)putB(0x7186+i,0);
 if(visited)putB(0x7186+visited[0],visited[1]);
 for(let i=0;i<20;++i){const n=npcs[i]??{...initial.npc[i],id:0,moving:0},o=0x6ca2+i*14;
  putW(o,n.id);putW(o+2,n.graphic);putW(o+4,n.first);putW(o+6,n.last);
  putB(o+8,n.direction);putB(o+9,n.x);putB(o+10,n.y);putB(o+11,n.moving);}
 for(let i=0;i<2;++i){const n=chases[i]??chase(0,0,{enemy:0}),o=0x783a+i*14;
  putW(o,n.graphic);putW(o+2,n.first);putW(o+4,n.last);putW(o+6,n.enemy);
  putW(o+8,n.x);putW(o+10,n.y);putB(o+12,n.cooldown);}
 mem().set([0,0,0,11],rngAddress);
 const before=state(),index=rows.length,prefix=`tmp/${outputName}-${index}`;
 fs.writeFileSync(`${prefix}-before.idx`,rt.framePixelIndices());
 const result=period(prefix),after=state();
 fs.writeFileSync(`${prefix}-after.idx`,rt.framePixelIndices());
 rows.push({name,before,after,...result});console.log(name,JSON.stringify({mode:after.mode,chase:after.chase,rng:after.rng,commits:result.commits.length}));
}
fs.writeFileSync(`tmp/${outputName}.json`,JSON.stringify({discovery:rng[0],rows},null,2)+'\n');rt.free();
