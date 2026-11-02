#!/usr/bin/env python3
"""Read-only G29 and Trooper V2 input bridge. No model dependencies."""
import argparse
from collections import deque
import copy
import json
import logging
import math
from pathlib import Path
import signal
import threading
import time
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit

from evdev import InputDevice, ecodes, list_devices

AXES = {ecodes.ABS_X: 'steering', ecodes.ABS_Z: 'throttle',
        ecodes.ABS_RZ: 'brake', ecodes.ABS_Y: 'clutch',
        ecodes.ABS_HAT0X: 'dpad_x', ecodes.ABS_HAT0Y: 'dpad_y'}
PEDALS = {'throttle', 'brake', 'clutch'}


def normalize(name, value, low, high):
    if high <= low:
        raise ValueError('Axis has no usable range')
    fraction = max(0.0, min(1.0, (value - low) / (high - low)))
    return 1 - fraction if name in PEDALS else 2 * fraction - 1


class Mapper:
    """Pure mapping: normalized input -> declarative actions, never side effects."""
    def __init__(self, document):
        if not isinstance(document, dict) or document.get('version') != 1:
            raise ValueError('Mapping version must be 1')
        bindings = document.get('bindings')
        if not isinstance(bindings, list):
            raise ValueError('bindings must be a list')
        self.bindings = copy.deepcopy(bindings)
        for item in self.bindings:
            if not isinstance(item, dict):
                raise ValueError('Each binding must be an object')
            control = item.get('control', '')
            button = isinstance(control, str) and control.startswith('button:') and control[7:].isdigit()
            if control not in AXES.values() and not button:
                raise ValueError('Unknown control')
            if not isinstance(item.get('action'), str) or not item['action']:
                raise ValueError('action must be a nonempty string')
            if item.get('on', 'change') not in ({'press', 'release'} if button else {'change'}):
                raise ValueError('Invalid trigger for control')
            for key in ('scale', 'offset'):
                value = item.get(key, 1 if key == 'scale' else 0)
                if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
                    raise ValueError('scale and offset must be finite numbers')
            if 'steps' in item and (type(item['steps']) is not int or item['steps'] < 2):
                raise ValueError('steps must be an integer >= 2')
        self.last = {}

    def actions(self, control, value, edge):
        result = []
        for index, binding in enumerate(self.bindings):
            if binding['control'] != control or binding.get('on', 'change') != edge:
                continue
            output = value * binding.get('scale', 1) + binding.get('offset', 0)
            if 'steps' in binding:
                low = 0 if control in PEDALS else -1
                unit = max(0, min(1, (value - low) / (1 - low)))
                output = min(binding['steps'], int(unit * binding['steps']) + 1)
                if self.last.get(index) == output:
                    continue
                self.last[index] = output
            result.append({'action': binding['action'], 'value': output,
                           'control': control, 'edge': edge})
        return result


class Hub:
    def __init__(self, mapper, capacity=4096):
        self.lock = threading.RLock()
        self.mapper = mapper
        self.events = deque(maxlen=capacity)
        self.sequence = 0
        self.session = str(uuid.uuid4())
        self.state = {'connected': False, 'device': None, 'axes': {}, 'buttons': [], 'error': None}

    def emit(self, kind, **fields):
        self.sequence += 1
        self.events.append({'sequence': self.sequence, 'time': time.time(), 'type': kind, **fields})

    def connect(self, device):
        axes = {}
        for code, name in AXES.items():
            info = device.absinfo(code)
            axes[name] = normalize(name, info.value, info.min, info.max)
        with self.lock:
            self.mapper.last.clear()
            self.state = {'connected': True, 'device': {'path': device.path, 'name': device.name},
                          'axes': axes, 'buttons': sorted(device.active_keys()), 'error': None}
            self.emit('connected', state=copy.deepcopy(self.state))

    def disconnect(self, reason):
        with self.lock:
            if self.state['connected'] or self.state['error'] != reason:
                self.state = {'connected': False, 'device': None, 'axes': {}, 'buttons': [], 'error': reason}
                self.mapper.last.clear()
                self.emit('disconnected', reason=reason)

    def input(self, control, value, edge='change'):
        with self.lock:
            if control.startswith('button:'):
                code = int(control[7:])
                pressed = set(self.state['buttons'])
                if (code in pressed) == bool(value):
                    return
                pressed.add(code) if value else pressed.discard(code)
                self.state['buttons'] = sorted(pressed)
            else:
                if self.state['axes'].get(control) == value:
                    return
                self.state['axes'][control] = value
            self.emit('input', control=control, value=value, edge=edge)
            for action in self.mapper.actions(control, value, edge):
                self.emit('action', **action)

    def replace_mapper(self, mapper):
        with self.lock:
            self.mapper = mapper
            self.emit('mappings_changed')

    def snapshot(self, after=None, session=None):
        with self.lock:
            result = {'version': 1, 'session': self.session, 'cursor': self.sequence,
                      'state': copy.deepcopy(self.state)}
            if after is not None:
                oldest = self.events[0]['sequence'] if self.events else self.sequence + 1
                reset = session != self.session or after < oldest - 1 or after > self.sequence
                result.update(resync=reset, events=[] if reset else [copy.deepcopy(e) for e in self.events if e['sequence'] > after])
            return result


