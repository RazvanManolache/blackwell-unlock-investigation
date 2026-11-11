# 14 — GSP-RM RPC audit: fuzzing, GC6_ENTRY, and undocumented commands

GB20x's GSP-RM main-body firmware ships in plaintext RISC-V (unlike the
encrypted FMC stage ahead of it). This made a direct RPC-level audit of its
~1038 control-handler dispatch table possible, in the spirit of the original
Ampere/Ada booter-overflow exploit but targeting a different component. Six
checklist threads came out of this audit; all are grouped here because
they share the same tooling and discipline.

## 14a. RPC fuzzing campaign

**Objective:** build a real, working fuzz harness for GSP-RM's RPC control
handlers and use it to probe command reachability and size-sensitivity.

**Method:** `fuzz_harness.c` was built from scratch as a standalone tool
(deliberately not linked against an unrelated internal reference tool
already used on a different shared machine with its own separate
authorization policy — reused only that tool's already-hardware-proven
minimal ioctl pattern). Hard-codes a "one command, stop after every attempt,
operator confirms before continuing" discipline directly into the control
flow — no flag disables it, no whole-list auto-loop exists.

A compiler-verified command list was generated from the open driver source
tree closest to the machine's actual running version: 658 of 1370 raw
`NV2080/NV0000/NV0080` commands matched to a real header struct by naming
convention, then given an exact `sizeof()` via a tiny host-side probe binary
that never touches `/dev/nvidia*`.

**Result: 56 real RPC attempts across 9 commands, zero incidents.** An
independent dmesg/Xid + `nvidia-smi` health check ran after every single
attempt, hard-wired to abort the whole sequence on any sign of trouble —
never triggered. 4 of 8 shortlisted commands succeeded outright; 2 hit a
permission wall; one returned a third, distinct status later root-caused as
a per-call rate throttle (see 14e); one (`CE_GET_CE_PCE_MASK`) rejected
every size tried — later resolved (see below) as a bad input value, not a
routing/privilege issue: `ceEngineType=0` simply isn't a valid enum value;
`ceEngineType=9` (`COPY0`) succeeds immediately with no channel/engine
routing needed at all.

**Status: ⚠️ Partial — 85%.** The harness and methodology are fully proven;
most of the full undocumented-command surface (see 14e) remains untested by
choice, not by failure.

## 14b. GC6_ENTRY size mismatch

**Objective:** chase a 12-vs-20-byte size discrepancy for
`NV2080_CTRL_CMD_GC6_ENTRY` (`0x2080270D`) found while cross-referencing the
firmware's own 32-byte-stride RPC dispatch table against all known compiler-
verified command sizes.

**Result:** of 500 compiler-verified commands checked, `GC6_ENTRY` is the
**sole mismatch** found (table declares 20 bytes, the real compiled header
struct is 12). Live-tested with real go-ahead, one size variant at a time:
`size=12` (header size) → `NV_ERR_INSUFFICIENT_PERMISSIONS`; `size=0` and
`size=11` → `NV_ERR_INVALID_ARGUMENT`. Clean, non-crash, Xid-free throughout
— a hard size≥12 gate followed by a permission gate this project's basic
client can't pass, so the actual 12-vs-20-byte question is never reached
through this path.

Ghidra decompilation of the handler (after a full, tuned whole-program
analysis pass) showed `callers=0` persists even with complete analysis — the
function is reached only via an indirect jump table that can't be traced by
normal control-flow analysis. The function itself turned out to be generic
debug/log/assert infrastructure, not GC6-specific logic; the real
field-parsing code sits behind at least two further layers of unresolved
indirection.

**Separately:** `subdeviceCtrlCmdGc6Entry_IMPL`'s real implementation was
read directly and found to call the genuine `gpuGc6Entry()` hardware
power-state transition — not a safe desktop no-op. Firing this command live
was deliberately held for the GPU owner's own explicit go-ahead; see
[attempt 14c](#14c-export-table-patch) for the related flag-patch
preparation, and the root README's safety section.

**Status: ⚠️ Partial — 80%.** The size mismatch is real and confirmed; the
question of whether it's exploitable is gated behind unresolved firmware
indirection and (separately) a live call this project chose not to make
without authorization.

## 14c. Export-table flag patch

**Objective:** the 462-of-480-undocumented-commands "structurally
unreachable" verdict rests on a host-side export table this project
compiles itself. Test whether flipping one command's flags to `0x48`
(`ROUTE_TO_PHYSICAL|NON_PRIVILEGED`) in a local driver build, then
rebuilding and retesting, actually changes reachability.

**Result:** `CE_GET_CE_PCE_MASK` was resolved separately (bad input value,
see 14a) — no patch needed there. `GC6_ENTRY`'s real implementation (see
14b) was found to be a genuine hardware power-state transition, not a safe
stub — flagged for the GPU owner's own go-ahead before ever sending the
live call. The flag-patch-and-rebuild half is safe/reversible and could be
prepared in advance, but wasn't done speculatively since it's only useful
as a precursor to a call that isn't yet authorized.

**Status: ⚠️ Partial — 60%.** One of two targeted commands resolved by a
different route; the other's patch is unbuilt pending authorization for the
live call it would enable.

## 14d. Kernel-privileged RM client

**Objective:** test whether a kernel-space RM client (as opposed to
userspace, even as root) is exempt from the `INSUFFICIENT_PERMISSIONS` gates
seen in 14b — which would mean writing a brand-new kernel module calling
undocumented internal driver symbols: a genuinely higher risk tier (a bug
there risks a full kernel panic, not just a recoverable GPU-level failure).

