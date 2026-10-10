import fs from 'node:fs';
import crypto from 'node:crypto';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
async function main(){
 await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
 const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
 const ids=Array.from(rt.audioTrackIds()),assets=Array.from(rt.audioTrackAssetIndices());
 console.log(JSON.stringify({ids,assets}));
 const rows=[];
 for(let i=0;i<assets.length;++i){const bytes=rt.audioAssetBytes(assets[i]);
  fs.writeFileSync(`tmp/frontend-audio-${ids[i]}.bin`,bytes);
  rows.push({track:ids[i],asset:assets[i],length:bytes.length,head:Buffer.from(bytes).subarray(0,32).toString('hex'),sha256:crypto.createHash('sha256').update(bytes).digest('hex')});
 }
 fs.writeFileSync('tmp/frontend-audio-assets.json',JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify(rows.map(r=>({track:r.track,length:r.length,head:r.head}))));rt.free();
}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
