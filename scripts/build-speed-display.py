#!/usr/bin/env python3
"""Build the Waveshare round speed monitor using the installed Arduino toolchain."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
tools = root / "data/firmware-tools"
sketch = root / "firmware/speed-display"
vendor = tools / "waveshare-s3-128/ESP32-S3-Touch-LCD-1.28-Demo/Arduino/libraries/TFT_eSPI"
flags = f"-DUSER_SETUP_LOADED -include {sketch}/tft_setup.h"
subprocess.run([
    str(tools / "bin/arduino-cli"), "--config-file", str(tools / "arduino-cli.yaml"), "compile",
    "--fqbn", "esp32:esp32:esp32s3:CDCOnBoot=default,FlashSize=16M,PartitionScheme=huge_app,PSRAM=enabled",
    "--library", str(vendor),
    "--build-property", "compiler.cpp.extra_flags=" + flags,
    "--build-path", str(tools / "speed-build"), "--jobs", "3", str(sketch)
], cwd=root, check=True)
