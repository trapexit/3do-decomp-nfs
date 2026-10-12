#include "memory.h"

#include "diagnostics.h"
#include "string_utilities.h"

#include "kernel.h"
#include "kernelnodes.h"
#include "mem.h"
#include "nodes.h"

#include "string.h"

#define MEMORY_WORD_BYTES 4
#define MEMORY_WORD_MASK 3
#define MEMORY_BYTE_MASK 0xFF
#define MEMORY_BYTE_BITS 8
#define MEMORY_HALF_BITS 16
#define MEMORY_BLOCK_FLAG_CHAIN_END 0x20
#define MEMORY_BLOCK_FLAG_PROTECTED 0x8000
#define MEMORY_BLOCK_MOVEMENT_MASK 0x18
#define MEMORY_BLOCK_MODE_MASK 0x7
#define MEMORY_BLOCK_PRIORITY_MASK 0x7
#define MEMORY_BLOCK_FLAG_PURGEABLE 0x8
#define MEMORY_BLOCK_FLAG_MOVABLE 0x10
#define MEMORY_BLOCK_FLAG_ALLOCATE_HIGH 0x20
#define MEMORY_BLOCK_FLAG_ADDRESS_ALIGNED 0x40
#define MEMORY_BLOCK_NAME_BYTES 12
#define MEMORY_REGION_INDEX_MASK 0x700
#define MEMORY_REGION_INDEX_SHIFT 8
#define MEMORY_ALLOCATION_QUANTUM_SHIFT 4
#define MEMORY_ALLOCATION_REPORT_ERRORS 1
#define MEMORY_ALLOCATION_SILENT 0
#define MEMORY_MANAGER_DEFAULT_WORKSPACE_WORD_COUNT 0xE00
#define MEMORY_MANAGER_DEFAULT_BACKING_WORD_COUNT 0x213D
#define MEMORY_MANAGER_EMPTY_WORD 0
#define MEMORY_SEARCH_GRANULARITY_BYTES 0x2000
#define MEMORY_DRAM_SEARCH_UPPER_BYTES 0x280000
#define MEMORY_VRAM_SEARCH_UPPER_BYTES 0x1F0000
#define MEMORY_BINARY_SEARCH_DIVISOR 2
#define MEMORY_MANAGER_MINIMUM_DESCRIPTOR_COUNT 10
#define MEMORY_MANAGER_MAXIMUM_DESCRIPTOR_COUNT 1000
#define MEMORY_MANAGER_MINIMUM_REQUEST_BYTES 10
#define MEMORY_MANAGER_MAXIMUM_REQUEST_BYTES 0x800000
#define MEMORY_MANAGER_MINIMUM_WORKSPACE_WORD_COUNT 0xCC
#define MEMORY_MANAGER_WORKSPACE_HEADER_WORDS 2
#define MEMORY_REGION_DEFAULT_ALIGNMENT_BYTES 0x10
#define MEMORY_REGION_VRAM_ADDRESS_ALIGNMENT_BYTES 0x800
#define MEMORY_REGION_DRAM_FLAGS 0x100
#define MEMORY_REGION_VRAM_FLAGS 0x200
#define MEMORY_MANAGER_DEFAULT_DRAM_REQUEST_BYTES 0x168000
#define MEMORY_MANAGER_DEFAULT_DRAM_RESERVE_BYTES 0xA800
#define MEMORY_MANAGER_DEFAULT_VRAM_REQUEST_BYTES 0xFC000
#define MEMORY_MANAGER_DEFAULT_VRAM_RESERVE_BYTES 0x4000
#define MEMORY_SIZE_ASSERT(name, expression)                                                       \
  typedef char memory_size_assert_##name[(expression) ? 1 : -1]

MEMORY_SIZE_ASSERT(memory_manager_state_size, sizeof(MemoryManagerState) == 32);
MEMORY_SIZE_ASSERT(memory_block_descriptor_size, sizeof(MemoryBlockDescriptor) == 40);
MEMORY_SIZE_ASSERT(memory_region_configuration_size, sizeof(MemoryRegionConfiguration) == 20);

MemoryRegionConfiguration g_MEMORY_REGIONS[MEMORY_REGION_COUNT];
MemoryPoolLayout          g_MEMORY_POOL_LAYOUT;

/*
 * The retail image uses late read-only code as writable backing for its
 * default workspace.  Preserve that image allocation and lifetime overlap
 * without exposing unrelated reconstructed functions to allocator writes.
 */
static const uint32
  G_MEMORY_MANAGER_DEFAULT_WORKSPACE[MEMORY_MANAGER_DEFAULT_BACKING_WORD_COUNT] = {
    MEMORY_MANAGER_DEFAULT_WORKSPACE_WORD_COUNT - MEMORY_MANAGER_WORKSPACE_HEADER_WORDS,
    MEMORY_MANAGER_EMPTY_WORD
  };

MemoryManagerState g_MEMORY_MANAGER_STATE = { (MemoryBlockList *) g_MEMORY_REGIONS,
                                              NULL,
                                              MEMORY_MANAGER_DEFAULT_WORKSPACE_WORD_COUNT,
                                              NULL,
                                              NULL,
                                              0,
                                              0,
                                              0 };
MEMORY_SIZE_ASSERT(memory_manager_state_object_size, sizeof(g_MEMORY_MANAGER_STATE) == 32);
MEMORY_SIZE_ASSERT(memory_pool_layout_size, sizeof(MemoryPoolLayout) == 24);

uint32 g_MEMORY_ALLOCATION_LAYOUT_PREFIX[3];
void  *g_MEMORY_ALLOCATION_END   = NULL;
void  *g_MEMORY_ALLOCATION_START = NULL;

extern void *
memory_payload_to_record(void *memory_);
extern int32
get_available_memory_bytes_locked(void);

void
move_bytes(const uint8 *source_, uint8 *destination_, uint32 byte_count_)
{
  const uint32 *source_words;
  uint32       *destination_words;

  if((destination_ > source_) && (destination_ < (source_ + byte_count_)))
    {
      source_ += byte_count_;
      destination_ += byte_count_;
      if((((uint32) source_ | (uint32) destination_) & MEMORY_WORD_MASK) == 0)
        {
          source_words      = (const uint32 *) source_;
          destination_words = (uint32 *) destination_;
          while(byte_count_ >= MEMORY_WORD_BYTES)
            {
              *--destination_words = *--source_words;
              byte_count_ -= MEMORY_WORD_BYTES;
            }
          source_      = (const uint8 *) source_words;
          destination_ = (uint8 *) destination_words;
        }
      while(byte_count_ > 0)
        {
          *--destination_ = *--source_;
          byte_count_--;
        }
      return;
    }

  if((source_ > destination_) && (source_ < (destination_ + byte_count_)))
    {
      if((((uint32) source_ | (uint32) destination_) & MEMORY_WORD_MASK) == 0)
        {
          source_words      = (const uint32 *) source_;
          destination_words = (uint32 *) destination_;
          while(byte_count_ >= MEMORY_WORD_BYTES)
            {
              *destination_words++ = *source_words++;
              byte_count_ -= MEMORY_WORD_BYTES;
            }
          source_      = (const uint8 *) source_words;
          destination_ = (uint8 *) destination_words;
        }
      while(byte_count_ > 0)
        {
          *destination_++ = *source_++;
          byte_count_--;
        }
      return;
    }

  memcpy(destination_, source_, (size_t) byte_count_);
}

