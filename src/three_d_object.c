#include "three_d_object.h"

#include "asset_file_load.h"
#include "diagnostics.h"
#include "string_utilities.h"

#include "stddef.h"

#define THREE_D_OBJECT_PATH_BYTES 132
#define THREE_D_OBJECT_IDENTIFIER_BYTES 4
#define THREE_D_OBJECT_MINIMUM_VERSION 500
#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

COMPILE_TIME_SIZE_ASSERT(three_d_object_size, sizeof(ThreeDObject) == 0x28);
COMPILE_TIME_SIZE_ASSERT(three_d_object_shape_count_offset,
                         offsetof(ThreeDObject, shape_count) == 0x20);
COMPILE_TIME_SIZE_ASSERT(three_d_object_shape_table_offset,
                         offsetof(ThreeDObject, shape_table) == 0x24);

const uint8 G_THREE_D_OBJECT_IDENTIFIER[8]         = "ORI3";
const uint8 G_THREE_D_OBJECT_WRAPPER_IDENTIFIER[8] = "WRAP";
const char  G_INVALID_THREE_D_OBJECT_MESSAGE[32]   = "setori - NOT AN WRAP 3OR FILE.\n";
const char  G_OLD_THREE_D_OBJECT_MESSAGE[44]       = "setori - 3OR FILE IS NOT CURRENT VERSION\n";

ThreeDObject *
fixup_three_d_object(ThreeDObject *object_)
{
  if((compare_strings_limited(object_->identifier,
                              G_THREE_D_OBJECT_IDENTIFIER,
                              THREE_D_OBJECT_IDENTIFIER_BYTES) != 0) &&
     (compare_strings_limited(object_->identifier,
                              G_THREE_D_OBJECT_WRAPPER_IDENTIFIER,
                              THREE_D_OBJECT_IDENTIFIER_BYTES) != 0))
    {
      fatal_error(G_INVALID_THREE_D_OBJECT_MESSAGE);
    }

  if(object_->version < THREE_D_OBJECT_MINIMUM_VERSION)
    fatal_error(G_OLD_THREE_D_OBJECT_MESSAGE);

  object_->material_table = (uint8 *) object_ + (uint32) object_->material_table;
  object_->shape_table    = (uint8 *) object_ + (uint32) object_->shape_table;

  return object_;
}

ThreeDObject *
load_three_d_object(const char *path_, uint32 memory_flags_, int32 report_error_)
{
  char path[THREE_D_OBJECT_PATH_BYTES];

  copy_string(path, path_);
  append_string(path, ".3or");
  return fixup_three_d_object((ThreeDObject *) load_asset_file(path, memory_flags_, report_error_));
}

ThreeDObject *
load_three_d_object_with_error(const char *path_, uint32 memory_flags_)
{
  return load_three_d_object(path_, memory_flags_, 1);
}

ThreeDObject *
load_three_d_object_quiet(const char *path_, uint32 memory_flags_)
{
  return load_three_d_object(path_, memory_flags_, 0);
}
