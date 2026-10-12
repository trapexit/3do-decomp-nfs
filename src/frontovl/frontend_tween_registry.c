#include "frontend_tween_registry.h"

#include "runtime_memory.h"

#define EMPTY_TWEEN_SLOT_MASK 0
#define EMPTY_TWEEN_VALUE 0
#define NO_TWEEN_ARGUMENT ((void *) 0)
#define NO_TWEEN_CALLBACK ((FrontendTweenCallback) 0)

FrontendTweenRegistryControl g_FRONTEND_TWEEN_REGISTRY_CONTROL         = { EMPTY_TWEEN_SLOT_MASK,
                                                                           EMPTY_TWEEN_SLOT_MASK };
int32 g_FRONTEND_TWEEN_FRAMES_REMAINING[FRONTEND_TWEEN_SLOT_COUNT]     = { EMPTY_TWEEN_VALUE };
int32 g_FRONTEND_TWEEN_HORIZONTAL_POSITIONS[FRONTEND_TWEEN_SLOT_COUNT] = { EMPTY_TWEEN_VALUE };
int32 g_FRONTEND_TWEEN_TARGET_HORIZONTAL_POSITIONS[FRONTEND_TWEEN_SLOT_COUNT] = {
  EMPTY_TWEEN_VALUE
};
int32 g_FRONTEND_TWEEN_VERTICAL_POSITIONS[FRONTEND_TWEEN_SLOT_COUNT] = { EMPTY_TWEEN_VALUE };
int32 g_FRONTEND_TWEEN_TARGET_VERTICAL_POSITIONS[FRONTEND_TWEEN_SLOT_COUNT] = { EMPTY_TWEEN_VALUE };
void *g_FRONTEND_TWEEN_FIRST_CALLBACK_ARGUMENTS[FRONTEND_TWEEN_SLOT_COUNT]  = { NO_TWEEN_ARGUMENT };
void *g_FRONTEND_TWEEN_SECOND_CALLBACK_ARGUMENTS[FRONTEND_TWEEN_SLOT_COUNT] = { NO_TWEEN_ARGUMENT };
FrontendTweenCallback g_FRONTEND_TWEEN_CALLBACKS[FRONTEND_TWEEN_SLOT_COUNT] = { NO_TWEEN_CALLBACK };
uint32                g_FRONTEND_TWEEN_PREVIOUS_TICK                        = EMPTY_TWEEN_VALUE;

void
reset_frontend_tween_registry(void)
{
  g_FRONTEND_TWEEN_REGISTRY_CONTROL.allocated_slot_mask = 0;
  g_FRONTEND_TWEEN_REGISTRY_CONTROL.active_slot_mask    = 0;
}

int32
register_frontend_tween(int32                 horizontal_position_,
                        int32                 target_horizontal_position_,
                        int32                 vertical_position_,
                        int32                 target_vertical_position_,
                        int32                 duration_,
                        void                 *first_callback_argument_,
                        void                 *second_callback_argument_,
                        FrontendTweenCallback callback_)
{
  uint32 slot_bit;
  int32  slot;

  slot_bit = 1;
  for(slot = 0; slot < FRONTEND_TWEEN_SLOT_COUNT; slot++)
    {
      if((g_FRONTEND_TWEEN_REGISTRY_CONTROL.allocated_slot_mask & slot_bit) == 0)
        {
          break;
        }
      slot_bit <<= 1;
    }
  if(slot == FRONTEND_TWEEN_SLOT_COUNT)
    {
      return -1;
    }

  g_FRONTEND_TWEEN_REGISTRY_CONTROL.allocated_slot_mask |= slot_bit;
  g_FRONTEND_TWEEN_FRAMES_REMAINING[slot]            = duration_;
  g_FRONTEND_TWEEN_HORIZONTAL_POSITIONS[slot]        = horizontal_position_;
  g_FRONTEND_TWEEN_VERTICAL_POSITIONS[slot]          = vertical_position_;
  g_FRONTEND_TWEEN_TARGET_HORIZONTAL_POSITIONS[slot] = target_horizontal_position_;
  g_FRONTEND_TWEEN_TARGET_VERTICAL_POSITIONS[slot]   = target_vertical_position_;
  g_FRONTEND_TWEEN_FIRST_CALLBACK_ARGUMENTS[slot]    = first_callback_argument_;
  g_FRONTEND_TWEEN_SECOND_CALLBACK_ARGUMENTS[slot]   = second_callback_argument_;
  g_FRONTEND_TWEEN_CALLBACKS[slot]                   = callback_;

  return slot;
}

void
release_frontend_tween(int32 slot_)
{
  uint32 slot_mask;

  slot_mask = ~(1U << slot_);
  g_FRONTEND_TWEEN_REGISTRY_CONTROL.allocated_slot_mask &= slot_mask;
  g_FRONTEND_TWEEN_REGISTRY_CONTROL.active_slot_mask &= slot_mask;
}

