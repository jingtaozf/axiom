#!/usr/bin/env python3
"""PostToolUse hook (Edit|Write|MultiEdit): sanity-check an edited pamphlet.

Marker recognition mirrors books/tanglec.c exactly (verified against
the source): ``\\begin{chunk}{name}``, ``\\end{chunk}`` and
``\\getchunk{name}`` count ONLY at line start (column 0), and tanglec
has no notion of verbatim -- a column-0 marker inside a verbatim block
is live.  Mid-line mentions (grep patterns, prose) are data.

Five checks, each grounded in a failure actually hit in this repo:

  1. chunk balance      -- begin/end pairing per tanglec's model.
  2. ghost getchunk     -- \\getchunk{X} with no \\begin{chunk}{X} in
                           the same file expands to NOTHING silently
                           (chunk names are file-local; the top-level
                           Makefile.pamphlet shipped ghost 'literate
                           commands' references for years).
  3. prose before chunk -- Rule 0: a chunk introduced by this edit
                           should not open directly after \\end{chunk}
                           with no explanatory prose between.
  4. chunk regression   -- every chunk this edit touched must extract
                           via books/tanglec and be non-empty (code
                           shown as an illustration looks like a chunk
                           but tangles to nothing -- the bookvol10 BSD
                           trap).
  5. Makefile pair      -- editing Makefile.pamphlet: chunk '*' must
                           still reproduce the tracked Makefile
                           byte-identically, else remind to re-tangle.

Severity uses a DELTA model, not "does new_text mention a marker":
old text (pre-edit) and new text (post-edit) are each reduced to a
multiset of (kind, name) markers; only markers that are actually ADDED
or REMOVED by this edit count as "this edit touched structure".  A
chunk header that merely appears as unchanged surrounding context in
an Edit's new_string cancels against the same marker in old_string and
is not "touched" -- so it neither trips the prose-gap/tangle-regression
checks (#3/#4) nor forces a pre-existing balance break to hard.
Deletion of a marker (present in old_string, gone from new_string)
counts as touched too, so an edit that removes \\end{chunk} is not
silently downgraded to a note.

Write has no accessible "old" text, so its whole content reads as
"added" -- but a Write legitimately replaces the whole file, so its
balance result is always hard (never silently muted), and its ghost
findings are always notes (no old/new split is possible to attribute
a ghost to this edit specifically).  A Write on a huge pamphlet caps
checks #3/#4 to the first 10 touched chunks, so it doesn't flood the
agent with every legacy defect in the file (a 20-year-old tree carries
~14 legacy balance breaks and ~177 legacy ghosts, 2026-07-11 survey).

Findings plausibly caused by THIS edit are "hard" (exit 2, fed back to
the agent); defects that pre-date the edit are "notes", shown only
alongside hard findings. PostToolUse cannot block; exit 2 only feeds
back.

Only pamphlets inside CLAUDE_PROJECT_DIR are in scope -- a personal
study pamphlet elsewhere (e.g. under ~/projects/xjt-doc) is silently
ignored, same root-scoping block-generated-edit.sh applies to Edit.

(Named .sh, run by python via shebang: the globally-installed
literate-agent plugin would false-block a .py file in this repo.)
"""
import json
import os
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

# tanglec.c semantics: line-start anchored (MULTILINE for fragments)
BEGIN_RE = re.compile(r"^\\begin\{chunk\}\{([^}]*)\}", re.M)
END_RE = re.compile(r"^\\end\{chunk\}", re.M)
GET_RE = re.compile(r"^\\getchunk\{([^}]*)\}", re.M)

TOUCHED_CAP = 10


def _marker_counts(text: str) -> Counter:
    """Multiset of (kind, name) markers in `text`."""
    c = Counter()
    for name in BEGIN_RE.findall(text):
        c[("begin", name)] += 1
    for _ in END_RE.findall(text):
        c[("end", None)] += 1
    for name in GET_RE.findall(text):
        c[("get", name)] += 1
    return c


def _edit_delta(tool_name: str, tool_input: dict):
    """(old_text, new_text) this edit replaces, per tool shape.

    Write has no accessible "old" text -- the caller treats that case
    specially (balance always hard, ghosts always notes) rather than
    pretending old_text is known.
    """
    if tool_name == "Write":
        return "", tool_input.get("content") or ""
    if tool_name == "MultiEdit":
        edits = tool_input.get("edits") or []
        old = "\n".join(e.get("old_string") or "" for e in edits)
        new = "\n".join(e.get("new_string") or "" for e in edits)
        return old, new
    return tool_input.get("old_string") or "", tool_input.get("new_string") or ""


