#!/usr/bin/env python3
"""PreToolUse hook (Bash): close the shell bypass around pamphlet-first.

The Edit/Write matcher cannot see `sed -i`, `tee`, `cp`, `mv`, or
shell redirections.  This hook rejects Bash commands that would write
into the build-generated trees (int/ obj/ mnt/), clobber the tangled
top-level Makefile, or clobber any file that has a sibling .pamphlet
(the same three protected-path classes block-generated-edit.sh
enforces for Edit/Write/MultiEdit).

One sanctioned write exists: regenerating the Makefile from its
pamphlet via books/tanglec -- that IS the pamphlet-first workflow.

Analysis is per shell-segment (split on newlines and `;` `&&` `||`
`|`), not over the whole command string:

  * A whole-command scan lets an unrelated write on one line "borrow"
    a protected path mentioned only in cross-line noise (`[^;|&]`
    matches newlines), and lets a `tanglec ... Makefile.pamphlet`
    clause anywhere in the command sanction an unrelated `> Makefile`
    on a different line/clause -- a real bypass:
    `echo pwned > Makefile; ./books/tanglec Makefile.pamphlet '*' > /dev/null`.
    Segmenting first, then requiring the sanction and the redirect to
    be in the SAME segment, closes both holes.
  * A bare `>`/`>>` redirect is checked against its target for every
    segment.  A write COMMAND (sed -i, tee, cp, mv, dd, rsync,
    install, perl -i) is checked against its DESTINATION argument only
    -- for cp/mv that is the last positional argument, so `cp
    mnt/x /tmp/y` (reads FROM mnt/, writes to /tmp/) is not blocked;
    generated-trees-readonly.md says read freely.

(Named .sh, run by python via shebang; see block-generated-edit.sh.)
"""
import json
import os
import re
import shlex
import sys
from pathlib import Path

GENERATED_TREES = {"int", "obj", "mnt"}
# tee/cp/mv/dd/rsync/install always write; sed/perl only write with -i.
WRITE_COMMANDS_ALWAYS = {"tee", "cp", "mv", "dd", "rsync", "install"}
WRITE_COMMANDS_INPLACE = {"sed", "perl"}

SEGMENT_SPLIT_RE = re.compile(r"\r?\n|\|\||&&|;|\|")
REDIRECT_RE = re.compile(r">{1,2}")
SANCTIONED_RETANGLE = re.compile(r"tanglec\s+(?:\./)?Makefile\.pamphlet")


def _quote_mask(segment: str):
    """Per-char bool: True if that position is inside a quoted string.

    Simple linear scan, no backslash-escape handling -- enough to stop
    treating a `>` inside a quoted grep/sed pattern as a real shell
    redirect (residual edge cases are acceptable for an advisory
    guard).
    """
    mask = []
    in_s = in_d = False
    for ch in segment:
        mask.append(in_s or in_d)
        if ch == "'" and not in_d:
            in_s = not in_s
        elif ch == '"' and not in_s:
            in_d = not in_d
    return mask


def _redirect_targets(segment: str):
    mask = _quote_mask(segment)
    targets = []
    for m in REDIRECT_RE.finditer(segment):
        if mask[m.start()]:
            continue
        rest = segment[m.end():].lstrip()
        if not rest:
            continue
        tok = rest.split()[0].strip("'\"")
        targets.append(tok)
    return targets


def _write_command_targets(segment: str):
    """Destination-path tokens for a write command in `segment`, or []."""
    try:
        tokens = shlex.split(segment)
    except ValueError:
        tokens = segment.split()
    i = 0
    while i < len(tokens) and re.match(r"^[A-Za-z_][A-Za-z0-9_]*=", tokens[i]):
        i += 1  # skip leading FOO=bar env assignments
    if i >= len(tokens):
        return []
    cmd = os.path.basename(tokens[i])
    args = tokens[i + 1:]

    if cmd in WRITE_COMMANDS_INPLACE:
        if not any(a == "-i" or a.startswith("-i") for a in args):
            return []  # sed/perl without -i only read
    elif cmd not in WRITE_COMMANDS_ALWAYS:
        return []

    positional = [a for a in args if not a.startswith("-")]
    if not positional:
        return []
    if cmd in {"cp", "mv", "dd", "rsync", "install"}:
        return positional[-1:]  # destination is the last positional arg
    return positional  # sed/tee/perl: every file arg is a write target


def _protected_target(tok: str, root: Path):
    if not tok or tok.startswith("-"):
        return False, ""
    p = Path(tok)
    if not p.is_absolute():
        p = root / p
    p = p.resolve()
    try:
        rel = p.relative_to(root)
    except ValueError:
        return False, ""

    if rel.parts and rel.parts[0] in GENERATED_TREES:
        return True, (
            "BLOCKED: this command writes into a build-generated tree "
            f"({rel.parts[0]}/). Those are rebuilt by make; edit the owning "
            ".pamphlet instead.\n")

    if str(rel) == "Makefile":
        return True, (
            "BLOCKED: this command writes into Makefile, which is tangled "
            "from Makefile.pamphlet (chunk '*', kept byte-identical). Edit "
            "Makefile.pamphlet, then regenerate with:\n"
            "    ./books/tanglec Makefile.pamphlet '*' > Makefile\n")

    if p.suffix != ".pamphlet" and p.parent.joinpath(p.name + ".pamphlet").exists():
        return True, (
            f"BLOCKED: this command writes into {rel}, which is tangle-owned "
            f"by {rel}.pamphlet. Edit the pamphlet chunk (prose first), then "
            "re-tangle with books/tanglec.\n")

    return False, ""


def main() -> int:
    try:
        payload = json.loads(sys.stdin.read())
    except Exception:
        return 0
    if payload.get("tool_name") != "Bash":
        return 0
    cmd = (payload.get("tool_input") or {}).get("command") or ""
    if not cmd.strip():
        return 0

    root = Path(os.environ.get("CLAUDE_PROJECT_DIR") or os.getcwd()).resolve()

    for segment in SEGMENT_SPLIT_RE.split(cmd):
        segment = segment.strip()
        if not segment:
            continue
        sanctioned = SANCTIONED_RETANGLE.search(segment) is not None
        if sanctioned:
            continue  # this segment's own retangle is the sanctioned write

        candidates = _redirect_targets(segment) + _write_command_targets(segment)
        for tok in candidates:
            blocked, msg = _protected_target(tok, root)
            if blocked:
                sys.stderr.write(msg)
                return 2

    return 0


if __name__ == "__main__":
    sys.exit(main())
