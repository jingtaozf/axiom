#!/usr/bin/env python3
"""org-cleanup.py — Remove LaTeX residue from Axiom .org files.

Usage:
    python3 tools/org-cleanup.py [--dry-run] [--phase N] [FILE ...]
"""
import re, os, sys, argparse

def classify_lines(lines):
    """Yield (index, line, region) for each line.
    region: 'prose', 'src', 'example', 'quote', 'name'"""
    state = 'prose'
    for i, line in enumerate(lines):
        stripped = line.rstrip()
        if re.match(r'^#\+begin_src', stripped):
            state = 'src'; yield i, line, 'src'; continue
        if re.match(r'^#\+end_src', stripped):
            yield i, line, 'src'; state = 'prose'; continue
        if re.match(r'^#\+begin_example', stripped):
            state = 'example'; yield i, line, 'example'; continue
        if re.match(r'^#\+end_example', stripped):
            yield i, line, 'example'; state = 'prose'; continue
        if re.match(r'^#\+begin_quote', stripped):
            state = 'quote'; yield i, line, 'quote'; continue
        if re.match(r'^#\+end_quote', stripped):
            yield i, line, 'quote'; state = 'prose'; continue
        if state != 'prose':
            yield i, line, state; continue
        if re.match(r'^#\+NAME:', stripped):
            yield i, line, 'name'; continue
        yield i, line, 'prose'

# Lines to convert to #+LATEX: (preserve as raw LaTeX for export)
PREAMBLE_TO_LATEX = [
    r'^\\documentclass', r'^\\usepackage', r'^\\input\{bookheader',
    r'^\\begin\{document\}', r'^\\end\{document\}',
    r'^\\newcommand', r'^\\renewcommand', r'^\\providecommand',
    r'^\\setcounter', r'^\\setlength',
    r'^\\addcontentsline', r'^\\phantomsection',
    r'^\\bibliographystyle', r'^\\bibliography',
    r'^\\frenchspacing', r'^\\sloppy',
    r'^\\printindex', r'^\\makeindex',
    r'^\\appendix', r'^\\frontmatter', r'^\\VolumeName',
    r'^\\input\{',
]

# Lines to delete (pure formatting, no content)
PREAMBLE_DELETE = [
    r'^\\maketitle', r'^\\eject', r'^\\tableofcontents',
    r'^\\mainmatter', r'^\\pagenumbering', r'^\\cleardoublepage',
]

def convert_preamble_line(line):
    """Convert or delete a single preamble line.
    Returns (new_line_or_None, changed_bool)."""
    stripped = line.strip()
    # \title{...} → #+TITLE: ...
    m = re.match(r'^\\title\{(.+)\}\s*$', stripped)
    if m:
        title = m.group(1)
        # Strip $SPAD/path prefix (may be LaTeX-escaped as \$SPAD)
        for prefix in ['\\$SPAD/', '$SPAD/']:
            if title.startswith(prefix):
                rest = title[len(prefix):]
                title = rest.split(' ', 1)[-1] if ' ' in rest else rest
                break
        return '#+TITLE: ' + title + '\n', True
    # \author{...} → #+AUTHOR: ...
    m = re.match(r'^\\author\{(.+)\}\s*$', stripped)
    if m:
        return '#+AUTHOR: ' + m.group(1) + '\n', True
    # \begin{abstract} → #+BEGIN_abstract
    if re.match(r'^\\begin\{abstract\}\s*$', stripped):
        return '#+BEGIN_abstract\n', True
    # \end{abstract} → #+END_abstract
    if re.match(r'^\\end\{abstract\}\s*$', stripped):
        return '#+END_abstract\n', True
    # Convert to #+LATEX: (preserve as raw LaTeX for export)
    for pat in PREAMBLE_TO_LATEX:
        if re.match(pat, stripped):
            return '#+LATEX: ' + stripped + '\n', True
    # Delete pure formatting lines
    for pat in PREAMBLE_DELETE:
        if re.match(pat, stripped):
            return None, True
    return line, False

