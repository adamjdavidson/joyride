import test from 'node:test';
import assert from 'node:assert/strict';
import { EventEmitter } from 'node:events';
import { Store } from '../src/store.js';
import { InstrumentLive, liveBody, responseEvents, EFFORTS } from '../src/instrument-live.js';

const request = () => ({ id: 'test-request-123456', mode: 'Download', person: null, controls: { wild: 0, speed: 3, initiative: 3 }, messages: [{ role: 'user', content: 'I want a place to think.' }], contexts: [] });
class Output extends EventEmitter {
  events = []; destroyed = false; writableEnded = false;
  writeHead(status) { this.status = status; } flushHeaders() {}
  write(line) { this.events.push(JSON.parse(line)); }
  end() { this.writableEnded = true; }
}
function fixture(fetchImpl) {
  const store = new Store(':memory:');
  return { store, live: new InstrumentLive({ store, config: { mode: 'live', key: 'never-expose-this-key', budget: 25 }, fetchImpl }) };
}

test('controls change real request settings and consultations suspend the activity', () => {
  for (let speed = 1; speed <= 5; speed++) {
    const data = request(); data.controls.speed = speed;
    assert.equal(liveBody(data).reasoning.effort, EFFORTS[speed]);
  }
  const data = request(); data.person = 'leonardo'; data.continuing = true;
  data.contexts.push({ name: 'My own context', body: 'Keep the surprising part.' });
  const body = liveBody(data);
  assert.match(body.instructions, /Suspend the Download activity/);
  assert.match(body.instructions, /close observation/);
  assert.match(body.instructions, /Do not repeat/);
  assert.match(body.instructions, /Keep the surprising part/);
  data.messages[0].role = 'system'; assert.throws(() => liveBody(data));
  data.mode = 'toString'; assert.throws(() => liveBody(data));
});

test('SSE parsing survives split frames and UTF-8, including CRLF', async () => {
  const bytes = new TextEncoder().encode('event: response.output_text.delta\r\ndata: {"type":"response.output_text.delta","delta":"café"}\r\n\r\ndata: {"type":"response.completed"}\n\ndata: [DONE]\n\n');
  async function* chunks() { for (const byte of bytes) yield Uint8Array.of(byte); }
  const events = []; for await (const event of responseEvents(chunks())) events.push(event);
  assert.equal(events[0].delta, 'café'); assert.equal(events[1].type, 'response.completed'); assert.equal(events.length, 2);
});

test('stream exposes real deltas, settles usage and keeps credentials private', async () => {
  const { store, live } = fixture(async (url, options) => {
    assert.equal(options.headers.Authorization, 'Bearer never-expose-this-key');
    const events = [{ type: 'response.output_text.delta', delta: 'A real streamed thought.' }, { type: 'response.completed', response: { usage: { input_tokens: 100, output_tokens: 20 } } }];
    return new Response(events.map(x => 'data: ' + JSON.stringify(x) + '\n\n').join(''));
  });
  const out = new Output(); await live.stream(request(), out);
  assert.equal(out.events.find(x => x.type === 'delta').text, 'A real streamed thought.');
  assert.equal(out.events.at(-1).type, 'done'); assert.equal(store.spent(), 0.0006);
  assert.doesNotMatch(JSON.stringify([live.status(), out.events]), /never-expose/);
  store.close();
});

test('brake aborts upstream, retains unknown usage reservation, and rejects late output', async () => {
  let upstreamSignal, started;
  const ready = new Promise(resolve => { started = resolve; });
  const { store, live } = fixture(async (url, { signal }) => {
    upstreamSignal = signal; started();
    return new Promise((resolve, reject) => signal.addEventListener('abort', () => reject(new Error('aborted')), { once: true }));
  });
  const out = new Output(), pending = live.stream(request(), out); await ready;
  live.stop(request().id); await pending;
  assert.ok(upstreamSignal.aborted); assert.ok(store.spent() > 0);
  assert.equal(out.events.filter(x => x.type === 'delta' || x.type === 'error').length, 0);
  assert.equal(live.jobs.size, 0);
  await assert.rejects(live.stream(request(), new Output()), /stopped before/);
  store.close();
});

test('disconnect aborts generation; exhausted budget never calls the API', async () => {
  let reached; const ready = new Promise(resolve => { reached = resolve; });
  const { store, live } = fixture(async (url, { signal }) => { reached(); return new Promise((resolve, reject) => signal.addEventListener('abort', () => reject(new Error('closed')), { once: true })); });
  const out = new Output(), pending = live.stream(request(), out); await ready;
  out.destroyed = true; out.emit('close'); await pending; assert.equal(live.jobs.size, 0);
  live.config.budget = store.spent();
  await assert.rejects(live.stream({ ...request(), id: 'another-request-12345' }, new Output()), /remaining budget/);
  store.close();
});
