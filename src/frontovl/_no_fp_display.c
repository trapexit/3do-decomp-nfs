/* Floating-point formatting is unsupported by the title's printf runtime. */
int
_no_fp_display(int     conversion_,
               double *value_,
               char    buffer_[],
               int     flags_,
               char  **prefix_,
               int    *precision_,
               int    *before_decimal_,
               int    *after_decimal_);

int
_no_fp_display(int     conversion_,
               double *value_,
               char    buffer_[],
               int     flags_,
               char  **prefix_,
               int    *precision_,
               int    *before_decimal_,
               int    *after_decimal_)
{
  (void) conversion_;
  (void) value_;
  (void) buffer_;
  (void) flags_;
  (void) prefix_;
  (void) precision_;
  (void) before_decimal_;
  (void) after_decimal_;

  return 0;
}
