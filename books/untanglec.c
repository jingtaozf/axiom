/* untanglec.c -- rebuild a .pamphlet from its .pamphlet.org, byte-for-byte.
 *
 * The exact inverse of `pamphlet-roundtrip-to-org' (tools/pamphlet-roundtrip.org):
 * only the chunk scaffolding was transformed, everything else is verbatim, so
 * the round trip is a pure byte identity.  Used at build time so `tanglec' (and
 * the rest of the Makefiles) keep operating on the same .pamphlet bytes after
 * the sources were migrated to .org.
 *
 *   #+NAME: NAME            )
 *   #+begin_src L :noweb yesEXTRA  ->  \begin{chunk}{NAME}EXTRA
 *   #+end_srcEXTRA          ->  \end{chunk}EXTRA
 *   <<NAME>>  (inside a src block)  ->  \getchunk{NAME}
 *   any other line         ->  itself
 *
 * Usage: untanglec file.pamphlet.org > file.pamphlet
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
/* Reverse the org-native heading rewrite on a PROSE line (outside chunks and
   \begin{verbatim} blocks).  Mirrors pamphlet-roundtrip's pr--prose-rev:
   `* X' .. `**** X' -> \chapter{X} .. \subsubsection{X}; a leading-comma escape
   (org's own ,*  convention) is stripped one level; everything else is verbatim. */
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
  /* comma-escaped dash-item ",- X" (e.g. a math-array row) -> strip one comma */
  if (c >= 1 && c < len && s[c] == '-' && c + 1 < len && s[c + 1] == ' ') {
    fwrite(s + 1, 1, (size_t) (len - 1), stdout); return;
  }
  fwrite(s, 1, (size_t) len, stdout);   /* identity */
}

/* a list-item line: "- " + at least one char of content */
static int is_dash_item(const char *s, long len) {
  return len >= 3 && s[0] == '-' && s[1] == ' ';
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

  /* split into line spans on '\n', keeping empty fields (so the exact
     trailing-newline state is preserved on join). */
  long nl = 0, i;
  for (i = 0; i < bufsize; i++) if (buf[i] == '\n') nl++;
  long nspans = nl + 1;
  Span *sp = malloc((size_t) nspans * sizeof(Span));
  long si = 0, ls = 0;
  for (i = 0; i < bufsize; i++) {
    if (buf[i] == '\n') { sp[si].start = ls; sp[si].len = i - ls; si++; ls = i + 1; }
  }
  sp[si].start = ls; sp[si].len = bufsize - ls; si++;

  int in_src = 0, in_verbatim = 0, first = 1;
  for (i = 0; i < nspans; i++) {
    char *s = buf + sp[i].start;
    long len = sp[i].len;

    /* Inside a prose \begin{verbatim} block every line is raw -- the forward
       never rewrote headings there, so we must not reverse them here. */
    if (in_verbatim) {
      if (!first) putchar('\n'); first = 0;
      fwrite(s, 1, (size_t) len, stdout);
      if (vb_end(s, len)) in_verbatim = 0;
      continue;
    }
    if (!in_src && vb_begin(s, len)) {
      if (!first) putchar('\n'); first = 0;
      fwrite(s, 1, (size_t) len, stdout);
      in_verbatim = 1;
      continue;
    }

    if (has_prefix(s, len, "#+NAME: ")) {
      if (i + 1 < nspans) {
        char *n = buf + sp[i + 1].start;
        long nlen = sp[i + 1].len;
        if (has_prefix(n, nlen, "#+begin_src ")) {
          long extra = -1, k;
          for (k = 0; k + 10 <= nlen; k++)
            if (memcmp(n + k, ":noweb yes", 10) == 0) { extra = k + 10; break; }
          if (!first) putchar('\n'); first = 0;
          fputs("\\begin{chunk}{", stdout);
          fwrite(s + 8, 1, (size_t) (len - 8), stdout);   /* NAME */
          putchar('}');
          if (extra >= 0) fwrite(n + extra, 1, (size_t) (nlen - extra), stdout);
          in_src = 1;
          i++;                          /* consume the #+begin_src line */
          continue;
        }
      }
      if (!first) putchar('\n'); first = 0;
      fwrite(s, 1, (size_t) len, stdout);   /* lone #+NAME -> verbatim */
      continue;
    }

    if (in_src && has_prefix(s, len, "#+end_src")) {
      if (!first) putchar('\n'); first = 0;
      fputs("\\end{chunk}", stdout);
      fwrite(s + 9, 1, (size_t) (len - 9), stdout);        /* EXTRA */
      in_src = 0;
      continue;
    }

    if (in_src && len >= 4 && s[0] == '<' && s[1] == '<' &&
        s[len - 1] == '>' && s[len - 2] == '>') {
      int has_gt = 0; long k;
      for (k = 2; k <= len - 3; k++) if (s[k] == '>') { has_gt = 1; break; }
      if (!has_gt) {
        if (!first) putchar('\n'); first = 0;
        fputs("\\getchunk{", stdout);
        fwrite(s + 2, 1, (size_t) (len - 4), stdout);      /* NAME */
        putchar('}');
        continue;
      }
    }

    /* a prose run of "- X" lines reverses to one itemize block, exactly as the
       forward dropped the \begin/\item/\end scaffolding */
    if (!in_src && is_dash_item(s, len)) {
      if (!first) putchar('\n'); first = 0;
      fputs("\\begin{itemize}", stdout);
      for (;;) {
        putchar('\n');
        fputs("\\item ", stdout);
        fwrite(s + 2, 1, (size_t) (len - 2), stdout);
        if (i + 1 < nspans) {
          char *ns = buf + sp[i + 1].start;
          long nl = sp[i + 1].len;
          if (is_dash_item(ns, nl)) { i++; s = ns; len = nl; continue; }
        }
        break;
      }
      putchar('\n');
      fputs("\\end{itemize}", stdout);
      continue;
    }

    if (!first) putchar('\n'); first = 0;
    if (in_src) fwrite(s, 1, (size_t) len, stdout);   /* chunk code: verbatim */
    else emit_prose(s, len);                          /* prose: reverse headings */
  }
  return 0;
}
