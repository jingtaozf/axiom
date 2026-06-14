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

  int in_src = 0, first = 1;
  for (i = 0; i < nspans; i++) {
    char *s = buf + sp[i].start;
    long len = sp[i].len;

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

    if (!first) putchar('\n'); first = 0;
    fwrite(s, 1, (size_t) len, stdout);   /* identity */
  }
  return 0;
}
