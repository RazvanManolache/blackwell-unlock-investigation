# 28 — Shipped-binary registry extraction and multiversion firmware diff

## 28a. Extract registry-key strings from the actual shipped binary

**Objective:** every registry-key sweep in this project to that point drew
its candidate key list from GSP-RM's own firmware (the keys the GPU-resident
code reads) or from NVIDIA's public SDK headers. Neither source can see a
key the **host-side closed binary** reads directly and never forwards to
the GPU at all.

**Method:** the real target here isn't a `.ko` file — the open kernel
module is built locally from already-public source. The actual shipped
closed component is the proprietary resource-manager object the open
source links against, a large (17.6MB) relocatable ELF. It is not stripped,
so a careless whole-file string search also pulls symbol-table identifiers,
not just registry strings — redone properly against the `.rodata` section
only (8.4MB).

**Result:** 456 raw `RM*`/`Rm*` substrings, 282 of genuine CamelCase
registry-key shape, **180 entirely absent from both this project's existing
667-key GSP-firmware-side list and the public registry header.** Two
clusters stand out:

- **GSP logging/tracing** — over a dozen real keys including buffer-size
  and crash-logging controls, none of which the existing sweep could ever
  have found (that sweep only covered keys GSP-RM's own firmware reads;
  these are host-side-only strings that never reach the GPU as a
  GSP-visible key at all).
- **MIG** — the untruncated form of a MIG boot-configuration key already
  known in truncated form from the firmware-side sweep, plus a previously
  unknown sibling key controlling MIG compute-instance assignment.

String presence alone is solid static evidence of what the binary *can*
read — it is not evidence any of these 180 candidates has an observable
effect. None has been live-tested.

**Status: ⚠️ Partial — 60%.** Extraction and triage complete and solid;
live-testing any of the 180 candidates — especially the two clusters above
— needs a disposable live-boot session with the project's existing
registry-sweep discipline (confirm transmission, read back the strict
unlock criterion, revert), not the environment this extraction was done on.

## 28b. Multiversion firmware diff

**Objective:** a third-party tool's documented history shows a *different*
privilege gate on this same GPU family tightening in one driver release and
then silently no longer being enforced a few releases later — i.e.,
software-side privilege checks on this exact product line are known to
shift release to release in general. Check whether the registry-read/ACR/
speed-selector-adjacent surface this project cares about shows the same
kind of drift.

**Method:** downloaded three real NVIDIA Linux driver packages spanning a
comparable release window and extracted the actual GSP-RM firmware blob
from each (not source-tree inference — the real shipped artifact from each
package).

**Result:** a real packaging side-finding first — the firmware file itself
shrinks sharply release to release, which turned out to be NVIDIA splitting
a new sibling microcode file out of what used to be one combined file
partway through the span (see [28c](#28c-ucodes-sibling-file-check) below),
not an enforcement change. The actual check: grepped all three for the
ACR/priv-sec registry-key strings plus the headline registry override this
project's own [attempt 10](../10-registry-override-smspeedselect/README.md)
targets. **Every key present, identical count, in all three versions, with
zero drift found anywhere in this surface.** A real verified negative,
same evidentiary bar as this project's other closed registry clusters.

**Status: ❌ Confirmed negative — 100% complete.** Caveat: string
presence/count is coarser than a full decompile; a behavioral tightening
that kept the same key names wouldn't show up this way. The natural
further step (a targeted decompile of just the ACR-cluster function region
in the newer firmware images) is fully static and not yet done, but isn't
expected to change this result based on how stable the surrounding code
otherwise is.

## 28c. `ucodes` sibling-file check

**Objective:** the new sibling microcode file found in 28b above is a
large (tens of MB), previously uncharacterized file. Check whether it uses
the same internal container format as the main firmware image (which this
project already knows how to parse — see
[attempt 27c](../27-firmware-infrastructure-round2/README.md#27c-per-elf-firmware-split))
and whether any of the kernel images this project cares about moved into
it.

**Result: a clean, complete negative — it's an entirely different kind of
file.** It has its own distinct container format (a different magic
header, no relation to the main image's manifest format), and its ~57MB
payload is indistinguishable from random data — encrypted, with zero
embedded ELF images and zero kernel-name strings found anywhere inside it.
Separately confirmed the main firmware image, at this newer driver version,
still has the same manifest format and still contains the same named
kernel images this project already tracks — none of them moved into the
new sibling file. It's almost certainly encrypted per-engine microcode for
something unrelated to the resource-manager firmware this project has been
analyzing.

**Status: ❌ Confirmed negative — 100% complete.** No future version-diff
work on the kernel-image/speed-selector path needs this file; it's simply
not relevant.

## Tools

Scratch extraction scripts (makeself/zstd unpacking, `.rodata`-only string
extraction, the raw-byte container-format check) were written for this
session and are not included in this repository — they're short,
single-purpose Python scripts over publicly downloadable NVIDIA driver
packages, reproducible from the method description above.

## See also

- [10 — Registry override: SmSpeedSelect](../10-registry-override-smspeedselect/README.md)
- [11 — Registry key sweep / ACR cluster](../11-registry-key-sweep-acr-cluster/README.md)
- [21 — GSP-RM multiversion diff](../21-gsp-rm-multiversion-diff/README.md) (the original, narrower version of 28b)
- [27 — Firmware infrastructure, round 2](../27-firmware-infrastructure-round2/README.md)
