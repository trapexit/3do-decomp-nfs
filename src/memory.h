#pragma once

#include "types.h"

typedef struct MemoryBlockDescriptor     MemoryBlockDescriptor;
typedef struct MemoryBlockList           MemoryBlockList;
typedef struct MemoryPoolLayout          MemoryPoolLayout;
typedef struct MemoryRegionConfiguration MemoryRegionConfiguration;

#define MEMORY_REGION_COUNT 8

struct MemoryBlockDescriptor
{
  void                  *payload;
  char                   name[12];
  uint32                 allocated_bytes;
  uint32                 requested_bytes;
  uint32                 flags;
  uint32                 allocation_id;
  MemoryBlockDescriptor *next;
  MemoryBlockDescriptor *previous;
};

struct MemoryBlockList
{
  MemoryBlockDescriptor *first;
  MemoryBlockDescriptor *last;
};

struct MemoryRegionConfiguration
{
  MemoryBlockDescriptor *first;
  MemoryBlockDescriptor *last;
  uint32                 size_alignment_mask;
  uint32                 address_alignment_mask;
  uint32                 next_region_flags;
};

struct MemoryPoolLayout
{
  void *dram_start;
  void *dram_end;
  void *primary_vram_start;
  void *primary_vram_end;
  void *secondary_vram_start;
  void *secondary_vram_end;
};

typedef struct MemoryManagerState
{
  MemoryBlockList       *allocated_blocks;
  MemoryBlockDescriptor *free_descriptors;
  uint32                 workspace_word_count;
  uint32                *workspace;
  uint32                *workspace_end;
  int32                  minimum_available_bytes;
  uint32                 next_allocation_id;
  Item                   lock_item;
} MemoryManagerState;

extern MemoryManagerState        g_MEMORY_MANAGER_STATE;
extern MemoryRegionConfiguration g_MEMORY_REGIONS[MEMORY_REGION_COUNT];
extern void                     *g_MEMORY_ALLOCATION_END;
extern void                     *g_MEMORY_ALLOCATION_START;
extern MemoryPoolLayout          g_MEMORY_POOL_LAYOUT;

void *
allocate_named_memory(const char *name_, uint32 byte_count_, uint32 flags_);
void *
allocate_named_memory_silently(const char *name_, uint32 byte_count_, uint32 flags_);
void *
allocate_memory_payload(const char *name_, uint32 byte_count_, uint32 flags_, int32 report_error_);
MemoryBlockDescriptor *
allocate_memory_record_locked(const char *name_,
                              uint32      byte_count_,
                              uint32      flags_,
                              int32       report_error_);
MemoryBlockDescriptor *
allocate_memory_record_core(const char *name_,
                            uint32      byte_count_,
                            uint32      flags_,
                            int32       report_error_);
MemoryRegionConfiguration *
initialize_memory_region(uint32 flags_,
                         void  *lower_address_,
                         void  *upper_address_,
                         uint32 size_alignment_,
                         uint32 address_alignment_,
                         uint32 next_region_flags_);
void
append_memory_region(uint32 destination_flags_,
                     uint32 region_flags_,
                     void  *lower_address_,
                     void  *upper_address_,
                     uint32 size_alignment_,
                     uint32 address_alignment_,
                     uint32 next_region_flags_);
void
append_memory_gap(uint32 flags_, void *lower_address_, void *upper_address_);
uint32
release_memory_at_address(uint32 flags_, void *address_);
void
configure_memory_manager_workspace(uint32 *workspace_, uint32 workspace_word_count_);
int32
find_largest_free_memory_block(uint32 type_bits_);
int32
allocate_memory_block_with_reserve(int32  requested_bytes_,
                                   int32  reserved_bytes_,
                                   uint32 type_bits_);
int32
initialize_memory_manager(int32 descriptor_count_,
                          int32 dram_requested_bytes_,
                          int32 dram_reserved_bytes_,
                          int32 vram_requested_bytes_,
                          int32 vram_reserved_bytes_);
