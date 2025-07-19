
import logging
import sys
import os

DEBUG_ENABLED = "--debug" in sys.argv

logger = logging.getLogger("dynamic_power")
logger.setLevel(logging.DEBUG if DEBUG_ENABLED else logging.INFO)

if os.path.exists("/dev/log"):
    from logging.handlers import SysLogHandler
    handler = SysLogHandler(address="/dev/log")
else:
    handler = logging.StreamHandler(sys.stdout)

formatter = logging.Formatter('%(name)s: %(levelname)s: %(message)s')
handler.setFormatter(formatter)
logger.addHandler(handler)

def debug_log(module, message):
    logger.debug(f"[{module}] {message}")

def info_log(module, message):
    logger.info(f"[{module}] {message}")

def error_log(module, message):
    logger.error(f"[{module}] {message}")
