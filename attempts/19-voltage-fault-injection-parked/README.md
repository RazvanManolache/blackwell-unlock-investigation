# 19 — Voltage/clock-margin fault injection (parked)

## Objective

Time a voltage or clock-margin fault injection against the PL3 privilege
check, attempting to *skip* the gate momentarily via a hardware glitch
rather than defeat it logically — a classic technique in the SoC/console
jailbreak world.

## Why this is parked, not attempted

This is genuine fault injection against the GPU owner's own desktop
hardware — real risk of a hang, a corrupted boot state, or silicon damage,
with no established recovery path tested anywhere in this project. This
project's safety policy already excludes fault-injection techniques
generally, and that exclusion stands unless the owner decides otherwise for
this specific case.

**Explicit standing guardrail from the GPU owner** (recorded verbatim
because it governs this item specifically): *"if we play with voltages make
sure the agent doesn't do anything stupid."* This item is the opposite of a
bounded, vendor-validated adjustment — it is deliberately out-of-spec
glitching, exactly the "something stupid" the instruction is about. If this
is ever approved, it does not get the light-touch treatment given to other
live-hardware items in this project: it would need a hard-bounded margin
range defined first, a watchdog/revert path, and stopping at the first sign
of instability rather than widening the margin to find a limit.

## Status

**🚫 Parked, not attempted — 0%.** Recorded here only so the idea isn't lost
or re-proposed as if it were low-risk.

## See also

- [20 — mVolt+ precedent study](../20-mvolt-plus-precedent-study/README.md) (a related but categorically different, bounded voltage-adjacent study that *was* performed)
- [23 — power-limit canary](../23-power-limit-canary/README.md)
