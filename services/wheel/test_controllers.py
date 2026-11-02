import unittest
from types import SimpleNamespace

from service import Hub, Mapper, ecodes
from controllers import Controllers, joystick_changes, stick_position


class ControllerTests(unittest.TestCase):
    def setUp(self):
        self.hub = Hub(Mapper({'version': 1, 'bindings': []}))
        self.controllers = Controllers(self.hub)
        for source in ('g29', 'joystick'):
            self.controllers.connect(source, SimpleNamespace(path='/fake', name=source), {})
        self.ranges = {code: SimpleNamespace(min=0, max=255) for code in (0, 1)}

    def test_directions_and_button(self):
        for raw, expected in [(0, -1), (127, 0), (128, 0), (255, 1)]:
            self.assertEqual(stick_position(raw, 0, 255), expected)
        self.assertEqual(joystick_changes(0, 0, self.ranges, 288), [('steering', -1)])
        self.assertEqual(joystick_changes(1, 0, self.ranges, 288), [('brake', 0), ('throttle', 1)])
        self.assertEqual(joystick_changes(1, 255, self.ranges, 288), [('brake', 1), ('throttle', 0)])
        self.assertEqual(joystick_changes(1, 127, self.ranges, 288), [('brake', 0), ('throttle', 0)])
        self.assertEqual(joystick_changes(('key', 288), 1, self.ranges, 288), [('clutch', 1)])
        self.assertEqual(joystick_changes(('key', 288), 0, self.ranges, 288), [('clutch', 0)])
        self.assertEqual(joystick_changes(('key', 289), 1, self.ranges, 288), [])

    def test_switching_releases_previous_controls_and_ignores_idle_releases(self):
        self.controllers.inputs('g29', [('throttle', 1), ('clutch', .5)])
        self.controllers.inputs('joystick', [('steering', -1)])
        self.assertEqual(self.hub.state['axes']['throttle'], 0)
        self.assertEqual(self.hub.state['axes']['clutch'], 0)
        self.controllers.inputs('g29', [('throttle', 0), ('clutch', 0)])
        self.assertEqual(self.controllers.owner, 'joystick')
        self.assertEqual(self.hub.state['axes']['steering'], -1)
        self.controllers.inputs('g29', [('steering', .005)])
        self.assertEqual(self.controllers.owner, 'joystick')
        self.controllers.inputs('g29', [('steering', .5)])
        self.assertEqual(self.controllers.owner, 'g29')
        self.assertEqual(self.hub.state['axes']['steering'], .5)

    def test_brake_precedes_gas_release(self):
        self.controllers.inputs('joystick', joystick_changes(1, 0, self.ranges, 288))
        cursor = self.hub.sequence
        self.controllers.inputs('joystick', joystick_changes(1, 255, self.ranges, 288))
        events = self.hub.snapshot(cursor, self.hub.session)['events']
        self.assertEqual([(e['control'], e['value']) for e in events], [('brake', 1), ('throttle', 0)])

    def test_unplug_owner_stops_without_replaying_other_controller(self):
        self.controllers.inputs('joystick', [('clutch', 1)])
        cursor = self.hub.sequence
        self.controllers.disconnect('g29', 'unplugged')
        self.assertFalse(any(e['type'] == 'controls_reset' for e in self.hub.snapshot(cursor, self.hub.session)['events']))
        self.controllers.disconnect('joystick', 'unplugged')
        self.assertEqual(self.hub.state['axes']['clutch'], 0)
        self.assertFalse(self.hub.state['connected'])
        self.assertTrue(any(e['type'] == 'controls_reset' and e['stop'] for e in self.hub.events))
        cursor = self.hub.sequence
        self.controllers.connect('joystick', SimpleNamespace(path='/fake', name='joystick'), {'throttle': 1, 'clutch': 1})
        self.assertIsNone(self.controllers.owner)
        self.assertEqual(self.hub.state['axes']['throttle'], 0)
        self.assertFalse(any(e['type'] == 'input' for e in self.hub.snapshot(cursor, self.hub.session)['events']))


if __name__ == '__main__':
    unittest.main()
