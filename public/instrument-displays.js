/* Publish instrument metadata to the server; no chat text or context bodies. */
const PhysicalDisplays = (() => {
  let initialized = false, token = '', pending = false, queued = false, timer = null, sequence = 0, last = '', lastSent = 0;
  const client = globalThis.crypto?.randomUUID?.() || 'file-preview-disabled';
  function snapshot() {
    const value = Instrument.values(state);
    return { mode: state.mode, person: state.person, engine: Live.enabled ? 'live' : 'scripted',
      status: state.stopped ? 'stopped' : activeMessage ? (activeMessage.shown ? 'responding' : 'thinking') : Live.enabled && $('liveError').textContent ? 'error' : 'ready',
      controls: { wild: value.wild, offset: value.offset, speed: value.speed, initiative: value.initiative },
      defaults: { ...state.defaults }, factory: state.factory, removeNext: state.removeNext,
      contexts: state.contexts.map(id => books.find(book => book.id === id)?.name || id) };
  }
  function label(text) { if ($('displayConnection')) $('displayConnection').textContent = text; }
  async function send() {
    timer = null;
    if (!initialized) return;
    if (pending) { queued = true; return; }
    const stateValue = snapshot(), encoded = JSON.stringify(stateValue);
    if (encoded === last && Date.now() - lastSent < 900) return;
    pending = true;
    try {
      if (!token) {
        const result = await fetch('/api/displays/state', { signal: AbortSignal.timeout(2500) });
        if (!result.ok) throw new Error('unavailable');
        token = (await result.json()).token;
      }
      const response = await fetch('/api/displays/state', { method: 'POST', headers: { 'Content-Type': 'application/json', 'X-Instrument-Token': token },
        body: JSON.stringify({ ...stateValue, client, seq: ++sequence }), signal: AbortSignal.timeout(2500) });
      if (response.status === 403) { token = ''; throw new Error('reconnecting'); }
      if (response.status === 409) { label('Mode display · another tab is connected'); return; }
      if (!response.ok) throw new Error('unavailable');
      const result = await response.json(); last = encoded; lastSent = Date.now();
      label(result.device.link === 'connected' ? `Mode display · ${result.device.transport === 'wifi' ? 'Wi-Fi' : 'USB'} connected` : result.device.link === 'disabled' ? 'Mode display · not configured' : 'Mode display · ' + result.device.link);
    } catch { label('Mode display · reconnecting'); }
    finally { pending = false; if (queued) { queued = false; queue(); } }
  }
  function queue() { if (!initialized || timer) return; timer = setTimeout(send, 35); }
  function init() {
    if (!['http:', 'https:'].includes(location.protocol)) { label('Mode display · open the Joyride address to connect'); return; }
    initialized = true; queue(); setInterval(queue, 1000);
  }
  return { init, queue, snapshot };
})();
