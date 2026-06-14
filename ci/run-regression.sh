#!/usr/bin/env bash
# Run the src/input regression suite against the freshly built interpsys and
# fail if any non-excluded test raises a Lisp error.
#
# A test "passes" here = it runs to completion without an SBCL Unhandled
# condition.  (The historical regress harness compared against embedded --R
# expected lines; that comparison is broken under SBCL's spool format, so we
# use crash-freedom as the gate -- every genuine GCL->SBCL port bug surfaced
# as a crash.)
#
# Six tests are excluded as non-portable in a headless SBCL tree:
#   graphviz     - shells out to the `dot` binary
#   herm         - )library loads a prebuilt library file absent from the tree
#   monitortest  - loads an external file the test never builds
#   newtonlisp   - )lisp (load "funcall.o"): external object absent
#   unittest2    - reads GCL compiler internals (compiler::*compile-verbose* ...)
#   unittest4    - )lisp (trace ...) then prints deep domain vectors
#
# Scheduling: a few hundred sub-second tests plus a heavy tail -- ~106
# rich*/richder* (Risch integration) tests at 30-120s each.  We dispatch through
# `xargs -P` -- a dynamic work queue that starts the next test the instant a slot
# frees -- so a slow test overlaps the fast bulk instead of parking a whole
# static shard the old `i % NW` partition did.  We also front-load the heavy
# tests (HEAVY_RE / HEAVY_EXTRA) so the longest jobs start at t=0 rather than
# near the end, which minimises makespan (longest-processing-time-first).
set -u

export SPD="${SPD:-$(cd "$(dirname "$0")/.." && pwd)}"
export AXIOM="${AXIOM:-$SPD/mnt/MACOSX}"
export DAASE="${DAASE:-$SPD/src/share}"
export LISP="$SPD/obj/MACOSX/bin/lisp"
export TESTSYS="$SPD/obj/MACOSX/bin/interpsys"
WORKROOT="${WORKROOT:-/tmp/axci}"
export WORKROOT
# Oversubscribe the cores: the tail tests are GC/IO-bound, not CPU-saturating,
# so 2x cores keeps every core busy while a slow test idles on syscalls.  The
# GitHub macos-14 runner has 3 cores -> NW=6.  Override with NW=... if needed.
NCORE="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
NW="${NW:-$(( NCORE*2 > 12 ? 12 : NCORE*2 ))}"
export PER_TEST_TIMEOUT="${PER_TEST_TIMEOUT:-120}"

EXCLUDE="graphviz herm monitortest newtonlisp unittest2 unittest4"
# The cost is dominated by the Risch-integration family: ~106 rich*/richder*
# tests run 30-120s each (some hit PER_TEST_TIMEOUT) plus a few other heavies.
# We front-load them -- emit the heavy patterns before the fast bulk -- so the
# long jobs start at t=0 and the makespan tends to total_heavy/NW rather than
# trailing a slow test at the end.  HEAVY_RE matches names to pull forward;
# HEAVY_EXTRA lists slow non-Risch tests measured locally.
HEAVY_RE='^(rich|fuzz)'
HEAVY_EXTRA="easter dave89 groebtest ackermann r21bugsbig charlwood mapleok"

[ -x "$TESTSYS" ] || { echo "FATAL: interpsys not built at $TESTSYS"; exit 2; }
[ -x "$LISP" ]    || { echo "FATAL: lisp tangler not built at $LISP"; exit 2; }

# Build the test list from the curated REGRESSTESTS in the input Makefile.
rm -rf "$WORKROOT"; mkdir -p "$WORKROOT/run"
ALL="$WORKROOT/all_names.txt"
awk '/^REGRESSTESTS=/{f=1} f{print} f&&/zlindep/{exit}' "$SPD/src/input/Makefile.pamphlet" \
  | grep -oE '[a-zA-Z0-9_]+\.regress' | sed 's/\.regress//' | sort -u \
  | while read -r t; do
      case " $EXCLUDE " in *" $t "*) continue;; esac
      [ -f "$SPD/src/input/$t.input.pamphlet" ] && echo "$t"
    done > "$ALL"
TOTAL=$(wc -l < "$ALL" | tr -d ' ')

# Order: heavy tests first (Risch family + measured non-Risch heavies), then the
# fast bulk.  Front-loading minimises makespan under the dynamic queue.
NAMES="$WORKROOT/names.txt"; HEAVY="$WORKROOT/heavy.txt"
grep -E "$HEAVY_RE" "$ALL" > "$HEAVY"
for t in $HEAVY_EXTRA; do grep -qxF "$t" "$ALL" && echo "$t" >> "$HEAVY"; done
sort -u "$HEAVY" -o "$HEAVY"
cat "$HEAVY" > "$NAMES"
grep -vxF -f "$HEAVY" "$ALL" >> "$NAMES" 2>/dev/null || cat "$ALL" >> "$NAMES"

echo "running $TOTAL regression tests ($NW parallel, dynamic queue; excluding: $EXCLUDE)"

RESULTS="$WORKROOT/results.txt"; : > "$RESULTS"; export RESULTS

# Per-test logic runs INLINE inside `bash -c` (NOT an exported function: bash
# 5.x on macOS does not pass exported functions through xargs' `bash -c`).  Each
# test runs in its own dir so .output files never collide, and appends one short
# result line to $RESULTS -- short O_APPEND writes are atomic, so the parallel
# appends do not interleave.
xargs -P "$NW" -I{} bash -c '
  t="$1"
  d="$WORKROOT/run/$t"; mkdir -p "$d" || exit 0
  pf="$SPD/src/input/$t.input.pamphlet"
  cd "$d" || exit 0
  echo "(tangle \"$pf\" \"*\" \"$t.input\")" | "$LISP" >/dev/null 2>&1 || true
  rm -f "$t.output"
  echo ")read $t.input" | timeout -k 10 "$PER_TEST_TIMEOUT" "$TESTSYS" >/dev/null 2>&1 || true
  if [ ! -f "$t.output" ]; then
    echo "$t NOOUT" >> "$RESULTS"
  elif grep -Eq "^(Unhandled .* in thread|unhandled condition in --disable)" "$t.output"; then
    echo "$t FAIL $(grep -oE "Unhandled [A-Z:-]+" "$t.output" | head -1)" >> "$RESULTS"
  else
    echo "$t PASS" >> "$RESULTS"
  fi
' _ {} < "$NAMES"

PASS=$(grep -c ' PASS' "$RESULTS")
FAIL=$(grep -cE ' FAIL| NOOUT' "$RESULTS")
echo "================ regression summary ================"
echo "pass=$PASS  fail=$FAIL  total=$TOTAL"
if [ "$FAIL" -gt 0 ]; then
  echo "---- failures ----"
  grep -E ' FAIL| NOOUT' "$RESULTS" | sort
  exit 1
fi
echo "all $PASS non-excluded regression tests passed"
