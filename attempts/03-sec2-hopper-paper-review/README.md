# 03 — Does the SEC2-on-Blackwell paper reopen the old exploit surface?

## Objective

arXiv paper 2507.02770 claims GSP can be initialized via SEC2 on Blackwell.
The original Ampere/Ada unlock exploited a specific vulnerability in SEC2's
signed "Booter" stage (a leaked test key enabling a stack-overflow-driven
ROP chain into a privileged write). If Blackwell still routes through a
similar SEC2 stage, that old technique family might still apply.

## Hypothesis

Going in: uncertain whether this reopens anything, hence reading the full
paper rather than a search summary before concluding either way.

## Method

Read arXiv 2507.02770 in full, particularly §7.1 (the boot-chain
description), rather than trusting a secondhand summary.

## Result

**Does not reopen anything.** Their test platform is Hopper (H100) only —
the Blackwell claim is an inference from driver source, not something
observed on real Blackwell hardware. More importantly, the SEC2 they
describe on Hopper/Blackwell is a redesigned, GPU-confidential-computing
-specific security coprocessor (decrypt/verify/attestation/scrubbing),
architecturally distinct from the Ampere/Ada "booter" SEC2 the original
exploit targeted. No indication of that same weakness. GSP's actual boot
chain per the paper's own §7.1 is entirely FSP-driven; the SEC2
initialization path they describe is a separate, still-opaque alternate
path, not a replacement for the FSP-driven one this project's own
architecture notes already assumed.

## Status

**❌ Confirmed negative — 100% complete.** Closed as investigated, not
assumed.

## See also

- [05 — closed Ampere/Ada techniques](../05-closed-ampere-ada-techniques/README.md)
