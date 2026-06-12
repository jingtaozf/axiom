# Project rule: daly 体例优先 (daly literate style is the source of truth)

> This is jingtaozf's fork of Tim Daly's `daly/axiom` — the canonical
> "30 Year Horizon" literate-programming Axiom. We are migrating its
> build from the bundled AKCL/GCL 2.6.x image to a modern ANSI Common
> Lisp (SBCL) **while preserving Axiom's original literate style**.
> Keeping that style is the entire reason we chose `daly/axiom` over
> the already-SBCL-capable forks (FriCAS / OpenAxiom). Do not erode it.

## Rule 0 — Documentation first, code second (THE most important rule)

This is literate programming. **The prose is the deliverable; the code
is a by-product the prose generates.** Internalise the ordering before
touching anything:

1. **Write the document, not the code.** When asked to add or change a
   behaviour, your first and largest output is *prose* — explaining the
   problem, the design, the trade-off, the *why*. The `\begin{chunk}`
   of code comes last and is usually small relative to the prose around
   it.
2. **Never lead with code.** If your edit is mostly code with a one-line
   comment, you have the ratio backwards. Stop and write the teaching
   prose first.
3. **The value we are producing is understanding, not a binary.** A
   working `AXIOMsys` with no narrative is a *failure* of this project;
   a richly documented chapter that explains the bootstrap is a success
   even before the code compiles. We can always (re)generate the code
   from a good document; we cannot regenerate the understanding from a
   bare binary.
4. **A reader with no Axiom background must be able to learn the system
   from the prose alone.** Write for that reader every time.
5. **Measure of done = "could a stranger rebuild and understand this
   from the .pamphlet?"** — not "does it compile?".

If you ever feel the pull to "just write the code and document later" —
that is the exact failure mode this rule exists to stop. Document first.

## The rule (style)

Every artifact you add or change in this repo MUST be authored in
Axiom's own literate-programming style. The documentation is primary;
the code is secondary. If you are about to write a plain `.lisp`,
`.boot`, or `.spad` file, or paste code without surrounding prose —
stop, you are violating this rule.

## Concrete conventions (non-negotiable)

| Aspect | Required form |
|--------|---------------|
| File extension | `*.lisp.pamphlet` / `*.spad.pamphlet` / `*.boot.pamphlet` — never bare `.lisp`/`.spad` for new source |
| Document shell | `\documentclass{article}` + `\usepackage{axiom}` + `\begin{document}` … `\end{document}` |
| Header | `\title{\$SPAD/<path> <name>}` + `\author{…}` + `\maketitle` + `\begin{abstract}…\end{abstract}` + `\tableofcontents` |
| Code blocks | `\begin{chunk}{<chunk name>} … \end{chunk}` — Axiom's own chunk env, **NOT** noweb `<<>>=` |
| Cross-reference / tangle | `\getchunk{<chunk name>}` to pull a chunk into another file or the `Makefile.pamphlet` |
| Call/data annotations | `\calls{a}{b}`, `\usesdollar{fn}{Var}`, `\refsto{…}` where a book uses them |
| Organization | Extend the existing `books/bookvolN.pamphlet` and `src/interp/*.lisp.pamphlet` **in place**. Do **NOT** create OpenAxiom-style `src/lisp/` host-abstraction directories. |
| Prose before code | Every chunk is preceded by prose explaining **why** — especially, for the SBCL migration, why the classic GCL form fails and what the ANSI replacement is. |
| Build wiring | Weave new chunks into the build via `\getchunk{…}` in the relevant literate `Makefile.pamphlet`, the same way existing chunks are woven. |

## OpenAxiom's role: oracle, never source

`~/projects/open-axiom` already ported this codebase to SBCL/CCL/CLISP/ECL.
Use it as a **semantic reference / behavioral oracle only**:

- READ OpenAxiom (e.g. `src/lisp/core.lisp.in` `|saveCore|`) to learn the
  per-Lisp semantics (image dump, gc, getenv, FFI mapping).
- Then **re-author** that logic as daly-style literate prose + `\begin{chunk}`
  in the appropriate `daly/axiom` pamphlet.
- In the prose, cite the oracle as the source, e.g.
  "Semantic reference (not copied): open-axiom `src/lisp/core.lisp.in`
  `\getchunk{saveCore}`."
- NEVER copy OpenAxiom files, its `src/lisp/` layout, or its CMake/autoconf
  structure into this repo. The port lands in Axiom's book/pamphlet structure.

## Migration narrative is itself literate

The migration log lives as a daly-style book (e.g.
`books/bookvolMIGRATION.pamphlet`, `\usepackage{axiom}`), not as a
separate `.org`/Markdown system. Learning the classic means learning the
*writing convention* too.

## Self-check before any edit

1. Is the new code inside a `\begin{chunk}{…}` in a `*.pamphlet`?
2. Is there prose **before** it explaining the *why*?
3. Did I extend an existing book/pamphlet rather than inventing an
   OpenAxiom-shaped directory?
4. Is the chunk woven in via `\getchunk{…}`?
5. If I referenced OpenAxiom, did I cite it as oracle and re-author
   (not copy)?

If any answer is "no", fix it before proceeding.
