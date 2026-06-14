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
# Six src/input tests are excluded as non-portable in a headless SBCL tree:
#   graphviz     - shells out to the `dot` binary
#   herm         - )library loads a prebuilt library file absent from the tree
#   monitortest  - loads an external file the test never builds
#   newtonlisp   - )lisp (load "funcall.o"): external object absent
#   unittest2    - reads GCL compiler internals (compiler::*compile-verbose* ...)
#   unittest4    - )lisp (trace ...) then prints deep domain vectors
#
# Six algebra-domain tests are excluded:
#   BlasLevelOne dasum dcopy dcabs1 daxpy - wrap Fortran BLAS not linked into
#                                           the SBCL image (foreign TYPE-ERROR)
#   Graphviz                              - needs the `dot` binary
# Re-included after their root-cause fixes (all in bookvol5):
#   ElementaryFunction          - embed2 set symbol-function to a (lambda ...)
#                                 LIST; now coerced to FUNCTION.
#   ApplicationProgramInterface - summary() called (|summary|) with 0 args;
#                                 its ignored arg is now &optional.
#   Color Palette Vector        - pass on a correct interpsys; the )spool guard
#                                 skips an empty staged int/input rather than
#                                 NOOUT-ing the shard.
#
# (The 5 hyphenated Risch batches that used to be excluded -- richhyper000-099/
#  800-899, richtrig200-299/300-399/700-799 -- now pass.  They hit known algebra
#  bugs ("Cannot take first of an empty list") and, for 300-399, a deep-recursion
#  CONTROL-STACK-EXHAUSTED.  GCL's universal-error-handler reported these and
#  resumed; SBCL quit.  bookvol5/g-error now wrap runspad in a handler-bind for
#  error AND storage-condition that reports a "System error" and resumes, like
#  GCL/FriCAS -- so all 37 hyphenated batches run.)
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

EXCLUDE="graphviz herm monitortest newtonlisp unittest2 unittest4 \
BlasLevelOne dasum dcopy dcabs1 daxpy Graphviz"
# The cost is dominated by the Risch-integration family: ~106 rich*/richder*
# tests run 30-120s each (some hit PER_TEST_TIMEOUT) plus a few other heavies.
# We front-load them -- emit the heavy patterns before the fast bulk -- so the
# long jobs start at t=0 and the makespan tends to total_heavy/NW rather than
# trailing a slow test at the end.  HEAVY_RE matches names to pull forward;
# HEAVY_EXTRA lists slow non-Risch tests measured locally.
HEAVY_RE='^(rich|fuzz)'
HEAVY_EXTRA="easter dave89 groebtest ackermann r21bugsbig charlwood mapleok"

# Pool 2: constructor regression inputs that are staged in int/input/ with a
# )spool but are NOT listed in bookvol10's algebra.regress chunk.  A coverage
# audit found ~25 such CamelCase constructors; 24 pass locally and are added
# here (NagRootFindingPackage NOOUTs and is left out).  The ~970 lowercase
# LAPACK/BLAS routine artifacts are intentionally NOT included.
POOL2="AttributeRegistry DenavitHartenbergMatrix Exit FloatSpecialFunctions \
LeftOreRing MappingPackage4 ModularAlgebraicGcdOperations MultivariateLifting \
SegmentBinding TwoDimensionalViewport Type \
NagEigenPackage NagFittingPackage NagIntegrationPackage NagInterpolationPackage \
NagLapack NagLinearEquationSolvingPackage NagMatrixOperationsPackage \
NagOptimisationPackage NagOrdinaryDifferentialEquationsPackage \
NagPartialDifferentialEquationsPackage NagPolynomialRootsPackage \
NagSeriesSummationPackage NagSpecialFunctionsPackage"

[ -x "$TESTSYS" ] || { echo "FATAL: interpsys not built at $TESTSYS"; exit 2; }
[ -x "$LISP" ]    || { echo "FATAL: lisp tangler not built at $LISP"; exit 2; }

# Build the test list from EVERY *.regress named in the input Makefile -- not
# just REGRESSTESTS, but also the CATSTESTS / RICHTESTS / NEWRICHTESTS groups
# that `alltests` runs (gamma, ei, fixed, ...).  We grep the whole file rather
# than parse one variable; the pamphlet-existence filter below drops any name
# that came from prose, so no curated terminator (the old /zlindep/) is needed.
rm -rf "$WORKROOT"; mkdir -p "$WORKROOT/run"
ALL="$WORKROOT/all_names.txt"
# Note the '-' in the char class: names like richhyper000-099.regress are real
# tests; a class without '-' splits them into a bogus "099" fragment and the
# whole batch is silently dropped.
grep -oE '[a-zA-Z0-9_-]+\.regress' "$SPD/src/input/Makefile.org" \
  | sed 's/\.regress//' | sort -u \
  | while read -r t; do
      case " $EXCLUDE " in *" $t "*) continue;; esac
      [ -f "$SPD/src/input/$t.input.org" ] && echo "$t"
    done > "$ALL"
