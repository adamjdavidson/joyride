# Customize the experiment

Joyride's defaults are proposals. There is no required number of modes, no scientifically calibrated initiative scale, and no universal mapping from a gesture to a thought. Try a small change, compare the experience, and keep what helps.

## Activities and perspectives

Edit `public/joyride-config.js`. Browser labels, live instructions and display-state validation share this file. Use a stable activity key and give it a `description` and `instructions`. An optional `sample` supplies its scripted response. For example, add this property inside `activities`:

```js
Reflect: {
  description: 'Notice what has changed in your understanding.',
  instructions: 'Reflect the change in the user’s understanding. Distinguish their claims from your interpretation. Leave room for uncertainty.',
  sample: 'Prepared example: the question has shifted from what to build to what kind of help you want while thinking.'
},
```

Restart and reload; the new activity appears as a tab and works in live requests. Keep the built-in keys if you want their walkthroughs unchanged; edit the `tours` and `scriptedResponse` sections in `public/instrument-prototype.html` when replacing them. Walkthroughs are authored scenes, not automatically generated from new definitions.

A perspective has `name`, `short`, `mark`, `role`, `description`, `instructions` and optionally `sample`. Use lowercase IDs such as `systems-thinker`; choose public figures or unnamed methods for public examples. A visit temporarily suspends the chosen activity and returns to it when you leave. The generated response interprets a lens; it does not speak for the actual person.

Use the [perspective skill](../.agents/skills/thought-leaders-to-perspectives/SKILL.md) to research candidates and distinguish source evidence from your operational interpretation. New definitions should make different decisions on the same input, not merely sound different.

## Levels and steering criteria

`speed` and `initiative` are ordered arrays; positions are one-based. Add or remove entries, edit their labels and meanings, and keep `defaults.speed` and `defaults.initiative` within their new bounds. Each speed entry has editable `instructions` for response pacing; its `effort` must be supported by the model you configure. `activityBoundaryLevel` is the highest initiative level that stays within the current activity. Above it, a live response may choose a different activity.

`creative.labels` must have an odd number of labels, at least three; the middle is the current normal. Its length sets the wheel/paddle range. `creative.instructions` defines what that axis means for live generation. Defaults can be captured again in the interface with **Set cruise**. **Hold for default** temporarily returns to the configuration defaults.

Level counts and labels flow into the on-screen gauges and request validation. The scripted scenes still illustrate the original low/middle/high choices. Rewrite their response branches if you want them to demonstrate a fundamentally different scale. Board firmware has fixed layouts and bounds; adapt it separately when changing level counts.

The actual steering mechanics live in the `Instrument` reducer in `public/instrument-prototype.html`: spring return, pedal thresholds, stop/resume, capturing defaults and the interaction between continuous inputs and discrete choices. Change those to try different gestures. Adding a completely new steering axis also requires changing the state, rendering, live payload and server validation; it is not a configuration-only change.

## Context, appearance and provider

- Replace `originalBooks` in the main HTML with your own sample context. Real private context belongs in your local session, not a public config file.
- Edit the main HTML's CSS to change appearance. `public/styles.css` styles the separate earlier pilot.
- `src/provider.js` contains the model, speech model, output cap and response reservation formula. `public/controls.js` contains the earlier pilot's effort mapping. Check both when changing models.
- The persistent pilot uses `public/modes.js` and `public/controls.js`; its mode buttons are in `public/index.html`. These are intentionally a separate earlier experiment.

After an edit, run `npm test` and `npm run check`. Try the changed control at both extremes, stop/resume, a perspective visit and returning to the activity. Compare scripted behavior with an explicitly chosen live test only if you want to incur API charges.