void
clear_bytes(uint8 *destination_, int32 byte_count_)
{
  memset(destination_, 0, (size_t) byte_count_);
}

void
move_bytes_to_destination(uint8 *destination_, const uint8 *source_, uint32 byte_count_)
{
  move_bytes(source_, destination_, byte_count_);
}

void *
copy_memory_bytes(void *destination_, const void *source_, uint32 byte_count_)
{
  return memcpy(destination_, source_, byte_count_);
}

void
swap_byte_ranges(uint8 *left_, uint8 *right_, uint32 byte_count_)
{
  uint8 temporary;

  if((right_ <= left_) || ((left_ + byte_count_) <= right_))
    {
      while(byte_count_ > 0)
        {
          temporary = *right_;
          *right_++ = *left_;
          *left_++  = temporary;
          byte_count_--;
        }
    }
  else
    {
      left_ += byte_count_;
      right_ += byte_count_;
      while(byte_count_ > 0)
        {
          temporary = *--right_;
          *right_   = *--left_;
          *left_    = temporary;
          byte_count_--;
        }
    }
}

uint32
memory_pointer_to_address(const void *pointer_)
{
  return (uint32) pointer_;
}

void *
memory_address_to_pointer(uint32 address_)
{
  return (void *) address_;
}

void
release_memory_block_descriptor(MemoryBlockDescriptor *descriptor_)
{
  descriptor_->next                       = g_MEMORY_MANAGER_STATE.free_descriptors;
  g_MEMORY_MANAGER_STATE.free_descriptors = descriptor_;
}

void
unlink_memory_block_descriptor(MemoryBlockDescriptor *descriptor_)
{
  if((descriptor_->flags & MEMORY_BLOCK_FLAG_PROTECTED) != 0)
    return;

  descriptor_->previous->next = descriptor_->next;
  descriptor_->next->previous = descriptor_->previous;
  release_memory_block_descriptor(descriptor_);
}

void *
memory_payload_to_record(void *memory_)
{
  MemoryBlockDescriptor *descriptor;
  MemoryBlockList       *allocated_blocks;

  allocated_blocks = g_MEMORY_MANAGER_STATE.allocated_blocks;
  for(descriptor = allocated_blocks->first->next; descriptor != allocated_blocks->last;
      descriptor = descriptor->next)
    {
      if((descriptor->payload == memory_) &&
         ((descriptor->flags & MEMORY_BLOCK_FLAG_PROTECTED) == 0))
        return descriptor;
    }

  fatal_error("findmemblock - BLOCK NOT FOUND (%p)\n", memory_);
  return NULL;
}

int32
free_memory_record_locked(void *record_)
{
  LockItem(g_MEMORY_MANAGER_STATE.lock_item, 1);
  unlink_memory_block_descriptor((MemoryBlockDescriptor *) record_);
  return UnlockItem(g_MEMORY_MANAGER_STATE.lock_item);
}

void
free_memory_block_chain_locked(MemoryBlockDescriptor *descriptor_)
{
  MemoryBlockDescriptor *next_descriptor;

  LockItem(g_MEMORY_MANAGER_STATE.lock_item, 1);
  next_descriptor = descriptor_->next;
  while((next_descriptor->flags & MEMORY_BLOCK_FLAG_CHAIN_END) == 0)
    {
      next_descriptor = next_descriptor->next;
      unlink_memory_block_descriptor(next_descriptor->previous);
    }
  UnlockItem(g_MEMORY_MANAGER_STATE.lock_item);
}

void
free_memory_block_chain(void *memory_)
{
  free_memory_block_chain_locked((MemoryBlockDescriptor *) memory_payload_to_record(memory_));
}

MemoryBlockDescriptor *
set_memory_payload_mode(void *memory_, uint32 mode_)
{
  MemoryBlockDescriptor *descriptor;

  descriptor = (MemoryBlockDescriptor *) memory_payload_to_record(memory_);
  LockItem(g_MEMORY_MANAGER_STATE.lock_item, 1);
  descriptor->flags =
    (descriptor->flags & ~MEMORY_BLOCK_MODE_MASK) | mode_ | MEMORY_BLOCK_FLAG_PURGEABLE;
  UnlockItem(g_MEMORY_MANAGER_STATE.lock_item);
  return descriptor;
}

MemoryBlockDescriptor *
set_memory_record_mode_one(MemoryBlockDescriptor *descriptor_)
{
  LockItem(g_MEMORY_MANAGER_STATE.lock_item, 1);
  descriptor_->flags =
    (descriptor_->flags & ~MEMORY_BLOCK_MODE_MASK) | 1 | MEMORY_BLOCK_FLAG_PURGEABLE;
  UnlockItem(g_MEMORY_MANAGER_STATE.lock_item);
  return descriptor_;
}

MemoryBlockDescriptor *
set_memory_payload_mode_one(void *memory_)
{
  return set_memory_record_mode_one((MemoryBlockDescriptor *) memory_payload_to_record(memory_));
}

uint32
get_memory_payload_byte_count(void *memory_)
{
  return ((MemoryBlockDescriptor *) memory_payload_to_record(memory_))->requested_bytes;
}

void *
allocate_memory_payload(const char *name_, uint32 byte_count_, uint32 flags_, int32 report_error_)
{
  return allocate_memory_record_locked(name_, byte_count_, flags_, report_error_)->payload;
}

void *
allocate_named_memory(const char *name_, uint32 byte_count_, uint32 flags_)
{
  return allocate_memory_record_locked(name_, byte_count_, flags_, 1)->payload;
}

void *
allocate_named_memory_silently(const char *name_, uint32 byte_count_, uint32 flags_)
{
  return allocate_memory_record_locked(name_, byte_count_, flags_, 0)->payload;
}

void *
malloc(long byte_count_)
{
  return allocate_named_memory("malloc", (uint32) byte_count_, 0);
}

void
free(void *memory_)
{
  free_memory(memory_);
}

MemoryBlockDescriptor *
allocate_memory_record_reporting(const char *name_, uint32 byte_count_, uint32 flags_)
{
  return allocate_memory_record_locked(name_, byte_count_, flags_, MEMORY_ALLOCATION_REPORT_ERRORS);
}

MemoryBlockDescriptor *
allocate_memory_record_silently(const char *name_, uint32 byte_count_, uint32 flags_)
{
  return allocate_memory_record_locked(name_, byte_count_, flags_, MEMORY_ALLOCATION_SILENT);
}

MemoryBlockDescriptor *
allocate_memory_record_locked(const char *name_,
                              uint32      byte_count_,
                              uint32      flags_,
                              int32       report_error_)
{
  MemoryBlockDescriptor *descriptor;

  LockItem(g_MEMORY_MANAGER_STATE.lock_item, 1);
  descriptor = allocate_memory_record_core(name_, byte_count_, flags_, report_error_);
  UnlockItem(g_MEMORY_MANAGER_STATE.lock_item);
  return descriptor;
}

