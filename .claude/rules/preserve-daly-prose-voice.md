# Preserve Daly's Prose Voice — No Convergent Regression

CLAUDE.md's Rule 0 is about *adding* prose; this rule is about *not
eroding the prose already there*. It is the one piece of the deleted
`literate-agent` cowork ruleset worth re-establishing natively, because
it guards the exact property CLAUDE.md calls load-bearing:

> "Keeping that [literate] style is the entire reason we chose
> `daly/axiom` over the already-SBCL-capable forks (FriCAS /
> OpenAxiom). Do not erode it."

## The failure mode: convergent regression

When an AI edits existing pamphlet prose, its output is pulled toward
the *pretraining median* — generic, textbook-standard technical
writing. Rare, project-specific patterns (Daly's first-person
narration, his 30-Year-Horizon asides, idiomatic Axiom terminology,
the `\calls`/`\usesdollar` cross-reference vocabulary) have low prior
probability and get silently diluted into "clean modern prose." The
drift is *unintentional* — the model isn't editing toward any goal, the
regression is a side effect of generation under prior pressure. No hook
catches it: `check-pamphlet.sh` only inspects chunk structure and
tangling, never prose content.

Concretely, this is the difference between leaving

> "We finally come to the domain constructor. A few subtle differences
> between packages and domains turn up some interesting issues."

and "improving" it into

> "This section describes the domain constructor and enumerates the
> differences between packages and domains."

The second is not wrong. It is *dead* — the voice that makes the book a
book, not a reference manual, is gone. Over enough edits the whole
corpus flattens toward generic documentation and the reason for
choosing this fork evaporates.

## The rule

When editing prose that already exists in a pamphlet:

1. **Default to preserving the author's wording.** Touch prose only to
   fix an actual error (wrong fact, stale reference, broken sentence)
   or to add genuinely new information. "Making it read more
   professionally" is not a reason — it is the regression.
2. **Preserve first-person and narrative asides.** "We finally come
   to…", "you will often want to…", the historical digressions — these
   are the voice. Do not rewrite them into impersonal declaratives.
3. **Preserve idiomatic Axiom terminology.** "capsule", "domain
   shell", "the `$` denotes this domain", "NRLIB", "BOOT", "SPAD" —
   never swap a project term for a generic synonym ("module", "object
   table", "compiled library") even when the synonym is more common in
   the wider world.
4. **Preserve the cross-reference vocabulary.** `\calls{a}{b}`,
   `\usesdollar{fn}{Var}`, `\refsto{…}`, `\defsdollar{…}` are the
   book's index infrastructure. Keep them on new code where the
   surrounding chunk uses them; never strip them "for cleanliness".
5. **When you *must* modernize** (a term is genuinely opaque, or two
   spellings exist and must be unified), do it *explicitly* — say so in
   the reply and, for anything beyond a single word, note it in the
   commit message. An announced, deliberate change is fine; a silent
   polish pass is the failure this rule names.

## The one-question test

Before rewording any existing pamphlet sentence, ask: *does this edit
add information or fix an error, or does it only change how the prose
sounds?* If only the latter — **stop, revert to the author's wording.**

## What this rule is NOT

- Not a ban on editing prose — fixing a wrong `\getchunk` reference, a
  stale line number, or a factually incorrect sentence is exactly the
  work.
- Not a style-freeze on *your own new* prose — write new sections in a
  voice consonant with the book, but this rule governs *existing* text.

## Provenance

Re-established natively 2026-07-11 after the `.claude/rules/literate-
agent` symlink (which carried `lp-agent-convergent-regression-defence`
and the cowork anti-sycophancy rules) was removed to drop the external
dependency. This file ports only the one rule whose subject —
prose-voice erosion of a deliberately-chosen literate style — is
first-class for this repo.
