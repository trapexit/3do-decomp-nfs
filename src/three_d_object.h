#pragma once

#include "types.h"

typedef struct ThreeDObject
{
  uint8  identifier[4];
  int32  file_bytes;
  int32  version;
  int32  flags;
  int32  material_count;
  uint8 *material_table;
  int32  vertex_count;
  int32  polygon_count;
  int32  shape_count;
  uint8 *shape_table;
} ThreeDObject;

extern const uint8 G_THREE_D_OBJECT_IDENTIFIER[8];
extern const uint8 G_THREE_D_OBJECT_WRAPPER_IDENTIFIER[8];
extern const char  G_INVALID_THREE_D_OBJECT_MESSAGE[32];
extern const char  G_OLD_THREE_D_OBJECT_MESSAGE[44];

ThreeDObject *
fixup_three_d_object(ThreeDObject *object_);
ThreeDObject *
load_three_d_object(const char *path_, uint32 memory_flags_, int32 report_error_);
ThreeDObject *
load_three_d_object_with_error(const char *path_, uint32 memory_flags_);
ThreeDObject *
load_three_d_object_quiet(const char *path_, uint32 memory_flags_);
