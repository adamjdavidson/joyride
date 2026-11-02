import unittest
from types import SimpleNamespace
from service import AXES, Hub, Mapper, ecodes, normalize


class G29Tests(unittest.TestCase):
    def test_axes_and_normalization(self):
        self.assertEqual(AXES[ecodes.ABS_Z], 'throttle')
        self.assertEqual(AXES[ecodes.ABS_RZ], 'brake')
        self.assertEqual(AXES[ecodes.ABS_Y], 'clutch')
        self.assertEqual(normalize('steering', 0, 0, 65535), -1)
        self.assertEqual(normalize('steering', 65535, 0, 65535), 1)
        self.assertAlmostEqual(normalize('steering', 32768, 0, 65535), 0, places=4)
        for name in ('throttle', 'brake', 'clutch'):
            self.assertEqual(normalize(name, 255, 0, 255), 0)
            self.assertEqual(normalize(name, 0, 0, 255), 1)
            self.assertEqual(normalize(name, -5, 0, 255), 1)

    def test_mapping_changes_meaning_without_reader(self):
        mapper = Mapper({'version': 1, 'bindings': [
            {'control': 'steering', 'action': 'dial.set', 'steps': 5},
            {'control': 'button:288', 'on': 'press', 'action': 'talk.start'},
            {'control': 'button:288', 'on': 'release', 'action': 'talk.stop'}]})
        self.assertEqual(mapper.actions('steering', -1, 'change')[0]['value'], 1)
        self.assertEqual(mapper.actions('steering', -.99, 'change'), [])
        self.assertEqual(mapper.actions('steering', 1, 'change')[0]['value'], 5)
        self.assertEqual(mapper.actions('button:288', 0, 'release')[0]['action'], 'talk.stop')

    def test_edges_disconnect_and_overflow(self):
        hub = Hub(Mapper({'version': 1, 'bindings': []}), capacity=2)
        hub.input('button:288', 1, 'press')
        hub.input('button:288', 1, 'press')
        hub.input('button:288', 0, 'release')
        result = hub.snapshot(0, hub.session)
        self.assertEqual([e['edge'] for e in result['events']], ['press', 'release'])
        hub.input('button:289', 1, 'press')
        self.assertTrue(hub.snapshot(0, hub.session)['resync'])
        hub.disconnect('unplugged')
        self.assertEqual(hub.snapshot()['state']['buttons'], [])
        self.assertEqual(hub.snapshot()['state']['axes'], {})
        self.assertTrue(hub.snapshot(hub.sequence, 'old-process')['resync'])

    def test_initial_state_is_read_without_actions(self):
        class Device:
            path, name = '/fake', 'G29'
            def absinfo(self, code):
                return SimpleNamespace(value=255, min=0, max=255)
            def active_keys(self):
                return [288]
        hub = Hub(Mapper({'version': 1, 'bindings': []}))
        hub.connect(Device())
        self.assertEqual(hub.snapshot()['state']['axes']['throttle'], 0)
        self.assertEqual(hub.snapshot()['state']['buttons'], [288])
        self.assertEqual([e['type'] for e in hub.events], ['connected'])

    def test_invalid_mapping(self):
        for binding in ({'control': 'unknown', 'action': 'a'},
                        {'control': 'steering', 'action': 'a', 'steps': 0},
                        {'control': 'steering', 'action': 'a', 'scale': float('nan')}):
            with self.assertRaises(ValueError):
                Mapper({'version': 1, 'bindings': [binding]})


if __name__ == '__main__':
    unittest.main()
