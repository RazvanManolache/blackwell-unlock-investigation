# 11 — Broader registry-key sweep and the ACR/priv-sec cluster

## 11a. Registry sweep

**Objective:** having confirmed the demand-logger oracle works (see
[attempt 10b](../10-registry-override-smspeedselect/README.md#10b-demand-logger)),
sweep beyond the original 4 name-guessed keys through the full,
authoritative list of 667 real registry keys GSP-RM's firmware reads.

**Method:** triaged the 667 keys by keyword relevance
(`speed|rate|clock|throttle|overclock|boost|tensor|issue|fuse|override|
limit`, ~96 matches), then tested in small batches, each with the same
cold-reinit + transmission-confirm + BAR0-readback procedure used in
attempt 10. Deliberately skipped keys whose expected value format looked
ambiguous (bitmask or numeric threshold rather than a simple boolean).

**Result: 24 keys tested, all clean negatives** — confirmed transmitted,
confirmed normal boot, confirmed `FEAT_OVR_PLM`/`SS0`/`SS1` byte-identical
to stock, every time:

`RMAcrExecuteOnSec2`, `RmBootFromHs`, `RMEnableAcrRiscv`,
`RmSwapAcrRegions`, `RmUseAutoAcr`, `RmAcrSigningAlgo`,
`RMDevidCheckIgnore`, `RMEnableOverclockingAllPstates`,
`RmGrRegInitOverride`, `RMHwSpeedoCheckIgnore`, `RmPerfLimitsOverride`,
`RmPerfCfOverride`, `RmPmgrOverride`, `DisableOverclockedPstates`,
`RmOverrideIdleSlowdownSettings`, `RmBootGspRmWithBoostClocks`,
`RMDisableGpuStateLoadBoost`, `RmSliGpuBoostSyncOverride`,
`RMPcieLtrOverride`, `RmEnableIntermediateLinkRates`,
`RMDebugOverrideSMCHwGpcReorder`, `RmBifCeRateEnable`, `RMNoECCFuseCheck`,
`RmOverrideSelfHostedMode`, `RmFbhubRelaxedOrderingOverride`,
`RMPriorityBoost`.

One, `RmDisableFspFuseErrorCheck=1`, was clean for `SS0`/`SS1` but genuinely
informative anyway: `dmesg` showed `kfspInitRegistryOverrides` explicitly
consulting it (`"FSP's fuse error detection status check during boot is
disabled using the regkey"`) — the first concrete, observed proof that
**FSP itself** (not just GSP-RM) directly reads and acts on a registry key.
See [attempt 13](../13-suspend-resume-register-rewrite/README.md#13c-mailbox-tamper)
for where this lead was traced further.

Roughly 70 keyword-matched candidates remain untested — mostly
numeric-looking by name (timeouts, thresholds, byte counts), deliberately
not guessed at per this project's "don't guess at ambiguous value formats"
discipline.

**Status: ⚠️ Partial — 70%.** A long tail remains, continued
opportunistically rather than as a top priority once the ACR cluster
(below) and the headline test were both closed.

## 11b. ACR/priv-sec cluster

**Objective:** test a cluster of 9 registry keys that all feed one shared
GSP-RM boot-security configuration function (`RmAllowPrivSecDisabled`,
`RMDisableAcr`, `RMAcrExecuteOnSec2`, `RmBootFromHs`, `RMEnableAcrRiscv`,
`RmSwapAcrRegions`, `RmUseAutoAcr`, `RmAcrSigningAlgo`,
`RMDevidCheckIgnore`) — carved out separately from the general sweep because
these target the protection mechanism's own *setup* path (the same general
category as the already-confirmed PL3 gate) rather than working around an
already-engaged gate.

**`RmAllowPrivSecDisabled`** is the standout: its name states directly that
it permits priv-sec (the generic hardware privilege/protection-level
scheme) to be left disabled, if the chip's fuse state allows it.

**Method:** same cold-reinit + transmission-confirm + strict
`SS0`/`SS1`-criterion procedure, one key at a time, with a recovery
procedure ready given the expectation that this cluster was more likely
than most to produce a real effect.

**Result:**

- `RmAllowPrivSecDisabled=1` → clean negative, `FEAT_OVR_PLM` unchanged, PL2
  write still silently dropped. Matches the expected result exactly (retail
  silicon's fuse state almost certainly doesn't allow priv-sec to be
  disabled).
- **`RmDisableAcr=1` → genuinely different: GSP-RM's boot actually failed.**

  ```
  NVRM: GPU0 _kgspBootGspRm: unexpected WPR2 already up, cannot proceed with booting GSP
  NVRM: GPU0 _kgspBootGspRm: (the GPU is likely in a bad state and may need to be reset)
  NVRM: GPU0 RmInitAdapter: Cannot initialize GSP firmware RM
  NVRM: GPU 0000:01:00.0: RmInitAdapter failed! (0x62:0x40:2168)
  ```

  `nvidia-smi` reported "No devices were found" — the first (and only)
  registry key this whole project found that produces a real failure
  instead of either working normally or being silently ignored.

  **Recovery, which worked cleanly on the first retry:**

  ```bash
  sudo rmmod nvidia
  echo 1 | sudo tee /sys/bus/pci/devices/0000:01:00.0/reset
  sudo modprobe nvidia   # plain, no override
  ```

  Fully healthy afterward (30°C idle, `P0` pstate), zero Xid, no lasting
  damage.

- The remaining 7 keys (`RMAcrExecuteOnSec2`, `RmBootFromHs`,
  `RMEnableAcrRiscv`, `RmSwapAcrRegions`, `RmUseAutoAcr`, `RmAcrSigningAlgo`,
  `RMDevidCheckIgnore`) were all confirmed transmitted and all clean
  negatives, with no further boot failures.

**Deliberately excluded from this cluster, found in the same decompile
sweep, never tested:** `RMRunFub` (name suggests NVIDIA's factory
fuse-burn test routine — irreversible if real) and
`RMSec2UseDebugFuseOnlyBinary` (adjacent to the already-closed
debug-image-backdoor investigation). Both are flagged for the GPU owner's
own explicit judgment, never added to any task list without it.

**Status: ❌ Confirmed, cluster closed — 100%.** One genuinely informative
failure mode (not progress toward unlock), one confirmed safe-fuse-state
negative, seven clean negatives.

## 11c. Pairwise combinations + enabler retest

**Objective:** test whether any safe *combination* of the ACR-cluster keys
(deliberately excluding `RmDisableAcr`, given its known boot-failure risk)
produces something beyond the individual clean negatives.

**Result:** tested `RMDevidCheckIgnore`+`RmUseAutoAcr` and
`RMDevidCheckIgnore`+`RMEnableAcrRiscv`+`RMAcrExecuteOnSec2` together. Both
combinations: all keys confirmed transmitted, clean boot,
`FEAT_OVR_PLM`/`SS0`/`SS1` unchanged. No sign that any safe combination
produces anything beyond the individual clean negatives already found.

**Status: ❌ Confirmed negative — 100%.**

## 11d. Shipped-binary registry-string candidates (update 2026-11-12/13)

**Objective:** the 667-key list used everywhere above was extracted from
GSP-RM's own firmware. The host-side *driver binary itself* (the shipped,
compiled `.o`, not just the public open-source tree) can contain additional
registry keys with zero trace in public headers — a pattern already
confirmed elsewhere in this project (an undocumented RPC command existed in
the real binary with nothing in the matching public source). Worth the same
check for registry strings.

**Method:** pulled the real shipped closed RM object (`nv-kernel.o_binary`)
across three actual driver versions, extracted `.rodata`-only strings (a
naive whole-binary grep would also match unrelated symbol-table names in
this unstripped object) shaped like `Rm*`/`RM*` registry keys, and diffed
against the existing 667-key list and the public `nv-reg.h` header.

**Result: 180 real candidate strings exist in the shipped binary that
aren't in either comparison source.** Two clusters stood out by name:

- A GSP log/trace group: `RmGspLogBufferSizeTaskInit`,
  `RmGspLogBufferSizeTaskIntr`, `RmGspLogBufferSizeTaskMnoc`,
  `RmGspLogBufferSizeTaskRm`, `RmGspTraceCrashLogging`.
- A MIG group, including one previously-unseen sibling key,
  `RmMIGBootConfigurationCIAssignment`.

**Live-tested in a later session:** every key in both named clusters
(including the untruncated form of `RmMIGBootConfigurationFeatureFlags`,
already known from the main sweep — not a new test) was confirmed
transmitted and produced a clean negative — booted clean,
`FEAT_OVR_PLM`/`SS0`/`SS1` byte-identical to stock, zero Xid, for every
single key. One additional named candidate, `RmGspBootInitialShift=1`, was
also tested the same way: clean negative.

**Status: ❌ Confirmed negative on the named/actionable subset — 100%ᵃ.**
ᵃ The broader 180-item candidate list itself wasn't preserved past the
session that extracted it, so the untested remainder (names not matching an
obvious functional category) would need re-extracting from the shipped
binary to pursue further — a cheap, purely mechanical re-run, not a new
technique.

## See also

- [10 — registry override: SmSpeedSelect](../10-registry-override-smspeedselect/README.md) (the demand-logger oracle this sweep reuses)
- [13 — suspend/resume register rewrite](../13-suspend-resume-register-rewrite/README.md) (where the `RmDisableFspFuseErrorCheck` lead goes next)
