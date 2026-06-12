;;; axiom-pamphlet.el --- Edit + preview Axiom .pamphlet literate LaTeX  -*- lexical-binding: t; -*-

;; Repo-local support for editing Tim Daly's literate Axiom pamphlets:
;; LaTeX prose interleaved with \begin{chunk}{name} ... \end{chunk}
;; verbatim code chunks (the chunk environment is defined in books/axiom.sty).
;;
;; The pamphlet is a self-contained LaTeX document, so preview is a plain
;; latexmk/pdflatex compile -- noweb tools are NOT required to typeset it.
;;
;; Loaded by the repository .dir-locals.el.  See that file.

(require 'tex-site nil t)            ; AUCTeX
(require 'latex nil t)

(defgroup axiom-pamphlet nil
  "Editing and preview support for Axiom .pamphlet files."
  :group 'tex :prefix "axiom-pamphlet-")

(defcustom axiom-pamphlet-chunk-mode 'lisp-mode
  "Major mode applied to code inside \\begin{chunk}...\\end{chunk}.
Most chunks in this repository are Lisp/BOOT; change per book if needed."
  :type 'function :group 'axiom-pamphlet)

(defconst axiom-pamphlet-chunk-begin-re
  "^[ \t]*\\\\begin{chunk}{\\([^}]*\\)}"
  "Match a chunk opening; group 1 is the chunk name.")

(defconst axiom-pamphlet-chunk-end-re
  "^[ \t]*\\\\end{chunk}"
  "Match a chunk closing.")

;;;; ---------------------------------------------------------------- imenu
(defun axiom-pamphlet--imenu-create-index ()
  "Index every chunk definition by name for imenu / completion."
  (let (index)
    (save-excursion
      (goto-char (point-min))
      (while (re-search-forward axiom-pamphlet-chunk-begin-re nil t)
        (push (cons (match-string-no-properties 1)
                    (copy-marker (match-beginning 0)))
              index)))
    (nreverse index)))

;;;; ----------------------------------------------------- chunk navigation
(defun axiom-pamphlet--chunk-at-point ()
  "Return the chunk name in a \\getchunk{...}/{chunk}{...} on the current line."
  (save-excursion
    (let ((p (point)) (eol (line-end-position)))
      (goto-char (line-beginning-position))
      (catch 'hit
        (while (re-search-forward "\\\\\\(?:get\\)?chunk{\\([^}]*\\)}" eol t)
          (when (<= (match-beginning 0) p (match-end 0))
            (throw 'hit (match-string-no-properties 1))))
        nil))))

(defun axiom-pamphlet-goto-chunk (name)
  "Jump to the definition of chunk NAME.
Interactively, default to the \\getchunk{...} at point, else complete."
  (interactive
   (list (let ((d (axiom-pamphlet--chunk-at-point)))
           (completing-read
            (format "Chunk%s: " (if d (format " (default %s)" d) ""))
            (mapcar #'car (axiom-pamphlet--imenu-create-index))
            nil nil nil nil d))))
  (let ((pos (save-excursion
               (goto-char (point-min))
               (when (re-search-forward
                      (concat "^[ \t]*\\\\begin{chunk}{"
                              (regexp-quote name) "}")
                      nil t)
                 (match-beginning 0)))))
    (if pos (progn (push-mark) (goto-char pos) (recenter 0))
      (user-error "No chunk named %S" name))))

;;;; ------------------------------------------------------ AUCTeX knowledge
(defun axiom-pamphlet--auctex-setup ()
  "Teach AUCTeX the Axiom literate macros + treat chunk as verbatim."
  (when (featurep 'latex)
    (TeX-add-symbols
     '("getchunk" "Chunk name")
     '("calls" "Caller" "Callee")
     '("callsdollar" "Caller" "Callee")
     '("uses" "Function" "Variable")
     '("usesdollar" "Function" "Variable")
     '("usesstruct" "Function" "Struct"))
    (LaTeX-add-environments '("chunk" "Chunk name"))
    (when (boundp 'LaTeX-verbatim-environments-local)
      (add-to-list 'LaTeX-verbatim-environments-local "chunk"))))

;;;; ----------------------------------------------------- mmm chunk regions
(defun axiom-pamphlet--register-mmm-class ()
  "Register the mmm region class for chunk bodies (idempotent)."
  (mmm-add-classes
   `((axiom-chunk
      :submode ,axiom-pamphlet-chunk-mode
      :face mmm-code-submode-face
      :front ,(concat axiom-pamphlet-chunk-begin-re "[ \t]*\n")
      :back ,axiom-pamphlet-chunk-end-re
      :include-front nil
      :include-back nil))))

(defun axiom-pamphlet--enable-mmm ()
  "Overlay chunk bodies with `axiom-pamphlet-chunk-mode' via mmm-mode."
  (when (require 'mmm-mode nil t)
    (axiom-pamphlet--register-mmm-class)
    (setq-local mmm-classes '(axiom-chunk))
    (mmm-mode 1)))

;;;; -------------------------------------------------------- preview wiring
(defun axiom-pamphlet--preview-setup ()
  "Compile with PDF + SyncTeX; prefer PDF Tools as the viewer when present."
  (when (featurep 'tex)
    (setq-local TeX-PDF-mode t)
    (setq-local TeX-source-correlate-method 'synctex)
    (TeX-source-correlate-mode 1)
    ;; Prefer latexmk as the default build command when auctex-latexmk is set up.
    (when (and (boundp 'TeX-command-list)
               (assoc "LatexMk" TeX-command-list))
      (setq-local TeX-command-default "LatexMk"))
    ;; Use PDF Tools for in-Emacs preview + forward/back search when installed.
    (when (and (or (featurep 'pdf-tools) (locate-library "pdf-tools"))
               (boundp 'TeX-view-program-list-builtin)
               (assoc "PDF Tools" TeX-view-program-list-builtin))
      (setq-local TeX-view-program-selection
                  (cons '(output-pdf "PDF Tools")
                        (default-value 'TeX-view-program-selection))))))

;;;; --------------------------------------------------------------- the mode
;;;###autoload
(define-derived-mode axiom-pamphlet-mode LaTeX-mode "Axiom-Pamphlet"
  "Major mode for Axiom .pamphlet literate-LaTeX files.

Built on AUCTeX `LaTeX-mode': full compile/preview/reftex, plus
\\begin{chunk}...\\end{chunk} code chunks highlighted in
`axiom-pamphlet-chunk-mode' (via mmm-mode), chunk imenu, and
\\[axiom-pamphlet-goto-chunk] to jump from \\getchunk{X} to its definition."
  (axiom-pamphlet--auctex-setup)
  (setq-local imenu-create-index-function #'axiom-pamphlet--imenu-create-index)
  (when (fboundp 'reftex-mode) (reftex-mode 1))
  (axiom-pamphlet--preview-setup)
  (axiom-pamphlet--enable-mmm))

(define-key axiom-pamphlet-mode-map (kbd "C-c C-j") #'axiom-pamphlet-goto-chunk)

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.pamphlet\\'" . axiom-pamphlet-mode))

(provide 'axiom-pamphlet)
;;; axiom-pamphlet.el ends here