**Result: resolved entirely from source, without building anything.** Read
`rmControlValidateClientPrivilegeAccess` directly: kernel-originated calls
bypass the `PRIVILEGED` check entirely (per the code's own comment), and a
separate check requires genuine `RS_PRIV_LEVEL_KERNEL` (not just root) for
any command lacking the `NON_PRIVILEGED`/`PRIVILEGED`/`INTERNAL` flags.
Confirmed every tool used throughout this project runs at
`RS_PRIV_LEVEL_USER_ROOT` only — never enough for the second check.
`GC6_ENTRY` falls under this kernel-only rule, independently explaining its
unreachability.

**Important architectural caveat, confirmed, not assumed:** this is a
host-side *software* API gate, entirely separate from the GPU's own hardware
PL0–PL3 wall. A kernel-privileged client would unlock the ability to *issue
more RM_CONTROL commands* — it would **not** bypass the hardware gate this
project actually cares about, since the host CPU is always PL0 regardless of
kernel/user mode. Given that architecturally-bounded benefit, actually
building the kernel module was judged not worth the risk tier.

**Status: ❌ Confirmed, from source — 90%.** The question is fully answered;
10% withheld because the module itself was never built/tested live (a
deliberate choice given the bounded benefit, not an open gap).

## 14e. Flags decode + ioctl trace

**Objective:** decode the remaining reachable-command flags statically, and
separately, passively observe what an ordinary tool actually sends at the
RPC/ioctl layer (not just what's theoretically reachable by source
analysis).

**Method:** decoded the host-side export table's real `flags` field
(`RMCTRL_FLAGS_*`) against every command already live-tested: all 3 that
succeeded share the identical `flags=0x48`
(`ROUTE_TO_PHYSICAL|NON_PRIVILEGED`) — a real, ground-truth reachability
signature. Of 480 undocumented commands, only 18 have *any* host export-table
entry at all.

Separately, wrote a small `LD_PRELOAD` shim intercepting every `ioctl()`
call a process makes, decoding `NV_ESC_RM_CONTROL` (ioctl low byte `0x2A`)
calls and printing the real command ID and status. Run against a completely
ordinary `nvidia-smi -q`.

**Result: a major methodology correction.** 83 distinct RM_CONTROL commands
fired during routine tool use; 32 of these are real NV2080-class commands
never tested or statically characterized before. Checking these against the
full driver source tree (case-insensitive, whole-tree, both the exact
matching version and a nearby one) found **24 of them have zero textual
trace anywhere in the public driver repository at all** — yet they return
`status=0` with real payloads up to 67 KB on this exact machine, exact
matching driver version.

**Stated carefully:** this doesn't imply anything sinister (large
first-party telemetry payloads succeeding quietly is mundane, expected
behavior) — but it's a real correction to this project's own earlier
methodology, which had been treating "absent from public source" as
reasonably strong evidence a command is unreachable. It's weaker evidence
than that. This directly weakens (without invalidating) the earlier
462-of-480-unreachable claim from the static flags analysis above — those
were never live-tested, only reasoned from source absence.

Regenerated the full 18-candidate `flags=0x48` list with real names (all
turned out to be clean public SDK functions — "undocumented" was mostly a
gap in this project's own extraction tool, not hidden commands). Live-tested
9 GET-only ones: 2 clean success, 7 sensible rejections (hardware-absent or
needs-nonzero-selector). 5 setters were deliberately left untested pending a
more careful revert-discipline pass, except one
(`BusSetPcieLtrLatency`) which was tested as a safe
read-write-same-value-reread round trip and confirmed to work cleanly.

**Status: ⚠️ Partial — 95%.** Near-complete; the 5 untested setters and the
weakened-but-not-fully-redone 462-command claim are the remaining gaps.

## 14f. strace surface

**Objective:** catalog every host file `modprobe nvidia` opens at init, as
same-class override candidates to the mVolt+ disk-persisted-blob pattern
(see [attempt 20](../20-mvolt-plus-precedent-study/README.md)).

**Result: a real negative for the technique, not the question.** A plain
`strace -f` on `modprobe nvidia` showed only ordinary userspace modprobe
bookkeeping (config files, `modules.dep`, the `.ko` itself) — zero
firmware/GSP-RM-related activity. Structural reason: `request_firmware()` is
a kernel-internal call, invisible to a userspace strace of modprobe. This
specific technique doesn't reach anything new; a kernel-side trace
(`ftrace`/`kprobe` on `request_firmware`) or a system-wide trace of `udevd`
would be needed instead.

**Status: ❌ Confirmed negative (for the technique) — 100%.**

## Tools

- [`tools/fuzz_harness.c`](tools/fuzz_harness.c) — the RPC fuzzing harness (14a, 14b).
- [`tools/extract_pairs.py`](tools/extract_pairs.py) + [`tools/sizeof_probe.c`](tools/sizeof_probe.c) — the compiler-verified command-size extraction pipeline.
- [`tools/command_sizes.tsv`](tools/command_sizes.tsv) — the resulting 658-command verified-size table.
- [`tools/ioctl_trace.c`](tools/ioctl_trace.c) — the `LD_PRELOAD` ioctl tracer (14e).

## See also

- [04 — direct register-writer static sweep](../04-direct-register-writer-static-sweep/README.md) (an earlier, narrower version of this same full-firmware analysis approach)
- [12 — per-SM register channel](../12-per-sm-register-channel/README.md)
