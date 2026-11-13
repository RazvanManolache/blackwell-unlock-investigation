# 29 — FSP host-driver code-smell audit and sanitizer fuzzing

## 29a. Static audit + sanitizer boundary testing

**Objective:** an independent researcher's separate, publicly-documented
project flagged six potential code-quality issues in NVIDIA's own
open-source FSP (Firmware Security Processor) host-driver code — places
where host-controlled input reaches the security-boundary packing logic
without obviously-complete bounds checking. Check whether the same six
patterns exist, unmodified, in this project's own tracked driver source,
and — going further than the original report — actually compile the real
functions standalone and boundary-test them rather than relying on reading
alone.

**Method, part 1 (static):** checked all six against the real
driver-source functions. Blackwell's own FSP driver file for this hardware
generation doesn't override any of the relevant functions, so whatever is
true in the shared Hopper-generation source is true on this die, unmodified.

**Result, part 1:** five of six confirmed present exactly as described —
an un-length-checked hash copy (unlike a neighboring signature/public-key
copy a few lines later, which *does* check); an admitted estimate (the
code's own comment says the exact reservation size can't be known, so it
uses an estimate) with no upper-bound check; a packet-send function with no
internal size cap, relying entirely on its caller to clamp the value first;
a sentinel value that's bit-for-bit ambiguous with a real address in
principle; and a register write that takes a host-supplied physical address
with only a non-null check, no value-range check, into a register whose own
name self-describes as insecure. One correction to the original report: a
field described there as guarded only by a bare debug-only assertion is, in
this project's own tracked source, actually guarded by a hard,
runtime-enforced check that returns a real error code — a real,
executed check, not a debug-only no-op.

**Method, part 2 (dynamic):** built a standalone C harness reproducing the
exact call shapes of the real functions, compiled under AddressSanitizer
and UndefinedBehaviorSanitizer, and fed each candidate deliberately
malformed/boundary inputs.

**Result, part 2:**

- **Two of six are real, sanitizer-confirmed memory-safety bugs**
  (heap-buffer-overflow reads) — and both are currently **unreachable by
  any live or attacker-controlled input path** in the present codebase.
  One only triggers if NVIDIA itself makes a specific build-time mistake in
  a compiled-in firmware hash blob, never from runtime input. The other's
  single real caller in this codebase always allocates a correctly-sized
  buffer before calling it, so the missing internal check is pure
  defense-in-depth absence today, not a live path — it would only matter if
  a different, future caller were ever added without the same care.
- One more is confirmed, by direct computation with real numbers, to
  silently overflow by several hundred times with zero assertion — but
  again, host-internal-input only, not something externally supplied.
- One (the sentinel value) is a confirmed logical ambiguity only, not
  independently exploitable — producing that exact sentinel value by
  accident isn't a realistic outcome of any normal allocation.
- One (the insecure-register write) isn't sanitizer-testable at all — its
  only effect is a hardware register write, not a host memory operation a
  sanitizer can observe. Already fully characterized by the static reading
  alone: a missing *value*-range check before a hardware write isn't a
  memory-safety bug in the sense a sanitizer detects.
- The corrected item (the hard-enforced check) was re-confirmed safe by
  actually executing it against both a normal and a deliberately-wrapped
  input — the real guard cleanly rejects the wrapped case.

**Status: 🔬 Analysis complete, no live-exploitable bug found — 100%
complete as an audit.** This found two real, reproducible memory-safety
bugs in NVIDIA's own open-source driver code — not nothing — but neither
has a path from anything a host process or external input currently
controls. Nothing here warrants escalation or a live test; it's a
complete, honest characterization of host-driver code quality in this
specific area, not a lead toward the tensor-rate cap.

## 29b. Version-drift check

**Objective:** one of the six findings above (the hard-enforced check that
corrected the original report) raised the question of whether this is a
genuine difference between driver versions — i.e., did NVIDIA harden this
specific check at some point, and might the other five similarly have
shifted across versions?

**Method:** diffed the relevant source file across three real driver
versions spanning a meaningful span of releases (CRLF-normalized first — a
known friction point when diffing files that have passed through different
toolchains). Then, to answer "which version introduced the hardening"
directly rather than just checking recent versions, fetched the same file
across the full public release history back to the earliest version
available.

**Result: a clean negative on the version-drift hypothesis.** All six
regions — every single one of the six findings from 29a — are
byte-identical across all three recent versions checked: same literal match
counts, zero diff lines anywhere near any of them. The file is genuinely
active elsewhere in that same span (new code was added nearby, a
telemetry/event-bus rework of an adjacent failure path, a new getter
function, a format-string widening), so this isn't a stale or frozen file —
the six flagged regions specifically just haven't moved. Pushing further
back: the function containing the hard-enforced check doesn't exist at all
in the three earliest available public driver versions (only lower-level
helpers exist there, under different names); it first appears, already
with the hard-enforced check in place, several versions later. **There is
no version in the entire available public history where this check was
weaker.** The "bare debug-only assert" description in the original report
most likely describes a different internal build never published, or is
an imprecision in that write-up — not a real difference in any version
this project can actually inspect.

**Status: ❌ Confirmed negative — 100% complete.** The FSP code-packing
surface this project examined is stable, not converging or diverging,
across the entire available history. Nothing here needed a live hardware
session; fully resolved from publicly available source.

## Tools

The sanitizer-fuzzing harness is a short, self-contained C file reproducing
real call shapes from the driver source, built with
`gcc -fsanitize=address,undefined -O0`; not included in this repository
since it's trivially reconstructible from the method description against
publicly available NVIDIA driver source.

## See also

- [24 — FSP/COT scratch-register boot-state timeline](../24-fsp-cot-scratch-registers/README.md)
- [25 — FMC/FSP parser heap-size field](../25-fmc-parser-heap-size/README.md)