def find_device(path=None, joystick=False):
    for candidate in ([path] if path else list_devices()):
        device = None
        try:
            device = InputDevice(candidate)
            codes = {code for code, _ in device.capabilities().get(ecodes.EV_ABS, [])}
            identity = (0x2e24, 0x386a) if joystick else (0x046d, 0xc24f)
            required = {ecodes.ABS_X, ecodes.ABS_Y} if joystick else set(AXES)
            if (device.info.vendor, device.info.product) == identity and required <= codes:
                return device
        except OSError:
            pass
        if device is not None:
            device.close()
    raise OSError(f'{"Trooper V2" if joystick else "G29"} unavailable; check connection and input-device permissions')


def handler_for(hub):
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            route = urlsplit(self.path)
            if route.path not in ('/state', '/events', '/health'):
                self.send_error(404)
                return
            try:
                query = parse_qs(route.query)
                if route.path == '/events':
                    after = int(query.get('after', ['-1'])[0])
                    if after < 0:
                        raise ValueError()
                    body = hub.snapshot(after, query.get('session', [None])[0])
                else:
                    body = hub.snapshot()
            except ValueError:
                self.send_error(400, 'Use /state first, then /events?after=CURSOR&session=SESSION')
                return
            payload = json.dumps(body, allow_nan=False).encode()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('Content-Length', str(len(payload)))
            self.end_headers()
            try:
                self.wfile.write(payload)
            except (BrokenPipeError, ConnectionResetError):
                pass

        def log_message(self, *_args):
            pass
    return Handler


def load_mapper(path):
    return Mapper(json.loads(Path(path).read_text()))


def main():
    from controllers import Controllers, read_controller
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=8766)
    parser.add_argument('--device', help='Optional /dev/input/eventN; default discovers USB 046d:c24f')
    parser.add_argument('--mappings', default=str(Path(__file__).with_name('mappings.json')))
    parser.add_argument('--clutch-button', type=int, default=288,
                        help='Trooper V2 evdev button code for the clutch')
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')
    hub = Hub(load_mapper(args.mappings))
    stop, reload_requested = threading.Event(), threading.Event()
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda *_: stop.set())
    signal.signal(signal.SIGHUP, lambda *_: reload_requested.set())
    server = ThreadingHTTPServer(('127.0.0.1', args.port), handler_for(hub))
    server.timeout = 0.25
    controllers = Controllers(hub)
    readers = [threading.Thread(target=read_controller,
                               args=(controllers, stop, source, args.clutch_button, path), daemon=True)
               for source, path in [('g29', args.device), ('joystick', None)]]
    for reader in readers:
        reader.start()
    logging.info('Wheel/joystick service: http://127.0.0.1:%s (Ctrl-C to stop)', args.port)
    try:
        while not stop.is_set():
            server.handle_request()
            if reload_requested.is_set():
                reload_requested.clear()
                try:
                    hub.replace_mapper(load_mapper(args.mappings))
                    logging.info('Mappings reloaded')
                except (OSError, ValueError, TypeError) as error:
                    logging.error('Keeping previous mappings: %s', error)
    finally:
        stop.set()
        server.server_close()
        for reader in readers:
            reader.join(timeout=2)


if __name__ == '__main__':
    main()
