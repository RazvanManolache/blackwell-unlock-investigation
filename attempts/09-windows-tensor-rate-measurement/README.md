# 09 — First direct tensor issue-rate measurement on desktop GB203

## Objective

Get a first-ever direct tensor-core issue-rate measurement on a desktop
GB203 (this project's actual RTX 5080), confirming the FP32-accumulate
throughput cap exists on this exact die, using nothing but ordinary CUDA
compute — no register reads, no driver/kernel tricks, same risk class as any
GPU compute workload, runnable directly on the user's live Windows desktop.

## Hypothesis

If the tensor-rate clamp is real and architectural (not specific to the one
GB202 die it was first measured on), the same roughly-2x throughput gap
between FP32-accumulate and FP16-accumulate tensor paths should show up here
too, at a comparable per-SM rate.

## Method

Ported the device kernels (verbatim — same `mma.sync` instruction sequences
and inputs, no modification) from an independent researcher's GB202
reference tool, and compiled them with a straight `nvcc` build (no
NVRTC/driver-API plumbing, no cubin-hash pinning — this is pure device code,
simpler than the original Linux tool's setup). Measured best-of-3 timing
across several tensor-math modes.

## Result

Measured on this exact RTX 5080 (GB203, `10de:2c02`, 84 SMs, driver 621.01):

```
FP32-accumulate path:  f16->f32  114.45 TF/s
                       bf16->f32 114.50 TF/s
FP16-accumulate path:  f16->f16  244.71 TF/s
```

A **2.14x gap** — matching the capping signature independently measured on
GB202 (227.1 vs. 441.4 TF/s = 1.94x gap). Per-SM throughput matches almost
exactly: 1.362 TF/s/SM here vs. 1.336 TF/s/SM on the GB202 reference —
same silicon generation, same clock-for-clock rate, same cap mechanism.

This is the first public data point confirming the tensor-rate cap applies
to desktop GB203 specifically, not just GB202 — independently corroborating
the `FEAT_OVR_SM_SS0`/`SS1` mechanism's real-world effect on this die, ahead
of (and independent from) the register-level baseline capture.

Caveat: this is a quick 3-rep timing run, not the fully validated,
per-cell-checked artifact the original Linux tool produces. Treat it as a
strong directional confirmation, not a publication-grade measurement.

## Status

**❌ Confirmed (the clamp is real on this die) — 100% complete.** This is a
confirmation of the problem, not a step toward solving it — included here
because it's part of this project's evidence base, not because it's an
unlock attempt.

## Tools

- [`tools/kernels.cu`](tools/kernels.cu) — the device-side tensor-math
  kernels, ported verbatim from the GB202 reference tool.
- [`tools/host.cu`](tools/host.cu) — the Windows-side host runner (timing
  harness, sanity checks, JSON output).

## See also

- [01 — baseline capture + write-gate confirmation](../01-baseline-capture-and-write-gate/README.md)
- [shuttie/bf16-unlock-blackwell-research](https://github.com/shuttie/bf16-unlock-blackwell-research) — origin of the reference kernels
