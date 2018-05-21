((c++-mode
  (eval add-hook 'before-save-hook #'clang-format-buffer nil t))
 (c-mode
  (eval add-hook 'before-save-hook #'clang-format-buffer nil t)))
