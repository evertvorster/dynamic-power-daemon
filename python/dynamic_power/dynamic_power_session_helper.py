#!/usr/bin/env python3
import asyncio
import logging
import os
import time
import yaml

from dbus_next.aio import MessageBus
from dbus_next.service import ServiceInterface, method
from dynamic_power.config import load_user_config
from dynamic_power.utils import (
    get_power_source, get_battery_status,
    get_cpu_load, get_cpu_freq,
    set_panel_autohide, set_refresh_rate,
    get_uid
)
from dynamic_power.dynamic_power_user import get_process_override, write_matches, send_profile, send_thresholds

CONFIG_PATH = os.path.expanduser("~/.config/dynamic_power/config.yaml")

class UserBusInterface(ServiceInterface):
    def __init__(self):
        super().__init__("org.dynamic_power.UserBus")

    @method()
    def Ping(self) -> 's':
        return "pong"

    @method()
    def GetMetrics(self) -> 'a{sv}':
        return self.metrics if hasattr(self, "metrics") else {}

class UserLogic:
    def __init__(self, config, bus_iface):
        self.config = config
        self.bus_iface = bus_iface
        self.last_match = None

    async def run(self):
        logging.info("UserLogic started")
        while True:
            try:
                self.loop_once()
            except Exception as e:
                logging.exception("UserLogic error: %s", e)
            await asyncio.sleep(self.config.get("general", {}).get("poll_interval", 5))

    def loop_once(self):
        match = get_process_override(self.config)
        if match:
            profile = match.get("active_profile")
            if profile:
                send_profile(profile, is_user_override=False)
            write_matches([match])
        else:
            write_matches([])

class SensorLogic:
    def __init__(self, config, bus_iface, uid):
        self.config = config
        self.bus_iface = bus_iface
        self.uid = uid
        self.last_power_source = None
        self.metrics_path = f"/run/user/{uid}/dynamic_power_metrics.yaml"
        self.poll_interval = 5

    async def run(self):
        logging.info("Sensor loop started (poll=%ss)", self.poll_interval)
        while True:
            try:
                await self.poll_once()
            except Exception as e:
                logging.exception("SensorLogic error: %s", e)
            await asyncio.sleep(self.poll_interval)

    async def poll_once(self):
        load_1s = get_cpu_load()
        freq = get_cpu_freq()
        power_source = get_power_source()
        battery_status = get_battery_status()

        # Power transition
        if power_source != self.last_power_source:
            logging.info("Power source changed %s → %s", self.last_power_source, power_source)
            self.last_power_source = power_source
            await self.handle_power_transition(power_source)

        self.write_metrics_file(load_1s, freq, power_source, battery_status)
        if hasattr(self.bus_iface, "metrics"):
            self.bus_iface.metrics = {
                "load_1s": load_1s,
                "cpu_freq_avg": freq,
                "power_source": power_source,
                "battery_status": battery_status,
            }

    def write_metrics_file(self, load, freq, source, battery):
        data = {
            "timestamp": time.time(),
            "load_1s": load,
            "cpu_freq_avg": freq,
            "power_source": source,
            "battery_status": battery
        }
        try:
            os.makedirs(os.path.dirname(self.metrics_path), exist_ok=True)
            with open(self.metrics_path + ".tmp", "w") as f:
                yaml.safe_dump(data, f)
            os.replace(self.metrics_path + ".tmp", self.metrics_path)
        except Exception as e:
            logging.warning("Failed to write metrics YAML: %s", e)

    async def handle_power_transition(self, source):
        panel_cfg = self.config.get("panel", {})
        if panel_cfg.get("auto_hide", {}).get("enable_on_battery", False):
            set_panel_autohide(source == "Battery")

        if panel_cfg.get("refresh_rate", {}).get("apply_on_switch", False):
            target = "battery_hz" if source == "Battery" else "ac_hz"
            hz = panel_cfg.get("refresh_rate", {}).get(target)
            if isinstance(hz, int):
                set_refresh_rate(hz)

async def main():
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s %(message)s")
    uid = get_uid()
    config = load_user_config()

    bus = await MessageBus().connect()
    iface = UserBusInterface()
    bus.export("/", iface)
    await bus.request_name("org.dynamic_power.UserBus")

    user_logic = UserLogic(config, iface)
    sensor_logic = SensorLogic(config, iface, uid)

    await asyncio.gather(
        user_logic.run(),
        sensor_logic.run()
    )

if __name__ == "__main__":
    asyncio.run(main())