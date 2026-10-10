import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
// Independent native RAM fixtures. Original NPE/PRG and continuous saves stay intact.
// Capture every display commit, including repeated base restoration frames.
const enemySpell=false;
const extra=process.argv.includes('--extra');
const preparedLabels=process.argv.includes('--prepared-labels');
let configs=(preparedLabels?[
 {name:'party-prepared-group-status',skill:33,group:true,own:false,target:0,enemyPoison:[0,10,0]},
 {name:'party-prepared-group-status-cure',skill:33,group:true,own:false,target:0,enemyPoison:[6,0,0]},
 {name:'party-prepared-group-mixed-status',skill:30,group:true,own:false,target:0,character:3},
 {name:'party-prepared-own-immunity',skill:40,group:false,own:true,target:1,character:2},
 {name:'party-prepared-group-buff',skill:14,group:true,own:true,target:0,character:2},
 {name:'party-prepared-group-status-two-party',skill:33,group:true,own:false,target:0,party:2,preparedWords:[101,0,65535,104,105,106]}
]:extra?[
 {name:'party-spell-group-heal-character2',skill:9,group:true,own:true,target:0,character:2},
 {name:'party-spell-single-status-character3',skill:20,group:false,own:false,target:2,character:3},
 {name:'party-spell-own-status-character2',skill:40,group:false,own:true,target:1,character:2}
]:[
 {name:'party-spell-single-damage',skill:4,group:false,own:false,target:0},
 {name:'party-spell-group-damage',skill:10,group:true,own:false,target:1},
 {name:'party-spell-single-heal',skill:6,group:false,own:true,target:1}
]).map(c=>({party:3,character:1,enemyActor:1,hp:[20000,20000,20000],attacks:[90,120,150],
 enemyHp:[20000,20000,20000],defenses:[0,3,90],...c}));
