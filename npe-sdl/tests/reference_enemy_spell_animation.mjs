import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
// Independent native RAM fixtures. Original NPE/PRG and continuous saves stay intact.
// Capture every display commit, including repeated base restoration frames.
const repeated=process.argv.includes('--repeat');
const normalAttack=process.argv.includes('--attack');
const attackSequence=process.argv.includes('--attack-sequence');
const attackDefend=process.argv.includes('--attack-defend');
const defendExtra=process.argv.includes('--attack-defend-extra');
const groupAttack=process.argv.includes('--group-attack');
const enemyAttack=process.argv.includes('--enemy-attack');
const enemyExtra=false;
const enemySpell=true;
let configs=Array.from({length:12},(_,i)=>({
 name:'enemy-spell-heal-attempt-'+i, party:1+i%3, enemyActor:Math.floor(i/3)%3,
 hp:[30000,30000,30000],attacks:[90,120,150],enemyHp:[20000,20000,20000],
 defenses:[0,3,90],actions:Array(1+i%3).fill('defend'),skill:6
}));
const healExtra=process.argv.includes('--heal-extra');
const hostileExtra=process.argv.includes('--hostile-extra');
const statusExtra=process.argv.includes('--status-extra');
const poisonExtra=process.argv.includes('--poison-extra');
const preparedRecords=process.argv.includes('--prepared-records');
if(healExtra)configs=[
 {name:'enemy-spell-group-heal',skill:9,enemyHp:[20000,29950,20000]},
 {name:'enemy-spell-heal-cap',skill:6,enemyHp:[20000,29950,20000]},
 {name:'enemy-spell-group-heal-fallen',skill:9,enemyHp:[0,29950,20000]}
].map(c=>({party:3,enemyActor:1,hp:[30000,30000,30000],attacks:[90,120,150],
 defenses:[0,3,90],actions:['defend','defend','defend'],...c}));
if(hostileExtra)configs=[
 {name:'enemy-spell-single-damage',skill:4},
 {name:'enemy-spell-group-damage',skill:10},
 {name:'enemy-spell-single-fallen',skill:4,hp:[30000,30,30000],initializeMap:true}
].map(c=>({party:3,enemyActor:1,hp:[30000,30000,30000],attacks:[90,120,150],
 enemyHp:[20000,20000,20000],defenses:[0,3,90],mental:10,
 actions:['defend','defend','defend'],...c}));
if(statusExtra)configs=[
 {name:'enemy-spell-single-status',skill:20},
 {name:'enemy-spell-group-status',skill:33},
 {name:'enemy-spell-own-status',skill:40}
].map(c=>({party:3,enemyActor:1,hp:[30000,30000,30000],attacks:[90,120,150],
 enemyHp:[20000,20000,20000],defenses:[0,3,90],mental:10,
 actions:['defend','defend','defend'],...c}));
if(poisonExtra)configs=[
 {name:'enemy-spell-enemy-poison',poisonEnemy:[0,2]},
 {name:'enemy-spell-both-poison',poisonEnemy:[2],poisonParty:[0,2]},
 {name:'enemy-spell-poison-fallen',poisonParty:[1],hp:[30000,10,30000],initializeMap:true}
].map(c=>({party:3,enemyActor:1,skill:20,hp:[30000,30000,30000],attacks:[90,120,150],
 enemyHp:[20000,20000,20000],defenses:[0,3,90],mental:10,
 actions:['defend','defend','defend'],...c}));
if(preparedRecords)configs=[
 {name:'enemy-prepared-attack',skill:0},
 {name:'enemy-prepared-attack-one-party',skill:0,party:1,enemyActor:2},
 {name:'enemy-prepared-single-damage',skill:4},
 {name:'enemy-prepared-group-damage',skill:10},
 {name:'enemy-prepared-single-heal',skill:6},
 {name:'enemy-prepared-group-heal',skill:9},
 {name:'enemy-prepared-single-status',skill:20},
 {name:'enemy-prepared-group-status',skill:33},
 {name:'enemy-prepared-own-status',skill:40}
].map(c=>({party:3,enemyActor:1,hp:[30000,30000,30000],attacks:[90,120,150],
 enemyHp:[20000,20000,20000],defenses:[0,3,90],mental:10,
 actions:Array(c.party??3).fill('defend'),...c}));
