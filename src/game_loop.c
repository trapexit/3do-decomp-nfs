#include "game_loop.h"

#include "control_input.h"
#include "frontend_transition.h"
#include "game_frame.h"
#include "render_thread_signal.h"
#include "simulation_snapshot.h"

#include "mem.h"

#define CONTROL_START_BUTTON 0x1000000
#define SIMULATION_EXIT_BLOCKING_MODE 7

extern void
wait_for_frontend_input(void);

int32
run_game_loop(void)
{
  int32  should_exit;
  uint32 control_input;

  should_exit = 0;
  while(should_exit == 0)
    {
      control_input = run_game_frame();
      enable_control_input_thread();
      if((g_RENDER_THREAD_STATE.pipeline_initialized != 0) && (G_FRONTEND_EXIT_REQUESTED != 0) &&
         (g_SIMULATION_RENDER_STATE.mode != SIMULATION_EXIT_BLOCKING_MODE))
        {
          if(g_FRONTEND_INPUT_OVERRIDE != 0)
            {
              control_input             = CONTROL_START_BUTTON;
              g_FRONTEND_INPUT_OVERRIDE = 0;
            }
          if((control_input & CONTROL_START_BUTTON) == 0)
            {
              ScavengeMem();
              stop_render_thread();
              ScavengeMem();
              should_exit = 1;
            }
          else
            {
              control_input = poll_control_input();
              while((control_input & CONTROL_START_BUTTON) != 0)
                {
                  disable_control_input_thread();
                  wait_for_frontend_input();
                  control_input = poll_control_input();
                }
              should_exit = run_frontend_transition();
              if(should_exit != 0)
                {
                  ScavengeMem();
                  stop_render_thread();
                  ScavengeMem();
                }
            }
        }
    }
  enable_control_input_thread();
  return g_GAME_RUNTIME_STATE.result;
}
