import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, readFile, writeFile, stat, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { CardRegistry, validCardTarget } from '../src/instrument-cards.js';
import '../public/instrument-cards.js';
import { liveBody } from '../src/instrument-live.js';

const reader = uid => ({ connected: true, error: null, uid });
function fixture() {
  const actions = [], bindings = new Map([['AA01', 'mode:Structure'], ['AA02', 'person:jane-jacobs'], ['AA03', 'person:vasubandhu'], ['AA04', 'message:continue'], ['AA05', 'context:foo'], ['AA06', 'person:jane-jacobs']]);
  const cards = new InstrumentCards({ bindings, act: (action, passive) => actions.push({ ...action, passive }) });
  const place = (id, uid) => cards.place(id, uid, false);
  return { cards, actions, place };
}
test('saved assignments survive a new registry instance; conflicts never rewrite a card', async () => {
  const dir = await mkdtemp(join(tmpdir(), 'joyride-cards-')), path = join(dir, 'cards.json');
  try {
    const registry = new CardRegistry(path);
    await registry.assign({ uid: 'AA01', target: 'mode:Structure', revision: 0 });
    await registry.assign({ uid: 'AA02', target: 'person:leonardo', revision: 1 });
    assert.deepEqual((await new CardRegistry(path).read()).cards, { AA01: 'mode:Structure', AA02: 'person:leonardo' });
    assert.equal((await stat(path)).mode & 0o777, 0o600);
    const before = await readFile(path, 'utf8');
    await assert.rejects(registry.assign({ uid: 'AA01', target: 'mode:Implement', revision: 2 }), /already assigned/);
    await assert.rejects(registry.assign({ uid: 'AA03', target: 'person:vasubandhu', revision: 0 }), /Assignments changed/);
    await assert.rejects(registry.assign({ uid: 'AA03', target: 'person:unknown', revision: 2 }), /valid card/);
    assert.equal(await readFile(path, 'utf8'), before);
    const concurrent = await Promise.allSettled([
      registry.assign({ uid: 'AA03', target: 'mode:Expand', revision: 2 }),
      registry.assign({ uid: 'AA04', target: 'mode:Challenge', revision: 2 })
    ]);
    assert.equal(concurrent.filter(r => r.status === 'fulfilled').length, 1);
    await writeFile(path, '{broken');
    await assert.rejects(registry.assign({ uid: 'AA05', target: 'mode:Expand', revision: 3 }));
    assert.equal(await readFile(path, 'utf8'), '{broken');
  } finally { await rm(dir, { recursive: true, force: true }); }
});
test('mode lift clears mode; most recent portrait wins and returns to remaining portrait', () => {
  const { cards, actions, place } = fixture();
  place('left', 'AA01'); cards.remove('left', 'AA01', false);
  assert.deepEqual(actions, [{ type: 'MODE', value: 'Structure', passive: false }, { type: 'MODE', value: null, preservePerson: true, passive: true }]);
  place('left', 'AA02'); place('right', 'AA03');
  cards.remove('left', 'AA02', false);
  assert.equal(actions.at(-1).value, 'vasubandhu');
  place('left', 'AA02'); cards.remove('left', 'AA02', false);
  assert.equal(actions.at(-1).value, 'vasubandhu');
  cards.remove('right', 'AA03', false);
  assert.equal(actions.at(-1).value, null);
});
test('two copies of same portrait do not end visit until both leave; stale removals ignored', () => {
  const { cards, actions, place } = fixture();
  place('left', 'AA02'); place('right', 'AA06');
  cards.remove('right', 'AA06', false);
  assert.equal(actions.at(-1).value, 'jane-jacobs');
  cards.remove('left', 'AA03', false);
  assert.equal(cards.present.size, 1);
  cards.remove('left', 'AA02', false); assert.equal(actions.at(-1).value, null);
});
test('mode or manual selection dismisses present portraits until lifted and replaced', () => {
  const { cards, actions, place } = fixture();
  place('left', 'AA02'); place('right', 'AA01');
  cards.update({ left: reader('AA02'), right: reader('AA01') }, [], true);
  cards.remove('left', 'AA02', false); assert.equal(actions.at(-1).type, 'MODE');
  place('left', 'AA02'); cards.releaseOwnership();
  const count = actions.length; cards.remove('left', 'AA02', false);
  assert.equal(actions.length, count);
});
test('initial snapshots apply mode then perspective silently, never replay message/context', () => {
  const { cards, actions } = fixture();
  cards.update({ left: reader('AA02'), right: reader('AA01') }, [], true);
  assert.deepEqual(actions, [{ type: 'MODE', value: 'Structure', passive: true }, { type: 'PERSON', value: 'jane-jacobs', passive: true }]);
  cards.update({ left: reader('AA02'), right: reader('AA01') }, [], true);
  assert.equal(actions.length, 2);
  cards.disconnect(); assert.deepEqual(actions.at(-1), { type: 'PERSON', value: null, passive: true });
  cards.update({ left: reader('AA04'), right: reader('AA05') }, [], true);
  assert.equal(actions.length, 4);
});
test('snapshot repairs missing removal and reader errors without generating a response', () => {
  const { cards, actions, place } = fixture();
  place('left', 'AA02'); place('right', 'AA03');
  cards.update({ left: reader('AA02'), right: { ...reader('AA03'), error: 'reader gone' } });
  assert.deepEqual(actions.at(-1), { type: 'PERSON', value: 'jane-jacobs', passive: true });
  cards.update({}); assert.equal(actions.at(-1).value, null);
});
test('mode follows present tags, including duplicates, fallback and reader loss', () => {
  const { cards, actions, place } = fixture();
  cards.bindings.set('BB01', 'mode:Structure');
  cards.bindings.set('BB02', 'mode:Expand');
  assert.equal(cards.mode, null);
  place('left', 'AA01'); place('right', 'BB01');
  const count = actions.length;
  cards.remove('right', 'BB01', false);
  assert.equal(cards.mode, 'Structure'); assert.equal(actions.length, count);
  place('right', 'BB02'); assert.equal(cards.mode, 'Expand');
  cards.remove('right', 'BB02', false);
  assert.equal(cards.mode, 'Structure'); assert.equal(actions.at(-1).passive, true);
  cards.update({ left: { ...reader('AA01'), error: 'Reader lost' } });
  assert.equal(cards.mode, null); assert.equal(actions.at(-1).value, null);
  cards.update({ left: reader('AA01') }, [], true);
  assert.equal(cards.mode, 'Structure'); assert.equal(actions.at(-1).passive, true);
  cards.update({ left: reader(null) });
  assert.equal(cards.mode, null);
});
test('lifting a mode preserves a remaining portrait and never starts a response', () => {
  const { cards, actions, place } = fixture();
  place('left', 'AA01'); place('right', 'AA02');
  cards.remove('left', 'AA01', false);
  assert.equal(cards.owner, 'right');
  assert.deepEqual(actions.at(-1), { type: 'MODE', value: null, preservePerson: true, passive: true });
});
test('fresh card edges activate once, context lift persists, resync never replays messages', () => {
  const { cards, actions } = fixture();
  const event = { type: 'card_present', reader_id: 'left', uid: 'AA04' };
  cards.update({ left: reader('AA04') }, [event]);
  cards.update({ left: reader('AA04') }, [event]);
  assert.deepEqual(actions, [{ type: 'MESSAGE', value: 'continue', passive: false }]);
  cards.update({ left: reader('AA05') }, [{ ...event, uid: 'AA05' }]);
  cards.update({}); assert.equal(actions.at(-1).type, 'CONTEXT');
  cards.update({ left: reader('AA04') }, [event], true);
  assert.equal(actions.length, 2);
});
test('every new portrait has a live consultation and returns to the selected mode', () => {
  for (const person of ['jane-jacobs', 'james-baldwin', 'lucille-ball', 'leonardo', 'vasubandhu', 'freud']) {
    const body = liveBody({ mode: 'Implement', person, controls: { wild: 0, speed: 3, initiative: 3 }, messages: [{ role: 'user', content: 'Help me develop this idea.' }], contexts: [] });
    assert.match(body.instructions, /Suspend the Implement activity/);
    assert.match(body.instructions, /returns to Implement/);
    assert.match(body.instructions, /do not claim to be the actual person/);
  }
});
test('Freud tag visits, falls back to another portrait and preserves the mode', () => {
  const { cards, actions, place } = fixture();
  assert.equal(validCardTarget('person:freud'), true);
  cards.bindings.set('BB03', 'person:freud');
  place('mode', 'AA01'); place('left', 'AA02'); place('right', 'BB03');
  assert.equal(actions.at(-1).value, 'freud');
  assert.equal(cards.mode, 'Structure');
  cards.remove('right', 'BB03', false);
  assert.equal(actions.at(-1).value, 'jane-jacobs');
  cards.remove('left', 'AA02', false);
  assert.equal(actions.at(-1).value, null);
  assert.equal(cards.mode, 'Structure');
});
