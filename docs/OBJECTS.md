# Optional objects and gizmos

Joyride begins in the browser. Physical objects are another way to explore the same experiment. The devices below describe the original setup and exploratory inventory; they are examples, not a shopping list or compatibility guarantee. Use what you own, substitute something else, or stay entirely on screen.

## Find an object by the move it gives you

Start with a useful gesture: turn to change creative reach, press to stop, hold to adjust initiative, place an object to invite a perspective, or glance at a display to see the current activity. Try that move on screen first.

Look for an accessible interface in the manufacturer's documentation: USB HID/gamepad, keyboard events, MIDI, serial, Bluetooth or a documented local network API. Check whether your operating system can read it and whether it reports continuous position, press/release, presence/removal or only a tap. Search terms such as “USB rotary encoder HID,” “MIDI control surface,” “PC/SC NFC reader,” and “microcontroller display serial example” can help you find candidates. Search by the interface and gesture rather than copying a brand.

Confirm the exact model, revision, driver and power requirements before connecting it. A look-alike board can have a different panel controller. Reuse a known interface when possible; a new gizmo usually needs a small adapter, calibration and a disconnect policy. Keep hardware reading separate from what the gesture means.

## Examples from the original experiment

| Object | Possible role | Included support |
| --- | --- | --- |
| Logitech G29 wheel, pedals and buttons | Creative reach, pace, initiative, stop/resume and shortcuts | Linux evdev reader in `services/wheel/` |
| Two ACS ACR1552 NFC readers with tags/cards and figurines | Separate placement/removal of perspectives and context references | PC/SC reader in `services/nfc/`; browser card mapping |
| Raspberry Pi host | Run the web server and local adapters | Ordinary Node server; the web core also runs on other computers |
| Samsung Galaxy Fold / another phone | Touch controls, optional tilt and microphone | Earlier persistent pilot via `npm run pilot` |
| OWC USB hub / another suitable powered hub | Connect several inputs | No hub-specific software |
| Bambu Lab printer / another 3D printer | Optional housings, holders and objects | Housing exploration only; no CAD models or printer integration included |

