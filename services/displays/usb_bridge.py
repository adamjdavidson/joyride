#!/usr/bin/env python3
"""Forward latest display state to one explicitly configured USB serial device.

No network access, flashing, input control, or automatic selection of other ports.
"""
import argparse
import json
import queue
import sys
import threading
import time
import serial


def emit(event):
    print(json.dumps(event, separators=(",", ":")), flush=True)


def run(device):
    incoming = queue.Queue(maxsize=1)
    ended = threading.Event()

    def read_input():
        try:
            for line in sys.stdin:
                if len(line) > 16000:
                    continue
                try:
                    packet = json.loads(line)
                    if packet.get("v") != 1:
                        continue
                    try:
                        incoming.get_nowait()
                    except queue.Empty:
                        pass
                    incoming.put_nowait(packet)
                except (ValueError, AttributeError, queue.Full):
                    continue
        finally:
            ended.set()

    threading.Thread(target=read_input, daemon=True).start()
    port = None
    latest = None
    last_input = 0
    last_send = 0
    reconnect_at = 0
    buffer = bytearray()
    while not ended.is_set():
        now = time.monotonic()
        fresh = False
        try:
            latest = incoming.get_nowait()
            last_input = now
            fresh = True
        except queue.Empty:
            pass
        try:
            if port is None and now >= reconnect_at:
                candidate = serial.Serial(port=None, baudrate=115200, timeout=0, write_timeout=0.5)
                # Assert host readiness for native USB CDC; leave reset deasserted.
                candidate.dtr = True
                candidate.rts = False
                candidate.port = device
                candidate.open()
                port = candidate
                buffer.clear()
                last_send = 0
                emit({"type": "link", "state": "open"})
            if port is not None:
                if latest is not None and (fresh or now - last_send >= 1):
                    packet = latest if now - last_input < 4 else {"v": 1, "seq": 0, "connected": False, "state": None}
                    port.write((json.dumps(packet, ensure_ascii=False, separators=(",", ":")) + "\n").encode())
                    last_send = now
                buffer.extend(port.read(4096))
                while b"\n" in buffer:
                    line, _, tail = buffer.partition(b"\n")
                    buffer = bytearray(tail)
                    try:
                        event = json.loads(line)
                        if isinstance(event, dict) and event.get("type") in ("display_ready", "display_ack"):
                            emit(event)
                    except (ValueError, UnicodeDecodeError):
                        pass
                if len(buffer) > 8192:
                    buffer.clear()
        except (serial.SerialException, OSError):
            if port:
                port.close()
            port = None
            reconnect_at = now + 2
            emit({"type": "link", "state": "disconnected"})
        time.sleep(0.01)
    if port:
        port.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--device", required=True)
    run(parser.parse_args().device)
