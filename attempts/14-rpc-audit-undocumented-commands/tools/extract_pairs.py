#!/usr/bin/env python3
"""Extract (CTRL_CMD define, numeric value, paired _PARAMS struct name) triples
from NVIDIA's public ctrl0000/ctrl0080/ctrl2080 headers, using the documented
naming convention: NV<pfx>_CTRL_CMD_<TAIL> value pairs with a
NV<pfx>_CTRL_<TAIL>_PARAMS struct declared in the SAME FILE (confirmed against
ctrl2080gr.h's GR_GET_SM_ISSUE_RATE_MODIFIER and ctrl2080power.h's GC6_ENTRY).
Read-only: parses header text, does not compile or touch the GPU.
"""
import re, sys, os

ROOT = sys.argv[1]
CMD_RE = re.compile(r'#define\s+(NV(2080|0000|0080)_CTRL_CMD_([A-Z0-9_]+))\s+[\(]?(0x[0-9A-Fa-f]+|\d+)')
STRUCT_RE = re.compile(r'\}\s*(NV(?:2080|0000|0080)_CTRL_[A-Z0-9_]+_PARAMS)\s*;')

pairs = []
for dirpath, _, files in os.walk(ROOT):
    for fn in files:
        if not fn.endswith('.h'):
            continue
        path = os.path.join(dirpath, fn)
        try:
            text = open(path, encoding='utf-8', errors='replace').read()
        except OSError:
            continue
        cmds = CMD_RE.findall(text)
        if not cmds:
            continue
        structs_in_file = set(STRUCT_RE.findall(text))
        for full, pfx, tail, val in cmds:
            want = f"NV{pfx}_CTRL_{tail}_PARAMS"
            if want in structs_in_file:
                pairs.append((full, val, want, path))

seen = set()
out = []
for full, val, struct, path in pairs:
    if full in seen:
        continue
    seen.add(full)
    out.append((full, val, struct, path))

out.sort(key=lambda x: x[0])
for full, val, struct, path in out:
    print(f"{full}\t{val}\t{struct}\t{os.path.relpath(path, ROOT)}")

print(f"# total matched: {len(out)}", file=sys.stderr)
