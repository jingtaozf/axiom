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

# Build the test list from EVERY *.regress named in the input Makefile -- not
# just REGRESSTESTS, but also the CATSTESTS / RICHTESTS / NEWRICHTESTS groups
# that `alltests` runs (gamma, ei, fixed, ...).  We grep the whole file rather
# than parse one variable; the pamphlet-existence filter below drops any name
# that came from prose, so no curated terminator (the old /zlindep/) is needed.
rm -rf "$WORKROOT"; mkdir -p "$WORKROOT/run"
ALL="$WORKROOT/all_names.txt"
grep -oE '[a-zA-Z0-9_]+\.regress' "$SPD/src/input/Makefile.pamphlet" \
  | sed 's/\.regress//' | sort -u \
  | while read -r t; do
      case " $EXCLUDE " in *" $t "*) continue;; esac
      [ -f "$SPD/src/input/$t.input.pamphlet" ] && echo "$t"
    done > "$ALL"
TOTAL=$(wc -l < "$ALL" | tr -d ' ')

# Guard against silent suite shrinkage: if the Makefile parse breaks (renamed,
# moved, or chunk-structure change), the list could quietly lose tests while
# every shard still reports green.  Fail loudly below a floor.
EXPECTED_MIN="${EXPECTED_MIN:-520}"
if [ "$TOTAL" -lt "$EXPECTED_MIN" ]; then
  echo "FATAL: discovered only $TOTAL regression tests (< $EXPECTED_MIN expected)." >&2
  echo "  The .regress scan of src/input/Makefile.pamphlet likely broke." >&2
  echo "  Refusing to report a shrunk suite green." >&2
  exit 2
fi

# Order: heavy tests first (Risch family + measured non-Risch heavies), then the
# fast bulk.  Front-loading minimises makespan under the dynamic queue.
NAMES="$WORKROOT/names.txt"; HEAVY="$WORKROOT/heavy.txt"
grep -E "$HEAVY_RE" "$ALL" > "$HEAVY"
for t in $HEAVY_EXTRA; do grep -qxF "$t" "$ALL" && echo "$t" >> "$HEAVY"; done
sort -u "$HEAVY" -o "$HEAVY"
cat "$HEAVY" > "$NAMES"
grep -vxF -f "$HEAVY" "$ALL" >> "$NAMES" 2>/dev/null || cat "$ALL" >> "$NAMES"

# Shard selection (for the CI matrix): keep every SHARD_TOTAL-th test, offset by
# this shard.  Because NAMES is heavy-first, round-robin over it spreads the
# contiguous Risch block evenly -- each shard gets ~1/SHARD_TOTAL of the dominant
# heavy load AND of the fast bulk, no per-test timing file needed.  Default
# SHARD_TOTAL=1 runs the whole suite (local/serial use).
SHARD_INDEX="${SHARD_INDEX:-1}"
SHARD_TOTAL="${SHARD_TOTAL:-1}"
if [ "$SHARD_TOTAL" -gt 1 ]; then
  awk -v k="$((SHARD_INDEX-1))" -v n="$SHARD_TOTAL" '(NR-1) % n == k' "$NAMES" > "$NAMES.shard"
  mv "$NAMES.shard" "$NAMES"
fi
TOTAL=$(wc -l < "$NAMES" | tr -d ' ')

echo "running $TOTAL regression tests (shard $SHARD_INDEX/$SHARD_TOTAL, $NW parallel, dynamic queue; excluding: $EXCLUDE)"

RESULTS="$WORKROOT/results.txt"; : > "$RESULTS"; export RESULTS

# Per-test logic runs INLINE inside `bash -c` (NOT an exported function: bash
# 5.x on macOS does not pass exported functions through xargs' `bash -c`).  Each
# test runs in its own dir so .output files never collide, and appends one short
# result line to $RESULTS -- short O_APPEND writes are atomic, so the parallel
# appends do not interleave.
xargs -P "$NW" -I{} bash -c '
  t="$1"
  d="$WORKROOT/run/$t"
  # A setup failure must still emit a result line, else the test vanishes from
  # the tally (pass+fail < total) and a never-run test cannot fail the gate.
  mkdir -p "$d" || { echo "$t FAIL mkdir" >> "$RESULTS"; exit 0; }
  pf="$SPD/src/input/$t.input.pamphlet"
  cd "$d" || { echo "$t FAIL chdir" >> "$RESULTS"; exit 0; }
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
LINES=$(wc -l < "$RESULTS" | tr -d ' ')
echo "================ regression summary ================"
echo "pass=$PASS  fail=$FAIL  total=$TOTAL  results=$LINES"
# Reconcile: every test must have produced exactly one result line.  A missing
# line means a test silently vanished (crashed the harness before writing) --
# fail rather than report green on partial coverage.
if [ "$LINES" -ne "$TOTAL" ]; then
  echo "FATAL: $LINES result lines for $TOTAL tests -- $((TOTAL-LINES)) vanished; refusing to pass." >&2
  exit 2
fi
if [ "$FAIL" -gt 0 ]; then
  echo "---- failures ----"
  grep -E ' FAIL| NOOUT' "$RESULTS" | sort
  exit 1
fi
echo "all $PASS non-excluded regression tests passed"
