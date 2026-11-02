# Joyride

**Updated November 2, 2026:** [Mode objects and explanations](docs/MODES.md) ·
[Historical perspectives and portraits](docs/PERSPECTIVES.md) ·
[Hardware photos and walkthrough](docs/OBJECTS.md) · [Current status and open repairs](docs/STATUS.md).

The physical experiment still needs RFID reliability work, continuous creative
reach and persistent RFID context. This snapshot documents those limits and keeps
the web demo usable without hardware.


**A conversation you can steer.**

Joyride is part of a project sponsored by OpenAI to explore physical interactions with AI: ways to use our full bodies and physical objects when thinking, imagining and developing ideas with a model.

We exist in the world through our bodies. We move, touch, hold, place, turn and respond. We can stay with a thought while our hands do one thing and our feet do another. Those activities can happen together, without each movement needing to become a separate verbal instruction or the sole object of our attention.

I want to explore what happens when AI becomes part of that experience. Could moving a wheel with your hands while adjusting a pedal with your foot change the way you develop an idea? Could picking up an object, feeling it and placing it beside you help you enter a different way of thinking? Could this open up different ways of imagining what is possible? It might. It might also just be fun, which seems worth exploring too.

This is one guy’s very initial attempt to find out. The choices here reflect how I work and what I wanted to try. It might not work at all. It might work for me and be completely wrong for you. I’m sharing the idea and the working parts so you can try them, change them and build something that makes sense to you.

You can explore the whole set of controls in a browser, using on-screen versions. That is the core experience available in this repository, and the easiest place to begin. Physical inputs and displays connect to the same system when you want to explore what changes when your body is more fully involved. The browser lets you try the interaction; understanding its bodily possibilities requires trying those too.

## Thinking with your body

When we play an instrument, cook or drive, our bodies participate in several things at once. Our hands and feet can have different jobs. We feel resistance, make small adjustments, notice where objects are and respond to what is happening around us. We do not have to stop and describe every movement before making it.

I want to explore that kind of involvement in thinking with AI. You might be absorbed in an emerging idea while your hands turn toward a stranger possibility, your foot asks the collaborator to take more initiative, and another movement brings it back. Your thought, your movements and the response can develop together.

Physical objects bring other possibilities. Something has weight, texture and a place in the room. You can hold it, move it closer, put it down or take it away. Choosing an object to represent a thinker or an activity gives that choice a bodily and spatial presence. A separate display can keep some part of the collaboration visible at a glance.

The way an interaction feels may matter to the thinking it makes possible. A turn, a sustained press and placing an object are different experiences. I want to find out whether those differences help ideas emerge or develop differently, and which arrangements feel natural, awkward, useful or enjoyable. Joyride is an early way to explore those questions.

## Knowing what kind of thinking we are doing

Creative work involves different kinds of attention. In my experience, generating possibilities and editing them down are especially hard to do at the same time. If I am trying to create something while also checking whether it is correct, practical, acceptable or compliant, I can end up stopping an idea before I have found out what it might become.

Both kinds of work matter. I want to be able to choose which one I am doing now, then change when I am ready. I might need to expand, narrow, question, organize and make something, in different orders and more than once.

LLMs often get that choice wrong. A model corrects an idea I wanted to expand. It asks broad questions when I want it to make something. It offers sympathy when I want a thoughtful critic. It starts executing while I am still exploring, or keeps exploring after I have decided to execute.

A central idea in Joyride is to make the current mode explicit to both of us. The interface shows what we are doing; the system supplies that instruction to the model. Changing modes should be an ordinary move in a conversation, including while a reply is arriving. I should be able to ask for a different kind of help without losing the idea I was working on.

