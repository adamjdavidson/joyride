import test from 'node:test';
import assert from 'node:assert/strict';
import { DisplayState, displaySnapshot } from '../src/instrument-displays.js';

const state = () => ({ client: 'test-client-123456', seq: 1, mode: 'Expand', person: null, status: 'ready', engine: 'scripted', controls: { wild: 2, offset: 2, speed: 3, initiative: 3 }, defaults: { wild: 0, speed: 3, initiative: 3 }, contexts: ['A thinking harness'], factory: false, removeNext: false });

test('display metadata excludes chat, context bodies and credentials', () => {
  const input = { ...state(), chat: 'private conversation', key: 'secret-key', contextBody: 'private source' };
  const result = displaySnapshot(input);
  assert.doesNotMatch(JSON.stringify(result), /private|secret/);
  assert.deepEqual(result.contexts, ['A thinking harness']);
  assert.throws(() => displaySnapshot({ ...input, mode: 'invented' }));
  assert.throws(() => displaySnapshot({ ...input, controls: { ...input.controls, speed: 6 } }));
});

test('a stale desk disappears; old packets and competing tabs cannot overwrite a live desk', () => {
  let now = 1000; const hub = new DisplayState({ now: () => now });
  assert.equal(hub.packet().connected, false);
  hub.publish(state()); assert.equal(hub.packet().connected, true);
  assert.throws(() => hub.publish(state()), /out of order/);
  assert.throws(() => hub.publish({ ...state(), client: 'other-client-12345' }), /Another/);
  hub.publish({ ...state(), seq: 2, mode: 'Challenge', person: 'leonardo', status: 'stopped' });
  assert.equal(hub.packet().state.mode, 'Challenge'); assert.equal(hub.packet().state.person, 'leonardo');
  now += 5001; assert.deepEqual(hub.packet(), { v: 1, seq: 2, connected: false, state: null });
  hub.publish({ ...state(), client: 'other-client-12345' });
  assert.equal(hub.packet().state.client, 'other-client-12345');
});
