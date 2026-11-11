# 23 — Power-limit accept-vs-ignore canary

## Objective

mVolt+'s own documentation (see
[attempt 20](../20-mvolt-plus-precedent-study/README.md)) showed driver
621.01+ silently accepts-but-ignores live NVAPI voltage/OCP-limit writes —
"accepts the write, reports success, keeps the previous limit." Check
whether the much more mundane `nvidia-smi -pl` (general power-limit, watts)
reproduces that same accept-but-ignore shape, which would give this project
a reusable classifier: any future "write succeeded, nothing changed" result
could be sorted into "software-shape" vs. "PL3-silicon-shape" instead of
guessed at.

## Important correction made before running this

mVolt+'s documented accept-but-ignore behavior was specifically about
**NVVDD/MSVDD voltage-limit offsets, the OCP maximum, and the board-current
auxiliary limit** — their own guide states "everything else is unaffected,"
which includes the general power limit (watts) `nvidia-smi -pl` controls.
That's a long-standing, broadly-relied-on datacenter power-capping API, with
no existing reason to expect it's silently ignored. This was treated as a
genuinely open question going in, not an assumed-reproducible shape.

## Method

Under this project's standing voltage/power-limit guardrail (read back and
log the stock value first, one small bounded step via `nvidia-smi -pl`,
immediate revert, stop at any sign of instability): set a new power limit,
read it back, revert.

## Result

**Power-limit writes genuinely apply.** Confirmed via persistence-mode
-enabled readback: `nvidia-smi -pl 300` stuck at 300W, then was reverted to
the stock 360W cleanly. The first attempt, without persistence mode enabled,
falsely looked like accept-but-ignore — that was a **persistence-mode
artifact** (driver state resets between separate `nvidia-smi` invocations
without it), not the gating pattern being tested for.

This is a clean "applies correctly" result, not accept-but-ignore — general
power limits are not gated the way `SS0`/`SS1` (or, per the mVolt+
precedent, voltage/OCP limits specifically) are.

## Status

**❌ Confirmed negative (for the canary's intended use) — 100%.** Doesn't
deliver the calibration oracle originally hoped for, but is itself a clean,
useful, correctly-reported result — power limits and voltage/OCP limits are
gated differently on this hardware family, and that distinction is now
confirmed rather than assumed.

## See also

- [20 — mVolt+ precedent study](../20-mvolt-plus-precedent-study/README.md)
- [19 — voltage fault injection (parked)](../19-voltage-fault-injection-parked/README.md)
