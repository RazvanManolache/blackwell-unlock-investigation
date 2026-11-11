# 08 — Windows-side NVAPI/ioctl passthrough feasibility study

## Objective

Investigate whether a documented, public, GET-only RM call
(`NV2080_CTRL_CMD_GR_GET_SM_ISSUE_RATE_MODIFIER`, with a working Linux
reference implementation) could be reached directly from the target
machine's live Windows desktop — a zero-risk, no-Raspberry-Pi-needed
shortcut to the same read this project was otherwise doing from a
disposable Linux boot.

## Hypothesis

If NVAPI (NVIDIA's public Windows API) exposes any kind of generic
"arbitrary RM command + buffer" passthrough, the same read-only measurement
could be done with zero setup, directly on the user's daily-driver OS.

## Method

Web research into NVAPI internals, structure, and how other tools interact
with it.

## Result

**❌ Ruled out — not viable.** `nvapi64.dll` exports only
`NvAPI_QueryInterface(numeric_id) -> function pointer`; every known private
function (fan control, pstate, voltage, I2C, etc.) that anyone has ever
documented was found by someone tracing a real tool's behavior in a system
API monitor, one numeric ID at a time. No generic "arbitrary RM command +
buffer" passthrough exists in NVAPI or any of its known open-source
wrappers. Finding the Windows kernel driver's own IOCTL code for
`RM_CONTROL` (the Linux equivalent, `NV_ESC_RM_CONTROL = 0x2A`, is public;
the Windows one is not) would mean reverse-engineering it directly against
the user's live driver — which also sits in a gray area against NVIDIA's
NVAPI SDK license (it has a no-reverse-engineering clause).

## Status

**❌ Confirmed negative — 100% complete.** Not worth re-proposing as a quick
win. The Pi harness reads the same registers over a known, already-reviewed
protocol with zero risk to the user's live Windows install, which is why
this project continued to rely on the disposable-Linux-boot approach
instead.

## See also

- [09 — Windows tensor-rate measurement](../09-windows-tensor-rate-measurement/README.md) (a different Windows-side measurement that *did* work — pure CUDA compute, no driver tricks)
