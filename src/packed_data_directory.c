#include "packed_data_directory.h"

#define PACKED_DIRECTORY_COUNT_OFFSET 8
#define PACKED_DIRECTORY_TAG_TABLE_OFFSET 16
#define PACKED_DIRECTORY_OFFSET_TABLE 20
#define PACKED_DIRECTORY_ENTRY_BYTES 8
#define PACKED_DIRECTORY_BYTE_ZERO 0
#define PACKED_DIRECTORY_BYTE_ONE 1
#define PACKED_DIRECTORY_BYTE_TWO 2
#define PACKED_DIRECTORY_BYTE_THREE 3
#define PACKED_DIRECTORY_HIGH_BYTE_SHIFT 24
#define PACKED_DIRECTORY_HIGH_MID_SHIFT 16
#define PACKED_DIRECTORY_LOW_MID_SHIFT 8

uint32
get_packed_directory_entry_count(const void *directory_)
{
  const uint8 *directory;

  directory = (const uint8 *) directory_;
  return *(const uint32 *) (directory + PACKED_DIRECTORY_COUNT_OFFSET);
}

const uint8 *
get_packed_directory_entry(const void *directory_, uint32 index_)
{
  const uint8  *directory;
  const uint32 *entry_offsets;

  directory = (const uint8 *) directory_;
  if(index_ < *(const uint32 *) (directory + PACKED_DIRECTORY_COUNT_OFFSET))
    {
      entry_offsets = (const uint32 *) (directory + PACKED_DIRECTORY_OFFSET_TABLE +
                                        (index_ * PACKED_DIRECTORY_ENTRY_BYTES));
      return directory + *entry_offsets;
    }

  return NULL;
}

void
write_packed_directory_entry_tag(const void *directory_, uint32 index_, uint8 *destination_)
{
  const uint8  *directory;
  const uint32 *entry_offsets;
  uint32        offset;

  directory = (const uint8 *) directory_;
  if(index_ < *(const uint32 *) (directory + PACKED_DIRECTORY_COUNT_OFFSET))
    {
      entry_offsets = (const uint32 *) (directory + PACKED_DIRECTORY_TAG_TABLE_OFFSET +
                                        (index_ * PACKED_DIRECTORY_ENTRY_BYTES));
      offset        = *entry_offsets;
      destination_[PACKED_DIRECTORY_BYTE_ZERO] =
        (uint8) (offset >> PACKED_DIRECTORY_HIGH_BYTE_SHIFT);
      destination_[PACKED_DIRECTORY_BYTE_ONE] = (uint8) (offset >> PACKED_DIRECTORY_HIGH_MID_SHIFT);
      destination_[PACKED_DIRECTORY_BYTE_TWO] = (uint8) (offset >> PACKED_DIRECTORY_LOW_MID_SHIFT);
      destination_[PACKED_DIRECTORY_BYTE_THREE] = (uint8) offset;
      return;
    }

  destination_[PACKED_DIRECTORY_BYTE_ZERO]  = 0;
  destination_[PACKED_DIRECTORY_BYTE_ONE]   = 0;
  destination_[PACKED_DIRECTORY_BYTE_TWO]   = 0;
  destination_[PACKED_DIRECTORY_BYTE_THREE] = 0;
}
