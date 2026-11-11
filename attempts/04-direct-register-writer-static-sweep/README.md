# 04 — Whole-firmware static sweep for direct SS0/SS1/PLM writer instructions

## Objective

Find out whether GSP-RM's own firmware contains code that directly writes
`SS0`/`SS1`, which would reopen the question of whether the speed cap is
software-written after all (and by extension, whether that code path could
be reached or redirected).

## Hypothesis

If such code exists and is reachable, the actual gate might be a software
check guarding a GSP-RM write, not a pure hardware wall — a meaningfully
different (and more hackable) shape of problem.

## Method

A whole-binary sweep of GSP-RM's firmware for `lui`-to-`0x823`/`0x824`
instruction pairs (i.e., any place code materializes an address in that
page as a compile-time constant) — 51 candidate instructions found and
manually triaged.

## Result

**4 direct, compile-time-fixed hits** on the exact named registers
(`0x823804`=PLM, `0x82381C`=SS0, `0x823820`=SS1, `0x823808`=PLM+4). An
independent researcher's measurement on real GB202 hardware names these
exact same addresses — strong cross-validation that the sweep found real
registers, not noise. The `PLM+4` (`0x823808`) site was also resolved: it's
`FEAT_OVR_QUADRO`, a classification/binning field read, not a rate control.

**The critical remaining question — who calls the SS0/SS1-writer — became
less important, not resolved**, once the PL3-only write-mask was
independently confirmed on real GB202 silicon (and later, directly on this
desktop die in [attempt 01](../01-baseline-capture-and-write-gate/README.md)):
even a confirmed GSP-RM caller of this code would itself be a no-op, since
GSP-RM runs at PL2 and the mask blocks both PL0 and PL2 writes.

## Status

**⚠️ Partial — 70% complete.** The static existence of writer code is
confirmed and cross-validated; the caller question was downgraded from
"reopens the core question" to "very likely pre-empted by the independently
confirmed hardware wall," not formally re-traced to a dead end on this die
specifically. Four exhausted static caller-search techniques were tried and
are not worth re-attempting.

## See also

- [01 — baseline capture + write-gate confirmation](../01-baseline-capture-and-write-gate/README.md)
- [14 — RPC audit / undocumented commands](../14-rpc-audit-undocumented-commands/README.md) (the full-firmware decompile this sweep was an early, narrower version of)
