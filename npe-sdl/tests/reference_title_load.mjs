import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
async function main(){
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
let rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const tick=mask=>{const o=rt.advance(mask);o.free();};
const idle=n=>{for(let t=0;t<n;++t)tick(0);};
const boot=()=>{rt.setBootTarget('title');const o=rt.boot();o.free();idle(1000);};
const mem=()=>new Uint8Array(exp.memory.buffer);
let base;
const find=mode=>{const found=[];for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===mode&&mem()[i+0x7935]===0&&mem()[i+0x7aec]===0xa9&&mem()[i+0x7aeb]===6)found.push(i);if(found.length!==1)throw Error('live RAM '+JSON.stringify(found));base=found[0];};
const a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
const state=()=>({npc:Array.from({length:20},(_,j)=>{const o=0x6ca2+j*14;return{id:w(o),direction:b(o+8),x:b(o+9),y:b(o+10),moving:b(o+11)};}),base,mode:w(0x7934),event:w(0x7936),map:w(0x7a58),progress:w(0x7a66),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),money:(w(0x799e)*65536+w(0x79a0))>>>0,redraw:w(0x7ab4)});
const reload=process.argv.includes('--load-only'),prefix=reload?'native-title-reload':'native-title-load';
const stages=[];
const key=(mask,label,ticks=32)=>{const commits=[];for(let t=0;t<ticks;++t){tick(t===0?mask:0);for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){const index=commits.length;fs.writeFileSync(`tmp/${prefix}-${label}-commit-${index}.idx`,p);commits.push({index,tick:t,state:state()});}}}const s={label,mask,commits,after:state()};stages.push(s);console.log(JSON.stringify(s));return s;};
let cold,beforeSave,saved;
if(!reload){
 boot();find(7);cold=state();console.log('cold',JSON.stringify(cold));
 key(16,'new',64);beforeSave=state();fs.writeFileSync('tmp/native-title-load-before-save.idx',rt.framePixelIndices());
 let i=0;for(const mask of [64,2,2,2,16,2,16,16])key(mask,'save-'+i++);
 saved=state();fs.writeFileSync('tmp/native-title-load-saved.snapshot',rt.snapshotBytes());
 const persistent=rt.persistentDataBytes();fs.writeFileSync('tmp/native-title-load-persistent.bin',persistent);rt.free();
 rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');rt.installPersistentDataBytes(persistent);
}else{
 const prior=JSON.parse(fs.readFileSync('tmp/native-title-load.json','utf8'));({cold,beforeSave,saved}=prior);
 saved.npc=JSON.parse(fs.readFileSync('tmp/native-title-load-npc.json','utf8'))[0].npc;
 rt.installPersistentDataBytes(fs.readFileSync('tmp/native-title-load-persistent.bin'));
}
boot();find(7);const reboot=state();console.log('reboot',JSON.stringify(reboot));
key(2,'title-down');key(16,'slots');key(16,'load',192);
fs.writeFileSync(`tmp/${prefix}-after.idx`,rt.framePixelIndices());
fs.writeFileSync(`tmp/${prefix}-after.snapshot`,rt.snapshotBytes());
fs.writeFileSync(`tmp/${prefix}.json`,JSON.stringify({cold,beforeSave,saved,reboot,stages,after:state()},null,2)+'\n');
rt.free();
}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
