import fs from 'node:fs';import{GameRuntime,initWasm}from'../../tmp/opennoah-headless.mjs';
await initWasm({module_or_path:fs.readFileSync(new URL('../../tmp/opennoah_wasm_bg.wasm',import.meta.url))});
const rt=new GameRuntime(fs.readFileSync(new URL('../../tmp/tianjiang-online.opennoah',import.meta.url)),'certified_original_v1');
// The real boot prepares resources needed by the new-game constructor.
// A battle snapshot with only game_mode patched to title is invalid here.
rt.setBootTarget('title');const output=rt.boot();output.free();
const tick=mask=>{const out=rt.advance(mask);out.free();};
for(let i=0;i<1000;++i)tick(0);
fs.writeFileSync(new URL('../../tmp/empty-title-cold-before.idx',import.meta.url),rt.framePixelIndices());
fs.writeFileSync(new URL('../../tmp/empty-title-cold-snapshot.bin',import.meta.url),rt.snapshotBytes());
for(const mask of[2,16]){tick(mask);for(let i=0;i<15;++i)tick(0);}
const commits=[];
for(let i=0;i<136;++i){
 tick(i===0?16:0);
 for(let j=0;j<rt.displayCommitCount();++j){
  const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
  const index=commits.length;
  fs.writeFileSync(new URL(`../../tmp/native-empty-title-map-commit-${index}.idx`,import.meta.url),pixels);
  commits.push({index,tick:i});
 }
}
fs.writeFileSync(new URL('../../tmp/empty-title-cold-after.idx',import.meta.url),rt.framePixelIndices());
const audio=JSON.parse(new TextDecoder().decode(rt.drainEventsBytes())).filter(e=>e.type==='audio');
fs.writeFileSync(new URL('../../tmp/native-empty-title.json',import.meta.url),JSON.stringify({keys:[2,16,16],audio,commits},null,2)+'\n');
console.log('Real original boot, title empty first slot and final map captured');rt.free();
