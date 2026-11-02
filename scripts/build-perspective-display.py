#!/usr/bin/env python3
"""Build the Waveshare 1.54 monochrome Wi-Fi perspective monitor (8 MB flash)."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
tools = root / "data/firmware-tools"
subprocess.run([
    str(tools / "bin/arduino-cli"), "--config-file", str(tools / "arduino-cli.yaml"), "compile",
    "--fqbn", "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=8M,PartitionScheme=huge_app,PSRAM=opi",
    "--build-path", str(tools / "perspective-build"), "--jobs", "3",
    str(root / "firmware/perspective-display")
], cwd=root, check=True)
