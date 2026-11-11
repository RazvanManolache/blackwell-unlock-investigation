# 24 — FSP/COT scratch-register boot-state timeline

## Objective

Read FSP/Chain-of-Trust scratch registers across several boots to build a
boot-state timeline — purely passive, not previously captured by this
project's own diagnostic payload.

## Method

Target registers: `NV_PFSP_FALCON_COMMON_SCRATCH_GROUP_2` (indices 0–3, 15,
16, 28). **Note:** this item's own original register name was wrong and had
to be fixed first — `NV_PSEC_FALCON_COMMON_SCRATCH_GROUP_2` (an SEC2-prefixed
name) does not exist in this driver's Blackwell source at all; the real
symbol is `NV_PFSP_...` (FSP-prefixed), consistent with this project's own
core SEC2-to-FSP architectural finding (see the root README's "mechanism"
section — Blackwell's boot chain has no SEC2 stage, FSP does what SEC2 used
to). The other two originally-named registers
(`NV_PMC_ZB_SCRATCH_RESET_PLUS_2`, `NV_PBUS_ZB_SW_SCRATCH`) are confirmed
**absent** from this die's headers entirely — datacenter-Blackwell-only,
not applicable here, not just unread.

Read the corrected register group via direct BAR0 access (zero risk, purely
passive).

## Result

```
GROUP_2[0]  = 0x9f
GROUP_2[1]  = 0
GROUP_2[2]  = 0
GROUP_2[3]  = 0xa
GROUP_2[15] = GROUP_2[16] = GROUP_2[28] = 0xbadf5620
```

The `0x56` tail of the sentinel value is publicly undocumented per this
project's own badf-decoding convention, but falls in the
access-restricted `0x5x` family — this capture is a second, independent,
cross-register sighting of that same undocumented code elsewhere in this
project, useful corroboration for an open question tracked separately.

**This is one boot's snapshot.** The full multi-boot timeline this item
actually wants needs the Raspberry Pi-side diagnostic payload extended to
capture this across many cold boots, which hasn't been done yet.

## Status

**⚠️ Partial — 60%.** One real capture done and register-name bug fixed;
the actual multi-boot timeline this item is named for is not yet built.

## See also

- [25 — FMC parser heap-size field](../25-fmc-parser-heap-size/README.md) (the other pre-driver-reachable avenue in this project)
