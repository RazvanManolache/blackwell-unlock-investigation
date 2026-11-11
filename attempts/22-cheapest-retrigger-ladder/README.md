# 22 — Cheapest retrigger ladder

## Objective

Find the cheapest event that re-fires the SmSpeedSelect registry read (and,
by extension, every registry-key test in this project): `nvidia-smi
--gpu-reset` vs. module reload vs. suspend/resume vs. a full cold reboot.

## Method (planned, not yet run)

Using the demand-logger build (see
[attempt 10b](../10-registry-override-smspeedselect/README.md#10b-demand-logger))
as the did-the-read-fire oracle, test each re-init event in order of
expected cost, cheapest first. A `gpu-reset` not re-running GSP init at all
is itself an expected, useful negative — log it and move to the next rung
rather than treating it as a failure.

Whichever event turns out to be the cheapest one that genuinely re-fires the
read becomes the standard iteration loop for every registry-key test this
project runs — directly feeding
[attempt 13e](../13-suspend-resume-register-rewrite/README.md#13e-fast-retrigger)'s
"use resume instead of reboot" idea, generalized to whatever the actual
cheapest event turns out to be.

## Status

**⏸️ Not started — 0%.** Disposable Linux only; no new risk, just unmeasured
convenience.

## See also

- [10 — registry override: SmSpeedSelect](../10-registry-override-smspeedselect/README.md)
- [13 — suspend/resume register rewrite](../13-suspend-resume-register-rewrite/README.md#13e-fast-retrigger)
