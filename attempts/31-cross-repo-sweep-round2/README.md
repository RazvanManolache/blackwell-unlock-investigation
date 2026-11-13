# 31 — Cross-repo sweep, round 2

A later check of tracked and newly-surfaced public repositories for updates
or new angles, done after the original prior-art table (root README) was
first assembled.

## 31a. Independent corroboration on a third die, via a fourth tool

**Objective:** check whether any tracked or newly-surfaced public project
has made progress on this exact mechanism since this project's own
research began.

**Result: no breakthrough anywhere, but one genuinely useful
corroboration.** The most relied-on tracked repositories showed no new
commits beyond what this project already incorporated. One real find: an
independent researcher (already credited in this project's prior-art table
for a GB206 card) posted directly to a *different* maintainer's issue
tracker — a Chinese-language UEFI-based unlock utility, also already
tracked in this project — with a GB206 (RTX 5060 Ti) read-only register
capture and a live write-ladder attempt (the same `SS0`/`SS1`/PLM values
this project's own attempts always target). **All three writes were
silently rejected, readback unchanged** — "same silent drop" as every
other die this project or its prior art has measured, now independently
reproduced on a **third physical die, through a fourth independent tool**
entirely separate from this project's own tooling or that researcher's own
primary repository. That maintainer's own response: they fixed an
unrelated *read-only reporting* gap for Blackwell dies, but explicitly kept
write/"transaction support" for Blackwell out of scope — consistent with
every other tracked maintainer's position on this exact mechanism.

**Status: 🔬 Corroboration only, no new technical content — 100%
complete as a check.** Doesn't change this project's model in any way;
strengthens confidence in the write-gate finding via a fully independent
measurement path.

## 31b. Four additional repositories checked

Checked on request against the question "does any of this help understand
the card better, or suggest a new avenue":

- **An actively-maintained instruction-set encoding reference** covering
  both datacenter and consumer Blackwell SM variants — useful tooling (this
  project's own [empirical benchmark attempt](../26-sm120-empirical-benchmark/README.md)
  drew on its consumer-variant reference tables to confirm which tensor-core
  instruction shapes this die's architecture actually supports), not a new
  unlock lead. Had one new upstream commit at the time of checking, adding
  more datacenter-variant instruction-encoding tables — not relevant to the
  consumer-die cap this project targets.
- **A page describing a paid, structured course on CUDA kernel
  optimization** — on inspection, this is a marketing/syllabus page for a
  training product, not a research repository. No code, no Blackwell-unlock
  content of any kind. Not useful for this project.
- **A real, well-built tensor-core microbenchmark suite** — but hard-targeted
  at the *datacenter* Blackwell architecture (a different tensor-core
  generation with dedicated tensor-memory instructions consumer Blackwell
  does not have) and will not compile for this die's architecture at all.
  Used only as a loose structural/methodology reference (warmup discipline,
  cycle-counted timing loop) for this project's own
  [attempt 26](../26-sm120-empirical-benchmark/README.md) — not reused
  directly, and not a lead in itself.
- **Two vGPU/SR-IOV "unlock" repositories** — both already covered by
  [attempt 06](../06-vgpu-sriov-priv-wall/README.md): a different NVIDIA
  feature gate (virtualization licensing, not the tensor-rate cap), hitting
  a different but architecturally analogous hardware wall. Confirmed no new
  content in either beyond what attempt 06 already covers; one of the two
  turned out to be a byte-identical mirror of the other, published under a
  different account.

**Status: 🔬 Reconnaissance only — 100% complete as a check.** No new
avenue, no new technical content beyond what's already folded into attempts
06 and 26.

## See also

- [06 — vGPU/SR-IOV VF-PRIV wall](../06-vgpu-sriov-priv-wall/README.md)
- [26 — Empirical SASS-level tensor-rate measurement](../26-sm120-empirical-benchmark/README.md)
- Root [README](../../README.md) prior-art table
