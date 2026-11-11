# 02 — Re-verify the open driver has no hidden GB20x booter-ucode branch

## Objective

A prior researcher's claim — that NVIDIA's open-source kernel driver has no
GB20x branch in `kgspGetBinArchiveBooterLoadUcode` (the function that would
supply a decryptable "booter" ucode image for this die family, the way one
exists for Turing/Ampere/Ada) — was a snapshot from one driver version. The
claim needed re-checking against whatever driver version is *actually
installed* on this machine, since a routine driver update could in principle
have silently added a GB20x variant since that snapshot was taken.

## Hypothesis

NVIDIA has not added a GB20x booter-ucode fallback; Blackwell uses the newer
FMC (firmware management controller) boot path instead, with no booter stage
at all.

## Method

Cloned [NVIDIA/open-gpu-kernel-modules](https://github.com/NVIDIA/open-gpu-kernel-modules)
at tag `595.99.02` — the **exact** version confirmed running on this machine
via `nvidia-smi`/`modinfo`, not a nearby snapshot — and inspected
`g_bindata.c`'s inclusion list for `kgspGetBinArchiveBooterLoadUcode`.
Re-checked again later at tag `620.70.06` (a newer release than this
machine's own installed version) via the GitHub contents API, to see whether
the gap had since closed at a *later* point in time.

## Result

**Confirmed absent at both versions.** The inclusion list only has
TU102/TU116/GA100/GA102/AD102 — no GB20x/GB100/GB10B variant exists at
either the exact-running version or the newer one. Blackwell dies visibly
use the FMC path instead, in the same file.

## Status

**❌ Confirmed negative — 100% complete.** This closes the "maybe there's a
decryptable booter ucode after all" door cleanly, strengthened rather than
just repeated at a second, later point in time.

## See also

- [05 — closed Ampere/Ada techniques](../05-closed-ampere-ada-techniques/README.md#5a-booter-leak) (the original booter-leak avenue this re-verifies)
- [07 — port-checklist vs. GB202 feasibility writeup](../07-port-checklist-gb202-blackwell/README.md)
