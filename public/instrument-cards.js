/* Reader reconciliation, shared by the browser and Node tests. No network or AI calls. */
globalThis.InstrumentCards = class InstrumentCards {
  constructor({ bindings, act }) {
    this.bindings = bindings;
    this.act = act;
    this.present = new Map();
    this.owner = null;
    this.mode = null;
  }
  syncMode(passive, explicit = false) {
    const card = [...this.present.values()].reverse().find(c => c.target?.startsWith('mode:'));
    const next = card ? card.target.slice(5) : null;
    if (next === this.mode && !explicit) return;
    this.mode = next;
    this.act({ type: 'MODE', value: next, ...(!explicit ? { preservePerson: true } : {}) }, passive);
  }
  // An explicit on-screen choice or mode card dismisses all existing visits.
  releaseOwnership() {
    this.owner = null;
    for (const card of this.present.values()) card.dismissed = true;
  }
  remove(reader, uid, passive) {
    const card = this.present.get(reader);
    if (!card || card.uid !== uid) return;
    this.present.delete(reader);
    // Lift/loss changes the mode without generating a new response. Another
    // remaining mode tag can become current; duplicates keep the same mode.
    if (card.target?.startsWith('mode:')) this.syncMode(true);
    if (this.owner !== reader) return;
    const fallback = [...this.present].reverse().find(([, c]) => !c.dismissed && c.target?.startsWith('person:'));
    this.owner = fallback?.[0] ?? null;
    this.act({ type: 'PERSON', value: fallback ? fallback[1].target.slice(7) : null }, passive);
  }
  place(reader, uid, passive) {
    if (this.present.get(reader)?.uid === uid) return;
    if (this.present.has(reader)) this.remove(reader, this.present.get(reader).uid, passive);
    const target = this.bindings.get(uid);
    this.present.set(reader, { uid, target, dismissed: false });
    if (!target) return;
    const [kind, value] = target.split(':');
    if (kind === 'mode') {
      this.releaseOwnership();
      this.syncMode(passive, true);
    } else if (kind === 'person') {
      this.owner = reader;
      this.act({ type: 'PERSON', value }, passive);
    } else if (!passive && kind === 'context') this.act({ type: 'CONTEXT', value }, false);
    else if (!passive && kind === 'message') this.act({ type: 'MESSAGE', value }, false);
  }
  update(readers, events = [], resync = false) {
    if (!resync) for (const event of events) {
      if (event.type === 'card_present') this.place(event.reader_id, event.uid, false);
      if (event.type === 'card_removed') this.remove(event.reader_id, event.uid, event.reason === 'reader_unavailable');
    }
    const healthy = new Map(Object.entries(readers).filter(([, r]) => r.connected && !r.error && r.uid));
    // Snapshot is authoritative even after missed events, reader loss or a service restart.
    for (const [reader, card] of this.present) {
      if (healthy.get(reader)?.uid !== card.uid) this.remove(reader, card.uid, true);
    }
    // On initial connection, establish the mode before a simultaneous portrait visit.
    const ordered = [...healthy].sort((a, b) => Number(this.bindings.get(b[1].uid)?.startsWith('mode:')) - Number(this.bindings.get(a[1].uid)?.startsWith('mode:')));
    for (const [reader, card] of ordered) this.place(reader, card.uid, true);
  }
  disconnect() { this.update({}, [], true); }
};
