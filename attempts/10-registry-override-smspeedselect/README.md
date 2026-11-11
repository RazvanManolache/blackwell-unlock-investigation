# 10 — The headline test: `RMOverrideSmSpeedSelect` registry override

This was, for a time, the single most promising lead this project produced.
It was tested fully, end to end, with a definitive negative result — three
sub-items below cover the discovery, the test itself, and closing every
remaining gap in the result.

## 10a. The override itself

**Objective:** test whether a real, firmware-recognized registry override —
found inside this project's own GSP-RM firmware decompile — can move the
speed selector past the PL3 hardware gate.

**What made this lead credible:** `RMOverrideSmSpeedSelect` /
`RMOverrideSmSpeedSelect1` are **real string literals** that GSP-RM's own
firmware looks up via its internal registry-read helper, inside two
frequently-called, directly-reachable chip-init functions — not stuck behind
an unresolved indirect jump table like almost everything else this project
examined. Confirmed absent from the public CPU-RM registry table (so not
previously known), but genuinely present and consumed in the closed GSP-RM
firmware itself.

**Why a prior researcher's "no change" result with the same key names was
judged uninformative, not disconfirming:** their test set the value via a
`modprobe -r`/`modprobe` reload while the GPU was presumably already
initialized — that may not force a genuine GSP-RM cold re-init, which this
registry read likely needs to run at all.

**Method:**

```bash
sudo systemctl stop gdm
sudo rmmod nvidia_drm nvidia_modeset nvidia_uvm nvidia
echo 1 | sudo tee /sys/bus/pci/devices/0000:01:00.0/reset   # real PCI-level FLR
time sudo modprobe nvidia \
  'NVreg_RegistryDwords=RMOverrideSmSpeedSelect=0x88888888;RMOverrideSmSpeedSelect1=0x8'
```

The PCI-level reset step (confirmed supported via `reset_method` ->
`flr bus`) was the detail that made this a genuine cold reinit rather than
a driver re-attach to an already-running GSP instance — confirmed indirectly
by load timing (2.708s, matching the real GSP boot timeline, vs. a
sub-100ms re-attach).

**Result:**

```
$ sudo cat /proc/driver/nvidia/params | grep RegistryDwords
RegistryDwords: "RMOverrideSmSpeedSelect=0x88888888;RMOverrideSmSpeedSelect1=0x8"

$ sudo ./regops 0 0000:01:00.0 bar
  FEAT_OVR_PLM        off=0x00823804 status=0 SUCCESS value=0xfffffb8f
  FEAT_OVR_SM_SS0     off=0x0082381c status=0 SUCCESS value=0x66212052
  FEAT_OVR_SM_SS1     off=0x00823820 status=0 SUCCESS value=0x00017001
```

**`SS0`/`SS1` came back byte-identical to the original stock baseline**,
despite a verifiably genuine cold reinit and a verifiably correctly-parsed
registry entry (confirmed by tracing `regFindRegistryEntry()` directly in
the open driver source — it checks per-device entries first, then falls
back to global, by case-insensitive name+type match, so scope wasn't the
problem either).

**Status: ❌ Confirmed negative — 100%.** The single most promising lead
this project had does not escape the PL3 gate.

## 10b. Demand logger

**Objective:** close the one remaining gap in 10a's result — did the
override value even get *transmitted* to GSP-RM, as opposed to being set
correctly on the host but silently never sent?

**Method:** patched the open-source host driver itself
(`src/nvidia/src/kernel/gpu/gsp/kernel_gsp.c`, right after the real
`osPackageRegistry()` call) to dump every DWORD registry key actually
packaged into the table sent to GSP-RM, rebuilt `nvidia.ko` from the real
top-level `make modules -j32` (~2 minutes, zero errors), and loaded the
custom build with the same override.

**Result:**

```
NVRM: GPU0 _kgspPrepareRegistry: GU_DEMAND_LOG: packaged registry table, numEntries=41 size=1535
NVRM: GPU0 _kgspPrepareRegistry: GU_DEMAND_LOG: DWORD key=RMOverrideSmSpeedSelect1 value=0x8
NVRM: GPU0 _kgspPrepareRegistry: GU_DEMAND_LOG: DWORD key=RMOverrideSmSpeedSelect value=0x88888888
```

**Both keys are genuinely packaged and sent to GSP-RM with the exact correct
values.** Combined with earlier static analysis of the firmware (GSP-RM's
own code never actually reads the "enable" flag for this override anywhere
in its ~22,000 decompiled functions, and the value field gets
unconditionally overwritten by a different, unrelated computation on this
chip's configuration before it's ever used), the full causal chain is now
traced with no missing link: *host sets it correctly → it's genuinely
transmitted to GSP-RM correctly → GSP-RM's own firmware simply never acts on
it.* Not a transmission bug, not a scope bug — the firmware-side plumbing
for this specific override looks unfinished or disabled, not actively
defended.

This patched build was kept and reused for the rest of the project as a
general-purpose oracle: load any registry key through it, and `dmesg` says
definitively whether GSP-RM actually received it, with the correct value,
every time — see [attempt 11](../11-registry-key-sweep-acr-cluster/README.md).

**Status: ❌ Confirmed negative — 100%.** No remaining ambiguity on this
item.

## 10c. Value encoding

**Objective:** static check of how the registry value gets parsed (dword vs.
hex string vs. decimal) and which byte of it actually lands in the final
override payload — if only the low byte matters, the whole test matrix
changes.

**Result:** the registry-read helper copies the raw 32-bit value straight to
the output pointer, no bit-packing or re-encoding of any kind. Rules out
"wrong value encoding" as an alternative explanation for 10a's negative
result — consistent with, and adding to, the two static findings quoted in
10b.

**Status: ❌ Confirmed negative — 100%.**

## Tools

- [`tools/kernel_gsp_demand_logger.patch`](tools/kernel_gsp_demand_logger.patch) —
  the instrumentation patch used for 10b.
- [`tools/registry_keys_all.txt`](tools/registry_keys_all.txt) — the full
  list of 667 registry keys GSP-RM's firmware reads, extracted from the full
  decompile — the candidate pool this and the next attempt draw from.

## See also

- [11 — registry key sweep / ACR cluster](../11-registry-key-sweep-acr-cluster/README.md)
- [13 — suspend/resume register rewrite](../13-suspend-resume-register-rewrite/README.md) (finds the *other* half of why this override has no effect — the value gets clobbered by an unrelated computation)
- [RainbowWizard-coder/gpu-unlock](https://github.com/RainbowWizard-coder/gpu-unlock) — source of the original lead
- [steviecurrie/gpu-unlock-gb206](https://github.com/steviecurrie/gpu-unlock-gb206) — the prior test whose "no change" result motivated the cold-reinit redesign here
