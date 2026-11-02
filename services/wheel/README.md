# Optional wheel adapter

See [the objects guide](../../docs/OBJECTS.md) for dependencies, supported hardware, commands, event contracts and customization. Run this service independently from the web application.


## Trooper V2 and controller handoff

The same service also discovers Hyperkin Trooper V2 (USB 2e24:386a).
Left/right supplies full steering, forward gas, back brake/stop, and holding the
left button (evdev 288) full clutch. The latest meaningful input takes over the
controller bank. Inactive releases and jitter do not steal control. Unplugging
an active controller clears held inputs and stops the app; reconnect does not
replay gas or buttons. After braking, centering remains stopped; tap forward
or hold it for 420 ms to resume. Other joystick buttons are unused.

`state.devices` lists detected controllers and `state.active_controller` names
the current owner. Controller reset events clear browser input. Physical
forward/back acceptance remains an explicit check in [current status](../../docs/STATUS.md).
