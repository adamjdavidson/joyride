# Optional perspective display

The public roster has seven historical profiles. Five include generated 200 × 200
portrait frames; Freud and Leonardo currently render name-only entries on the
panel. The documentation illustrates those two with credited external historical
images. See [the perspective guide](../../docs/PERSPECTIVES.md).

Regenerate the bundled header with Pillow:

```sh
python3 scripts/encode-perspective-portraits.py firmware/perspective-display/portraits.json
```

The manifest only includes approved historical frames. Keep private photographs
and living-person portraits out of the public manifest and header. Consult
[the objects guide](../../docs/OBJECTS.md) for the exact monochrome board,
dependencies, source attribution and programming instructions.
