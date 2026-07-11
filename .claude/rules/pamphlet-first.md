# Pamphlet First — the mechanical checklist behind Rule 0

CLAUDE.md's Rule 0 says the document is the deliverable. This rule is
the *mechanical* checklist an edit must pass. It is pamphlet-specific:
Axiom's literate unit is the LaTeX `.pamphlet` with
`\begin{chunk}{name}` blocks, tangled by `books/tanglec` — not org-mode,
not noweb `<<...>>=`.

## Before writing any code

1. Name the owning pamphlet. New interpreter code → the matching
   `src/interp/*.lisp.pamphlet`; algebra → `books/bookvol10.2/.3/.4`;
   build wiring → the directory's `Makefile.pamphlet`; migration
   narrative → `books/bookvolMIGRATION.pamphlet`.
2. Write the prose FIRST: why the change is needed, what was
   considered, what trade-off the code embodies. For SBCL-migration
   work: why the classic GCL form fails, what the ANSI replacement is.
3. Only then add or edit the `\begin{chunk}{...}` code.
4. If the chunk is new, weave it in: a `\getchunk{...}` reference from
   the chunk that builds the output (`*` for a Makefile.pamphlet), or
   the build target that extracts it. An unwoven chunk is dead text.
5. Re-tangle and verify. For the top-level Makefile:
   `./books/tanglec Makefile.pamphlet '*' > Makefile` must leave
   `git diff Makefile` showing exactly your intended change —
   the pair is kept byte-identical.

## Verification commands

```sh
./books/tanglec <file>.pamphlet '<chunk name>'   # extract one chunk
./books/tanglec Makefile.pamphlet '*' | diff - Makefile   # pair check
```

## Incident anchors (why each item exists)

- Item 1/5: 2026-07-11 — `make run DIR=` was added to `Makefile` only;
  `Makefile.pamphlet` forgotten until review caught it. The pair had
  also drifted (21 whitespace spots) from years of direct edits; both
  fixed, hook now enforces.
- Item 4: `Makefile.pamphlet` carried `\getchunk{literate commands}`
  with no such chunk defined — tanglec silently expands ghosts to
  nothing, so the omission survived for years.
