# 13 — The suspend/resume register-rewrite thread

The trigger for this whole thread: an independent researcher's suspend/resume
logs on a different die (GB206, RTX 5060 Ti) showed `FEAT_OVR_SM_SS0`/`SS1`
**changing value across a plain `mem` suspend/resume cycle with zero writes
from the host**, reproduced across multiple runs on the same physical die.
Both PL0 (host) and PL2 (GSP-RM) are already proven unable to write these
registers — so *something* with PL3 authority is rewriting them as routine
behavior on every resume, not just once at cold boot. This reframes the
question: not "get PL3 execution," but "find what this PL3-authorized
computation actually depends on, and whether any dependency is
host-influenceable."

Nine distinct sub-threads came out of this reframe.

## 13a. sr-resume-blob

**Objective:** investigate the suspend/resume state blob (~206 MB/card,
measured entropy 7.9998 — i.e., indistinguishable from random) as a
potential tampering surface, since resume appears to reuse cold-boot trust
rather than re-verifying from scratch.

**Result:** not run on this die. An independent researcher tried this
item's own proposed next step (mutating the suspend/resume descriptor —
halved `sizeOfSuspendResumeData`, `sysmemAddrOfSuspendResumeData` shifted by
one page) on a different die and got a kernel slab Oops on resume, not a
bypass — a useful negative data point (expect a crash from this class of
input) before ever attempting the same mutation here.

**Status: ⏸️ Not started — 0%.**

## 13b. ss-select-input-hunt

**Objective:** find what computes the per-boot/per-resume `SS0`/`SS1` value
in the decompiled firmware, and trace every input to that computation —
anything host-influenceable, even indirectly, is a candidate for biasing
the result without ever touching the gated register directly.

**Method:** searched the full firmware decompile for where an SS0/SS1-shaped
composite value (8 nibbles, enable-bit + 3-bit selector per nibble) gets
assembled, near the `RMOverrideSmSpeedSelect` read site already found (see
[attempt 10](../10-registry-override-smspeedselect/README.md)).

**Result — two concrete findings:**

1. The override's own enable-flag bytes (`+0xe45`/`+0xe46`) are **written but
   never read anywhere** in the 22,212-function corpus — checked by name and
   by bare hex offset, both empty. A neighboring, structurally identical
   flag (`+0xe47`) *is* read elsewhere, ruling out "grep just can't see byte
   flag reads here" as an alternative explanation.
2. The consumer function unconditionally **overwrites the
   `SmSpeedSelect1` value field (`+0xeac`)** with a freshly computed boolean
   from an unrelated fuse/config struct whenever a chip-mode field doesn't
   equal a specific value — clobbering the override before it's ever used,
   on (most likely) every retail die.

This gives a second, purely-software explanation for attempt 10's negative
result, independent of the PL3 hardware gate itself: the override path may
simply be dead/unfinished code, not just hardware-blocked.

**Status: 🔬 Analysis complete — 100%.** Real, decompile-only findings; no
host-influenceable input was identified that reaches the actual rewrite.

## 13c. gsp-fsp-mailbox-tamper

**Objective:** find the GSP(PL2)→FSP(PL3) handoff that results in `SS0`/
`SS1` being rewritten on every boot/resume, and whether the message GSP
prepares is tamperable before FSP consumes it (a confused-deputy angle: GSP
itself can't write these registers either, so whatever does must be acting
on a request GSP handed off earlier, almost certainly via shared memory
rather than a direct call).

**Result:** four channel-handle globals flagged as a possible handoff point
were traced through Ghidra's own memory map — confirmed genuinely
initialized/file-backed (correcting an earlier "maybe uninitialized BSS"
guess), but the actual bytes are high-entropy and don't look like plausible
small-integer handles at all. Left as a real, honestly unresolved question:
either the real value only exists after GSP-RM's own runtime
initialization (unrecoverable from a static firmware image), or this data
was never simple handles in the first place.

