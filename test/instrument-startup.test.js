import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {readFile} from 'node:fs/promises';
import vm from 'node:vm';

const html=readFileSync(new URL('../public/instrument-prototype.html',import.meta.url),'utf8');
class Element {
  constructor(){this.style={};this.dataset={};this.attributes={};this.options=[];this.value='';this.textContent='';this.scrollHeight=0;this.scrollTop=0;this.clientHeight=0;this.listeners={};this.classList={toggle(){},add(){},remove(){}};}
  addEventListener(name,fn){this.listeners[name]=fn;}
  setAttribute(name,value){this.attributes[name]=value;}
  querySelector(){return new Element();}
  querySelectorAll(){return [];}
  showModal(){this.open=true;}
  close(){this.open=false;}
  focus(){}
  click(){this.onclick?.();}
}
async function browser(){
  const elements=Object.fromEntries([...html.matchAll(/id="([^"]+)"/g)].map(([,id])=>[id,new Element()]));
  const requests=[];
  const context=vm.createContext({console,URL,AbortSignal,structuredClone,
    document:{getElementById:id=>elements[id]??null,body:new Element(),querySelector:()=>new Element(),querySelectorAll:()=>[],addEventListener(){},hidden:false},
    window:{addEventListener(){}},location:{protocol:'http:'},localStorage:{getItem:()=>null,setItem(){}},
    crypto:{randomUUID:()=> 'startup-browser-12345678'},performance:{now:()=>0},
    setTimeout:()=>1,clearTimeout(){},setInterval:()=>1,
    fetch:async path=>{requests.push(path);return {ok:true,json:async()=>path==='/api/cards'?{version:1,revision:0,cards:{},token:'test'}:{ready:false,token:'test'}};}
  });
  for(const [,attrs,body] of html.matchAll(/<script([^>]*)>([\s\S]*?)<\/script>/g)){
    const src=attrs.match(/src="([^"]+)"/);
    vm.runInContext(src?readFileSync(new URL('../public/'+src[1],import.meta.url),'utf8'):body,context);
  }
  await new Promise(r=>setImmediate(r));
  return {context,elements,requests,run:code=>vm.runInContext(code,context)};
}
test('entire shipped page initializes, binds controls and accepts card/controller snapshots',async()=>{
  const page=await browser();
  assert.equal(typeof page.elements.viewToggle.onclick,'function');
  assert.equal(typeof page.elements.hardwareOpen.onclick,'function');
  assert.equal(page.run('state.mode'),'Download');
  assert.equal(page.run('Object.keys(people).length'),7);
  page.run("dispatch({type:'MODE',value:'Expand'});applyControllerData({events:[{type:'input',control:'steering',value:1}]},false)");
  assert.equal(page.run('Instrument.values(state).offset'),3);
  page.run("cardBindings.set('AA01','mode:Structure');cardReader.update({left:{connected:true,error:null,uid:'AA01'}},[],true)");
  assert.equal(page.run('state.mode'),'Structure');
  page.run('cardReader.update({},[],true)');
  assert.equal(page.run('state.mode'),null);
  assert.equal(page.elements.continue.disabled,true);
  assert.equal(page.run('PhysicalDisplays.snapshot().mode'),null);
  page.run("dispatch({type:'MODE',value:'Implement'})");
  assert.equal(page.run('state.mode'),'Implement');
  for(const id of page.run('Object.keys(people)')){
    page.run(`dispatch({type:'PERSON',value:${JSON.stringify(id)}});startResponse('test')`);
    assert.ok(page.run('activeMessage.text.length')>0);
  }
  assert.ok(!page.requests.some(p=>p.includes('/respond')));
});
test('running server handler serves every required script instead of a misleading healthy API',async()=>{
  const source=readFileSync(new URL('../scripts/demo.js',import.meta.url),'utf8');
  const callback=source.slice(source.indexOf('async (req, res) => {'),source.indexOf('\n});\nserver.on'))+'\n})';
  const executable='('+callback.replaceAll('import.meta.url',JSON.stringify(new URL('../scripts/demo.js',import.meta.url).href));
  const handler=vm.runInNewContext(executable,{URL,readFile,allowedHosts:new Set(['localhost:8788']),port:8788,file:new URL('../public/instrument-prototype.html',import.meta.url)});
  for(const path of ['/',...[...html.matchAll(/<script src="([^"]+)"/g)].map(([,src])=>'/'+src)]){
    let status=200,body;const headers={};
    const res={setHeader:(k,v)=>headers[k]=v,writeHead(code,h){status=code;Object.assign(headers,h);return this;},end(value){body=value;}};
    await handler({headers:{host:'localhost:8788'},url:path,method:'GET'},res);
    assert.equal(status,200,path);assert.ok(body.length>0,path);
    assert.match(headers['Content-Type'],path==='/'?/text\/html/:/javascript/);
  }
});
