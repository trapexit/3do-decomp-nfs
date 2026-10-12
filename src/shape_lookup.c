#include "shape_lookup.h"

#include "diagnostics.h"

#define SHAPE_NAME_BYTE_1_SHIFT 24
#define SHAPE_NAME_BYTE_2_SHIFT 16
#define SHAPE_NAME_BYTE_3_SHIFT 8
#define SHAPE_NAME_BYTE_3_INDEX 2
#define SHAPE_NAME_BYTE_4_INDEX 3
#define SHAPE_COLLECTION_HEADER_BYTES 16
#define SHAPE_DIRECTORY_ENTRY_BYTES 8
#define SHAPE_NOT_FOUND_MESSAGE_BYTES 44
#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

typedef struct ShapeCollectionHeader
{
  uint32 identifier;
  uint32 byte_size;
  int32  shape_count;
  uint32 reserved;
} ShapeCollectionHeader;

typedef struct ShapeDirectoryEntry
{
  uint32 shape_name;
  uint32 shape_offset;
} ShapeDirectoryEntry;

COMPILE_TIME_SIZE_ASSERT(shape_collection_header_size,
                         sizeof(ShapeCollectionHeader) == SHAPE_COLLECTION_HEADER_BYTES);
COMPILE_TIME_SIZE_ASSERT(shape_directory_entry_size,
                         sizeof(ShapeDirectoryEntry) == SHAPE_DIRECTORY_ENTRY_BYTES);

const char G_SHAPE_NOT_FOUND_MESSAGE[SHAPE_NOT_FOUND_MESSAGE_BYTES] =
  "localeshape - '%-4.4s' SHAPE NOT FOUND\r\n";

void *
locate_shape(const void *collection_, const uint32 *shape_name_)
{
  return locate_shape_entry(collection_, shape_name_, NULL);
}

void *
require_shape(const void *collection_, const uint32 *shape_name_)
{
  return locate_shape_entry(collection_, shape_name_, G_SHAPE_NOT_FOUND_MESSAGE);
}

void *
locate_shape_entry(const void *collection_, const uint32 *shape_name_, const char *error_format_)
{
  const ShapeCollectionHeader *header;
  const ShapeDirectoryEntry   *entry;
  const uint8                 *name_bytes;
  int32                        remaining;
  uint32                       shape_name;

  name_bytes = (const uint8 *) shape_name_;
  shape_name = (((uint32) name_bytes[0] << SHAPE_NAME_BYTE_1_SHIFT) |
                ((uint32) name_bytes[1] << SHAPE_NAME_BYTE_2_SHIFT) |
                ((uint32) name_bytes[SHAPE_NAME_BYTE_3_INDEX] << SHAPE_NAME_BYTE_3_SHIFT) |
                (uint32) name_bytes[SHAPE_NAME_BYTE_4_INDEX]);
  header     = (const ShapeCollectionHeader *) collection_;
  entry      = (const ShapeDirectoryEntry *) (header + 1);
  remaining  = header->shape_count;
  while(remaining > 0)
    {
      if(entry->shape_name == shape_name)
        return (void *) ((const uint8 *) collection_ + entry->shape_offset);

      entry++;
      remaining--;
    }

  if(error_format_ != NULL)
    fatal_error(error_format_, &shape_name);

  return NULL;
}
