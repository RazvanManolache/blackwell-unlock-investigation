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

## Tools

- [`tools/persm_channel_probe.c`](tools/persm_channel_probe.c) — the full
  staged channel-build-and-probe tool from 12a.
- [`tools/persm_sweep_loop.c`](tools/persm_sweep_loop.c) — the extension
  used for 12b's sweep (partial reconstruction — see the file's own header
  comment for what's verbatim vs. reconstructed).

## See also

- [14 — RPC audit / undocumented commands](../14-rpc-audit-undocumented-commands/README.md) (the full-firmware decompile searches for the array's actual address)
- [01 — baseline capture + write-gate confirmation](../01-baseline-capture-and-write-gate/README.md)
