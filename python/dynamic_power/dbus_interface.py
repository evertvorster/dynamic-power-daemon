import dbus
import dbus.service
import dbus.mainloop.glib
from gi.repository import GLib
import threading

from .debug import info_log, debug_log, error_log

BUS_NAME = "org.dynamic_power.Daemon"
OBJECT_PATH = "/org/dynamic_power/Daemon"

_set_profile_override = None  # Callback set by main daemon

def set_profile_override_callback(cb):
    """Register a callback that will be invoked when SetProfile is called via DBus."""
    global _set_profile_override
    _set_profile_override = cb
    debug_log("dbus", "Profile override callback registered")

class DynamicPowerInterface(dbus.service.Object):
    def __init__(self, loop):
        self.loop = loop
        bus = dbus.SystemBus()
        bus_name = dbus.service.BusName(BUS_NAME, bus=bus)
        super().__init__(bus_name, OBJECT_PATH)
        info_log("dbus", f"System DBus service '{BUS_NAME}' registered at {OBJECT_PATH}")

    @dbus.service.method(BUS_NAME, in_signature="", out_signature="s")
    def Ping(self):
        debug_log("dbus", "Ping received")
        return "Pong from root dynamic_power daemon"

    @dbus.service.method(BUS_NAME, in_signature="s", out_signature="b")
    def SetProfile(self, profile):
        debug_log("dbus", f"SetProfile requested: {profile}")
        if _set_profile_override:
            _set_profile_override(profile)
            return True
        else:
            error_log("dbus", "SetProfile called but no callback registered")
            return False

def start_dbus_interface():
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    loop = GLib.MainLoop()
    DynamicPowerInterface(loop)

    thread = threading.Thread(target=loop.run, daemon=True, name="DBusMainLoop")
    thread.start()
    info_log("dbus", "DBus main loop started in background thread")
