import dbus
import dbus.service
import dbus.mainloop.glib
from gi.repository import GLib
import threading

from .debug import info_log, debug_log, error_log

BUS_NAME = "org.dynamic_power.Daemon"
OBJECT_PATH = "/org/dynamic_power/Daemon"

_set_profile_override = None
_set_poll_interval = None
_set_thresholds = None

def set_profile_override_callback(cb):
    global _set_profile_override
    _set_profile_override = cb
    debug_log("dbus", "Profile override callback registered")

def set_poll_interval_callback(cb):
    global _set_poll_interval
    _set_poll_interval = cb
    debug_log("dbus", "Poll interval callback registered")

def set_thresholds_callback(cb):
    global _set_thresholds
    _set_thresholds = cb
    debug_log("dbus", "Thresholds callback registered")

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
        error_log("dbus", "SetProfile called but no callback registered")
        return False

    @dbus.service.method(BUS_NAME, in_signature="u", out_signature="b")
    def SetPollInterval(self, interval):
        debug_log("dbus", f"SetPollInterval requested: {interval}")
        if _set_poll_interval:
            _set_poll_interval(interval)
            return True
        error_log("dbus", "SetPollInterval called but no callback registered")
        return False

    @dbus.service.method(BUS_NAME, in_signature="dd", out_signature="b")
    def SetLoadThresholds(self, low, high):
        debug_log("dbus", f"SetLoadThresholds requested: low={low}, high={high}")
        if _set_thresholds:
            _set_thresholds(low, high)
            return True
        error_log("dbus", "SetLoadThresholds called but no callback registered")
        return False

def start_dbus_interface():
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    loop = GLib.MainLoop()
    iface = DynamicPowerInterface(loop)

    thread = threading.Thread(target=loop.run, daemon=True)
    thread.start()
    info_log("dbus", "DBus main loop started in background thread")