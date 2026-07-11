#!/usr/bin/env python3
"""PreToolUse hook (Edit|Write|MultiEdit): enforce pamphlet-first editing.

Axiom is a literate system: the .pamphlet files are the source of
truth, everything else is tangled from them.  Three classes of path
must never be hand-edited:

  1. int/ obj/ mnt/          -- build-generated trees (gitignored,
                                rebuilt by make; edits are lost).
  2. Makefile                -- tangled from Makefile.pamphlet chunk
                                '*'; kept byte-identical, so the only
                                legitimate write is the re-tangle.
  3. any file F with a       -- e.g. foo.lisp beside foo.lisp.pamphlet;
     sibling F.pamphlet         the pamphlet owns it.

On block: exit 2 with a message naming the owning pamphlet and the
exact re-tangle command, so the agent can route the edit correctly.

Path checks run against BOTH the lexical path (string-normalized,
symlinks NOT followed) and the resolved path (symlinks followed).  A
symlink lexically inside mnt/ whose target lives outside the repo
resolves out-of-root -- checking resolve() alone would then treat it
as "not our scope" and let a generated-tree edit through.  Checking
the lexical form closes that hole; keeping the resolved-path check
too still catches ".." escapes through a real (non-symlinked) path.

On malformed/empty stdin, allow (exit 0) rather than let the
resulting traceback fail this PreToolUse hook open by accident: any
non-2 exit already allows the call, so a clean 0 is the honest
version of that outcome.

(Named .sh, run by python via shebang: the globally-installed
literate-agent plugin would false-block a .py file in this repo.)
"""
import json
import os
import sys
from pathlib import Path

GENERATED_TREES = {"int", "obj", "mnt"}


def _classify(f: Path, root: Path):
    """Return (blocked, message) for candidate path `f` under `root`.

    `f` may be either the lexical or the resolved form of the edited
    file's path; the caller tries both.  (False, "") means either `f`
    is outside `root` (not our scope) or it isn't a protected path.
    """
    try:
        rel = f.relative_to(root)
    except ValueError:
        return False, ""

    if rel.parts and rel.parts[0] in GENERATED_TREES:
        return True, (
            f"BLOCKED: {rel} is inside the build-generated '{rel.parts[0]}/' "
            "tree (gitignored; make rebuilds it and your edit is lost). "
            "Edit the owning .pamphlet under src/ or books/ instead.\n")

    if str(rel) == "Makefile":
        return True, (
            "BLOCKED: Makefile is tangled from Makefile.pamphlet (chunk '*', "
            "kept byte-identical). Edit Makefile.pamphlet -- prose first, "
            "then the chunk -- and regenerate with:\n"
            "    ./books/tanglec Makefile.pamphlet '*' > Makefile\n")

    if f.suffix != ".pamphlet" and (f.parent / (f.name + ".pamphlet")).exists():
        return True, (
            f"BLOCKED: {rel} is tangle-owned by {rel}.pamphlet. "
            "Edit the pamphlet chunk (prose first), then re-tangle the "
            "appropriate chunk with books/tanglec (the top-level Makefile "
            "uses chunk '*'; per-directory Makefiles use their own chunk "
            "name).\n")

    return False, ""


def main() -> int:
    try:
        payload = json.loads(sys.stdin.read())
    except Exception:
        return 0
    if payload.get("tool_name") not in {"Edit", "Write", "MultiEdit"}:
        return 0
    fp = (payload.get("tool_input") or {}).get("file_path") or ""
    if not fp:
        return 0
    root = Path(os.environ.get("CLAUDE_PROJECT_DIR") or os.getcwd()).resolve()
    f_raw = Path(fp)
    if not f_raw.is_absolute():
        f_raw = root / f_raw
    f_lexical = Path(os.path.normpath(str(f_raw)))
    f_resolved = f_raw.resolve()

    for candidate in (f_lexical, f_resolved):
        blocked, msg = _classify(candidate, root)
        if blocked:
            sys.stderr.write(msg)
            return 2

    return 0


if __name__ == "__main__":
    sys.exit(main())
