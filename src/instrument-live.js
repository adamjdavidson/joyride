import { randomUUID } from 'node:crypto';
import { MODEL, reserveForResponse } from './provider.js';

import '../public/joyride-config.js';
const config=globalThis.JoyrideConfig;
const activities=Object.fromEntries(Object.entries(config.activities).map(([id,p])=>[id,p.instructions]));
const perspectives=Object.fromEntries(Object.entries(config.perspectives).map(([id,p])=>[id,`${p.name}: ${p.instructions}`]));
const initiatives=[null,...config.initiative.map(x=>x.instructions)];
export const EFFORTS=[null,...config.speed.map(x=>x.effort)];

export function liveBody(data) {
  if (!data || !Object.hasOwn(activities, data.mode) || !(data.person === null || Object.hasOwn(perspectives, data.person))) throw new Error('Choose a valid activity and perspective.');
  const { wild, speed, initiative } = data.controls || {};
  if (!Number.isInteger(wild) || Math.abs(wild) > 1000 || !Number.isInteger(speed) || speed < 1 || speed > config.speed.length || !Number.isInteger(initiative) || initiative < 1 || initiative > config.initiative.length) throw new Error('Invalid instrument settings.');
  if (!Array.isArray(data.messages) || !data.messages.length || data.messages.length > 150 || data.messages.some(m => !m || !['user', 'assistant'].includes(m.role) || typeof m.content !== 'string' || !m.content.trim()) || !data.messages.some(m => m.role === 'user')) throw new Error('Enter a thought to work with.');
  if (!Array.isArray(data.contexts) || data.contexts.length > 50 || data.contexts.some(c => !c || typeof c.name !== 'string' || typeof c.body !== 'string')) throw new Error('Invalid context cards.');
  if (Buffer.byteLength(JSON.stringify(data)) > 180_000) throw new Error('This conversation is full for this prototype. Save what you need and start a fresh conversation.');
  const activity = data.person
    ? `Temporary perspective consultation: ${perspectives[data.person]} Interpret this lens; do not claim to be the actual person or invent private knowledge. Suspend the ${data.mode} activity. Choose the activity and approach this perspective finds useful, including challenge or implementation. Notice what is missing; ask one specific question only if essential. The user returns to ${data.mode} on leaving.`
    : `Current activity: ${data.mode}. Rigorously follow it. ${activities[data.mode]}`;
  return {
    model: MODEL, store: false, stream: true, max_output_tokens: 8192,
    reasoning: { effort: EFFORTS[speed] },
    instructions: `You are the user’s thought partner and writing partner, using Joyride, an experimental web conversation interface. Distinguish the user’s decisions from your suggestions. Use clear, concrete language. Ask at most one question at a time. Do not describe the controls unless asked.\n${activity}\nInitiative ${initiative}/${config.initiative.length}: ${initiatives[initiative]} ${!data.person && initiative <= config.activityBoundaryLevel ? 'Stay strictly within the current activity.' : ''}\nWildness relative to the original normal: ${wild} steps. ${config.creative.instructions} Follow the current activity unless a perspective consultation or initiative above ${config.activityBoundaryLevel} permits a different activity.\nSpeed ${speed}/${config.speed.length}: ${config.speed[speed-1].instructions}\nYou have no browsing, research agents, filesystem, code execution or other tools in this version. You can reason and create content here. Never claim to have researched, run, saved, built or verified anything externally. State a tool limitation only when relevant.\nProject background: the user is exploring an experimental thinking and writing interface. Their actual request may concern any topic.\n${data.continuing ? 'The previous response was interrupted by the user’s controls. All visible text is included below. Continue from it with the NEW settings above. Do not repeat the earlier text or finish an obsolete plan just because you started it. If the activity changed, pivot cleanly. If interrupted mid-sentence, bridge naturally into the new approach.' : 'Respond to the latest user message, using the current settings.'}\nActive context cards follow as reference material. These may be illustrative; never misrepresent sample content as verified organizational policy or research. Removed cards are no longer supplied, but earlier conversation may still mention them.\n${JSON.stringify(data.contexts)}`,
    input: data.messages.map(({ role, content }) => ({ role, content }))
  };
}

