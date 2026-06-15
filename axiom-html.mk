# axiom-html.mk -- parallel org -> HTML publish of the axiom tree.
#
#   make -f axiom-html.mk -j6        # full build, incremental (only changed files)
#   make -f axiom-html.mk clean
#
# One headless Emacs per file (ox-html, via axiom-publish.org loaded literately).
# make owns BOTH parallelism (-j) and incrementality (output mtime vs source),
# so org-publish's cross-process timestamp cache -- which would race across
# parallel workers -- is never used.

EMACS  ?= emacs
OUT    ?= public
LELISP ?= $(HOME)/projects/literate-elisp
PUB    := axiom-publish.org

# tracked .org only: excludes the gitignored src/{interp,algebra} bookvol copies,
# includes the comma-named richhyper file.  Size-descending (ls -S) so the big
# files launch first under -j and overlap the long tail of small ones.
SRCS   := $(shell git ls-files '*.org' | xargs ls -S)
HTMLS  := $(patsubst %.org,$(OUT)/%.html,$(SRCS))

# literate-elisp bootstrap, mirrors tools/migrate-dryrun.org.
# literate-elisp-load resolves via `load' (load-path), so pass an absolute path.
BOOT   := --eval '(progn (add-to-list (quote load-path) "$(LELISP)") \
                         (require (quote literate-elisp)) \
                         (literate-elisp-load (expand-file-name "$(PUB)")))'

.PHONY: all index clean
all: $(HTMLS) index

# default per-file recipe (2 GB GC, set inside axiom-publish.org)
$(OUT)/%.html: %.org $(PUB)
	$(EMACS) -Q --batch $(BOOT) --eval '(axiom-publish-file "$<" "$@")'

# bookvol10.5.org: 56k headings, ~519s CPU (deterministic).  Standalone wall is
# ~9.6 min, but under -j6 it shares cores with the pool, so its wall stretches
# past 15 min -> 30 min cap (3x headroom; safe even at half-core efficiency).
# 6 GB GC.  This explicit rule overrides the pattern rule for this one target.
$(OUT)/books/bookvol10.5.html: books/bookvol10.5.org $(PUB)
	timeout 1800 $(EMACS) -Q --batch $(BOOT) \
	  --eval '(setq axiom-publish-gc-bytes (* 6 1024 1024 1024))' \
	  --eval '(axiom-publish-file "$<" "$@")'

# single pass after all pages exist; reads the finished tree so it is complete
index: $(HTMLS)
	@sh gen-index.sh $(OUT) > $(OUT)/index.html
	@echo "index: $$(find $(OUT) -name '*.html' ! -name index.html | wc -l | tr -d ' ') pages -> $(OUT)/index.html"

clean:
	rm -rf $(OUT)
