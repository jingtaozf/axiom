# Pamphlet Chunk Discipline — tanglec's actual semantics

`books/tanglec` (C, ~300 lines) is the tangler for **build wiring and
standalone extraction** — the top-level `Makefile` (chunk `*`), the
per-directory `Makefile`s, and any one-off `tanglec <file> <chunk>`.
It is NOT the only tangler: the **algebra** domain/category/package
chunks in `books/bookvol10.*` are tangled during `make` by the
Lisp-image-resident tangler `LISPTANGLE` (`${OBJ}/${SYS}/bin/lisp`,
see `books/bookvol10.pamphlet`), which is more lenient than tanglec
(it tolerated the 9-year-old PERMGRP duplicate-`\begin` that tanglec
mis-tangles). So `check-pamphlet`'s per-chunk regression check shells
out to `tanglec` as a fast, close *approximation* of the real algebra
build, not the build itself — treat a tanglec-only signal on a
`bookvol10.*` chunk as advisory. What tanglec really does with the
markers it recognises — verified against the binary, not assumed:

| Fact | Consequence |
|------|-------------|
| Chunks are `\begin{chunk}{name}` … `\end{chunk}` | noweb `<<name>>=` and org `#+begin_src` syntax mean NOTHING here |
| Same-name chunks concatenate in file order | Split a chunk to interleave prose; standard daly pattern (see `src/interp/patches.lisp.pamphlet`) |
| Chunk names are file-local | `\getchunk{X}` only resolves within the same pamphlet |
| Unknown `\getchunk{X}` expands to NOTHING, exit 0 | Ghost references are silent data loss — the check-pamphlet hook flags them |
| Usage: `tanglec <file> <chunk> > out` | Extraction is per-chunk; the whole-file chunk is conventionally named `*` |

## The verbatim trap (bit us on 2026-07-11)

Code shown inside `\begin{verbatim}` for *narration* is invisible to
tanglec even if it looks like a chunk header. `books/bookvol10.pamphlet`
line ~270 displays `<domain BSD BasicStochasticDifferential>>=` inside a
verbatim block — it is a *picture* of code; the real, tangle-able chunk
lives in `bookvol10.3.pamphlet` as
`\begin{chunk}{domain BSD BasicStochasticDifferential}`. Before
tangling anything, confirm the chunk is a real `\begin{chunk}`, not an
illustration.

## Chunk naming conventions (observed in the books)

- Algebra: `category ABBREV Name` / `domain ABBREV Name` /
  `package ABBREV Name` (e.g. `domain ZMOD IntegerMod`).
- Interpreter: `defun <name>` in bookvol5/9 (e.g. `defun regress`).
- Build: target-shaped names in Makefile.pamphlet (`run`, `install`,
  `tanglec.c`) woven into `*`.

Follow the file's existing shape; never invent a new naming scheme in
an existing book.

## Cross-reference annotations

Where a book uses them, maintain `\calls{a}{b}`, `\usesdollar{fn}{Var}`,
`\refsto{...}`, `\defsdollar{...}` for new code — they are the books'
index infrastructure (see bookvol5/bookvol9 for examples).
