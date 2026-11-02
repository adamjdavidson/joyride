import tls from 'node:tls';
import { readFileSync } from 'node:fs';
import { networkInterfaces } from 'node:os';
import { timingSafeEqual } from 'node:crypto';

function equalSecret(actual, expected) {
  if (typeof actual !== 'string' || typeof expected !== 'string') return false;
  const a = Buffer.from(actual), b = Buffer.from(expected);
  return a.length === b.length && timingSafeEqual(a, b);
}

// A separate TLS listener exposes display metadata only, never the app or AI API.
export class WifiDisplays {
  constructor(config, { now = Date.now } = {}) {
    this.config = config; this.now = now; this.peers = new Map(); this.last = null;
    this.server = null; this.link = 'connecting'; this.address = null;
  }
  start() {
    const entry = networkInterfaces()[this.config.interface]?.find(x => x.family === 'IPv4' && !x.internal);
    if (!entry) { this.link = 'Wi-Fi interface unavailable'; return; }
    this.address = entry.address;
    this.server = tls.createServer({ key: readFileSync(this.config.keyFile), cert: readFileSync(this.config.certFile), minVersion: 'TLSv1.2', maxVersion: 'TLSv1.2', handshakeTimeout: 5000 }, socket => this.accept(socket));
    this.server.on('tlsClientError', () => {});
    this.server.on('error', () => { this.link = 'wireless listener unavailable'; });
    this.server.listen(this.config.port, this.address, () => { this.link = 'waiting for Wi-Fi display'; });
  }
  accept(socket) {
    let id = null, buffer = '';
    const authTimeout = setTimeout(() => socket.destroy(), 5000); authTimeout.unref();
    socket.setNoDelay(true); socket.setKeepAlive(true, 3000); socket.setTimeout(12000, () => socket.destroy());
    socket.setEncoding('utf8');
    socket.on('error', () => {});
    socket.on('data', chunk => {
      buffer += chunk;
      if (buffer.length > 4096) { socket.destroy(); return; }
      let newline;
      while ((newline = buffer.indexOf('\n')) >= 0) {
        const line = buffer.slice(0, newline); buffer = buffer.slice(newline + 1);
        let event; try { event = JSON.parse(line); } catch { socket.destroy(); return; }
        if (!event || typeof event !== 'object') { socket.destroy(); return; }
        if (!id) {
          const registration = this.config.devices.find(x => x.id === event.id);
          if (event.type !== 'hello' || event.v !== 1 || !registration || !equalSecret(event.token, registration.token)) { socket.destroy(); return; }
          id = registration.id; clearTimeout(authTimeout);
          this.peers.get(id)?.socket.destroy();
          this.peers.set(id, { socket, role: registration.role, ack: null, at: 0 });
          if (this.last) this.write(socket, this.last);
        } else if (event.type === 'display_ack' && event.v === 1 && Number.isSafeInteger(event.seq) && typeof event.connected === 'boolean') {
          const peer = this.peers.get(id);
          if (peer?.socket === socket) { peer.ack = { seq: event.seq, connected: event.connected }; peer.at = this.now(); }
        } else { socket.destroy(); return; }
      }
    });
    socket.on('close', () => { clearTimeout(authTimeout); if (id && this.peers.get(id)?.socket === socket) this.peers.delete(id); });
  }
  write(socket, packet) {
    if (!socket.destroyed && socket.writableLength < 16384) socket.write(JSON.stringify(packet) + '\n');
    else socket.destroy();
  }
  send(packet) { this.last = packet; for (const peer of this.peers.values()) this.write(peer.socket, packet); }
  status() {
    const peers = this.config.devices.map(device => {
      const peer = this.peers.get(device.id), age = peer?.at ? this.now() - peer.at : null;
      return { id: device.id, role: device.role, transport: 'wifi', link: peer ? (age !== null && age < 4000 ? 'connected' : 'waiting for acknowledgment') : this.link,
        acknowledged: peer?.ack?.seq ?? null, hasDesk: peer?.ack?.connected === true, ackAgeMs: age };
    });
    const mode = peers.find(x => x.role === 'mode');
    return { ...(mode || { link: this.link }), transport: 'wifi', devices: peers };
  }
  close() { for (const peer of this.peers.values()) peer.socket.destroy(); this.server?.close(); }
}
