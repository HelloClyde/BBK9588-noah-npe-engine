import fs from 'node:fs';
import {GameRuntime, initWasm} from '../../tmp/opennoah-headless.mjs';

// Read-only evidence from the original loaded game. No NPE/PRG/save edits.
const exp = await initWasm({module_or_path: fs.readFileSync(new URL('../../tmp/opennoah_wasm_bg.wasm', import.meta.url))});
const rt = new GameRuntime(fs.readFileSync(new URL('../../tmp/tianjiang-online.opennoah', import.meta.url)), 'certified_original_v1');
rt.restoreBytes(fs.readFileSync(new URL('../../tmp/battle-fresh-start.bin', import.meta.url)));
const mem = new Uint8Array(exp.memory.buffer), found = [];
for (let i = 0; i + 0x8000 < mem.length; ++i)
    if (mem[i+0x7934] === 3 && mem[i+0x7935] === 0 && mem[i+0x7936] === 1 && mem[i+0x7a58] === 9 && mem[i+0x7a66] === 6) found.push(i);
if (found.length !== 1) throw Error('RAM ' + found.length);
const base = found[0], word = o => mem[base+o-1] << 8 | mem[base+o];
const far = o => word(o) * 65536 + word(o+2);
const npe = fs.readFileSync(new URL('../../tmp/tjfy-world-probe.npe', import.meta.url));
// Original loader 31:F076..F4xx: four scene, three map, seven battle
// resource pairs. Slots are engine resource roles; later sections are not
// represented by this table and are excluded from this comparison.
const offsets = [0x78d4,0x78d8,0x78dc,0x78e0,0x78fe,0x7902,0x7906,
    0x7872,0x7876,0x787a,0x787e,0x7882,0x7886,0x788a];
const sizes = [0x78c4,0x78c8,0x78cc,0x78d0,0x78f2,0x78f6,0x78fa,
    0x7856,0x785a,0x785e,0x7862,0x7866,0x786a,0x786e];
const rows = offsets.map((o,i) => {
    const section = 16+i, offset = far(o), size = far(sizes[i]);
    const expectedOffset = npe.readUInt32BE(0x3a+8*section), expectedSize = npe.readUInt32BE(0x3e+8*section);
    return {section, offsetAddress:o, sizeAddress:sizes[i], offset, size, expectedOffset, expectedSize,
        matches: offset === expectedOffset && size === expectedSize};
});
fs.writeFileSync(new URL('../../tmp/native-sound-resources.json', import.meta.url), JSON.stringify(rows,null,2));
console.log(JSON.stringify(rows));
rt.free();
if (rows.some(row => !row.matches)) throw Error('original audio resource table mismatch');
