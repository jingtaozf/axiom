/* untanglec.c -- rebuild a .pamphlet from its .pamphlet.org.
 *
 * Reverse the org-native cleanup transforms so `tanglec' (and the rest of
 * the Makefiles) keep operating on valid .pamphlet bytes.  The output is
 * NOT byte-identical to the original pamphlet (that contract was relaxed),
 * but it IS valid LaTeX that tanglec can extract chunks from.
 *
 * Transforms (org -> pamphlet):
 *
 *   Chunk scaffolding (unchanged from original):
 *     #+NAME: NAME + #+begin_src L :noweb yesEXTRA  ->  \begin{chunk}{NAME}EXTRA
 *     #+end_srcEXTRA                                  ->  \end{chunk}EXTRA
 *     <<NAME>> (inside src)                           ->  \getchunk{NAME}
 *
 *   New org-native constructs:
 *     #+LATEX: \foo ...         ->  \foo ...           (strip prefix)
 *     #+TITLE: X                ->  \title{X}
 *     #+AUTHOR: X               ->  \author{X}
 *     #+BEGIN_abstract          ->  \begin{abstract}
 *     #+END_abstract            ->  \end{abstract}
 *     #+BEGIN_LATEX             ->  (enter passthrough mode)
 *     #+END_LATEX               ->  (exit passthrough mode)
 *     #+INDEX: X                ->  \index{X}
 *     # label: X                ->  \label{X}
 *     # calls: fn -> target     ->  \calls{fn}{target}
 *     # usesdollar: fn -> var   ->  \usesdollar{fn}{var}
 *     # callsdollar: fn -> var  ->  \callsdollar{fn}{var}
 *     # defdollar: fn -> var    ->  \defdollar{fn}{var}
 *     # refsdollar: fn -> var   ->  \refsdollar{fn}{var}
 *     # defsdollar: fn -> var   ->  \defsdollar{fn}{var}
 *     # uses: fn -> target      ->  \uses{fn}{target}
 *     # usesstruct: fn -> tgt   ->  \usesstruct{fn}{tgt}
 *     # catches: fn -> target   ->  \catches{fn}{target}
 *     # throws: fn -> target    ->  \throws{fn}{target}
 *     1. X (enumerate items)    ->  \begin{enumerate}\item X\end{enumerate}
 *     - X (itemize items)       ->  \begin{itemize}\item X\end{itemize}
 *     * X .. **** X (headings)  ->  \chapter{X} .. \subsubsection{X}
 *     #+begin_example / #+end   ->  \begin{verbatim} / \end{verbatim}
 *
 *   Everything else: verbatim pass-through.
 *
 * Usage: untanglec file.org > file.pamphlet
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

static char *buf;
static long bufsize;

typedef struct { long start; long len; } Span;

static int has_prefix(const char *s, long len, const char *p) {
  long pl = (long) strlen(p);
  return len >= pl && memcmp(s, p, pl) == 0;
}

/* trimmed-equal to "\begin{verbatim}" */
static int vb_begin(const char *s, long len) {
  long a = 0, b = len;
  while (a < b && (s[a] == ' ' || s[a] == '\t')) a++;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) b--;
  return b - a == 16 && memcmp(s + a, "\\begin{verbatim}", 16) == 0;
}
/* trimmed ends with "\end{verbatim}" */
static int vb_end(const char *s, long len) {
  long b = len;
  while (b > 0 && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) b--;
  return b >= 14 && memcmp(s + b - 14, "\\end{verbatim}", 14) == 0;
}
/* trimmed-equal to TOK */
static int trimmed_eq(const char *s, long len, const char *tok, long *lead, long *trail) {
  long a = 0, b = len, tl = (long) strlen(tok);
  while (a < b && (s[a] == ' ' || s[a] == '\t')) a++;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) b--;
  if (b - a == tl && memcmp(s + a, tok, tl) == 0) { *lead = a; *trail = b; return 1; }
  return 0;
}

/* Emit a line that starts with PREFIX followed by content → \CMD{content} */
static void emit_wrapped(const char *s, long len, const char *prefix, const char *cmd) {
  long pl = (long) strlen(prefix);
  fputs(cmd, stdout);
  putchar('{');
  /* strip trailing \r */
  long end = len;
  while (end > pl && s[end - 1] == '\r') end--;
  fwrite(s + pl, 1, (size_t)(end - pl), stdout);
  putchar('}');
}

