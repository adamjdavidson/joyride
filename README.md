# Joyride

**A conversation you can steer.**

Joyride is an initial experiment: a playful proposal for changing the kind of help you get from an AI while you think. Choose an activity, adjust creative reach, deliberation and initiative, invite a perspective, or bring context into the conversation.

The web experience is the core product. You can explore it with a browser and ordinary on-screen controls. The supplied modes, levels, steering criteria and perspectives are one starting point for a toy—not a validated theory of thinking or a finished product. Change them, replace them, and find out what works for you.

## Try it

Install Node.js 24 or newer, download or clone this repository, and run these commands from its folder:

```sh
npm start
```

Open **http://localhost:8788**. There are no npm dependencies, no build step, and no API key required for the scripted experience. Click **Play demo**, try an activity, invite a perspective, move the on-screen controls, and use **Stop** and **Continue**. **Hide instruments** gives you a quieter conversation view.

Scripted replies are prepared examples, not AI responding to arbitrary input. They make the interaction explorable without spending money. Conversation, saved defaults, context cards and input mappings last until the page reloads; download context items to keep them. Only the view preference persists in browser storage.

## Make it yours

Start with [public/joyride-config.js](public/joyride-config.js). It holds the shared activity descriptions and instructions, public-figure perspectives, level labels, model effort mapping, creative steering criteria and initial defaults. Edit the file, restart the server, then reload the browser.

The initial activities are **Download, Expand, Challenge, Structure and Implement**. The initial perspectives are interpretations inspired by **Sigmund Freud, Niccolò Machiavelli and Leonardo da Vinci**. These are experimental prompts, not impersonations, endorsements or authoritative representations of their work. Motive speculation is treated as hypothesis, never diagnosis.

[Customize Joyride](docs/CUSTOMIZING.md) explains how to add your own modes, levels, criteria, sample responses and context. The included [thought-leaders-to-perspectives skill](.agents/skills/thought-leaders-to-perspectives/SKILL.md) guides an assistant from a public figure's documented work to a distinct, source-grounded thinking lens. You can also use an unnamed lens, such as “find the throughline.”

## Use live AI

Copy `.env.example` to `.env`. Set `PILOT_MODE=live`, supply your own `OPENAI_API_KEY`, and choose a positive `PILOT_BUDGET_USD`. Restart `npm start`, click **Use live AI**, and enter a thought. Messages and active context are sent to OpenAI. Steering interrupts a response and continues from its visible text using the new settings.

The model adapter is in [src/provider.js](src/provider.js). Its model identifiers and cost estimates are experimental defaults; check your account's available models and current rates before paid use. Model effort values must match your chosen model. Live behavior has automated mock coverage; this public package has not been verified with paid API calls. The local budget ledger reserves costs conservatively and retains uncertain reservations after interruptions. It is not an account-wide spending cap or a billing statement.

Live generation produces text. It has no connected browsing, research agents, filesystem access or code execution. High initiative changes the conversation; it does not grant external tool access.

Keep `.env` and `data/` private. Keys are server-side. The service binds to loopback; a public GitHub repository does not turn this personal prototype into a public multi-user service. Private remote access and optional credential setup are described in [the tooling guide](docs/TOOLING.md).

## Add objects, if you like

Any gizmo can become a candidate: an object you already own, a button, dial, card, sensor or small display. It needs an adapter that translates its signals into Joyride's controls; arbitrary hardware is not automatically compatible. Start with a gesture or feedback you want, then look for an accessible interface.

The [optional objects guide](docs/OBJECTS.md) covers finding objects, evaluating interfaces, the original setup's hardware examples, and connecting alternatives. Input services, display firmware, provisioning and build tools are included as optional experiments.

## Explore the code

| Location | Purpose |
| --- | --- |
| `public/instrument-prototype.html` | Main interface, state transitions, scripted walkthroughs and input mapping |
| `public/joyride-config.js` | Shared experimental definitions |
| `public/instrument-live.js`, `src/instrument-live.js` | Streaming, stop/continue, instruction construction and budget gating |
| `src/instrument-displays.js`, `src/wifi-displays.js` | Display state, serial and authenticated Wi-Fi transport |
| `services/`, `firmware/`, `scripts/` | Optional adapters, firmware, build and provisioning tools |
| `public/app.js`, `src/pilot.js`, `src/store.js` | Earlier persistent conversation pilot, available with `npm run pilot` |
| `test/` | Node tests; optional adapter tests live beside their services |

Run `npm test` and `npm run check`. The earlier pilot also includes speech capture, phone tilt, pairing, SQLite conversation history and JSON export. It uses separate controls and applies changes on the next turn; the main Joyride interface steers streaming replies. See [tooling](docs/TOOLING.md) for the distinction.

This repository starts with clean source history. Private installation records, personal photographs, saved conversations, credentials and toolchain downloads are not part of the public experiment. See [third-party notices](THIRD_PARTY_NOTICES.md) for included asset and firmware attribution.

Joyride’s original code is released under the [MIT License](LICENSE). Third-party assets retain their own licenses.