This grows out of my [thinking-harness proposal](https://ada.md/foo-thinking/): tooling that helps a person sustain and direct their thinking with an AI. Sometimes the result is a piece of work. Sometimes it is a new understanding, a better question, or a decision that there is nothing to build yet. The proposal reaches beyond what this first implementation does.

## Inviting another mind—and letting it leave

Some of my best work has come from talking with a colleague who thinks differently from me. Someone notices the power dynamics I missed. Someone else sees a mechanism, an unexpected connection or a tension I have been avoiding. Their contribution changes what I can see.

Then I need to get back to my work. With love and respect, I want that colleague, friend or mentor to go away for a while so I can concentrate on making something.

Joyride tries to make that visit explicit. Invite a thinker to sit beside you for a minute. Hear what their perspective brings to the problem. Take what is useful, then end the visit and return to your activity. The public examples use deceased historical figures or unnamed methods. The useful question is what their way of looking helps you notice.

The public starting set contains interpretations inspired by **Jane Jacobs**, **James Baldwin**, **Lucille Ball**, **Vasubandhu**, **Sigmund Freud**, **Niccolò Machiavelli** and **Leonardo da Vinci**. They bring different kinds of attention: lived systems, human truth, playful invention, conceptual frames, inner tensions, power and stability, and observation and invention. These are authored thinking lenses, not the actual people or their endorsements. See the illustrated [perspective guide](docs/PERSPECTIVES.md) for each profile, its questions, boundaries and sources.

## How this version works

There are several things you can change independently. A **mode** chooses the activity. The three **steering settings** change creative reach, deliberation and initiative. A **perspective** is a temporary consultation. **Context cards** bring background material into the conversation. **Stop** gives you a pause without choosing a new activity.

The mappings below are the ones I started with. They are choices to experiment with, not a proposed universal set of controls for thinking.

### Modes: what are we doing?

The interface also calls these *activities*.

| Mode | What you are asking the AI to do |
| --- | --- |
| **Download** | Help get what is on your mind into words, with light structure. Understand and articulate your thought; hold back its own ideas, objections and advice. Ask a specific clarification only when needed. |
| **Expand** | Broaden the idea. Explore possibilities, implications, connections and directions you have not considered. |
| **Challenge** | Look for weaknesses, contradictions, ambiguities and questionable assumptions. Be a thoughtful critic, distinguishing uncertainty from an established error. |
| **Structure** | Work out the order, necessary elements and narrative architecture. Help decide what belongs where and what is missing. |
| **Implement** | Produce something: usable text, an outline, code or slide content in the conversation. Move the work into a concrete form. |

Switching mode keeps your steering settings. You can ask for an expansive challenge, a conventional implementation or a slow, careful exploration. “Challenge” does not automatically mean “stop exploring and produce a plan.”

These are instructions to a probabilistic model, not guaranteed behavior. Two deliberate exceptions to staying in the mode are explained below: higher initiative can permit a change of activity, and a perspective visit temporarily suspends it.

### Steering: how far from the familiar?

The wheel controls **creative reach**. Turn left for more conventional, predictable approaches; turn right for more surprising, experimental or outlandish ones. It changes the ideas and approach you are asking for, not the obligation to be factually honest.

The initial seven positions are **Obvious → Conventional → Restrained → Current normal → Fresh → Outlandish → Ridiculous**. Center is your current baseline. Recenter the wheel to return to it. The on-screen wheel returns when you release it.

The left and right paddles move creative reach one step at a time. A paddle adjustment stays until you change it, move the wheel or release/reset the controls. **Set cruise** can make the current settings your new baseline: what was a wild departure can become the place from which you explore further.

### Pedals: pace, pause and initiative

| Pedal | What it does |
| --- | --- |
| **Accelerator / gas** | Requests a quicker, more direct exchange while pressed. Release it to return to your saved pace. After a stop, release the brake and tap the gas to resume. |
| **Brake** | A partial press asks for slower, more deliberate work. A firm press stops the response. Releasing the brake leaves the conversation stopped until you deliberately resume. |
| **Clutch** | Selects how much initiative the AI should take. A light press asks it to stay close to your lead; pressing further gives it more room to choose and develop the work. Release to return to your saved initiative. You can also choose an initiative level with the numbered on-screen buttons. |

**Deliberation / speed** has five initial levels: **Deepest, Deliberate, Conversational, Quick, Immediate**. In live mode, these change the requested model reasoning effort and response guidance. They express a preference for the exchange; they do not guarantee a particular response time or quality. In the scripted demo, they change the timing of prepared replies.

**Initiative** has six initial levels:

| Level | Meaning |
| --- | --- |
| **1 · Minimal response** | Give a very short response, tightly connected to what I said. |
| **2 · Follow me** | Answer what I asked and leave the next move to me. |
| **3 · Contribute** | Add useful observations or possibilities, while leaving me the lead. |
| **4 · Lead this activity** | Choose and develop a useful next step within the current mode. |
| **5 · Pursue what’s needed** | Take more responsibility for advancing the work, including choosing a different activity when useful. |
| **6 · Take charge** | Reframe, prioritize and develop the intellectual work until there is a concrete result or you need my input. |

Levels 1–4 instruct the AI to stay within the selected mode. Levels 5–6 explicitly give it permission to move between activities. That is a choice you make, rather than an invisible change in what the collaborator thinks its job is. High initiative still only produces work in the conversation; this version has no connected research agents or external execution tools.

### Buttons: deliberate moves

| Button | What it does |
| --- | --- |
| **Set cruise** (shown as **L2**) | Saves the current creative reach, speed and initiative together as your new defaults for this page session. Releasing the controls then returns to those settings. |
| **Hold for default** (**L3**) | Temporarily uses the original configuration defaults while held. Release to return to your own settings. |
| **Clear cards** (**R1**) | Removes all currently active bookshelf cards from the context supplied with subsequent requests. |
| **Remove next** (**R2**) | Arms removal of one context card; choose the card to remove. |
| **Stop** | Stops output while retaining the activity and selected settings. |
| **Continue / Tap gas** | Resumes after a stop, or asks the current activity or perspective to continue. |
| **Return to [activity]** | Ends a perspective visit and returns to the activity you stepped away from. |
| **Make this a context** | Turns a response into a named item you can add to your bookshelf or download. |
| **Reset** | Starts a fresh desk with the original settings and sample bookshelf, clearing the current conversation. |

L2/L3/R1/R2 are labels for the initial button layout. With physical controls, use **Connect controls** to assign those functions to buttons you choose. No particular controller is required for the idea. On screen, click the named buttons. Outside text fields, the arrow keys adjust creative reach, Space stops, Enter continues, `l` sets cruise, and holding `d` temporarily uses the original defaults.

**Play demo** runs prepared walkthroughs. **Use live AI** switches to a real conversation once you have configured your own key. **Hide instruments / Show instruments** changes the view while preserving the current conversation and settings.

### Visits, context and the current state

Click a perspective to begin a visit. It can choose the kind of contribution it finds useful, including challenging or making something, while the previous activity waits. End the visit to return. In the physical version, an assigned object or card can start the visit when placed on a reader; removing it ends the visit.

Context works differently. A card represents background material you want available: a project brief, an audience, a working method or a thought worth keeping. Activate it from the bookshelf or with an assigned physical card. Lifting a context card off the reader leaves its context active; use **Remove next** or **Clear cards** to remove it deliberately. Removal stops supplying the card on subsequent requests, but it does not erase references already present in the conversation.

The display distinguishes being ready, working and stopped. In live mode, it can show thinking before text arrives, and connection or request errors appear explicitly. These are operational states, separate from your chosen thinking mode. Stopping does not mean you have abandoned Expand or finished Implement.

In live use, changing a setting interrupts the current request and starts a continuation with the visible conversation and new instructions. This is stop-and-restart steering; the code does not reach inside an already-running generation and alter it. The intention is to let you redirect the exchange while keeping hold of your thought.

## Try it in the browser

Install Node.js 24 or newer, clone or download this repository, then run this from its folder:

```sh
npm start
```

Open **http://localhost:8788**. There are no npm dependencies, no build step and no API key needed for the scripted experience. Click **Play demo**, then try the controls yourself. The replies are prepared examples so you can explore the interaction without making paid requests; they are not AI responses to arbitrary input.

For live AI, copy `.env.example` to `.env`, set `PILOT_MODE=live`, supply your own `OPENAI_API_KEY` and choose a positive `PILOT_BUDGET_USD`. Restart the server and click **Use live AI**. Your messages and active context are sent to OpenAI. Check the model configuration and current rates before paid use. The local budget ledger is an estimate, not an account-wide spending cap. Keep `.env` and `data/` private; [the tooling guide](docs/TOOLING.md) explains configuration and private remote access.

In the main interface, the conversation, saved defaults, context shelf and control mappings last for the page session. Download useful context items before reloading. Only the view preference persists in browser storage. An earlier interface with saved conversations, speech and phone controls remains available through `npm run pilot`; it applies settings on the next turn rather than steering a streaming reply.

## Try it with objects—or change the whole arrangement

The [optional objects guide](docs/OBJECTS.md) describes the original hardware examples, finding other gizmos, and connecting them. The repository includes input adapters, display firmware, build tools and provisioning tools. You can use different objects, controls and displays; an unfamiliar device will need an adapter that translates its signals into the interaction you want.

You may want a dial instead of a wheel, a button instead of a pedal, or an object whose meaning makes sense only to you. The physical arrangement is part of the experiment. The browser gives you a place to try the underlying behavior before making or buying anything.

## Make a version that fits your mind

The modes, levels, steering criteria, default settings, context and visiting thinkers are all things to reconsider. My distinctions may be unhelpful for your work. Your version might need fewer modes, a different relationship between challenge and expansion, or a completely different set of gestures.

Start with [public/joyride-config.js](public/joyride-config.js), which contains the shared activity and perspective definitions, level labels, instructions and defaults. [Customize Joyride](docs/CUSTOMIZING.md) explains those edits and where to change the interaction itself. Some changes are configuration edits; a new control or physical arrangement may also need code and firmware changes.

The [thought-leaders-to-perspectives skill](.agents/skills/thought-leaders-to-perspectives/SKILL.md) helps an assistant research a public thinker's work and translate it into an editable perspective. It asks for evidence, distinctive questions and a clear distinction between the source material and our interpretation. You can also create an unnamed method or an explicitly fictional visitor.

The larger proposal includes ideas about background collaborators, tracking assumptions and recording where claims came from. Those remain directions to explore. This version has modes, steering, perspective visits, context cards and optional physical connections; it does not yet provide that research and verification system. The model's compliance with your chosen mode, and whether any of this helps you think, still need to be tested through use.

I would like people to try it on an idea they care about, notice what gets easier or harder, and change what feels wrong. Useful, surprising, frustrating or simply enjoyable would all tell us something about where to go next.

## Development and license

Run `npm test` and `npm run check` for the application checks. Optional adapter tests and setup are described in [the tooling guide](docs/TOOLING.md). The main interface is in `public/instrument-prototype.html`; live instructions and streaming are in `src/instrument-live.js` and `public/instrument-live.js`; optional adapters and displays are in `services/` and `firmware/`.

Joyride's original code is released under the [MIT License](LICENSE). Included third-party code and images retain their own licenses; see [third-party notices](THIRD_PARTY_NOTICES.md).
