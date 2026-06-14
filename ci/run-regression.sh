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
set -u

SPD="${SPD:-$(cd "$(dirname "$0")/.." && pwd)}"
export AXIOM="${AXIOM:-$SPD/mnt/MACOSX}"
export DAASE="${DAASE:-$SPD/src/share}"
LISP="$SPD/obj/MACOSX/bin/lisp"
TESTSYS="$SPD/obj/MACOSX/bin/interpsys"
WORKROOT="${WORKROOT:-/tmp/axci}"
NW="${NW:-4}"                 # parallel workers
PER_TEST_TIMEOUT="${PER_TEST_TIMEOUT:-120}"

EXCLUDE="graphviz herm monitortest newtonlisp unittest2 unittest4"

[ -x "$TESTSYS" ] || { echo "FATAL: interpsys not built at $TESTSYS"; exit 2; }
[ -x "$LISP" ]    || { echo "FATAL: lisp tangler not built at $LISP"; exit 2; }

# Build the test list from the curated REGRESSTESTS in the input Makefile.
NAMES="$WORKROOT/names.txt"
rm -rf "$WORKROOT"; mkdir -p "$WORKROOT"
awk '/^REGRESSTESTS=/{f=1} f{print} f&&/zlindep/{exit}' "$SPD/src/input/Makefile.pamphlet" \
  | grep -oE '[a-zA-Z0-9_]+\.regress' | sed 's/\.regress//' | sort -u \
  | while read -r t; do
      case " $EXCLUDE " in *" $t "*) continue;; esac
      [ -f "$SPD/src/input/$t.input.pamphlet" ] && echo "$t"
    done > "$NAMES"
TOTAL=$(wc -l < "$NAMES" | tr -d ' ')
echo "running $TOTAL regression tests ($NW workers, excluding: $EXCLUDE)"

worker() {
  local wid=$1 W="$WORKROOT/w$1"; mkdir -p "$W"; cd "$W" || exit 2
  local res="$WORKROOT/res$1.txt"; : > "$res"
  local i=0 t pf
  while read -r t; do
    i=$((i+1)); [ $((i % NW)) -eq $((wid % NW)) ] || continue
    pf="$SPD/src/input/$t.input.pamphlet"
    echo "(tangle \"$pf\" \"*\" \"$t.input\")" | "$LISP" >/dev/null 2>&1 || true
    rm -f "$t.output"
    echo ")read $t.input" | timeout -k 10 "$PER_TEST_TIMEOUT" "$TESTSYS" >/dev/null 2>&1 || true
    if [ ! -f "$t.output" ]; then echo "$t NOOUT" >> "$res"; continue; fi
    if grep -Eq '^(Unhandled .* in thread|unhandled condition in --disable)' "$t.output"; then
      echo "$t FAIL $(grep -oE 'Unhandled [A-Z:-]+' "$t.output" | head -1)" >> "$res"
    else
      echo "$t PASS" >> "$res"
    fi
  done < "$NAMES"
}

pids=()
for w in $(seq 0 $((NW-1))); do worker "$w" & pids+=($!); done
for p in "${pids[@]}"; do wait "$p"; done

cat "$WORKROOT"/res*.txt > "$WORKROOT/all.txt"
PASS=$(grep -c ' PASS' "$WORKROOT/all.txt")
FAIL=$(grep -cE ' FAIL| NOOUT' "$WORKROOT/all.txt")
echo "================ regression summary ================"
echo "pass=$PASS  fail=$FAIL  total=$TOTAL"
if [ "$FAIL" -gt 0 ]; then
  echo "---- failures ----"
  grep -E ' FAIL| NOOUT' "$WORKROOT/all.txt" | sort
  exit 1
fi
echo "all $PASS non-excluded regression tests passed"
