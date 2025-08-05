# dynamic_power/user_dbus_interface.py
# -*- coding: utf-8 -*-
"""
user_dbus_interface.py

Client-side wrapper for org.dynamic_power.UserBus using dbus-next.
Used by dynamic_power_user and dynamic_power_command to send and receive DBus messages.
"""

from dbus_next.aio import MessageBus
from dbus_next.constants import BusType
from dbus_next.proxy_object import ProxyInterface
from dbus_next import Variant
import asyncio

class UserBusClient:
    def __init__(self, iface: ProxyInterface):
        self.iface = iface
        self._power_state_handler = None

    @classmethod
    async def connect(cls):
        bus = await MessageBus(bus_type=BusType.SESSION).connect()
        introspection = await bus.introspect("org.dynamic_power.UserBus", "/")
        obj = bus.get_proxy_object("org.dynamic_power.UserBus", "/", introspection)
        iface = obj.get_interface("org.dynamic_power.UserBus")
        client = cls(iface)

        # Optional: connect signal handler if user sets one
        iface.on_power_state_changed(lambda s: client._handle_power_state(s))
        return client

    def _handle_power_state(self, state):
        if self._power_state_handler:
            self._power_state_handler(state)

    def on_power_state_changed(self, callback):
        """Register callback for PowerStateChanged signal"""
        self._power_state_handler = callback

    async def get_metrics(self):
        reply = await self.iface.call_get_metrics()
        return {k: v.value for k, v in reply.items()}

    async def get_user_override(self):
        return await self.iface.call_get_user_override()

    async def set_user_override(self, mode: str):
        return await self.iface.call_set_user_override(mode)

    async def get_process_matches(self):
        reply = await self.iface.call_get_process_matches()
        return [
            {
                "process_name": item.get("process_name").value,
                "priority": item.get("priority").value,
                "active": item.get("active").value
            }
            for item in reply
        ]

    async def update_process_matches(self, matches: list[dict]):
        packed = []
        for entry in matches:
            packed.append({
                "process_name": Variant('s', entry.get("process_name", "")),
                "priority": Variant('i', entry.get("priority", 0)),
                "active": Variant('b', entry.get("active", False))
            })
        return await self.iface.call_update_process_matches(packed)

    async def update_daemon_state(self, profile: str):
        return await self.iface.call_update_daemon_state(profile)
