#!/usr/bin/env python3
"""Read-only NFC identity service; application meanings live in a separate registry."""
import argparse
from collections import deque
import copy
import json
import logging
from pathlib import Path
import re
import signal
import threading
import time
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit

from smartcard.System import readers
from smartcard.Exceptions import NoCardException, SmartcardException
from smartcard.scard import SCARD_LEAVE_CARD, SCARD_E_NO_SMARTCARD, SCARD_W_REMOVED_CARD


def load_registry(path):
    document = json.loads(Path(path).read_text())
    if not isinstance(document, dict) or document.get('version') != 1 or not isinstance(document.get('cards'), dict):
        raise ValueError('Expected version 1 and cards object')
    for uid, entry in document['cards'].items():
        if not re.fullmatch(r'(?:[0-9A-F]{2}){4,16}', uid):
            raise ValueError('Card IDs must be uppercase hexadecimal, 4–16 bytes')
        if not isinstance(entry, dict) or set(entry) - {'label', 'mode', 'memory'}:
            raise ValueError('Card entries support label, mode, memory only')
        if not entry or any(not isinstance(v, str) or not v.strip() for v in entry.values()):
            raise ValueError('Card fields must be nonempty strings')
    return document['cards']


def load_stations(path):
    document = json.loads(Path(path).read_text())
    if not isinstance(document, dict) or document.get('version') != 1 or not isinstance(document.get('readers'), dict):
        raise ValueError('Expected station version 1 and readers object')
    for serial, entry in document['readers'].items():
        if not serial or not isinstance(entry, dict) or set(entry) != {'label', 'role'}:
            raise ValueError('Each station needs label and role')
        if entry['role'] not in ('mode', 'message', 'context') or not isinstance(entry['label'], str) or not entry['label'].strip():
            raise ValueError('Invalid station role or label')
    return document['readers']


class Hub:
    def __init__(self, registry, capacity=1024, stations=None):
        self.lock = threading.Lock()
        self.registry = registry
        self.stations = copy.deepcopy(stations or {})
        self.session = str(uuid.uuid4())
        self.sequence = 0
        self.events = deque(maxlen=capacity)
        self.state = {'readers': {}, 'error': None}
        self.absent_since = {}

    def emit(self, kind, **fields):
        self.sequence += 1
        if 'reader_id' in fields:
            fields.update(self.stations.get(fields['reader_id'], {'role': None, 'label': None}))
        self.events.append({'sequence': self.sequence, 'time': time.time(), 'type': kind, **fields})

    def observe(self, reader, uid, error=None, now=None, name=None):
        now = time.monotonic() if now is None else now
        with self.lock:
            station = self.state['readers'].setdefault(reader, {
                **self.stations.get(reader, {'role': None, 'label': None}),
                'connected': False, 'reader': name or reader, 'uid': None,
                'assignment': None, 'error': None})
            changed = station['connected'] != (error is None) or station['error'] != error
            if changed:
                if station['uid']:
                    self.emit('card_removed', reader_id=reader, uid=station['uid'], reason='reader_unavailable')
                station.update(connected=error is None, error=error, uid=None, assignment=None)
                self.absent_since.pop(reader, None)
            station['reader'] = name or station['reader']
            if changed:
                self.emit('reader_status', reader_id=reader, state=copy.deepcopy(station))
            if error:
                return
            old = station['uid']
            if uid is None and old is not None:
                since = self.absent_since.setdefault(reader, now)
                if now - since < .35:
                    return
            else:
                self.absent_since.pop(reader, None)
            if uid == old:
                return
            if old:
                self.emit('card_removed', reader_id=reader, uid=old,
                          reason='removed' if uid is None else 'replaced')
            assignment = copy.deepcopy(self.registry.get(uid))
            station.update(uid=uid, assignment=assignment)
            if uid:
                self.emit('card_present', reader_id=reader, uid=uid, assignment=assignment)

    def reconcile(self, present, error=None):
        with self.lock:
            missing = set(self.state['readers']) - set(present)
            if self.state['error'] != error:
                self.state['error'] = error
                self.emit('service_status', error=error)
        for reader in missing:
            self.observe(reader, None, error or 'Reader disconnected')

    def reload(self, registry, stations=None):
        with self.lock:
            self.registry = registry
            if stations is not None:
                self.stations = copy.deepcopy(stations)
            for serial, station in self.state['readers'].items():
                station.update(self.stations.get(serial, {'role': None, 'label': None}))
                station['assignment'] = copy.deepcopy(registry.get(station['uid']))
            self.emit('registry_changed', state=copy.deepcopy(self.state))

    def snapshot(self, after=None, session=None):
        with self.lock:
            result = {'version': 2, 'session': self.session, 'cursor': self.sequence,
                      'state': copy.deepcopy(self.state)}
            if after is not None:
                oldest = self.events[0]['sequence'] if self.events else self.sequence + 1
                reset = session != self.session or after < oldest - 1 or after > self.sequence
                result.update(resync=reset, events=[] if reset else [copy.deepcopy(e) for e in self.events if e['sequence'] > after])
            return result


