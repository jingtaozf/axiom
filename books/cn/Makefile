# 由 books/cn/Makefile.pamphlet 抽取,勿直接编辑;
# 重建: ./books/tanglec books/cn/Makefile.pamphlet '*' > books/cn/Makefile
SPD?=$(abspath ../..)
BOOKS?=${SPD}/books
IN?=${SPD}/books/cn
AXIOM?=${SPD}/mnt/MACOSX
PDF?=${AXIOM}/doc/cn
XELATEX?=xelatex -interaction=nonstopmode
MAKEINDEX?=makeindex
BIBTEX?=bibtex
RM?=rm -f
# texlive 缺失的 Axiom 私有宏包白名单;其余 books/*.sty 是 2012 快照,
# 在现代 texlive 上会压住系统版本导致硬错误,一律不拷
CNSTY=axiom.sty catmac.sty dissdef.sty multind.sty selatex.sty slatex.sty

all: ${PDF}/bookvol1.pdf ${PDF}/glossary.pdf ${PDF}/bookvol0.pdf

${PDF}:
	mkdir -p ${PDF}

${PDF}/axiom.bib: ${AXIOM}/doc/axiom.bib | ${PDF}
	cp $< $@

${PDF}/axiom.bst: ${AXIOM}/doc/axiom.bst | ${PDF}
	cp $< $@

${PDF}/%.pdf: ${IN}/%.pamphlet ${IN}/bookheader.tex ${PDF}/axiom.bib ${PDF}/axiom.bst
	@ echo ===========================================
	@ echo making ${PDF}/$*.pdf from ${IN}/$*.pamphlet
	@ echo ===========================================
	@(cd ${PDF} ; \
	  (cd ${BOOKS} ; cp ${CNSTY} ${PDF}) ; \
	  cp ${AXIOM}/doc/master ${PDF} ; \
	  cp ${IN}/$*.pamphlet ${PDF} ; \
	  cp ${IN}/bookheader.tex ${PDF} ; \
	  cp -pr ${BOOKS}/ps ${PDF} ; \
	  ${RM} $*.toc ; \
	  touch signatures.tex ; \
	  touch $*.sigs ; \
	  ${XELATEX} $*.pamphlet >/dev/null ; \
	  ${MAKEINDEX} $*.idx 1>/dev/null 2>/dev/null ; \
	  ${BIBTEX} $*.aux ; \
	  ${XELATEX} $*.pamphlet >/dev/null ; \
	  ${XELATEX} $*.pamphlet >/dev/null ; \
	  ${RM} $*.aux $*.idx $*.ilg $*.ind $*.pamphlet ; \
	  ${RM} $*.sigs signatures.tex signatures.sort bookheader.tex )

.PHONY: all