MemoryRegionConfiguration *
initialize_memory_region(uint32 flags_,
                         void  *lower_address_,
                         void  *upper_address_,
                         uint32 size_alignment_,
                         uint32 address_alignment_,
                         uint32 next_region_flags_)
{
  MemoryBlockDescriptor     *high_sentinel;
  MemoryBlockDescriptor     *low_sentinel;
  MemoryRegionConfiguration *region;
  uint32                     region_index;

  region_index                   = (flags_ & MEMORY_REGION_INDEX_MASK) >> MEMORY_REGION_INDEX_SHIFT;
  region                         = &g_MEMORY_REGIONS[region_index];
  region->size_alignment_mask    = size_alignment_ - 1;
  region->address_alignment_mask = address_alignment_ - 1;
  region->next_region_flags      = next_region_flags_;

  low_sentinel = acquire_memory_block_descriptor_locked();
  copy_fixed_string(low_sentinel->name, "LOW MEM", MEMORY_BLOCK_NAME_BYTES);
  low_sentinel->payload = (void *) (((uint32) lower_address_ + region->size_alignment_mask) &
                                    ~region->size_alignment_mask);
  low_sentinel->allocated_bytes = 0;
  low_sentinel->requested_bytes = 0;
  low_sentinel->flags           = flags_ | MEMORY_BLOCK_FLAG_PROTECTED;
  low_sentinel->previous        = NULL;
  region->first                 = low_sentinel;

  high_sentinel = acquire_memory_block_descriptor_locked();
  copy_fixed_string(high_sentinel->name, "HIGH MEM", MEMORY_BLOCK_NAME_BYTES);
  high_sentinel->payload = (void *) ((uint32) upper_address_ & ~region->size_alignment_mask);
  high_sentinel->allocated_bytes = 0;
  high_sentinel->requested_bytes = 0;
  high_sentinel->flags = flags_ | MEMORY_BLOCK_FLAG_PROTECTED | MEMORY_BLOCK_FLAG_CHAIN_END;
  high_sentinel->next  = NULL;
  region->last         = high_sentinel;

  region->first->next     = high_sentinel;
  high_sentinel->previous = low_sentinel;
  return region;
}

void
append_memory_region(uint32 destination_flags_,
                     uint32 region_flags_,
                     void  *lower_address_,
                     void  *upper_address_,
                     uint32 size_alignment_,
                     uint32 address_alignment_,
                     uint32 next_region_flags_)
{
  MemoryRegionConfiguration *destination;
  MemoryRegionConfiguration *region;
  uint32                     destination_index;

  destination_index = (destination_flags_ & MEMORY_REGION_INDEX_MASK) >> MEMORY_REGION_INDEX_SHIFT;
  destination       = &g_MEMORY_REGIONS[destination_index];
  region            = initialize_memory_region(region_flags_,
                                               lower_address_,
                                               upper_address_,
                                               size_alignment_,
                                               address_alignment_,
                                               next_region_flags_);

  if(destination->first->payload == destination->last->payload)
    destination->first = region->first;
  else
    {
      destination->last->next = region->first;
      region->first->previous = destination->last;
      destination->last->allocated_bytes =
        (uint8 *) region->first->payload - (uint8 *) destination->last->payload;
      destination->last->requested_bytes = destination->last->allocated_bytes;
    }
  destination->last = region->last;
}

void
append_memory_gap(uint32 flags_, void *lower_address_, void *upper_address_)
{
  MemoryBlockDescriptor     *gap_sentinel;
  MemoryRegionConfiguration *region;
  uint8                     *aligned_lower_address;
  uint8                     *aligned_upper_address;
  uint32                     region_index;

  region_index          = (flags_ & MEMORY_REGION_INDEX_MASK) >> MEMORY_REGION_INDEX_SHIFT;
  region                = &g_MEMORY_REGIONS[region_index];
  aligned_lower_address = (uint8 *) (((uint32) lower_address_ + region->size_alignment_mask) &
                                     ~region->size_alignment_mask);
  aligned_upper_address = (uint8 *) ((uint32) upper_address_ & ~region->size_alignment_mask);

  gap_sentinel = acquire_memory_block_descriptor_locked();
  copy_fixed_string(gap_sentinel->name, "GAP", MEMORY_BLOCK_NAME_BYTES);
  gap_sentinel->payload         = aligned_upper_address;
  gap_sentinel->allocated_bytes = 0;
  gap_sentinel->requested_bytes = 0;
  gap_sentinel->flags = flags_ | MEMORY_BLOCK_FLAG_PROTECTED | MEMORY_BLOCK_FLAG_CHAIN_END;
  gap_sentinel->next  = NULL;

  copy_fixed_string(region->last->name, "GAP", MEMORY_BLOCK_NAME_BYTES);
  region->last->next            = gap_sentinel;
  gap_sentinel->previous        = region->last;
  region->last->allocated_bytes = aligned_lower_address - (uint8 *) region->last->payload;
  region->last->requested_bytes = region->last->allocated_bytes;
  region->last                  = gap_sentinel;
}

uint32
release_memory_at_address(uint32 flags_, void *address_)
{
  MemoryBlockDescriptor     *descriptor;
  MemoryBlockDescriptor     *released_descriptor;
  MemoryRegionConfiguration *region;
  uint8                     *descriptor_end;
  uint32                     region_index;
  uint32                     released_bytes;

  region_index = (flags_ & MEMORY_REGION_INDEX_MASK) >> MEMORY_REGION_INDEX_SHIFT;
  region       = &g_MEMORY_REGIONS[region_index];
  descriptor   = region->first;
  while(descriptor != region->last)
    {
      descriptor_end = (uint8 *) descriptor->payload + descriptor->allocated_bytes;
      if(descriptor_end == (uint8 *) address_)
        break;
      descriptor = descriptor->next;
    }

  if(descriptor == region->last)
    fatal_error("Release Memory Not Found\n");

  released_descriptor = descriptor->next;
  released_bytes      = ((uint8 *) released_descriptor->payload - (uint8 *) descriptor->payload -
                         descriptor->allocated_bytes);
  if(released_descriptor == region->last)
    {
      descriptor->allocated_bytes = 0;
      descriptor->requested_bytes = 0;
      descriptor->next            = NULL;
      region->last                = descriptor;
    }
  else
    {
      descriptor->allocated_bytes = ((uint8 *) released_descriptor->payload +
                                     released_descriptor->allocated_bytes - (uint8 *) address_);
      descriptor->requested_bytes = descriptor->allocated_bytes;
      descriptor->next            = released_descriptor->next;
      released_descriptor->next->previous = descriptor;
    }
  release_memory_block_descriptor(released_descriptor);

  if(descriptor == region->first)
    {
      descriptor->allocated_bytes = 0;
      descriptor->requested_bytes = 0;
    }
  return released_bytes;
}

void
configure_memory_manager_workspace(uint32 *workspace_, uint32 workspace_word_count_)
{
  g_MEMORY_MANAGER_STATE.workspace_word_count = workspace_word_count_;
  g_MEMORY_MANAGER_STATE.workspace            = workspace_;
}

int32
find_largest_free_memory_block(uint32 type_bits_)
{
  int32 lower_bytes;
  int32 midpoint_bytes;
  int32 upper_bytes;
  void *memory;

  lower_bytes = 0;
  if(type_bits_ == MEMTYPE_DRAM)
    upper_bytes = MEMORY_DRAM_SEARCH_UPPER_BYTES;
  else
    upper_bytes = MEMORY_VRAM_SEARCH_UPPER_BYTES;

  while((upper_bytes - lower_bytes) > MEMORY_SEARCH_GRANULARITY_BYTES)
    {
      midpoint_bytes = ((lower_bytes + upper_bytes) / MEMORY_BINARY_SEARCH_DIVISOR);
      memory         = AllocMem(midpoint_bytes, type_bits_);
      if(memory == NULL)
        upper_bytes = midpoint_bytes;
      else
        {
          lower_bytes = midpoint_bytes;
          FreeMem(memory, midpoint_bytes);
        }
    }
  return lower_bytes;
}

