// Scripted preview, hardware bridge and opt-in live streaming instrument.
import http from 'node:http';
import { readFile } from 'node:fs/promises';
import { mkdirSync, chmodSync, readFileSync } from 'node:fs';
import { randomBytes } from 'node:crypto';
import { ApiSettings } from '../src/api-settings.js';
import { Store } from '../src/store.js';
import { InstrumentLive } from '../src/instrument-live.js';
import { DisplayState, SerialDisplay } from '../src/instrument-displays.js';
import { WifiDisplays } from '../src/wifi-displays.js';
import { CardRegistry } from '../src/instrument-cards.js';
import { fileURLToPath } from 'node:url';

process.umask(0o077);
const dataDirectory=new URL('../data/',import.meta.url);
mkdirSync(dataDirectory,{recursive:true,mode:0o700});
chmodSync(dataDirectory,0o700);
const config = new ApiSettings(new URL('../data/api-settings.json', import.meta.url)).load({ mode: process.env.PILOT_MODE || 'demo', budget: Number(process.env.PILOT_BUDGET_USD || 0), key: process.env.OPENAI_API_KEY });
if (!['demo','live'].includes(config.mode) || !Number.isFinite(config.budget) || config.budget < 0) throw new Error('Invalid live configuration.');
const store = new Store(new URL('../data/pilot.sqlite', import.meta.url), { recover: false });
const live = new InstrumentLive({ config, store });
const token = randomBytes(32).toString('hex');
const cards = new CardRegistry(fileURLToPath(new URL('../data/instrument-cards.json', import.meta.url)));
const displayState = new DisplayState();
let wifiConfig;
try { wifiConfig = JSON.parse(readFileSync(new URL('../data/displays/wifi.json', import.meta.url), 'utf8')); }
catch (error) { if (error.code !== 'ENOENT') throw new Error('Cannot load private display Wi-Fi configuration.'); }
let display;
const displayHeartbeat = setInterval(() => display?.send(displayState.packet()), 1000);
displayHeartbeat.unref();

