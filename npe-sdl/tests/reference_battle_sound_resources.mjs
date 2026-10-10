import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync(new URL('../../tmp/opennoah_wasm_bg.wasm',import.meta.url))});
const rt=new GameRuntime(fs.readFileSync(new URL('../../tmp/tianjiang-online.opennoah',import.meta.url)),'certified_original_v1');
try{
    rt.restoreBytes(fs.readFileSync(new URL('../../tmp/battle-fresh-start.bin',import.meta.url)));
    const mem=new Uint8Array(exp.memory.buffer),found=[];
    for(let i=0;i+0x8000<mem.length;++i)
        if(mem[i+0x7934]===3 && mem[i+0x7935]===0 && mem[i+0x7936]===1 && mem[i+0x7a58]===9 && mem[i+0x7a66]===6)found.push(i);
    if(found.length!==1)throw Error('RAM '+found.length);
    const base=found[0],word=o=>mem[base+o-1]*256+mem[base+o],far=o=>word(o)*65536+word(o+2);
    const npe=fs.readFileSync(new URL('../../tmp/tjfy-world-probe.npe',import.meta.url));
    const addresses=[0x794a,0x7972,0x796a,0x7962,0x795a,0x7952,0x7942,0x793a];
    const rows=addresses.map((a,i)=>({section:30+i,sizeAddress:a,offsetAddress:a+4,
        size:far(a),offset:far(a+4),expectedSize:npe.readUInt32BE(0x3e+8*(30+i)),
        expectedOffset:npe.readUInt32BE(0x3a+8*(30+i))}));
    for(const r of rows)r.matches=r.size===r.expectedSize && r.offset===r.expectedOffset;
    fs.writeFileSync(new URL('../../tmp/native-battle-sound-resources.json',import.meta.url),JSON.stringify(rows,null,2)+'\n');
    console.log(JSON.stringify(rows));
    if(rows.some(r=>!r.matches))throw Error('native battle voice directory mismatch');
}finally{rt.free();}
