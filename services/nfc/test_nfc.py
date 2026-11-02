import unittest
from unittest.mock import patch
from nfc_service import Hub, load_registry, read_uid, reader_id, poll_once


class NFCTests(unittest.TestCase):
    def test_presence_dedup_removal_grace_and_replacement(self):
        hub = Hub({'01020304': {'mode': 'expand'}})
        hub.observe('reader', '01020304', now=0)
        hub.observe('reader', '01020304', now=.2)
        self.assertEqual([e['type'] for e in hub.events].count('card_present'), 1)
        hub.observe('reader', None, now=.3)
        hub.observe('reader', '01020304', now=.4)
        self.assertEqual(hub.state['readers']['reader']['uid'], '01020304')
        hub.observe('reader', None, now=.5)
        hub.observe('reader', None, now=1)
        self.assertIsNone(hub.state['readers']['reader']['uid'])
        hub.observe('reader', '01020304', now=2)
        hub.observe('reader', '05060708', now=3)
        self.assertEqual(hub.events[-2]['type'], 'card_removed')
        self.assertIsNone(hub.state['readers']['reader']['assignment'])

    def test_disconnect_and_history_gap(self):
        hub = Hub({}, capacity=2)
        hub.observe('reader', '01020304')
        hub.reconcile([])
        self.assertIsNone(hub.state['readers']['reader']['uid'])
        self.assertFalse(hub.state['readers']['reader']['connected'])
        self.assertTrue(hub.snapshot(0, hub.session)['resync'])
        self.assertTrue(hub.snapshot(hub.sequence, 'old')['resync'])

    def test_registry_modes_and_memories_reload_independently(self):
        hub = Hub({'01020304': {'mode': 'expand', 'memory': 'project-a'}})
        hub.observe('reader', '01020304')
        hub.reload({'01020304': {'memory': 'project-b'}})
        self.assertEqual(hub.state['readers']['reader']['assignment'], {'memory': 'project-b'})
        self.assertEqual(hub.events[-1]['type'], 'registry_changed')

    def test_two_stations_remove_and_disconnect_independently(self):
        hub = Hub({})
        hub.observe('mode', '01020304', now=0)
        hub.observe('context', '05060708', now=0)
        hub.observe('mode', None, now=1)
        hub.observe('mode', None, now=2)
        self.assertIsNone(hub.state['readers']['mode']['uid'])
        self.assertEqual(hub.state['readers']['context']['uid'], '05060708')
        self.assertEqual(hub.events[-1]['reader_id'], 'mode')
        hub.observe('mode', '01020304', now=3)
        hub.reconcile(['mode'])
        self.assertEqual(hub.state['readers']['mode']['uid'], '01020304')
        self.assertIsNone(hub.state['readers']['context']['uid'])
        self.assertFalse(hub.state['readers']['context']['connected'])

    def test_serial_survives_slot_renumbering(self):
        self.assertEqual(reader_id('ACS PICC (EXAMPLE-READER-B) 02 00'),
                         reader_id('ACS PICC (EXAMPLE-READER-B) 00 00'))

    def test_one_reader_failure_does_not_hide_other_reader(self):
        from smartcard.Exceptions import CardConnectionException
        first = 'ACS ACR1552 PICC (first) 00 00'
        second = 'ACS ACR1552 PICC (second) 02 00'
        hub = Hub({})
        with patch('nfc_service.readers', return_value=[first, second]), patch(
                'nfc_service.read_uid', side_effect=[CardConnectionException('denied'), '01020304']):
            poll_once(hub)
        self.assertFalse(hub.state['readers']['first']['connected'])
        self.assertEqual(hub.state['readers']['second']['uid'], '01020304')

    def test_roles_follow_readers_not_cards_and_reload_updates_held_state(self):
        hub = Hub({}, stations={'first': {'role': 'mode', 'label': 'Mode reader'},
                                'second': {'role': 'message', 'label': 'Message reader'}})
        hub.observe('second', '01020304')
        hub.observe('first', '05060708')
        self.assertEqual(hub.state['readers']['first']['role'], 'mode')
        self.assertEqual(hub.events[-1]['role'], 'mode')
        hub.reload({}, {'first': {'role': 'context', 'label': 'Context reader'}})
        self.assertEqual(hub.state['readers']['first']['uid'], '05060708')
        self.assertEqual(hub.state['readers']['first']['role'], 'context')
        self.assertIsNone(hub.state['readers']['second']['role'])

    def test_invalid_registry(self):
        for document in ('{}', '{"version":1,"cards":{"bad":{"mode":"play"}}}',
                         '{"version":1,"cards":{"01020304":{"memory":false}}}'):
            with patch('nfc_service.Path.read_text', return_value=document):
                with self.assertRaises(ValueError):
                    load_registry('unused')

    def test_uid_command_is_read_only_and_checks_status(self):
        from unittest.mock import Mock, create_autospec
        from smartcard.CardConnectionDecorator import CardConnectionDecorator
        reader = Mock()
        connection = create_autospec(CardConnectionDecorator, instance=True)
        reader.createConnection.return_value = connection
        connection.transmit.return_value = ([1, 2, 3, 4], 0x90, 0)
        self.assertEqual(read_uid(reader), '01020304')
        connection.transmit.assert_called_once_with([255, 202, 0, 0, 0])
        connection.transmit.return_value = ([], 0x6A, 0x81)
        with self.assertRaises(ValueError):
            read_uid(reader)
        self.assertEqual(connection.disconnect.call_count, 2)
        from smartcard.Exceptions import CardConnectionException
        from smartcard.scard import SCARD_E_NO_SMARTCARD
        connection.transmit.side_effect = CardConnectionException('removed mid-read', hresult=SCARD_E_NO_SMARTCARD)
        self.assertIsNone(read_uid(reader))


if __name__ == '__main__':
    unittest.main()