int32
allocate_memory_block_with_reserve(int32 requested_bytes_, int32 reserved_bytes_, uint32 type_bits_)
{
  int32 maximum_bytes;

  g_MEMORY_ALLOCATION_START = AllocMem(requested_bytes_, type_bits_);
  if(g_MEMORY_ALLOCATION_START == NULL)
    {
      maximum_bytes = find_largest_free_memory_block(type_bits_) - reserved_bytes_;
      if(requested_bytes_ > maximum_bytes)
        requested_bytes_ = maximum_bytes;
      g_MEMORY_ALLOCATION_START = AllocMem(requested_bytes_, type_bits_);
    }

  if(g_MEMORY_ALLOCATION_START == NULL)
    return 0;
  g_MEMORY_ALLOCATION_END = (uint8 *) g_MEMORY_ALLOCATION_START + requested_bytes_;
  return requested_bytes_;
}

int32
initialize_memory_manager(int32 descriptor_count_,
                          int32 dram_requested_bytes_,
                          int32 dram_reserved_bytes_,
                          int32 vram_requested_bytes_,
                          int32 vram_reserved_bytes_)
{
  int32  minimum_available_bytes;
  int32  primary_vram_allocated_bytes;
  int32  secondary_vram_allocated_bytes;
  int32  secondary_vram_requested_bytes;
  uint32 descriptor_bytes;

  g_MEMORY_MANAGER_STATE.lock_item = CreateSizedItem(MKNODEID(KERNELNODE, SEMAPHORENODE), NULL, 0);
  LockItem(g_MEMORY_MANAGER_STATE.lock_item, 1);

  allocate_memory_block_with_reserve(dram_requested_bytes_, dram_reserved_bytes_, MEMTYPE_DRAM);
  g_MEMORY_POOL_LAYOUT.dram_start = g_MEMORY_ALLOCATION_START;
  g_MEMORY_POOL_LAYOUT.dram_end   = g_MEMORY_ALLOCATION_END;

  primary_vram_allocated_bytes =
    allocate_memory_block_with_reserve(vram_requested_bytes_, vram_reserved_bytes_, MEMTYPE_VRAM);
  g_MEMORY_POOL_LAYOUT.primary_vram_start = g_MEMORY_ALLOCATION_START;
  g_MEMORY_POOL_LAYOUT.primary_vram_end   = g_MEMORY_ALLOCATION_END;
  secondary_vram_requested_bytes          = (vram_requested_bytes_ - primary_vram_allocated_bytes);
  if((secondary_vram_requested_bytes >= MEMORY_SEARCH_GRANULARITY_BYTES) &&
     (secondary_vram_requested_bytes >= vram_reserved_bytes_))
    {
      secondary_vram_allocated_bytes =
        allocate_memory_block_with_reserve(secondary_vram_requested_bytes,
                                           vram_reserved_bytes_,
                                           MEMTYPE_VRAM);
    }
  else
    secondary_vram_allocated_bytes = 0;
  g_MEMORY_POOL_LAYOUT.secondary_vram_start = g_MEMORY_ALLOCATION_START;
  g_MEMORY_POOL_LAYOUT.secondary_vram_end   = g_MEMORY_ALLOCATION_END;

  if((descriptor_count_ < MEMORY_MANAGER_MINIMUM_DESCRIPTOR_COUNT) ||
     (descriptor_count_ > MEMORY_MANAGER_MAXIMUM_DESCRIPTOR_COUNT))
    fatal_error("initmemman - blocks must be 10..1000\n");
  if((dram_requested_bytes_ < MEMORY_MANAGER_MINIMUM_REQUEST_BYTES) ||
     (dram_requested_bytes_ > MEMORY_MANAGER_MAXIMUM_REQUEST_BYTES))
    fatal_error("initmemman - unreasonable amount of dram requested\n");
  if((vram_requested_bytes_ < MEMORY_MANAGER_MINIMUM_REQUEST_BYTES) ||
     (vram_requested_bytes_ > MEMORY_MANAGER_MAXIMUM_REQUEST_BYTES))
    fatal_error("initmemman - unreasonable amount of vram requested\n");

  g_MEMORY_ALLOCATION_START = g_MEMORY_POOL_LAYOUT.dram_start;
  if(g_MEMORY_MANAGER_STATE.workspace == NULL)
    g_MEMORY_MANAGER_STATE.workspace = (uint32 *) G_MEMORY_MANAGER_DEFAULT_WORKSPACE;
  if(g_MEMORY_MANAGER_STATE.workspace_word_count < MEMORY_MANAGER_MINIMUM_WORKSPACE_WORD_COUNT)
    {
      fatal_error("initmemman - INSUFFICIENT ROW TABLE SPACE\n");
    }
  g_MEMORY_MANAGER_STATE.workspace_end =
    (g_MEMORY_MANAGER_STATE.workspace + g_MEMORY_MANAGER_STATE.workspace_word_count);
  g_MEMORY_MANAGER_STATE.workspace[0] =
    (g_MEMORY_MANAGER_STATE.workspace_word_count - MEMORY_MANAGER_WORKSPACE_HEADER_WORDS);
  g_MEMORY_MANAGER_STATE.workspace[1] = 0;

  descriptor_bytes = (descriptor_count_ * sizeof(MemoryBlockDescriptor));
  initialize_memory_block_descriptor_pool((MemoryBlockDescriptor *) g_MEMORY_POOL_LAYOUT.dram_start,
                                          descriptor_count_);
  initialize_memory_region(0,
                           NULL,
                           NULL,
                           MEMORY_REGION_DEFAULT_ALIGNMENT_BYTES,
                           MEMORY_REGION_DEFAULT_ALIGNMENT_BYTES,
                           0);
  append_memory_region(0,
                       MEMORY_REGION_DRAM_FLAGS,
                       (uint8 *) g_MEMORY_POOL_LAYOUT.dram_start + descriptor_bytes,
                       g_MEMORY_POOL_LAYOUT.dram_end,
                       MEMORY_REGION_DEFAULT_ALIGNMENT_BYTES,
                       MEMORY_REGION_DEFAULT_ALIGNMENT_BYTES,
                       0);
  append_memory_region(0,
                       MEMORY_REGION_VRAM_FLAGS,
                       g_MEMORY_POOL_LAYOUT.primary_vram_start,
                       g_MEMORY_POOL_LAYOUT.primary_vram_end,
                       MEMORY_REGION_DEFAULT_ALIGNMENT_BYTES,
                       MEMORY_REGION_VRAM_ADDRESS_ALIGNMENT_BYTES,
                       0);
  if(secondary_vram_allocated_bytes != 0)
    {
      append_memory_gap(0,
                        g_MEMORY_POOL_LAYOUT.secondary_vram_start,
                        g_MEMORY_POOL_LAYOUT.secondary_vram_end);
    }

  UnlockItem(g_MEMORY_MANAGER_STATE.lock_item);
  minimum_available_bytes                        = get_available_memory_bytes();
  g_MEMORY_MANAGER_STATE.minimum_available_bytes = minimum_available_bytes;
  return minimum_available_bytes;
}

