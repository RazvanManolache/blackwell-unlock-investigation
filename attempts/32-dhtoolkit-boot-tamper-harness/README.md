# 32 — A found third-party boot-tamper harness, and what it already answers

## Objective

A 34-script live-hardware test harness, built by the same independent
researcher behind this project's most detailed prior-art source, was found
sitting outside this project's own tracked directory tree during an
unrelated investigation. Catalogue it, work out what's actually relevant
to this card, and use it responsibly rather than either ignoring it or
blindly running everything in it.

## What it is

A numbering scheme inside the scripts matches that researcher's own public
experiment log directly — this is almost certainly their own development
harness, the actual tooling behind their published findings, not a copy of
anything public. Verified byte-identical against its original location
before moving it into the tracked tree and writing a full per-script
catalogue.

Two real mismatches with this project's own hardware were found and
documented rather than assumed:

1. Every script defaults to PCI addresses for a different, dual-GPU
   machine — nothing in the toolkit runs against this card without an
   explicit address override.
2. Two scripts patch Hopper-specific driver files; this card's hardware
   family has its own separate implementation files for the same logic —
   those two scripts, as written, would compile but touch nothing relevant
   on this card.

Other scripts use the exact register addresses and the exact locked
privilege-mask value this project has independently measured on its own
hardware all session — genuinely relevant, just configured for the other
machine.

## The most directly useful part

One script has subcommands that install a tiny boot-time service for
reading registers both before and after the driver binds, across a real
cold boot — a path to exactly the pre-bind timing data this project has
wanted (see [attempt 24](../24-fsp-cot-scratch-registers/README.md)),
without needing any dedicated boot hardware.

## The highest-risk scripts — found, catalogued, deliberately not run

Several scripts in the toolkit are a materially higher risk tier than
anything else attempted in this project: one halts the GPU's security
microcontroller outright (by its own comment, requiring a reboot
regardless of outcome), and two install a deliberately-corrupted signed
firmware image specifically to probe the signature-verification boundary
(the same general question as [attempt 17](../17-firmware-signature-oracle-parked/README.md),
but via a different, more invasive route). None of these were run this
session.

Before considering reconstructing what they'd need (most depend on a
patched firmware binary and an instrumented older driver build that no
longer exist on disk — only the orchestration scripts survived), checked
whether the same researcher's own public experiment log already answered
the question. It had, for several of them, run and measured on real
Blackwell silicon (two RTX 5090s plus an RTX PRO 6000 reference — the same
die family as this project's card):

- **Boot-descriptor tampering** (the signed-firmware-patch class): already
  run, 16 times across four attempts on two separate cards — the same
  fatal firmware-verification error every single time. One adjacent
  variant in the same family is documented as having actually left the
  original rig in a bad post-tamper state once, confirming the risk
  warning in these scripts' own comments wasn't theoretical.
- **The security-microcontroller halt**: already run, with the exact
  halt attempted at microsecond-precision timing against the boot window —
  blocked throughout the entire relevant phase regardless of timing.
- **The write-race-after-reset variant**: already measured — a narrow
  timing window that closes well before anything useful could happen in
  it, and additionally structurally impossible to attempt at all on this
  specific board regardless (see [attempt 15](../15-flr-race-timing/README.md)).

Per this project's own discipline of not re-spending a real cold-boot
cycle to re-confirm an already-measured negative on the same chip family,
none of these three categories were re-run here. Reported this reasoning
to the GPU owner rather than either refusing outright or re-running
everything; owner confirmed: skip reconstructing and re-running the
signed-firmware-patch scripts specifically, since the existing 16-for-16
result on real hardware already settles the question.

## What WAS run live on this card (read-only, zero risk)

Two of the toolkit's safe, read-only scripts were run live, address
overrides corrected for this machine:

1. A register snapshot/self-test, which matched every value this project
   had already independently confirmed — plus one genuinely new,
   unexplained observation: a specific boot-window-only status register
   still reads its "mid-boot" sentinel value a full two hours into normal,
   fully-booted operation, where the published reference measurement on
   the other researcher's hardware shows this value only during a brief
   pre-boot window. Logged as a real, reproducible difference worth
   explaining in a future session (driver-version difference vs. a genuine
   per-board difference — not determined here), not a conclusion in
   itself.
2. A non-halting diagnostic command sequence, reproduced manually with the
   same supporting tools this project's RPC-audit work already built.
   Found a second genuine, reproducible difference from the published
   reference measurement: an internal diagnostic command/address block
   that was live and writable on the reference hardware reads uniformly
   locked-down on this card, post-boot, via a direct register read — and,
   separately, the exact same address range returns completely different
   values depending on whether it's read directly versus through the
   GPU's own management-firmware RPC path (one path returns what looks
   like real internal code-segment data; the other returns an inert
   placeholder pattern). Confirmed stable on repeat, not a fluke. Flagged
   as a real, novel, unexplained observation for a future session, not a
   conclusion about mechanism.

GPU health was confirmed clean (temperature, power draw, pstate, zero
unexplained Xid) before and after every live action in this attempt.

## Status

**⚠️ Partial — 70%.** The toolkit itself is catalogued and two safe
scripts were run live, producing two genuinely new and currently
unexplained observations. The highest-risk scripts remain deliberately
unattempted, not because they were refused, but because the same
researcher's own public measurements on the same die family already
answer what they would have tested — re-running them would cost a real
cold-boot cycle for no new information. Nothing here found a way past the
privilege gate; the new observations are open curiosities, not progress
toward one.

## See also

- [17 — firmware signature oracle](../17-firmware-signature-oracle-parked/README.md) (this project's own, less invasive version of the same signature-boundary question)
- [15 — FLR race timing](../15-flr-race-timing/README.md) (the write-race class this toolkit also covers, independently corroborated)
- [24 — FSP/COT scratch registers](../24-fsp-cot-scratch-registers/README.md) (the pre-bind timing question this toolkit's boot-install capability could help answer)