let recording=false,commits=[];
const preparedCase=process.argv.find(v=>v.startsWith('--prepared-case='))?.split('=')[1];
if(preparedCase){if(!preparedRecords||!configs.some(c=>c.name===preparedCase))throw Error('prepared case');configs=configs.filter(c=>c.name===preparedCase);}
const exp=await initWasm({module_or_path:fs.readFileSync(new URL('../../tmp/opennoah_wasm_bg.wasm',import.meta.url))});
const rt=new GameRuntime(fs.readFileSync(new URL('../../tmp/tianjiang-online.opennoah',import.meta.url)),'certified_original_v1');
const samples=[];
for(const config of configs){
 commits=[];
 const preparedPhases=[];
 rt.restoreBytes(fs.readFileSync(new URL('../../tmp/battle-fresh-start.bin',import.meta.url)));
 const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
 for(let i=0;i+0x8000<mem().length;i++)if(mem()[i+0x7934]===3&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7a58]===9&&mem()[i+0x7a66]===6)found.push(i);
 if(found.length!==1)throw Error('RAM '+found.length);
 const base=found[0],a=o=>base+o-(o<0x7000?8:0),w=o=>mem()[a(o)-1]<<8|mem()[a(o)],b=o=>mem()[a(o)-1];
 const putW=(o,v)=>{mem()[a(o)-1]=(v&65535)>>>8;mem()[a(o)]=v&255;},putB=(o,v)=>mem()[a(o)-1]=v;
 const tick=mask=>{const out=rt.advance(mask);out.free();if(recording){for(let j=0;j<rt.displayCommitCount();++j){let pix=rt.displayCommitPixelIndices(j);if(pix){let index=commits.length;fs.writeFileSync(new URL('../../tmp/native-'+config.name+'-commit-'+index+'.idx',import.meta.url),pix);commits.push({index,tick:Number(rt.logicalTick()),stepOffset:String(rt.displayCommitStepOffset(j)),stepSpan:String(rt.displayCommitStepSpan()),stepsPerSecond:rt.displayCommitStepsPerSecond(),execution:b(0x7acc),flags:[b(0x7a8c),b(0x7a8e),b(0x7a90)],points:[0x7982,0x7984,0x7986,0x7988,0x798a,0x798c,0x798e,0x7990].map(w)});}}
  if(preparedRecords&&b(0x7acc)===config.party+config.enemyActor+1){
   const prepared=Array.from({length:config.party+3},(_,i)=>Array.from({length:12},(_,j)=>b(0x64b6+i*148+j)));
   if(JSON.stringify(preparedPhases.at(-1)?.prepared)!==JSON.stringify(prepared))preparedPhases.push({tick:Number(rt.logicalTick()),execution:b(0x7acc),target:b(0x7ac0),prepared});
  }
 }},idle=n=>{for(let i=0;i<n;++i)tick(0);},key=mask=>{tick(mask);idle(10);};
 const hero=mem().slice(a(0x69c6)-1,a(0x69c6+148)-1);
 for(let i=1;i<config.party;++i)mem().set(hero,a(0x69c6+i*148)-1);
 if(config.initializeMap)for(let i=0;i<config.party;++i){putW(0x6a14+148*i,1000);putW(0x6a16+148*i,config.hp[i]);}
 putB(0x7afa,config.party);putB(0x7626,config.emptyOuter||config.enemyHp[0]===0?0:51);putB(0x7627,config.emptyOuter||config.enemyHp[2]===0?0:51);putW(0x7ab4,1);idle(30);
 for(let i=0;i<config.party+3;++i){const o=i*148;
  const hp=i<config.party?config.hp[i]:config.enemyHp[i-config.party];
  putW(0x64a0+o,hp);putW(0x649e+o,30000);putW(0x64a4+o,i<config.party?500:0);
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
 // Current resources/firmware are intact; only isolated RAM words change.
 putB(0x69de,99);putB(0x69df,6);putB(0x69e0,0);putB(0x7a9e,0);
 if(preparedRecords)for(let i=0;i<config.party+3;++i)for(let j=0;j<6;++j)putW(0x64b6+i*148+j*2,100*(i+1)+j+1);
 const events=[],state=label=>{const e={label,mode:w(0x7934),actor:b(0x7a9e),menu:b(0x7ac8),row:b(0x7a9c),target:b(0x64ce),displayTarget:b(0x7ac0),action:b(0x64cf),execution:b(0x7acc),
  prepared:preparedRecords?Array.from({length:config.party+3},(_,i)=>Array.from({length:12},(_,j)=>b(0x64b6+i*148+j))):undefined,
  commands:Array.from({length:config.party+3},(_,i)=>({kind:b(0x64cf+i*148),skill:b(0x64d6+i*148),target:b(0x64ce+i*148),flag:b(0x64e2+i*148),type:w(0x64d0+i*148),type2:w(0x64d2+i*148)})),
  records:Array.from({length:config.party+3},(_,i)=>({hp:w(0x64a0+i*148),maximum:w(0x649e+i*148),qi:w(0x64a4+i*148),mental:w(0x64a6+i*148),
   attack:w(0x64b0+i*148),defense:w(0x64b2+i*148),level:b(0x6468+i*148),experience:w(0x649c+i*148),
   attackTimer:w(0x6496+i*148),defenseTimer:w(0x6498+i*148),status:[0,1,2,3].map(s=>w(0x648e+i*148+s*2))}))};events.push(e);fs.writeFileSync(new URL(`../../tmp/native-${config.name}-${events.length-1}.idx`,import.meta.url),rt.framePixelIndices());console.log(JSON.stringify({name:config.name,...e}));};
 try{
  state('before');
  if(groupAttack){
   if(config.queuedDefend){
    key(4);key(2);key(16);key(2);key(2);state('defend');key(16);state('queued');
    if(b(0x7a9e)!==1||b(0x7ac8)!==30)throw Error('group queued gate');
    key(16);while(b(0x7a9c)!==0)key(1);
   }else{key(4);key(2);key(16);}
   if(b(0x7ac8)!==31||b(0x7a9c)!==0)throw Error('group menu gate');
   state('group');recording=true;key(16);idle(300);recording=false;state('after');
  }else if(enemySpell||attackDefend||defendExtra||enemyAttack||enemyExtra){
   for(let actor=0;actor<config.party;++actor){
    if(config.actions[actor]==='defend'){
     if(b(0x7ac8)!==30){key(4);key(2);}
     key(16);while(b(0x7a9c)!==2)key(2);
     if(b(0x7ac8)!==31)throw Error('defend menu gate');
    }else{
     if(b(0x7ac8)===30){key(1);key(8);}
     if(enemyExtra&&config.party===1&&config.emptyOuter){
      state('command'+actor);recording=true;key(16);idle(300);recording=false;state('after');break;
     }
     key(16);if(b(0x7ac8)!==17)throw Error('attack target gate');
     for(let step=0;step<(config.attackTargets?.[actor]??0);++step)key(2);
    }
    state('command'+actor);
    if(b(0x7a9e)!==actor)throw Error('mixed actor gate');
    recording=actor===config.party-1;key(16);idle(300);recording=false;
    state(actor===config.party-1?'after':'queued'+actor);
   }
  }else if(attackSequence){
   for(let actor=0;actor<config.party;++actor){
    key(16);state('target'+actor);
    if(b(0x7ac8)!==17||b(0x7a9e)!==actor)throw Error('sequence target gate');
    recording=actor===config.party-1;key(16);idle(300);recording=false;
    state(actor===config.party-1?'after':'queued'+actor);
   }
  }else
  for(let round=0;round<(config.rounds??1);++round){
   const direct=normalAttack&&config.emptyOuter;
   if(direct)recording=true;
   key(normalAttack?16:2);state(normalAttack?(direct?'direct':'target'):'icon');
   if(!normalAttack&&b(0x7ac8)!==40)throw Error('icon gate');
   if(normalAttack&&!direct&&b(0x7ac8)!==17)throw Error('target gate');
   if(!direct){recording=true;key(16);}
   idle(300);recording=false;if(commits.length<(normalAttack?1:28)*(round+1))throw Error('incomplete animation commits');state('after');
  }
  fs.writeFileSync(new URL('../../tmp/native-'+config.name+'-commits.json',import.meta.url),JSON.stringify(commits,null,2));console.log('commits',commits.length);
  samples.push({config,events,...(preparedRecords?{preparedPhases}:{})});
 }catch(error){recording=false;rt.free();throw error;}
 putW(0x7934,0);
}
const output=new URL('../../tmp/'+(preparedRecords?'native-enemy-prepared-records.json':poisonExtra?'native-enemy-spell-poison.json':statusExtra?'native-enemy-spell-status.json':hostileExtra?'native-enemy-spell-hostile.json':healExtra?'native-enemy-spell-extra.json':enemySpell?'native-enemy-spell-attempts.json':enemyExtra?'native-enemy-animation-extra.json':enemyAttack?'native-enemy-animation.json':groupAttack?'native-group-animation.json':defendExtra?'native-attack-defend-extra.json':attackDefend?'native-attack-defend.json':attackSequence?'native-attack-sequence.json':normalAttack?'native-attack-animation.json':repeated?'native-combine-repeat.json':'native-combine-animation.json'),import.meta.url);
const rows=preparedCase?JSON.parse(fs.readFileSync(output)).filter(r=>r.config.name!==preparedCase).concat(samples):samples;
fs.writeFileSync(output,JSON.stringify(rows,null,2));rt.free();
