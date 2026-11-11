# 20 — mVolt+ precedent study

mVolt+ is a closed-source, third-party RTX 50-series voltage/power-limit
override tool by [b00nz](https://github.com/b00nz/mVolt). Studied purely as
a structural precedent for this project's own registry-override theory —
nothing here involved live voltage/power-limit writes on the target
hardware; see [attempt 23](../23-power-limit-canary/README.md) for the one
item that did.

## 20a. NVAPI surface study

**Objective:** understand how mVolt+'s override mechanism actually works,
originally hoping to extract reusable NVAPI ordinals or RM_CONTROL command
numbers.

**Correction to the original premise:** the project's own GitHub repo is
**not** the application source — it's documentation plus screenshots for a
closed-source, binary-only freeware `.exe`. No ordinals or command numbers
to extract from it directly; that half of the plan was a dead end.

**What the user guide documents instead, directly useful:** mVolt+'s "BIOS
override" (Direct or BIOS-file method) does **not** flash the card — it
saves a per-GPU override blob on the host (`%PROGRAMDATA%\mVolt+\
NativeLimits\<64-char-id>\`), which the driver/firmware reads and applies
at the next "Restart GPU" event (a narrower re-init than a full Windows
reboot), overriding otherwise-fixed BIOS-reported clock/voltage/current
limits.

**This is functionally the same shape as this project's own registry-key
override approach**: a host-persisted value, consumed at a chip-(re)init
event, overriding a value that's otherwise presented as fixed. It's
independent precedent that the general *class* of mechanism this project
pursued in [attempt 10](../10-registry-override-smspeedselect/README.md) is
at least the right category, even though the specific registers this
project targeted turned out not to work that way.

Two more directly relevant details documented in mVolt+'s own guide:

1. Driver 621.01+ disabled *live* NVAPI dashboard voltage/OCP writes
   specifically — "the driver accepts these writes and reports success, but
   the GPU keeps its previous limits; the limits appear to be enforced below
   the driver" (their words) — yet the **persisted BIOS-override path still
   works** on 621.01+, mirroring this project's own expectation that a
   boot/reinit-consumed override can reach deeper than a live runtime write
   ever could.
2. A malformed/corrupted override can produce a Windows Code 43 (driver
   refuses to start the GPU at all), with a documented CLI recovery path
   (`mVolt+.exe --remove-bios-override`) that works without the NVIDIA
   driver running — useful cautionary precedent for having an equivalent
   recovery procedure in mind before setting any new registry key for the
   first time (this project already did, via full reboot + bogus-key
   control).

**Status: 🔬 Analysis complete — 100%.** Nothing in this item touched live
hardware — it was reading a published user guide.

## 20b. Binary strings survey

**Objective:** a purely static `strings` pass over the real mVolt+ release
executable (**never executed**), to confirm or extend 20a's findings.

**Result:** UTF-16 extraction found the real persistence mechanism by name
(`ProgramData\mVolt+\NativeLimits\<64-char-id>\`, with its own
`--recover-limits` CLI recovery path) — confirms 20a's blob-persistence
theory textually. Cross-checked all 667 known GSP-RM registry key names
(see [attempt 10](../10-registry-override-smspeedselect/README.md)) against
both ASCII and UTF-16 dumps — **zero matches either way**: mVolt's mechanism
is a separate NVAPI-level format, not a reuse of GSP-RM's own registry
keys.

**Status: 🔬 Analysis complete — 100%.** Confirms the general pattern, not a
specific new lever.

## See also

- [10 — registry override: SmSpeedSelect](../10-registry-override-smspeedselect/README.md)
- [23 — power-limit canary](../23-power-limit-canary/README.md)
- [19 — voltage fault injection (parked)](../19-voltage-fault-injection-parked/README.md)
