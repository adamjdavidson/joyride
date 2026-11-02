#!/usr/bin/env python3
"""Build the Waveshare 1.91-inch initiative/status AMOLED monitor."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
tools = root / "data/firmware-tools"
sketch = root / "firmware/initiative-display"
flags = f"-DLV_CONF_INCLUDE_SIMPLE -I{sketch}"
subprocess.run([
    str(tools / "bin/arduino-cli"), "--config-file", str(tools / "arduino-cli.yaml"), "compile",
    "--fqbn", "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi",
    "--library", str(tools / "waveshare-s3-191/01_Arduino_Libraries/lvgl8/lvgl"),
    "--build-property", "compiler.c.extra_flags=" + flags,
    "--build-property", "compiler.cpp.extra_flags=" + flags,
    "--build-path", str(tools / "initiative-build"), "--jobs", "3", str(sketch)
], cwd=root, check=True)
