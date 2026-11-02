#!/usr/bin/env python3
"""Build the Waveshare 3.97-inch Wi-Fi context monitor."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
tools = root / "data/firmware-tools"
subprocess.run([
    str(tools / "bin/arduino-cli"), "--config-file", str(tools / "arduino-cli.yaml"), "compile",
    "--fqbn", "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi",
    "--build-path", str(tools / "context-build"), "--jobs", "3",
    str(root / "firmware/context-display")
], cwd=root, check=True)
