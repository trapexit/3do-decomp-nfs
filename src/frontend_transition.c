#include "frontend_transition.h"

#include "control_input_poll_state.h"
#include "frontend_state.h"
#include "render_thread_signal.h"
#include "vbl_delay.h"

#include "stddef.h"

extern int32
run_frontend_menu(void);

#define GAME_RUNTIME_FLAG_RESTART 2
#define FRONTEND_SELECTION_DISPLAY 2
#define FRONTEND_SELECTION_RESTART 3
#define FRONTEND_SELECTION_QUIT 4
#define FRONTEND_INITIAL_DELAY 15
#define FRONTEND_FRAME_DELAY 5
#define FRONTEND_RESTART_DELAY 30

GameRuntimeState g_GAME_RUNTIME_STATE;

typedef char GameRuntimeStateSize[(sizeof(GameRuntimeState) == 0x28) ? 1 : -1];
typedef char GameRuntimeStateRaceFinishOffset
  [(offsetof(GameRuntimeState, race_finish_elapsed_frames) == 0x08) ? 1 : -1];
typedef char
  GameRuntimeStateRaceStartOffset[(offsetof(GameRuntimeState, race_start_frame) == 0x0C) ? 1 : -1];

int32
run_frontend_transition(void)
{
  int32 selection;

  delay_vbls(FRONTEND_INITIAL_DELAY);
  for(;;)
    {
      selection                                      = run_frontend_menu();
      g_FRONTEND_RENDER_STATE.menu.transition_active = 0;
      switch(selection)
        {
          case 0:
            return 1;
          case 1:
            return 0;
          case FRONTEND_SELECTION_DISPLAY:
            configure_frontend_display(1, 0);
            return 0;
          case FRONTEND_SELECTION_RESTART:
            stop_render_thread();
            initialize_frontend_state();
            set_frontend_state(1, 0, 1, 1);
            start_render_thread();
            while(g_FRONTEND_RENDER_STATE.menu.render_complete == 0)
              {
                signal_render_thread();
                delay_vbls(FRONTEND_FRAME_DELAY);
              }
            delay_vbls(FRONTEND_RESTART_DELAY);
            break;
          case FRONTEND_SELECTION_QUIT:
            g_GAME_RUNTIME_STATE.flags_14 |= GAME_RUNTIME_FLAG_RESTART;
            g_GAME_RUNTIME_STATE.flags_18 |= GAME_RUNTIME_FLAG_RESTART;
            return 1;
          default:
            return 1;
        }
    }
}