def main() -> int:
    try:
        payload = json.loads(sys.stdin.read())
    except Exception:
        return 0
    tool_name = payload.get("tool_name")
    if tool_name not in {"Edit", "Write", "MultiEdit"}:
        return 0
    tool_input = payload.get("tool_input") or {}
    fp = tool_input.get("file_path") or ""
    if not fp.endswith(".pamphlet"):
        return 0

    root = Path(os.environ.get("CLAUDE_PROJECT_DIR") or os.getcwd()).resolve()
    f = Path(fp)
    if not f.is_absolute():
        f = root / f
    f = f.resolve()
    try:
        rel_self = f.relative_to(root)
    except ValueError:
        return 0  # outside this repo: not our scope
    if not f.exists():
        return 0

    tanglec = root / "books" / "tanglec"
    text = f.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()

    hard = []    # findings this edit plausibly caused -> exit 2
    notes = []   # pre-existing defects, shown only alongside hard ones

    old_text, new_text = _edit_delta(tool_name, tool_input)
    old_counts = _marker_counts(old_text)
    new_counts = _marker_counts(new_text)
    added = new_counts - old_counts      # Counter subtraction keeps only >0
    removed = old_counts - new_counts
    touched = {name for (kind, name) in added if kind == "begin"}
    new_refs = {name for (kind, name) in added if kind == "get"}
    structure_changed = bool(added) or bool(removed)
    balance_is_hard = structure_changed or tool_name == "Write"

    # cap checks #3/#4 so a Write of a huge legacy pamphlet doesn't flood
    # every edit with pre-existing defects across the whole file.
    touched_list = sorted(touched)
    if len(touched_list) > TOUCHED_CAP:
        notes.append(
            f"({tool_name} touched {len(touched_list)} chunks; verified "
            f"first {TOUCHED_CAP})")
        touched_list = touched_list[:TOUCHED_CAP]
    touched_capped = set(touched_list)

    # 1. balance + nesting, whole file, tanglec's model
    depth, first_bad = 0, None
    for i, line in enumerate(lines, 1):
        if BEGIN_RE.match(line):
            depth += 1
            if depth > 1 and first_bad is None:
                first_bad = (f"line {i}: \\begin{{chunk}} while previous "
                             "chunk still open")
            depth = min(depth, 1)
        elif END_RE.match(line):
            depth -= 1
            if depth < 0 and first_bad is None:
                first_bad = f"line {i}: \\end{{chunk}} without matching begin"
            depth = max(depth, 0)
    if depth != 0 and first_bad is None:
        first_bad = "file ends inside an open chunk (missing \\end{chunk})"
    if first_bad:
        (hard if balance_is_hard else notes).append(
            "chunk balance: " + first_bad)

    # 2. ghost getchunk -- new ones are hard; a Write can't distinguish
    #    new from pre-existing, so all its ghosts stay notes.
    defined = set(BEGIN_RE.findall(text))
    all_ghosts = set(GET_RE.findall(text)) - defined
    if tool_name == "Write":
        new_ghosts, old_ghosts = [], sorted(all_ghosts)
    else:
        new_ghosts = sorted(all_ghosts & new_refs)
        old_ghosts = sorted(all_ghosts - new_refs)
    if new_ghosts:
        hard.append(
            "ghost getchunk introduced by this edit (expands to NOTHING "
            "silently): " + ", ".join(new_ghosts))
    if old_ghosts:
        notes.append(
            f"{len(old_ghosts)} pre-existing ghost getchunk(s): "
            + ", ".join(old_ghosts[:5])
            + (" ..." if len(old_ghosts) > 5 else ""))

    # 3. prose-before-chunk for touched chunks only (capped)
    prev_nonblank = ""
    for i, line in enumerate(lines, 1):
        m = BEGIN_RE.match(line)
        if m and m.group(1) in touched_capped and END_RE.match(prev_nonblank):
            hard.append(
                f"line {i}: chunk '{m.group(1)}' opens right after "
                "\\end{chunk} with no prose between -- Rule 0 wants the "
                "why-text first")
        if line.strip():
            prev_nonblank = line

    # 4. chunk regression for touched chunks (capped)
    if tanglec.exists():
        for name in touched_list:
            if name not in defined:
                continue
            try:
                r = subprocess.run([str(tanglec), str(f), name],
                                   capture_output=True, text=True, timeout=60)
            except (subprocess.TimeoutExpired, OSError) as exc:
                notes.append(
                    f"tanglec check skipped for chunk '{name}' "
                    f"({type(exc).__name__}: {exc})")
                continue
            if r.returncode != 0:
                hard.append(f"tanglec failed on chunk '{name}': "
                            + (r.stderr.strip()[:160] or "nonzero exit"))
            elif not r.stdout.strip():
                hard.append(
                    f"chunk '{name}' tangles to EMPTY output -- is the code "
                    "an illustration (verbatim) instead of a real chunk?")

    # 5. Makefile pair stays byte-identical
    makefile = root / "Makefile"
    if str(rel_self) == "Makefile.pamphlet" and tanglec.exists() and makefile.exists():
        try:
            r = subprocess.run([str(tanglec), str(f), "*"],
                               capture_output=True, text=True, timeout=60)
        except (subprocess.TimeoutExpired, OSError) as exc:
            notes.append(f"Makefile pair check skipped ({type(exc).__name__}: {exc})")
        else:
            if r.stdout != makefile.read_text():
                hard.append(
                    "Makefile.pamphlet now diverges from Makefile -- "
                    "regenerate with: ./books/tanglec Makefile.pamphlet '*' "
                    "> Makefile")

    if hard:
        msgs = hard + notes
        sys.stderr.write("pamphlet-check [" + os.path.basename(fp) + "]:\n - "
                         + "\n - ".join(msgs) + "\n")
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
