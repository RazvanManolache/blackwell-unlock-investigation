# 06 — vGPU/SR-IOV VF-PRIV-register angle

## Objective

Check whether NVIDIA's vGPU/SR-IOV virtual-function privilege registers
(`0x111xxx` range) offer a different route to privileged state than the
`FEAT_OVR_PLM` wall this project focused on directly.

## Hypothesis

A different register family, gated by a different mechanism, might not
share the same hardware wall.

## Result

**❌ Ruled out — confirmed hardware-fused-off.** The VF PRIV registers are a
different register target than the tensor-rate clamp, but hit the same
architectural wall: GSP owns MMIO directly, with no CPU-side interception
point available. This avenue's own proposed next step (an FPGA interposer
sitting physically between the CPU and the GPU's PCIe link) is out of scope
for anything this project's tooling can do — a fundamentally different,
much larger hardware-engineering undertaking, not a software or firmware
technique.

## Status

**❌ Confirmed negative — 100% complete**, for the software/firmware-level
question this project is actually equipped to pursue. The FPGA-interposer
idea is recorded as a theoretical alternative, not evaluated further.

## See also

- [bird/vgpu-unlock-blackwell](https://github.com/bird/vgpu-unlock-blackwell) — origin of this angle
