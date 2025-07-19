import sys

DEBUG_ENABLED = "--debug" in sys.argv

def debug_log(module, message):
    if DEBUG_ENABLED:
        print(f"[debug:{module}] {message}")
