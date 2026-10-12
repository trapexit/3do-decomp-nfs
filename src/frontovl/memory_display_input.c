#include "memory_display_input.h"

#include "controller_input.h"
#include "frontend_runtime_state.h"
#include "sdk_veneers.h"

const MemoryDisplayInputLiterals G_MEMORY_DISPLAY_INPUT_LITERALS = {
  "ShowMemSizeDisplay\n",
  read_memory_display_input,
  &g_MEMORY_DISPLAY_INPUT_CALLBACK_SLOT
};

int32
read_memory_display_input(void *display_context_, int32 display_mode_, uint32 *control_bits_)
{
  (void) display_context_;
  (void) display_mode_;

  *control_bits_ = read_third_controller_state();
  return 0;
}

void
show_memory_size_display(void)
{
  printf(G_MEMORY_DISPLAY_INPUT_LITERALS.message);
  **G_MEMORY_DISPLAY_INPUT_LITERALS.callback_slot_reference =
    G_MEMORY_DISPLAY_INPUT_LITERALS.callback;
}
