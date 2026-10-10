import fs from 'node:fs';import{GameRuntime,initWasm}from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/empty-title-first-map-snapshot.bin');
rt.restoreBytes(snapshot);const mem=()=>new Uint8Array(exp.memory.buffer),bases=[];
for(let i=0;i+0x8000<mem().length;++i){const w=o=>mem()[i+o-1]*256+mem()[i+o];
 if(w(0x7934)===1&&w(0x7a58)===1&&w(0x7a66)===1&&mem()[i+0x69d2-9]===1&&mem()[i+0x69d5-9]===7&&mem()[i+0x69d6-9]===1)bases.push(i);}
if(bases.length!==1)throw Error('RAM discovery '+bases.length);
const base=bases[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>mem()[a(o)-1]*256+mem()[a(o)];
const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
const u32=(m,j)=>((m[j]*16777216+m[j+1]*65536+m[j+2]*256+m[j+3])>>>0);
const state=()=>({counter:b(0x7a32),mode:w(0x7934),progress:w(0x7a66),
 npc:Array.from({length:20},(_,j)=>{const o=0x6ca2+j*14;return{id:w(o),graphic:w(o+2),first:w(o+4),last:w(o+6),direction:b(o+8),x:b(o+9),y:b(o+10),moving:b(o+11)};}),
 objects:Array.from({length:8},(_,j)=>[b(0x75a4+j*8),b(0x75a5+j*8)]),
 exits:Array.from({length:12},(_,j)=>[b(0x6b82+j*24),b(0x6b83+j*24)])});
const tick=()=>{const out=rt.advance(0);out.free();};
const cycle=before=>{for(let ticks=0;ticks<300;++ticks){tick();if(b(0x7a32)!==before.counter)return {ticks,after:state()};}throw Error('periodic event timeout');};
const before=state(),rawBefore=mem().slice(),baseline=cycle(before),rawAfter=mem(),changes=new Set();
for(let j=0;j<Math.min(rawBefore.length,rawAfter.length);++j)if(rawBefore[j]!==rawAfter[j])
 for(let k=Math.max(0,j-3);k<=j&&k+4<rawBefore.length;++k)changes.add(k);
const rng=[];
for(const j of changes){const old=u32(rawBefore,j),next=u32(rawAfter,j);if(old<=1||next<=1||old===next)continue;
 let v=old;for(let calls=1;calls<=128;++calls){v=(Math.imul(v,0x514f111f)+1)>>>0;if(v===next){rng.push({offset:j,before:old,after:next,calls});break;}}}
if(rng.length!==1)throw Error('resident RNG discovery '+JSON.stringify(rng));
const address=rng[0].offset,readRng=()=>u32(mem(),address),seed=value=>{mem().set([value>>>24,value>>>16&255,value>>>8&255,value&255],address);};
const rows=[{name:'original-eight',seed:rng[0].before,before,after:baseline.after,ticks:baseline.ticks,rngAfter:rng[0].after,pixels:true}];
fs.writeFileSync('tmp/native-npc-periodic-0.idx',rt.framePixelIndices());
const directionSeed=d=>{for(let s=1;s<1000;++s)if(((Math.imul(s,0x514f111f)+1)>>>16&3)===d)return s;throw Error('seed');};
const npe=fs.readFileSync('tmp/tjfy-world-probe.npe'),maps=npe.readUInt32BE(0x3a+8*8),h=maps+npe.readUInt32BE(maps);
const width=npe[h+2],height=npe[h+3],blocked=[...before.objects,...before.exits,...before.npc.map(n=>[n.x,n.y]),[43,2]];
let open;
for(let y=1;y<height-1&&!open;++y)for(let x=1;x<width-1&&!open;++x){
 const targets=[[x,y+1],[x+1,y],[x,y-1],[x-1,y]];
 if(targets.every(([tx,ty])=>npe.readUInt16BE(h+17+(ty*width+tx)*2)<=11&&
     !blocked.some(([bx,by])=>bx===tx&&by===ty)))open={x,y};
}
if(!open)throw Error('no open four-direction diagnostic point');
const cases=[...Array.from({length:4},(_,d)=>({name:'free-'+d,...open,d})),
 {name:'hero',x:42,y:2,d:1}, {name:'npc',x:49,y:3,d:1},
 {name:'hidden-npc',x:49,y:3,d:1,hidden:true},
 {name:'empty-npc',x:43,y:4,d:1,empty:true},
 {name:'object',x:43,y:4,d:1,object:true},
 {name:'exit',x:43,y:4,d:1,exit:true},
 {name:'wall',x:38,y:3,d:1},
 {name:'inactive-moving',x:43,y:4,d:0,inactive:true},
 {name:'moving-byte-two',x:43,y:4,d:0,moving:2},
 {name:'left-boundary',x:0,y:4,d:3}];
for(const config of cases){
 // Keep this live firmware instance and its timer/resident allocation.
 // Restore only the diagnostic NPC/object/exit fields between cases.
 // A whole-runtime snapshot restore may relocate the RAM backing buffers.
 if(w(0x7934)!==1||b(0x69d5)!==7)throw Error('unexpected map state');
 for(let i=0;i<20;++i){const n=before.npc[i],p=0x6ca2+i*14;
  putW(p,n.id);putW(p+2,n.graphic);putW(p+4,n.first);putW(p+6,n.last);
  putB(p+8,n.direction);putB(p+9,n.x);putB(p+10,n.y);putB(p+11,n.moving);}
 before.objects.forEach(([x,y],i)=>{putB(0x75a4+i*8,x);putB(0x75a5+i*8,y);});
 before.exits.forEach(([x,y],i)=>{putB(0x6b82+i*24,x);putB(0x6b83+i*24,y);});
 for(let i=0;i<20;++i)putB(0x6cad+i*14,0);
 const slot=6,o=0x6ca2+slot*14;putB(o+11,config.moving??1);putB(o+9,config.x);putB(o+10,config.y);
 if(config.hidden){putW(0x6ca2+3*14+4,9);putW(0x6ca2+3*14+6,9);}
 if(config.inactive){putW(o+4,9);putW(o+6,9);}
 if(config.empty){putB(0x6ca2+13*14+9,44);putB(0x6ca2+13*14+10,4);}
 if(config.object){putB(0x75a4,44);putB(0x75a5,4);}
 if(config.exit){putB(0x6b82,44);putB(0x6b83,4);}
 const initial=directionSeed(config.d);seed(initial);const start=state(),result=cycle(start);
 rows.push({name:config.name,seed:initial,before:start,after:result.after,ticks:result.ticks,rngAfter:readRng(),pixels:false});
 const actor=result.after.npc[slot];console.log(config.name,JSON.stringify({seed:initial,actor,rngAfter:readRng()}));
}
fs.writeFileSync('tmp/native-npc-periodic.json',JSON.stringify({discovery:rng[0],rows},null,2)+'\n');rt.free();
