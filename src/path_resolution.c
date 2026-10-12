#include "path_resolution.h"

#include "file_information.h"
#include "string_utilities.h"

#define PATH_EXTENSION_BYTES 4
#define PATH_EXTENSION_SEPARATOR '.'

char *
find_path_with_extensions(const char *path_, char *output_, const char *extensions_)
{
  const char *extension;
  char       *suffix;

  copy_string(output_, path_);
  if(find_last_character(output_, PATH_EXTENSION_SEPARATOR) != NULL)
    return output_;

  suffix                       = output_ + string_length(output_);
  suffix[PATH_EXTENSION_BYTES] = '\0';
  extension                    = extensions_;
  while(*extension != '\0')
    {
      copy_string_padded(suffix, extension, PATH_EXTENSION_BYTES);
      if(file_exists(output_) != 0)
        return output_;

      extension += PATH_EXTENSION_BYTES;
    }

  *suffix = '\0';
  return output_;
}