FORMAT_DELETE_PATTERNS = [
    r'^\\eject\s*$', r'^\\newpage\s*$', r'^\\pagebreak\s*$',
    r'^\\nopagebreak\s*$', r'^\\noindent\s*$', r'^\\newline\s*$',
    r'^\\par\s*$', r'^\\center\s*$', r'^\\centering\s*$',
    r'^\\raggedright\s*$', r'^\\smallbreak\s*$', r'^\\medbreak\s*$',
    r'^\\bigbreak\s*$', r'^\\largerbreak\s*$', r'^\\bigskip\s*$',
    r'^\\medskip\s*$', r'^\\smallskip\s*$',
]

def delete_format_standalone(line):
    """Delete standalone format commands. Returns None to delete."""
    stripped = line.strip()
    for pat in FORMAT_DELETE_PATTERNS:
        if re.match(pat, stripped):
            return None
    return line

def convert_inline_formatting(line):
    """Convert LaTeX inline formatting to org markup in prose text."""
    result = line
    changed = True
    iterations = 0
    # Quick skip: no LaTeX markers means no work needed
    if '\\' not in result:
        return result
    while changed and iterations < 3:
        changed = False
        iterations += 1
        prev = result
        # \textbf{X} → *X*
        result = re.sub(r'\\textbf\{([^{}]*(?:\{[^{}]*\}[^{}]*)*)\}',
                        lambda m: '*' + m.group(1) + '*', result)
        # \texttt{X} → =X=
        result = re.sub(r'\\texttt\{([^{}]*(?:\{[^{}]*\}[^{}]*)*)\}',
                        lambda m: '=' + m.group(1) + '=', result)
        # \emph{X} → /X/
        result = re.sub(r'\\emph\{([^{}]*(?:\{[^{}]*\}[^{}]*)*)\}',
                        lambda m: '/' + m.group(1) + '/', result)
        # \underline{X} → _X_
        result = re.sub(r'\\underline\{([^{}]*(?:\{[^{}]*\}[^{}]*)*)\}',
                        lambda m: '_' + m.group(1) + '_', result)
        # \textrm{X} → X (plain text)
        result = re.sub(r'\\textrm\{([^{}]*(?:\{[^{}]*\}[^{}]*)*)\}',
                        lambda m: m.group(1), result)
        # \textsuperscript{X} → ^{X}
        result = re.sub(r'\\textsuperscript\{([^{}]*)\}',
                        lambda m: '^{' + m.group(1) + '}', result)
        # {\bf X} → *X*
        result = re.sub(r'\{\\bf\s+([^{}]+)\}',
                        lambda m: '*' + m.group(1).strip() + '*', result)
        # {\tt X} → =X=
        result = re.sub(r'\{\\tt\s+([^{}]+)\}',
                        lambda m: '=' + m.group(1).strip() + '=', result)
        # {\sl X} → /X/
        result = re.sub(r'\{\\sl\s+([^{}]+)\}',
                        lambda m: '/' + m.group(1).strip() + '/', result)
        # {\it X} → /X/
        result = re.sub(r'\{\\it\s+([^{}]+)\}',
                        lambda m: '/' + m.group(1).strip() + '/', result)
        # {\em X} → /X/
        result = re.sub(r'\{\\em\s+([^{}]+)\}',
                        lambda m: '/' + m.group(1).strip() + '/', result)
        # {\rm X} → X
        result = re.sub(r'\{\\rm\s+([^{}]+)\}',
                        lambda m: m.group(1).strip(), result)
        # {\sc X} → X
        result = re.sub(r'\{\\sc\s+([^{}]+)\}',
                        lambda m: m.group(1).strip(), result)
        # \verb|X| → =X=
        result = re.sub(r'\\verb(.)(.*?)\1',
                        lambda m: '=' + m.group(2) + '=', result)
        # Simple replacements
        result = result.replace('\\ldots', '...')
        result = result.replace('\\dots', '...')
        result = result.replace('\\TeX', 'TeX')
        result = result.replace('\\LaTeX', 'LaTeX')
        result = result.replace('\\#', '#')
        if result != prev:
            changed = True
    return result