void
complete_frontend_tweens(uint32 slot_mask_)
{
  FrontendTweenCallback callback;
  void                 *first_callback_argument;
  void                 *second_callback_argument;
  uint32 current_slot_mask;
  int32  horizontal_position;
  int32  vertical_position;
  int32  slot;

  current_slot_mask = 1;
  for(slot = 0; slot < FRONTEND_TWEEN_SLOT_COUNT; slot++)
    {
      if((slot_mask_ & current_slot_mask) != 0)
        {
          g_FRONTEND_TWEEN_REGISTRY_CONTROL.active_slot_mask |= current_slot_mask;
          second_callback_argument = g_FRONTEND_TWEEN_SECOND_CALLBACK_ARGUMENTS[slot];
          first_callback_argument  = g_FRONTEND_TWEEN_FIRST_CALLBACK_ARGUMENTS[slot];
          vertical_position        = g_FRONTEND_TWEEN_VERTICAL_POSITIONS[slot];
          horizontal_position      = g_FRONTEND_TWEEN_HORIZONTAL_POSITIONS[slot];
          callback                 = g_FRONTEND_TWEEN_CALLBACKS[slot];
          callback(horizontal_position,
                   vertical_position,
                   first_callback_argument,
                   second_callback_argument);
        }
      current_slot_mask <<= 1;
    }

  g_FRONTEND_TWEEN_PREVIOUS_TICK = get_runtime_animation_tick();
}

int32
update_frontend_tweens(void)
{
  FrontendTweenCallback callback;
  void                 *first_callback_argument;
  void                 *second_callback_argument;
  int32  previous_horizontal_position;
  int32  previous_vertical_position;
  int32  remaining_frames;
  int32  active_tween_count;
  int32  elapsed_ticks;
  uint32 slot_bit;
  int32  slot;

  begin_runtime_frame();
  active_tween_count = 0;
  elapsed_ticks      = get_runtime_elapsed_animation_ticks(g_FRONTEND_TWEEN_PREVIOUS_TICK);
  while(elapsed_ticks < 1)
    {
      elapsed_ticks = get_runtime_elapsed_animation_ticks(g_FRONTEND_TWEEN_PREVIOUS_TICK);
    }
  g_FRONTEND_TWEEN_PREVIOUS_TICK = get_runtime_animation_tick();
  clear_runtime_display(0);

  slot_bit = 1;
  for(slot = 0; slot < FRONTEND_TWEEN_SLOT_COUNT; slot++)
    {
      if((g_FRONTEND_TWEEN_REGISTRY_CONTROL.active_slot_mask & slot_bit) != 0)
        {
          remaining_frames                        = g_FRONTEND_TWEEN_FRAMES_REMAINING[slot] - 1;
          g_FRONTEND_TWEEN_FRAMES_REMAINING[slot] = remaining_frames;

          if(remaining_frames >= 0)
            {
              previous_horizontal_position = g_FRONTEND_TWEEN_HORIZONTAL_POSITIONS[slot];
              previous_vertical_position   = g_FRONTEND_TWEEN_VERTICAL_POSITIONS[slot];
              if(previous_horizontal_position ==
                   g_FRONTEND_TWEEN_TARGET_HORIZONTAL_POSITIONS[slot] &&
                 previous_vertical_position == g_FRONTEND_TWEEN_TARGET_VERTICAL_POSITIONS[slot])
                {
                  g_FRONTEND_TWEEN_FRAMES_REMAINING[slot] = 0;
                }
              else
                {
                  g_FRONTEND_TWEEN_HORIZONTAL_POSITIONS[slot] =
                    previous_horizontal_position +
                    (g_FRONTEND_TWEEN_TARGET_HORIZONTAL_POSITIONS[slot] -
                     previous_horizontal_position) /
                      remaining_frames;
                  g_FRONTEND_TWEEN_VERTICAL_POSITIONS[slot] =
                    previous_vertical_position + (g_FRONTEND_TWEEN_TARGET_VERTICAL_POSITIONS[slot] -
                                                  previous_vertical_position) /
                                                   remaining_frames;
                }
              active_tween_count++;
            }

          second_callback_argument = g_FRONTEND_TWEEN_SECOND_CALLBACK_ARGUMENTS[slot];
          first_callback_argument  = g_FRONTEND_TWEEN_FIRST_CALLBACK_ARGUMENTS[slot];
          previous_vertical_position   = g_FRONTEND_TWEEN_VERTICAL_POSITIONS[slot];
          previous_horizontal_position = g_FRONTEND_TWEEN_HORIZONTAL_POSITIONS[slot];
          callback                     = g_FRONTEND_TWEEN_CALLBACKS[slot];
          callback(previous_horizontal_position,
                   previous_vertical_position,
                   first_callback_argument,
                   second_callback_argument);
        }
      slot_bit <<= 1;
    }

  finish_runtime_frame();
  present_runtime_frame();
  return active_tween_count;
}
