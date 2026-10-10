import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const variant=process.argv[2]||'fallen',party=Number(process.argv[3]||3),loaded=Number(process.argv[4]||3),enemies=3;
const enemyMask=2,markedEnemy=1;
const exp=await initWasm({module_or_path:fs.readFileSync(new URL('../../tmp/opennoah_wasm_bg.wasm',import.meta.url))});
const rt=new GameRuntime(fs.readFileSync(new URL('../../tmp/tianjiang-online.opennoah',import.meta.url)),'certified_original_v1');
rt.restoreBytes(fs.readFileSync(new URL('../../tmp/battle-fresh-start.bin',import.meta.url)));
const mem=()=>new Uint8Array(exp.memory.buffer);let found=[];
for(let i=0;i+0x8000<mem().length;i++)if(mem()[i+0x7934]===3&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7a58]===9&&mem()[i+0x7a66]===6)found.push(i);
if(found.length!==1)throw Error('RAM');const base=found[0],a=o=>base+o-(o<0x7000?8:0);
const w=o=>mem()[a(o)-1]<<8|mem()[a(o)],b=o=>mem()[a(o)-1];
const putW=(o,v)=>{mem()[a(o)-1]=(v&65535)>>>8;mem()[a(o)]=v&255;},putB=(o,v)=>mem()[a(o)-1]=v;
const tick=k=>{const o=rt.advance(k);o.free();},idle=n=>{for(let i=0;i<n;++i)tick(0);},key=k=>{tick(k);idle(10);};
const hero=mem().slice(a(0x69c6)-1,a(0x69c6+148)-1);
for(let i=1;i<party;++i)mem().set(hero,a(0x69c6+i*148)-1);
// Formation key at7625 is also middle ID; source39:AFD8/B01E/B064
// reads physical slots in order7626,7625,7627. Keep the real key51.
putB(0x7625,51);putB(0x7626,enemyMask&1?51:0);putB(0x7627,enemyMask&4?51:0);
putB(0x7afa,party);putW(0x7ab4,1);idle(30);
if(b(0x7af0)!==3)throw Error('scripted physical slots');
// Independent RAM fixture; original entry loads all image handles/positions.
for(let i=0;i<party+enemies;++i){const o=148*i;
 putW(0x649e+o,i<party?1000:10000);putW(0x64a0+o,i<party?500:10000);
 putW(0x64a2+o,i<party?999:300);putW(0x64a4+o,i<party?500:200);putW(0x64a8+o,i<party?999-i:0);
 for(let s=0;s<4;++s)putW(0x648e+o+2*s,0);
 if(i>=party&&!(enemyMask&(1<<(i-party)))){putW(0x64a0+o,0);continue;}
 if(i>=party)putW(0x6492+o,-36);
 if((i===party-1&&i!==0)||i===party+markedEnemy){
  for(let s=0;s<4;++s)if(variant==='all'||variant==='fallen'||variant==='positive'||variant==='single'||Number(variant)===s)putW(0x648e+o+2*s,variant==='positive'?5:-5);
 }
 if(variant==='single'&&i===0){putW(0x648e+o,-5);putW(0x6494+o,-5);}
 if(variant==='fallen'&&i===party-1&&i!==0)putW(0x64a0+o,0);
 if(variant==='enemy-fallen'&&i===party){for(let s=0;s<4;++s)putW(0x648e+o+2*s,-5);putW(0x64a0+o,0);}
 if(variant==='positive'&&i===party+markedEnemy)putW(0x6490+o,-36);
 if(variant==='enemy-negative'&&i===party){for(let s=0;s<4;++s)putW(0x648e+o+2*s,-5);putW(0x64a0+o,32768);}
}
const records=()=>Array.from({length:party+enemies},(_,i)=>({hp:w(0x64a0+i*148),qi:w(0x64a4+i*148),status:[0,1,2,3].map(s=>w(0x648e+i*148+2*s)),x:b(0x64cc+i*148),y:b(0x64cd+i*148)}));
const entry=records();
function defend(){if(b(0x7ac8)===10){key(4);key(2);}else if(b(0x7ac8)===20)key(2);
 if(b(0x7ac8)!==30)throw Error('flag');key(16);
 for(let i=0;i<6&&b(0x7a9c)!==2;++i)key(b(0x7a9c)<2?2:1);key(16);}
for(let i=0;i<party;++i){if(w(0x64a0+i*148)===0||w(0x6490+i*148)>32767||w(0x6492+i*148)>32767)continue;
 if(b(0x7a9e)!==i)throw Error('actor '+b(0x7a9e)+' expected '+i);defend();}
idle(700);if(w(0x7934)!==3)throw Error('mode');
const prefix=`native-fallen-empty-defend-${variant}-${party}-${loaded}`;
fs.writeFileSync(new URL(`./${prefix}.idx`,import.meta.url),rt.framePixelIndices());
fs.writeFileSync(new URL(`./${prefix}.json`,import.meta.url),JSON.stringify({variant,party,enemies,loaded,enemyMask,entry,records:records(),actor:b(0x7a9e),menu:b(0x7ac8)},null,2));
console.log(JSON.stringify({prefix,records:records(),actor:b(0x7a9e),menu:b(0x7ac8)}));rt.free();