int32
initialize_memory_manager_default(int32 descriptor_count_)
{
  return initialize_memory_manager(descriptor_count_,
                                   MEMORY_MANAGER_DEFAULT_DRAM_REQUEST_BYTES,
                                   MEMORY_MANAGER_DEFAULT_DRAM_RESERVE_BYTES,
                                   MEMORY_MANAGER_DEFAULT_VRAM_REQUEST_BYTES,
                                   MEMORY_MANAGER_DEFAULT_VRAM_RESERVE_BYTES);
}

int32
initialize_memory_manager_uniform(int32 descriptor_count_, int32 requested_bytes_)
{
  return initialize_memory_manager(descriptor_count_,
                                   requested_bytes_,
                                   MEMORY_MANAGER_DEFAULT_DRAM_RESERVE_BYTES,
                                   requested_bytes_,
                                   MEMORY_MANAGER_DEFAULT_VRAM_RESERVE_BYTES);
}

int32
configure_display_memory(int32 descriptor_count_,
                         int32 dram_requested_bytes_,
                         int32 vram_requested_bytes_)
{
  return initialize_memory_manager(descriptor_count_,
                                   dram_requested_bytes_,
                                   MEMORY_MANAGER_DEFAULT_DRAM_RESERVE_BYTES,
                                   vram_requested_bytes_,
                                   MEMORY_MANAGER_DEFAULT_VRAM_RESERVE_BYTES);
}

MemoryBlockDescriptor *
allocate_memory_record_core(const char *name_,
                            uint32      byte_count_,
                            uint32      flags_,
                            int32       report_error_)
{
  MemoryBlockDescriptor     *cursor;
  MemoryBlockDescriptor     *descriptor;
  MemoryRegionConfiguration *region;
  const char                *leaf_name;
  uint8                     *block_end;
  uint8                     *gap_address;
  uint8                     *upper_address;
  uint32                     aligned_bytes;
  uint32                     alignment_mask;
  uint32                     available_bytes;
  uint32                     gap_bytes;
  uint32                     region_index;

  if((int32) byte_count_ < 0)
    {
      if(report_error_ == 0)
        return NULL;
      UnlockItem(g_MEMORY_MANAGER_STATE.lock_item);
      available_bytes = (uint32) get_available_memory_bytes();
      fatal_error("reservemem - INVALID SIZE REQUESTED\n"
                  "requested: %ld available: %ld\n",
                  byte_count_,
                  available_bytes);
      return NULL;
    }

  region_index = (flags_ & MEMORY_REGION_INDEX_MASK) >> MEMORY_REGION_INDEX_SHIFT;
  region       = &g_MEMORY_REGIONS[region_index];
  leaf_name    = find_path_leaf(name_);
  if((flags_ & MEMORY_BLOCK_FLAG_ADDRESS_ALIGNED) != 0)
    alignment_mask = region->address_alignment_mask;
  else
    alignment_mask = region->size_alignment_mask;
  aligned_bytes = (byte_count_ + alignment_mask) & ~alignment_mask;

  if(region->first == NULL)
    goto invalid_flags;

  if((flags_ & MEMORY_BLOCK_FLAG_ALLOCATE_HIGH) != 0)
    goto allocate_high;

  if((flags_ & MEMORY_BLOCK_FLAG_MOVABLE) == 0)
    compact_memory_blocks_backward(region->last, region->first);

retry_low:
  gap_address = (uint8 *) region->first->payload;
  cursor      = region->first->next;
  if((flags_ & MEMORY_BLOCK_FLAG_ADDRESS_ALIGNED) != 0)
    {
      gap_address = (uint8 *) (((uint32) gap_address + region->address_alignment_mask) &
                               ~region->address_alignment_mask);
    }

  for(;;)
    {
      gap_bytes = 0;
      if((uint8 *) cursor->payload > gap_address)
        gap_bytes = (uint8 *) cursor->payload - gap_address;
      else
        {
          gap_address = (uint8 *) cursor->payload + cursor->allocated_bytes;
          if((flags_ & MEMORY_BLOCK_FLAG_ADDRESS_ALIGNED) != 0)
            {
              gap_address = (uint8 *) (((uint32) gap_address + region->address_alignment_mask) &
                                       ~region->address_alignment_mask);
            }
        }

      if(gap_bytes >= aligned_bytes)
        {
          descriptor             = acquire_memory_block_descriptor_locked();
          descriptor->payload    = gap_address;
          descriptor->previous   = cursor->previous;
          descriptor->next       = cursor;
          cursor->previous->next = descriptor;
          cursor->previous       = descriptor;
          goto initialize_descriptor;
        }

      if(cursor == region->last)
        break;
      gap_address = (uint8 *) cursor->payload + cursor->allocated_bytes;
      if((flags_ & MEMORY_BLOCK_FLAG_ADDRESS_ALIGNED) != 0)
        {
          gap_address = (uint8 *) (((uint32) gap_address + region->address_alignment_mask) &
                                   ~region->address_alignment_mask);
        }
      cursor = cursor->next;
    }

  if(evict_or_relocate_memory_block(flags_) != 0)
    goto retry_low;
  if(compact_memory_blocks_backward(region->last, region->first) != 0)
    goto retry_low;
  goto out_of_memory;

allocate_high:
  if((flags_ & MEMORY_BLOCK_FLAG_MOVABLE) == 0)
    compact_memory_blocks_forward(region->first, region->last);

retry_high:
  upper_address = (uint8 *) region->last->payload;
  cursor        = region->last->previous;
  if((flags_ & MEMORY_BLOCK_FLAG_ADDRESS_ALIGNED) != 0)
    {
      upper_address = (uint8 *) ((uint32) upper_address & ~region->address_alignment_mask);
    }

  for(;;)
    {
      gap_bytes = 0;
      block_end = (uint8 *) cursor->payload + cursor->allocated_bytes;
      if((flags_ & MEMORY_BLOCK_FLAG_ADDRESS_ALIGNED) != 0)
        {
          block_end = (uint8 *) (((uint32) block_end + region->address_alignment_mask) &
                                 ~region->address_alignment_mask);
        }

      if(block_end < upper_address)
        gap_bytes = upper_address - block_end;
      else
        {
          upper_address = (uint8 *) cursor->payload;
          if((flags_ & MEMORY_BLOCK_FLAG_ADDRESS_ALIGNED) != 0)
            {
              upper_address = (uint8 *) ((uint32) upper_address & ~region->address_alignment_mask);
            }
        }

      if(gap_bytes >= aligned_bytes)
        {
          descriptor             = acquire_memory_block_descriptor_locked();
          descriptor->payload    = upper_address - aligned_bytes;
          descriptor->previous   = cursor;
          descriptor->next       = cursor->next;
          cursor->next->previous = descriptor;
          cursor->next           = descriptor;
          goto initialize_descriptor;
        }

      if(cursor == region->first)
        break;
      cursor = cursor->previous;
    }

  if(evict_or_relocate_memory_block(flags_) != 0)
    goto retry_high;
  if(compact_memory_blocks_forward(region->first, region->last) != 0)
    goto retry_high;

out_of_memory:
  if(report_error_ == 0)
    return NULL;
  UnlockItem(g_MEMORY_MANAGER_STATE.lock_item);
  available_bytes = (uint32) get_available_memory_bytes();
  fatal_error("reservemem - OUT OF MEMORY\n"
              "requested: %ld available: %ld\n",
              byte_count_,
              available_bytes);

invalid_flags:
  if(report_error_ != 0)
    {
      UnlockItem(g_MEMORY_MANAGER_STATE.lock_item);
      fatal_error("reservemem - INVALID TYPE FLAGS (%304x)\n", flags_);
    }
  return NULL;

initialize_descriptor:
  descriptor->flags           = flags_;
  descriptor->allocation_id   = g_MEMORY_MANAGER_STATE.next_allocation_id++;
  descriptor->requested_bytes = byte_count_;
  descriptor->allocated_bytes = aligned_bytes;
  copy_fixed_string(descriptor->name, leaf_name, MEMORY_BLOCK_NAME_BYTES);
  update_minimum_available_memory_bytes();
  return descriptor;
}

