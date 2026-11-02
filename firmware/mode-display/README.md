# Optional mode display

The current C6 firmware renders five supplied object artworks in navy (#292163)
on white, with a distinct no-mode state. It remains dedicated to the activity
while a portrait visit is shown separately. Source PNGs and a hash manifest are
in `assets/`; `scripts/build-mode-display.py` first runs the Pillow encoder to
create `mode_artwork.h`, then compiles with the existing LVGL toolchain.

See [mode meanings and art](../../docs/MODES.md) and
[board setup and dependencies](../../docs/OBJECTS.md). This public update includes
source; it does not install firmware on an attached device.
