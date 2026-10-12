#include "resource_partition.h"

#include "diagnostics.h"
#include "string_utilities.h"

#include "stddef.h"

#define RESOURCE_PARTITION_NAME_BYTES 8
#define RESOURCE_PARTITION_ENTRY_BYTES 0x1C
#define RESOURCE_TAG_ENTRY_BYTES 0x0C
#define RESOURCE_PARTITION_INVALID_ID_MESSAGE "GetPartition: invalid partition ID"
#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

typedef struct ResourcePartitionHeader
{
  uint8  reserved_00[0x54];
  int32  partition_count;
  uint32 partition_directory_offset;
  uint32 reserved_5c;
  uint32 partition_data_offset;
  int32  object_tag_count;
  uint32 object_tag_directory_offset;
  int32  shape_tag_count;
  uint32 shape_tag_directory_offset;
} ResourcePartitionHeader;

typedef struct ResourcePartitionEntry
{
  char  name[RESOURCE_PARTITION_NAME_BYTES];
  uint8 reserved_08[4];
  int32 element_count;
  int32 element_offset;
  uint8 reserved_14[8];
} ResourcePartitionEntry;

typedef struct ResourceTagEntry
{
  char  name[RESOURCE_PARTITION_NAME_BYTES];
  int32 index;
} ResourceTagEntry;

COMPILE_TIME_SIZE_ASSERT(resource_partition_entry_size,
                         sizeof(ResourcePartitionEntry) == RESOURCE_PARTITION_ENTRY_BYTES);
COMPILE_TIME_SIZE_ASSERT(resource_partition_count_offset,
                         offsetof(ResourcePartitionHeader, partition_count) == 0x54);
COMPILE_TIME_SIZE_ASSERT(resource_partition_directory_offset,
                         offsetof(ResourcePartitionHeader, partition_directory_offset) == 0x58);
COMPILE_TIME_SIZE_ASSERT(resource_partition_data_offset,
                         offsetof(ResourcePartitionHeader, partition_data_offset) == 0x60);
COMPILE_TIME_SIZE_ASSERT(resource_object_tag_count_offset,
                         offsetof(ResourcePartitionHeader, object_tag_count) == 0x64);
COMPILE_TIME_SIZE_ASSERT(resource_object_tag_directory_offset,
                         offsetof(ResourcePartitionHeader, object_tag_directory_offset) == 0x68);
COMPILE_TIME_SIZE_ASSERT(resource_shape_tag_count_offset,
                         offsetof(ResourcePartitionHeader, shape_tag_count) == 0x6C);
COMPILE_TIME_SIZE_ASSERT(resource_shape_tag_directory_offset,
                         offsetof(ResourcePartitionHeader, shape_tag_directory_offset) == 0x70);
COMPILE_TIME_SIZE_ASSERT(resource_tag_entry_size,
                         sizeof(ResourceTagEntry) == RESOURCE_TAG_ENTRY_BYTES);

int32
find_resource_object_tag_index(const void *resource_, const char *name_)
{
  const ResourcePartitionHeader *header;
  const ResourceTagEntry        *entry;
  int32                          entry_index;

  header = (const ResourcePartitionHeader *) resource_;
  entry =
    (const ResourceTagEntry *) ((const uint8 *) resource_ + header->object_tag_directory_offset);
  for(entry_index = 0; entry_index < header->object_tag_count; entry_index += 1)
    {
      if(compare_strings_limited_ignore_case(entry->name, name_, RESOURCE_PARTITION_NAME_BYTES) ==
         0)
        return entry->index;
      entry += 1;
    }

  return -1;
}

int32
find_resource_shape_tag_index(const void *resource_, const char *name_)
{
  const ResourcePartitionHeader *header;
  const ResourceTagEntry        *entry;
  int32                          entry_index;

  header = (const ResourcePartitionHeader *) resource_;
  entry =
    (const ResourceTagEntry *) ((const uint8 *) resource_ + header->shape_tag_directory_offset);
  for(entry_index = 0; entry_index < header->shape_tag_count; entry_index += 1)
    {
      if(compare_strings_limited_ignore_case(entry->name, name_, RESOURCE_PARTITION_NAME_BYTES) ==
         0)
        return entry->index;
      entry += 1;
    }

  return -1;
}

int32
find_resource_partition_id(const void *resource_, const char *name_)
{
  const ResourcePartitionHeader *header;
  const ResourcePartitionEntry  *entry;
  int32                          partition_id;

  header = (const ResourcePartitionHeader *) resource_;
  entry  = (const ResourcePartitionEntry *) ((const uint8 *) resource_ +
                                             header->partition_directory_offset);
  for(partition_id = 0; partition_id < header->partition_count; partition_id += 1)
    {
      if(compare_strings_limited_ignore_case(entry->name, name_, RESOURCE_PARTITION_NAME_BYTES) ==
         0)
        return partition_id;
      entry += 1;
    }

  return -1;
}

void
get_resource_partition(const void *resource_,
                       int32       partition_id_,
                       int32      *element_count_,
                       int32     **elements_)
{
  const ResourcePartitionHeader *header;
  const ResourcePartitionEntry  *entry;
  int32                         *partition_data;

  header = (const ResourcePartitionHeader *) resource_;
  if((uint32) partition_id_ >= (uint32) header->partition_count)
    fatal_error(RESOURCE_PARTITION_INVALID_ID_MESSAGE);

  entry           = (const ResourcePartitionEntry *) ((const uint8 *) resource_ +
                                                      header->partition_directory_offset) +
                    partition_id_;
  partition_data  = (int32 *) ((const uint8 *) resource_ + header->partition_data_offset);
  *element_count_ = entry->element_count;
  *elements_      = partition_data + entry->element_offset;
}

int32 *
get_resource_partition_data(const void *resource_, int32 partition_id_, int32 *element_count_)
{
  const ResourcePartitionHeader *header;
  const ResourcePartitionEntry  *entry;
  int32                         *partition_data;

  header = (const ResourcePartitionHeader *) resource_;
  if((uint32) partition_id_ >= (uint32) header->partition_count)
    fatal_error(RESOURCE_PARTITION_INVALID_ID_MESSAGE);

  entry           = (const ResourcePartitionEntry *) ((const uint8 *) resource_ +
                                                      header->partition_directory_offset) +
                    partition_id_;
  partition_data  = (int32 *) ((const uint8 *) resource_ + header->partition_data_offset);
  *element_count_ = entry->element_count;
  return partition_data + entry->element_offset;
}
