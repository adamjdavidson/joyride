# Current implementation and open work

Updated November 2, 2026. This is an experimental implementation, not a claim that
the physical desk is reliable in every session.

## Included in this public snapshot

- Web-first scripted mode, optional live AI and editable shared configuration.
- Five activities and seven historical perspective profiles, with an illustrated
  [mode guide](MODES.md) and [perspective guide](PERSPECTIVES.md).
- Saved mode/perspective tag assignments in private local
  `data/instrument-cards.json`; new installations start without assignments.
- Reader snapshot reconciliation, mode presence/removal and temporary portrait
  visits. On-screen activities remain available without hardware.
- G29 plus Trooper V2 input support, controller handoff, disconnect reset and
  hidden-tab replay protection. Input services remain optional and separate.
- Six Wi-Fi display roles. The mode screen uses supplied object artwork in navy
  on white; the speed screen includes the upright mounting correction. Five
  historical portrait frames are bundled; two other profiles have name-only
  firmware entries. This update does not flash any attached device.

## Still needs work

1. **RFID activation reliability.** The physical desk has a report of cards losing
   activation. Determine whether this occurs while a tag stays on the reader,
   after removal, or during a browser/service interruption. Compare raw reader
   events with browser state and screen acknowledgments before changing lifetime
   semantics. Automated reconciliation tests do not establish physical reliability.
2. **Continuous creative reach.** The current wheel control rounds to seven
   positions. Continuous needle animation between targets is not continuous input.
   A future change must cover the browser reducer, labels, live/display validation,
   saved defaults and reach-display firmware together. This update retains steps.
3. **Persistent RFID context.** Mode and perspective assignments are durable;
   context/message assignments and captured context remain page-session-only.
   Context currently stays active after its tag is lifted. Persistent context
   content/mappings and the intended physical removal behavior still need design
   and implementation. A tag identifies supplied material; it does not contain
   an unsupplied book or automatically fetch one.
4. **Session persistence.** The main prototype loses conversation, captured shelf
   items and wheel-button mappings on refresh. The separate earlier pilot has
   its own persistent conversation store.

## Lessons for deployment and recovery

A recent context experiment made the HTML require a new script without restarting
the server that served the script allowlist. The page loaded but its required
script returned 404, so the controls never initialized. That attempt was reverted;
it is not included here. API health alone was insufficient.

Before considering a deployment healthy, load every referenced browser asset,
check page initialization and console errors, exercise on-screen controls, then
verify physical input and screen publishing. Preserve healthy hardware services
while replacing an app server. Back up files before a targeted change and keep
rollback separate from new feature work. Confirm the actual running version after
restarting. A failed connection from an isolated agent environment is not evidence
that the host service stopped.

The intended repair order is a working browser/desk, RFID reliability, continuous
reach, then persistent RFID context. Code checks and simulated input are useful;
physical acceptance and paid model tests must be reported separately.