def convert_annotations(line):
    """Convert annotation macros to org comments."""
    for macro, tag in [
        ('calls', 'calls'), ('usesdollar', 'usesdollar'),
        ('callsdollar', 'callsdollar'), ('defdollar', 'defdollar'),
        ('refsdollar', 'refsdollar'), ('defsdollar', 'defsdollar'),
        ('uses', 'uses'), ('usesstruct', 'usesstruct'),
        ('catches', 'catches'), ('throws', 'throws'),
    ]:
        pat = r'\\' + macro + r'\{([^{}]+)\}\{([^{}]+)\}'
        m = re.search(pat, line)
        if m:
            line = (line[:m.start()] + '# ' + tag + ': ' +
                    m.group(1) + ' -> ' + m.group(2) + line[m.end():])
    return line

def convert_crossrefs(line):
    """Convert cross-reference macros to org equivalents."""
    # \cite{X} → [cite:@X]
    line = re.sub(r'\\cite\{([^{}]+)\}', r'[cite:@\1]', line)
    # \pageref{X} → [[#\1][pageref]]
    line = re.sub(r'\\pageref\{([^{}]+)\}', r'[[#\1][\1]]', line)
    return line

def convert_labels_refs_footnotes(line, footnote_counter):
    """Convert label, ref, footnote to org equivalents."""
    # \label{X} → org comment # label: X
    line = re.sub(r'\\label\{([^{}]+)\}', r'# label: \1', line)
    # \ref{X} → [[#X][X]]
    line = re.sub(r'\\ref\{([^{}]+)\}', r'[[#\1][\1]]', line)
    # \pageref{X} → [[#X][X]] (already in Phase 5, but belt-and-suspenders)
    line = re.sub(r'\\pageref\{([^{}]+)\}', r'[[#\1][\1]]', line)
    # \footnote{X} → [fn:N]
    m = re.search(r'\\footnote\{([^{}]*(?:\{[^{}]*\}[^{}]*)*)\}', line)
    if m:
        footnote_counter[0] += 1
        fn_num = footnote_counter[0]
        line = line[:m.start()] + f'[fn:{fn_num}]' + line[m.end():]
    return line

def cleanup_layout(line):
    """Remove inline layout commands that have no content."""
    # \hfill at end of line (but keep \\ line break marker)
    line = re.sub(r'\\hfill(\\\\)?$', lambda m: m.group(1) or '', line)
    # \hfill in middle (but not before \\ or #+NAME:)
    line = re.sub(r'\s*\\hfill\s*(?!\\\\|#+NAME)', ' ', line)
    # \quad / \qquad as spacing
    line = re.sub(r'\s*\\qquad\s*', '  ', line)
    line = re.sub(r'\s*\\quad\s*', ' ', line)
    # \hspace{...}
    line = re.sub(r'\\hspace\{[^{}]*\}', '', line)
    # Standalone \vskip, \hskip, \vspace
    if re.match(r'^\\vskip\s', line.strip()): return None
    if re.match(r'^\\hskip\s', line.strip()): return None
    if re.match(r'^\\vspace\{[^{}]*\}\s*$', line.strip()): return None
    # \sp at end of line
    line = re.sub(r'\\sp\s*$', '', line)
    # \allowbreak
    line = line.replace('\\allowbreak', '')
    return line

