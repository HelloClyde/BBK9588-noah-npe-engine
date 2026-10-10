import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snap=fs.readFileSync('tmp/native-exit-dialogue-1-after.snapshot');
const configs=[
 {name:'buy-one',money:100,owned:0,quantity:1},
 {name:'buy-exact',money:50,owned:3,quantity:5},
 {name:'buy-poor',money:49,owned:3,quantity:5},
 {name:'buy-inventory-wrap',money:100,owned:65535,quantity:1},
 {name:'sell-one',sell:true,money:100,owned:2,quantity:1},
 {name:'sell-last',sell:true,money:100,owned:5,quantity:5},
 {name:'sell-money-wrap',sell:true,money:4294967294,owned:3,quantity:1},
 {name:'buy-signed-money',money:2147483648,owned:0,quantity:1},
];
const rows=[];
for(const cfg of configs){
 rt.restoreBytes(snap);const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
 for(let i=0;i+0x8000<mem().length;++i)
  if(mem()[i+0x7934]===4&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7938]===13&&mem()[i+0x7a58]===7)found.push(i);
 /* restoreBytes keeps a serialized RAM copy in the WASM heap. Identify
    the live bank by an actual Down key changing its selected item. */
 if(found.length>1){
  const selected=found.map(i=>mem()[i+0x7a4b]*256+mem()[i+0x7a4c]);
  for(let t=0;t<32;++t){const out=rt.advance(t===0?2:0);out.free();}
  const live=found.filter((i,j)=>mem()[i+0x7a4b]*256+mem()[i+0x7a4c]!==selected[j]);
  found.splice(0,found.length,...live);
 }
 if(found.length!==1)throw Error('RAM '+found.length);
 const base=found[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putW=(o,v)=>{mem()[a(o)-1]=v>>>8;mem()[a(o)]=v&255;},putB=(o,v)=>{mem()[a(o)-1]=v;};
 const state=()=>({mode:w(0x7934),event:w(0x7936),context:w(0x7938),money:(w(0x799e)*65536+w(0x79a0))>>>0,
  owned:w(0x67c8+45*2),price:w(0x741e),selected:w(0x7a4c),ui:b(0x7a4a),quantity:b(0x7a48),choice:b(0x7a0c),sell:b(0x7ac2)});
 putW(0x799e,cfg.money>>>16);putW(0x79a0,cfg.money&65535);putW(0x67c8+45*2,cfg.owned);
 putW(0x7a4c,45);putB(0x7a72,45);putW(0x741e,10);putB(0x7ac2,cfg.sell?1:0);
 putB(0x7a4a,2);putB(0x7a0c,1);putB(0x7a48,cfg.quantity);putW(0x7ab4,0);putW(0x7a76,0);
 const before=state(),commits=[];let delivered=false;
 for(let t=0;t<160;++t){putW(0x7a76,t>=3&&!delivered?0x86:0);const out=rt.advance(0);out.free();
  for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){
   const n=commits.length;fs.writeFileSync(`tmp/native-shop-${rows.length}-commit-${n}.idx`,p);commits.push({tick:t,state:state()});}}
  const now=state();if(commits.length || now.ui!==2 || now.money!==before.money || now.owned!==before.owned)delivered=true;
 }
 if(!delivered)throw Error('Enter not delivered '+cfg.name);
 const row={...cfg,before,after:state(),commits};rows.push(row);console.log(cfg.name,JSON.stringify(row.after));
 fs.writeFileSync('tmp/native-shop.json',JSON.stringify({rows},null,2)+'\n');
}
rt.free();
