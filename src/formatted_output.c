#include "formatted_output.h"

#include "debug.h"

#include "string.h"

#define FORMAT_VALUE_BUFFER_BYTES 258
#define FORMAT_KERNEL_BUFFER_BYTES 512
#define FORMAT_FIXED_FRACTION_DIGITS 5
#define FORMAT_FIXED_INTEGER_DIVISOR 10000
#define FORMAT_FIXED_FRACTION_MULTIPLIER 10
#define FORMAT_DECIMAL_BASE 10
#define FORMAT_HEXADECIMAL_BASE 16
#define FORMAT_OCTAL_BASE 8
#define FORMAT_BINARY_BASE 2
#define FORMAT_FIXED_POINT_SHIFT 16
#define FORMAT_FIXED_FRACTION_MASK 0xFFFF
#define FORMAT_PACKED_CHARACTER_SHIFT 8
#define FORMAT_ALTERNATE_PREFIX_BYTES 2

#define FORMAT_FLAG_LONG 0x01
#define FORMAT_FLAG_SHORT 0x02
#define FORMAT_FLAG_LEFT 0x04
#define FORMAT_FLAG_ZERO 0x08
#define FORMAT_FLAG_ALTERNATE 0x10

typedef struct FormatSpecification
{
  uint32 flags;
  int32  width;
  int32  precision;
  char   prefix;
  char   conversion;
} FormatSpecification;

typedef struct FormattedValue
{
  const char *text;
  int32       length;
  int32       precision_width;
  uint8       alternate_prefix;
} FormattedValue;

static const char LOWERCASE_DIGITS[] = "0123456789abcdef";
static const char UPPERCASE_DIGITS[] = "0123456789ABCDEF";
static const char NULL_TEXT[]        = "null";

int32
floating_point_display_disabled(void)
{
  return 0;
}

int32
sprintf(char *buffer_, const char *format_, ...)
{
  va_list arguments;
  int32   result;

  va_start(arguments, format_);
  result = vsprintf(buffer_, format_, arguments);
  va_end(arguments);
  return result;
}

