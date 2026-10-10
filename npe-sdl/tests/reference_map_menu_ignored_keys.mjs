import fs from 'node:fs';
import {spawnSync} from 'node:child_process';

// Reuse the original entry and diagnostic seeds in existing reference samplers.
// Derived harnesses write separate files; old source, frames and states remain.
const families=[
 ['map-item-record','same-single-use','map-menu-ignored-item'],
 ['equip-item-record','cached-empty-gear','map-menu-ignored-equip'],
 ['map-skill-record','same-single','map-menu-ignored-skill'],
 ['map-item-record','same-single-use','map-menu-ignored-depleted'],
];
const rawKeys=[32,0x90,9,...'qQuUeEdDfF'.split('').map(c=>c.charCodeAt(0))];
const results=[];
for(const [family,name,prefix] of families){
 const originalPath=`npe-sdl/tests/reference_${family.replaceAll('-','_')}.mjs`;
 let source=fs.readFileSync(originalPath,'utf8');
 const depleted=prefix==='map-menu-ignored-depleted';
 if(depleted){
  const keys='keys:[16,16,64,64]';
  if(!source.includes(keys))throw Error('depletion seed changed');
  source=source.replace(keys,'keys:[16,16,16,1,2,16,64,64]');
 }
 source=source.replaceAll('../../tmp/','./').replaceAll(`native-${family}`,`native-${prefix}`);
 const marker=family==='map-skill-record'?'for(const mask of c.keys){const commits=[];':'for(const key of cfg.keys){';
 if(!source.includes(marker))throw Error('reference loop changed '+family);
 const inject=`const ignored=[];const ignoredKeys=${JSON.stringify(rawKeys)};
 const checkIgnored=()=>{for(const raw of ignoredKeys){
  const before=state(),pixels=Buffer.from(rt.framePixelIndices());let commits=0;
  for(let t=0;t<16;++t){putW(0x7a76,t>=3?raw:0);tick(0);commits+=rt.displayCommitCount();}
  putW(0x7a76,0);const after=state();
  if(commits||JSON.stringify(before)!==JSON.stringify(after)||!pixels.equals(Buffer.from(rt.framePixelIndices())))
   throw Error('ignored key changed original '+JSON.stringify({raw,before,after,commits}));
  ignored.push({raw,mode:before.mode,menu:before.menu,commits,statePreserved:true,pixelsPreserved:true});
 }};
 `;
 source=source.replace(marker,inject+marker+'checkIgnored();');
 // Deliver valid confirmations through the same raw-key window as the
 // ignored inputs. Matching the old positive route calibrates delivery.
 if(family==='map-skill-record'){
  source=source.replace(marker,marker+'let delivered=false;');
  const tick='for(let t=0;t<(c.ticks??32);++t){tick(t===0?mask:0);';
  if(!source.includes(tick))throw Error('magic tick loop changed');
  source=source.replace(tick,'for(let t=0;t<(c.ticks??32);++t){if(mask===16)putW(0x7a76,t>=3&&!delivered?0x86:0);tick(mask!==16&&t===0?mask:0);if(mask===16&&rt.displayCommitCount())delivered=true;');
 }else{
  source=source.replace('if(key>=256)putW','if(key===16)putW(0x7a76,t>=3&&!delivered?0x86:0);else if(key>=256)putW');
  source=source.replace('tick(key<256&&t===0?key:0);','tick(key<256&&key!==16&&t===0?key:0);');
 }
 const endMarker='}finally{rt.free();}';
 if(!source.includes(endMarker))throw Error('reference cleanup changed '+family);
 source=source.replace(endMarker,`fs.writeFileSync('tmp/native-${prefix}-ignored.json',JSON.stringify(ignored,null,2)+'\\n');\n`+endMarker);
 const harness=`tmp/reference-${prefix}.mjs`;
 fs.writeFileSync(harness,source);
 fs.writeFileSync(`tmp/native-${prefix}.json`,'[]\n');
 const p=spawnSync(process.execPath,[harness,name],{encoding:'utf8'});
 fs.writeFileSync(`tmp/native-${prefix}.log`,(p.stdout??'')+(p.stderr??''));
 if(p.error)throw p.error;if(p.status)throw Error(family+' failed: '+p.stderr);
 const cases=JSON.parse(fs.readFileSync(`tmp/native-${prefix}-ignored.json`));
 const old=JSON.parse(fs.readFileSync(`tmp/native-${family}.json`)).find(r=>r.name===name);
 const current=JSON.parse(fs.readFileSync(`tmp/native-${prefix}.json`))[0];
 if(JSON.stringify(old.before)!==JSON.stringify(current.before)||(!depleted&&old.phases.length!==current.phases.length))
  throw Error('original entry changed '+family);
 let calibratedFrames=0;
 if(depleted){
  const used=current.phases[2];
  if(used.after.stock[0][1]!==0||used.after.party.hp!==64||used.after.party.qi!==106)
   throw Error('original depletion effects differed');
  for(const i of [3,4,5])if(current.phases[i].commits.length||JSON.stringify(current.phases[i].after)!==JSON.stringify(used.after))
   throw Error('original depleted direction/confirm changed state or LCD');
 }
 for(let i=0;i<(depleted?2:old.phases.length);++i){
  if(JSON.stringify(old.phases[i].after)!==JSON.stringify(current.phases[i].after))
   throw Error('positive raw confirmation state differed '+family+' '+i);
  const a=old.phases[i].commits.filter(f=>f.state.mode!==1),b=current.phases[i].commits.filter(f=>f.state.mode!==1);
  if(a.length!==b.length)throw Error('positive commit count differed '+family+' '+i);
  for(let j=0;j<a.length;++j){
   const pixels=fs.readFileSync(`tmp/native-${family}-${name}-${a[j].index}.idx`);
   if(!pixels.equals(fs.readFileSync(`tmp/native-${prefix}-${name}-${b[j].index}.idx`)))
    throw Error('positive raw confirmation frame differed '+family+' '+i);
   ++calibratedFrames;
  }
 }
 results.push({family,name,prefix,cases,rawConfirmationCalibrated:true,calibratedFrames,depletedDirectionsIgnored:depleted});
 console.log(`${family}: ${cases.length} original ignored inputs, no sampled state/pixel change or LCD commit`);
}
fs.writeFileSync('tmp/native-map-menu-ignored-keys.json',JSON.stringify({passed:true,families:results},null,2)+'\n');
