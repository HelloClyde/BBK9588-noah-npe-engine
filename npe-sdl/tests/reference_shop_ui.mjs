import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snap=fs.readFileSync('tmp/native-exit-dialogue-1-after.snapshot');
const configs=[
 {name:'buy-cancel',money:100,owned:0,keys:[0x86,0x83,0x82,0x86]},
 {name:'buy-max-poor',money:100,owned:0,keys:[0x86,0x83,0x80,0x86]},
 {name:'buy-quantity-cycle',money:100,owned:98,keys:[0x86,0x82,0x83,0x81,0x80,0x8e]},
 {name:'buy-poor',money:0,owned:0,keys:[0x86,0x80,0x86]},
 {name:'sell-one',sell:true,money:100,owned:2,keys:[0x86,0x83,0x82,0x80,0x86]},
 {name:'sell-last',sell:true,money:100,owned:1,keys:[0x86,0x83,0x80,0x86]},
 {name:'buy-two',money:1000,owned:0,keys:[0x86,0x82,0x80,0x86]},
 {name:'sell-description-pages',sell:true,item:91,money:100,owned:2,keys:[0x86,0x85,0x84,0x86]},
];
const rows=process.argv.includes('--append')?JSON.parse(fs.readFileSync('tmp/native-shop-ui.json','utf8')).rows:[];
for(const cfg of configs.slice(rows.length)){
 const item=cfg.item??45;
 rt.restoreBytes(snap);const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
 for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===4&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7938]===13&&mem()[i+0x7a58]===7)found.push(i);
 if(found.length>1){const sel=found.map(i=>mem()[i+0x7a4b]*256+mem()[i+0x7a4c]);for(let t=0;t<32;++t){const o=rt.advance(t===0?2:0);o.free();}const live=found.filter((i,j)=>mem()[i+0x7a4b]*256+mem()[i+0x7a4c]!==sel[j]);found.splice(0,found.length,...live);}
 if(found.length!==1)throw Error('RAM '+found.length);const base=found[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putW=(o,v)=>{mem()[a(o)-1]=v>>>8;mem()[a(o)]=v&255;},putB=(o,v)=>{mem()[a(o)-1]=v;};
 const state=()=>({mode:w(0x7934),event:w(0x7936),context:w(0x7938),money:(w(0x799e)*65536+w(0x79a0))>>>0,owned:w(0x67c8+item*2),selected:w(0x7a4c),first:w(0x7ac6),last:w(0x7ab6),row:b(0x7a46),ui:b(0x7a4a),quantity:b(0x7a48),choice:b(0x7a0c),sell:b(0x7ac2),uiBase:w(0x7aec),descLength:w(0x7aba),descEnd:w(0x7abc),descStart:w(0x7abe)});
 putW(0x799e,cfg.money>>>16);putW(0x79a0,cfg.money&65535);for(let id=1;id<255;++id)putW(0x67c8+id*2,0);putW(0x67c8+item*2,cfg.owned);putW(0x7938,cfg.sell?10:13);putW(0x7ab4,1);putW(0x7a76,0);
 let frameCount=0;const collect=t=>{const c=[];for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){fs.writeFileSync(`tmp/native-shop-ui-${rows.length}-commit-${frameCount}.idx`,p);c.push({index:frameCount++,tick:t,state:state()});}}return c;};
 const init=[];for(let t=0;t<64;++t){const o=rt.advance(0);o.free();init.push(...collect(t));}
 const before=state();fs.writeFileSync(`tmp/native-shop-ui-${rows.length}-before.idx`,rt.framePixelIndices());const phases=[];
 for(const key of cfg.keys){const commits=[];let delivered=false;for(let t=0;t<(cfg.money<1000&&key===0x86&&!cfg.sell?180:96);++t){putW(0x7a76,t>=3&&!delivered?key:0);const o=rt.advance(0);o.free();commits.push(...collect(t));if(commits.length)delivered=true;}
 phases.push({key,delivered,commits,after:state()});if(!delivered)throw Error('key not delivered '+cfg.name+' '+key.toString(16));}
 rows.push({...cfg,init,before,phases});fs.writeFileSync('tmp/native-shop-ui.json',JSON.stringify({rows},null,2)+'\n');console.log(cfg.name,JSON.stringify({before,after:state(),counts:phases.map(p=>p.commits.length)}));
}
rt.free();
