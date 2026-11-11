# 16 — Force the legacy GSP-free CPU-RM path

## Objective

NVIDIA's open driver supports an older code path that doesn't use GSP-RM
firmware at all (`NVreg_EnableGpuFirmware=0`). Retest every write/read
result from this entire project in that mode: a completely different,
fully-readable code path with different internal gates.

## Hypothesis

This project's own checklist expected this to most likely fail outright on
modern Blackwell silicon ("card may not boot without GSP; clean negative if
so").

## Method

```bash
sudo rmmod nvidia_uvm nvidia
echo 1 | sudo tee /sys/bus/pci/devices/0000:01:00.0/reset
sudo modprobe nvidia 'NVreg_EnableGpuFirmware=0'
```

## Result

**It booted and fully worked.** Confirmed genuinely GSP-free (not just a
silently-ignored request) two ways: `EnableGpuFirmware: 0` in
`/proc/driver/nvidia/params`, and — more convincingly — **zero occurrences
of the word "gsp" or "firmware" anywhere in `dmesg`** for this entire boot,
a stark contrast to every other boot in this project.

Ran the full existing toolchain against this mode:

- The same `regops wrr` write-then-restore test on `SS0`:
  ```
  WRITE off=0x82381c value=0x88888888 (write)
  READBACK off=0x82381c value=0x66212052   <- unchanged, silently dropped
  ```
  Byte-identical to every GSP-RM-mode result.
- The full `persm_channel_probe` staged build (see
  [attempt 12](../12-per-sm-register-channel/README.md)), including a real,
  completed GPU engine submission, worked end-to-end with GSP-RM completely
  absent. The `GR_CTX_SM` sweep still returned the same plain
  `INVALID_ARGUMENT`.

**This is the single cleanest piece of evidence this project produced that
`FEAT_OVR_PLM` is a pure hardware gate.** Two architecturally unrelated
software implementations — the closed GSP-RM RISC-V firmware, and the fully
open-source legacy CPU-RM path — produce byte-identical results for both the
write-drop and the array-read failure. Neither software stack is causing
either behavior; both are downstream of the same fixed silicon state.

As a side effect of this mode, it was noted (not separately tested) that
forcing non-GSP-client mode would also reopen the `kfspCheckForClockBoostCapability_HAL`
path normally skipped via `!IS_GSP_CLIENT` (see
[attempt 13c](../13-suspend-resume-register-rewrite/README.md#13c-mailbox-tamper)) —
worth keeping in mind if this mode is ever revisited for a different
purpose.

Reverted cleanly to stock GSP-RM mode afterward.

## Status

**❌ Confirmed negative (for the unlock question) — 100% complete.** The
single strongest piece of evidence in this project that the clamp is
silicon-level, not software-level.

## See also

- [01 — baseline capture + write-gate confirmation](../01-baseline-capture-and-write-gate/README.md)
- [15 — FLR race timing](../15-flr-race-timing/README.md)
- [12 — per-SM register channel](../12-per-sm-register-channel/README.md)
