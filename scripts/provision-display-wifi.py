#!/usr/bin/env python3
"""Provision a display over USB from the host's saved Wi-Fi. Never prints secrets.

Run with data/firmware-tools/venv/bin/python (cryptography and pyserial installed).
--prepare creates credentials/certificate only; no serial access or Wi-Fi changes.
"""
import argparse
import datetime
import ipaddress
import json
import os
from pathlib import Path
import secrets
import socket
import subprocess
import time

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.x509.oid import NameOID
import serial

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = ROOT / "data/displays"


def private_write(path, content):
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(content)
    temporary.chmod(0o600)
    temporary.replace(path)


def prepare(identifier, role):
    os.umask(0o077)
    DIRECTORY.mkdir(parents=True, exist_ok=True)
    DIRECTORY.chmod(0o700)
    path = DIRECTORY / "wifi.json"
    host = socket.gethostname() + ".local"
    addresses = json.loads(subprocess.check_output(["ip", "-j", "-4", "address", "show", "wlan0"], text=True))
    ip = addresses[0]["addr_info"][0]["local"]
    key_path, cert_path = DIRECTORY / "server-key.pem", DIRECTORY / "server-cert.pem"
    if not key_path.exists() or not cert_path.exists():
        key = ec.generate_private_key(ec.SECP256R1())
        name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, host)])
        now = datetime.datetime.now(datetime.timezone.utc)
        cert = (x509.CertificateBuilder().subject_name(name).issuer_name(name)
                .public_key(key.public_key()).serial_number(x509.random_serial_number())
                .not_valid_before(now - datetime.timedelta(days=1))
                .not_valid_after(now + datetime.timedelta(days=3650))
                .add_extension(x509.BasicConstraints(ca=True, path_length=0), critical=True)
                .add_extension(x509.SubjectAlternativeName([x509.DNSName(host), x509.IPAddress(ipaddress.ip_address(ip))]), critical=False)
                .sign(key, hashes.SHA256()))
        private_write(key_path, key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()).decode())
        private_write(cert_path, cert.public_bytes(serialization.Encoding.PEM).decode())
    config = json.loads(path.read_text()) if path.exists() else {"interface": "wlan0", "host": host, "port": 8791, "keyFile": str(key_path), "certFile": str(cert_path), "devices": []}
    device = next((x for x in config["devices"] if x["id"] == identifier), None)
    if device is None:
        device = {"id": identifier, "role": role, "token": secrets.token_hex(32)}
        config["devices"].append(device)
    if device["role"] != role:
        raise RuntimeError("Existing display role differs; update its registration deliberately.")
    private_write(path, json.dumps(config, indent=2) + "\n")
    return config, device, ip


def provision(config, device, ip, port):
    # Capture secrets in memory, never in command arguments, output or source files.
    connection = subprocess.check_output(["nmcli", "-g", "GENERAL.CONNECTION", "device", "show", "wlan0"], text=True).strip()
    def field(name, elevated=False):
        cmd = (["sudo", "-n"] if elevated else []) + ["nmcli", "--escape", "no", "--show-secrets", "-g", name, "connection", "show", connection]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError("Cannot access the host's saved Wi-Fi configuration.")
        return result.stdout.rstrip("\n")
    ssid = field("802-11-wireless.ssid")
    password = field("802-11-wireless-security.psk", True)
    if not ssid or not password:
        raise RuntimeError("The host needs a saved personal Wi-Fi profile before provisioning.")
    packet = {"type": "wifi_config", "v": 1, "ssid": ssid, "password": password, "host": config["host"], "ip": ip,
              "port": config["port"], "id": device["id"], "token": device["token"], "ca": Path(config["certFile"]).read_text(), "time": int(time.time())}
    wire = (json.dumps(packet, ensure_ascii=False, separators=(",", ":")) + "\n").encode()
    if len(wire) > 8000:
        raise RuntimeError("Provisioning frame is too large.")
    usb = serial.Serial(port=None, baudrate=115200, timeout=.2, write_timeout=3)
    usb.dtr = True
    usb.rts = False
    usb.port = port
    usb.open()
    try:
        usb.reset_input_buffer()
        usb.write(wire)
        deadline = time.monotonic() + 12
        while time.monotonic() < deadline:
            line = usb.readline()
            try:
                event = json.loads(line)
            except (ValueError, UnicodeDecodeError):
                continue
            if event.get("type") == "wifi_configured":
                print("Wi-Fi configuration stored on the display; credentials were not printed.")
                return
        raise RuntimeError("No provisioning acknowledgment; device may still be starting. Retry once ready.")
    finally:
        usb.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--id", required=True)
    parser.add_argument("--role", choices=["mode", "speed", "reach", "initiative", "context", "perspective"], required=True)
    parser.add_argument("--device")
    parser.add_argument("--prepare", action="store_true")
    args = parser.parse_args()
    config, device, ip = prepare(args.id, args.role)
    if args.prepare:
        print("Private display credentials and local TLS certificate prepared.")
    elif args.device:
        provision(config, device, ip, args.device)
    else:
        parser.error("Use --prepare or supply --device.")
