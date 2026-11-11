# 01 — Baseline register capture + PL3 write-gate confirmation

## Objective

Get the first clean, read-only register capture ever taken on a **desktop**
GB203 (RTX 5080, `10de:2c02`) — no prior public data point existed for this
specific die, only for GB203M (laptop), GB202 (RTX 5090), and GB206 (RTX
5060 Ti) — and confirm whether the hardware write-gate (`FEAT_OVR_PLM`)
behaves the same way here as on those other dies.

## Hypothesis

Blackwell's FEAT block architecture (register layout, fuse values, write
mask) is shared across the whole GB20x family, so this desktop die should be
byte-identical to the already-measured dies. Any divergence would be new
information worth flagging explicitly, not silently folded into "same as
usual."

## Method

Read-only BAR0 dump via `regops bar` mode (a `PROT_READ` mmap of
`/sys/bus/pci/devices/<bdf>/resource0`, zero driver/GSP involvement) over:

- The entire FEAT block (`0x823800`–`0x82382C`)
- All nine rate fuses (`0x8207xx`/`0x82059C`/`0x820224`)
- Both override-gate fuses (`0x8203F0`, `0x820040`)
- `FUSE_SS_PLM` (`0x8200FC`)
- `SM_ISSUE_RATE_MODIFIER` (`0x504204`)
- `SEC2_CPUCTL`/`PLM` (`0x840100`/`0x8403C4`)

Then a getter-only RM call, `NV2080_CTRL_CMD_GR_GET_SM_ISSUE_RATE_MODIFIER`
(`0x20801230`), via `read_sm_rates.py`.

Then **the one write test in this phase**, via `regops wrr` mode (write,
read back, restore, read back — all in one RPC batch): write the actual
unlock values to `SS0`/`SS1` and see what sticks.

## Result

**Byte-identical to the GB202/GB203M/GB206 baseline everywhere that
matters.** No divergence found on this die.

```
FEAT_OVR_PLM (0x823804)    = 0xfffffb8f   (stock PL3-only write mask)
FEAT_READOUT_1 (0x823818)  = 0x00c36180
FEAT_OVR_ECC (0x82380c)    = 0x00888888
all nine rate fuses        = match (FMLA32/IMLA1/IMLA2/IMLA4=3, rest 0)
FUSE_SS_PLM (0x8200fc)     = 0xfffffbff   (stock re-locked)
PMC_BOOT_0                 = 0x1b3000a1
SM_ISSUE_RATE_MODIFIER getter: imla0=0 fmla16=0 dp=0 fmla32=3 ffma=0
                                imla1=3 imla2=3 imla3=0 imla4=3
```

The getter result is byte-for-byte identical to an independent researcher's
measurement on real GB202 hardware (RTX 5090 / RTX PRO 6000).

**The write test:**

```
SS0 write 0x88888888 -> readback 0x66212052   (UNCHANGED, status SUCCESS)
SS1 write 0x8        -> readback 0x00017001   (UNCHANGED, status SUCCESS)
```

The write is accepted by the RPC layer (`status = SUCCESS`) but silently
dropped by the hardware itself — an independent BAR0 readback confirms both
registers unchanged. GPU stayed healthy throughout, zero Xid.

### 1b. Pre-bind check

A separate, narrower question: is the gate already closed by the time any
host-visible code gets control at all, or only after the OS driver binds?
A previously-cited (then-unverified) community probe on GB203M found
`FEAT_OVR_PLM` already closed pre-bind. This session's capture answers it
directly for the desktop die too, for the same reason the full baseline
does: the PLM read (and the write-and-readback test above) happened as part
of the very first capture this project ever took on this hardware, at the
earliest point this project's tooling can reach. Same result: already
closed, same mask, same silent-drop behavior.

## Status

**❌ Confirmed negative — 100% complete.** The write-gate holds on this die
exactly as measured elsewhere. This result is the foundation every other
attempt in this repository either builds on or is measured against.

## Tools

- [`tools/regops.c`](tools/regops.c) — the register read/write/probe tool
  used throughout this whole project, by [shuttie](https://github.com/shuttie/bf16-unlock-blackwell-research)
  (MIT-licensed, from `bf16-unlock-blackwell-research/tools/register-access/`).
  Build: `gcc -O2 -Wall -o regops regops.c`.
- [`tools/read_sm_rates.py`](tools/read_sm_rates.py) — the public,
  read-only RM getter client (GPL-2.0-only, from
  [JustAResearcher/gpu-unlock-ada](https://github.com/JustAResearcher/gpu-unlock-ada)'s
  `tools/` directory).
- [`tools/verdict.py`](tools/verdict.py) — this project's own mechanical
  unlock-criterion checker, used on every capture from this point on.
- [`tools/checklist_util.py`](tools/checklist_util.py) — shared
  progress-tracking logic for the project's tracked checklist.
- [`tools/baseline_registers.json`](tools/baseline_registers.json) — the
  cross-die reference values this capture was compared against.

## See also

- [04 — direct register-writer static sweep](../04-direct-register-writer-static-sweep/README.md)
- [16 — GSP-free CPU-RM path](../16-gsp-free-cpu-rm-path/README.md) (re-ran this exact write test with GSP-RM entirely absent — identical result)
- [15 — FLR race timing](../15-flr-race-timing/README.md)