const preparedCase=process.argv.find(v=>v.startsWith('--prepared-case='))?.split('=')[1];
if(preparedCase){if(!preparedLabels||!configs.some(c=>c.name===preparedCase))throw Error('prepared label case');configs=configs.filter(c=>c.name===preparedCase);}
let recording=false,commits=[];
const exp=await initWasm({module_or_path:fs.readFileSync(new URL('../../tmp/opennoah_wasm_bg.wasm',import.meta.url))});
const rt=new GameRuntime(fs.readFileSync(new URL('../../tmp/tianjiang-online.opennoah',import.meta.url)),'certified_original_v1');
const samples=[];
const npe=fs.readFileSync(new URL('../../tmp/tjfy-world-probe.npe',import.meta.url));
const characterSection=npe.readUInt32BE(0x3a+12*8);
const characterSize=npe.readUInt32BE(0x3a+12*8+4);
const characterRecord=id=>{const end=npe.readUInt32BE(characterSection+id*4);return npe.subarray(characterSection+npe.readUInt32BE(characterSection+(id-1)*4),characterSection+(end===0xffffffff?characterSize:end));};
for(const config of configs){
 commits=[];
 const preparedPhases=[];
 rt.restoreBytes(fs.readFileSync(new URL('../../tmp/battle-fresh-start.bin',import.meta.url)));
 const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
 for(let i=0;i+0x8000<mem().length;i++)if(mem()[i+0x7934]===3&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7a58]===9&&mem()[i+0x7a66]===6)found.push(i);
 if(found.length!==1)throw Error('RAM '+found.length);
 const base=found[0],a=o=>base+o-(o<0x7000?8:0),w=o=>mem()[a(o)-1]<<8|mem()[a(o)],b=o=>mem()[a(o)-1];
 const putW=(o,v)=>{mem()[a(o)-1]=(v&65535)>>>8;mem()[a(o)]=v&255;},putB=(o,v)=>mem()[a(o)-1]=v;
 const prepared=()=>Array.from({length:config.party+3},(_,i)=>Array.from({length:12},(_,j)=>b(0x64b6+i*148+j)));
 const tick=mask=>{const out=rt.advance(mask);out.free();if(recording){for(let j=0;j<rt.displayCommitCount();++j){let pix=rt.displayCommitPixelIndices(j);if(pix){let index=commits.length;fs.writeFileSync(new URL('../../tmp/native-'+config.name+'-commit-'+index+'.idx',import.meta.url),pix);commits.push({index,tick:Number(rt.logicalTick()),stepOffset:String(rt.displayCommitStepOffset(j)),stepSpan:String(rt.displayCommitStepSpan()),stepsPerSecond:rt.displayCommitStepsPerSecond(),execution:b(0x7acc),...(preparedLabels?{displayTarget:b(0x7ac0),kinds:Array.from({length:config.party+3},(_,i)=>b(0x64cf+i*148)),types:Array.from({length:config.party+3},(_,i)=>b(0x64d0+i*148))}:{}),flags:[b(0x7a8c),b(0x7a8e),b(0x7a90)],points:[0x7982,0x7984,0x7986,0x7988,0x798a,0x798c,0x798e,0x7990].map(w)});}}
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
 if(config.initializeMap)for(let i=0;i<config.party;++i){putW(0x6a14+148*i,1000);putW(0x6a16+148*i,config.hp[i]);}
 putB(0x7afa,config.party);putB(0x7626,config.emptyOuter||config.enemyHp[0]===0?0:51);putB(0x7627,config.emptyOuter||config.enemyHp[2]===0?0:51);putW(0x7ab4,1);idle(30);
 if(b(0x7afa)!==config.party)throw Error('native party entry count '+b(0x7afa));
 const originalCharacter=characterRecord(config.character);
 for(let i=0;i<config.party;++i){const o=i*148;
  if(b(0x64d9+o)!==originalCharacter[85]||b(0x64da+o)!==originalCharacter[86])throw Error('native casting group does not match current NPE');
  for(let j=0;j<8;++j)if(w(0x647e+o+2*j)!==originalCharacter.readUInt16BE(19+2*j))throw Error('native pose does not match current NPE');
 }
 for(let i=0;i<config.party+3;++i){const o=i*148;
  const hp=i<config.party?config.hp[i]:config.enemyHp[i-config.party];
  putW(0x64a0+o,hp);putW(0x649e+o,30000);putW(0x64a4+o,i<config.party?500:0);
  putW(0x64a6+o,1);putW(0x64a8+o,i<config.party?(config.secondFast?[1,999,2][i]:999-i):1);putW(0x64aa+o,1);
  putW(0x64ac+o,i<config.party?config.attacks[i]:1);putW(0x64b0+o,i<config.party?config.attacks[i]:1);putW(0x64ae+o,i<config.party?0:config.defenses[i-config.party]);putW(0x64b2+o,i<config.party?0:config.defenses[i-config.party]);putB(0x6468+o,i<config.party?(config.secondFast&&i===1?10:99):10);
  for(let s=0;s<4;++s)putW(0x648e+o+s*2,0);
  if(i<config.party && config.poisonParty?.includes(i))putW(0x648e+o,-4);
  if(i>=config.party && config.poisonEnemy?.includes(i-config.party))putW(0x648e+o,-4);
  if(i>=config.party && config.enemyPoison)putW(0x648e+o,config.enemyPoison[i-config.party]);
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
 putB(0x69de,99);putB(0x69df,config.skill);putB(0x69e0,0);putB(0x7a9e,0);
 const events=[],state=label=>{const e={label,...(preparedLabels?{prepared:prepared()}:{}),mode:w(0x7934),actor:b(0x7a9e),menu:b(0x7ac8),row:b(0x7a9c),target:b(0x64ce),displayTarget:b(0x7ac0),action:b(0x64cf),execution:b(0x7acc),commands:Array.from({length:config.party+3},(_,i)=>({kind:b(0x64cf+i*148),skill:b(0x64d6+i*148),target:b(0x64ce+i*148),flag:b(0x64e2+i*148),type:w(0x64d0+i*148),type2:w(0x64d2+i*148)})),
  records:Array.from({length:config.party+3},(_,i)=>({hp:w(0x64a0+i*148),maximum:w(0x649e+i*148),qi:w(0x64a4+i*148),mental:w(0x64a6+i*148),
   attack:w(0x64b0+i*148),defense:w(0x64b2+i*148),level:b(0x6468+i*148),experience:w(0x649c+i*148),
   attackTimer:w(0x6496+i*148),defenseTimer:w(0x6498+i*148),status:[0,1,2,3].map(s=>w(0x648e+i*148+s*2))}))};events.push(e);fs.writeFileSync(new URL(`../../tmp/native-${config.name}-${events.length-1}.idx`,import.meta.url),rt.framePixelIndices());console.log(JSON.stringify({name:config.name,...e}));};
 try{
  state('before');
  key(4);key(1);key(16); // original magic icon/list
  console.log('list gate',config.name,b(0x7ac8),b(0x7a9e));
  if(!config.group){
   key(16);console.log('target gate',config.name,b(0x7ac8),b(0x7a9e));
   for(let i=0;i<config.target;++i)key(2);
  }
  state('command0');key(16);idle(30);state('queued0');
  if(b(0x7a9e)!==1)throw Error('party magic queue gate');
  for(let actor=1;actor<config.party;++actor){
   if(b(0x7ac8)!==30){key(4);key(2);}
   key(16);for(let moves=0;b(0x7a9c)!==2&&moves<6;++moves)key(2);
   if(b(0x7a9c)!==2)throw Error('party defend selection did not settle');
   if(b(0x7ac8)!==31||b(0x7a9e)!==actor)throw Error('party defend gate');
   state('command'+actor);recording=actor===config.party-1;
   key(16);idle(300);recording=false;state(actor===config.party-1?'after':'queued'+actor);
  }
  fs.writeFileSync(new URL('../../tmp/native-'+config.name+'-commits.json',import.meta.url),JSON.stringify(commits,null,2));console.log('commits',commits.length);
  samples.push({config,events,...(preparedLabels?{preparedPhases}:{})});
 }catch(error){recording=false;rt.free();throw error;}
 putW(0x7934,0);
}
const output=new URL('../../tmp/native-party-spell-'+(preparedLabels?'prepared-labels':extra?'extra':'animation')+'.json',import.meta.url);
const rows=preparedCase&&fs.existsSync(output)?JSON.parse(fs.readFileSync(output)).filter(r=>r.config.name!==preparedCase).concat(samples):samples;
fs.writeFileSync(output,JSON.stringify(rows,null,2));rt.free();
