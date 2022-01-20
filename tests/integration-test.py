#!/usr/bin/python3

# gnome-bluetooth integration test suite
#
# Copyright: (C) 2021 Bastien Nocera <hadess@hadess.net>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

import os
import sys
import dbus
import inspect
import tempfile
import random
import subprocess
import unittest
import time

try:
    import gi
    from gi.repository import GLib
    from gi.repository import Gio

    gi.require_version('Gtk', '4.0')
    from gi.repository import Gtk
except ImportError as e:
    sys.stderr.write('Skipping tests, PyGobject not available for Python 3, or missing GI typelibs: %s\n' % str(e))
    sys.exit(77)

gi.require_version('GIRepository', '2.0')
from gi.repository import GIRepository
builddir = os.getenv('top_builddir', '.')
GIRepository.Repository.prepend_library_path(builddir + '/lib/')
GIRepository.Repository.prepend_search_path(builddir + '/lib/')

GNOME_BLUETOOTH_PRIV_UNAVAILABLE = False
try:
    gi.require_version('GnomeBluetoothPriv', '3.0')
    from gi.repository import GnomeBluetoothPriv
except (ImportError, ValueError) as e:
    GNOME_BLUETOOTH_PRIV_UNAVAILABLE = True

try:
    import dbusmock
except ImportError:
    sys.stderr.write('Skipping tests, python-dbusmock not available (http://pypi.python.org/pypi/python-dbusmock).\n')
    sys.exit(77)

