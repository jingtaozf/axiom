/* orgtangle.c -- tangle source directly from a migrated .org, byte-identical
 * to tanglec(untanglec(org)) but WITHOUT ever materialising the .pamphlet.
 *
 * Why not a "native" noweb tangler over the org?  tanglec advances a fixed
 * `getlen+12' past each \getchunk{...} (the literal pamphlet syntax length);
 * when a getchunk line carries trailing bytes the original pamphlet kept those
 * as a literal \getchunk{..}<junk> line, which the forward org conversion
 * preserves verbatim (it only rewrites a CLEAN <<name>>).  A native org tangler
 * cannot know such a literal line should expand -- so it could never reproduce
 * tanglec byte-for-byte.  The only byte-exact "tangle from org" is therefore:
 *   1. untanglec's transform: .org bytes -> the exact .pamphlet bytes, in memory
 *   2. tanglec's tangler: run verbatim on that buffer
 * Composed here in one tool, so the source build reads .org and writes source
 * with no .pamphlet on disk (the .pamphlet is rebuilt only for the PDF path).
 *
 * Usage: orgtangle file.org [chunkname]   (default chunk: *)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

/* ---- tanglec's globals + algorithm, verbatim (operating on `buffer') ---- */
char *buffer;
int bufsize;

int nextline(int i) {
  int j;
  if (i >= bufsize) return (-1);
  for (j = 0; ((i + j < bufsize) && (buffer[i + j] != '\n')); j++);
  return (j);
}
int printline(int i, int length) {
  int j;
  for (j = 0; j < length; j++) { putchar(buffer[i + j]); }
  printf("\n");
  return (0);
}
int getchunk(char *chunkname);
int foundchunk(int i, char *chunkname) {
  if ((strncmp(&buffer[i + 14], chunkname, strlen(chunkname)) == 0) &&
      (strncmp(&buffer[i], "\\begin{chunk}", 13) == 0) &&
      (buffer[i + 13] == '{') &&
      (buffer[i + 14 + strlen(chunkname)] == '}')) {
    return (1);
  }
  return (0);
}
int foundEnd(int i, char *chunkname) {
  if ((buffer[i] == '\\') &&
      (strncmp(&buffer[i + 1], "end{chunk}", 10) == 0)) {
    return (1);
  }
  return (0);
}
int foundGetchunk(int i, int linelen) {
  int len;
  if (strncmp(&buffer[i], "\\getchunk{", 10) == 0) {
    for (len = 0; ((len < linelen) && (buffer[i + len] != '}')); len++);
    return (len - 10);
  }
  return (0);
}
char *getChunkname(int k, int getlen) {
  char *result = (char *) malloc(getlen + 1);
  strncpy(result, &buffer[k + 10], getlen);
  result[getlen] = '\0';
  return (result);
}
int printchunk(int i, int chunklinelen, char *chunkname) {
  int k;
  int linelen;
  char *getname;
  int getlen = 0;
  for (k = i + chunklinelen + 1; ((linelen = nextline(k)) != -1);) {
    if ((getlen = foundGetchunk(k, linelen)) > 0) {
      getname = getChunkname(k, getlen);
      getchunk(getname);
      free(getname);
      k = k + getlen + 12l;
    } else {
      if ((linelen >= 11) && (foundEnd(k, chunkname) == 1)) {
        return (k + 12);
      } else {
        printline(k, linelen);
        k = k + linelen + 1;
      }
    }
  }
  return (k);
}
int getchunk(char *chunkname) {
  int i;
  int linelen;
  int chunklen = strlen(chunkname);
  for (i = 0; ((linelen = nextline(i)) != -1);) {
    if ((linelen >= chunklen + 15) && (foundchunk(i, chunkname) == 1)) {
      i = printchunk(i, linelen, chunkname);
    } else {
      i = i + linelen + 1;
    }
  }
  return (i);
}

/* ---- untanglec's transform: .org bytes -> .pamphlet bytes in `out' ---- */
static char *org;
static long orgsize;
static char *out;
static long outcap, outlen;

