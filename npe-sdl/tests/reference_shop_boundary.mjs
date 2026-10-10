import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snap=fs.readFileSync('tmp/native-exit-dialogue-1-after.snapshot');
const down=n=>Array(n).fill(0x83),up=n=>Array(n).fill(0x82);
const configs=[
 {name:'sell-empty-entry',sell:true,item:45,money:100,owned:0,stocks:[],keys:[0x82,0x83,0x86,0x80,0x81]},
 {name:'sell-negative-stock',sell:true,item:45,money:100,owned:65535,stocks:[[45,65535],[46,32768]],keys:[0x82,0x83,0x86]},
 {name:'buy-stock-99-quantity',money:1000,owned:99,keys:[0x86,0x82,0x83,0x82,0x86]},
 {name:'buy-stock-100-quantity',money:1000,owned:100,keys:[0x86,0x83,0x82,0x86]},
 {name:'buy-negative-stock-quantity',money:1000,owned:65535,keys:[0x86,0x83,0x82,0x86]},
 {name:'sell-stock-256-quantity',sell:true,money:1000,owned:256,keys:[0x86,0x83,0x82,0x83,0x86]},
 {name:'sell-stock-257-quantity',sell:true,money:1000,owned:257,keys:[0x86,0x83,0x82,0x86]},
 {name:'sell-stock-32767-quantity',sell:true,money:1000,owned:32767,keys:[0x86,0x83,0x82,0x86]},
 {name:'sell-empty-cleared-resident',sell:true,item:45,money:100,owned:0,stocks:[],seed:{first:0,last:0},keys:[0x82,0x83,0x86,0x81,0x81]},
 {name:'buy-list-L-retained',money:1000,owned:0,keys:[0x81,0x81,0x81,0x83,0x86,0x86]},
 {name:'buy-list-L-direct-quantity',money:1000,owned:0,keys:[0x81,0x86,0x86,0x83]},
 {name:'buy-list-L-page-reset',money:1000,owned:0,keys:[0x81,...down(7),0x82]},
];
const rows=process.argv.includes('--append')?JSON.parse(fs.readFileSync('tmp/native-shop-boundary.json','utf8')).rows:[];
for(const cfg of configs.slice(rows.length)){
 const item=cfg.item??45;
 rt.restoreBytes(snap);const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
 for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===4&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7938]===13&&mem()[i+0x7a58]===7)found.push(i);
 if(found.length>1){const sel=found.map(i=>mem()[i+0x7a4b]*256+mem()[i+0x7a4c]);for(let t=0;t<32;++t){const o=rt.advance(t===0?2:0);o.free();}const live=found.filter((i,j)=>mem()[i+0x7a4b]*256+mem()[i+0x7a4c]!==sel[j]);found.splice(0,found.length,...live);}
 if(found.length!==1)throw Error('RAM '+found.length);const base=found[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putW=(o,v)=>{mem()[a(o)-1]=v>>>8;mem()[a(o)]=v&255;},putB=(o,v)=>{mem()[a(o)-1]=v;};
 const state=()=>({mode:w(0x7934),event:w(0x7936),context:w(0x7938),money:(w(0x799e)*65536+w(0x79a0))>>>0,owned:w(0x67c8+item*2),selected:w(0x7a4c),first:w(0x7ac6),last:w(0x7ab6),row:b(0x7a46),ui:b(0x7a4a),quantity:b(0x7a48),choice:b(0x7a0c),sell:b(0x7ac2),uiBase:w(0x7aec),descLength:w(0x7aba),descEnd:w(0x7abc),descStart:w(0x7abe)});
 const seed={first:w(0x7ac6),last:w(0x7ab6),...cfg.seed};putW(0x7ac6,seed.first);putW(0x7ab6,seed.last);
 putW(0x799e,cfg.money>>>16);putW(0x79a0,cfg.money&65535);for(let id=1;id<255;++id)putW(0x67c8+id*2,0);for(const [id,quantity] of cfg.stocks??[[item,cfg.owned]])putW(0x67c8+id*2,quantity);putW(0x7938,cfg.sell?10:13);putW(0x7ab4,1);putW(0x7a76,0);
 let frameCount=0;const collect=t=>{const c=[];for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){fs.writeFileSync(`tmp/native-shop-boundary-${rows.length}-commit-${frameCount}.idx`,p);c.push({index:frameCount++,tick:t,state:state()});}}return c;};
 const init=[];for(let t=0;t<64;++t){const o=rt.advance(0);o.free();init.push(...collect(t));}
 const before=state();fs.writeFileSync(`tmp/native-shop-boundary-${rows.length}-before.idx`,rt.framePixelIndices());const phases=[];
 for(const key of cfg.keys){const commits=[];let delivered=false;for(let t=0;t<32;++t){putW(0x7a76,t>=3&&!delivered?key:0);const o=rt.advance(0);o.free();commits.push(...collect(t));if(commits.length)delivered=true;}
 phases.push({key,delivered,commits,after:state()});/* Boundary keys may be intentionally ignored. Record zero submits. */}
 rows.push({...cfg,seed,init,before,phases});fs.writeFileSync('tmp/native-shop-boundary.json',JSON.stringify({rows},null,2)+'\n');console.log(cfg.name,JSON.stringify({before,after:state(),counts:phases.map(p=>p.commits.length)}));
}
rt.free();