The [G29 manufacturer's page](https://www.logitechg.com/en-us/shop/p/driving-force-racing-wheel) identifies the wheel family. Joyride's driver targets USB `046d:c24f` and reads axis/button events without force feedback. For readers, consult the [ACS reference manual](https://www.acs.com.hk/download-manual/13475/REF-ACR1552U-Series-1.05.pdf) and [pyscard guide](https://pyscard.sourceforge.io/user-guide.html). Reading a tag ID does not write data to the tag or make the tag an authentication credential.

The following display targets have firmware in this repository. Each receives one slice of shared state. Browser-only use needs none of them.

| Role / source folder | Original target | Upstream documentation |
| --- | --- | --- |
| `mode-display` | Waveshare ESP32-C6-Touch-AMOLED-2.16, 480 × 480 | [Board docs](https://docs.waveshare.com/ESP32-C6-Touch-AMOLED-2.16) |
| `speed-display` | Waveshare ESP32-S3-Touch-LCD-1.28, round 240 × 240 | [Board docs](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.28) |
| `reach-display` | Waveshare ESP32-S3-Touch-LCD-1.47, 172 × 320 | [Board docs](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.47) |
| `initiative-display` | Waveshare ESP32-S3-Touch-AMOLED-1.91, 240 × 536 | [Family docs](https://docs.waveshare.com/ESP32-S3-AMOLED-1.91) |
| `context-display` | Waveshare ESP32-S3-ePaper-3.97, 800 × 480 | [Board docs](https://docs.waveshare.com/ESP32-S3-ePaper-3.97) |
| `perspective-display` | Waveshare ESP32-S3-ePaper-1.54, monochrome 200 × 200, **without G** | [Board docs](https://docs.waveshare.com/ESP32-S3-ePaper-1.54) |

The exploratory inventory also included a 1.54-inch four-color ESP32-S3 e-paper board (reported SKU 34586), a 1.54-inch ESP32-C6 e-paper board (34393), and a 1.54-inch monochrome/touch listing (34211). Purchase descriptions and delivered revisions can differ. These extra variants are not all supported by the included firmware. The G/four-color driver is not interchangeable with the monochrome perspective target. Check the label and official documentation for your actual board.

## Run the optional input adapters

On Debian-family Linux, install the optional packages for the adapters you need:

```sh
sudo apt-get install python3-evdev
sudo apt-get install pcscd libccid pcsc-tools python3-pyscard
```

Run each adapter in its own terminal from the repository root:

```sh
python3 services/wheel/service.py
python3 services/nfc/nfc_service.py
```

The wheel exposes `http://127.0.0.1:8766/state`; NFC uses port 8767. Confirm `connected`/reader state before opening **Connect controls** in Joyride. Match button functions by pressing them. Present a card, choose its meaning, assign it, then lift and present it again. These browser mappings last for the current page session. Perspective removal ends a visit; context remains active until removed in the interface.

The wheel reader uses kernel axis limits: steering is −1…1, pedals 0…1. Current axes are `ABS_X` steering, `ABS_Z` throttle, `ABS_RZ` brake, `ABS_Y` clutch, plus hat and button events. `--device` pins a device; the default discovery filters its USB identity. Edit discovery and `AXES` to support another evdev device. `mappings.json` defines optional semantic action events, and SIGHUP reloads it. The current browser consumes raw input and its own button bindings; changing action-event names alone will not remap the browser.

The NFC reader keeps station identity separate from card meaning. `stations.json` and `cards.json` start empty. Add your own reader serials/roles and card mappings if using the service independently; SIGHUP reloads those registries. The browser's card mapping is separate. `49-thinking-console-nfc.rules` is an optional policy template with placeholders, not a ready-to-install grant. Ordinary local PC/SC access may need no extra policy.

Both input APIs offer `/state`, `/health` and `/events?after=CURSOR&session=SESSION`. Start with state, then consume ordered events. On `resync`, reconcile current state without inventing past button presses. On disconnect, cancel held gestures; do not treat a disconnect as an intentional release or a request to send a message. NFC API version 2 handles independent readers; it does not promise multi-card inventory on one reader.

## Adapt any other gizmo

The main browser reads `/hardware/wheel/{state,events}` and `/hardware/nfc/{state,events}` through its own local server. An alternative adapter can emit those contracts, or you can add another local route and translate its events into the `Instrument.reduce` actions (`MODE`, `PERSON`, `AXIS`, `PADDLE`, `INITIATIVE`, `STOP`, `RESUME`, `CONTEXT`). Keep reconnect behavior explicit. Continuous controls need range calibration, dead zones and a stable neutral position.

Displays consume projected metadata from `src/instrument-displays.js`; USB sends newline-delimited JSON through `services/displays/usb_bridge.py`. Wi-Fi uses `src/wifi-displays.js` and the corresponding firmware `wifi_link` implementation for TLS, per-device authentication, heartbeat and acknowledgments. Use those implementations as the protocol reference. Changing browser levels or perspective IDs may require updating firmware validation and screen layouts.

## Rebuild display tooling

The build recipes preserve the original experimental targets. They are not a universal installer. They were developed with Arduino CLI 1.5.1 and arduino-esp32 3.3.3. Install an appropriate CLI for your host at `data/firmware-tools/bin/arduino-cli`, provide its `arduino-cli.yaml`, install the ESP32 core, and download the needed vendor libraries into the paths used by the selected `scripts/build-*-display.py` recipe. The recipes contain exact FQBN, flash, PSRAM and compiler flags.

| Recipe | Local dependency path below `data/firmware-tools/` | Source |
| --- | --- | --- |
| Mode | `waveshare-c6-216/01_Arduino_Libraries/lvgl9/lvgl` and `XPowersLib` | [Vendor repo](https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-2.16), original commit `294543798f1a44e2f2c4d2976522323f2beee11d` |
| Speed | `waveshare-s3-128/ESP32-S3-Touch-LCD-1.28-Demo/Arduino/libraries/TFT_eSPI` | [Vendor archive](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28/ESP32-S3-Touch-LCD-1.28-Demo.zip) |
| Reach | `waveshare-s3-147/ESP32-S3-Touch-LCD-1.47-Demo/Arduino/libraries/GFX_Library_for_Arduino` | [Vendor archive](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.47/ESP32-S3-Touch-LCD-1.47-Demo.zip) |
| Initiative | `waveshare-s3-191/01_Arduino_Libraries/lvgl8/lvgl` | [Vendor repo](https://github.com/waveshareteam/ESP32-S3-AMOLED-1.91), original commit `1986f380a4e668da5f4e2b93de9bd82555788018` |
| Context | Panel/paint/font sources included in `firmware/context-display` | [Vendor repo](https://github.com/waveshareteam/ESP32-S3-ePaper-3.97), source commit `9b12d40731a80213b927ee8a421cae4082952819` |
| Perspective | Panel/paint/font sources included in `firmware/perspective-display` | [Vendor repo](https://github.com/waveshareteam/ESP32-S3-ePaper-1.54), panel source commit `aaf395c1bbd86f7c5b7a2f9f0401241ff5c44956` |

For example, once dependencies are in place, `python3 scripts/build-speed-display.py` compiles into `data/firmware-tools/speed-build/`. Back up the actual board's flash before any upload and use the board's real serial port and matching FQBN. USB bridge boards and native USB boards use different CDC options. The speed recipe's explicit FSPI selection and the context panel's ALDO3 power initialization are required by those targets.

For Wi-Fi provisioning, create a local Python environment with `pyserial` and `cryptography`, then run `scripts/provision-display-wifi.py --help`. Choose a device ID and role; `--prepare` creates local credentials only, while `--device YOUR_SERIAL_PORT` transfers credentials to the board. The current helper expects Linux NetworkManager and `wlan0`. Restart the main server after registering devices, when no live request is running. A display retaining an image can be powered off; confirm both acknowledgment and visible refresh.

The public perspective display starts with text-only entries. `scripts/encode-perspective-portraits.py` accepts a local JSON manifest and, optionally, square images whose reuse rights you have checked; it generates `portraits.h`. Pillow is required. The encoder never downloads or generates portraits. No personal photographs or generated face bitmaps are bundled.
