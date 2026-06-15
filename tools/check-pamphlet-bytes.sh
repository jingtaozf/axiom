#!/bin/bash
# check-pamphlet-bytes.sh -- byte-exact pamphlet gate for org-native migration phases.
#
# Invariant (the migration's go/no-go): an org-native prose rewrite must NOT
# change a single byte of any regenerated .pamphlet.  We prove that directly,
# with no allowlist, by regenerating every tracked source pamphlet twice --
# once from a BASE ref (with BASE's own untanglec), once from the working tree
# (with the current untanglec) -- and asserting they are byte-identical.
#
# This is the allowlist-free form of "test 2": it compares the BUILD OUTPUT
# across a change, so intentional content edits living in BASE..worktree (an
# SBCL code patch, a Makefile rewiring) are invisible to it -- only a prose
# rewrite that accidentally altered pamphlet bytes can make it fail.
#
# Usage:
#   tools/check-pamphlet-bytes.sh [BASE_REF]      # default BASE_REF = HEAD
# Examples:
#   tools/check-pamphlet-bytes.sh                 # working tree vs HEAD
#   tools/check-pamphlet-bytes.sh 46e04105        # working tree vs pre-phase1
# Exit: 0 if every common pamphlet is byte-identical, 1 on any DIFFER.
set -u
cd "$(git rev-parse --show-toplevel)" || exit 2
BASE="${1:-HEAD}"
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT

# Build the two untanglec binaries: BASE's and the working tree's.
git show "$BASE:books/untanglec.c" > "$T/unt_base.c" 2>/dev/null || {
  echo "fatal: cannot read books/untanglec.c at $BASE" >&2; exit 2; }
cc -O2 -o "$T/unt_base" "$T/unt_base.c" 2>/dev/null || { echo "fatal: base untanglec build failed" >&2; exit 2; }
cc -O2 -o "$T/unt_cur"  books/untanglec.c   2>/dev/null || { echo "fatal: current untanglec build failed" >&2; exit 2; }

# Candidate sources: TRACKED .org carrying a ':noweb yes' chunk, excluding
# tools/ (literate-elisp, not pamphlets) -- mirrors the regen-pamphlets target,
# but tracked-only so git-ignored build artifacts (e.g. src/interp/bookvol5.org)
# never enter the gate.  (No mapfile: portable to macOS /bin/bash 3.2.)
git ls-files '*.org' | grep -vE '^tools/' | while read -r f; do
  grep -Iq ':noweb yes' "$f" 2>/dev/null && echo "$f"; done | sort > "$T/files.txt"

SAME=0; DIFF=0; SKIP=0
: > "$T/differ.txt"
while read -r f; do
  [ -z "$f" ] && continue
  if ! git cat-file -e "$BASE:$f" 2>/dev/null; then SKIP=$((SKIP+1)); continue; fi
  git show "$BASE:$f" > "$T/base.org" 2>/dev/null
  "$T/unt_base" "$T/base.org" > "$T/base.pam" 2>/dev/null
  "$T/unt_cur"  "$f"          > "$T/cur.pam"  2>/dev/null
  if cmp -s "$T/base.pam" "$T/cur.pam"; then SAME=$((SAME+1));
  else DIFF=$((DIFF+1)); echo "$f" >> "$T/differ.txt"; fi
done < "$T/files.txt"

echo "pamphlet byte gate: BASE=$BASE  vs  working tree"
echo "  byte-identical : $SAME"
echo "  DIFFER         : $DIFF"
echo "  skipped (new since BASE) : $SKIP"
if [ "$DIFF" -ne 0 ]; then
  echo "REGRESSION -- these pamphlets changed bytes:"
  sed 's/^/    /' "$T/differ.txt"
  exit 1
fi
echo "OK: org-native rewrite is byte-transparent ($SAME pamphlets)"
exit 0
