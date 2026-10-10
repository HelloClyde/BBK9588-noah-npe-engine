import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const out=[];
for(const name of ['saved','after']){
 rt.restoreBytes(fs.readFileSync(`tmp/native-title-load-${name}.snapshot`));
 const m=new Uint8Array(exp.memory.buffer),matches=[];
 for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===1&&m[i+0x7935]===0&&m[i+0x7a58]===1&&m[i+0x7a66]===1&&m[i+0x69d5-9]===7&&m[i+0x69d6-9]===1)matches.push(i);
 const states=matches.map(base=>{const a=o=>base+o-(o<0x7000?8:0),b=o=>m[a(o)-1],w=o=>b(o)*256+m[a(o)];return {base,x:b(0x7a2e)+b(0x69d5),npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),direction:b(o+8),x:b(o+9),y:b(o+10),moving:b(o+11)};})};});
 if(!states.length)throw Error('no RAM '+name);
 for(let t=0;t<48;++t){const o=rt.advance(t===0?8:0);o.free();}
 const now=new Uint8Array(exp.memory.buffer);
 const live=states.filter(s=>now[s.base+0x7a2e-1]+now[s.base+0x69d5-9]!==s.x);
 if(live.length!==1)throw Error('live RAM movement '+name+' '+live.length);
 out.push({name,bases:matches,liveBase:live[0].base,npc:live[0].npc});
}
fs.writeFileSync('tmp/native-title-load-npc.json',JSON.stringify(out,null,2)+'\n');console.log(JSON.stringify(out));rt.free();
