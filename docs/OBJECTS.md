# Optional objects and gizmos: what I used

Here is how I brought Joyride into the physical world: a racing wheel and pedals for my hands and feet, small displays that give different parts of the conversation a place on the desk, and objects I can pick up, put down and associate with a way of thinking.

This is a walkthrough of my setup. I’m sharing the actual parts and what I did with them so you have something concrete to borrow from. The arrangement is still an experiment, and the parts are replaceable. You can start with the browser, try one physical element, or build a completely different arrangement.

## A Logitech wheel for my hands, pedals for my feet

I use a **Logitech G29 racing wheel and pedal set**. It gives me a wheel, accelerator, brake, clutch, paddles and buttons in one physical arrangement. My hands and feet can do different things while I stay with the idea I am developing.

I mapped the wheel to **creative reach**: left asks for more conventional thinking, right for more surprising or outlandish possibilities. The paddles make smaller, one-step adjustments. Centering the wheel returns to the current baseline.

The **accelerator** asks for a quicker, more direct exchange. The **brake** asks for more deliberation when partly pressed and stops the response when pressed firmly. Releasing the brake leaves the conversation stopped; tapping the accelerator resumes it. The **clutch** changes how much initiative the AI takes, from following my lead to taking more responsibility for developing the work.

Buttons handle deliberate moves such as saving the current settings as my new normal, temporarily returning to the original defaults, or removing context. The interface calls these **Set cruise (L2)**, **Hold for default (L3)**, **Clear cards (R1)** and **Remove next (R2)**. The button-learning interface lets you assign these functions to the physical buttons you want.

That combination lets me explore several things at once. My hands might move toward a stranger idea while my foot asks for a more considered response. I can stop, sit with what happened, and resume. The wheel and pedals give those changes a bodily feel.

