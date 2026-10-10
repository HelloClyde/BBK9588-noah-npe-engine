import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const rows=[];
const configs=[
 {name:'buy-resident',context:13,npcContext:13,kind:1,selected:46,first:45,last:54,row:1,ui:1,quantity:99,choice:1,descStart:82,descEnd:97},
 {name:'empty-sell-resident',context:10,npcContext:10,kind:2,selected:65535,first:9,last:37,row:6,ui:2,quantity:0,choice:0,descStart:0,descEnd:65535},
 {name:'word-byte-boundaries',context:65535,npcContext:255,kind:1,selected:254,first:1,last:254,row:6,ui:2,quantity:255,choice:1,descStart:65534,descEnd:65535}
];
for(const entry of ['system','title'])for(const cfg of configs){
 const choice=1,slot=3;
 rt.restoreBytes(fs.readFileSync('tmp/battle-fresh-start.bin'));
 const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
 for(let i=0;i+0x8000<mem().length;++i)
  if(mem()[i+0x7934]===3&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7a58]===9&&mem()[i+0x7a66]===6)found.push(i);
 if(found.length!==1)throw Error('RAM '+found.length);
 const base=found[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putW=(o,v)=>{mem()[a(o)-1]=v>>>8;mem()[a(o)]=v&255;},putB=(o,v)=>{mem()[a(o)-1]=v;};
 const tick=mask=>{const out=rt.advance(mask);out.free();};
 const idle=n=>{for(let i=0;i<n;++i)tick(0);};
 const key=mask=>{tick(mask);idle(15);};
 const resident=()=>({context:w(0x7938),npcContext:b(0x7a68),sellFlag:b(0x7ac2),kind:b(0x7a0e),selected:w(0x7a4c),first:w(0x7ac6),last:w(0x7ab6),row:b(0x7a46),ui:b(0x7a4a),quantity:b(0x7a48),choice:b(0x7a0c),descStart:w(0x7abe),descEnd:w(0x7abc)});
 const seed=()=>{putW(0x7938,cfg.context);putB(0x7a68,cfg.npcContext);putB(0x7a0e,cfg.kind);putB(0x7ac2,cfg.kind===2?1:0);putW(0x7a4c,cfg.selected);putW(0x7ac6,cfg.first);putW(0x7ab6,cfg.last);putB(0x7a46,cfg.row);putB(0x7a4a,cfg.ui);putB(0x7a48,cfg.quantity);putB(0x7a0c,cfg.choice);putW(0x7abe,cfg.descStart);putW(0x7abc,cfg.descEnd);};
 const state=()=>({resident:resident(),mode:w(0x7934),event:w(0x7936),map:w(0x7a58),progress:w(0x7a66),
  choice:b(0x7ac4),exitSlot:w(0x7a84),moveKind:w(0x7a86),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c)});
 putW(0x7934,1);putW(0x7ab4,1);putW(0x7a66,1);putW(0x7a50,0);putB(0x7ac4,0);idle(35);
 for(const mask of [64,2,2,2,16,2,16,16])key(mask);
 if(w(0x7934)!==1)throw Error('save failed '+JSON.stringify(state()));
 const saved=state();putW(0x7a66,4);putB(0x7ac4,choice);putW(0x7a84,slot);
 if(entry==='title'){putW(0x7934,7);putW(0x7ab4,1);idle(35);}
 const before=state();
 for(const mask of entry==='system'?[64,2,2,2,16,16]:[2,16])key(mask);
 const menu=state();seed();const beforeLoad=state();key(16);idle(150);
 const after=state();
 if(after.mode!==1||after.progress!==1)throw Error('load failed '+JSON.stringify(after));
 const row={entry,cfg,saved,before,menu,beforeLoad,after};rows.push(row);
 fs.writeFileSync('tmp/native-shop-control-load.json',JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify(row));putW(0x7934,0);
}
rt.free();
