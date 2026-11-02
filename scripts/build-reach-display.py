#!/usr/bin/env python3
"""Build the Waveshare 1.47-inch creative reach monitor."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
tools = root / "data/firmware-tools"
vendor = tools / "waveshare-s3-147/ESP32-S3-Touch-LCD-1.47-Demo/Arduino/libraries/GFX_Library_for_Arduino"
subprocess.run([
    str(tools / "bin/arduino-cli"), "--config-file", str(tools / "arduino-cli.yaml"), "compile",
    "--fqbn", "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi",
    "--library", str(vendor), "--build-path", str(tools / "reach-build"),
    "--jobs", "3", str(root / "firmware/reach-display")
], cwd=root, check=True)