/* Reverse the org-native heading rewrite on a PROSE line */
static void emit_prose(char *s, long len) {
  long c = 0;
  while (c < len && s[c] == ',') c++;
  if (c < len && s[c] == '*') {
    if (s[0] == ',') { fwrite(s + 1, 1, (size_t) (len - 1), stdout); return; }
    long ns = 0;
    while (ns < len && s[ns] == '*') ns++;
    if (ns >= 1 && ns <= 4 && ns < len && s[ns] == ' ') {
      static const char *cmd[5] = { 0, "\\chapter{", "\\section{",
                                    "\\subsection{", "\\subsubsection{" };
      fputs(cmd[ns], stdout);
      fwrite(s + ns + 1, 1, (size_t) (len - ns - 1), stdout);
      putchar('}');
      return;
    }
  }
  /* comma-escaped dash-item ",- X" -> strip one comma */
  if (c >= 1 && c < len && s[c] == '-' && c + 1 < len && s[c + 1] == ' ') {
    fwrite(s + 1, 1, (size_t) (len - 1), stdout); return;
  }
  /* comma-escaped example marker */
  if (c >= 1 && ((len - c == 15 && memcmp(s + c, "#+begin_example", 15) == 0) ||
                 (len - c == 13 && memcmp(s + c, "#+end_example",   13) == 0))) {
    fwrite(s + 1, 1, (size_t) (len - 1), stdout); return;
  }
  fwrite(s, 1, (size_t) len, stdout);   /* identity */
}

/* a list-item line: "- " + at least one char of content */
static int is_dash_item(const char *s, long len) {
  return len >= 3 && s[0] == '-' && s[1] == ' ';
}

/* a numbered-item line: "1. " + at least one char of content */
static int is_num_item(const char *s, long len) {
  if (len < 4) return 0;
  if (s[0] < '0' || s[0] > '9') return 0;
  long k = 1;
  while (k < len && s[k] >= '0' && s[k] <= '9') k++;
  return k < len && s[k] == '.' && k + 1 < len && s[k + 1] == ' ';
}

/* get content after "N. " prefix */
static long num_item_content_start(const char *s, long len) {
  long k = 1;
  while (k < len && s[k] >= '0' && s[k] <= '9') k++;
  return k + 2; /* skip ". " */
}

