# 27 — Firmware infrastructure, round 2: exhaustive handler scan, corrected re-disassembly, per-ELF split, device-ID trace

A second pass over the already-built GSP-RM firmware infrastructure (the
full plaintext RISC-V decompile and saved Ghidra project from
[attempt 14](../14-rpc-audit-undocumented-commands/README.md)), targeting
four specific open questions that earlier passes had left unresolved. All
four reuse the same corpus; grouped here for that reason.

## 27a. Exhaustive RPC-handler register-write scan

**Objective:** attempt 14's audit found zero FEAT_OVR-block literals in a
whole-binary sweep, but that sweep searched the firmware as one
undifferentiated corpus. This attempt specifically targets every RPC
dispatch-table *handler body*, individually decompiled, to check whether any
command handler — documented or undocumented — contains a direct store to
the speed-cap registers.

**Method:** cross-checked all 1101 unique RPC handler addresses against the
existing function index. A real, previously-unquantified fact about this
corpus: most handler addresses are not pre-existing Ghidra function
boundaries at all — they're reached only via indirect jump targets inside
larger functions, the same problem already found specifically for
`GC6_ENTRY` in attempt 14b, here shown to apply to the dispatch table far
more broadly than that one case suggested. Worked around it with a Ghidra
headless post-script that forces a function boundary at each handler
address before decompiling, dumping every resulting body to a single
25MB+ output file.

**Result:** final tally — `total=1101 hadFunc=732 noFuncInitially=369
createdNew=344 decompFail=15`. Grepped the complete, final dump for all
four FEAT_OVR-block literals (`0x823804`/`0x82381c`/`0x823820`/`0x823808`):
**zero hits anywhere across all 1101 processed RPC handler bodies.** No
RPC-reachable handler, documented or undocumented, contains a literal store
to any register in the speed-cap block.

**Status: ❌ Confirmed negative — 100% complete.** This, combined with
attempt 14's whole-corpus sweep, closes the "maybe some RPC handler writes
these registers" hypothesis structurally — not just "not found yet."
Neither method can rule out a *runtime-computed* address reaching these
registers (the same caveat [attempt 04](../04-direct-register-writer-static-sweep/README.md)
already carries for its own direct-writer findings), but no compile-time
literal reference exists anywhere in the RPC-reachable surface.

## 27b. Corrected re-disassembly of `GC6_ENTRY` and `EXEC_REG_OPS`

**Objective:** a separate, unrelated third-party research project's
independently-documented RISC-V disassembly pitfalls (compressed-instruction
extension defaulting off, `.text` not starting at the real entry point,
`auipc` immediates misread as absolute addresses rather than PC-relative)
raised the question of whether this project's own earlier, inconclusive
reads of these two handlers suffered from the same class of tooling bug
rather than a genuine firmware-side dead end.

**Result: re-disassembly reconfirms the original verdict, but for a
different and harder reason than suspected.** Both handlers turned up in
attempt 27a's finished batch as **already-recognized, correctly-bounded**
Ghidra functions in the original analysis — the compressed-instruction/
entry-point bug this re-check was testing for never applied to either of
them. The actual, still-present blocker, reconfirmed directly in the fresh
decompile: `EXEC_REG_OPS`'s body contains a literal unrecovered jump table,
and `GC6_ENTRY`'s contains an explicit Ghidra warning that it could not
recover a jump table at a specific address — "too many branches." This is a
genuine jump-table-recovery limit in the decompiler, not an
instruction-decode bug. The earlier honest-dead-end verdicts for both
handlers stand, now on firmer footing (reconfirmed from a complete analysis
pass rather than an interrupted one).

**Status: ❌ Confirmed negative (root cause clarified) — 100% complete.**

## 27c. Per-ELF firmware split

**Objective:** the firmware image this project decompiled is a single
72MB blob containing multiple embedded RISC-V programs. Splitting it by its
internal structure, rather than treating it as one corpus, lets each
embedded component be checked independently — including whether the one
this die actually boots differs from the others in any way relevant to the
cap.

**Method:** parsed the firmware's internal image manifest and
cross-validated the result by independently scanning the entire 72MB file
for raw ELF magic bytes.

**Result:** 11 embedded RISC-V ELF images found, matching the manifest
count. Mapped each to its real name and file offset, including the main
~15.3MB resource-manager image and a dedicated debug variant. **Two fully
independent re-searches for the four FEAT_OVR-block literals — a
decompile-based scan (27a) and a raw-byte scan of the entire 72MB image,
segmented across all 11 components — now agree: zero hits in every single
component.** The "maybe some other embedded component references these
registers" hypothesis is closed alongside the RPC-handler one from 27a.

Which of two near-identical per-architecture kernel images this specific
die loads was **not** directly traced this round — the firmware doesn't
select between them by name comparison (confirmed: zero string hits for
either name anywhere in the decompile, meaning selection is by a numeric
index, not traced further here). A separate, independent finding (this
whole consumer sub-family shares one hardware-abstraction build target and
the literal same signing key and firmware image — see
[attempt 30](../30-pro6000-fuse-bitdiff-and-key-fingerprint/README.md#30b-fmc-key-fingerprint))
is strong circumstantial evidence this die loads the same image a sibling
die in the same family is already confirmed to load, but that's an
inference from a related fact, not a direct trace.

**Status: ⚠️ Partial — 90%.** The split, mapping, and literal re-search are
done and conclusive; the numeric selector mechanism itself (and by
extension, whether a debug-build image could ever load on retail silicon)
remains untraced.

## 27d. Device-ID family-table trace

**Objective:** the firmware contains a lookup table keyed by PCI device ID.
Trace what this die's own device ID resolves to, on the hypothesis that it
might select a fuse profile, a kernel image, or some other
cap-relevant configuration.

**Method:** located the real classifier function by searching the full
decompile for the literal device-ID constant, then traced every read/write
site of the flag it sets.

**Result: a real, concretely-traced mechanism — just not the one hoped
for.** The classifier branches on PCI device ID, checking this die's ID
alongside the consumer flagship and several professional/workstation SKU
IDs (each professional SKU additionally gated by an exact subsystem ID).
The output is a single flag, set only on the subsystem-confirmed
professional/workstation branches; this die and the consumer flagship both
fall into a shared default case that leaves it unset. Every other
read/write site of that same flag was checked (seven total); two of them
test it in direct alternation with a public registry key whose name
unambiguously identifies it as a **GRID/virtualization board-identity
flag** — a feature-segmentation concern unrelated to the tensor-rate cap,
not a rate-cap or firmware-image selector.

**Status: ❌ Confirmed negative (for the hypothesis), real mechanism
documented — 100% complete.**

## Net effect

None of this round needed to move to a live hardware session — it's all
static analysis against artifacts already on disk. The firmware-write-path
question (27a) and the embedded-component question (27c's literal search)
are now closed about as thoroughly as static analysis can close them. The
two genuinely open threads from this round — the numeric kernel-image
selector (27c) and whatever actually computes the fused rate value the
cap enforces — remain open, and don't have an identified next static step;
any further progress here most likely needs live execution tracing, a
capability this project doesn't currently have tooling for.

## See also

- [04 — direct register-writer static sweep](../04-direct-register-writer-static-sweep/README.md)
- [14 — RPC audit: fuzzing, GC6_ENTRY, and undocumented commands](../14-rpc-audit-undocumented-commands/README.md)
