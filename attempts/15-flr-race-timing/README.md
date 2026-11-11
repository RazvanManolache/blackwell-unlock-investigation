# 15 — FLR + BAR0 write race with the driver unbound

## Objective

Test whether `FEAT_OVR_PLM`'s re-close after an FLR (function-level reset)
is a pure hardware timer, or tied to GSP-RM's own boot progress — by
unbinding the driver, binding `vfio-pci`, triggering an FLR via sysfs, and
tight-looping BAR0 writes to `SS0`/`SS1` with GSP held down (no driver
reload).

## Hypothesis

If the reclose window stays open while GSP-RM is held down, that's evidence
the reclose is firmware-driven — a real, exploitable timing opening. If it
closes anyway, the reclose is hardware-auto, and this specific timing angle
is a dead end.

## Result

**The underlying question is substantially answered by evidence already in
hand, without building the actual VFIO tooling.** Two independent pieces of
evidence both point the same direction:

1. [Attempt 16](../16-gsp-free-cpu-rm-path/README.md)'s GSP-free boot
   (`NVreg_EnableGpuFirmware=0`) showed `FEAT_OVR_PLM` already closed with no
   GSP-RM running at all.
2. An independent researcher's hardware measurement (cited elsewhere in this
   project) already showed the reclose happens *before* GSP-RM's first PL2
   instruction runs.

Both point to **FSP's mandatory Chain-of-Trust sequence** as the actual
gate, independent of GSP-RM specifically.

The literal proposed method (building a `vfio-pci`-based BAR0 race tool) was
**not built** — new subsystem, real setup cost, marginal additional
confirmation at this point given the above.

## Status

**⚠️ Partial — 60%.** Left `in_progress` deliberately: the literal method
wasn't run, but the underlying question this test was designed to answer is
no longer genuinely open.

## See also

- [16 — GSP-free CPU-RM path](../16-gsp-free-cpu-rm-path/README.md)
- [01 — baseline capture + write-gate confirmation](../01-baseline-capture-and-write-gate/README.md)
