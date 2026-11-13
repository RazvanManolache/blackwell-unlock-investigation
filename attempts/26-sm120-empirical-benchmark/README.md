# 26 — Empirical SASS-level tensor-rate measurement (SM120)

## Objective

Every prior measurement of the Blackwell tensor-rate cap in this project —
the RM getter (`NV2080_CTRL_CMD_GR_GET_SM_ISSUE_RATE_MODIFIER`),
`read_sm_rates.py`, the NVAPI ordinal survey, raw BAR0 register dumps — goes
through an NVIDIA-owned reporting path. None of it independently confirms
that real tensor-core execution on this die is actually throttled by the
reported amount, as opposed to the driver merely *reporting* a cap it
doesn't enforce. This attempt closes that gap with a directly compiled and
executed measurement.

## Hypothesis

If the cap is real hardware/firmware enforcement (not just a status field),
a hand-written CUDA kernel issuing real tensor-core instructions back to
back should show a substantially lower throughput than the architecture's
documented uncapped ratio over scalar FP32 — independent of anything the
driver reports.

## Method

Two kernels, same warp/occupancy structure, targeting `sm_120a` (this card's
real architecture — consumer Blackwell does **not** have the datacenter
UMMA/`tcgen05`/TMEM tensor-core path; a third-party benchmark suite surveyed
earlier in this project targets exactly that datacenter path and will not
compile for this GPU at all, so it was used only as a loose methodology
reference, not reused directly):

- `sm120_mma_tput.cu` — 2688 warps (84 SMs × 4 blocks/SM × 8 warps/block,
  matching this card's real SM count) each issue 200,000 iterations of 8
  independent-accumulator-chain `mma.sync.aligned.m16n8k16.row.col.f32.bf16.bf16.f32`
  instructions (the standard PTX tensor-core op, forward-compatible since
  `sm_80`). 8 independent chains hide MMA pipeline latency so the
  measurement reflects sustained throughput, not serial latency.
- `sm120_fp32_fma_tput.cu` — a same-methodology scalar FP32 reference
  kernel, included as an apples-to-apples baseline for this specific
  harness (this path isn't gated by the `FEAT_OVR` speed-selector bank at
  all).

Compiled `nvcc -O3 -arch=sm_120a` and verified via `cuobjdump --dump-sass`
that the compiled binary contains the intended real instructions — not an
optimized-away loop.

### A real bug found and fixed along the way

The first pass of the scalar FP32 reference kernel used identical
compile-time-constant inputs across every thread in a warp. `ptxas` proved
the whole computation warp-uniform and silently moved it onto the
**uniform datapath** — a real, documented Turing-and-later hardware
feature: a single scalar ALU per SM-partition reserved for warp-uniform
arithmetic, entirely separate from the 128 real per-thread FP32 CUDA cores
per SM. The SASS dump showed `UFFMA` on `UR*` (uniform) registers
throughout the inner loop, not `FFMA` on general-purpose per-thread
registers — a real, honestly-measured number, just of the wrong hardware
unit entirely.

**Fix:** the reference kernel's inputs were changed to derive from
`threadIdx`, denying the compiler the uniformity proof it needs to make
that optimization. Re-verified via SASS: clean per-thread `FFMA` on general
registers, 0 spills. An occupancy check (67% → 100%) changed throughput
<2%, confirming the kernel is genuinely instruction-issue-bound rather than
occupancy-starved.

This is a generally useful trap worth flagging for any future
microbenchmark on this hardware: a reference kernel with identical,
data-independent inputs across threads is at risk of the same silent
uniform-datapath hoist.

## Result

**Measured on the real RTX 5080 (GB203, `10de:2c02`), production driver,
SM clock 2730MHz, corrected kernel:**

```
BF16->FP32 tensor throughput:  ~123.16 TFLOPS  (3 runs, 122.9-123.3)
Scalar FP32 FMA reference:     ~42.25 TFLOPS   (5 runs, 41.0-42.3;
                                                 ~72% of card's public spec)
Measured ratio:                ~2.92x
```

**Cross-validated on a second, completely independent operating
system/toolchain on the same physical card.** The exact same two source
files were compiled and run unchanged under a disposable Linux live boot
(CUDA 13.1 installed fresh via NVIDIA's own official apt repository —
confirmed via a dry run first that this pulls in zero kernel-driver
packages, only userspace toolkit/libraries, so the carefully-configured
driver setup was never touched). SASS re-verified clean on both kernels
under the Linux toolchain too (no `UFFMA` fallback on the fixed reference
kernel).

```
Tensor (BF16->FP32):  ~119.44 TFLOPS  (3 runs, 119.42-119.47)
FP32 FMA (scalar):    ~40.88 TFLOPS   (5 runs, 40.87-40.90)
Ratio:                2.922x
```

Both absolute numbers are ~3% lower on Linux than on Windows —
unremarkable, consistent with ordinary run-to-run clock/thermal/driver-build
variance between two different boots and operating systems, not a
systematic skew. **The ratio itself matches to three significant figures
(2.922 vs. 2.92) across two completely different operating systems,
compilers, and driver builds.** This was the explicit cross-check this
attempt was designed to pass: nothing about the Windows/MSVC toolchain or
driver stack was itself producing the measured gap.

**Interpretation.** Against this project's existing architectural
reference point — an uncapped dense BF16 tensor path on Ampere-class-or-later
hardware (this instruction shape, FP32 accumulate) runs roughly 16x the
scalar FP32 rate per SM — a measured ~2.92x gap implies this die is
currently achieving roughly **1/5.5 of its full tensor rate**. That's
materially closer to the public `ctrl2080gr.h` enum's "1/8 rate" reading
(the `fmla32=3` value this project has measured repeatedly via the RM
getter) than to a competing "~1.79x gap"/half-rate figure measured
elsewhere in this project on a *different* die (RTX 5090/GB202). Neither
is an exact match.

**What this does and doesn't resolve.** This is the first independent,
driver-API-free confirmation that the cap is real *execution* behavior on
this exact die, not just metadata the driver reports without enforcing —
closing the "maybe nothing is actually enforced" possibility for good. It
is **not** a precision measurement of the exact rate fraction: the 16x
architectural constant used for interpretation isn't independently
re-derived here, and this remains a same-card tensor-vs-scalar comparison,
not a capped-vs-uncapped same-die comparison — there is still no
known-uncapped GB203 sample available to calibrate against directly, which
is what would be needed to pin the exact fraction rather than narrow it.

## Cross-OS validation (2026-11-12)

The obvious objection to a single-toolchain measurement: is this a
Windows/MSVC/driver-build artifact rather than a real property of the
hardware? Replicated the exact same two unmodified source files on a
disposable Linux live-boot of the same physical machine, specifically to
check that.

Ubuntu's own bundled CUDA package predates Blackwell support entirely and
wouldn't compile `sm_120a` at all — installed the real CUDA 13.1 toolkit
from NVIDIA's own official package repository instead (confirmed, via a
dry run first, that this pulls in zero kernel-driver packages — it doesn't
touch the GPU driver setup at all), landing on the exact same toolchain
version Windows used. Re-verified SASS cleanliness with the same
discipline as the original run (real per-thread `FFMA`, no uniform-
datapath shortcut; real back-to-back tensor-core `HMMA`).