int32
free_memory(void *memory_)
{
  return free_memory_record_locked(memory_payload_to_record(memory_));
}

int32
resize_memory(void *memory_, int32 byte_count_)
{
  MemoryBlockDescriptor *descriptor;

  descriptor = (MemoryBlockDescriptor *) memory_payload_to_record(memory_);
  return resize_memory_record(descriptor, byte_count_, 1);
}

int32
resize_memory_with_error_policy(void *memory_, int32 byte_count_, int32 report_error_)
{
  MemoryBlockDescriptor *descriptor;

  descriptor = (MemoryBlockDescriptor *) memory_payload_to_record(memory_);
  return resize_memory_record(descriptor, byte_count_, report_error_);
}

int32
resize_memory_silently(void *memory_, int32 byte_count_)
{
  MemoryBlockDescriptor *descriptor;

  descriptor = (MemoryBlockDescriptor *) memory_payload_to_record(memory_);
  return resize_memory_record(descriptor, byte_count_, 0);
}

int32
resize_memory_record_reporting(MemoryBlockDescriptor *descriptor_, int32 byte_count_)
{
  return resize_memory_record(descriptor_, byte_count_, 1);
}

int32
resize_memory_record_silently(MemoryBlockDescriptor *descriptor_, int32 byte_count_)
{
  return resize_memory_record(descriptor_, byte_count_, 0);
}

int32
resize_memory_record(MemoryBlockDescriptor *descriptor_, int32 byte_count_, int32 report_error_)
{
  MemoryRegionConfiguration *region;
  int32                      available_bytes;
  uint32                     aligned_bytes;
  uint32                     region_index;

  region_index = (descriptor_->flags & MEMORY_REGION_INDEX_MASK) >> MEMORY_REGION_INDEX_SHIFT;
  region       = &g_MEMORY_REGIONS[region_index];
  LockItem(g_MEMORY_MANAGER_STATE.lock_item, 1);
  available_bytes = (uint8 *) descriptor_->next->payload - (uint8 *) descriptor_->payload;
  aligned_bytes =
    ((uint32) byte_count_ + region->size_alignment_mask) & ~region->size_alignment_mask;

  if(byte_count_ < 0)
    {
      UnlockItem(g_MEMORY_MANAGER_STATE.lock_item);
      if(byte_count_ == -1)
        return available_bytes;
      if(report_error_ == 0)
        return 0;

      UnlockItem(g_MEMORY_MANAGER_STATE.lock_item);
      available_bytes = get_available_memory_bytes();
      fatal_error("resizemem - INVALID SIZE REQUEST\n"
                  "requested: %ld available: %ld\n",
                  byte_count_,
                  available_bytes);
      return 0;
    }

  for(;;)
    {
      if((int32) aligned_bytes < available_bytes)
        {
          descriptor_->allocated_bytes = aligned_bytes;
          descriptor_->requested_bytes = (uint32) byte_count_;
          UnlockItem(g_MEMORY_MANAGER_STATE.lock_item);
          return byte_count_;
        }

      if(((descriptor_->next->flags & MEMORY_BLOCK_MOVEMENT_MASK) != 0) &&
         (compact_memory_blocks_backward(region->last, descriptor_->next) != 0))
        {
          available_bytes = (uint8 *) descriptor_->next->payload - (uint8 *) descriptor_->payload;
          aligned_bytes =
            ((uint32) byte_count_ + region->size_alignment_mask) & ~region->size_alignment_mask;
          continue;
        }

      if(((descriptor_->flags & MEMORY_BLOCK_MOVEMENT_MASK) != 0) &&
         (compact_memory_blocks_forward(region->first, descriptor_) != 0))
        {
          available_bytes = (uint8 *) descriptor_->next->payload - (uint8 *) descriptor_->payload;
          aligned_bytes =
            ((uint32) byte_count_ + region->size_alignment_mask) & ~region->size_alignment_mask;
          continue;
        }
      break;
    }

  UnlockItem(g_MEMORY_MANAGER_STATE.lock_item);
  if(report_error_ != 0)
    {
      fatal_error("resizememblock - NO ROOM TO RESIZE\n"
                  "requested: %ld available: %ld\n",
                  byte_count_,
                  available_bytes);
    }
  return 0;
}

int32
compact_memory_blocks_backward(MemoryBlockDescriptor *last_, MemoryBlockDescriptor *first_)
{
  MemoryBlockDescriptor *gap_cursor;
  MemoryBlockDescriptor *movable;
  uint8                 *block_end;
  uint8                 *new_payload;
  uint8                 *upper_address;
  int32                  gap_bytes;
  int32                  moved_block;

  moved_block   = 0;
  upper_address = (uint8 *) last_->payload;
  gap_cursor    = last_->previous;
  movable       = gap_cursor;
  for(;;)
    {
      gap_bytes = 0;
      for(;;)
        {
          block_end = (uint8 *) gap_cursor->payload + gap_cursor->allocated_bytes;
          if(block_end < upper_address)
            {
              gap_bytes     = upper_address - block_end;
              upper_address = block_end;
              break;
            }
          if(gap_cursor == first_)
            break;

          upper_address = (uint8 *) gap_cursor->payload;
          gap_cursor    = gap_cursor->previous;
        }

      if(gap_bytes == 0)
        return moved_block;

      while(((movable->flags & MEMORY_BLOCK_MOVEMENT_MASK) == 0) ||
            ((uint8 *) movable->payload > upper_address))
        {
          if(movable == first_)
            return moved_block;
          movable = movable->previous;
        }

      if(((uint8 *) movable->payload + movable->allocated_bytes) == upper_address)
        {
          new_payload = upper_address + gap_bytes - movable->allocated_bytes;
          move_bytes((const uint8 *) movable->payload, new_payload, movable->allocated_bytes);
          movable->payload = new_payload;
          upper_address    = new_payload;
          moved_block      = 1;
          continue;
        }

      if(movable->allocated_bytes > (uint32) gap_bytes)
        {
          upper_address = (uint8 *) gap_cursor->payload;
          continue;
        }

      new_payload = upper_address + gap_bytes - movable->allocated_bytes;
      move_bytes((const uint8 *) movable->payload, new_payload, movable->allocated_bytes);
      movable->payload = new_payload;

      movable->previous->next    = movable->next;
      movable->next->previous    = movable->previous;
      movable->next              = gap_cursor;
      movable->previous          = gap_cursor->previous;
      gap_cursor->previous->next = movable;
      gap_cursor->previous       = movable;

      upper_address = (uint8 *) movable->payload;
      moved_block   = 1;
    }
}

