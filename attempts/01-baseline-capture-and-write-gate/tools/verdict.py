"""
Mechanical "did it actually unlock" check -- separate from "did the boot
reach FINAL". Reaching FINAL only means the firmware ran to completion; it
says nothing about whether the speed-selector write took.

The only valid unlock judgement, per blackwell-gb20x-status.md §6 and
confirmed independently by gpu-unlock-blackwell's own tensor-math probe:

    SS0 == 0x88888888  AND  SS1 == 0x8   (exact match, not "nonzero")

Everything else -- a nonzero SS0/SS1 that isn't that exact pair, a FLR
having happened, the write test having been "attempted" -- is NOT evidence
of an unlock. baseline_registers.json's decoding_rules says this explicitly;
this module enforces it mechanically instead of leaving it to be
re-remembered in prose every run.

CAVEAT, stated plainly rather than guessed past: the exact text format the
loader.efi build prints register values in has not been verified against
this harness's actual first real capture. The regexes below are written to
be tolerant of common "NAME = 0xHEX" / "NAME: 0xHEX" / "NAME=0xHEX"
formattings, but the FIRST real run should be eyeballed by hand to confirm
they're actually matching -- see README.md "First run checklist".
"""
import re

REGISTER_PATTERNS = {
    "SS0": re.compile(r"\bSS0\b[^0-9a-fx]{0,10}(0x[0-9a-fA-F]+)", re.IGNORECASE),
    "SS1": re.compile(r"\bSS1\b[^0-9a-fx]{0,10}(0x[0-9a-fA-F]+)", re.IGNORECASE),
    "FEAT_READOUT_1": re.compile(r"FEAT_READOUT_1[^0-9a-fx]{0,10}(0x[0-9a-fA-F]+)", re.IGNORECASE),
    "FEAT_OVR_PLM": re.compile(r"FEAT_OVR_PLM[^0-9a-fx]{0,10}(0x[0-9a-fA-F]+)", re.IGNORECASE),
}

UNLOCK_SS0 = 0x88888888
UNLOCK_SS1 = 0x8


def evaluate_run(sections) -> dict:
    """sections: either a dict of {section_name: text} (preferred -- e.g.
    {"loader.log": "...", "loader.log.0": "..."}) or a plain string (treated
    as one unnamed section, kept for backward compatibility).

    Evaluated per-section rather than one concatenated blob, with the
    source section recorded for anything matched: loader.log.0's exact
    semantics aren't documented anywhere upstream (an earlier attempt
    within the same boot? a rotated log?), and this is explicitly billed
    elsewhere as "ground truth, independent of your own reading" for what
    would be a major finding -- it needs a provenance trail, not just a
    first-match-wins scan across sections of unclear relationship."""
    if isinstance(sections, str):
        sections = {"(unnamed)": sections}

    found = {}  # register name -> {"value": int, "section": str}
    for section_name, text in sections.items():
        for reg_name, pattern in REGISTER_PATTERNS.items():
            if reg_name in found:
                continue  # first section wins for a given register, but recorded
            m = pattern.search(text)
            if m:
                try:
                    found[reg_name] = {"value": int(m.group(1), 16), "section": section_name}
                except ValueError:
                    pass

    ss0_info = found.get("SS0")
    ss1_info = found.get("SS1")
    ss0 = ss0_info["value"] if ss0_info else None
    ss1 = ss1_info["value"] if ss1_info else None
    unlock_detected = (ss0 == UNLOCK_SS0 and ss1 == UNLOCK_SS1) if (ss0 is not None and ss1 is not None) else False
    cross_section = bool(ss0_info and ss1_info and ss0_info["section"] != ss1_info["section"])

    return {
        "registers_found": {k: {"hex": hex(v["value"]), "section": v["section"]} for k, v in found.items()},
        "ss0_hex": hex(ss0) if ss0 is not None else None,
        "ss1_hex": hex(ss1) if ss1 is not None else None,
        "ss0_ss1_from_same_section": (not cross_section) if (ss0_info and ss1_info) else None,
        "unlock_detected_by_register": unlock_detected,
        "note": (
            "No SS0/SS1 found in this capture -- either the payload doesn't log them, "
            "the regex didn't match this build's actual format (check by hand), or the "
            "capture didn't reach the point where they're read."
            if ss0 is None or ss1 is None else
            "SS0/SS1 found but do NOT equal the unlock pair -- this is expected shadow-state, "
            "not evidence of anything, per baseline_registers.json's decoding_rules."
            if not unlock_detected else
            "SS0==0x88888888 and SS1==0x8 -- matches the unlock criterion exactly. "
            + ("WARNING: these came from DIFFERENT log sections (see registers_found) -- "
               "verify by hand that this isn't an artifact of comparing two unrelated "
               "capture sections before trusting it. " if cross_section else "")
            + "This would be a major finding either way. Confirm with an independent "
              "tensor-math measurement in the booted OS before trusting a register read "
              "alone -- see reference/gpu-unlock-blackwell/probe/mma_rate.py."
        ),
    }
