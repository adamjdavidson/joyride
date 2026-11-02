/* Real streaming adapter. The standalone scripted instrument remains the default. */
const Live = (() => {
  let enabled = false, token = '', status = null, request = null, epoch = 0, timer = null;
  let demoMessages = null, liveMessages = [], resumeNeeded = false;
  const headers = () => ({ 'Content-Type': 'application/json', 'X-Instrument-Token': token });
  const hasThought = () => messages.some(m => m.role === 'user' && m.shown.trim());
  async function refreshStatus() {
    const response = await fetch('/api/live/status');
    if (!response.ok) throw new Error('Open this through your local Joyride address to use live AI.');
    status = await response.json(); token = status.token; renderStatus(); return status;
  }
  function renderStatus() {
    $('liveToggle').textContent = enabled ? 'Switch to scripted' : 'Use live AI';
    $('engineBadge').textContent = enabled ? '● Live AI' : '● Scripted demo';
    $('liveComposer').hidden = !enabled;
    document.querySelector('.transport').hidden = enabled;
    $('startDemo').hidden = enabled;
    $('conversation').setAttribute('aria-label', enabled ? 'Live conversation' : 'Scripted conversation');
    if (enabled) {
      $('composerHint').textContent = state.stopped ? 'Stopped. Release the brake, then tap gas to continue.' : 'Steer while it responds. Settings change the next continuation.';
      $('projectDescription').textContent = 'A live thought and writing partner. Steer the reply as it arrives.';
      $('connectionLabel').textContent = `${hardwareRunning ? 'Physical controls connected' : 'On-screen controls'} · live AI · text sent to OpenAI`;
      $('engineFootnote').textContent = 'Real model output · context samples remain illustrative';
      $('liveBudget').textContent = status ? `${status.model} · $${status.accounted.toFixed(3)} accounted / $${status.budget.toFixed(2)} budget (includes interrupted-call reservations)` : 'Checking live connection…';
      if (request && activeMessage && !activeMessage.shown) $('statusText').textContent = 'Thinking · controls can interrupt';
    } else {
      $('projectDescription').textContent = 'An initial experiment in steering a conversation. Prepared responses; real interaction.';
      $('engineFootnote').textContent = 'Scripted responses & illustrative context · changes reset on reload';
      $('connectionLabel').textContent = `${hardwareRunning ? 'Physical controls connected' : 'On-screen controls'} · no AI calls`;
    }
  }
  function interrupt(note = 'Interrupted by your steering.') {
    clearTimeout(timer); timer = null; ++epoch;
    if (request) {
      const old = request; request = null; old.controller.abort();
      // Explicit cancellation also reaches the server if the reverse proxy holds the stream open.
      fetch('/api/live/stop', { method: 'POST', headers: headers(), body: JSON.stringify({ id: old.id }), keepalive: true }).catch(() => {});
    }
    if (activeMessage) { activeMessage.annotation = note; activeMessage = null; messages = messages.filter(m => m.shown); }
  }
  function controls(before, previousSignature, action) {
    if (!state.mode) {
      resumeNeeded = false;
      interrupt('Mode tag removed. Choose an activity or place a mode tag to continue.');
      renderMessages(); render(); return;
    }
    const changed = previousSignature !== signature(state);
    if (state.stopped) {
      if (!before.stopped) { resumeNeeded = Boolean(request || timer || hasThought()); interrupt('Stopped. Tap gas to continue.'); renderMessages(); render(); }
      return;
    }
    if (before.stopped && resumeNeeded) { resumeNeeded = false; start('Resumed'); return; }
    if (!changed || !hasThought()) return;
    const running = Boolean(request || timer);
    if (action.type === 'RELEASE' && !running) return;
    interrupt('Settings changed; continuing with your new direction.');
    timer = setTimeout(() => { timer = null; start('Following your lead'); }, 400);
    renderMessages(); render();
  }
  async function start(reason = 'Continuing') {
    interrupt();
    if (!enabled || state.stopped) return;
    if (!state.mode) { say('Choose an activity or place a mode tag to continue.'); return; }
    if (!hasThought()) { $('liveInput').focus(); say('Enter a thought to start the live conversation.'); return; }
    $('liveError').textContent = '';
    const current = ++epoch, id = crypto.randomUUID(), controller = new AbortController();
    const v = Instrument.values(state);
    const history = messages.filter(m => m.shown).map(m => ({ role: m.role, content: m.shown }));
    const payload = { id, mode: state.mode, person: state.person, controls: { wild: v.wild, speed: v.speed, initiative: v.initiative },
      contexts: state.contexts.map(key => books.find(b => b.id === key)).filter(Boolean).map(({ name, body }) => ({ name, body })),
      messages: history, continuing: history.at(-1)?.role === 'assistant' || reason !== 'Your thought' };
    const message = { id: ++revision, role: 'assistant', speaker: state.person ? `${people[state.person].name} · perspective` : `Thinking partner · ${state.mode}`, tag: 'Live · ' + (state.person ? 'consultation' : state.mode), shown: '', annotation: `${wildNames[v.offset + reach]} · ${speedNames[v.speed]} · initiative ${v.initiative}` };
    request = { id, controller }; activeMessage = message; messages.push(message); renderMessages(); render();
    let terminal = false;
    try {
      const response = await fetch('/api/live/respond', { method: 'POST', headers: headers(), body: JSON.stringify(payload), signal: controller.signal });
      if (!response.ok) {
        const body = await response.text(); let error; try { error = JSON.parse(body).error; } catch {}
        throw new Error(error || `Live request failed (HTTP ${response.status}). Reload if the server restarted.`);
      }
      const reader = response.body.getReader(), decoder = new TextDecoder(); let buffer = '';
      while (true) {
        const chunk = await reader.read(); if (chunk.done || current !== epoch) break;
        buffer += decoder.decode(chunk.value, { stream: true }); let newline;
        while ((newline = buffer.indexOf('\n')) >= 0) {
          const line = buffer.slice(0, newline); buffer = buffer.slice(newline + 1); if (!line.trim()) continue;
          const event = JSON.parse(line);
          if (current !== epoch) return;
          if (event.type === 'delta') { message.shown += event.text; renderMessages(); }
          if (event.type === 'start') { message.annotation += ` · reasoning ${event.effort}`; renderMessages(); }
          if (event.type === 'error') throw new Error(event.message);
          if (event.type === 'done') {
            terminal = true; status = { ...status, ...event };
            if (event.incomplete) message.annotation += ' · Output limit reached; Continue for more.';
            if (!message.shown) message.annotation += ' · No visible text returned; try more speed or Continue.';
          }
        }
      }
      if (current === epoch && !terminal) throw new Error('Connection ended early. Continue to resume from the visible text.');
    } catch (error) {
      if (current === epoch && !controller.signal.aborted) { $('liveError').textContent = error.message; message.annotation += ' · Interrupted by connection error.'; }
    } finally {
      if (current === epoch) { request = null; activeMessage = null; renderMessages(); render(); refreshStatus().catch(() => {}); }
    }
  }
  async function toggle() {
    $('liveToggle').disabled = true;
    try {
      if (!enabled) {
        await refreshStatus();
        if (!status.ready) throw new Error('Live AI needs your own API key and budget. See the README for setup.');
        pauseTour(); clearTimeout(streamTimer); clearTimeout(steerTimer);
        activeMessage = null; demoMessages = messages; messages = liveMessages;
        enabled = true; $('liveError').textContent = ''; renderMessages(); render(); $('liveInput').focus();
      } else {
        interrupt('Live session paused.'); liveMessages = messages; enabled = false; messages = demoMessages || [];
        renderMessages(); render();
      }
    } catch (error) { say(error.message); }
    finally { $('liveToggle').disabled = false; }
  }
  function reset() { interrupt(); resumeNeeded = false; liveMessages = []; if (enabled) messages = []; $('liveError').textContent = ''; }
  function submit() {
    const text = $('liveInput').value.trim(); if (!text) return;
    interrupt('A new thought arrived.'); messages.push({ id: ++revision, role: 'user', speaker: 'You', tag: 'Your thought', shown: text }); $('liveInput').value = '';
    renderMessages();
    if (state.stopped) { resumeNeeded = true; say('Thought added. Tap gas when you want to resume.'); render(); }
    else start('Your thought');
  }
  function init() {
    $('liveToggle').onclick = toggle; $('liveSend').onclick = submit;
    $('liveInput').addEventListener('keydown', event => { if (event.key === 'Enter' && !event.shiftKey) { event.preventDefault(); submit(); } });
    window.addEventListener('pagehide', () => { if (enabled) interrupt('Page closed.'); });
    renderStatus();
  }
  return { get enabled() { return enabled; }, init, start, controls, reset, render: renderStatus };
})();