int main(int argc, char *argv[]) {
  if (argc != 2) { fprintf(stderr, "Usage: untanglec file.org\n"); return 1; }
  int fd = open(argv[1], O_RDONLY);
  if (fd < 0) { perror("open"); return 2; }
  struct stat st;
  if (fstat(fd, &st) < 0) { perror("fstat"); close(fd); return 3; }
  bufsize = (long) st.st_size;
  if (bufsize == 0) { close(fd); return 0; }
  buf = mmap(0, bufsize, PROT_READ, MAP_PRIVATE, fd, 0);
  if (buf == MAP_FAILED) { perror("mmap"); close(fd); return 4; }

  /* split into line spans on '\n' */
  long nl = 0, i;
  for (i = 0; i < bufsize; i++) if (buf[i] == '\n') nl++;
  long nspans = nl + 1;
  Span *sp = malloc((size_t) nspans * sizeof(Span));
  long si = 0, ls = 0;
  for (i = 0; i < bufsize; i++) {
    if (buf[i] == '\n') { sp[si].start = ls; sp[si].len = i - ls; si++; ls = i + 1; }
  }
  sp[si].start = ls; sp[si].len = bufsize - ls; si++;

  int in_src = 0, in_verbatim = 0, in_example = 0, in_latex_block = 0, first = 1;
  long lead, trail;
  for (i = 0; i < nspans; i++) {
    char *s = buf + sp[i].start;
    long len = sp[i].len;

    /* strip trailing \r for matching */
    long slen = len;
    while (slen > 0 && s[slen - 1] == '\r') slen--;

    /* --- State checks (inside blocks) --- */

    /* Inside #+BEGIN_LATEX ... #+END_LATEX: emit content verbatim */
    if (in_latex_block) {
      if (trimmed_eq(s, slen, "#+END_LATEX", &lead, &trail)) {
        in_latex_block = 0;
        continue;  /* skip the #+END_LATEX marker */
      }
      if (!first) putchar('\n'); first = 0;
      fwrite(s, 1, (size_t) len, stdout);
      continue;
    }

    /* Inside #+begin_example: reverse to \begin{verbatim} */
    if (in_example) {
      if (!first) putchar('\n'); first = 0;
      if (trimmed_eq(s, slen, "#+end_example", &lead, &trail)) {
        fwrite(s, 1, (size_t) lead, stdout);
        fputs("\\end{verbatim}", stdout);
        fwrite(s + trail, 1, (size_t) (len - trail), stdout);
        in_example = 0;
      } else {
        fwrite(s, 1, (size_t) len, stdout);
      }
      continue;
    }

    /* Inside raw \begin{verbatim} block */
    if (in_verbatim) {
      if (!first) putchar('\n'); first = 0;
      fwrite(s, 1, (size_t) len, stdout);
      if (vb_end(s, slen)) in_verbatim = 0;
      continue;
    }

    /* --- Block openers --- */

    if (!in_src && trimmed_eq(s, slen, "#+begin_example", &lead, &trail)) {
      if (!first) putchar('\n'); first = 0;
      fwrite(s, 1, (size_t) lead, stdout);
      fputs("\\begin{verbatim}", stdout);
      fwrite(s + trail, 1, (size_t) (len - trail), stdout);
      in_example = 1;
      continue;
    }
    if (!in_src && vb_begin(s, slen)) {
      if (!first) putchar('\n'); first = 0;
      fwrite(s, 1, (size_t) len, stdout);
      in_verbatim = 1;
      continue;
    }
    if (trimmed_eq(s, slen, "#+BEGIN_LATEX", &lead, &trail)) {
      in_latex_block = 1;
      continue;  /* skip the marker, emit content */
    }

    /* --- Chunk scaffolding (unchanged) --- */

    if (has_prefix(s, slen, "#+NAME: ")) {
      if (i + 1 < nspans) {
        char *n = buf + sp[i + 1].start;
        long nlen = sp[i + 1].len;
        /* strip \r from next line for matching */
        long nnlen = nlen;
        while (nnlen > 0 && n[nnlen - 1] == '\r') nnlen--;
        if (has_prefix(n, nnlen, "#+begin_src ")) {
          long extra = -1, k;
          for (k = 0; k + 10 <= nnlen; k++)
            if (memcmp(n + k, ":noweb yes", 10) == 0) { extra = k + 10; break; }
          if (!first) putchar('\n'); first = 0;
          fputs("\\begin{chunk}{", stdout);
          fwrite(s + 8, 1, (size_t) (slen - 8), stdout);
          putchar('}');
          if (extra >= 0) fwrite(n + extra, 1, (size_t) (nlen - extra), stdout);
          in_src = 1;
          i++;
          continue;
        }
      }
      if (!first) putchar('\n'); first = 0;
      fwrite(s, 1, (size_t) len, stdout);
      continue;
    }

    if (in_src && has_prefix(s, slen, "#+end_src")) {
      if (!first) putchar('\n'); first = 0;
      fputs("\\end{chunk}", stdout);
      fwrite(s + 9, 1, (size_t) (len - 9), stdout);
      in_src = 0;
      continue;
    }

    if (in_src && slen >= 4 && s[0] == '<' && s[1] == '<' &&
        s[slen - 1] == '>' && s[slen - 2] == '>') {
      int has_gt = 0; long k;
      for (k = 2; k <= slen - 3; k++) if (s[k] == '>') { has_gt = 1; break; }
      if (!has_gt) {
        if (!first) putchar('\n'); first = 0;
        fputs("\\getchunk{", stdout);
        fwrite(s + 2, 1, (size_t) (slen - 4), stdout);
        putchar('}');
        continue;
      }
    }

    /* Inside src block: verbatim */
    if (in_src) {
      if (!first) putchar('\n'); first = 0;
      fwrite(s, 1, (size_t) len, stdout);
      continue;
    }

    /* --- New org-native construct reversals --- */

    /* #+LATEX: \foo ... → \foo ... (strip prefix) */
    if (has_prefix(s, slen, "#+LATEX: ")) {
      if (!first) putchar('\n'); first = 0;
      fwrite(s + 9, 1, (size_t) (len - 9), stdout);
      continue;
    }

    /* #+TITLE: X → \title{X} */
    if (has_prefix(s, slen, "#+TITLE: ")) {
      if (!first) putchar('\n'); first = 0;
      emit_wrapped(s, len, "#+TITLE: ", "\\title");
      continue;
    }

    /* #+AUTHOR: X → \author{X} */
    if (has_prefix(s, slen, "#+AUTHOR: ")) {
      if (!first) putchar('\n'); first = 0;
      emit_wrapped(s, len, "#+AUTHOR: ", "\\author");
      continue;
    }

    /* #+BEGIN_abstract → \begin{abstract} */
    if (trimmed_eq(s, slen, "#+BEGIN_abstract", &lead, &trail)) {
      if (!first) putchar('\n'); first = 0;
      fputs("\\begin{abstract}", stdout);
      continue;
    }

    /* #+END_abstract → \end{abstract} */
    if (trimmed_eq(s, slen, "#+END_abstract", &lead, &trail)) {
      if (!first) putchar('\n'); first = 0;
      fputs("\\end{abstract}", stdout);
      continue;
    }

    /* #+INDEX: X → \index{X} */
    if (has_prefix(s, slen, "#+INDEX: ")) {
      if (!first) putchar('\n'); first = 0;
      emit_wrapped(s, len, "#+INDEX: ", "\\index");
      continue;
    }

    /* # label: X → \label{X} */
    if (has_prefix(s, slen, "# label: ")) {
      if (!first) putchar('\n'); first = 0;
      emit_wrapped(s, len, "# label: ", "\\label");
      continue;
    }

    /* # TAG: fn -> target → \TAG{fn}{target} */
    {
      static const struct { const char *prefix; const char *macro; } ann[] = {
        { "# calls: ",        "\\calls" },
        { "# usesdollar: ",   "\\usesdollar" },
        { "# callsdollar: ",  "\\callsdollar" },
        { "# defdollar: ",    "\\defdollar" },
        { "# refsdollar: ",   "\\refsdollar" },
        { "# defsdollar: ",   "\\defsdollar" },
        { "# uses: ",         "\\uses" },
        { "# usesstruct: ",   "\\usesstruct" },
        { "# catches: ",      "\\catches" },
        { "# throws: ",       "\\throws" },
        { "# sig: ",          "\\sig" },
        { NULL, NULL }
      };
      int matched = 0;
      for (int ai = 0; ann[ai].prefix && !matched; ai++) {
        if (has_prefix(s, slen, ann[ai].prefix)) {
          /* parse "fn -> target" or "fn -> var" */
          long pl = (long) strlen(ann[ai].prefix);
          long end = slen;
          while (end > pl && s[end - 1] == '\r') end--;
          /* find " -> " separator */
          long sep = -1;
          for (long k = pl; k + 3 < end; k++) {
            if (s[k] == ' ' && s[k+1] == '-' && s[k+2] == '>' && s[k+3] == ' ') {
              sep = k; break;
            }
          }
          if (sep >= 0) {
            if (!first) putchar('\n'); first = 0;
            fputs(ann[ai].macro, stdout);
            putchar('{');
            fwrite(s + pl, 1, (size_t)(sep - pl), stdout);
            putchar('}');
            putchar('{');
            fwrite(s + sep + 4, 1, (size_t)(end - sep - 4), stdout);
            putchar('}');
            matched = 1;
          }
        }
      }
      if (matched) continue;
    }

    /* --- List environments --- */

    /* Itemize: "- X" lines */
    if (is_dash_item(s, slen)) {
      if (!first) putchar('\n'); first = 0;
      fputs("\\begin{itemize}", stdout);
      for (;;) {
        putchar('\n');
        fputs("\\item ", stdout);
        fwrite(s + 2, 1, (size_t) (len - 2), stdout);
        if (i + 1 < nspans) {
          char *ns = buf + sp[i + 1].start;
          long nlen = sp[i + 1].len;
          long nslen = nlen;
          while (nslen > 0 && ns[nslen - 1] == '\r') nslen--;
          if (is_dash_item(ns, nslen)) { i++; s = ns; len = nlen; continue; }
        }
        break;
      }
      putchar('\n');
      fputs("\\end{itemize}", stdout);
      continue;
    }

    /* Enumerate: "1. X" lines */
    if (is_num_item(s, slen)) {
      if (!first) putchar('\n'); first = 0;
      fputs("\\begin{enumerate}", stdout);
      for (;;) {
        putchar('\n');
        fputs("\\item ", stdout);
        long cs = num_item_content_start(s, slen);
        fwrite(s + cs, 1, (size_t) (len - cs), stdout);
        if (i + 1 < nspans) {
          char *ns = buf + sp[i + 1].start;
          long nlen = sp[i + 1].len;
          long nslen = nlen;
          while (nslen > 0 && ns[nslen - 1] == '\r') nslen--;
          if (is_num_item(ns, nslen)) { i++; s = ns; len = nlen; continue; }
        }
        break;
      }
      putchar('\n');
      fputs("\\end{enumerate}", stdout);
      continue;
    }

    /* --- Prose: reverse headings, pass through everything else --- */
    if (!first) putchar('\n'); first = 0;
    emit_prose(s, len);
  }
  putchar('\n');
  return 0;
}
