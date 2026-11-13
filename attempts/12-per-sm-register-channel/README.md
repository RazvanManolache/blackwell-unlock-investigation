# 12 — Per-SM register channel: building the infrastructure, then the sweep

## 12a. Channel build

## Objective

Everything measured up to this point (the global FEAT block, `SS0`/`SS1`,
`PLM`) is a set of singular, global registers. There's a separate,
genuinely unexplored target: **per-SM shadow copies** of the speed
selector, reachable (per NVIDIA's own documentation for
`NV2080_CTRL_GPU_EXEC_REG_OPS`) only through a `GR_CTX_SM`-routed register
read, which needs a live, scheduled GPU channel/context to resolve against.
No such channel existed anywhere in this project before this attempt.

## Hypothesis

If the per-SM copies are reachable via a real channel and turn out to be
writable (even if the global copies aren't), that's a different, unexplored
lever.

## Method

Built `persm_channel_probe.c` from scratch: a staged tool
(client → device → subdevice → vaspace → GPU memory via UVM external range
+ DMA map → channel group + context share + GPFIFO channel
(`BLACKWELL_CHANNEL_GPFIFO_A`) + compute object (`BLACKWELL_COMPUTE_B`) →
UVM channel registration + `GPFIFO_SCHEDULE`). Uses NVIDIA's own real
headers directly (unlike the simpler `regops.c`, which hand-rolls just the
few simple fields it needs) since channel/UVM/VA-space objects have real
memory-safety stakes if a struct layout is guessed wrong.

Three real bugs were found and fixed by careful re-reading of the actual
kernel source before ever running the broken version on hardware: a missing
`UVM_MM_INITIALIZE` call, a 2-bit TYPE enum in
`NV2080_CTRL_GR_ROUTE_INFO.flags` initially treated as a bitmask, and a
CPU-memory-mapping helper opening the wrong device node for
`NV01_MEMORY_SYSTEM` allocations.

## Result

**All four build stages ran clean on real hardware**, GPU health checked
after each (29°C idle throughout, zero Xid, clean RM auto-teardown on every
exit). Stage 5 (the `GR_CTX_SM` probe on the live-but-idle channel) still
returned `INVALID_ARGUMENT`, same as with no channel at all.

**Stage 6: this project's first real completed GPU engine submission.**
Submitted a real semaphore-release method through the now-live channel
(two more real bugs found and fixed first — a dropped method word, and the
same wrong-device-node issue recurring for the pushbuffer buffer). It
worked completely: ring entry written, doorbell rung, marker became the
exact expected value (`0x16c0ffee`), confirming genuine hardware
completion.

**The `GR_CTX_SM` probe immediately after returned the identical
rejection** — this rules out, by direct experiment, the hypothesis that
GSP-RM needs the channel's context "promoted" by real submitted work first.

**Why it actually fails, found by reading the real docs rather than
guessing further:** `regGroupMask`/`regSubGroupMask` (left at 0 throughout)
are not optional — they select a TPC/SM for an **array read**, and
`regOffset` "must be the first register in an array." The four addresses
this whole investigation used are **singular global registers, not array
elements** — a fundamentally mismatched request regardless of channel
state.

**Net result: the stated blocker (no caller-owned channel) is fully
resolved** with real, hardware-proven, reusable infrastructure — including
this project's first successful real GPU engine submission. But the actual
per-SM register array's base address remains unknown and is not the same
as the global FEAT block offset.

**Status: ⚠️ Partial — 80%.** The infrastructure problem is completely
solved and reusable; the actual research question (where is the array) is
still open.

## 12b. Offset sweep

## Objective

Rule out "group/subgroup index 0 just happened to be out of range" as the
explanation for 12a's rejection, via a bounded brute-force sweep, in
parallel with the static-analysis search for the array's real base address.

## Method

Swept `regGroupMask`/`regSubGroupMask` 0–7 × 0–3 (64 combinations) for
`FEAT_OVR_SM_SS0`/`SS1` via the proven real channel, both before and after a
completed real GPU submission.

## Result

**All 128 calls (64 combos × 2 registers, before and after) returned
identical plain `INVALID_ARGUMENT`.** Rules out the "just an out-of-range
index" explanation; strengthens the standing theory that these two
registers really are singular, non-array global registers — the real
per-SM array, if one exists, lives at a different base address this
sweep didn't find.

**Status: ❌ Confirmed negative — 100%.** A real, useful negative result:
the array base address is a genuinely separate unknown, not findable by
further brute-forcing at these specific addresses.

## 12c. Ghidra data-flow trace for the array's base address (update 2026-11-12/13)

**Objective:** 12a/12b ruled out brute-forcing the array at the wrong base
address; separate rounds of corpus-wide textual/regex search for the
addressing computation's *shape* (`base + group*stride + subgroup*stride`)
had already come up empty. The honestly-scoped next step: real Ghidra
data-flow/cross-reference analysis starting from the actual `GR_CTX_SM`
regType constant, not textual pattern matching.

**Method:** queried the saved, fully-analyzed Ghidra project directly
(not a flat-text grep) for references to the `GR_CTX_SM` constant and for
the real entry point of the function believed to be the client-facing
`EXEC_REG_OPS` RPC handler.

**Result — a real methodology correction, then a sharper wall:**

1. `regType` is a **bitmask**, not a sequential enum — every prior
   switch/case-shaped search in this project's history was looking for the
   wrong code pattern entirely.
2. The function's real entry point is a different address than the one
   this project had been citing as "the `EXEC_REG_OPS` handler" since an
   earlier round — the old address was a point mid-function, not the real
   boundary, which is why prior reference lookups silently missed it.
3. That function makes 44 calls. **All 44 are indirect** (`jalr` through a
   register, zero direct `jal`); only 3 resolve to anything nameable at
   all, and those 3 are generic logging/assert infrastructure, not
   per-regType dispatch. The remaining ~41 are genuinely unresolvable by
   static analysis as found.
4. **A further round surfaced a real open discrepancy, reported honestly
   rather than papered over:** a fresh reference search for the address
   that originally anchored this function's identification as `EXEC_REG_OPS`
   found *zero* references to it anywhere in the firmware, and the one
   place that literal value appears in the raw binary sits at a
   non-instruction-boundary offset (a coincidental byte match, not a real
   pointer). Net: either this function isn't actually the client-facing
   `EXEC_REG_OPS` handler and the real one hasn't been found yet, or it is,
   and the real call path reaches it only through a genuinely indirect
   object-vtable call a plain reference search can't follow. A follow-up
   attempt at manually matching the vtable installer for that call pattern
   found ~220 corpus-wide hits for the relevant offset — too noisy to
   narrow without a much larger manual effort, not completed.

**This sharpens exactly why static analysis can't find the base address**
(a specific, now-identified class of unresolvable indirect dispatch, plus
an unresolved identification question on top of it) rather than leaving it
a vague, open-ended mystery. The real next step needs live execution
tracing — a tool this project does not have.

**Status: ⚠️ Partial / sharpened dead end — 90%.** Infrastructure and
methodology are now as far as static analysis alone can take this; the
array's base address itself remains unknown.

## Tools

- [`tools/persm_channel_probe.c`](tools/persm_channel_probe.c) — the full
  staged channel-build-and-probe tool from 12a.
- [`tools/persm_sweep_loop.c`](tools/persm_sweep_loop.c) — the extension
  used for 12b's sweep (partial reconstruction — see the file's own header
  comment for what's verbatim vs. reconstructed).

## See also

- [14 — RPC audit / undocumented commands](../14-rpc-audit-undocumented-commands/README.md) (the full-firmware decompile searches for the array's actual address)
- [01 — baseline capture + write-gate confirmation](../01-baseline-capture-and-write-gate/README.md)
