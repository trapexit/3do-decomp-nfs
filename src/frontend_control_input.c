#include "frontend_control_input.h"

#include "control_input.h"
#include "frontend_input_wait.h"
#include "memory.h"

#define FRONTEND_INPUT_ZERO_INITIAL 0
#define FRONTEND_INPUT_RESERVED_04_INITIAL 0x89D189D1

FrontendInputState g_FRONTEND_INPUT_STATE = { FRONTEND_INPUT_ZERO_INITIAL,
                                              FRONTEND_INPUT_RESERVED_04_INITIAL,
                                              NULL,
                                              NULL,
                                              FRONTEND_INPUT_ZERO_INITIAL };

void
release_frontend_input_buffer(void)
{
  if(G_FRONTEND_INPUT_BUFFER == NULL)
    return;

  free_memory(G_FRONTEND_INPUT_BUFFER);
  G_FRONTEND_INPUT_BUFFER = NULL;
}

int32
poll_frontend_control_input(int32 control_pad_count_, int32 wait_, uint32 *control_input_)
{
  GetControlPad(control_pad_count_, wait_, (ControlPadEventData *) control_input_);
  *control_input_ |= G_CONTROL_INPUT_BUTTONS;
  disable_control_input_thread();
  wait_for_frontend_input();

  return (*control_input_ != 0);
}

uint32
read_frontend_control_input(void)
{
  uint32 control_input;

  poll_frontend_control_input(1, 0, &control_input);
  return control_input;
}

uint32
wait_for_frontend_control_press(uint32 control_mask_)
{
  uint32 control_input;

  for(;;)
    {
      poll_frontend_control_input(1, 0, &control_input);
      control_input &= control_mask_;
      if(control_input != g_FRONTEND_INPUT_STATE.previous_buttons)
        {
          g_FRONTEND_INPUT_STATE.previous_buttons = control_input;
          if(control_input != 0)
            return g_FRONTEND_INPUT_STATE.previous_buttons;
        }

      wait_for_frontend_input();
    }
}

void
wait_for_frontend_control_release(void)
{
  uint32 control_input;

  do
    {
      poll_frontend_control_input(1, 1, &control_input);
    }
  while(control_input == 0);

  do
    {
      poll_frontend_control_input(1, 1, &control_input);
    }
  while(control_input != 0);
}

int32
clear_frontend_control_values(uint32 *first_value_, uint32 *second_value_)
{
  *first_value_  = 0;
  *second_value_ = 0;
  return 0;
}
