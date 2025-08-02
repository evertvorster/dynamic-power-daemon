from dbus_next.service import ServiceInterface, method
from dbus_next.aio import MessageBus
from dbus_next.constants import BusType
from dbus_next import Variant
import asyncio
from .debug import debug_log, info_log, error_log

class DynamicPowerInterface(ServiceInterface):
    def __init__(self):
        super().__init__("org.dynamic_power.Daemon")
        self._state_provider = None
        self._set_user_profile_cb = None
        self._set_thresholds_cb = None

    def register_state_provider(self, callback):
        self._state_provider = callback

    def register_set_user_profile(self, callback):
        self._set_user_profile_cb = callback

    def register_set_load_thresholds(self, callback):
        self._set_thresholds_cb = callback

    @method()
    def GetDaemonState(self) -> 'a{sv}':
        if not self._state_provider:
            error_log("dbus_interface", "GetDaemonState called with no provider")
            return {}
        try:
            state = self._state_provider()
            return {
                "threshold_low": Variant('d', state["thresholds"]["low"]),
                "threshold_high": Variant('d', state["thresholds"]["high"]),
                "active_profile": Variant('s', state["active_profile"]),
            }
        except Exception as e:
            error_log("dbus_interface", f"Exception in GetDaemonState: {e}")
            return {}

    @method()
    def SetUserProfile(self, profile: 's') -> 'b':
        if not self._set_user_profile_cb:
            error_log("dbus_interface", "SetUserProfile called but no callback registered")
            return False
        try:
            self._set_user_profile_cb(profile)
            return True
        except Exception as e:
            error_log("dbus_interface", f"Failed to handle SetUserProfile: {e}")
            return False

    @method()
    def SetLoadThresholds(self, low: 'd', high: 'd') -> '':
        if not self._set_thresholds_cb:
            error_log("dbus_interface", "SetLoadThresholds called but no callback registered")
            return
        try:
            self._set_thresholds_cb(low, high)
        except Exception as e:
            error_log("dbus_interface", f"Failed to set thresholds: {e}")


async def serve_on_dbus(interface: DynamicPowerInterface):
    bus = await MessageBus(bus_type=BusType.SYSTEM).connect()
    bus.export("/org/dynamic_power/Daemon", interface)
    await bus.request_name("org.dynamic_power.Daemon")
    info_log("dbus_interface", "org.dynamic_power.Daemon registered on system DBus")
    return bus  # Return the bus so it can be closed if needed
