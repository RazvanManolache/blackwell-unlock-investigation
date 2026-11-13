# 21 — GSP-RM firmware diff across driver releases

## Objective

Pull GSP-RM firmware blobs from a spread of other Blackwell-era driver
releases and check for enforcement drift around the registry-read/ACR/
SmSpeedSelect-adjacent code regions this project has already located and
decompiled.

## Hypothesis

This project has only decompiled one GSP-RM firmware blob in depth (the one
behind the full registry key list and the ACR-cluster finding). mVolt+'s own
driver-to-driver enforcement changes (see
[attempt 20](../20-mvolt-plus-precedent-study/README.md) — a gate tightened
in driver 616.56, then silently stopped being enforced in 621.01+) prove
software-side gates on this product line are **not** static across releases,
independent of the hardware fuse logic (`FEAT_OVR_PLM` itself is proven
identical across every die measured — that part won't move; it's the
registry-read/ACR/priv-sec decision logic *around* it that might).

## Method

Pulled real GSP-RM firmware blobs (`gsp_ga10x.bin`) from three actual
driver releases spanning the exact window where the unrelated mVolt+ gate
is independently known to have drifted (see [attempt 20](../20-mvolt-plus-precedent-study/README.md)).
Per this item's own instruction not to re-decompile per version unless
something differs, the cheap first pass was a literal-string presence/count
check: the nine ACR/priv-sec registry-key strings
(`RmAllowPrivSecDisabled`, `RMDisableAcr`, `RMAcrExecuteOnSec2`,
`RmBootFromHs`, `RMEnableAcrRiscv`, `RmSwapAcrRegions`, `RmUseAutoAcr`,
`RmAcrSigningAlgo`, `RMDevidCheckIgnore`) plus `SmSpeedSelect`, across all
three firmware blobs.

## Result

**Identical in every version checked.** Every one of the nine ACR/priv-sec
keys appears exactly once, and `SmSpeedSelect` exactly twice, byte-for-byte
the same count, in all three driver releases spanning that window.
`FEAT_OVR` as a literal string: zero hits in all three — consistent with it
being a compiled constant rather than a string anywhere, matching this
project's findings elsewhere.

**Side-finding, not a security change:** the firmware file itself shrank
noticeably release to release — explained by a packaging split (a new
sibling file split out of the main firmware image starting at one of the
later versions), not by anything being removed from the GSP-RM logic
itself. Worth knowing before anyone diffs raw file sizes alone and jumps to
a conclusion.

**Caveat stated plainly:** string presence/count is a coarser signal than
a full decompile of the relevant function region in each version (which
this item's own method explicitly deferred unless a difference showed up).
A real behavioral change that keeps the same strings — e.g. a tightened
comparison inside the same function — would not be caught by this method.
The next, still-static step if anyone wants to go deeper is a targeted
decompile of just the ACR-cluster function region in each version, not a
full re-run.

## Status

**❌ Confirmed negative — 90%.** No version-dependent weakening found for
this specific mechanism across the span checked. Not 100%, since the
method used (string presence) is coarser than the full decompile comparison
this item originally specified.

## See also

- [10 — registry override: SmSpeedSelect](../10-registry-override-smspeedselect/README.md)
- [11 — registry key sweep / ACR cluster](../11-registry-key-sweep-acr-cluster/README.md)
- [20 — mVolt+ precedent study](../20-mvolt-plus-precedent-study/README.md)