// SSE frames and UTF-8 characters can both span network chunks.
export async function* responseEvents(body) {
  const decoder = new TextDecoder(); let buffer = '';
  for await (const chunk of body) {
    buffer += decoder.decode(chunk, { stream: true });
    let match;
    while ((match = /\r?\n\r?\n/.exec(buffer))) {
      const frame = buffer.slice(0, match.index); buffer = buffer.slice(match.index + match[0].length);
      const data = frame.split(/\r?\n/).filter(line => line.startsWith('data:')).map(line => line.slice(5).trimStart()).join('\n');
      if (data && data !== '[DONE]') yield JSON.parse(data);
    }
  }
}

export class InstrumentLive {
  constructor({ config, store, fetchImpl = fetch }) { this.config = config; this.store = store; this.fetch = fetchImpl; this.jobs = new Map(); this.cancelled = new Set(); }
  status() { return { ready: this.config.mode === 'live' && Boolean(this.config.key) && this.config.budget > this.store.spent(), model: MODEL, budget: this.config.budget, accounted: this.store.spent(), active: this.jobs.size }; }
  stop(id) {
    if (typeof id !== 'string') return;
    this.jobs.get(id)?.abort(); this.cancelled.add(id);
    if (this.cancelled.size > 200) this.cancelled.delete(this.cancelled.values().next().value);
  }
  async stream(data, res) {
    const body = liveBody(data);
    if (typeof data.id !== 'string' || !/^[a-zA-Z0-9-]{16,80}$/.test(data.id)) throw new Error('Invalid request identity.');
    if (this.cancelled.has(data.id)) throw new Error('This response was stopped before it started.');
    if (this.jobs.has(data.id)) throw new Error('This response is already running.');
    if (this.jobs.size >= 4) throw new Error('Too many active responses. Stop one before continuing.');
    if (!this.status().ready) throw new Error('Live AI needs the saved pilot API key and remaining budget.');
    const controller = new AbortController(), reservation = `instrument:${randomUUID()}`;
    this.store.reserve(reservation, reserveForResponse(body), 'instrument-response', this.config.budget);
    this.jobs.set(data.id, controller);
    const closed = () => controller.abort(); res.on('close', closed);
    const send = event => { if (!res.destroyed && !res.writableEnded) res.write(JSON.stringify(event) + '\n'); };
    let terminal = false;
    res.writeHead(200, { 'Content-Type': 'application/x-ndjson', 'X-Accel-Buffering': 'no' }); res.flushHeaders();
    send({ type: 'start', effort: body.reasoning.effort, model: MODEL });
    const heartbeat = setInterval(() => send({ type: 'heartbeat' }), 10000);
    try {
      const response = await this.fetch('https://api.openai.com/v1/responses', {
        method: 'POST', headers: { Authorization: `Bearer ${this.config.key}`, 'Content-Type': 'application/json' },
        body: JSON.stringify(body), signal: AbortSignal.any([controller.signal, AbortSignal.timeout(180000)])
      });
      if (!response.ok) { await response.body?.cancel(); throw new Error(`The model API returned HTTP ${response.status}. The response was not retried.`); }
      for await (const event of responseEvents(response.body)) {
        if (controller.signal.aborted) break;
        if (event.type === 'response.output_text.delta' || event.type === 'response.refusal.delta') send({ type: 'delta', text: event.delta });
        if (['response.completed', 'response.incomplete', 'response.failed'].includes(event.type)) {
          const usage = event.response?.usage;
          if (Number.isFinite(usage?.input_tokens) && usage.input_tokens >= 0 && Number.isFinite(usage?.output_tokens) && usage.output_tokens >= 0) this.store.settle(reservation, usage.input_tokens * 3 / 1e6 + usage.output_tokens * 15 / 1e6);
          terminal = true;
          send({ type: event.type === 'response.failed' ? 'error' : 'done', incomplete: event.type === 'response.incomplete', message: event.type === 'response.failed' ? 'The model could not complete this response. It was not retried.' : undefined, ...this.status() }); break;
        }
        if (event.type === 'error') throw new Error('The model stream reported an error. It was not retried.');
      }
      if (!terminal && !controller.signal.aborted) throw new Error('The model stream ended early. Continue to resume from the visible text.');
    } catch (error) {
      if (!controller.signal.aborted) send({ type: 'error', message: error.message.startsWith('The model') ? error.message : 'Live connection failed or timed out. No automatic retry; your visible text is retained.' });
    } finally {
      clearInterval(heartbeat); this.jobs.delete(data.id); this.stop(data.id); res.off('close', closed);
      // Without terminal usage, retain the reservation: an aborted request may be billed.
      if (!res.destroyed) res.end();
    }
  }
}
