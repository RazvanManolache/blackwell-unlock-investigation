# 30 — RTX PRO 6000 fuse bit-diff and FMC key fingerprint

## 30a. Bit-level fuse differential

**Objective:** this project has long known, at the summary level, that the
uncapped professional-tier card and the capped consumer card are the same
silicon with a different fuse configuration. Pin that down to the exact
bit level: which specific fused bits actually differ, and do they match
the already-named rate-limiting fuses this project has measured
elsewhere.

**Method:** this project maintains a documented decoding rule for the
fuse-mirror readout register central to this comparison: its nine
rate-limit fields sit at **non-contiguous** bit offsets (0, 3, 7, 10, 13,
16, 19, 22, 28 — three bits each, with deliberate one-bit and three-bit
gaps between some of them), explicitly flagged elsewhere in this project as
easy to mis-decode with a naive "nine contiguous fields" assumption.
Decoded an already-captured uncapped RTX PRO 6000 reading and the
already-captured capped consumer-die baseline using the correct,
non-contiguous offsets.

**Result:** the consumer baseline decodes to four fields reading the
capped value and five reading the uncapped value; the PRO 6000 reading
decodes to all nine fields at the uncapped value. The four fields that
flip are, bit-for-bit:

| Field | Bits | Capped (consumer) | PRO 6000 |
|---|---:|---:|---:|
| `fmla32` | 9:7 | 3 | 0 |
| `imla1` | 15:13 | 3 | 0 |
| `imla2` | 18:16 | 3 | 0 |
| `imla4` | 24:22 | 3 | 0 |

(The remaining five fields — `imla0`, `fmla16`, `ffma`, `imla3`, and the
double-precision field — read 0 on both parts; they were never capped to
begin with.)

**Methodological note worth recording:** the obvious first approach —
mapping these nine bit positions directly against the parameter order of
the public RPC getter struct that reports the same values — gives the
**wrong** answer. That struct's field order is a software/API convenience
ordering; it has no reason to match the hardware's own fuse bit-packing
order, and naively zipping the two together decodes the stock value to a
completely different, internally-inconsistent set of "capped" fields. The
correct mapping above was cross-validated by two independent methods: a
static decode of the actual firmware function that fills this register
from the fuse bank, and a live register read on real hardware using a
different, purpose-built tool — both agree exactly on every field.

**Status: 🔬 Analysis complete — 100% complete as a static finding.** This is a real, concrete,
bit-level identification of the exact fused bits the whole cap boils down
to on this register. It is **not** independently writable, and does not
by itself open a new path to flip it — `FEAT_OVR_PLM`'s hardware
write-gate (see [attempt 01](../01-baseline-capture-and-write-gate/README.md))
governs the live selector registers this readout only mirrors, not this
fuse-mirror register itself. What this attempt adds is precision, not a
new write path: the project now knows exactly which fuse bits are being
asked about, not just that "some fuse bits" differ.

## 30b. FMC key fingerprint

**Objective:** check whether this specific die has its own distinct
signing key/firmware image, or shares one with a broader family — relevant
because a per-die-unique firmware or key would be a different kind of
target than a shared one.

**Method:** searched the open-source driver tree for this die's own
firmware-signing archive by name.

**Result: stronger than expected.** This die has **no firmware-signing
archive of its own anywhere in the source** — only a handful of other
dies/families each have their own. This die's hardware-abstraction
specification file groups it together with four sibling consumer dies
under one shared build target, consistent with it dispatching entirely to
a sibling die's implementation. The clinching detail: that sibling's own
firmware files are named generically by family in NVIDIA's own build
system — not by the specific die's model number — confirming this is
NVIDIA's own intentional grouping, not an inference from absence. Extracted
the public key from the shared firmware package and independently
recomputed its fingerprint: it matches, byte for byte, a fingerprint
already on file for a different die in the same family, reproduced here
from this project's own source copy rather than taken on trust.

**So this die isn't just "in the same key family" as its sibling — it has
no distinct key or firmware image at all; at runtime it loads and verifies
against the literal same artifact the sibling die does.** This closes off
any residual hope that a different firmware build for this specific die
could ever be a lever — there is nothing SKU-specific to find or swap; the
differentiation is entirely in fuses (see 30a above), never in firmware.

**Status: 🔬 Analysis complete — 100% complete as a static finding.**

## See also

- [05 — Closed Ampere/Ada techniques](../05-closed-ampere-ada-techniques/README.md) (background on the FMC signing scheme this key belongs to)
- [26 — Empirical SASS-level tensor-rate measurement](../26-sm120-empirical-benchmark/README.md) (an independent, execution-level measurement of the effect this fuse differential produces)