A real, named FSP mailbox transport selector was found along the way
(`NV_REG_STR_RM_FSP_USE_MNOC`, confirmed via the `RmDisableFspFuseErrorCheck`
lead in [attempt 11a](../11-registry-key-sweep-acr-cluster/README.md#11a-registry-sweep))
but not yet confirmed relevant to `SS0`/`SS1` specifically.

**A real FSP command, `NVDM_TYPE_CLOCK_BOOST`, was traced in full** — tied
directly to the already-tested `RmBootGspRmWithBoostClocks` registry key.
Root-caused why that key does nothing: the capability check that would
enable this path is wrapped in `if (!IS_GSP_CLIENT(pGpu))` in the driver
source — skipped entirely on this machine's GSP-client configuration.
Confirmed structurally, not guessed.

**Flagged, not tested:** `NV_REG_STR_RM_DISABLE_COT_CMD`'s `GSPFMC` bit
disables a Chain-of-Trust boot step entirely — same risk category as
`RmDisableAcr`'s real boot failure (see
[attempt 11b](../11-registry-key-sweep-acr-cluster/README.md#11b-acr-cluster)),
possibly worse. Needs the GPU owner's own explicit go-ahead before testing.

**Status: ⚠️ Partial — 40%.** Real tracing progress, the mailbox itself not
yet identified.

## 13d. resume-handler-patch

**Objective:** patch the open-driver's own resume handler to force override
values or skip the PLM restore step, instead of blind blob mutation.

**Result:** not attempted — needs a netconsole collector (so a hung resume
doesn't go undiagnosed) that wasn't available in any session so far; this
project's own safety procedure for suspend/resume work requires it.

**Status: ⏸️ Not started — 0%.**

## 13e. resume-cycle-fast-retrigger

**Objective:** use a suspend/resume cycle instead of a full reboot as the
re-trigger for every registry-key test queued elsewhere in this project —
resume demonstrably re-runs the same registry-read/chip-init path cold boot
does (that's the entire reason this thread exists), so this is a faster
iteration loop, not a new lever by itself.

**Status: ⏸️ Not applied yet — 0%.** Methodology identified, not yet used to
accelerate the sweeps in [attempt 11](../11-registry-key-sweep-acr-cluster/README.md).

## 13f. ss-selector-rotation-survey

**Objective:** log the full 8-nibble `SS0` breakdown across many boot/resume
cycles — if the uncapped selector value ever appears naturally for any
individual SM group, that's an existence proof worth escalating; a clean
negative across many cycles is equally informative.

**Status: ⏸️ Not started — 0%.**

## 13g. gb203-plain-resume-baseline

**Objective:** the first plain suspend/resume on this exact die with zero
mutations — does the GB206 resume-drift finding transfer here at all?

**Status: ⏸️ Not started — 0%.** Needs the same netconsole-collector
precondition as 13d.

## 13h. srmeta-hook-port-59599

**Objective:** port a `GspFwSRMeta` kprobe hook (demonstrated host-writable
before GSP's own resume processing, on a different driver version/die) to
this project's exact running driver version (595.99.02): locate the copy
site, diff the struct layout against the version it was originally
demonstrated on.

**Status: ⏸️ Not started — 0%.** Pure static work, not yet done.

## 13i. ss-input-writers-fuse-callers

**Objective:** enumerate every writer of the stored-override context offsets
(`+0xea8`/`+0xeac`) and every caller of the `FUSE_SS` fuse-read helper, to
check whether a second, undiscovered writer exists beyond the one already
found in 13b.

**Result:** checked every `+0xea8`/`+0xeac` write in the decompile (69 raw
hits) for genuine struct identity — no second writer found; every other hit
is a confirmed struct-offset coincidence in unrelated code (callback-pointer
storage, generic struct-clearing, allocation bookkeeping, array/sort
processing). Only the already-known registry-set writes and the already-known
13b consumer are genuinely the same struct. The `FUSE_SS` string-literal
search came up empty — fuse register names don't exist as strings in
compiled firmware, the same limitation noted elsewhere in this project;
resolving that half would need address-based tracing instead, not attempted.

**Status: ❌ Confirmed negative — 100%.** A real, checked negative, not a
gap.

## Tools

- [`tools/ioctl_trace.c`](tools/ioctl_trace.c) is filed under
  [attempt 14](../14-rpc-audit-undocumented-commands/README.md) — it's a
  tracing tool, not specific to this thread, reused across both.

## See also

- [10 — registry override: SmSpeedSelect](../10-registry-override-smspeedselect/README.md)
- [11 — registry key sweep / ACR cluster](../11-registry-key-sweep-acr-cluster/README.md)
- [steviecurrie/gpu-unlock-gb206](https://github.com/steviecurrie/gpu-unlock-gb206) — source of the original resume-drift observation that opened this whole thread
