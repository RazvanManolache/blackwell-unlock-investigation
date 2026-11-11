# 18 — VBIOS devinit table

## 18a. Devinit override (parked)

**Objective:** whether Blackwell's VBIOS still runs host-executed devinit
scripts before GSP takes over, and whether an init-table *edit* (not a full
firmware swap) could set `FEAT_OVR` before the PL3 gate engages.

**Why this is parked, not attempted:** VBIOS modification is a different
risk tier than anything else active in this project — a bad edit can
hard-brick the card, and no recovery path has been established or tested
here. Even the research premise (does Blackwell still execute host-visible
devinit opcode scripts at all, given how much of the boot chain has moved
into FSP/GSP firmware) is unconfirmed — this may be a dead premise on GB20x.

**Status: 🚫 Parked, not attempted — 0%.** If ever greenlit: research-only
first (does a devinit table exist and get executed pre-GSP on this die at
all — see 18b below, which already answers half of this), with no actual
flash attempted until that's answered and separately approved.

## 18b. Read-only premise check

**Objective:** the safe half of 18a — confirm or deny, without writing
anything, whether a host-visible pre-GSP init table exists on this die at
all.

**Method:** dumped the VBIOS read-only via the standard sysfs ROM mechanism
(`echo 1 > .../rom`, `dd` the file, `echo 0 > .../rom`).

**Result:** confirmed a real 158 KiB Option ROM matching this exact device
ID. While already in hand, also grepped the whole dump for all 667 known
GSP-RM registry key name strings (see
[attempt 10](../10-registry-override-smspeedselect/README.md)) — **zero
matches**, expected, since these are firmware-internal strings, not VBIOS
devinit table strings. Did not go further into devinit table structure
parsing this pass.

**Status: ❌ Confirmed negative — 100%.** The ROM dump itself is clean and
read-only; no edits made, none planned without separate approval.

## See also

- [17 — firmware signature oracle (parked)](../17-firmware-signature-oracle-parked/README.md)
- [Panchovix/nvidia-vbios-vix-editor](https://github.com/Panchovix/nvidia-vbios-vix-editor) — tooling this avenue would use if ever approved
