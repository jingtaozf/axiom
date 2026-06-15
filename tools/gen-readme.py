import re, os, subprocess
TRACKED = subprocess.check_output(['git','ls-files','*.org']).decode().split()
def in_dir(d): return sorted(p for p in TRACKED if os.path.dirname(p)==d
                             and os.path.basename(p).lower()!='makefile.org'
                             and not p.endswith('.png.org'))
def clean(s):
    s = s.replace('\\$','$').replace('\\&','&').replace('\\_','_').replace('\\#','#')
    s = re.sub(r'[{}]','',s); s = re.sub(r'\\[a-zA-Z]+','',s)
    return re.sub(r'\s+',' ',s).strip()
def title_of(f):
    txt = open(f, errors='replace').read()
    m = re.search(r'\\newcommand\{\\VolumeName\}\{([^}]*)\}', txt)
    if m: return clean(m.group(1))
    m = re.search(r'^\\title\{(.+?)\}\s*$', txt, re.M)
    if m:
        mm = re.match(r'\\\$SPAD/\S+\s+(.+)', m.group(1))
        return clean(mm.group(1) if mm else m.group(1))
    m = re.search(r'^\*+ (.+)$', txt, re.M)
    if m: return clean(m.group(1))
    return os.path.basename(f)[:-4]
def link(f): return f"- [[{f}][{title_of(f)}]]"

books   = in_dir('books')
interp  = in_dir('src/interp')
algebra = in_dir('src/algebra')
others  = in_dir('src/doc') + in_dir('books/cookbook') + in_dir('docs/hyperdoc') + in_dir('docs') + in_dir('src/etc')
ninput  = len([p for p in TRACKED if p.startswith('src/input/') and p.endswith('.org')])
ncats   = len([p for p in TRACKED if p.startswith('docs/CATS/') and p.endswith('.org')])

o=['#+TITLE: Axiom — the 30 Year Horizon (org-native fork)','#+OPTIONS: toc:nil num:nil','',
   "jingtaozf's fork of Tim Daly's literate-programming Axiom, migrated to",
   '*org-native* sources: headings, item lists, and verbatim blocks are authored in',
   'org-mode so GitHub renders each =.org= directly, while the build still regenerates',
   'the exact original =.pamphlet= (hence the identical PDF) byte-for-byte.  This page',
   'indexes the literate sources by title.','','* Books']
o+=[link(f) for f in books]
o+=['','* Interpreter source (=src/interp=)']+[link(f) for f in interp]
if algebra: o+=['','* Algebra source (=src/algebra=)']+[link(f) for f in algebra]
if others: o+=['','* Other documents']+[link(f) for f in others]
o+=['','* Test inputs',
    f'Regression-test inputs live in =src/input/= ({ninput} files) and =docs/CATS/=',
    f'({ncats} files); they are not indexed individually here.','']
open('README.org','w').write('\n'.join(o)+'\n')
print(f"books={len(books)} interp={len(interp)} algebra={len(algebra)} others={len(others)} inputs={ninput}+{ncats} lines={len(o)+1}")
