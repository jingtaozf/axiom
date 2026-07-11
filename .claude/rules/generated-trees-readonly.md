# Generated Trees Are Read-Only

Three top-level trees are build products (all gitignored). Never edit,
never write into them — `make` recreates them and your change is lost:

| Tree | What lands there | Source of truth |
|------|------------------|-----------------|
| `int/` | tangled .lisp/.spad, NRLIBs, intermediate builds | `src/**/*.pamphlet`, `books/bookvol10.*` |
| `obj/` | compiled objects | same |
| `mnt/` | the installed system (AXIOMsys image, algebra, docs) | same |

Enforced by `.claude/hooks/block-generated-edit.sh` (Edit/Write) and
`block-bash-generated-write.sh` (shell redirects, `sed -i`, `cp`, ...).

## The tracked special case: `Makefile`

The top-level `Makefile` IS tracked, but it is tangled from
`Makefile.pamphlet` chunk `*` and the pair is kept **byte-identical**
(reconciled 2026-07-11 after years of drift). Direct edits are blocked;
the workflow is:

```sh
$EDITOR Makefile.pamphlet            # prose + chunk
./books/tanglec Makefile.pamphlet '*' > Makefile
git diff Makefile                    # must show exactly your change
```

The only sanctioned shell write to `Makefile` is that tanglec command —
the Bash hook whitelists it.

## Inspecting generated code is encouraged

Reading these trees is often the fastest way to understand the system
(e.g. `int/algebra/ZMOD.nrlib/code.lsp` shows what a domain compiles
to). Read freely; route every change through the owning pamphlet.
