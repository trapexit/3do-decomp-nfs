#include "controller_input.h"

#include "frontend_sound.h"
#include "runtime_memory.h"
#include "runtime_timing.h"
#include "sdk_veneers.h"
#include "timer_io.h"

#include "event.h"
#include "kernel.h"

typedef struct ControllerInputState
{
  uint32 previous_control_bits;
  Item   control_device;
} ControllerInputState;

static ControllerInputState g_CONTROLLER_INPUT_STATE;
uint32                     *g_CONTROLLER_AUXILIARY_CONTROL_BITS;

enum
{
  PRIMARY_CONTROLLER_NUMBER        = 1,
  CONTROLLER_INPUT_PAD_COUNT       = 3,
  MEMORY_DISPLAY_CONTROLLER_NUMBER = 3,
  STABLE_CONTROL_SAMPLE_COUNT      = 4,
  CONFIRMATION_CONTROL_MASK        = 0x0E000000
};

uint32
read_third_controller_state(void)
{
  uint32 control_bits;

  GetControlPad(MEMORY_DISPLAY_CONTROLLER_NUMBER, 0, (ControlPadEventData *) &control_bits);
  return control_bits;
}

int32
read_controller_state(int32 controller_number_, int32 wait_for_input_, uint32 *control_bits_)
{
  GetControlPad(controller_number_, wait_for_input_, (ControlPadEventData *) control_bits_);
  *control_bits_ |= *g_CONTROLLER_AUXILIARY_CONTROL_BITS;
  return *control_bits_ != 0;
}

void
wait_for_controller_input(uint32 requested_control_mask_)
{
  uint32 control_bits;
  uint32 secondary_control_bits;

  read_controller_state(PRIMARY_CONTROLLER_NUMBER, 0, &control_bits);
  while(control_bits != 0)
    {
      read_controller_state(PRIMARY_CONTROLLER_NUMBER, 0, &control_bits);
    }

  read_controller_state(PRIMARY_CONTROLLER_NUMBER, 0, &control_bits);
  while(control_bits == 0)
    {
      GetControlPad(MEMORY_DISPLAY_CONTROLLER_NUMBER,
                    0,
                    (ControlPadEventData *) &secondary_control_bits);
      read_controller_state(PRIMARY_CONTROLLER_NUMBER, 0, &control_bits);
    }

  if(((control_bits & requested_control_mask_) != 0) || (requested_control_mask_ == 0))
    {
      if((control_bits & CONFIRMATION_CONTROL_MASK) != 0)
        {
          play_confirmation_sound();
        }
      else
        {
          play_navigation_sound();
        }
    }
  else
    {
      play_error_sound();
    }

  read_controller_state(PRIMARY_CONTROLLER_NUMBER, 0, &control_bits);
  while(control_bits != 0)
    {
      GetControlPad(MEMORY_DISPLAY_CONTROLLER_NUMBER,
                    0,
                    (ControlPadEventData *) &secondary_control_bits);
      read_controller_state(PRIMARY_CONTROLLER_NUMBER, 0, &control_bits);
    }
}

uint32
poll_controller_input(int32 frame_limit_, int32 play_feedback_, uint32 requested_control_mask_)
{
  uint32 control_bits;
  uint32 secondary_control_bits;
  int32  stable_sample_count;
  int32  frame_number;

  stable_sample_count = 0;
  read_controller_state(PRIMARY_CONTROLLER_NUMBER, 0, &control_bits);
  while((g_CONTROLLER_INPUT_STATE.previous_control_bits & control_bits) != 0)
    {
      wait_runtime_timer_ticks(g_CONTROLLER_INPUT_STATE.control_device, 1);
      read_controller_state(PRIMARY_CONTROLLER_NUMBER, 0, &control_bits);
    }

  frame_number = 0;
  while(frame_number < frame_limit_)
    {
      read_controller_state(PRIMARY_CONTROLLER_NUMBER, 0, &control_bits);
      if(control_bits != g_CONTROLLER_INPUT_STATE.previous_control_bits)
        {
          stable_sample_count                            = 0;
          g_CONTROLLER_INPUT_STATE.previous_control_bits = control_bits;
        }
      else if(control_bits != 0)
        {
          stable_sample_count++;
          if(stable_sample_count >= STABLE_CONTROL_SAMPLE_COUNT)
            {
              if(play_feedback_ != 0)
                {
                  if(((control_bits & requested_control_mask_) != 0) ||
                     (requested_control_mask_ == 0))
                    {
                      if((control_bits & CONFIRMATION_CONTROL_MASK) != 0)
                        {
                          play_confirmation_sound();
                        }
                      else
                        {
                          play_navigation_sound();
                        }
                    }
                  else
                    {
                      play_error_sound();
                    }
                }
              return g_CONTROLLER_INPUT_STATE.previous_control_bits;
            }
        }

      GetControlPad(MEMORY_DISPLAY_CONTROLLER_NUMBER,
                    0,
                    (ControlPadEventData *) &secondary_control_bits);
      synchronize_runtime_frame();
      frame_number++;
    }

  return 0;
}

uint32
read_stable_controller_input(void)
{
  uint32 control_bits;
  uint32 previous_control_bits;

  read_controller_state(PRIMARY_CONTROLLER_NUMBER, 0, &control_bits);
  if(control_bits == 0)
    {
      return 0;
    }

  previous_control_bits = control_bits;
  wait_runtime_timer_ticks(g_CONTROLLER_INPUT_STATE.control_device, 1);
  read_controller_state(PRIMARY_CONTROLLER_NUMBER, 0, &control_bits);
  while(control_bits != previous_control_bits)
    {
      previous_control_bits = control_bits;
      wait_runtime_timer_ticks(g_CONTROLLER_INPUT_STATE.control_device, 1);
      read_controller_state(PRIMARY_CONTROLLER_NUMBER, 0, &control_bits);
    }

  return previous_control_bits;
}

int32
initialize_controller_input(void)
{
  g_CONTROLLER_INPUT_STATE.previous_control_bits = 0;
  InitEventUtility(CONTROLLER_INPUT_PAD_COUNT, 0, 0);
  if(g_CONTROLLER_INPUT_STATE.control_device != 0)
    {
      return g_CONTROLLER_INPUT_STATE.control_device;
    }

  g_CONTROLLER_INPUT_STATE.control_device = create_frontend_timer_io_request();
  return g_CONTROLLER_INPUT_STATE.control_device;
}

int32
shutdown_controller_input(void)
{
  DeleteItem(g_CONTROLLER_INPUT_STATE.control_device);
  return KillEventUtility();
}
