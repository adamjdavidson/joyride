#!/usr/bin/env python3
"""Encode optional 200x200 monochrome portraits from a local manifest. Requires Pillow.

Manifest: [{"id":"example","first":"Example","last":"Lens","image":"relative.png"}]
Omit image for a name-only display. Paths are relative to the manifest.
"""
import argparse
import json
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("manifest", type=Path)
parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "firmware/perspective-display/portraits.h")
args = parser.parse_args()
entries = json.loads(args.manifest.read_text())
lines = ['#pragma once', '#include <stdint.h>', 'struct Portrait { const char *id, *first, *last; const uint8_t *pixels; };']
rows = []
seen = set()
for i, entry in enumerate(entries):
    ident = entry['id']
    if not isinstance(ident, str) or not ident or len(ident) > 31 or ident in seen:
        raise ValueError('Each perspective needs a unique ID of 1–31 characters')
    seen.add(ident)
    image = entry.get('image')
    symbol = 'nullptr'
    if image:
        with Image.open(args.manifest.parent / image) as source:
            if source.width != source.height:
                raise ValueError('Supply a square composition; faces are not cropped automatically')
            frame = source.convert('L').resize((200, 200), Image.Resampling.LANCZOS).convert('1', dither=Image.Dither.FLOYDSTEINBERG)
            packed = bytes(value ^ 255 for value in frame.tobytes())
        symbol = f'portrait_{i}'
        lines.append(f'static const uint8_t {symbol}[5000] = {{')
        lines.extend('  ' + ','.join(f'0x{v:02x}' for v in packed[j:j+25]) + ',' for j in range(0, len(packed), 25))
        lines.append('};')
    fields = [json.dumps(entry[k], ensure_ascii=True) for k in ['id', 'first', 'last']]
    rows.append('  {' + ', '.join(fields + [symbol]) + '},')
if not rows:
    raise ValueError('Supply at least one perspective')
args.output.write_text('\n'.join(lines + ['static const Portrait portraits[] = {'] + rows + ['};']) + '\n')
print(f'Encoded {len(rows)} perspective entries')