int32
compact_memory_blocks_forward(MemoryBlockDescriptor *first_, MemoryBlockDescriptor *last_)
{
  MemoryBlockDescriptor *gap_cursor;
  MemoryBlockDescriptor *movable;
  uint8                 *lower_address;
  int32                  gap_bytes;
  int32                  moved_block;

  moved_block   = 0;
  lower_address = (uint8 *) first_->payload;
  gap_cursor    = first_->next;
  movable       = gap_cursor;
  for(;;)
    {
      gap_bytes = 0;
      for(;;)
        {
          if((uint8 *) gap_cursor->payload > lower_address)
            {
              gap_bytes = (uint8 *) gap_cursor->payload - lower_address;
              break;
            }
          if(gap_cursor == last_)
            break;

          lower_address = (uint8 *) gap_cursor->payload + gap_cursor->allocated_bytes;
          gap_cursor    = gap_cursor->next;
        }

      if(gap_bytes == 0)
        return moved_block;

      while(((movable->flags & MEMORY_BLOCK_MOVEMENT_MASK) == 0) ||
            ((uint8 *) movable->payload < lower_address))
        {
          if(movable == last_)
            return moved_block;
          movable = movable->next;
        }

      if(movable == gap_cursor)
        {
          move_bytes((const uint8 *) movable->payload, lower_address, movable->allocated_bytes);
          movable->payload = lower_address;
        }
      else
        {
          if(movable->allocated_bytes > (uint32) gap_bytes)
            {
              lower_address = (uint8 *) gap_cursor->payload + gap_cursor->allocated_bytes;
              continue;
            }

          move_bytes((const uint8 *) movable->payload, lower_address, movable->allocated_bytes);
          movable->payload = lower_address;

          movable->previous->next    = movable->next;
          movable->next->previous    = movable->previous;
          movable->next              = gap_cursor;
          movable->previous          = gap_cursor->previous;
          gap_cursor->previous->next = movable;
          gap_cursor->previous       = movable;
        }

      lower_address += movable->allocated_bytes;
      moved_block = 1;
    }
}

int32
evict_memory_block_by_priority(uint32 flags_)
{
  MemoryRegionConfiguration *region;
  MemoryBlockDescriptor     *candidate;
  MemoryBlockDescriptor     *descriptor;
  uint32                     candidate_age;
  uint32                     current_allocation_id;
  uint32                     descriptor_age;
  uint32                     descriptor_priority;
  uint32                     region_index;
  uint32                     requested_priority;

  region_index          = (flags_ & MEMORY_REGION_INDEX_MASK) >> MEMORY_REGION_INDEX_SHIFT;
  region                = &g_MEMORY_REGIONS[region_index];
  requested_priority    = flags_ & MEMORY_BLOCK_PRIORITY_MASK;
  candidate             = NULL;
  candidate_age         = 0;
  current_allocation_id = g_MEMORY_MANAGER_STATE.next_allocation_id;
  descriptor            = region->first->next;
  while(descriptor != region->last)
    {
      if((descriptor->flags & MEMORY_BLOCK_FLAG_PURGEABLE) != 0)
        {
          descriptor_priority = descriptor->flags & MEMORY_BLOCK_PRIORITY_MASK;
          descriptor_age      = current_allocation_id - descriptor->allocation_id;
          if((descriptor_priority > requested_priority) ||
             ((descriptor_priority == requested_priority) && (descriptor_age >= candidate_age)))
            {
              candidate          = descriptor;
              requested_priority = descriptor_priority;
              candidate_age      = descriptor_age;
            }
        }
      descriptor = descriptor->next;
    }

  if(candidate == NULL)
    return 0;
  unlink_memory_block_descriptor(candidate);
  return 1;
}

int32
evict_memory_block_by_priority_synchronized(uint32 flags_)
{
  int32 evicted_block;

  LockItem(g_MEMORY_MANAGER_STATE.lock_item, 1);
  evicted_block = evict_memory_block_by_priority(flags_);
  UnlockItem(g_MEMORY_MANAGER_STATE.lock_item);
  return evicted_block;
}

int32
evict_or_relocate_memory_block_synchronized(uint32 flags_)
{
  int32 reclaimed_block;

  LockItem(g_MEMORY_MANAGER_STATE.lock_item, 1);
  reclaimed_block = evict_or_relocate_memory_block(flags_);
  UnlockItem(g_MEMORY_MANAGER_STATE.lock_item);
  return reclaimed_block;
}

int32
evict_or_relocate_memory_block(uint32 flags_)
{
  char                       relocation_name[MEMORY_BLOCK_NAME_BYTES + 1];
  MemoryBlockDescriptor     *candidate;
  MemoryBlockDescriptor     *descriptor;
  MemoryBlockDescriptor     *replacement;
  MemoryRegionConfiguration *region;
  int32                      name_index;
  uint32                     candidate_age;
  uint32                     candidate_priority;
  uint32                     current_allocation_id;
  uint32                     descriptor_age;
  uint32                     descriptor_priority;
  uint32                     region_index;

  region_index = (flags_ & MEMORY_REGION_INDEX_MASK) >> MEMORY_REGION_INDEX_SHIFT;
  region       = &g_MEMORY_REGIONS[region_index];
  if(region->next_region_flags == 0)
    return evict_memory_block_by_priority(flags_);

  candidate_priority    = flags_ & MEMORY_BLOCK_PRIORITY_MASK;
  candidate             = NULL;
  candidate_age         = 0;
  current_allocation_id = g_MEMORY_MANAGER_STATE.next_allocation_id;
  descriptor            = region->first->next;
  while(descriptor != region->last)
    {
      if((descriptor->flags & MEMORY_BLOCK_FLAG_PURGEABLE) != 0)
        {
          descriptor_priority = descriptor->flags & MEMORY_BLOCK_PRIORITY_MASK;
          descriptor_age      = current_allocation_id - descriptor->allocation_id;
          if((descriptor_priority > candidate_priority) ||
             ((descriptor_priority == candidate_priority) && (descriptor_age >= candidate_age)))
            {
              candidate          = descriptor;
              candidate_priority = descriptor_priority;
              candidate_age      = descriptor_age;
            }
        }
      descriptor = descriptor->next;
    }

  if(candidate == NULL)
    return 0;

  for(name_index = 0; name_index < MEMORY_BLOCK_NAME_BYTES; name_index++)
    {
      relocation_name[name_index] = descriptor->name[name_index];
    }
  relocation_name[MEMORY_BLOCK_NAME_BYTES] = '\0';

  for(;;)
    {
      replacement = allocate_memory_record_core(relocation_name,
                                                candidate->requested_bytes,
                                                descriptor->flags | MEMORY_BLOCK_FLAG_PURGEABLE,
                                                0);
      if(replacement != NULL)
        break;
      if(evict_or_relocate_memory_block(candidate_priority) == 0)
        return 0;
    }

  move_bytes((const uint8 *) candidate->payload,
             (uint8 *) replacement->payload,
             candidate->requested_bytes);
  unlink_memory_block_descriptor(candidate);
  return 1;
}

