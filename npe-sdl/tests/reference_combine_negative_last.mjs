import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const configs=[{name:'combine-negative-last',party:3,hp:[300,300,300],enemyHp:[30000,30000,30000],defenses:[0,0,1],attacks:[32767,1,0]}];
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
 const hero=mem().slice(a(0x69c6)-1,a(0x69c6+148)-1);
 for(let i=1;i<config.party;++i)mem().set(hero,a(0x69c6+i*148)-1);
 putB(0x7afa,config.party);putB(0x7626,51);putB(0x7627,51);putW(0x7ab4,1);idle(30);
 for(let i=0;i<config.party+3;++i){const o=i*148;
  const hp=i<config.party?config.hp[i]:config.enemyHp[i-config.party];
  putW(0x64a0+o,hp);putW(0x649e+o,i<config.party?1000:30000);putW(0x64a4+o,i<config.party?500:0);
  putW(0x64a6+o,1);putW(0x64a8+o,i<config.party?(config.secondFast?[1,999,2][i]:999-i):1);putW(0x64aa+o,1);
  putW(0x64ac+o,i<config.party?config.attacks[i]:1);putW(0x64b0+o,i<config.party?config.attacks[i]:1);putW(0x64ae+o,i<config.party?0:config.defenses[i-config.party]);putW(0x64b2+o,i<config.party?0:config.defenses[i-config.party]);putB(0x6468+o,i<config.party?(config.secondFast&&i===1?10:99):10);
  for(let s=0;s<4;++s)putW(0x648e+o+s*2,0);
  if(config.sleep&&i===2)putW(0x6490+o,-4);
  if(i>=config.party){putW(0x6492+o,-36);for(let s=0;s<3;++s)putB(0x6469+o+s,0);}
 }
 // Current resources/firmware are intact; only isolated RAM words change.
 putB(0x69de,99);putB(0x69df,6);putB(0x69e0,0);putB(0x7a9e,0);
 const events=[],state=label=>{const e={label,mode:w(0x7934),actor:b(0x7a9e),menu:b(0x7ac8),row:b(0x7a9c),target:b(0x64ce),action:b(0x64cf),execution:b(0x7acc),commands:Array.from({length:config.party+3},(_,i)=>({kind:b(0x64cf+i*148),target:b(0x64ce+i*148),flag:b(0x64e2+i*148),type:w(0x64d0+i*148),type2:w(0x64d2+i*148)})),
  records:Array.from({length:config.party+3},(_,i)=>({hp:w(0x64a0+i*148),maximum:w(0x649e+i*148),qi:w(0x64a4+i*148),mental:w(0x64a6+i*148),
   attack:w(0x64b0+i*148),defense:w(0x64b2+i*148),level:b(0x6468+i*148),experience:w(0x649c+i*148),
   attackTimer:w(0x6496+i*148),defenseTimer:w(0x6498+i*148),status:[0,1,2,3].map(s=>w(0x648e+i*148+s*2))}))};events.push(e);fs.writeFileSync(new URL(`../../tmp/native-${config.name}-${events.length-1}.idx`,import.meta.url),rt.framePixelIndices());console.log(JSON.stringify({name:config.name,...e}));};
 const defend=()=>{
  if(b(0x7ac8)===10){key(4);key(2);}else if(b(0x7ac8)===20)key(2);
  if(b(0x7ac8)!==30)throw Error('flag '+b(0x7ac8));key(16);
  for(let i=0;i<5&&b(0x7a9c)!==2;++i)key(b(0x7a9c)<2?2:1);
  if(b(0x7a9c)!==2)throw Error('defend row');key(16);
 };
 try{
  state('before');
  key(2);state('icon');if(b(0x7ac8)!==40)throw Error('icon gate');key(16);
  idle(700);state('after');
  samples.push({config,events});
 }catch(error){samples.push({config,events,error:error.code||String(error)});console.log(JSON.stringify({name:config.name,error:error.code||String(error)}));}
 putW(0x7934,0);
}
fs.writeFileSync(new URL('../../tmp/native-combine-negative-last.json',import.meta.url),JSON.stringify(samples,null,2));rt.free();
