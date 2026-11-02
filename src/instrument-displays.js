import { spawn } from 'node:child_process';
import { createInterface } from 'node:readline';

import '../public/joyride-config.js';
const config=globalThis.JoyrideConfig;
const modes=new Set(Object.keys(config.activities));
const people=new Set(Object.keys(config.perspectives));
const statuses = new Set(['ready', 'thinking', 'responding', 'stopped', 'error']);
function settings(value, defaults = false) {
  if (!value || !Number.isInteger(value.wild) || Math.abs(value.wild) > 1000 || !Number.isInteger(value.speed) || value.speed < 1 || value.speed > config.speed.length || !Number.isInteger(value.initiative) || value.initiative < 1 || value.initiative > config.initiative.length) throw new Error('Invalid display settings.');
  if (!defaults && (!Number.isInteger(value.offset) || Math.abs(value.offset) > (config.creative.labels.length-1)/2)) throw new Error('Invalid creative reach.');
  return { wild: value.wild, speed: value.speed, initiative: value.initiative, ...(!defaults ? { offset: value.offset } : {}) };
}
export function displaySnapshot(input) {
  if (!input || typeof input.client !== 'string' || !/^[a-zA-Z0-9-]{16,80}$/.test(input.client) || !Number.isSafeInteger(input.seq) || input.seq < 1) throw new Error('Invalid display publisher.');
  if (!(input.mode === null || modes.has(input.mode)) || !(input.person === null || people.has(input.person)) || !statuses.has(input.status) || !['live', 'scripted'].includes(input.engine)) throw new Error('Invalid display activity.');
  if (!Array.isArray(input.contexts) || input.contexts.length > 50 || input.contexts.some(x => typeof x !== 'string' || x.length > 80)) throw new Error('Invalid display contexts.');
  // Explicit projection: never forward chat, context bodies, tokens or credentials.
  const snapshot = { client: input.client, seq: input.seq, mode: input.mode, person: input.person, status: input.status, engine: input.engine,
    controls: settings(input.controls), defaults: settings(input.defaults, true), contexts: input.contexts,
    factory: input.factory === true, removeNext: input.removeNext === true };
  if (Buffer.byteLength(JSON.stringify(snapshot)) > 6500) throw new Error('Too much display metadata.');
  return snapshot;
}

export class DisplayState {
  constructor({ now = Date.now } = {}) { this.now = now; this.state = null; this.updated = 0; this.revision = 0; }
  publish(input) {
    const next = displaySnapshot(input), now = this.now();
    if (this.state && now - this.updated < 5000) {
      if (next.client !== this.state.client) throw new Error('Another instrument tab is driving the displays. Close it and retry.');
      if (next.seq <= this.state.seq) throw new Error('Display update arrived out of order.');
    }
    this.state = next; this.updated = now; this.revision++;
  }
  packet() {
    const connected = this.state !== null && this.now() - this.updated < 5000;
    return { v: 1, seq: this.revision, connected, state: connected ? this.state : null };
  }
}

export class SerialDisplay {
  constructor(device, { spawnImpl = spawn, now = Date.now } = {}) {
    this.now = now; this.child = null; this.lastAck = null; this.ackAt = 0; this.link = 'disabled';
    if (!device) return;
    this.link = 'connecting';
    this.child = spawnImpl('/usr/bin/python3', [new URL('../services/displays/usb_bridge.py', import.meta.url).pathname, '--device', device], { stdio: ['pipe', 'pipe', 'pipe'] });
    const lines = createInterface({ input: this.child.stdout });
    lines.on('line', line => {
      try {
        const event = JSON.parse(line);
        if (event.type === 'display_ack' && event.v === 1 && Number.isSafeInteger(event.seq)) { this.lastAck = event; this.ackAt = this.now(); this.link = 'connected'; }
        else if (event.type === 'link') this.link = event.state === 'open' ? 'waiting for board' : 'disconnected';
      } catch { /* Ignore boot chatter. */ }
    });
    this.child.stderr.on('data', () => {});
    this.child.stdin.on('error', () => { this.link = 'disconnected'; });
    this.child.on('error', () => { this.link = 'unavailable'; });
    this.child.on('exit', () => { this.link = 'disconnected'; this.child = null; });
  }
  send(packet) {
    if (this.child?.stdin.writable && this.child.stdin.writableLength < 16384) this.child.stdin.write(JSON.stringify(packet) + '\n');
  }
  status() {
    const age = this.ackAt ? this.now() - this.ackAt : null;
    return { link: this.link === 'connected' && age > 4000 ? 'not responding' : this.link, acknowledged: this.lastAck?.seq ?? null, hasDesk: this.lastAck?.connected === true, ackAgeMs: age };
  }
  close() { this.child?.stdin.end(); }
}
