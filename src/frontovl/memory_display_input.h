#pragma once

#include "types.h"

enum
{
  MEMORY_DISPLAY_MESSAGE_BYTES = 20
};

typedef int32 (*MemoryDisplayInputCallback)(void   *display_context_,
                                            int32   display_mode_,
                                            uint32 *control_bits_);

typedef struct MemoryDisplayInputLiterals
{
  char                         message[MEMORY_DISPLAY_MESSAGE_BYTES];
  MemoryDisplayInputCallback   callback;
  MemoryDisplayInputCallback **callback_slot_reference;
} MemoryDisplayInputLiterals;

extern const MemoryDisplayInputLiterals G_MEMORY_DISPLAY_INPUT_LITERALS;

int32
read_memory_display_input(void *display_context_, int32 display_mode_, uint32 *control_bits_);
void
show_memory_size_display(void);
