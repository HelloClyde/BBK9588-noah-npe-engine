import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
// Independent native RAM fixtures. Original NPE/PRG and continuous saves stay intact.
// Capture every display commit, including repeated base restoration frames.
const enemySpell=false;
const preparedLabels=process.argv.includes('--prepared-labels');
const configs=(preparedLabels?[
 {name:'item-prepared-group-attack',item:62,target:0,character:2},
 {name:'item-prepared-group-defense',item:63,target:0,character:3},
 {name:'item-prepared-group-revival',item:65,target:0,character:2,hp:[20000,0,0],preparedWords:[101,0,103,104,105,106]},
 {name:'item-prepared-group-revival-negative-display',item:65,target:0,character:2,hp:[5000,0,0],maximum:10000}
]:[
 {name:'item-animation-hp',item:46,target:1,character:1},
 {name:'item-animation-qi',item:55,target:1,character:2},
 {name:'item-animation-group-hp',item:61,target:0,character:3},
 {name:'item-animation-attack',item:62,target:0,character:2},
 {name:'item-animation-defense',item:63,target:0,character:3},
 {name:'item-animation-sleep-cure',item:69,target:1,character:1},
 {name:'item-animation-group-revival',item:65,target:0,character:2,hp:[20000,0,0]},
 {name:'item-animation-hp-cap',item:46,target:1,character:3,hp:[20000,20000,29980]}
]).map(c=>({party:3,character:1,enemyActor:1,initializeMap:true,hp:[20000,20000,20000],
 attacks:[90,120,150],enemyHp:[20000,20000,20000],defenses:[0,3,90],...c}));