# Out-of-process tests
class OopTests(dbusmock.DBusTestCase):
    @classmethod
    def setUp(self):
        if GNOME_BLUETOOTH_PRIV_UNAVAILABLE:
            sys.stderr.write('Could not find GnomeBluetoothPriv gobject-introspection data in the build dir: %s\n' % str(e))
            sys.exit(1)

        self.client = GnomeBluetoothPriv.Client.new()
        # used in test_pairing
        self.paired = False

    def print_tree(self, model):
        def print_row(model, treepath, treeiter):
            print("\t" * (treepath.get_depth() - 1), model[treeiter][:], sep="")
        model.foreach(print_row)

    def print_list_store(self, model):
        for device in model:
            print(f"{device.props.name}: {device.props.address}")

    def wait_for_mainloop(self):
        ml = GLib.MainLoop()
        GLib.timeout_add_seconds(1, ml.quit)
        ml.run()

    def wait_for_condition(self, condition, timeout=5):
        ctx = GLib.main_context_default()
        timed_out = False
        def timeout_cb():
            nonlocal timed_out
            timed_out = True
        GLib.timeout_add_seconds(1, timeout_cb)
        while not condition() and not timed_out:
          ctx.iteration(True)

    def test_no_adapters(self):
        self.wait_for_mainloop()
        self.assertEqual(self.client.props.num_adapters, 0)

    def test_one_adapter(self):
        self.wait_for_condition(lambda: self.client.props.num_adapters != 0)
        self.assertEqual(self.client.props.num_adapters, 1)

    def test_one_device(self):
        self.wait_for_condition(lambda: self.client.props.num_adapters != 0)
        self.assertEqual(self.client.props.num_adapters, 1)

        # GListStore
        list_store = self.client.get_devices()
        self.assertEqual(list_store.get_n_items(), 1)
        device = list_store.get_item(0)
        self.assertIsNotNone(device)
        self.assertEqual(device.props.address, '22:33:44:55:66:77')

    def test_device_notify(self):
        bus = dbus.SystemBus()
        dbusmock_bluez = dbus.Interface(bus.get_object('org.bluez', '/org/bluez/hci0/dev_22_33_44_55_66_77'), 'org.freedesktop.DBus.Mock')

        list_store = self.client.get_devices()
        self.wait_for_mainloop()
        self.assertEqual(list_store.get_n_items(), 1)
        device = list_store.get_item(0)
        self.assertIsNotNone(device)
        self.assertEqual(device.props.connected, False)

        received_notification = False
        def device_notify_cb(device, pspec):
            nonlocal received_notification
            received_notification = True
            self.assertEqual(pspec.name, 'connected')
        device.connect('notify', device_notify_cb)

        dbusmock_bluez.UpdateProperties('org.bluez.Device1', {
                'Connected': True,
        })
        self.wait_for_condition(lambda: received_notification == True)
        self.assertEqual(received_notification, True)
        self.assertEqual(device.props.connected, True)

    def test_device_removal(self):
        bus = dbus.SystemBus()
        dbusmock_bluez = dbus.Interface(bus.get_object('org.bluez', '/'), 'org.bluez.Mock')
        hci0_bluez = dbus.Interface(bus.get_object('org.bluez', '/org/bluez/hci0'), 'org.bluez.Adapter1')
        list_store = self.client.get_devices()

        num_devices = 0
        num_mice = 0
        num_devices_signal = 0
        def device_added_cb(client, device):
            nonlocal num_devices_signal
            num_devices_signal += 1
        def device_removed_cb(client, path):
            nonlocal num_devices_signal
            num_devices_signal -= 1
        self.client.connect('device-added', device_added_cb)
        self.client.connect('device-removed', device_removed_cb)

        self.wait_for_mainloop()

        for i in range(1, 3):
            to_add = random.randrange(5, 10)
            to_remove = random.randrange(1, to_add)
            total = num_devices + to_add - to_remove

            print(f"Device removal iteration {i}: +{to_add} -{to_remove} = {total}")

            for i in range(1, to_add + 1):
                print(f"Adding mouse {i}")
                num_mice += 1
                address_start = num_mice
                address = f"{address_start:02d}:{address_start+1:02d}:{address_start+2:02d}:" + \
                        f"{address_start+3:02d}:{address_start+4:02d}:{address_start+5:02d}"
                dbusmock_bluez.AddDevice('hci0', address, f'My Mouse {num_mice}')
            self.wait_for_mainloop()
            # self.wait_for_condition(lambda: list_store.get_n_items() == num_devices + to_add)
            self.assertEqual(list_store.get_n_items(), num_devices + to_add)
            self.assertEqual(list_store.get_n_items(), num_devices_signal)

            for i in range(to_remove - 1, -1, -1):
                print(f"Removing mouse {i}")
                device = list_store.get_item(i)
                self.assertIsNotNone(device, f"Device at index {i} in list store did not exist")
                hci0_bluez.RemoveDevice(device.get_object_path())
            self.wait_for_mainloop()
            # self.wait_for_condition(lambda: list_store.get_n_items() == total)
            self.assertEqual(list_store.get_n_items(), total)
            self.assertEqual(list_store.get_n_items(), num_devices_signal)
            num_devices = total

        print(f"Device removal finishing: -{num_devices}")
        for i in range(num_devices - 1, -1, -1):
            print(f"Removing mouse {i}")
            device = list_store.get_item(i)
            hci0_bluez.RemoveDevice(device.get_object_path())

        self.wait_for_mainloop()
        # self.wait_for_condition(lambda: list_store.get_n_items() == 0)
        self.assertEqual(list_store.get_n_items(), 0)
        self.assertEqual(num_devices_signal, 0)

    def test_default_adapter(self):
        bus = dbus.SystemBus()
        bluez_server = bus.get_object('org.bluez', '/org/bluez')
        dbusmock_bluez = dbus.Interface(bluez_server, 'org.bluez.Mock')
        hci0_props = dbus.Interface(bus.get_object('org.bluez', '/org/bluez/hci0'), 'org.freedesktop.DBus.Properties')
        hci1_props = dbus.Interface(bus.get_object('org.bluez', '/org/bluez/hci1'), 'org.freedesktop.DBus.Properties')

        self.wait_for_condition(lambda: self.client.props.num_adapters != 0)
        self.assertEqual(self.client.props.num_adapters, 2)

        # GListModel
        list_store = self.client.get_devices()
        self.assertEqual(list_store.get_n_items(), 1)
        device = list_store.get_item(0)
        self.assertIsNotNone(device)
        self.assertEqual(device.props.name, 'Device on hci1')

        # Set default adapter in setup mode
        self.assertEqual (self.client.props.default_adapter_setup_mode, False)
        self.assertEqual(hci0_props.Get('org.bluez.Adapter1', 'Discoverable'), False)
        self.assertEqual(hci1_props.Get('org.bluez.Adapter1', 'Discoverable'), False)
        default_adapter_path = self.client.props.default_adapter
        self.client.props.default_adapter_setup_mode = True
        self.wait_for_condition(lambda: hci1_props.Get('org.bluez.Adapter1', 'Discoverable') == True)
        self.assertEqual(hci1_props.Get('org.bluez.Adapter1', 'Discoverable'), True)
        self.wait_for_mainloop()
        self.assertEqual(self.client.props.default_adapter_setup_mode, True)
        self.assertEqual(hci0_props.Get('org.bluez.Adapter1', 'Discoverable'), False)
        self.assertEqual(hci1_props.Get('org.bluez.Adapter1', 'Discoverable'), True)

        # Remove default adapter
        dbusmock_bluez.RemoveAdapter('hci1')
        self.wait_for_condition(lambda: self.client.props.num_adapters != 2)
        self.assertEqual(self.client.props.num_adapters, 1)
        self.assertNotEqual(self.client.props.default_adapter, default_adapter_path)
        self.assertEqual(self.client.props.default_adapter_setup_mode, False)
        self.assertEqual(hci0_props.Get('org.bluez.Adapter1', 'Discoverable'), False)

        # GListModel
        self.assertEqual(list_store.get_n_items(), 1)
        device = list_store.get_item(0)
        self.assertIsNotNone(device)
        self.assertEqual(device.props.name, 'Device on hci0')

        # Make hci0 discoverable
        self.client.props.default_adapter_setup_mode = True

        # Re-add the old adapter, device is still there
        dbusmock_bluez.AddAdapter('hci1', 'hci1')
        self.wait_for_condition(lambda: self.client.props.num_adapters == 2)
        self.assertEqual(self.client.props.num_adapters, 2)

        # GListModel
        self.assertEqual(list_store.get_n_items(), 1)
        device = list_store.get_item(0)
        self.assertIsNotNone(device)
        self.assertEqual(device.props.name, 'Device on hci1')

        self.wait_for_condition(lambda: hci0_props.Get('org.bluez.Adapter1', 'Discoverable') == False)
        self.assertEqual(hci0_props.Get('org.bluez.Adapter1', 'Discoverable'), False)
        self.assertEqual(hci1_props.Get('org.bluez.Adapter1', 'Discoverable'), False)

    def test_default_adapter_powered(self):
        bus = dbus.SystemBus()
        dbusmock_bluez = dbus.Interface(bus.get_object('org.bluez', '/org/bluez/hci0'), 'org.freedesktop.DBus.Mock')
        dbusprops_bluez = dbus.Interface(bus.get_object('org.bluez', '/org/bluez/hci0'), 'org.freedesktop.DBus.Properties')

        self.assertEqual(dbusprops_bluez.Get('org.bluez.Adapter1', 'Powered'), False)
        self.wait_for_mainloop()
        self.assertEqual(dbusprops_bluez.Get('org.bluez.Adapter1', 'Powered'), False)
        self.client.props.default_adapter_powered = True
        self.wait_for_condition(lambda: dbusprops_bluez.Get('org.bluez.Adapter1', 'Powered') == True)
        self.assertEqual(self.client.props.num_adapters, 1)
        self.assertEqual(dbusprops_bluez.Get('org.bluez.Adapter1', 'Powered'), True)
        self.wait_for_mainloop()
        self.assertEqual(self.client.props.default_adapter_powered, True)

        self.client.props.default_adapter_powered = False
        self.wait_for_condition(lambda: dbusprops_bluez.Get('org.bluez.Adapter1', 'Powered') == False)
        self.assertEqual(dbusprops_bluez.Get('org.bluez.Adapter1', 'Powered'), False)
        self.wait_for_mainloop()
        self.assertEqual(self.client.props.default_adapter_powered, False)

        dbusmock_bluez.UpdateProperties('org.bluez.Adapter1', {
                'Powered': True,
        })
        self.wait_for_condition(lambda: dbusprops_bluez.Get('org.bluez.Adapter1', 'Powered') == True)
        self.assertEqual(dbusprops_bluez.Get('org.bluez.Adapter1', 'Powered'), True)
        self.wait_for_mainloop()
        self.assertEqual(self.client.props.default_adapter_powered, True)

    def _pair_cb(self, client, result, user_data=None):
        success, path = client.setup_device_finish(result)
        self.assertEqual(success, True)
        address = '11:22:33:44:55:66'
        self.assertEqual(path, '/org/bluez/hci0/dev_' + address.replace(':', '_'))
        self.paired = True

    def test_pairing(self):
        self.wait_for_condition(lambda: self.client.props.num_adapters != 0)
        self.assertEqual(self.client.props.num_adapters, 1)

        model = self.client.get_devices()

        # Get first device
        device = model.get_item(0)
        self.assertEqual(device.props.address, '11:22:33:44:55:66')
        self.assertEqual(device.props.paired, False)

        self.client.setup_device (device.get_object_path(),
                True,
                None,
                self._pair_cb)
        self.wait_for_condition(lambda: self.paired == True)
        self.assertEqual(self.paired, True)

        self.assertEqual(device.props.paired, True)
        self.assertEqual(device.props.icon, 'phone')

    def test_set_trusted(self):
        self.wait_for_condition(lambda: self.client.props.num_adapters != 0)
        self.assertEqual(self.client.props.num_adapters, 1)

        model = self.client.get_devices()

        # Get first device
        device = model.get_item(0)
        self.assertEqual(device.props.address, '11:22:33:44:55:66')
        self.assertEqual(device.props.trusted, False)

        self.client.set_trusted(device.get_object_path(), True)
        self.wait_for_condition(lambda: device.props.trusted == True)
        self.assertEqual(device.props.trusted, True)

        self.client.set_trusted(device.get_object_path(), False)
        self.wait_for_condition(lambda: device.props.trusted == False)
        self.assertEqual(device.props.trusted, False)

    def test_connect(self):
        self.wait_for_condition(lambda: self.client.props.num_adapters != 0)
        self.assertEqual(self.client.props.num_adapters, 1)

        model = self.client.get_devices()

        # Get first device
        device = model.get_item(0)
        self.assertEqual(device.props.address, '11:22:33:44:55:66')
        self.assertEqual(device.props.connected, False)

        self.client.connect_service(device.get_object_path(), True)
        self.wait_for_condition(lambda: device.props.connected == True)
        self.assertEqual(device.props.connected, True)

        self.client.connect_service(device.get_object_path(), False)
        self.wait_for_condition(lambda: device.props.connected == False)
        self.assertEqual(device.props.connected, False)

    def test_agent(self):
        agent = GnomeBluetoothPriv.Agent.new ('/org/gnome/bluetooth/integration_test')
        self.assertIsNotNone(agent)
        # Process D-Bus daemon appearing and agent being registered
        self.wait_for_mainloop()

        self.assertTrue(agent.register())
        self.wait_for_mainloop()

        self.assertTrue(agent.unregister())
        self.wait_for_mainloop()

        # Check that we don't have a re-register warning
        self.assertTrue(agent.register())
        self.wait_for_mainloop()

        agent.unregister()
        self.wait_for_mainloop()

        # And try again
        agent = GnomeBluetoothPriv.Agent.new ('/org/gnome/bluetooth/integration_test')
        self.assertIsNotNone(agent)
        self.wait_for_mainloop()
        agent.unregister()
        self.wait_for_mainloop()

    def test_connected_input_devices(self):
        bus = dbus.SystemBus()
        dbusmock_bluez = dbus.Interface(bus.get_object('org.bluez', '/'), 'org.bluez.Mock')

        path = dbusmock_bluez.AddDevice('hci0', '11:22:33:44:55:66', 'My LE Mouse')
        dev1 = dbus.Interface(bus.get_object('org.bluez', path), 'org.freedesktop.DBus.Mock')
        dev1.UpdateProperties('org.bluez.Device1',
                {'UUIDs': dbus.Array(['00001812-0000-1000-8000-00805f9b34fb'], variant_level=1)})

        path = dbusmock_bluez.AddDevice('hci0', '11:22:33:44:55:67', 'My Classic Mouse')
        dev2 = dbus.Interface(bus.get_object('org.bluez', path), 'org.freedesktop.DBus.Mock')
        dev2.UpdateProperties('org.bluez.Device1',
                {'UUIDs': dbus.Array(['00001124-0000-1000-8000-00805f9b34fb'], variant_level=1)})

        self.wait_for_mainloop()
        list_store = self.client.get_devices()
        self.assertEqual(list_store.get_n_items(), 2)
        self.assertEqual(self.client.has_connected_input_devices(), False)

        dev1.UpdateProperties('org.bluez.Device1',
                {'Connected': dbus.Boolean(True, variant_level=1)})

        self.wait_for_mainloop()
        self.assertEqual(self.client.has_connected_input_devices(), True)

        dev1.UpdateProperties('org.bluez.Device1',
                {'Connected': dbus.Boolean(False, variant_level=1)})

        self.wait_for_mainloop()
        self.assertEqual(self.client.has_connected_input_devices(), False)

        dev2.UpdateProperties('org.bluez.Device1',
                {'Connected': dbus.Boolean(True, variant_level=1)})

        self.wait_for_mainloop()
        self.assertEqual(self.client.has_connected_input_devices(), True)

        dev2.UpdateProperties('org.bluez.Device1',
                {'Connected': dbus.Boolean(False, variant_level=1)})

        self.wait_for_mainloop()
        self.assertEqual(self.client.has_connected_input_devices(), False)


