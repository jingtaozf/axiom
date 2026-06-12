;;; Directory Local Variables for the Axiom repository.  -*- no-byte-compile: t -*-
;;; Loads axiom-pamphlet.el and switches .pamphlet buffers into
;;; axiom-pamphlet-mode.  The first time you open a .pamphlet, Emacs asks
;;; whether to run this eval form -- answer "!" to trust it permanently.

((nil
  . ((eval
      . (let ((lib (locate-dominating-file
                    (or buffer-file-name default-directory)
                    "axiom-pamphlet.el")))
          (when lib
            (load (expand-file-name "axiom-pamphlet.el" lib) nil t))
          (when (and buffer-file-name
                     (string-match-p "\\.pamphlet\\'" buffer-file-name)
                     (fboundp 'axiom-pamphlet-mode)
                     (not (derived-mode-p 'axiom-pamphlet-mode)))
            (axiom-pamphlet-mode)))))))
