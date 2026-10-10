import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync(new URL('../../tmp/opennoah_wasm_bg.wasm',import.meta.url))});
const pack=fs.readFileSync(new URL('../../tmp/tianjiang-online.opennoah',import.meta.url));
const rows=[];
const rt=new GameRuntime(pack,'certified_original_v1');
// Each case makes an actual original first-slot save through its UI.
for(const entry of ['system','title'])for(const enabled of [0,1])for(const slot of [0,1,2]){
 rt.restoreBytes(fs.readFileSync(new URL('../../tmp/battle-fresh-start.bin',import.meta.url)));
 const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
 for(let i=0;i+0x8000<mem().length;++i)
  if(mem()[i+0x7934]===3&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7a58]===9&&mem()[i+0x7a66]===6)found.push(i);
 if(found.length!==1)throw Error('RAM '+found.length);
 const base=found[0],a=o=>base+o-(o<0x7000?8:0),w=o=>mem()[a(o)-1]*256+mem()[a(o)],b=o=>mem()[a(o)-1];
 const putW=(o,v)=>{mem()[a(o)-1]=v>>>8;mem()[a(o)]=v&255;},putB=(o,v)=>{mem()[a(o)-1]=v;};
 const state=()=>({mode:w(0x7934),map:w(0x7a58),progress:w(0x7a66),enabled:w(0x7a50),slot:b(0x7a04),menu:b(0x7a54)});
 const tick=mask=>{const out=rt.advance(mask);out.free();},idle=n=>{for(let i=0;i<n;++i)tick(0);};
 let recording=false;const audio=[];
 const drain=label=>{const ev=JSON.parse(new TextDecoder().decode(rt.drainEventsBytes()));if(recording)for(const e of ev)if(e.type==='audio')audio.push({label,...e});};
 const key=(mask,label)=>{tick(mask);idle(15);drain(label);};
 putW(0x7934,1);putW(0x7ab4,1);putW(0x7a66,1);idle(35);putB(0x7a04,0);
 for(const mask of [64,2,2,2,16,2,16,16])key(mask,'save');
 if(w(0x7934)!==1)throw Error('original save did not return '+JSON.stringify(state()));
 // Save has progress1 and slot0; live progress4 proves successful restoration.
 putW(0x7a66,4);putW(0x7a50,enabled);putB(0x7a04,slot);
 if(entry==='title'){putW(0x7934,7);putW(0x7ab4,1);idle(35);}
 drain('setup');recording=true;
 const before=state();
 const keys=entry==='system'?[64,2,2,2,16,16]:[2,16];
 for(let i=0;i<keys.length;++i)key(keys[i],'open-'+i);
 const at_menu=state();
 fs.writeFileSync(new URL(`../../tmp/native-music-load-${entry}-${enabled}-${slot}-menu.idx`,import.meta.url),rt.framePixelIndices());
 key(16,'load');
 for(let i=0;i<150&&w(0x7934)!==1;++i)tick(0);
 idle(15);drain('after');
 const after=state();
 if(after.mode!==1||after.progress!==1||after.slot!==slot||after.enabled!==enabled)
  throw Error('load result '+JSON.stringify({before,at_menu,after,audio}));
 const plays=audio.filter(e=>e.command==='play');
 const expected=entry==='system'&&enabled;
 if(plays.length!==(expected?1:0)||(expected&&(plays[0].track_id!==20+slot||!plays[0].looping)))
  throw Error('load audio '+JSON.stringify({entry,enabled,slot,audio}));
 const row={entry,enabled,slot,before,at_menu,after,audio};rows.push(row);console.log(JSON.stringify(row));
 putW(0x7934,0);
}
fs.writeFileSync(new URL('../../tmp/native-music-slot-load.json',import.meta.url),JSON.stringify(rows,null,2)+'\n');
rt.free();
