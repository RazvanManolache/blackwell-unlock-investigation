# Blackwell tensor-core speed-selector: unlock investigation

A from-scratch investigation into whether NVIDIA's "Blackwell" (RTX 50-series)
consumer GPUs can have their tensor-core issue-rate clamp removed or
bypassed, carried out on the owner's own desktop **RTX 5080 (die GB203, PCI
device `10de:2c02`)**.

Consumer Blackwell cards ship with a hardware-level register pair that caps
tensor-core throughput well below what the silicon is physically capable of
(the same die, sold as the unlocked RTX PRO 6000, runs at roughly double the
tensor rate with no other hardware difference). This repository documents
every avenue this project tried to find a way around that cap: what worked,
what didn't, what's still open, and — just as importantly — what was
deliberately *not* attempted and why.

**Short version: no unlock was found.** The cap is enforced by a genuine
hardware privilege wall, not a software flag, and nothing tried here — real
hardware writes, firmware registry overrides, driver patches, RPC fuzzing,
or static firmware analysis — got past it. Several real, useful negative
results came out of the process anyway, documented below.

## The mechanism, in brief

Three BAR0 memory-mapped registers on the GB20x die family govern the tensor
issue-rate selector:

| Register | Offset | Role |
|---|---|---|
| `FEAT_OVR_PLM` | `0x823804` | Privilege-level mask (PLM) for the selector registers below |
| `FEAT_OVR_SM_SS0` | `0x82381c` | SM speed-selector word 0 |
| `FEAT_OVR_SM_SS1` | `0x823820` | SM speed-selector word 1 |

`FEAT_OVR_PLM`'s stock value, confirmed identical across every Blackwell die
this project or its prior art has measured (GB202/GB203/GB203M/GB206), is
`0xfffffb8f`. The write-mask bit this encodes means **only hardware
privilege-level 3 (PL3) may write `SS0`/`SS1`.** The host CPU is always PL0.
GSP-RM, NVIDIA's own GPU-resident management firmware, runs at PL2 — one
level short. Both are confirmed, by direct experiment on this exact hardware,
to have their writes to these registers **silently accepted and dropped**:
the ioctl/regop returns `SUCCESS`, and an independent readback shows the
register unchanged.

**The only thing this project ever counts as a confirmed unlock:**

```
SS0 == 0x88888888  AND  SS1 == 0x8      (exact match)
```

A nonzero `SS0`/`SS1` that isn't exactly this pair is pre-driver shadow-state
noise, not progress — this criterion is enforced mechanically throughout
(`verdict.py`, see `attempts/01-baseline-capture-and-write-gate/tools/`), not
left to a prose judgment call that could drift.

**Worth stating plainly:** a GitHub code search for the literal register
names `FEAT_OVR_PLM` and `FEAT_OVR_SM_SS` returns **zero results** anywhere
on GitHub as of this writing. This project, and the handful of prior-art
repos it draws on (below), are working without published precedent on the
exact mechanism — there is no known public write-up of someone defeating
this specific gate.

## Legend

| Icon | Meaning |
|---|---|
| ✅ | Confirmed success — the strict unlock criterion (`SS0==0x88888888 && SS1==0x8`) was met |
| ❌ | Tested — confirmed negative. A real, definitive result, not an inconclusive one |
| ⚠️ | Partial / inconclusive — genuinely attempted, produced real signal, not resolved either way |
| 🔬 | Analysis / infrastructure only — static or tooling work done, no hardware-decisive test yet |
| ⏸️ | Not started |
| 🚫 | Parked — deliberately not attempted, pending the GPU owner's own explicit authorization |

**Completeness %** is how far each attempt actually got, independent of
whether the result was good news: 100% means fully executed with a
definitive result either way (success *or* a clean, confirmed negative);
50–90% means meaningfully executed but left genuinely inconclusive; 10–40%
means analysis/theory only, no live test; 0% means not started or parked.

**No row below is ✅.** That's the honest headline result of this whole
project so far, not an omission.

## All attempts

