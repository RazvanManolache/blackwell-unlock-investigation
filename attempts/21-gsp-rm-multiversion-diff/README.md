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

## Method (planned, not yet run)

Pull Blackwell-generation GSP-RM blobs from several other driver releases
using the same extraction approach already working for firmware/signature
pairs elsewhere in this project. Start cheap: binary-diff the fwimage blobs
directly, and only decompile the specific already-known function regions
(the ACR cluster's shared config function, the SmSpeedSelect read site) in
any version that differs there at all — not a full re-decompile per
version.

A clean "identical in every version checked" result would still be a
useful, reportable negative: it would confirm the hardware fuse gate is
doing 100% of the work and no amount of driver-version shopping helps. Any
version-to-version drift in that specific region would be the first
version-dependent lead in the whole project.

## Status

**⏸️ Not started — 0%.** Static/binary-diff work, no hardware required
until (if ever) a live difference needs confirming.

## See also

- [10 — registry override: SmSpeedSelect](../10-registry-override-smspeedselect/README.md)
- [11 — registry key sweep / ACR cluster](../11-registry-key-sweep-acr-cluster/README.md)
- [20 — mVolt+ precedent study](../20-mvolt-plus-precedent-study/README.md)
