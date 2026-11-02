import test from 'node:test';
import assert from 'node:assert/strict';
import { EventEmitter } from 'node:events';
import { WifiDisplays } from '../src/wifi-displays.js';

class Socket extends EventEmitter {
  destroyed = false; writableLength = 0; frames = [];
  setNoDelay() {} setKeepAlive() {} setTimeout() {} setEncoding() {}
  write(line) { this.frames.push(JSON.parse(line)); }
  destroy() { this.destroyed = true; this.emit('close'); }
}
const config = () => ({ devices: [{ id: 'mode-one', role: 'mode', token: 'secret-one' }, { id: 'speed-two', role: 'speed', token: 'secret-two' }] });
const hello = (id, token) => JSON.stringify({ v: 1, type: 'hello', id, token }) + '\n';

test('Wi-Fi clients must authenticate before receiving any instrument state', () => {
  const hub = new WifiDisplays(config()); hub.send({ v: 1, seq: 1, connected: true, state: { mode: 'Download' } });
  const bad = new Socket(); hub.accept(bad); bad.emit('data', hello('mode-one', 'wrong'));
  assert.equal(bad.destroyed, true); assert.deepEqual(bad.frames, []);
  const good = new Socket(); hub.accept(good); const line = hello('mode-one', 'secret-one');
  good.emit('data', line.slice(0, 12)); assert.deepEqual(good.frames, []);
  good.emit('data', line.slice(12)); assert.equal(good.frames[0].state.mode, 'Download');
  assert.doesNotMatch(JSON.stringify(hub.status()), /secret-one|secret-two/);
  hub.close();
});

test('multiple Wi-Fi displays get the same state and acknowledge independently', () => {
  let now = 1000; const hub = new WifiDisplays(config(), { now: () => now });
  const mode = new Socket(), speed = new Socket();
  hub.accept(mode); mode.emit('data', hello('mode-one', 'secret-one'));
  hub.accept(speed); speed.emit('data', hello('speed-two', 'secret-two'));
  hub.send({ v: 1, seq: 8, connected: true, state: { mode: 'Challenge' } });
  assert.deepEqual(mode.frames, speed.frames);
  mode.emit('data', JSON.stringify({ type: 'display_ack', v: 1, seq: 8, connected: true }) + '\n');
  assert.equal(hub.status().link, 'connected'); assert.equal(hub.status().acknowledged, 8);
  assert.equal(hub.status().devices[1].acknowledged, null);
  now += 5000; assert.equal(hub.status().link, 'waiting for acknowledgment');
  const replacement = new Socket(); hub.accept(replacement); replacement.emit('data', hello('mode-one', 'secret-one'));
  assert.ok(mode.destroyed); assert.equal(hub.peers.get('mode-one').socket, replacement);
  hub.close();
});

test('Wi-Fi clients cannot publish modes or send unbounded input', () => {
  const hub = new WifiDisplays(config()), socket = new Socket();
  hub.accept(socket); socket.emit('data', hello('mode-one', 'secret-one'));
  socket.emit('data', JSON.stringify({ type: 'publish', mode: 'Implement' }) + '\n');
  assert.ok(socket.destroyed); assert.equal(hub.peers.size, 0);
  const huge = new Socket(); hub.accept(huge); huge.emit('data', 'x'.repeat(4097)); assert.ok(huge.destroyed);
  hub.close();
});
