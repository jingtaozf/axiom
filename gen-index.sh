#!/bin/sh
# gen-index.sh OUTDIR  -> writes (to stdout) an index.html of every published
# page, grouped by directory.  Reads the finished output tree, so the index is
# always complete regardless of how the pages were built in parallel.
out="$1"
printf '<!doctype html>\n<html><head><meta charset="utf-8">\n'
printf '<title>axiom org -&gt; HTML</title>\n'
printf '<style>body{font:14px/1.5 sans-serif;margin:2rem;max-width:60rem}'
printf 'h2{margin-top:1.5em;border-bottom:1px solid #ccc;font-size:1em;color:#555}'
printf 'a{display:inline-block;margin:.1em 1.2em .1em 0}</style>\n</head><body>\n'
n=$(find "$out" -name '*.html' ! -name index.html | wc -l | tr -d ' ')
printf '<h1>axiom &mdash; %s pages</h1>\n' "$n"
find "$out" -name '*.html' ! -name index.html | sort | sed "s#^$out/##" | awk '
  # last-slash split via substr (portable; avoids BSD-awk regex char-class quirks)
  { p=$0; pos=0
    for (i=length(p); i>=1; i--) if (substr(p,i,1)=="/") { pos=i; break }
    dir  = (pos ? substr(p,1,pos-1) : ".")
    base = (pos ? substr(p,pos+1)   : p)
    if (dir!=prev) { printf "<h2>%s</h2>\n", dir; prev=dir }
    printf "<a href=\"%s\">%s</a>\n", p, base }'
printf '</body></html>\n'