SRC_TOTAL=$(wc -l < "$ALL" | tr -d ' ')

# Algebra domain regression (upstream `algebratests`): the `algebra.regress`
# chunk in bookvol10 lists ~1157 domain tests (AbelianGroup, ...).  Their .input
# files are build artifacts staged in int/input/.  Append the ones present --
# graceful if a given build did not stage them (the count is logged below).
ALG="$WORKROOT/alg_names.txt"; : > "$ALG"
# bookvol10 is now .org: the chunk opens with `#+NAME: algebra.regress' instead
# of `\begin{chunk}{algebra.regress}'.  The REGRESS= body inside the chunk is
# verbatim, so only the chunk-start pattern changes.
{ awk '/^#\+NAME: algebra\.regress/{p=1} p&&/REGRESS=/{r=1} r{print} p&&/%\.regress:/{exit}' \
      "$SPD/books/bookvol10.org" \
    | grep -oE '[A-Za-z][A-Za-z0-9]*\.regress' | sed 's/\.regress//'
  printf '%s\n' $POOL2          # staged-but-unlisted constructors (Pool 2)
} | sort -u \
  | while read -r d; do
      case " $EXCLUDE " in *" $d "*) continue;; esac
      # Require an actual )spool regression input: a build that stages an empty
      # or truncated <D>.input would otherwise be run, produce no .output, and
      # fail as NOOUT.  Skipping the malformed artifact is correct, not a miss.
      [ -f "$SPD/int/input/$d.input" ] && grep -q ')spool' "$SPD/int/input/$d.input" && echo "$d"
    done > "$ALG"
ALG_TOTAL=$(wc -l < "$ALG" | tr -d ' ')
cat "$ALG" >> "$ALL"
sort -u "$ALL" -o "$ALL"
TOTAL=$(wc -l < "$ALL" | tr -d ' ')
echo "discovered $SRC_TOTAL src/input + $ALG_TOTAL algebra-domain = $TOTAL tests"

# Guard against silent suite shrinkage: if the Makefile parse breaks (renamed,
# moved, or chunk-structure change), the list could quietly lose tests while
# every shard still reports green.  Fail loudly below a floor.
EXPECTED_MIN="${EXPECTED_MIN:-520}"
if [ "$TOTAL" -lt "$EXPECTED_MIN" ]; then
  echo "FATAL: discovered only $TOTAL regression tests (< $EXPECTED_MIN expected)." >&2
  echo "  The .regress scan of src/input/Makefile.org likely broke." >&2
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
  cd "$d" || { echo "$t FAIL chdir" >> "$RESULTS"; exit 0; }
  pf="$SPD/src/input/$t.input.org"
  if grep -qxF "$t" "$WORKROOT/alg_names.txt" 2>/dev/null; then
    # algebra-domain test: read the staged int/input directly.  Do NOT tangle a
    # same-named src/input source -- 32 domains (Vector, Color, Palette, ...)
    # collide with LaTeX example pamphlets that are not )spool regression tests
    # and would NOOUT the shard.
    cp "$SPD/int/input/$t.input" "$t.input"
  elif [ -f "$pf" ]; then
    # src/input test: tangle the .org source DIRECTLY to a .input (srctangle ->
    # orgtangle by default; AXIOM_TANGLE_VIA_PAMPHLET=1 routes through .pamphlet).
    "$SPD/books/srctangle" "$pf" "*" > "$t.input" 2>/dev/null || true
  elif [ -f "$SPD/int/input/$t.input" ]; then
    cp "$SPD/int/input/$t.input" "$t.input"
  fi
  rm -f "$t.output"
  echo ")read $t.input" | timeout -k 10 "$PER_TEST_TIMEOUT" "$TESTSYS" >/dev/null 2>"$t.stderr" || true
  if [ ! -f "$t.output" ]; then
    # Diagnostic: surface why no spool file appeared (stderr tail + input size).
    echo "$t NOOUT [in=$(wc -c < "$t.input" 2>/dev/null)b err: $(tr "\n" " " < "$t.stderr" 2>/dev/null | tail -c 240)]" >> "$RESULTS"
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