| # | Attempt | Status | Completeness | Details |
|---|---|---|---|---|
| 1 | First read-only register baseline on desktop GB203 + the PL3-only write-gate confirmation | ❌ | 100% | [01](attempts/01-baseline-capture-and-write-gate/README.md) |
| 1b | Pre-bind `FEAT_OVR_PLM` check (is the gate closed before the OS driver even binds?) | ❌ | 100% | [01](attempts/01-baseline-capture-and-write-gate/README.md#1b-pre-bind-check) |
| 2 | Re-verify "no GB20x branch" in the open driver's booter-ucode table, at the exact running version | ❌ | 100% | [02](attempts/02-open-source-driver-recheck/README.md) |
| 3 | Read the arXiv SEC2-on-Blackwell paper in full — does it reopen the old SEC2 exploit surface? | ❌ | 100% | [03](attempts/03-sec2-hopper-paper-review/README.md) |
| 4 | Whole-firmware static sweep for direct `SS0`/`SS1`/`PLM` writer instructions | ⚠️ | 70% | [04](attempts/04-direct-register-writer-static-sweep/README.md) |
| 5a | Find/leak a public GB20x SEC2 booter image to mine gadgets from | ❌ | 100% | [05](attempts/05-closed-ampere-ada-techniques/README.md#5a-booter-leak) |
| 5b | Use a debug-signed FMC as a backdoor (old test-key-37 pattern) | ❌ | 100% | [05](attempts/05-closed-ampere-ada-techniques/README.md#5b-debug-image-backdoor) |
| 5c | Brute-force the FMC encryption key via an MD5-IV permutation family | ❌ | 100% | [05](attempts/05-closed-ampere-ada-techniques/README.md#5c-fmc-key-bruteforce) |
| 5d | Other known write paths (direct GSP-RM write, CUDA-debugger-class write, VBIOS reflash, fuse rewrite, SM_ISSUE_RATE_MODIFIER, FECS doorbell) | ❌ | 100% | [05](attempts/05-closed-ampere-ada-techniques/README.md#5d-other-closed-paths) |
| 6 | vGPU/SR-IOV VF-PRIV-register angle | ❌ | 100% | [06](attempts/06-vgpu-sriov-priv-wall/README.md) |
| 7 | Port-readiness checklist vs. a GB202 feasibility writeup (decrypted firmware / pre-driver payload) | ⏸️ / 🔬 | 0% / 100% | [07](attempts/07-port-checklist-gb202-blackwell/README.md) |
| 8 | Windows-side NVAPI/ioctl passthrough feasibility study | ❌ | 100% | [08](attempts/08-windows-nvapi-passthrough-feasibility/README.md) |
| 9 | First direct tensor issue-rate measurement on desktop GB203 (Windows, CUDA) | ❌ | 100% | [09](attempts/09-windows-tensor-rate-measurement/README.md) |
| 10a | **Headline test**: `RMOverrideSmSpeedSelect`/`SmSpeedSelect1` — a real, firmware-recognized registry override | ❌ | 100% | [10](attempts/10-registry-override-smspeedselect/README.md#10a-the-override-itself) |
| 10b | Registry-transmission demand-logger (confirms the override really reaches GSP-RM) | ❌ | 100% | [10](attempts/10-registry-override-smspeedselect/README.md#10b-demand-logger) |
| 10c | Static check of how the override value is encoded/parsed | ❌ | 100% | [10](attempts/10-registry-override-smspeedselect/README.md#10c-value-encoding) |
| 11a | Broader 667-key registry sweep (24 keys tested so far) | ⚠️ | 70% | [11](attempts/11-registry-key-sweep-acr-cluster/README.md#11a-registry-sweep) |
| 11b | ACR / priv-sec registry cluster (9 keys) — the one real boot failure of the project | ❌ | 100% | [11](attempts/11-registry-key-sweep-acr-cluster/README.md#11b-acr-cluster) |
| 11c | ACR pairwise combinations + `RMDevidCheckIgnore` enabler retest | ❌ | 100% | [11](attempts/11-registry-key-sweep-acr-cluster/README.md#11c-pairwise) |
| 12a | Build a real caller-owned GPU channel to reach per-SM register copies | ⚠️ | 80% | [12](attempts/12-per-sm-register-channel/README.md#12a-channel-build) |
| 12b | Bounded group/subgroup brute-force sweep over the per-SM channel | ❌ | 100% | [12](attempts/12-per-sm-register-channel/README.md#12b-offset-sweep) |
| 13a | Suspend/resume state blob mutation | ⏸️ | 0% | [13](attempts/13-suspend-resume-register-rewrite/README.md#13a-sr-resume-blob) |
| 13b | What computes the per-boot/per-resume `SS0`/`SS1` value, and is any input host-influenceable? | 🔬 | 100% | [13](attempts/13-suspend-resume-register-rewrite/README.md#13b-ss-select-input-hunt) |
| 13c | GSP(PL2)→FSP(PL3) mailbox handoff hunt (confused-deputy angle) | ⚠️ | 40% | [13](attempts/13-suspend-resume-register-rewrite/README.md#13c-mailbox-tamper) |
| 13d | Resume-handler patch to force override values or skip the PLM restore | ⏸️ | 0% | [13](attempts/13-suspend-resume-register-rewrite/README.md#13d-resume-handler-patch) |
| 13e | Suspend/resume as a cheaper re-trigger loop for registry tests | ⏸️ | 0% | [13](attempts/13-suspend-resume-register-rewrite/README.md#13e-fast-retrigger) |
| 13f | Per-SM-group selector rotation survey across many boot/resume cycles | ⏸️ | 0% | [13](attempts/13-suspend-resume-register-rewrite/README.md#13f-rotation-survey) |
| 13g | Plain suspend/resume baseline on this exact die (no mutation) | ⏸️ | 0% | [13](attempts/13-suspend-resume-register-rewrite/README.md#13g-plain-resume-baseline) |
| 13h | Port the `GspFwSRMeta` kprobe hook to this driver version | ⏸️ | 0% | [13](attempts/13-suspend-resume-register-rewrite/README.md#13h-srmeta-port) |
| 13i | Enumerate every writer of the stored-override context offsets | ❌ | 100% | [13](attempts/13-suspend-resume-register-rewrite/README.md#13i-writers-sweep) |
| 14a | Full GSP-RM RPC fuzzing campaign (fuzz_harness, 56+ live attempts) | ⚠️ | 85% | [14](attempts/14-rpc-audit-undocumented-commands/README.md#14a-rpc-fuzzing) |
| 14b | `GC6_ENTRY`'s 12-vs-20-byte size mismatch | ⚠️ | 80% | [14](attempts/14-rpc-audit-undocumented-commands/README.md#14b-gc6-entry) |
| 14c | Patch the host export-table flags to make unreachable commands reachable | ⚠️ | 60% | [14](attempts/14-rpc-audit-undocumented-commands/README.md#14c-export-table-patch) |
| 14d | Kernel-privileged RM client instead of a userspace one | ❌ | 90% | [14](attempts/14-rpc-audit-undocumented-commands/README.md#14d-kernel-client) |
| 14e | Decode the remaining reachable-command flags; LD_PRELOAD ioctl trace of ordinary tool use | ⚠️ | 95% | [14](attempts/14-rpc-audit-undocumented-commands/README.md#14e-flags-and-trace) |
| 14f | strace the driver load for host-file-open override candidates | ❌ | 100% | [14](attempts/14-rpc-audit-undocumented-commands/README.md#14f-strace-surface) |
| 15 | FLR + BAR0 write race with the driver unbound (is the PLM re-close hardware-timed or GSP-driven?) | ⚠️ | 60% | [15](attempts/15-flr-race-timing/README.md) |
| 16 | Force the legacy GSP-free CPU-RM path and retest everything there | ❌ | 100% | [16](attempts/16-gsp-free-cpu-rm-path/README.md) |
| 17 | GSP firmware signature oracle (single-byte flip, accept vs. reject) | 🚫 | 0% | [17](attempts/17-firmware-signature-oracle-parked/README.md) |
| 18a | VBIOS devinit-table override (the flagged edit half) | 🚫 | 0% | [18](attempts/18-vbios-devinit/README.md#18a-devinit-override-parked) |
| 18b | Read-only VBIOS dump — does a host-visible pre-GSP init table even exist on GB203? | ❌ | 100% | [18](attempts/18-vbios-devinit/README.md#18b-readonly-premise-check) |
| 19 | Voltage/clock-margin fault injection timed against the PL3 check | 🚫 | 0% | [19](attempts/19-voltage-fault-injection-parked/README.md) |
| 20a | mVolt+ (third-party RTX 50-series voltage/power tool) mechanism study | 🔬 | 100% | [20](attempts/20-mvolt-plus-precedent-study/README.md#20a-nvapi-surface-study) |
| 20b | mVolt+ binary strings survey (static only, never executed) | 🔬 | 100% | [20](attempts/20-mvolt-plus-precedent-study/README.md#20b-binary-strings-survey) |
| 21 | Diff GSP-RM firmware across driver releases for enforcement drift | ⏸️ | 0% | [21](attempts/21-gsp-rm-multiversion-diff/README.md) |
| 22 | Find the cheapest event that re-triggers the registry read (gpu-reset vs. reload vs. resume vs. reboot) | ⏸️ | 0% | [22](attempts/22-cheapest-retrigger-ladder/README.md) |
| 23 | Power-limit accept-vs-ignore canary (does `nvidia-smi -pl` apply cleanly or silently no-op?) | ❌ | 100% | [23](attempts/23-power-limit-canary/README.md) |
| 24 | FSP/COT scratch-register boot-state timeline | ⚠️ | 60% | [24](attempts/24-fsp-cot-scratch-registers/README.md) |
| 25 | FMC/FSP parser heap-size field (the one non-signature-checked input) | ⏸️ | 0% | [25](attempts/25-fmc-parser-heap-size/README.md) |

## Infrastructure built along the way

**Full GSP-RM firmware decompile.** GB20x's GSP-RM main-body firmware ships
in plaintext RISC-V (unlike the encrypted FMC stage ahead of it). This
project decompiled the entire thing — 22,267 functions, 22,212 successfully
— using Ghidra 12.2 driven headlessly, and kept the saved project and full
decompiled-C output as a durable asset reused across many of the attempts
above (static register-writer sweeps, registry-key extraction, jump-table
resolution). Several rounds of structural/behavioral search across the full
corpus were run looking for the per-SM register array's base address;
documented honestly as "exhausted for this technique," not "found."

**Raspberry Pi hang-detection/logging harness.** A disposable Raspberry Pi,
acting as a USB boot device for the target machine, runs a pre-driver UEFI
diagnostic payload and gives an AI coding agent full autonomy to iterate
overnight: serve the payload, collect the resulting log, mechanically check
it against the strict unlock criterion above (code, not prose judgment),
update a tracked checklist, and decide what to try next — with the only
backstop being automatic quarantine after repeated hangs, not a permission
gate. This is the harness referenced throughout as the source of the
mechanical-verdict discipline and much of the overnight live-hardware work.

**Disposable-Linux-live-boot safety discipline.** All hardware-risking
testing (module reloads, PCI-level resets, firmware registry overrides,
RPC fuzzing, GPU channel/engine submissions) was run from a Ventoy-based
Ubuntu live boot on the target machine's own hardware — the real Windows
installation was never touched. Every live action followed a fixed
before/after discipline: capture GPU temperature/power/pstate and clear the
kernel log ring buffer before, then re-check both plus an independent
`dmesg` Xid/error grep after. A hang or confirmed-bad state triggered a full
power cycle, never a same-session retry.

## Prior art and inspiration

This project builds directly on public research by several independent
researchers, tracked and periodically re-synced throughout. Credited here as
prior art — none of these are forks of this project's own work, and this
project is not affiliated with any of them.

| Repo | Author | What it contributes |
|---|---|---|
| [zagitta/gpu-unlock](https://github.com/zagitta/gpu-unlock) | zagitta | The original Ampere unlock: canonical register names/offsets and the recipe-driven family structure everything downstream builds on |
| [JustAResearcher/gpu-unlock-ada](https://github.com/JustAResearcher/gpu-unlock-ada) | JustAResearcher | Ada (AD10x) port of the above, plus verification tooling (`read_sm_rates.py`, booter-stack tracing) reused directly in this project |
| [Consensus-Protocol/cmp170hx](https://github.com/Consensus-Protocol/cmp170hx) | Consensus-Protocol | Clearest written explanation of the original V67/ROP/PLM technique for newcomers to the mechanism family |
| [phiraml/gpu-unlock-blackwell](https://github.com/phiraml/gpu-unlock-blackwell) | phiraml | GB202 feasibility/measurement/port-research — measured real tensor-math clamp ratios and defined the 5-item port-readiness checklist this project tracked against |
| [steviecurrie/gpu-unlock-gb206](https://github.com/steviecurrie/gpu-unlock-gb206) | steviecurrie | GB206 (RTX 5060 Ti) read-only capture tooling and logs — the primary source for this project's cross-die register baseline, and the suspend/resume `SS0`/`SS1` drift observation that opened this project's whole resume-rewrite thread |
| [shuttie/bf16-unlock-blackwell-research](https://github.com/shuttie/bf16-unlock-blackwell-research) | shuttie | The most detailed public negative-result log for this exact mechanism — 87 logged experiments on real GB202 hardware (RTX 5090 / RTX PRO 6000), including the first measurement of the PL3-only write mask and the FLR-reopen timing window. `regops.c`, reused directly in this project, is theirs |
| [meng-luo/blackwell-lock-research](https://github.com/meng-luo/blackwell-lock-research) | meng-luo | Read-only archive articulating why the old debug-signed-image trick doesn't port to Blackwell (test-key vs. no-test-key, 99.5% byte difference) |
| [bird/vgpu-unlock-blackwell](https://github.com/bird/vgpu-unlock-blackwell) | bird | The vGPU/SR-IOV angle — a different register target (VF PRIV) hitting the same architectural wall (GSP owns MMIO directly, nothing to intercept CPU-side) |
| [84634E1A607A/gpu-unlock-4090-48g-64g-bar1](https://github.com/84634E1A607A/gpu-unlock-4090-48g-64g-bar1) | 84634E1A607A | Ada BAR1-expansion fork — not about the tensor clamp directly, kept for the four-write PLM chain pattern it documents |
| [RainbowWizard-coder/gpu-unlock](https://github.com/RainbowWizard-coder/gpu-unlock) | RainbowWizard-coder | Independent GB20x research notes; source of the original `RMOverrideSmSpeedSelect` lead this project's headline test (#10) is built on |
| [Panchovix/nvidia-vbios-vix-editor](https://github.com/Panchovix/nvidia-vbios-vix-editor) | Panchovix | VBIOS editing tooling, referenced for the (parked) VBIOS-devinit avenue |
| [b00nz/mVolt](https://github.com/b00nz/mVolt) | b00nz | A closed-source but well-documented third-party voltage/power-limit override tool for this same GPU family — its host-persisted, consumed-at-reinit override mechanism is a useful structural parallel to this project's own registry-override theory |
| [NVIDIA/open-gpu-kernel-modules](https://github.com/NVIDIA/open-gpu-kernel-modules) | NVIDIA | NVIDIA's own open-source kernel driver — primary source for every host-side gate, struct layout, and export-table check this project relied on, and the base for every locally-patched/rebuilt driver used in testing |

**No known public precedent exists for the exact mechanism this project
targets.** A GitHub code search for the literal strings `FEAT_OVR_PLM` and
`FEAT_OVR_SM_SS` returns zero results anywhere on GitHub outside this
project's own work — everything above establishes the surrounding
architecture and prior technique family, not a solved version of this
specific problem.

## Safety and ethics

Every piece of hardware-facing work in this repository was performed on
hardware the investigator owns, from a disposable Ventoy Linux live-boot
environment that never touched the machine's real Windows installation.
Every live action followed a fixed health-check discipline (GPU
temperature/power/pstate and kernel log state captured before and after
every attempt), and anything that produced a confirmed-bad GPU state was
recovered via a full PCI-level reset or power cycle, never pushed further
in the same session.

A small number of avenues were deliberately **not attempted**, flagged
throughout as needing the GPU owner's own explicit go-ahead rather than
being executed on an AI coding agent's own initiative:

- **Firmware-signature acceptance testing** (flipping a byte in a copy of
  the extracted GSP firmware and seeing whether the driver accepts or
  rejects it) — rejection is harmless, but acceptance would mean altered
  code executing on a privileged RISC-V core with no established recovery
  story.
- **Registry keys whose names suggest an irreversible factory routine**
  (a fuse-burn test routine, a debug-fuse-only binary path) — irreversible
  by definition if real.
- **VBIOS devinit-table modification** — real hard-brick potential, no
  tested recovery path exists for this card.
- **Voltage/clock-margin fault injection** — genuine glitching against the
  owner's own desktop GPU, categorically different from a bounded,
  vendor-demonstrated offset adjustment, with no established recovery path.
- **Live `GC6_ENTRY` invocation** — a real GPU deep-sleep power-rail-gating
  transition whose exit path depends on laptop-style platform firmware
  support a desktop board has never been validated for.

Where voltage- or power-limit-adjacent experiments *were* run (the
power-limit canary, the mVolt+ precedent study), they stayed within ranges
already demonstrated safe by existing vendor/community overclocking tools,
read back and logged the stock value before any write, reverted immediately
after, and stopped at the first sign of instability rather than searching
for a limit.
