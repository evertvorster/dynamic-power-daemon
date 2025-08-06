import dbus
import dbus.service
import dbus.mainloop.glib
from gi.repository import GLib
import threading
import time

from .debug import info_log, debug_log, error_log

BUS_NAME = "org.dynamic_power.Daemon"
OBJECT_PATH = "/org/dynamic_power/Daemon"

_set_profile_override = None
_set_poll_interval = None
_set_thresholds = None
_state_iface = None

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

def set_current_state(profile, low, high):
    if _state_iface:
        _state_iface._active_profile = profile
        _state_iface._threshold_low = float(low)
        _state_iface._threshold_high = float(high)

class DynamicPowerInterface(dbus.service.Object):
    def __init__(self, loop):
        self.loop = loop
        bus = dbus.SystemBus()
        bus_name = dbus.service.BusName(BUS_NAME, bus=bus)
        super().__init__(bus_name, OBJECT_PATH)
        self._active_profile = None
        self._threshold_low = None
        self._threshold_high = None
        info_log("dbus", f"System DBus service '{BUS_NAME}' registered at {OBJECT_PATH}")

    @dbus.service.method(BUS_NAME, in_signature="", out_signature="a{sv}")
    def GetDaemonState(self):
        return {
            "active_profile": self._active_profile or "unknown",
            "threshold_low": self._threshold_low or 0.0,
            "threshold_high": self._threshold_high or 0.0,
            "timestamp": time.time()
        }

    @dbus.service.method(BUS_NAME, in_signature="", out_signature="s")
    def Ping(self):
        debug_log("dbus", "Ping received")
        return "Pong from root dynamic_power daemon"

    @dbus.service.method(BUS_NAME, in_signature="sb", out_signature="b")
    def SetProfile(self, profile, is_user_override):
        request = (profile, is_user_override)
        if request == getattr(self, "_last_set_profile", None):
            debug_log("dbus", f"Duplicate SetProfile request ignored: {profile} (boss={is_user_override})")
            return True
        self._last_set_profile = request
        debug_log("dbus", f"SetProfile requested: {profile} (boss={is_user_override})")
        if _set_profile_override:
            _set_profile_override(profile, is_user_override)
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
    global _state_iface
    _state_iface = iface

_set_user_profile = None

def set_user_profile_callback(func):
    global _set_user_profile
    _set_user_profile = func