int32
initialize_memory_manager_default(int32 descriptor_count_);
int32
initialize_memory_manager_uniform(int32 descriptor_count_, int32 requested_bytes_);
int32
configure_display_memory(int32 descriptor_count_,
                         int32 dram_requested_bytes_,
                         int32 vram_requested_bytes_);
MemoryBlockDescriptor *
allocate_memory_record_reporting(const char *name_, uint32 byte_count_, uint32 flags_);
MemoryBlockDescriptor *
allocate_memory_record_silently(const char *name_, uint32 byte_count_, uint32 flags_);
int32
free_memory(void *memory_);
int32
resize_memory(void *memory_, int32 byte_count_);
int32
resize_memory_with_error_policy(void *memory_, int32 byte_count_, int32 report_error_);
int32
resize_memory_silently(void *memory_, int32 byte_count_);
int32
resize_memory_record(MemoryBlockDescriptor *descriptor_, int32 byte_count_, int32 report_error_);
int32
resize_memory_record_reporting(MemoryBlockDescriptor *descriptor_, int32 byte_count_);
int32
resize_memory_record_silently(MemoryBlockDescriptor *descriptor_, int32 byte_count_);
int32
compact_memory_blocks_backward(MemoryBlockDescriptor *last_, MemoryBlockDescriptor *first_);
int32
compact_memory_blocks_forward(MemoryBlockDescriptor *first_, MemoryBlockDescriptor *last_);
Err
compact_memory_blocks_forward_locked(void);
Err
compact_memory_blocks_backward_locked(void);
int32
evict_memory_block_by_priority(uint32 flags_);
int32
evict_memory_block_by_priority_synchronized(uint32 flags_);
int32
evict_or_relocate_memory_block(uint32 flags_);
int32
evict_or_relocate_memory_block_synchronized(uint32 flags_);
MemoryBlockDescriptor *
acquire_named_memory_record(const char *path_);
void *
acquire_named_memory(const char *path_);
MemoryBlockDescriptor *
find_memory_block_by_name(const char *path_, uint32 flags_);
MemoryBlockDescriptor *
find_evictable_memory_block_by_name(const char *path_, uint32 flags_);
MemoryBlockDescriptor *
find_evictable_memory_block_by_name_default(const char *path_);
int32
update_minimum_available_memory_bytes(void);
int32
get_available_memory_bytes(void);
int32
get_available_memory_quanta(void);
int32
get_nonmovable_memory_bytes(void);
void
initialize_memory_block_descriptor_pool(MemoryBlockDescriptor *descriptors_,
                                        int32                  descriptor_count_);
MemoryBlockDescriptor *
acquire_memory_block_descriptor_locked(void);
void
free_memory_block_chain(void *memory_);
void
free_memory_block_chain_locked(MemoryBlockDescriptor *descriptor_);
int32
free_memory_record_locked(void *record_);
void
unlink_memory_block_descriptor(MemoryBlockDescriptor *descriptor_);
MemoryBlockDescriptor *
set_memory_payload_mode(void *memory_, uint32 mode_);
MemoryBlockDescriptor *
set_memory_payload_mode_one(void *memory_);
MemoryBlockDescriptor *
set_memory_record_mode_one(MemoryBlockDescriptor *descriptor_);
uint32
get_memory_payload_byte_count(void *memory_);
void
move_bytes(const uint8 *source_, uint8 *destination_, uint32 byte_count_);
void
fill_bytes(uint8 *destination_, int32 byte_count_, uint32 value_);
void
clear_bytes(uint8 *destination_, int32 byte_count_);
void
move_bytes_to_destination(uint8 *destination_, const uint8 *source_, uint32 byte_count_);
void *
copy_memory_bytes(void *destination_, const void *source_, uint32 byte_count_);
void
swap_byte_ranges(uint8 *left_, uint8 *right_, uint32 byte_count_);
uint32
memory_pointer_to_address(const void *pointer_);
void *
memory_address_to_pointer(uint32 address_);
void
release_memory_block_descriptor(MemoryBlockDescriptor *descriptor_);
