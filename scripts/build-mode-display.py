#!/usr/bin/env python3
"""Build the first mode display with the pinned, locally installed toolchain."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
tools = root / "data/firmware-tools"
vendor = tools / "waveshare-c6-216/01_Arduino_Libraries"
sketch = root / "firmware/mode-display"
flags = f"-DLV_CONF_INCLUDE_SIMPLE -I{sketch}"
subprocess.run([
    str(tools / "bin/arduino-cli"), "--config-file", str(tools / "arduino-cli.yaml"), "compile",
    "--fqbn", "esp32:esp32:esp32c6:CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=huge_app",
    "--library", str(vendor / "lvgl9/lvgl"), "--library", str(vendor / "XPowersLib"),
    "--build-property", "compiler.c.extra_flags=" + flags,
    "--build-property", "compiler.cpp.extra_flags=" + flags,
    "--build-path", str(tools / "mode-build"), "--jobs", "3", str(sketch)
], cwd=root, check=True)