The included Linux adapter is in `services/wheel/`. It reads the controls without sending force-feedback commands to the wheel. The [manufacturer’s page](https://www.logitechg.com/en-us/shop/p/driving-force-racing-wheel) identifies the G29 family; the adapter targets USB `046d:c24f`. The [README](../README.md#how-this-version-works) explains the current control meanings in more detail.

## Six small Waveshare displays

I wanted a collection of small instruments, a little like a cockpit: each display has a particular job, and I can see the state of the collaboration at a glance. The main screen can hold the conversation while these other pieces keep the activity, settings and context visible around it.

My setup uses six **Waveshare** displays. They have different shapes and kinds of screen, which lets me try different ways of presenting the information. Here is what each one does.

### 1. The current activity

The **Waveshare ESP32-C6-Touch-AMOLED-2.16** is a 480 × 480 display for the current mode: Download, Expand, Challenge, Structure or Implement. During a perspective visit, it can show the visitor and the activity waiting for me when I return. This gives the question “what are we doing together right now?” its own visible place.

[Board documentation](https://docs.waveshare.com/ESP32-C6-Touch-AMOLED-2.16) · [Firmware](../firmware/mode-display/)

### 2. Deliberation and speed

The **Waveshare ESP32-S3-Touch-LCD-1.28** is a round, 240 × 240 screen. I use it as a speed/deliberation gauge, showing where the exchange sits between deeper consideration and a quicker response. It makes the pace setting visible as I use the pedals.

[Board documentation](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.28) · [Firmware](../firmware/speed-display/)

### 3. Creative reach

The **Waveshare ESP32-S3-Touch-LCD-1.47** is a rectangular, 172 × 320 screen, used sideways in this setup. It shows creative reach: how conventional or wild I am asking the conversation to become. Turning the wheel changes this setting, and the display lets me see it separately from the conversation text.

[Board documentation](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.47) · [Firmware](../firmware/reach-display/)

### 4. Initiative and status

The **Waveshare ESP32-S3-Touch-AMOLED-1.91** is a 240 × 536 display, also used in landscape. It shows the initiative level and whether the system is ready, working or stopped. This is where I can glance to see how much room I have given the AI to lead.

[Family documentation](https://docs.waveshare.com/ESP32-S3-AMOLED-1.91) · [Firmware](../firmware/initiative-display/)

### 5. The context on the desk

The **Waveshare ESP32-S3-ePaper-3.97** is an 800 × 480 monochrome e-paper screen, used in portrait orientation here. It lists the active context cards: the material I have brought into the conversation. Giving that material a separate display helps keep track of what is currently in play.

[Board documentation](https://docs.waveshare.com/ESP32-S3-ePaper-3.97) · [Firmware](../firmware/context-display/)

### 6. The visiting perspective

The **Waveshare ESP32-S3-ePaper-1.54** is a small, 200 × 200 monochrome e-paper screen. I use it for the visiting thinker’s portrait and name. Inviting a perspective can then have a visible presence beside the work; ending the visit clears the visitor state. The public firmware starts with name-only entries, and the portrait-encoding tool lets you supply your own images.

This particular board is the monochrome model **without G** in its name. A similarly named four-color model uses different display commands. Matching the exact board matters when reusing the firmware.

[Board documentation](https://docs.waveshare.com/ESP32-S3-ePaper-1.54) · [Firmware](../firmware/perspective-display/)

### Connecting the displays

These displays receive their updates over **Wi-Fi**. USB is used for programming and can also provide power from a charger. Each display receives the relevant state from the same Joyride session; the screen is a view of the collaboration, not a separate AI conversation.

The included firmware covers these six roles. My exploratory purchases also included a 1.54-inch four-color ESP32-S3 e-paper board (reported SKU 34586), a 1.54-inch ESP32-C6 e-paper board (34393), and a 1.54-inch monochrome/touch listing (34211). Those listings helped me explore options, but they should not all be read as additional working display targets. Purchase descriptions and delivered revisions can differ; use the exact model printed on your board when choosing firmware.

## Objects and cards I can place on the desk

I use **two ACS ACR1552 NFC readers** to detect cards or tagged objects. A tag gives an object an identity that Joyride can associate with a mode, a perspective or a piece of context. The shape and meaning of the object are yours to choose.

In the current interaction, placing an assigned perspective card starts a visit; removing it ends the visit. A context card works differently: placing it adds the context, which stays available after I lift the card. I remove that context deliberately with the controls. Two readers let objects be detected independently at two stations.

This gives the act of inviting someone or bringing material into the conversation a physical form. I can choose something, move it into place and take it away again.

The reader service is in `services/nfc/`. It reads tag identity and leaves the contents of the tag alone. The [ACS reference manual](https://www.acs.com.hk/download-manual/13475/REF-ACR1552U-Series-1.05.pdf) and [pyscard guide](https://pyscard.sourceforge.io/user-guide.html) explain the underlying reader interfaces.

## Making objects with my printer

I use a **Bambu Lab P2S 3D printer** to create some of the physical objects for this experiment. Being able to make an object gives me a way to try a shape, hold it, move it around and decide whether it feels right for the idea or action I want it to represent. [About the printer](https://blog.bambulab.com/the-icon-redefined-meet-the-p2s-a-completely-reengineered-version-of-the-ultra-productive-p1-series/).

A printed object can carry an NFC tag so the system recognizes it. Printing also opens up possibilities for holders and enclosures that put the displays where I want them. Those are ways to extend the arrangement; this repository currently includes the software and firmware, without a collection of printable CAD or STL files.

**But you can do whatever you want. There are lots of ways to bring the physical world into how you think with AI.**

Use objects you already have. Make something from clay, cardboard or wood. Put a tag on a familiar object. Try a dial, a joystick, a musical controller, a pressure-sensitive object or a different set of buttons. An object can also help you think simply through holding it, moving it or giving it a place; it does not have to send a signal to a computer.

You might want a single control, a room full of objects, or a completely different set of bodily movements. My wheel, screens and printer are one route into the experiment. The interesting part is finding out what your arrangement makes possible for you.

## The computer and the other pieces

A **Raspberry Pi** runs my server and local input adapters. An **OWC USB hub** helps connect the inputs. The earlier version also explored a **Samsung Galaxy Fold** as a touch interface with optional tilt and microphone controls; that version remains available through `npm run pilot`.

You can run the main web application on another computer with Node.js. The hub and phone are optional parts of my setup, and other suitable devices can fill those roles. The Linux input adapters and board-specific firmware have their own requirements, described below.

## Finding an interface for your own gizmo

Start with what you want to do with your body: turn, squeeze, press, hold, place, lift, move or glance. Consider the feel of the action and whether it can happen alongside your thinking. You can try the meaning of a control on screen, then explore what the physical experience adds.

When you want the object to control Joyride, look for an accessible interface in its manufacturer’s documentation: USB HID/gamepad, keyboard events, MIDI, serial, Bluetooth or a documented local network API. Check whether your operating system can read it and whether it reports continuous position, press/release, presence/removal or only a tap. Search terms such as “USB rotary encoder HID,” “MIDI control surface” and “PC/SC NFC reader” can help you find candidates.

A new gizmo usually needs an adapter that translates its signals into Joyride actions, plus calibration and a policy for what happens when it disconnects. The details below are here if you want to reuse or adapt my implementation.

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