def process_file(filepath, dry_run=False, phases=None):
    """Process a single .org file through cleanup phases."""
    if phases is None:
        phases = {1, 2, 3, 4, 5, 6, 7}
    with open(filepath, 'r', errors='replace') as f:
        original_lines = f.readlines()
    lines = list(original_lines)
    changes = {'total': 0, 'by_phase': {}}
    footnote_counter = [0]  # mutable counter for footnotes

    # Phase 1: Preamble (only at file start, before any block)
    if 1 in phases:
        new_lines = []
        phase_changes = 0
        for i, line in enumerate(lines):
            # Convert preamble lines at file start (before first #+NAME: or #+begin)
            if i < 30:
                if not any(l.strip().startswith(('#+NAME:', '#+begin_src', '#+begin_example')) for l in lines[:i]):
                    new_line, changed = convert_preamble_line(line)
                    if changed:
                        phase_changes += 1
                        if new_line is not None:
                            new_lines.append(new_line)
                        continue
            new_lines.append(line)
        lines = new_lines
        changes['by_phase'][1] = phase_changes
        changes['total'] += phase_changes

    # Phases 2-6: Apply to prose lines only
    if any(p in phases for p in [2, 3, 4, 5, 6, 7]):
        classified = list(classify_lines(lines))
        new_lines = []
        phase_counts = {2: 0, 3: 0, 4: 0, 5: 0, 6: 0, 7: 0}
        for i, line, region in classified:
            if region != 'prose':
                new_lines.append(line)
                continue
            result = line
            # Phase 2: Delete standalone format commands
            if 2 in phases:
                r = delete_format_standalone(result)
                if r is None:
                    phase_counts[2] += 1; continue
                if r != result: phase_counts[2] += 1
                result = r
            # Phase 6: Layout cleanup
            if 6 in phases:
                r = cleanup_layout(result)
                if r is None:
                    phase_counts[6] += 1; continue
                if r != result: phase_counts[6] += 1
                result = r
            # Phase 3: Inline formatting
            if 3 in phases:
                r = convert_inline_formatting(result)
                if r != result: phase_counts[3] += 1
                result = r
            # Phase 4: Annotation macros
            if 4 in phases:
                r = convert_annotations(result)
                if r != result: phase_counts[4] += 1
                result = r
            # Phase 5: Cross-references
            if 5 in phases:
                r = convert_crossrefs(result)
                if r != result: phase_counts[5] += 1
                result = r
            # Phase 7: Labels, refs, footnotes
            if 7 in phases:
                r = convert_labels_refs_footnotes(result, footnote_counter)
                if r != result: phase_counts[7] += 1
                result = r
            new_lines.append(result)
        lines = new_lines
        for p, c in phase_counts.items():
            changes['by_phase'][p] = c
            changes['total'] += c

    if not dry_run and lines != original_lines:
        with open(filepath, 'w') as f:
            f.writelines(lines)
    return changes

def find_org_files(root):
    """Find all .org files in the tree."""
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d != '.git']
        for f in filenames:
            if f.endswith('.org'):
                yield os.path.join(dirpath, f)

def main():
    parser = argparse.ArgumentParser(
        description='Clean up LaTeX in Axiom .org files')
    parser.add_argument('--dry-run', action='store_true',
                        help='Show changes without modifying files')
    parser.add_argument('--phase', type=int, help='Run only this phase (1-6)')
    parser.add_argument('files', nargs='*', help='Specific files (default: all)')
    args = parser.parse_args()
    phases = {args.phase} if args.phase else {1, 2, 3, 4, 5, 6}
    files = args.files if args.files else sorted(find_org_files('.'))
    total_changes = 0
    total_files_changed = 0
    for filepath in files:
        if not os.path.exists(filepath):
            print(f"SKIP: {filepath} (not found)"); continue
        changes = process_file(filepath, dry_run=args.dry_run, phases=phases)
        if changes['total'] > 0:
            total_files_changed += 1
            total_changes += changes['total']
            phase_str = ' '.join(
                f"p{p}:{c}" for p, c in sorted(changes['by_phase'].items()) if c > 0)
            print(f"{'[DRY] ' if args.dry_run else ''}"
                  f"{filepath}: {changes['total']} changes ({phase_str})")
    print(f"\n{'[DRY RUN] ' if args.dry_run else ''}"
          f"Summary: {total_changes} changes across "
          f"{total_files_changed}/{len(files)} files")

if __name__ == '__main__':
    main()
