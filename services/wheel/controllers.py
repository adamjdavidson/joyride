"""Two physical controllers, one neutral-position input stream."""
import copy
import select

from evdev import ecodes

NEUTRAL = {'steering': 0, 'throttle': 0, 'brake': 0, 'clutch': 0,
           'dpad_x': 0, 'dpad_y': 0}


def stick_position(value, low, high):
    if high <= low:
        raise ValueError('Axis has no usable range')
    position = 2 * (value - low) / (high - low) - 1
    # The Trooper is a directional stick. Treat each direction as a full press.
    return -1 if position < -.5 else 1 if position > .5 else 0


def joystick_changes(code, value, ranges, clutch_button):
    if code == ('key', clutch_button):
        return [('clutch', int(bool(value)))]
    if code not in (ecodes.ABS_X, ecodes.ABS_Y):
        return []
    info = ranges[code]
    position = stick_position(value, info.min, info.max)
    if code == ecodes.ABS_X:
        return [('steering', position)]
    # Brake first: switching directly from forward to back must stop before
    # releasing the gas, which otherwise could complete a resume gesture.
    return [('brake', int(position > 0)), ('throttle', int(position < 0))]


class Controllers:
    """Last deliberate input wins; idle devices and their releases cannot steal it."""
    def __init__(self, hub):
        self.hub = hub
        self.owner = None
        self.sources = {}
        self.last = {}
        self.hub.state.update(axes=dict(NEUTRAL), devices={}, active_controller=None)

    def reset(self, stop=False):
        self.hub.state['axes'] = dict(NEUTRAL)
        self.hub.state['buttons'] = []
        self.hub.mapper.last.clear()
        self.hub.emit('controls_reset', stop=stop)

    def status(self):
        self.hub.state.update(connected=bool(self.sources), devices=copy.deepcopy(self.sources),
                              device=self.sources.get(self.owner), active_controller=self.owner,
                              error=None if self.sources else 'No controller connected')

    def connect(self, source, device, initial):
        with self.hub.lock:
            self.sources[source] = {'path': device.path, 'name': device.name}
            self.last[source] = dict(initial)
            self.status()
            # Query held controls for comparison only. Connecting never presses
            # gas or replays a held button into the application.
            self.hub.emit('connected', source=source)

    def disconnect(self, source, reason):
        with self.hub.lock:
            if source not in self.sources:
                return
            del self.sources[source]
            self.last.pop(source, None)
            if self.owner == source:
                self.owner = None
                self.reset(stop=True)
            self.status()
            self.hub.emit('disconnected', source=source, reason=reason)

    def inputs(self, source, changes):
        with self.hub.lock:
            if source not in self.sources:
                return
            changed = []
            for control, value in changes:
                previous = self.last[source].get(control, 0)
                # Suppress small G29 sensor jitter, retaining exact end releases.
                threshold = .015 if control == 'steering' else .025
                if value == previous or (value != 0 and abs(value - previous) < threshold):
                    continue
                self.last[source][control] = value
                changed.append((control, value))
            if not changed:
                return
            if self.owner != source:
                if not any(abs(value) > .06 for _, value in changed):
                    return
                self.reset()
                self.owner = source
                self.status()
            for control, value in changed:
                edge = ('press' if value else 'release') if control.startswith('button:') else 'change'
                self.hub.input(control, value, edge)


def read_controller(controllers, stop, source, clutch_button, path=None):
    # Imported here because service.py is also the executable entrypoint.
    from service import AXES, find_device, normalize
    joystick = source == 'joystick'
    while not stop.is_set():
        device = None
        try:
            device = find_device(path, joystick=joystick)
            codes = (ecodes.ABS_X, ecodes.ABS_Y) if joystick else AXES
            ranges = {code: device.absinfo(code) for code in codes}
            initial = dict(NEUTRAL)
            if joystick:
                for code, info in ranges.items():
                    initial.update(joystick_changes(code, info.value, ranges, clutch_button))
                initial['clutch'] = int(clutch_button in device.active_keys())
            else:
                initial.update({name: normalize(name, ranges[code].value, ranges[code].min,
                                               ranges[code].max) for code, name in AXES.items()})
                initial.update({f'button:{code}': 1 for code in device.active_keys()})
            controllers.connect(source, device, initial)
            while not stop.is_set():
                if not select.select([device.fd], [], [], .25)[0]:
                    continue
                for event in device.read():
                    if event.type == ecodes.EV_SYN and event.code == ecodes.SYN_DROPPED:
                        raise OSError('Input overflow; resynchronizing')
                    changes = []
                    if event.type == ecodes.EV_ABS and event.code in ranges:
                        if joystick:
                            changes = joystick_changes(event.code, event.value, ranges, clutch_button)
                        else:
                            info = ranges[event.code]
                            name = AXES[event.code]
                            value = normalize(name, event.value, info.min, info.max)
                            if abs(value) < .015:
                                value = 0
                            changes = [(name, value)]
                    elif event.type == ecodes.EV_KEY and event.value in (0, 1):
                        changes = (joystick_changes(('key', event.code), event.value, ranges, clutch_button)
                                   if joystick else [(f'button:{event.code}', event.value)])
                    controllers.inputs(source, changes)
        except (OSError, ValueError) as error:
            controllers.disconnect(source, str(error))
            stop.wait(1)
        finally:
            if device is not None:
                device.close()
    controllers.disconnect(source, 'Service stopped')
