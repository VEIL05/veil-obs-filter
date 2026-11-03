// Generate native numeric references from unchanged upstream JS, not from C++.
import fs from 'node:fs';
import { pathToFileURL } from 'node:url';
import path from 'node:path';
const base=process.argv[2];
if(!base)throw new Error('Pass the unchanged upstream face-beautifier/src directory as the first argument.');
const load=name=>import(pathToFileURL(path.resolve(base,name)).href);
const {syntheticLandmarks}=await load('selftest.js');
const {computeFace}=await load('face.js');
const {buildWarps,faceUniforms,blurScaleFor}=await load('warp.js');
const {toShader,PRESETS,DEFAULT_COLORS,allControls}=await load('controls.js');
const actual=JSON.parse(fs.readFileSync(new URL('../presets/trueitem-user-2026-11-03.json',import.meta.url),'utf8'));
const pts=syntheticLandmarks();
const controls=allControls().map(c=>c.key);
const cases=[];
for(const width of [1280,1920])for(const id of ['off','glam','user']){
 const preset=id==='user'?actual:{values:PRESETS.find(p=>p.id===id).values,colors:DEFAULT_COLORS,mirror:false};
 const values=Object.fromEntries(controls.map(k=>[k,preset.values[k]??0]));
 const colors=preset.colors;
 const p=toShader(values,colors),face=computeFace(pts,width/720),warps=buildWarps(face,p),blobs=faceUniforms(face,1);
 cases.push({name:id,width,height:720,values,colors,mirror:preset.mirror??false,p,face,warps,blobs,blur:blurScaleFor(face,720,p.radius)});
}
const lines=[`${pts.length/2} ${cases.length}`];
for(let i=0;i<pts.length;i+=2)lines.push(`${pts[i]} ${pts[i+1]}`);
const rgb=hex=>{let n=parseInt(hex.slice(1),16);return [(n>>16&255)/255,(n>>8&255)/255,(n&255)/255];};
for(const c of cases){
 lines.push(c.name,`${c.width} ${c.height}`,controls.map(k=>c.values[k]).join(' '),[...rgb(c.colors.lip),...rgb(c.colors.blush),+c.mirror].join(' '));
 lines.push(`${c.warps.length} ${c.blur}`,[...c.blobs.eyeL,...c.blobs.eyeR,...c.blobs.cheekL,...c.blobs.cheekR].join(' '));
 for(const w of c.warps)lines.push([w.cx,w.cy,w.r,w.mode,w.dx||0,w.dy||0,w.k||0,0].join(' '));
}
fs.writeFileSync(new URL('math_reference.txt',import.meta.url),lines.join('\n')+'\n');
console.log(`Generated ${cases.length} reference cases, ${pts.length/2} upstream synthetic landmarks.`);
