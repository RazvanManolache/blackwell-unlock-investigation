# 05 — Closed: Ampere/Ada-era technique family doesn't port to Blackwell

Four distinct avenues, all inherited from the prior Ampere/Ada unlock
technique family, all independently investigated and closed for this
project before any live Blackwell hardware testing began. Kept here as a
single combined entry because they share one root cause: Blackwell's boot
chain (`FSP (ROM, PL3) -> GSP BootROM (ROM, PL3) -> FMC (host-supplied,
signed+encrypted, PL3) -> GSP-RM (PL2)`) has no SEC2 stage at all — the
component the original exploit family targeted simply doesn't exist on this
architecture.

## 5a. Booter leak

**Objective:** find or leak a public GB20x SEC2 booter image to mine ROP
gadgets from, the way the original Ampere exploit did.

**Result: ❌ ruled out.** `kgspGetBinArchiveBooterLoadUcode` has explicit
ucode variants only through AD102 (Ada) — GB20x falls through to a
NULL-stub shared fallback in the open module. Independently re-verified
later against the exact running driver version and a newer release — see
[attempt 02](../02-open-source-driver-recheck/README.md).

## 5b. Debug-image backdoor

**Objective:** use a debug-signed FMC as a backdoor, the way the old
test-key-37 debug booter worked on earlier architectures.

**Result: ❌ ruled out.** The debug FMC carries no test-key marker and
differs from the production image in 99.5% of its bytes — a structurally
different image, not just a different key. No equivalent backdoor exists.

## 5c. FMC key bruteforce

**Objective:** brute-force the FMC's encryption key via an MD5-IV
permutation family (a technique that worked for earlier key derivations).

**Result: ❌ ruled out.** A full 16! search across 2 containers × 5 modes
produced zero hits. The key most likely lives in hardware-protected SCP
secret storage, out of reach of this technique entirely.

## 5d. Other closed paths

**Objective:** a 13-entry table of other known write paths — direct GSP-RM
write, CUDA-debugger-class write, VBIOS reflash, fuse rewrite,
`SM_ISSUE_RATE_MODIFIER` direct manipulation, FECS doorbell, and others.

**Result: ❌ all ruled out.** Every one confirmed dropped, inert, or blocked
by independent testing before this project's own live-hardware phase began.

## Status

**❌ All four sub-items confirmed negative — 100% complete.** Useful mainly
as a record of what *not* to re-try, and why the Ampere/Ada technique family
doesn't transfer architecturally.

## See also

- [06 — vGPU/SR-IOV priv wall](../06-vgpu-sriov-priv-wall/README.md) (a related but distinct architectural wall)
- [17 — firmware signature oracle (parked)](../17-firmware-signature-oracle-parked/README.md)
