import { readFile, writeFile, rename, unlink } from 'node:fs/promises';
import { randomUUID } from 'node:crypto';
import '../public/joyride-config.js';
const perspectives=globalThis.JoyrideConfig.perspectives;

const modes = new Set(Object.keys(globalThis.JoyrideConfig.activities));
export function validCardTarget(target) {
  if (typeof target !== 'string') return false;
  const [kind, value, extra] = target.split(':');
  return extra === undefined && (kind === 'mode' ? modes.has(value) : kind === 'person' && Object.hasOwn(perspectives, value));
}
export class CardRegistry {
  constructor(path) { this.path = path; this.pending = Promise.resolve(); }
  async read() {
    let data;
    try { data = JSON.parse(await readFile(this.path, 'utf8')); }
    catch (error) { if (error.code === 'ENOENT') return { version: 1, revision: 0, cards: {} }; throw error; }
    if (data.version !== 1 || !Number.isSafeInteger(data.revision) || data.revision < 0 || !data.cards || typeof data.cards !== 'object' || Array.isArray(data.cards) || Object.entries(data.cards).some(([uid, target]) => !/^[0-9A-F]{4,64}$/.test(uid) || !validCardTarget(target))) throw new Error('Invalid saved card registry; existing file was not changed.');
    return data;
  }
  assign(input) {
    const job = this.pending.then(async () => {
      if (!input || !/^[0-9A-F]{4,64}$/.test(input.uid) || !validCardTarget(input.target)) throw new Error('Choose a valid card, mode or perspective.');
      const data = await this.read();
      if (input.revision !== data.revision) throw new Error('Assignments changed. Reload the assignments and try again.');
      if (data.cards[input.uid] && data.cards[input.uid] !== input.target) throw new Error('This card is already assigned. Use a different card.');
      if (data.cards[input.uid] === input.target) return data;
      data.cards[input.uid] = input.target;
      data.revision++;
      const tmp = `${this.path}.${randomUUID()}.tmp`;
      try { await writeFile(tmp, JSON.stringify(data, null, 2) + '\n', { mode: 0o600 }); await rename(tmp, this.path); }
      finally { await unlink(tmp).catch(() => {}); }
      return data;
    });
    this.pending = job.catch(() => {});
    return job;
  }
}