int32
vsprintf(char *buffer_, const char *format_, va_list arguments_)
{
  char                value_buffer[FORMAT_VALUE_BUFFER_BYTES];
  char               *buffer_end;
  char               *integer_output;
  char               *output;
  const char         *digits;
  const char         *string;
  char                character;
  va_list             arguments;
  FormattedValue      formatted;
  FormatSpecification specification;
  uint32              value;
  uint32              base;
  uint32              quotient;
  uint32              digit;
  uint32              magnitude;
  uint32              integer;
  uint32              fraction;
  uint32              next_fraction;
  uint32              divisor;
  int32               signed_value;
  int32               parsed_value;
  int32               length;
  int32               character_count;
  int32               fraction_digit_count;
  int32               decorated_length;
  int32               field_length;
  int32               padding;
  int32               value_index;
  uint32              alignment_flags;
  uint8               alternate_octal;

  output     = buffer_;
  buffer_end = (char *) value_buffer + FORMAT_VALUE_BUFFER_BYTES;
  arguments  = arguments_;
  while((character = *format_) != 0)
    {
      ++format_;
      if(character != '%')
        {
          *output = character;
          ++output;
          continue;
        }

      specification.flags      = 0;
      specification.width      = 0;
      specification.precision  = -1;
      specification.prefix     = 0;
      specification.conversion = 0;
      for(;;)
        {
          character = *format_;
          ++format_;
          if(character == '+')
            specification.prefix = '+';
          else if(character == ' ')
            {
              if(specification.prefix == 0)
                specification.prefix = ' ';
            }
          else if(character == '#')
            specification.flags |= FORMAT_FLAG_ALTERNATE;
          else if(character == '-')
            specification.flags |= FORMAT_FLAG_LEFT;
          else if(character == '0')
            specification.flags |= FORMAT_FLAG_ZERO;
          else if(character == '*')
            {
              parsed_value = va_arg(arguments, int32);
              if(parsed_value < 0)
                {
                  parsed_value = -parsed_value;
                  specification.flags |= FORMAT_FLAG_LEFT;
                }
              specification.width = parsed_value;
            }
          else if((character >= '1') && (character <= '9'))
            {
              parsed_value = (int32) character - '0';
              while((*format_ >= '0') && (*format_ <= '9'))
                {
                  parsed_value = (parsed_value * FORMAT_DECIMAL_BASE) + ((int32) *format_ - '0');
                  ++format_;
                }
              specification.width = parsed_value;
            }
          else if(character == '.')
            {
              if(*format_ == '*')
                {
                  ++format_;
                  parsed_value = va_arg(arguments, int32);
                }
              else
                {
                  parsed_value = 0;
                  while((*format_ >= '0') && (*format_ <= '9'))
                    {
                      parsed_value =
                        (parsed_value * FORMAT_DECIMAL_BASE) + ((int32) *format_ - '0');
                      ++format_;
                    }
                }
              if(parsed_value < 0)
                parsed_value = -1;
              specification.precision = parsed_value;
            }
          else if(character == 'l')
            specification.flags |= FORMAT_FLAG_LONG;
          else if(character == 'h')
            specification.flags |= FORMAT_FLAG_SHORT;
          else
            {
              specification.conversion = character;
              break;
            }
        }

      if(specification.conversion == 0)
        break;

      formatted.alternate_prefix = 0;
      digits                     = LOWERCASE_DIGITS;
      base                       = FORMAT_DECIMAL_BASE;
      alternate_octal            = 0;
      switch(specification.conversion)
        {
          case 'X':
            digits = UPPERCASE_DIGITS;
            /* FALLTHROUGH */
          case 'x':
            value = va_arg(arguments, uint32);
            base  = FORMAT_HEXADECIMAL_BASE;
            formatted.alternate_prefix =
              ((specification.flags & FORMAT_FLAG_ALTERNATE) != 0) && (value != 0);
            specification.prefix = 0;
            break;

          case 'p':
            value                = (uint32) va_arg(arguments, void *);
            base                 = FORMAT_HEXADECIMAL_BASE;
            specification.prefix = 0;
            break;

          case 'O':
            specification.flags |= FORMAT_FLAG_LONG;
            /* FALLTHROUGH */
          case 'o':
            value                = va_arg(arguments, uint32);
            base                 = FORMAT_OCTAL_BASE;
            alternate_octal      = (specification.flags & FORMAT_FLAG_ALTERNATE) != 0;
            specification.prefix = 0;
            break;

          case 'b':
            value                = va_arg(arguments, uint32);
            base                 = FORMAT_BINARY_BASE;
            specification.prefix = 0;
            break;

          case 'U':
            specification.flags |= FORMAT_FLAG_LONG;
            /* FALLTHROUGH */
          case 'u':
            value                = va_arg(arguments, uint32);
            specification.prefix = 0;
            break;

          case 'D':
            specification.flags |= FORMAT_FLAG_LONG;
            /* FALLTHROUGH */
          case 'd':
          case 'i':
            signed_value = va_arg(arguments, int32);
            if(signed_value < 0)
              {
                value                = 0 - (uint32) signed_value;
                specification.prefix = '-';
              }
            else
              value = signed_value;
            break;

          case 'f':
            signed_value = va_arg(arguments, int32);
            if(signed_value < 0)
              {
                magnitude            = 0 - (uint32) signed_value;
                specification.prefix = '-';
              }
            else
              magnitude = signed_value;

            integer_output = (char *) value_buffer;
            integer        = magnitude >> FORMAT_FIXED_POINT_SHIFT;
            divisor        = FORMAT_FIXED_INTEGER_DIVISOR;
            while(divisor != 0)
              {
                digit = integer / divisor;
                if((digit != 0) || (integer_output != value_buffer) || (divisor == 1))
                  {
                    *integer_output = (char) (digit + '0');
                    ++integer_output;
                  }
                integer -= (digit * divisor);
                divisor /= FORMAT_DECIMAL_BASE;
              }

            fraction             = (magnitude & FORMAT_FIXED_FRACTION_MASK);
            fraction_digit_count = 0;
            if((fraction != 0) || (specification.precision > 0))
              {
                *integer_output = '.';
                ++integer_output;
                fraction *= FORMAT_FIXED_FRACTION_MULTIPLIER;
                while(fraction_digit_count < FORMAT_FIXED_FRACTION_DIGITS)
                  {
                    if((specification.precision >= 0) &&
                       (specification.precision <= fraction_digit_count))
                      break;
                    if((specification.precision < 0) && (fraction == 0))
                      break;

                    digit = fraction >> FORMAT_FIXED_POINT_SHIFT;
                    next_fraction =
                      (fraction & FORMAT_FIXED_FRACTION_MASK) * FORMAT_FIXED_FRACTION_MULTIPLIER;
                    ++fraction_digit_count;
                    if((digit != 0) || (next_fraction != 0))
                      {
                        *integer_output = (char) (digit + '0');
                        ++integer_output;
                      }
                    fraction = next_fraction;
                  }
              }

            formatted.text            = value_buffer;
            formatted.length          = (integer_output - value_buffer);
            formatted.precision_width = fraction_digit_count;
            goto write_formatted_value;

          case 'c':
            value                = va_arg(arguments, uint32);
            specification.prefix = 0;
            integer_output       = buffer_end;
            character_count      = 0;
            while(((specification.precision < 0) && (value != 0)) ||
                  ((specification.precision >= 0) && (character_count < specification.precision)))
              {
                --integer_output;
                *integer_output = (char) value;
                value >>= FORMAT_PACKED_CHARACTER_SHIFT;
                ++character_count;
              }
            formatted.text            = integer_output;
            formatted.length          = (buffer_end - integer_output);
            formatted.precision_width = 0;
            goto write_formatted_value;

          case 's':
            string = va_arg(arguments, const char *);
            if(string == NULL)
              string = NULL_TEXT;
            if(specification.precision < 0)
              length = strlen(string);
            else
              {
                length = 0;
                while((length < specification.precision) && (string[length] != 0))
                  ++length;
              }
            specification.prefix      = 0;
            formatted.text            = string;
            formatted.length          = length;
            formatted.precision_width = 0;
            goto write_formatted_value;

          default:
            *output = specification.conversion;
            ++output;
            continue;
        }

      if(specification.precision >= 0)
        specification.flags &= ~FORMAT_FLAG_ZERO;
      integer_output = buffer_end;
      if((value != 0) || (specification.precision != 0))
        {
          do
            {
              quotient = value / base;
              digit    = value - (quotient * base);
              --integer_output;
              *integer_output = digits[digit];
              value           = quotient;
            }
          while(value != 0);

          if((alternate_octal != 0) && (*integer_output != '0'))
            {
              --integer_output;
              *integer_output = '0';
            }
        }
      formatted.text            = integer_output;
      formatted.length          = (buffer_end - integer_output);
      formatted.precision_width = specification.precision;

    write_formatted_value:
      decorated_length = formatted.length;
      if(specification.prefix != 0)
        ++decorated_length;
      if(formatted.alternate_prefix != 0)
        decorated_length += FORMAT_ALTERNATE_PREFIX_BYTES;
      field_length = decorated_length;
      if(formatted.precision_width > field_length)
        field_length = formatted.precision_width;

      alignment_flags = (specification.flags & (FORMAT_FLAG_LEFT | FORMAT_FLAG_ZERO));
      if(alignment_flags == 0)
        {
          padding = (specification.width - field_length);
          while(padding > 0)
            {
              *output = ' ';
              ++output;
              --padding;
            }
        }
      if(specification.prefix != 0)
        {
          *output = specification.prefix;
          ++output;
        }
      if(formatted.alternate_prefix != 0)
        {
          *output = '0';
          ++output;
          *output = specification.conversion;
          ++output;
        }
      if(alignment_flags == FORMAT_FLAG_ZERO)
        {
          padding = (specification.width - field_length);
          while(padding > 0)
            {
              *output = '0';
              ++output;
              --padding;
            }
        }
      padding = (formatted.precision_width - decorated_length);
      while(padding > 0)
        {
          *output = '0';
          ++output;
          --padding;
        }
      for(value_index = 0; value_index < formatted.length; ++value_index)
        {
          *output = formatted.text[value_index];
          ++output;
        }
      if((specification.flags & FORMAT_FLAG_LEFT) != 0)
        {
          padding = (specification.width - field_length);
          while(padding > 0)
            {
              *output = ' ';
              ++output;
              --padding;
            }
        }
    }

  *output = 0;
  return 0;
}

void
print_formatted_kernel_message(const char *format_, ...)
{
  char    message[FORMAT_KERNEL_BUFFER_BYTES];
  va_list arguments;

  va_start(arguments, format_);
  vsprintf(message, format_, arguments);
  va_end(arguments);
  kprintf("%s", message);
}