def read_uid(reader):
    connection = reader.createConnection()
    try:
        connection.connect(disposition=SCARD_LEAVE_CARD)
        data, sw1, sw2 = connection.transmit([0xFF, 0xCA, 0, 0, 0])
        if (sw1, sw2) != (0x90, 0) or not data:
            raise ValueError(f'UID read rejected: {sw1:02X}{sw2:02X}')
        return bytes(data).hex().upper()
    except SmartcardException as error:
        if isinstance(error, NoCardException) or error.hresult in (SCARD_E_NO_SMARTCARD, SCARD_W_REMOVED_CARD):
            return None
        raise
    finally:
        try:
            connection.disconnect()
        except SmartcardException as error:
            if error.hresult not in (SCARD_E_NO_SMARTCARD, SCARD_W_REMOVED_CARD):
                raise


def reader_id(reader):
    # PC/SC slot suffixes change on reconnect; ACS serials identify physical stations.
    match = re.search(r'\(([^()]+)\)\s+\d+\s+\d+$', str(reader))
    if not match:
        raise ValueError('Reader has no stable serial identifier: ' + str(reader))
    return match.group(1)


def poll_once(hub):
    try:
        available = [r for r in readers() if 'ACR1552' in str(r) and 'PICC' in str(r)]
        stations = {}
        for reader in available:
            serial = reader_id(reader)
            if serial in stations:
                raise ValueError('Duplicate NFC reader serial: ' + serial)
            stations[serial] = reader
    except (SmartcardException, ValueError) as error:
        hub.reconcile([], str(error))
        return
    hub.reconcile(stations)
    for serial, reader in stations.items():
        try:
            hub.observe(serial, read_uid(reader), name=str(reader))
        except (SmartcardException, ValueError) as error:
            hub.observe(serial, None, str(error), name=str(reader))


def monitor(hub, stop):
    while not stop.is_set():
        poll_once(hub)
        stop.wait(.2)


def handler_for(hub):
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            route = urlsplit(self.path)
            if route.path not in ('/state', '/events', '/health'):
                self.send_error(404)
                return
            try:
                if route.path == '/events':
                    query = parse_qs(route.query)
                    after = int(query.get('after', ['-1'])[0])
                    if after < 0:
                        raise ValueError()
                    body = hub.snapshot(after, query.get('session', [None])[0])
                else:
                    body = hub.snapshot()
            except ValueError:
                self.send_error(400, 'Fetch /state, then provide after=CURSOR&session=SESSION')
                return
            payload = json.dumps(body).encode()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('Content-Length', str(len(payload)))
            self.end_headers()
            try:
                self.wfile.write(payload)
            except (BrokenPipeError, ConnectionResetError):
                pass

        def log_message(self, *_):
            pass
    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=8767)
    parser.add_argument('--registry', default=str(Path(__file__).with_name('cards.json')))
    parser.add_argument('--stations', default=str(Path(__file__).with_name('stations.json')))
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO)
    hub = Hub(load_registry(args.registry), stations=load_stations(args.stations))
    stop, reload_requested = threading.Event(), threading.Event()
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda *_: stop.set())
    signal.signal(signal.SIGHUP, lambda *_: reload_requested.set())
    server = ThreadingHTTPServer(('127.0.0.1', args.port), handler_for(hub))
    server.timeout = .2
    worker = threading.Thread(target=monitor, args=(hub, stop), daemon=True)
    worker.start()
    logging.info('NFC service on http://127.0.0.1:%s', args.port)
    try:
        while not stop.is_set():
            server.handle_request()
            if reload_requested.is_set():
                reload_requested.clear()
                try:
                    hub.reload(load_registry(args.registry), load_stations(args.stations))
                except (OSError, ValueError, TypeError) as error:
                    logging.error('Registry unchanged: %s', error)
    finally:
        stop.set()
        server.server_close()
        worker.join(timeout=3)


if __name__ == '__main__':
    main()