const port = Number(process.env.DEMO_PORT || 8788);
const allowedHosts = new Set([`127.0.0.1:${port}`, `localhost:${port}`, `[::1]:${port}`]);
// Tailscale Serve preserves the browser's Host header. Allow only the configured
// private preview address in addition to loopback; the listener stays local.
if (process.env.DEMO_PUBLIC_ORIGIN) allowedHosts.add(new URL(process.env.DEMO_PUBLIC_ORIGIN).host);
const origins = new Set([`http://127.0.0.1:${port}`, `http://localhost:${port}`, `http://[::1]:${port}`, ...(process.env.DEMO_PUBLIC_ORIGIN ? [process.env.DEMO_PUBLIC_ORIGIN] : [])]);
const file = new URL('../public/instrument-prototype.html', import.meta.url);
const services = { wheel: 8766, nfc: 8767 };
const server = http.createServer(async (req, res) => {
  const host = req.headers.host || '';
  if (!allowedHosts.has(host)) {
    res.writeHead(403).end('Open this demo through its configured preview address.'); return;
  }
  const url = new URL(req.url, `http://127.0.0.1:${port}`);
  res.setHeader('Cache-Control', 'no-store');
  res.setHeader('X-Content-Type-Options', 'nosniff');
  res.setHeader('Referrer-Policy', 'no-referrer');
  res.setHeader('Content-Security-Policy', "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; connect-src 'self'; img-src 'self' data:; frame-ancestors 'none'; base-uri 'none'; form-action 'none'");
  try {
    if (url.pathname === '/api/cards') {
      if (req.method === 'GET') {
        if (req.headers['sec-fetch-site'] === 'cross-site') { res.writeHead(403).end(); return; }
        res.setHeader('Content-Type', 'application/json');
        res.end(JSON.stringify({ ...await cards.read(), token })); return;
      }
      if (req.method !== 'POST' || !origins.has(req.headers.origin) || req.headers['x-instrument-token'] !== token || req.headers['content-type'] !== 'application/json') { res.writeHead(403).end(); return; }
      const chunks = []; let bytes = 0;
      for await (const chunk of req) { bytes += chunk.length; if (bytes > 2048) { res.writeHead(413).end(); return; } chunks.push(chunk); }
      try {
        const input = JSON.parse(Buffer.concat(chunks));
        const readerResponse = await fetch('http://127.0.0.1:8767/state', { signal: AbortSignal.timeout(1500) });
        if (!readerResponse.ok) throw new Error('Reader service unavailable.');
        const snapshot = await readerResponse.json();
        const present = Object.values(snapshot.state.readers).filter(r => r.connected && !r.error && r.uid);
        if (present.length !== 1 || present[0].uid !== input.uid) throw new Error('Present only the card you want to assign, then try again.');
        res.setHeader('Content-Type', 'application/json');
        res.end(JSON.stringify({ ...await cards.assign(input), token }));
      } catch (error) {
        res.writeHead(409, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: error.message }));
      }
      return;
    }
    if (url.pathname === '/api/displays/state') {
      if (req.method === 'GET') {
        if (req.headers['sec-fetch-site'] === 'cross-site') { res.writeHead(403).end(); return; }
        res.setHeader('Content-Type', 'application/json');
        res.end(JSON.stringify({ ...displayState.packet(), device: display.status(), token })); return;
      }
      if (req.method !== 'POST' || !origins.has(req.headers.origin) || req.headers['x-instrument-token'] !== token || req.headers['content-type'] !== 'application/json') { res.writeHead(403).end('Open the instrument page to drive displays.'); return; }
      const chunks = []; let size = 0;
      for await (const chunk of req) { size += chunk.length; if (size > 10000) { res.writeHead(413).end(); return; } chunks.push(chunk); }
      try { displayState.publish(JSON.parse(Buffer.concat(chunks))); }
      catch (error) { res.writeHead(409, { 'Content-Type': 'application/json' }); res.end(JSON.stringify({ error: error.message })); return; }
      display.send(displayState.packet());
      res.setHeader('Content-Type', 'application/json'); res.end(JSON.stringify({ accepted: displayState.revision, device: display.status() })); return;
    }
    if (url.pathname.startsWith('/api/live/')) {
      if (req.method === 'GET' && url.pathname === '/api/live/status') {
        if (req.headers['sec-fetch-site'] === 'cross-site') { res.writeHead(403).end(); return; }
        res.setHeader('Content-Type', 'application/json'); res.end(JSON.stringify({ ...live.status(), token })); return;
      }
      if (req.method !== 'POST' || !origins.has(req.headers.origin) || req.headers['x-instrument-token'] !== token || req.headers['content-type'] !== 'application/json') { res.writeHead(403).end('Use the instrument page to control live AI.'); return; }
      if (!['/api/live/respond', '/api/live/stop'].includes(url.pathname)) { res.writeHead(404).end(); return; }
      const chunks = []; let bytes = 0;
      for await (const chunk of req) { bytes += chunk.length; if (bytes > 200000) { res.writeHead(413).end('Conversation too large.'); return; } chunks.push(chunk); }
      let data; try { data = JSON.parse(Buffer.concat(chunks)); } catch { res.writeHead(400).end('Invalid request.'); return; }
      if (url.pathname === '/api/live/stop') { live.stop(data.id); res.writeHead(204).end(); return; }
      try { await live.stream(data, res); }
      catch (error) { if (!res.headersSent) { res.writeHead(400, { 'Content-Type': 'application/json' }); res.end(JSON.stringify({ error: error.message })); } }
      return;
    }
    if (req.method !== 'GET') { res.writeHead(405).end('Method not allowed'); return; }
    if (url.pathname === '/' || url.pathname === '/instrument-prototype.html') {
      res.setHeader('Content-Type', 'text/html; charset=utf-8'); res.end(await readFile(file)); return;
    }
    if (url.pathname === '/favicon.ico') { res.writeHead(204).end(); return; }
    if (['/instrument-live.js', '/instrument-displays.js', '/joyride-config.js', '/instrument-cards.js'].includes(url.pathname)) {
      res.setHeader('Content-Type', 'text/javascript'); res.end(await readFile(new URL('../public' + url.pathname, import.meta.url))); return;
    }
    const match = url.pathname.match(/^\/hardware\/(wheel|nfc)\/(state|events)$/);
    if (!match) { res.writeHead(404).end('Not found'); return; }
    const target = new URL(`http://127.0.0.1:${services[match[1]]}/${match[2]}`);
    if (match[2] === 'events') {
      for (const key of ['after', 'session']) if (url.searchParams.has(key)) target.searchParams.set(key, url.searchParams.get(key));
    }
    const response = await fetch(target, { signal: AbortSignal.timeout(1000) });
    res.writeHead(response.status, { 'Content-Type': 'application/json' });
    res.end(await response.text());
  } catch {
    res.writeHead(503, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ error: 'Local input service is not running. On-screen controls remain available.' }));
  }
});
server.on('close', () => { clearInterval(displayHeartbeat); display?.close(); });
for (const signal of ['SIGINT', 'SIGTERM']) process.on(signal, () => {
  clearInterval(displayHeartbeat); display?.close(); server.close();
  setTimeout(() => process.exit(0), 1500).unref();
});
server.listen(port, '127.0.0.1', () => {
  if (wifiConfig) { display = new WifiDisplays(wifiConfig); display.start(); }
  else display = new SerialDisplay(process.env.INSTRUMENT_DISPLAY_PORT);
  console.log(`Joyride: http://localhost:${port}\nScripted by default; Live AI is opt-in. Ctrl-C stops this preview.`);
});
