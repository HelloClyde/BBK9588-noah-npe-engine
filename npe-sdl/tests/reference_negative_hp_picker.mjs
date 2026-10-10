import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const configs=[
 {name:'single-enemy-minus1',hp:[30000,0,0],actor:0,enemyHp:[65535,30000,0]},
 {name:'single-enemy-min',hp:[30000,0,0],actor:0,enemyHp:[32768,30000,0]},
 {name:'cycle-enemy-minus1',hp:[30000,0,0],actor:0,enemyHp:[65535,30000,30000],formation:true},
 {name:'cycle-enemy-min',hp:[30000,0,0],actor:0,enemyHp:[32768,30000,30000],formation:true}
];
const exp=await initWasm({module_or_path:fs.readFileSync(new URL('../../tmp/opennoah_wasm_bg.wasm',import.meta.url))});
const rt=new GameRuntime(fs.readFileSync(new URL('../../tmp/tianjiang-online.opennoah',import.meta.url)),'certified_original_v1');
const samples=[];
for(const config of configs){
 rt.restoreBytes(fs.readFileSync(new URL('../../tmp/battle-fresh-start.bin',import.meta.url)));
 const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
 for(let i=0;i+0x8000<mem().length;i++)if(mem()[i+0x7934]===3&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7a58]===9&&mem()[i+0x7a66]===6)found.push(i);
 if(found.length!==1)throw Error('RAM '+found.length);
 const base=found[0],a=o=>base+o-(o<0x7000?8:0),w=o=>mem()[a(o)-1]<<8|mem()[a(o)],b=o=>mem()[a(o)-1];
 const putW=(o,v)=>{mem()[a(o)-1]=(v&65535)>>>8;mem()[a(o)]=v&255;},putB=(o,v)=>mem()[a(o)-1]=v;
 const tick=mask=>{const out=rt.advance(mask);out.free();},idle=n=>{for(let i=0;i<n;++i)tick(0);},key=mask=>{tick(mask);idle(10);};
 // Independent original RAM fixture; source files and continuous story saves stay intact.
 const hero=mem().slice(a(0x69c6)-1,a(0x69c6+148)-1);
 for(let i=1;i<3;++i)mem().set(hero,a(0x69c6+i*148)-1);
 if(config.formation){putB(0x7626,51);putB(0x7627,51);}
 putB(0x7afa,3);putW(0x7ab4,1);idle(30);
 for(let i=0;i<3;++i){const o=i*148;
  putW(0x64a0+o,config.hp[i]);putW(0x649e+o,config.maximum??30000);
  putW(0x64a4+o,0);putW(0x64a6+o,config.mental??1);putW(0x64a8+o,999-i);putW(0x64aa+o,1);
  putW(0x64ac+o,config.win?9999:1);putW(0x64b0+o,config.win?9999:1);putW(0x64ae+o,0);putW(0x64b2+o,0);
  putB(0x6468+o,i===config.actor?99:10);putW(0x649a+o,32767);putW(0x649c+o,100);
  for(let s=0;s<4;++s)putW(0x648e+o+s*2,0);
 }
 for(let i=0;i<3;++i){const o=(3+i)*148,hp=config.enemyHp?.[i]??(i===1?(config.win?1:30000):0);
  putW(0x64a0+o,hp);putW(0x649e+o,30000);putW(0x64a4+o,0);putW(0x64a8+o,1);putW(0x64aa+o,1);
  putW(0x64ac+o,1);putW(0x64b0+o,1);putW(0x64ae+o,0);putW(0x64b2+o,0);putB(0x6468+o,10);
  for(let s=0;s<3;++s)putB(0x6469+o+s,0);
  for(let s=0;s<4;++s)putW(0x648e+o+s*2,0);
 }
 putB(0x7a9e,config.actor);
 const records=()=>Array.from({length:6},(_,i)=>({hp:w(0x64a0+i*148),maximum:w(0x649e+i*148),qi:w(0x64a4+i*148),mental:w(0x64a6+i*148),
  attack:w(0x64b0+i*148),defense:w(0x64b2+i*148),level:b(0x6468+i*148),experience:w(0x649c+i*148),
  attackTimer:w(0x6496+i*148),defenseTimer:w(0x6498+i*148),status:[0,1,2,3].map(s=>w(0x648e+i*148+s*2))}));
 const before=records();
 try {
  key(16);let picker={menu:b(0x7ac8),target:b(0x7ac0)};
  const pickerTrace=[picker];
  if(config.formation){for(const mask of [2,2,1]){key(mask);pickerTrace.push({menu:b(0x7ac8),target:b(0x7ac0)});}key(16);}
  idle(700);
  const sample={config,before,after:records(),picker,pickerTrace,mode:w(0x7934),actor:b(0x7a9e),menu:b(0x7ac8),enemyTarget:b(0x64ce+4*148),
   mapParty:Array.from({length:3},(_,i)=>({hp:w(0x6a16+i*148),level:b(0x69de+i*148),experience:w(0x6a12+i*148)}))};
  samples.push(sample);console.log(JSON.stringify(sample));
 }catch(error){const sample={config,before,error:error.code||String(error)};samples.push(sample);console.log(JSON.stringify(sample));}
 putW(0x7934,0);
}
fs.writeFileSync(new URL('../../tmp/native-negative-hp-picker.json',import.meta.url),JSON.stringify(samples,null,2));rt.free();
