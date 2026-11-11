"""
Shared checklist logic -- imported by both gu_pi.py and dashboard/server.py
so the progress formula can't drift between the two (it was previously
duplicated in both places; this is the single source of truth now).

Also provides the prompt-summarization that keeps checklist.json from
growing unboundedly in build_prompt()'s token cost the same way prior-run
reports already had to be capped: terminal-state items (resolved either
way) get collapsed to one line, since Claude doesn't need their full notes
re-read every cycle to act on them -- only non-terminal items need full
detail, since those are what's actually still being worked.
"""
import json

TERMINAL_STATUSES = ("ruled_out", "done", "blocked")
PARTIAL_STATUSES = ("promising", "in_progress")


def progress(checklist: dict) -> float:
    items = checklist.get("items", [])
    if not items:
        return 0.0
    score = sum(
        1.0 if it["status"] in TERMINAL_STATUSES
        else 0.5 if it["status"] in PARTIAL_STATUSES
        else 0.0
        for it in items
    )
    return round(100.0 * score / len(items), 1)


def summarize_for_prompt(checklist: dict) -> str:
    """Full detail for anything still open/in-progress; one line for
    anything resolved either way. Returns a formatted string, not JSON --
    this is what actually goes into the prompt, not checklist.json verbatim."""
    items = checklist.get("items", [])
    lines = [f"Progress: {progress(checklist)}%", ""]
    open_items = [it for it in items if it["status"] not in TERMINAL_STATUSES]
    terminal_items = [it for it in items if it["status"] in TERMINAL_STATUSES]

    lines.append("## Open / in-progress items (full detail)")
    for it in open_items:
        lines.append(f"- [{it['status']}] {it['id']}: {it['title']}")
        if it.get("notes"):
            lines.append(f"    notes: {it['notes']}")
        if it.get("source_citation"):
            lines.append(f"    source: {it['source_citation']}")

    lines.append("\n## Resolved items (one line each -- see checklist.json directly for full notes if needed)")
    for it in terminal_items:
        lines.append(f"- [{it['status']}] {it['id']}: {it['title']}")

    return "\n".join(lines)