**Measured:** tensor ~119.44 TFLOPS, scalar FP32 ~40.88 TFLOPS, ratio
**2.922x** — against Windows's 123.16 / 42.25 / **2.92x**. The two absolute
numbers are each about 3% lower on Linux, well within ordinary run-to-run
clock/thermal/driver-build variance between two different boots and
operating systems. **The ratio itself agrees to three significant figures
across two completely different operating systems, compilers, and driver
builds.**

## Status

**🔬 Methodology fully validated and independently cross-OS confirmed,
cap confirmed real at execution level — 100% complete as a confirmation
test; the exact rate fraction remains open.** (Not ✅ — this icon in this
repo's legend is reserved specifically for the strict `SS0==0x88888888 &&
SS1==0x8` unlock criterion, which this attempt never tests; it measures
the *magnitude* of the already-confirmed cap, not whether it was bypassed.) Not a new unlock lead — a
new, now twice-independently-validated piece of evidence that the
already-known cap genuinely throttles real tensor-core execution on this
hardware regardless of software stack, with the measured gap favoring the
public "1/8 rate" reading over the competing "half rate" figure without
nailing it exactly.

## Tools

- [`tools/sm120_mma_tput.cu`](tools/sm120_mma_tput.cu) — the BF16 tensor-core
  throughput kernel.
- [`tools/sm120_fp32_fma_tput_v2.cu`](tools/sm120_fp32_fma_tput_v2.cu) — the
  corrected scalar FP32 reference kernel (the original, uniform-datapath
  version is kept alongside it for history, clearly flagged as superseded
  for ratio purposes).

## See also

- [09 — Windows-side tensor rate measurement](../09-windows-tensor-rate-measurement/README.md) (an earlier, driver-API-based measurement on this same die)
- [30 — RTX PRO 6000 fuse bit-diff and key fingerprint](../30-pro6000-fuse-bitdiff-and-key-fingerprint/README.md) (the exact fused bits this cap's reported value corresponds to)