MemoryBlockDescriptor *
acquire_named_memory_record(const char *path_)
{
  MemoryBlockDescriptor *candidate;
  MemoryBlockDescriptor *replacement;
  uint32                 original_flags;
  uint32                 search_flags;
  uint32                 region_index;

  candidate = find_evictable_memory_block_by_name_default(path_);
  if(candidate != NULL)
    {
      candidate->flags &= ~MEMORY_BLOCK_FLAG_PURGEABLE;
      if((candidate->flags & MEMORY_BLOCK_FLAG_MOVABLE) != 0)
        return candidate;

      replacement =
        allocate_memory_record_core(path_, candidate->requested_bytes, candidate->flags, 0);
      if(replacement == NULL)
        return candidate;
      move_bytes((const uint8 *) candidate->payload,
                 (uint8 *) replacement->payload,
                 candidate->requested_bytes);
      free_memory_record_locked(candidate);
      return replacement;
    }

  search_flags =
    ((MemoryRegionConfiguration *) g_MEMORY_MANAGER_STATE.allocated_blocks)->next_region_flags;
  while(search_flags != 0)
    {
      candidate = find_evictable_memory_block_by_name(path_, search_flags);
      if(candidate != NULL)
        {
          original_flags = candidate->flags;
          candidate->flags &= ~MEMORY_BLOCK_FLAG_PURGEABLE;
          replacement =
            allocate_memory_record_core(path_, candidate->requested_bytes, candidate->flags, 0);
          candidate->flags = original_flags;
          move_bytes((const uint8 *) candidate->payload,
                     (uint8 *) replacement->payload,
                     candidate->requested_bytes);
          return replacement;
        }

      region_index = (search_flags & MEMORY_REGION_INDEX_MASK) >> MEMORY_REGION_INDEX_SHIFT;
      search_flags = g_MEMORY_REGIONS[region_index].next_region_flags;
    }
  return NULL;
}

void *
acquire_named_memory(const char *path_)
{
  MemoryBlockDescriptor *descriptor;

  descriptor = acquire_named_memory_record(path_);
  if(descriptor == NULL)
    return NULL;
  return descriptor->payload;
}

MemoryBlockDescriptor *
find_memory_block_by_name(const char *path_, uint32 flags_)
{
  const char                *name;
  MemoryBlockDescriptor     *descriptor;
  MemoryRegionConfiguration *region;
  uint32                     region_index;

  region_index = (flags_ & MEMORY_REGION_INDEX_MASK) >> MEMORY_REGION_INDEX_SHIFT;
  region       = &g_MEMORY_REGIONS[region_index];
  name         = get_path_leaf(path_);
  descriptor   = region->first;
  while(descriptor != region->last)
    {
      if(compare_fixed_strings((const uint8 *) name,
                               (const uint8 *) descriptor->name,
                               MEMORY_BLOCK_NAME_BYTES) == 0)
        return descriptor;
      descriptor = descriptor->next;
    }
  return NULL;
}

MemoryBlockDescriptor *
find_evictable_memory_block_by_name(const char *path_, uint32 flags_)
{
  const char                *name;
  MemoryBlockDescriptor     *descriptor;
  MemoryRegionConfiguration *region;
  uint32                     region_index;

  region_index = (flags_ & MEMORY_REGION_INDEX_MASK) >> MEMORY_REGION_INDEX_SHIFT;
  region       = &g_MEMORY_REGIONS[region_index];
  name         = get_path_leaf(path_);
  descriptor   = region->first;
  while(descriptor != region->last)
    {
      if((compare_fixed_strings((const uint8 *) name,
                                (const uint8 *) descriptor->name,
                                MEMORY_BLOCK_NAME_BYTES) == 0) &&
         ((descriptor->flags & MEMORY_BLOCK_FLAG_PURGEABLE) != 0))
        {
          return descriptor;
        }
      descriptor = descriptor->next;
    }
  return NULL;
}

MemoryBlockDescriptor *
find_evictable_memory_block_by_name_default(const char *path_)
{
  return find_evictable_memory_block_by_name(path_, 0);
}

Err
compact_memory_blocks_forward_locked(void)
{
  LockItem(g_MEMORY_MANAGER_STATE.lock_item, 1);
  compact_memory_blocks_forward(g_MEMORY_MANAGER_STATE.allocated_blocks->first,
                                g_MEMORY_MANAGER_STATE.allocated_blocks->last);
  return UnlockItem(g_MEMORY_MANAGER_STATE.lock_item);
}

Err
compact_memory_blocks_backward_locked(void)
{
  LockItem(g_MEMORY_MANAGER_STATE.lock_item, 1);
  compact_memory_blocks_backward(g_MEMORY_MANAGER_STATE.allocated_blocks->last,
                                 g_MEMORY_MANAGER_STATE.allocated_blocks->first);
  return UnlockItem(g_MEMORY_MANAGER_STATE.lock_item);
}

int32
update_minimum_available_memory_bytes(void)
{
  int32 available_bytes;

  available_bytes = get_available_memory_bytes_locked();
  if(available_bytes < g_MEMORY_MANAGER_STATE.minimum_available_bytes)
    g_MEMORY_MANAGER_STATE.minimum_available_bytes = available_bytes;
  return g_MEMORY_MANAGER_STATE.minimum_available_bytes;
}

int32
get_available_memory_bytes(void)
{
  int32 available_bytes;

  LockItem(g_MEMORY_MANAGER_STATE.lock_item, 1);
  available_bytes = get_available_memory_bytes_locked();
  UnlockItem(g_MEMORY_MANAGER_STATE.lock_item);
  return available_bytes;
}

int32
get_available_memory_bytes_locked(void)
{
  int32                  available_bytes;
  int32                  gap_bytes;
  MemoryBlockDescriptor *current;
  MemoryBlockDescriptor *next;

  compact_memory_blocks_backward(g_MEMORY_MANAGER_STATE.allocated_blocks->last,
                                 g_MEMORY_MANAGER_STATE.allocated_blocks->first);

  available_bytes = 0;
  current         = g_MEMORY_MANAGER_STATE.allocated_blocks->first;
  next            = current->next;
  while(next != NULL)
    {
      gap_bytes = (uint8 *) next->payload - (uint8 *) current->payload - current->allocated_bytes;
      if(gap_bytes > available_bytes)
        available_bytes = gap_bytes;
      current = next;
      next    = next->next;
    }
  return available_bytes;
}

int32
get_available_memory_quanta(void)
{
  return get_available_memory_bytes() >> MEMORY_ALLOCATION_QUANTUM_SHIFT;
}

int32
get_nonmovable_memory_bytes(void)
{
  MemoryBlockDescriptor *descriptor;
  int32                  byte_count;

  byte_count = 0;
  descriptor = g_MEMORY_MANAGER_STATE.allocated_blocks->first;
  while(descriptor != NULL)
    {
      if((descriptor->flags & MEMORY_BLOCK_MOVEMENT_MASK) == 0)
        byte_count += descriptor->allocated_bytes;
      descriptor = descriptor->next;
    }
  return byte_count;
}

void
initialize_memory_block_descriptor_pool(MemoryBlockDescriptor *descriptors_,
                                        int32                  descriptor_count_)
{
  int32                  descriptor_index;
  MemoryBlockDescriptor *descriptor;

  g_MEMORY_MANAGER_STATE.free_descriptors = descriptors_;
  descriptor                              = descriptors_;
  for(descriptor_index = 1; descriptor_index < descriptor_count_; descriptor_index++)
    {
      descriptor->next = descriptor + 1;
      descriptor++;
    }
  descriptor->next = NULL;
}

MemoryBlockDescriptor *
acquire_memory_block_descriptor_locked(void)
{
  MemoryBlockDescriptor *descriptor;

  descriptor = g_MEMORY_MANAGER_STATE.free_descriptors;
  if(descriptor == NULL)
    {
      UnlockItem(g_MEMORY_MANAGER_STATE.lock_item);
      fatal_error("getmemblock - NO MEMORY BLOCKS LEFT\n");
    }
  descriptor                              = g_MEMORY_MANAGER_STATE.free_descriptors;
  g_MEMORY_MANAGER_STATE.free_descriptors = descriptor->next;
  return descriptor;
}
