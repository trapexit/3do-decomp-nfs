#include "frontend_animation_steps.h"

extern void
advance_frontend_animation_step(int32 step_);

void
advance_frontend_animation_steps(int32 first_step_, int32 step_count_)
{
  int32 step;

  step = first_step_ + step_count_;
  while(step_count_ > 0)
    {
      --step;
      advance_frontend_animation_step(step);
      --step_count_;
    }
}
