# Optional tooling

`npm start` runs the core web experiment on port 8788. Everything below is optional.

## Persistent conversation pilot

Run `npm run pilot` and open http://localhost:8787. Enter the pairing code printed in the terminal. This earlier interface has SQLite conversation history, JSON export, keyboard steering, optional microphone recording and phone tilt. Its control settings apply to the next turn; they do not interrupt a streaming reply.

Its **API setup** panel can save a key and budget to `data/api-settings.json`. That saved configuration takes precedence over environment variables in both servers. Restart the main server after changing saved settings in the pilot. The ledger is cumulative and shared by both interfaces; preserve `data/` to retain accounting. Avoid concurrent paid calls from both experiments.

Speech capture uses a legacy browser audio API and needs browser compatibility checks. Microphone and motion generally require a secure context (localhost or HTTPS). Phone controls remain experimental.

## Private remote access

Both services listen on loopback. To access one from another computer, use a private HTTPS reverse proxy. Set `DEMO_PUBLIC_ORIGIN` for the main interface or `APP_ORIGIN` for the earlier pilot to its exact HTTPS origin. Preserve existing proxy routes. The optional `bash scripts/setup-pilot.sh` wizard explains one private-network approach for the earlier pilot and captures credentials locally. It does not run paid tests.

## Input and display services

The [objects guide](OBJECTS.md) documents device examples, dependencies, sources and build requirements. Input services are local read-only sources; the browser interprets their events after **Connect controls**. Display state includes mode, perspective, levels, status and context titles, not chat text, context bodies or API credentials.

`services/displays/usb_bridge.py` requires pyserial. `scripts/provision-display-wifi.py` requires pyserial and cryptography; its current host integration expects Linux NetworkManager and a `wlan0` interface. Adapt those assumptions to your host before running it. It prepares local TLS credentials under ignored `data/displays/` and transfers the selected Wi-Fi profile to a display over USB. It does not belong in the web-only startup path.

Build scripts compile only; they do not flash hardware. They expect Arduino CLI and downloaded libraries in `data/firmware-tools/`. That downloaded toolchain is not included. Provisioning, flash backup, uploads and real panel behavior require checks on the actual board. This public preparation has not rebuilt or reflashed the optional firmware.

## Checks

```sh
npm test
npm run check
python3 -B -m unittest discover -s services/wheel -p 'test_*.py'
python3 -B -m unittest discover -s services/nfc -p 'test_*.py'
```

The Python tests need the corresponding optional driver libraries. Node checks use fake provider boundaries and make no paid calls. A passing transport acknowledgment does not establish visible pixels, correct orientation or useful AI behavior.
