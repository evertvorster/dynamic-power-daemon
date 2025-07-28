#!/usr/bin/env python3
import os
import sys

# Prevent running as non-root
if os.geteuid() != 0:
    print("Error: dynamic_power must be run as root.", file=sys.stderr)
    sys.exit(1)

from dynamic_power import run

if __name__ == "__main__":
    run()
