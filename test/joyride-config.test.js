import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';
import '../public/joyride-config.js';
import { liveBody } from '../src/instrument-live.js';
import { displaySnapshot } from '../src/instrument-displays.js';

const config = globalThis.JoyrideConfig;
const html = readFileSync(new URL('../public/instrument-prototype.html', import.meta.url), 'utf8');
const reducer = html.slice(html.indexOf('const config=globalThis.JoyrideConfig'), html.indexOf('const $=id=>'));
function instrument(custom = config) {
  const context = vm.createContext({ JoyrideConfig: structuredClone(custom) });
  vm.runInContext(reducer + '\nglobalThis.api=Instrument;', context);
  return context.api;
}
const input = () => ({ mode: config.defaults.mode, person: null,
  controls: { wild: 0, speed: config.defaults.speed, initiative: config.defaults.initiative },
  messages: [{ role: 'user', content: 'Help me think about my idea.' }], contexts: [] });

test('every configured activity and perspective reaches the live prompt and display contract', () => {
  for (const mode of Object.keys(config.activities)) {
    const request = { ...input(), mode };
    assert.ok(liveBody(request).instructions.includes(config.activities[mode].instructions));
  }
  for (const [person, profile] of Object.entries(config.perspectives)) {
    const request = { ...input(), person };
    assert.ok(liveBody(request).instructions.includes(profile.instructions));
    const state = displaySnapshot({ ...request, client: 'test-browser-123456', seq: 1, status: 'ready', engine: 'scripted',
      controls: { ...request.controls, offset: 0 }, defaults: request.controls });
    assert.equal(state.person, person);
    assert.ok(!Object.hasOwn(state, 'messages'));
  }
});

test('new activity, shortened scales and wider creative reach work in the actual browser reducer', () => {
  const custom = structuredClone(config);
  custom.activities.Reflect = { description: 'Reflect', instructions: 'Reflect the change.' };
  custom.defaults = { mode: 'Reflect', wild: 2, speed: 2, initiative: 2 };
  custom.speed = custom.speed.slice(0, 3);
  custom.initiative = custom.initiative.slice(0, 4);
  custom.creative.labels = ['a','b','c','d','e','f','g','h','i'];
  const api = instrument(custom);
  let state = api.initial();
  assert.equal(state.mode, 'Reflect');
  state = api.reduce(state, { type: 'AXIS', key: 'gas', value: 1 });
  state = api.reduce(state, { type: 'AXIS', key: 'clutch', value: 1 });
  state = api.reduce(state, { type: 'AXIS', key: 'wheel', value: 1 });
  assert.equal(api.values(state).speed, 3);
  assert.equal(api.values(state).initiative, 4);
  assert.equal(api.values(state).wild, 6);
  state = api.reduce(state, { type: 'SAVE' });
  state = api.reduce(state, { type: 'RELEASE' });
  assert.equal(api.values(state).wild, 6);
  state = api.reduce(state, { type: 'FACTORY', value: true });
  assert.equal(api.values(state).wild, 2);
  assert.equal(api.values(state).initiative, 2);
  assert.equal(api.values(state).speed, 2);
});

test('brake release does not resume; a deliberate gas tap resumes', () => {
  const api = instrument();
  let state = api.reduce(api.initial(), { type: 'AXIS', key: 'brake', value: 1 });
  state = api.reduce(state, { type: 'AXIS', key: 'brake', value: 0 });
  assert.equal(state.stopped, true);
  state = api.reduce(state, { type: 'AXIS', key: 'gas', value: .3 });
  assert.equal(state.stopped, true);
  state = api.reduce(state, { type: 'AXIS', key: 'gas', value: 0 });
  assert.equal(state.stopped, false);
});