let recording=false,commits=[];
const exp=await initWasm({module_or_path:fs.readFileSync(new URL('../../tmp/opennoah_wasm_bg.wasm',import.meta.url))});
const rt=new GameRuntime(fs.readFileSync(new URL('../../tmp/tianjiang-online.opennoah',import.meta.url)),'certified_original_v1');
const samples=[];
const npe=fs.readFileSync(new URL('../../tmp/tjfy-world-probe.npe',import.meta.url));
const characterSection=npe.readUInt32BE(0x3a+12*8);
const characterSize=npe.readUInt32BE(0x3a+12*8+4);
const itemSection=npe.readUInt32BE(0x3a+2*8);
const itemRecord=id=>npe.subarray(itemSection+npe.readUInt32BE(itemSection+(id-1)*4));
const characterRecord=id=>{const end=npe.readUInt32BE(characterSection+id*4);return npe.subarray(characterSection+npe.readUInt32BE(characterSection+(id-1)*4),characterSection+(end===0xffffffff?characterSize:end));};
for(const config of configs){
 const itemRaw=itemRecord(config.item);config.group=itemRaw[36]===1;config.own=itemRaw[37]===1;
 if(!config.own)throw Error("own-side item fixture required");
 commits=[];
 const preparedPhases=[];
 rt.restoreBytes(fs.readFileSync(new URL('../../tmp/battle-fresh-start.bin',import.meta.url)));
 const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
 for(let i=0;i+0x8000<mem().length;i++)if(mem()[i+0x7934]===3&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7a58]===9&&mem()[i+0x7a66]===6)found.push(i);
 if(found.length!==1)throw Error('RAM '+found.length);
 const base=found[0],a=o=>base+o-(o<0x7000?8:0),w=o=>mem()[a(o)-1]<<8|mem()[a(o)],b=o=>mem()[a(o)-1];
 const putW=(o,v)=>{mem()[a(o)-1]=(v&65535)>>>8;mem()[a(o)]=v&255;},putB=(o,v)=>mem()[a(o)-1]=v;
 const prepared=()=>Array.from({length:config.party+3},(_,i)=>Array.from({length:12},(_,j)=>b(0x64b6+i*148+j)));
 const tick=mask=>{const out=rt.advance(mask);out.free();if(recording){for(let j=0;j<rt.displayCommitCount();++j){let pix=rt.displayCommitPixelIndices(j);if(pix){let index=commits.length;fs.writeFileSync(new URL('../../tmp/native-'+config.name+'-commit-'+index+'.idx',import.meta.url),pix);commits.push({index,tick:Number(rt.logicalTick()),stepOffset:String(rt.displayCommitStepOffset(j)),stepSpan:String(rt.displayCommitStepSpan()),stepsPerSecond:rt.displayCommitStepsPerSecond(),execution:b(0x7acc),command:{kind:b(0x64cf),item:b(0x64d5),target:b(0x64ce)},flags:[b(0x7a8c),b(0x7a8e),b(0x7a90)],points:[0x7982,0x7984,0x7986,0x7988,0x798a,0x798c,0x798e,0x7990].map(w)});}}
  if(preparedLabels&&b(0x7acc)===1){const values=prepared();if(JSON.stringify(preparedPhases.at(-1)?.prepared)!==JSON.stringify(values))preparedPhases.push({tick:Number(rt.logicalTick()),execution:1,target:b(0x7ac0),prepared:values});}
 }},idle=n=>{for(let i=0;i<n;++i)tick(0);},key=mask=>{tick(mask);idle(10);};
 const hero=mem().slice(a(0x69c6)-1,a(0x69c6+148)-1);
 for(let i=0;i<config.party;++i)mem().set(hero,a(0x69c6+i*148)-1);
 // Retain the active hero's diagnostic properties; substitute only the
 // selected current-NPE character's name, pose images and animation IDs.
 if(config.character!==1){
  const r=characterRecord(config.character);if(r.length!==94)throw Error('character record');
  for(let i=0;i<config.party;++i){const o=i*148;
   mem().set(r.subarray(0,12),a(0x69c6+o)-1);putB(0x69d2+o,config.character);
   for(let j=0;j<8;++j)putW(0x69f4+o+2*j,r.readUInt16BE(19+2*j));
   for(let j=0;j<4;++j)putB(0x6a4d+o+j,r[83+j]);
  }
 }
 if(config.initializeMap)for(let i=0;i<config.party;++i){putW(0x6a14+148*i,config.maximum??30000);putW(0x6a16+148*i,config.hp[i]);}
 putB(0x7afa,config.party);putB(0x7626,config.emptyOuter||config.enemyHp[0]===0?0:51);putB(0x7627,config.emptyOuter||config.enemyHp[2]===0?0:51);putW(0x7ab4,1);idle(30);
 if(b(0x7afa)!==config.party)throw Error('native party entry count '+b(0x7afa));
 const originalCharacter=characterRecord(config.character);
 for(let i=0;i<config.party;++i){const o=i*148;
  if(b(0x64d9+o)!==originalCharacter[85]||b(0x64da+o)!==originalCharacter[86])throw Error('native casting group does not match current NPE');
  for(let j=0;j<8;++j)if(w(0x647e+o+2*j)!==originalCharacter.readUInt16BE(19+2*j))throw Error('native pose does not match current NPE');
 }
 for(let i=0;i<config.party+3;++i){const o=i*148;
  const hp=i<config.party?config.hp[i]:config.enemyHp[i-config.party];
  putW(0x64a0+o,hp);putW(0x649e+o,config.maximum??30000);putW(0x64a4+o,i<config.party?500:0);putW(0x64a2+o,i<config.party?999:0);
  putW(0x64a6+o,1);putW(0x64a8+o,i<config.party?(config.secondFast?[1,999,2][i]:999-i):1);putW(0x64aa+o,1);
  putW(0x64ac+o,i<config.party?config.attacks[i]:1);putW(0x64b0+o,i<config.party?config.attacks[i]:1);putW(0x64ae+o,i<config.party?0:config.defenses[i-config.party]);putW(0x64b2+o,i<config.party?0:config.defenses[i-config.party]);putB(0x6468+o,i<config.party?(config.secondFast&&i===1?10:99):10);
  for(let s=0;s<4;++s)putW(0x648e+o+s*2,0);
  if(i<config.party && config.poisonParty?.includes(i))putW(0x648e+o,-4);
  if(i>=config.party && config.poisonEnemy?.includes(i-config.party))putW(0x648e+o,-4);
  if(config.sleep&&i===2)putW(0x6490+o,-4);
  if(config.sleepSlot===i)putW(0x6490+o,-4);
  if(i>=config.party){putW(0x6492+o,-36);for(let s=0;s<3;++s)putB(0x6469+o+s,0);}
  if(enemySpell&&i===config.party+config.enemyActor){
   putW(0x64a6+o,config.mental??1);
   putW(0x6492+o,0);putW(0x64ac+o,120);putW(0x64b0+o,120);putW(0x64a8+o,config.enemyFast?1000:1);for(let k=0;k<3;++k)putB(0x6469+o+k,config.skill);
  }
 }
 if(preparedLabels)for(let i=0;i<config.party+3;++i)for(let j=0;j<6;++j)putW(0x64b6+i*148+2*j,i===0&&config.preparedWords?config.preparedWords[j]:100*(i+1)+j+1);
 // Current resources/firmware are intact; only isolated RAM words change.
 for(let i=0;i<255;++i)putW(0x67c8+2*i,0);
 putW(0x67c8+2*config.item,1);putB(0x7a9e,0);
 const events=[],state=label=>{const e={label,...(preparedLabels?{prepared:prepared()}:{}),mode:w(0x7934),actor:b(0x7a9e),menu:b(0x7ac8),row:b(0x7a9c),quantity:w(0x67c8+2*config.item),target:b(0x64ce),displayTarget:b(0x7ac0),action:b(0x64cf),execution:b(0x7acc),commands:Array.from({length:config.party+3},(_,i)=>({kind:b(0x64cf+i*148),item:b(0x64d5+i*148),target:b(0x64ce+i*148),flag:b(0x64e2+i*148),type:w(0x64d0+i*148),type2:w(0x64d2+i*148)})),
  records:Array.from({length:config.party+3},(_,i)=>({hp:w(0x64a0+i*148),maximum:w(0x649e+i*148),qi:w(0x64a4+i*148),mental:w(0x64a6+i*148),
   attack:w(0x64b0+i*148),defense:w(0x64b2+i*148),level:b(0x6468+i*148),experience:w(0x649c+i*148),
   attackTimer:w(0x6496+i*148),defenseTimer:w(0x6498+i*148),status:[0,1,2,3].map(s=>w(0x648e+i*148+s*2))}))};events.push(e);fs.writeFileSync(new URL(`../../tmp/native-${config.name}-${events.length-1}.idx`,import.meta.url),rt.framePixelIndices());console.log(JSON.stringify({name:config.name,...e}));};
 try{
  state('before');
  key(4);key(2);key(16); // flag menu
  for(let moves=0;b(0x7a9c)!==1&&moves<6;++moves)key(2);
  if(b(0x7a9c)!==1)throw Error('item command selection did not settle');
  key(16);key(16); // use/throw submenu -> original inventory
  if(b(0x7ac8)!==33)throw Error('original inventory gate');
  if(!config.group){
   key(16);
   if(b(0x7ac8)!==37)throw Error('original item target gate');
   for(let i=0;i<config.target;++i)key(2);
  }
  const active=Array.from({length:config.party},(_,i)=>i).filter(i=>config.hp[i]>0&&config.hp[i]<32768);
  state('command0');recording=active.length===1;
  key(16);idle(active.length===1?300:30);recording=false;
  state(active.length===1?'after':'queued0');
  if(active.length>1&&b(0x7a9e)!==active[1])throw Error('party magic queue gate');
  for(const actor of active.slice(1)){
   if(b(0x7ac8)!==30){key(4);key(2);}
   key(16);for(let moves=0;b(0x7a9c)!==2&&moves<6;++moves)key(2);
   if(b(0x7a9c)!==2)throw Error('party defend selection did not settle');
   if(b(0x7ac8)!==31||b(0x7a9e)!==actor)throw Error('party defend gate');
   state('command'+actor);recording=actor===active.at(-1);
   key(16);idle(300);recording=false;state(actor===active.at(-1)?'after':'queued'+actor);
  }
  fs.writeFileSync(new URL('../../tmp/native-'+config.name+'-commits.json',import.meta.url),JSON.stringify(commits,null,2));console.log('commits',commits.length);
  samples.push({config,events,...(preparedLabels?{preparedPhases}:{})});
 }catch(error){recording=false;rt.free();throw error;}
 putW(0x7934,0);
}
fs.writeFileSync(new URL('../../tmp/native-item-'+(preparedLabels?'prepared-labels':'animation')+'.json',import.meta.url),JSON.stringify(samples,null,2));rt.free();
