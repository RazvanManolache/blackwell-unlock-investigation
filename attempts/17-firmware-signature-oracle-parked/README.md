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

## Status

**🚫 Parked, not attempted — 0%.** Left in the project's tracked checklist
as a recorded, deliberately-not-executed idea. Needs the GPU owner's own
explicit go-ahead before anyone attempts it, same standard as the other
parked items in this repository (see the root README's safety section).

## See also

- [05 — closed Ampere/Ada techniques](../05-closed-ampere-ada-techniques/README.md#5b-debug-image-backdoor) (the related, already-closed debug-signature avenue)
- [18 — VBIOS devinit](../18-vbios-devinit/README.md)
- [19 — voltage fault injection (parked)](../19-voltage-fault-injection-parked/README.md)