static void oreserve(long n) {
  if (outlen + n > outcap) {
    while (outlen + n > outcap) outcap = outcap * 2 + 4096;
    out = realloc(out, outcap);
  }
}
static void oputc(char c) { oreserve(1); out[outlen++] = c; }
static void owrite(const char *p, long n) { oreserve(n); memcpy(out + outlen, p, n); outlen += n; }
static void oputs(const char *s) { owrite(s, (long) strlen(s)); }

typedef struct { long start; long len; } Span;
static int hasprefix(const char *s, long len, const char *p) {
  long pl = (long) strlen(p);
  return len >= pl && memcmp(s, p, pl) == 0;
}

static void org_to_pamphlet(void) {
  long nl = 0, i;
  for (i = 0; i < orgsize; i++) if (org[i] == '\n') nl++;
  long nspans = nl + 1;
  Span *sp = malloc((size_t) nspans * sizeof(Span));
  long si = 0, ls = 0;
  for (i = 0; i < orgsize; i++) {
    if (org[i] == '\n') { sp[si].start = ls; sp[si].len = i - ls; si++; ls = i + 1; }
  }
  sp[si].start = ls; sp[si].len = orgsize - ls; si++;

  int in_src = 0, first = 1;
  for (i = 0; i < nspans; i++) {
    char *s = org + sp[i].start;
    long len = sp[i].len;
    if (hasprefix(s, len, "#+NAME: ")) {
      if (i + 1 < nspans) {
        char *n = org + sp[i + 1].start;
        long nlen = sp[i + 1].len;
        if (hasprefix(n, nlen, "#+begin_src ")) {
          long extra = -1, k;
          for (k = 0; k + 10 <= nlen; k++)
            if (memcmp(n + k, ":noweb yes", 10) == 0) { extra = k + 10; break; }
          if (!first) oputc('\n'); first = 0;
          oputs("\\begin{chunk}{"); owrite(s + 8, len - 8); oputc('}');
          if (extra >= 0) owrite(n + extra, nlen - extra);
          in_src = 1; i++; continue;
        }
      }
      if (!first) oputc('\n'); first = 0; owrite(s, len); continue;
    }
    if (in_src && hasprefix(s, len, "#+end_src")) {
      if (!first) oputc('\n'); first = 0;
      oputs("\\end{chunk}"); owrite(s + 9, len - 9); in_src = 0; continue;
    }
    if (in_src && len >= 4 && s[0] == '<' && s[1] == '<' &&
        s[len - 1] == '>' && s[len - 2] == '>') {
      int has_gt = 0; long k;
      for (k = 2; k <= len - 3; k++) if (s[k] == '>') { has_gt = 1; break; }
      if (!has_gt) {
        if (!first) oputc('\n'); first = 0;
        oputs("\\getchunk{"); owrite(s + 2, len - 4); oputc('}'); continue;
      }
    }
    if (!first) oputc('\n'); first = 0; owrite(s, len);
  }
  free(sp);
  /* mmap zero-fills the page tail past EOF; tanglec's strncmp probes can read a
     few bytes past the logical end, so mirror that with a zeroed pad here. */
  oreserve(64);
  memset(out + outlen, 0, outcap - outlen);
}

int main(int argc, char *argv[]) {
  if (argc < 2 || argc > 3) { fprintf(stderr, "Usage: orgtangle file.org [chunkname]\n"); return 1; }
  int fd = open(argv[1], O_RDONLY);
  if (fd < 0) { perror("open"); return 2; }
  struct stat st;
  if (fstat(fd, &st) < 0) { perror("fstat"); close(fd); return 3; }
  orgsize = (long) st.st_size;
  if (orgsize == 0) { close(fd); return 0; }
  org = mmap(0, orgsize, PROT_READ, MAP_PRIVATE, fd, 0);
  if (org == MAP_FAILED) { perror("mmap"); close(fd); return 4; }
  org_to_pamphlet();
  buffer = out;
  bufsize = (int) outlen;
  getchunk(argc == 3 ? argv[2] : "*");
  close(fd);
  return 0;
}
