;; Emacs mode for .tasm files
;; (to allow syntax highlighting)
;;
;; Author: Rachit Dhar
;; ******************************

(require 'subr-x)

(defvar tasm-mode-syntax-table
  (let ((table (make-syntax-table)))
	(modify-syntax-entry ?/ ". 124b" table)
	(modify-syntax-entry ?\n "> b" table)
    table))

(defun tasm-keywords ()
  '("put" "mov" "cmp" "jmp" "je" "jne" "jg" "jge" "jl" "jle" "call"
    "and" "or" "xor" "not" "lsh" "rsh" "add" "sub" "mul" "div" "ret" "out" "hlt"))

(defun tasm-font-lock-keywords ()
  (list
   `("\"[^\"]*\"" . font-lock-string-face)
   `(,(regexp-opt (tasm-keywords) 'symbols) . font-lock-keyword-face)))

(define-derived-mode tasm-mode prog-mode "TASM"
  "Major mode for editing TASM files."
  :syntax-table tasm-mode-syntax-table
  (setq-local font-lock-defaults '(tasm-font-lock-keywords))
  (setq-local comment-start "// "))

(provide 'tasm-mode)