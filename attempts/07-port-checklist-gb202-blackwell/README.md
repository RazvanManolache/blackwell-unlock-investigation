# 07 — Port-readiness checklist vs. a GB202 feasibility writeup

## Objective

A separate researcher's GB202 feasibility/port-research project defined a
5-item checklist for what a genuine Blackwell port of the Ampere/Ada unlock
technique would need. This project tracked its own status against that
same checklist.

## Item 1/5: a decrypted debug booter/FMC to mine gadgets from

**Status: ⏸️ blocked, 0%.** This is the actual blocker — every other item in
the 5-item checklist is reusable or already solved; this one specifically
is not, and nothing in this project found a way past it (see
[attempt 05](../05-closed-ampere-ada-techniques/README.md) for why the
decryption/leak/backdoor avenues are all closed).

## Item 5/5: a kmod/EFI payload that runs pre-driver

**Status: 🔬 done, 100% (infrastructure, not an unlock result).** This is
exactly what this project's own Raspberry Pi USB-gadget harness and its
pre-driver UEFI diagnostic payload already are — see the root README's
"Infrastructure" section. Listed here because it satisfies this specific
checklist item, not because it constitutes progress toward an unlock by
itself.

## Status

**Mixed: item 1 is the real, unresolved blocker (⏸️ 0%); item 5 is
infrastructure that already exists (🔬 100%).** The other 3 items in the
original 5-item list were judged reusable/solved by the source project and
weren't independently re-verified here.

## See also

- [02 — open-source driver recheck](../02-open-source-driver-recheck/README.md)
- [05 — closed Ampere/Ada techniques](../05-closed-ampere-ada-techniques/README.md)
- [phiraml/gpu-unlock-blackwell](https://github.com/phiraml/gpu-unlock-blackwell) — source of this checklist
