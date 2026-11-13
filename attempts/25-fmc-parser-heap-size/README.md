# 25 — FMC/FSP parser heap-size field

## Objective

The FMC/FSP parser's heap-size field is, per a prior researcher's analysis,
the one genuinely "soft" (non-signature-checked) input in the entire
pre-GSP boot chain — with 11 valid configurations documented.

## Why this matters

Almost everything upstream of GSP-RM on Blackwell is either signature-checked
or encrypted (see the root README's architecture notes: `FSP (ROM, PL3) ->
GSP BootROM (ROM, PL3) -> FMC (host-supplied, signed+encrypted, PL3) ->
GSP-RM (PL2)`). A field that's accepted *without* signature verification,
even a narrow one like a heap-size parameter, is architecturally a different
category of target than everything else this project examined — the kind
of input a parser bug could plausibly act on before any cryptographic check
has a say.

This is also the one open avenue this project's own pre-driver Raspberry Pi
USB-gadget harness can actually reach on its own: unlike the suspend/resume
surface (written by an already-booted OS driver) or the per-SM channel
surface (needs a live OS-level RM client), both the FMC parser and the
FSP/COT scratch registers above are pre-driver, BAR0-reachable surfaces a
USB-gadget payload can exercise directly.

## Update (2026-11-12) — the field identified and confirmed; the broader hypothesis ruled out

**Method:** identified the real field in NVIDIA's own open-source driver —
a genuine, public registry key, `RmGspFirmwareHeapSizeMB`, drives a GSP
firmware-initialization struct field (`GspFwWprMeta.gspFwHeapSize`) via a
specific chip-HAL function, clamped by chip-specific minimum/maximum
bounds. Cross-validated against an independent live test on real GB202
hardware from prior-art research: a 256 MiB heap request was honoured
byte-exact; requesting below the minimum does not fail cleanly — it hard
wedges the boot (`NV_ERR_TIMEOUT`) instead.

**Result:** this is the one layout parameter in the whole pre-GSP boot
chain that's honoured across a wide live-tested range rather than pinned to
one exact signature-checked value — exactly the "soft input" framing this
item started from, now concretely confirmed rather than just asserted. Not
fully pinned down: the literal "11 valid configurations" count referenced
in this item's original framing wasn't traced to one specific enumerated
list in either source examined — doesn't block closing the mechanism
question, since the real behavior (honoured arbitrarily within bounds, not
a fixed small set) is now established from source plus live confirmation.

**The natural follow-up question — does varying this field reach the
`FEAT_OVR_PLM` gate itself — was then asked directly, and ruled out.**
Combined with separately-established timing evidence that `FEAT_OVR_PLM`
closes at a fixed point in the boot sequence regardless of whether GSP-RM
runs at all, none of the soft FMC/FSP boot-layout inputs (this field
included) are consumed by, or plausibly reach, the fuse-read logic that
sets `FEAT_OVR_PLM`'s value. Valid-but-unusual values are honoured as pure
layout changes; invalid ones hard-wedge the boot with a small set of
specific error codes — there is no observed or source-traced third
behavior (an altered `PLM` outcome) anywhere in this input space. The
layout-input surface and the `PLM`-gate surface are different,
non-overlapping parts of the boot sequence, not the same mechanism.

One adjacent question was explicitly **not** pursued: whether an
*induced* boot failure (the hard-wedge case above) could leave `PLM` in a
different transient state is a distinct, fault-injection-shaped question in
the same risk category as the already-parked
[voltage fault-injection attempt](../19-voltage-fault-injection-parked/README.md)
— noted for completeness, not investigated further without the GPU owner's
own separate explicit go-ahead.

**Status: ❌ Confirmed negative on the core hypothesis — 100%.** The field
itself is a real, now well-understood mechanism; it does not reach the
`FEAT_OVR_PLM` gate. Fully resolved statically, no live hardware access
needed for this conclusion beyond the independent third-party live
confirmation already cited.

## See also

- [24 — FSP/COT scratch registers](../24-fsp-cot-scratch-registers/README.md)
- [07 — port-checklist vs. GB202 feasibility writeup](../07-port-checklist-gb202-blackwell/README.md)
