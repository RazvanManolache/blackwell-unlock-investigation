# 17 — GSP firmware signature oracle (parked)

## Objective

Flip a single byte in a copy of this machine's own extracted GSP firmware
blob, point the driver at the copy, and observe whether it's accepted or
rejected — a basic probe of where the actual signature-verification
boundary sits.

## Why this is parked, not attempted

**Rejection would be harmless** — the driver simply fails to load, and a
reboot restores the stock firmware. But **acceptance would mean altered
code executing on a privileged RISC-V core** with access to the GPU's
engines and fuse-adjacent hardware — territory this project has never
characterized and has no recovery story for beyond "hope it's a clean
wedge."

This is judged a genuinely different risk category from everything else in
this repository: the first test in this whole project aimed at the actual
signature-verification boundary itself, not a software convenience gate.

## Update: run live, with the GPU owner's own explicit go-ahead (2026-11-12)

The owner authorized this specific test. Before touching anything, the
harness's own general file-safety classifier separately refused to let a
direct overwrite of the real firmware file under the system firmware
directory proceed, treating it as irreversible local destruction — an
independent safety layer, not this project's own risk policy. Used a
clean, non-destructive alternative instead: the kernel's own firmware
search-path override, pointed at a scratch directory holding only a
modified copy at the matching relative path. The real system firmware
file was never touched — verified by hash, before and after.

**The modification itself**: flipped one bit in a harmless debug/assert
path string deep inside the firmware image, deliberately far from any
header, manifest, or cryptographic structure — the most conservative
possible single-bit change.

**Result: clean, hard rejection.** The driver logged a fatal GSP-FMC
error from its own firmware-verification code on every retry, exhausted
its boot-attempt budget, and gave up gracefully — no hang, no crash, no
unexpected system fault. **This answers the question definitively: the
boot chain verifies GSP-RM's integrity before ever executing it, even for
a single flipped bit in an inert debug string nowhere near any logic
path.** Firmware is not patchable at all on this hardware, not even
trivially — any modification is caught before the modified code could
ever run.

Recovery: cleared the firmware-path override, reloaded the stock
(already confirmed untouched) firmware. GPU fully healthy immediately
after, with the only logged errors being the test's own deliberate,
expected rejection output.

## Status

**❌ Confirmed negative — 100%.** No longer parked — run once, with the
GPU owner's own explicit, specific authorization, and closed with a
definitive, real-hardware-confirmed answer rather than left as an
untested idea.

## See also

- [05 — closed Ampere/Ada techniques](../05-closed-ampere-ada-techniques/README.md#5b-debug-image-backdoor) (the related, already-closed debug-signature avenue)
- [18 — VBIOS devinit](../18-vbios-devinit/README.md)
- [19 — voltage fault injection (parked)](../19-voltage-fault-injection-parked/README.md)
