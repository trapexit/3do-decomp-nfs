/* SDK-origin operamath folio initialization and vector veneers. */

#include "find_kernel_item.h"
#include "runtime_error.h"

#include "folio.h"
#include "item.h"
#include "operamath.h"

#include "stdio.h"

#define OPERAMATH_OPEN_ERROR_TEXT "Error opening Operamath Folio\n"

enum
{
  OPERAMATH_DIV_UNSIGNED_OFFSET           = 0x0C,
  OPERAMATH_DIV_REMAINDER_UNSIGNED_OFFSET = 0x10,
  OPERAMATH_DIV_SIGNED_OFFSET             = 0x14,
  OPERAMATH_DIV_REMAINDER_SIGNED_OFFSET   = 0x18,
  OPERAMATH_RECIPROCAL_UNSIGNED_OFFSET    = 0x1C,
  OPERAMATH_RECIPROCAL_SIGNED_OFFSET      = 0x20,
  OPERAMATH_MULTIPLY_SIGNED_OFFSET        = 0x08,
  OPERAMATH_MULTIPLY_UNSIGNED_OFFSET      = 0x04
};

typedef ufrac16 (*OperamathDivideUnsignedFunction)(ufrac16 dividend_, ufrac16 divisor_);
typedef ufrac16 (*OperamathDivideRemainderUnsignedFunction)(uint32  *remainder_,
                                                            ufrac16  dividend_,
                                                            ufrac16  divisor_);
typedef frac16 (*OperamathDivideSignedFunction)(frac16 dividend_, frac16 divisor_);
typedef frac16 (*OperamathDivideRemainderSignedFunction)(int32  *remainder_,
                                                         frac16  dividend_,
                                                         frac16  divisor_);
typedef ufrac16 (*OperamathReciprocalUnsignedFunction)(ufrac16 divisor_);
typedef frac16 (*OperamathReciprocalSignedFunction)(frac16 divisor_);
typedef frac16 (*OperamathMultiplySignedFunction)(frac16 first_, frac16 second_);
typedef ufrac16 (*OperamathMultiplyUnsignedFunction)(ufrac16 first_, ufrac16 second_);

MathFolio *_MathBase;

Err
OpenMathFolio(void)
{
  Item item;

  item = OpenItem(find_named_kernel_item(MKNODEID(KERNELNODE, FOLIONODE), "Operamath"), NULL);
  if(item < 0)
    {
      printf(OPERAMATH_OPEN_ERROR_TEXT);
      print_runtime_error(item);
      return item;
    }

  _MathBase = (MathFolio *) LookupItem(item);
  return 0;
}


ufrac16
DivUF16(ufrac16 dividend_, ufrac16 divisor_)
{
  OperamathDivideUnsignedFunction function;

  function = (*(OperamathDivideUnsignedFunction *) ((uint8 *) _MathBase -
                                                     OPERAMATH_DIV_UNSIGNED_OFFSET));
  return function(dividend_, divisor_);
}


ufrac16
DivRemUF16(uint32 *remainder_, ufrac16 dividend_, ufrac16 divisor_)
{
  OperamathDivideRemainderUnsignedFunction function;

  function =
    (*(OperamathDivideRemainderUnsignedFunction *)
       ((uint8 *) _MathBase - OPERAMATH_DIV_REMAINDER_UNSIGNED_OFFSET));
  return function(remainder_, dividend_, divisor_);
}


frac16
DivSF16(frac16 dividend_, frac16 divisor_)
{
  OperamathDivideSignedFunction function;

  function = (*(OperamathDivideSignedFunction *) ((uint8 *) _MathBase -
                                                   OPERAMATH_DIV_SIGNED_OFFSET));
  return function(dividend_, divisor_);
}


frac16
DivRemSF16(int32 *remainder_, frac16 dividend_, frac16 divisor_)
{
  OperamathDivideRemainderSignedFunction function;

  function =
    (*(OperamathDivideRemainderSignedFunction *)
       ((uint8 *) _MathBase - OPERAMATH_DIV_REMAINDER_SIGNED_OFFSET));
  return function(remainder_, dividend_, divisor_);
}


ufrac16
RecipUF16(ufrac16 divisor_)
{
  OperamathReciprocalUnsignedFunction function;

  function = (*(OperamathReciprocalUnsignedFunction *)
                ((uint8 *) _MathBase - OPERAMATH_RECIPROCAL_UNSIGNED_OFFSET));
  return function(divisor_);
}


frac16
RecipSF16(frac16 divisor_)
{
  OperamathReciprocalSignedFunction function;

  function = (*(OperamathReciprocalSignedFunction *)
                ((uint8 *) _MathBase - OPERAMATH_RECIPROCAL_SIGNED_OFFSET));
  return function(divisor_);
}


frac16
MulSF16(frac16 first_, frac16 second_)
{
  OperamathMultiplySignedFunction function;

  function = (*(OperamathMultiplySignedFunction *) ((uint8 *) _MathBase -
                                                     OPERAMATH_MULTIPLY_SIGNED_OFFSET));
  return function(first_, second_);
}


ufrac16
MulUF16(ufrac16 first_, ufrac16 second_)
{
  OperamathMultiplyUnsignedFunction function;

  function = (*(OperamathMultiplyUnsignedFunction *)
                ((uint8 *) _MathBase - OPERAMATH_MULTIPLY_UNSIGNED_OFFSET));
  return function(first_, second_);
}
