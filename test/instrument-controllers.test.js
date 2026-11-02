import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';
import '../public/joyride-config.js';

// Exercise the real reducer and browser bridge together, without a network or AI.
const html = readFileSync(new URL('../public/instrument-prototype.html', import.meta.url), 'utf8');
const reducer = html.slice(html.indexOf('const Instrument ='), html.indexOf('const Instrument =') + html.slice(html.indexOf('const Instrument =')).indexOf('</script>'));
const reducerOnly = reducer.slice(0, reducer.indexOf('})();') + 5);
const bridge = html.slice(html.indexOf('function applyControllerData('), html.indexOf('async function pollHardware('));
function instrument() {
  const context = vm.createContext({config:globalThis.JoyrideConfig,reach:3});
  vm.runInContext(`${reducerOnly}
    let state=Instrument.initial(),buttons=[],calls=[];
    function dispatch(action,options){calls.push({action,options});state=Instrument.reduce(state,action);}
    function releaseInputs(){dispatch({type:'RELEASE'},{respond:false});}
    function handleHardwareButton(event){buttons.push(event);}
    ${bridge}
    globalThis.api={
      apply:applyControllerData,
      state:()=>state, values:()=>Instrument.values(state), buttons:()=>buttons,
      calls:()=>calls
    };`, context);
  return context.api;
}
const input = (control, value) => ({ type: 'input', control, value });
const apply = (page, ...events) => page.apply({ state: {}, events }, false);

test('joystick steers, stops, stays stopped at center, resumes with gas and holds clutch', () => {
  const page = instrument();
  apply(page, input('steering', -1)); assert.equal(page.values().offset, -3);
  apply(page, input('steering', 1)); assert.equal(page.values().offset, 3);
  apply(page, input('steering', 0)); assert.equal(page.values().offset, 0);
  apply(page, input('clutch', 1)); assert.equal(page.values().initiative, 6);
  apply(page, input('clutch', 0)); assert.equal(page.values().initiative, 3);
  apply(page, input('brake', 1)); assert.equal(page.state().stopped, true);
  apply(page, input('brake', 0)); assert.equal(page.state().stopped, true);
  apply(page, input('throttle', 1)); assert.equal(page.state().resumePending, true);
  apply(page, input('throttle', 0)); assert.equal(page.state().stopped, false);
});

test('controller handoff cancels pending resume; unplug clears held controls and stops', () => {
  const page = instrument();
  apply(page, input('brake', 1), input('brake', 0), input('throttle', 1));
  apply(page, { type: 'controls_reset', stop: false }, input('steering', .5));
  assert.equal(page.state().stopped, true);
  assert.equal(page.state().resumePending, false);
  assert.equal(page.state().axes.gas, 0);
  apply(page, input('throttle', 1), input('throttle', 0), input('clutch', 1));
  apply(page, { type: 'controls_reset', stop: true });
  assert.equal(page.state().stopped, true);
  assert.equal(page.state().axes.clutch, 0);
  assert.equal(page.state().axes.wheel, 0);
});

test('reconnect and foreground snapshots do not replay gas or buttons', () => {
  const page = instrument();
  page.apply({ state: { axes: { steering: 1, throttle: 1, clutch: 1 } }, events: [input('button:288', 1), input('throttle', 1)] }, true);
  assert.equal(page.state().axes.gas, 0);
  assert.equal(page.state().axes.clutch, 1);
  assert.equal(page.buttons().length, 0);
  assert.ok(page.calls().every(call => call.options.respond === false));
});
