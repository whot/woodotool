#!/usr/bin/python3

import sys

from gi.repository import GLib
from gi.repository import Gio

DBUS_PATH_PREFIX = "/org/freedesktop/WooDoTool1"
DBUS_NAME_PREFIX = "org.freedesktop.WooDoTool1"

class WoodoToolDBusUnavailable(BaseException):
    pass


class _WDTDbusObject(object):
    def __init__(self, interface, path):
        self._dbus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
        self._proxy = Gio.DBusProxy.new_sync(self._dbus, Gio.DBusProxyFlags.NONE, None,
                                             DBUS_NAME_PREFIX,
                                             path, 
                                             "{}.{}".format(DBUS_NAME_PREFIX, interface),
                                             None)
        if self._proxy.get_name_owner() == None:
            raise WoodoToolDBusUnavailable()

    def dbus_property(self, name):
        p = self._proxy.get_cached_property(name)
        if p != None:
            p = p.unpack()
        return p

    def dbus_call(self, method, type, *value):
        val = None
        if type is not None:
            val = GLib.Variant("({})".format(type), value)
        r = self._proxy.call_sync(method,
                                  val,
                                  Gio.DBusCallFlags.NO_AUTO_START,
                                  500, None)
        if r != None:
            r = r.unpack()
        return r

class WoodoToolKeyboard(_WDTDbusObject):
    def __init__(self, path):
        _WDTDbusObject.__init__(self, "Keyboard", path)

    def press(self, keycode):
        keycode = int(keycode)
        self.dbus_call("Press", "u", keycode)

    def release(self, keycode):
        keycode = int(keycode)
        self.dbus_call("Press", "u", keycode)

class WoodoToolMouse(_WDTDbusObject):
    def __init__(self, path):
        _WDTDbusObject.__init__(self, "Mouse", path)

    def press(self, button):
        button = int(button)
        self.dbus_call("Press", "u", button)

    def release(self, button):
        button = int(button)
        self.dbus_call("Press", "u", button)

    def relative(self, x, y):
        x = int(x)
        y = int(y)
        self.dbus_call("MoveRelative", "ii", x, y)

    def absolute(self, x, y):
        x = int(x)
        y = int(y)
        self.dbus_call("MoveAbsolute", "ii", x, y)

class WoodoToolManager(_WDTDbusObject):
    def __init__(self):
        _WDTDbusObject.__init__(self, "Manager", DBUS_PATH_PREFIX)
        kbd_path = self.dbus_call("GetKeyboard", None, None)[0]
        mouse_path = self.dbus_call("GetMouse", None, None)[0]

        self._kbd = WoodoToolKeyboard(kbd_path)
        self._mouse = WoodoToolMouse(mouse_path)

        self.calltable = {
                "key press" : self._kbd.press,
                "key release" : self._kbd.release,
                "mouse press" : self._mouse.press,
                "mouse release" : self._mouse.release,
                "mouse relative" : self._mouse.relative,
                "mouse absolute" : self._mouse.absolute,
        }

    def run(self, fd):
        line = fd.readline()

        while len(line) > 0:
            for key, func in self.calltable.items():
                if line.startswith(key):
                    args = line.split()[2:]
                    func(*args)
            line = fd.readline()

mgr = WoodoToolManager()
with open(sys.argv[1]) as f:
    mgr.run(f)

