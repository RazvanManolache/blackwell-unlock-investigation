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

## Status

**⏸️ Not started — 0%.** No live testing has been done on this field yet.
Flagged as a priority avenue for the pre-driver harness specifically, since
most of this project's other open avenues structurally require an
already-booted OS.

## See also

- [24 — FSP/COT scratch registers](../24-fsp-cot-scratch-registers/README.md)
- [07 — port-checklist vs. GB202 feasibility writeup](../07-port-checklist-gb202-blackwell/README.md)
