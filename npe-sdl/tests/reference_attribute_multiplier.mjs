import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const configs=[{"name": "party-attribute-0", "pa": 1, "pd": 0, "ea": 1, "ed": 0, "pl": 1, "el": 1}, {"name": "party-attribute-1", "pa": 1, "pd": 0, "ea": 1, "ed": 0, "pl": 100, "el": 65437}, {"name": "party-attribute-2", "pa": 1, "pd": 0, "ea": 1, "ed": 0, "pl": 100, "el": 65435}, {"name": "party-attribute-3", "pa": 1, "pd": 0, "ea": 1, "ed": 0, "pl": 1000, "el": 64537}, {"name": "party-attribute-4", "pa": 1, "pd": 0, "ea": 1, "ed": 0, "pl": 100, "el": 65436}, {"name": "party-attribute-5", "pa": 1, "pd": 0, "ea": 1, "ed": 0, "pl": 0, "el": 0}, {"name": "party-attribute-6", "pa": 1, "pd": 0, "ea": 1, "ed": 0, "pl": 32768, "el": 32767}, {"name": "party-attribute-7", "pa": 1, "pd": 0, "ea": 1, "ed": 0, "pl": 32767, "el": 32767}, {"name": "party-attribute-8", "pa": 1, "pd": 0, "ea": 1, "ed": 0, "pl": 32768, "el": 32768}, {"name": "party-attribute-product", "pa": 1000, "pd": 0, "ea": 1, "ed": 0, "pl": 100, "el": 65437}, {"name": "enemy-attribute-0", "pa": 1, "pd": 0, "ea": 1, "ed": 0, "pl": 1, "el": 1}, {"name": "enemy-attribute-1", "pa": 1, "pd": 0, "ea": 1, "ed": 0, "pl": 65437, "el": 100}, {"name": "enemy-attribute-2", "pa": 1, "pd": 0, "ea": 1, "ed": 0, "pl": 65435, "el": 100}, {"name": "enemy-attribute-3", "pa": 1, "pd": 0, "ea": 1, "ed": 0, "pl": 64537, "el": 1000}, {"name": "enemy-attribute-4", "pa": 1, "pd": 0, "ea": 1, "ed": 0, "pl": 65436, "el": 100}, {"name": "enemy-attribute-5", "pa": 1, "pd": 0, "ea": 1, "ed": 0, "pl": 0, "el": 0}, {"name": "enemy-attribute-6", "pa": 1, "pd": 0, "ea": 1, "ed": 0, "pl": 32767, "el": 32768}, {"name": "enemy-attribute-7", "pa": 1, "pd": 0, "ea": 1, "ed": 0, "pl": 32767, "el": 32767}, {"name": "enemy-attribute-8", "pa": 1, "pd": 0, "ea": 1, "ed": 0, "pl": 32768, "el": 32768}, {"name": "enemy-attribute-product", "pa": 1, "pd": 0, "ea": 1000, "ed": 0, "pl": 65437, "el": 100}];
const exp=await initWasm({module_or_path:fs.readFileSync(new URL('../../tmp/opennoah_wasm_bg.wasm',import.meta.url))});
const rt=new GameRuntime(fs.readFileSync(new URL('../../tmp/tianjiang-online.opennoah',import.meta.url)),'certified_original_v1');
const samples=[];
for(const config of configs.filter(c=>!process.argv[2]||c.name.startsWith(process.argv[2]+'-'))){
 rt.restoreBytes(fs.readFileSync(new URL('../../tmp/battle-fresh-start.bin',import.meta.url)));
 const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
 for(let i=0;i+0x8000<mem().length;i++)if(mem()[i+0x7934]===3&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7a58]===9&&mem()[i+0x7a66]===6)found.push(i);
 if(found.length!==1)throw Error('RAM '+found.length);
 const base=found[0],a=o=>base+o-(o<0x7000?8:0),w=o=>mem()[a(o)-1]<<8|mem()[a(o)],b=o=>mem()[a(o)-1];
 const putW=(o,v)=>{mem()[a(o)-1]=(v&65535)>>>8;mem()[a(o)]=v&255;},putB=(o,v)=>mem()[a(o)-1]=v;
 const tick=mask=>{const out=rt.advance(mask);out.free();},idle=n=>{for(let i=0;i<n;++i)tick(0);},key=mask=>{tick(mask);idle(10);};
 putW(0x7ab4,1);idle(30);
 // Independent RAM fixtures: one party, native three physical enemy slots, middle enemy only.
 config.pa??=0;config.pd??=13;config.ea??=60;config.ed??=45;
 for(const i of [0,2]){const o=i*148;
  const hp=config.name.startsWith('enemy-')&&i===0?32767:30000;
  putW(0x649e+o,hp);putW(0x64a0+o,hp);putW(0x64a4+o,0);putW(0x64aa+o,i===0?config.pl:config.el);putB(0x6468+o,10);
  for(let s=0;s<4;++s)putW(0x648e+o+2*s,0);
 }
 putW(0x64a8,999);putW(0x64a8+2*148,1);
 putW(0x64ac,config.pa);putW(0x64b0,config.pa);putW(0x64ae,config.pd);putW(0x64b2,config.pd);
 putW(0x64ac+2*148,config.ea);putW(0x64b0+2*148,config.ea);putW(0x64ae+2*148,config.ed);putW(0x64b2+2*148,config.ed);
 for(let s=0;s<3;++s)putB(0x6469+2*148+s,0);
 for(const i of [1,3])putW(0x64a0+i*148,0);
 if(config.name.startsWith('party-'))putW(0x6492+2*148,-36);
 const records=()=>Array.from({length:4},(_,i)=>({hp:w(0x64a0+i*148),qi:w(0x64a4+i*148),attribute:w(0x64aa+i*148),attack:w(0x64b0+i*148),defense:w(0x64b2+i*148),
  attackTimer:w(0x6496+i*148),defenseTimer:w(0x6498+i*148),status:[0,1,2,3].map(s=>w(0x648e+i*148+s*2))}));
 const before=records();
 try {
 key(16);key(16);
 idle(700);if(w(0x7934)!==3)throw Error('battle mode '+config.name+' '+w(0x7934));
 const sample={config,before,after:records(),prepared:w(0x7926),actor:b(0x7a9e),menu:b(0x7ac8)};
 samples.push(sample);console.log(JSON.stringify(sample));
 } catch(error) {const sample={config,before,error:error.code||String(error)};samples.push(sample);console.log(JSON.stringify(sample));}
 // restoreBytes relocates RAM; invalidate only this retired fixture's signature
 // before restoring, so the next sample must still find one live candidate.
 putW(0x7934,0);
}
fs.writeFileSync(new URL(`../../tmp/native-attribute-multiplier-${process.argv[2]||'all'}.json`,import.meta.url),JSON.stringify(samples,null,2));rt.free();