class Tests(dbusmock.DBusTestCase):

    @classmethod
    def setUpClass(cls):
        os.environ['G_MESSAGES_DEBUG'] = 'all'
        os.environ['G_DEBUG'] = 'fatal_warnings'
        cls.start_system_bus()
        cls.dbus_con = cls.get_dbus(True)
        (cls.p_mock, cls.obj_bluez) = cls.spawn_server_template(
            'bluez5', {})

        cls.exec_path = [sys.argv[0]]
        if os.getenv('VALGRIND') != None:
            cls.exec_path = ['valgrind'] + cls.exec_path

    @classmethod
    def tearDownClass(cls):
        cls.p_mock.terminate()
        cls.p_mock.wait()

    def setUp(self):
        self.obj_bluez.Reset()
        self.dbusmock = dbus.Interface(self.obj_bluez, dbusmock.MOCK_IFACE)
        self.dbusmock_bluez = dbus.Interface(self.obj_bluez, 'org.bluez.Mock')

    def run_test_process(self):
        # Get the calling function's name
        test_name = inspect.stack()[1][3]
        print(f"Running out-of-process test {test_name}")
        # And run the test with the same name in the OopTests class in a separate process
        out = subprocess.run(self.exec_path + ['OopTests.' + test_name], capture_output=True)
        self.assertEqual(out.returncode, 0, "Running test " + test_name + " failed:" + out.stderr.decode('UTF-8') + '\n\n\nSTDOUT:\n' + out.stdout.decode('UTF-8'))
        if os.getenv('VALGRIND') != None:
            print(out.stderr.decode('UTF-8'))

    def test_no_adapters(self):
        self.run_test_process()

    def test_one_adapter(self):
        self.dbusmock_bluez.AddAdapter('hci0', 'my-computer')
        self.run_test_process()

    def test_one_device(self):
        self.dbusmock_bluez.AddAdapter('hci0', 'my-computer')
        self.dbusmock_bluez.AddDevice('hci0', '22:33:44:55:66:77', 'My Mouse')
        self.run_test_process()

    def test_device_notify(self):
        self.dbusmock_bluez.AddAdapter('hci0', 'my-computer')
        self.dbusmock_bluez.AddDevice('hci0', '22:33:44:55:66:77', 'My Mouse')
        self.run_test_process()

    def test_device_removal(self):
        self.dbusmock_bluez.AddAdapter('hci0', 'my-computer')
        self.run_test_process()

    def test_default_adapter(self):
        self.dbusmock_bluez.AddAdapter('hci0', 'hci0')
        self.dbusmock_bluez.AddAdapter('hci1', 'hci1')
        self.dbusmock_bluez.AddDevice('hci0', '11:22:33:44:55:66', 'Device on hci0')
        self.dbusmock_bluez.AddDevice('hci1', '22:33:44:55:66:77', 'Device on hci1')
        self.run_test_process()

    def test_default_adapter_powered(self):
        path = self.dbusmock_bluez.AddAdapter('hci0', 'my-computer')
        bus = dbus.SystemBus()
        dbusmock_adapter = dbus.Interface(bus.get_object('org.bluez', path), 'org.freedesktop.DBus.Mock')
        dbusmock_adapter.UpdateProperties('org.bluez.Adapter1', {
            'Powered': False,
        })
        self.run_test_process()

    def test_pairing(self):
        adapter_name = 'hci0'
        self.dbusmock_bluez.AddAdapter(adapter_name, 'my-computer')

        address = '11:22:33:44:55:66'
        alias = 'My Phone'

        path = self.dbusmock_bluez.AddDevice(adapter_name, address, alias)
        self.assertEqual(path, '/org/bluez/' + adapter_name + '/dev_' + address.replace(':', '_'))

        self.run_test_process()

    def test_set_trusted(self):
        self.dbusmock_bluez.AddAdapter('hci0', 'my-computer')
        self.dbusmock_bluez.AddDevice('hci0', '11:22:33:44:55:66', 'My Phone')
        self.run_test_process()

    def test_connect(self):
        self.dbusmock_bluez.AddAdapter('hci0', 'my-computer')
        self.dbusmock_bluez.AddDevice('hci0', '11:22:33:44:55:66', 'My Phone')
        self.run_test_process()

    def test_agent(self):
        self.run_test_process()

    def test_connected_input_devices(self):
        self.dbusmock_bluez.AddAdapter('hci0', 'my-computer')
        self.run_test_process()

if __name__ == '__main__':
    unittest.main()
