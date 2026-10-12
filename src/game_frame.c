#include "game_frame.h"

#include "control_input.h"
#include "display_buffer_state.h"
#include "frontend_render_state.h"
#include "frontend_transition.h"
#include "game_audio_system.h"
#include "game_frame_count.h"
#include "game_loop.h"
#include "grayscale_palette.h"
#include "render_thread_signal.h"
#include "shutdown_callbacks.h"
#include "simulation_snapshot.h"
#include "simulation_thread_signal.h"
#include "vbl_timer.h"
#include "video_window_state.h"
#include "window_fill.h"

#include "audio.h"
#include "item.h"
#include "task.h"

#include "stddef.h"

#define GAME_FRAME_TASK_PRIORITY 94
#define GAME_FRAME_AUDIO_START_OFFSET 2
#define GAME_FRAME_AUDIO_INTERVAL 4
#define GAME_FRAME_FADE_FRAME_COUNT 16
#define GAME_FRAME_START_BUTTON 0x01000000

typedef char SizeAssertGameFrameRunningOffset[(offsetof(GameFrameState, running) == 0x04) ? 1 : -1];
typedef char SizeAssertGameFrameCueOffset[(offsetof(GameFrameState, cue) == 0x10) ? 1 : -1];
typedef char
  SizeAssertGameFrameCueSignalOffset[(offsetof(GameFrameState, cue_signal) == 0x14) ? 1 : -1];
typedef char SizeAssertGameFrameStateSize[(sizeof(GameFrameState) == 0x2C) ? 1 : -1];

GameFrameState g_GAME_FRAME_STATE = { 0, 1, { 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int32          g_FRONTEND_INPUT_OVERRIDE;

uint32
run_game_frame(void)
{
  uint32 control_input;

  /* This preserves the oracle's single scheduling and exit-control boundary. */
  control_input              = 0;
  g_GAME_FRAME_STATE.running = 1;
  g_CONTROL_PAD_READER       = read_forced_control_pad;
  (void) SetItemPri(KernelBase->kb_CurrentTask->t.n_Item, GAME_FRAME_TASK_PRIORITY);
  g_GAME_FRAME_STATE.initial_audio_time = GetAudioTime();
  g_GAME_FRAME_STATE.scheduled_audio_time =
    (g_GAME_FRAME_STATE.initial_audio_time + GAME_FRAME_AUDIO_START_OFFSET);
  g_CONTROL_PAD_READER = read_forced_control_pad;

  while(g_GAME_FRAME_STATE.running != 0)
    {
      g_GAME_FRAME_STATE.scheduled_audio_time =
        (g_GAME_FRAME_STATE.scheduled_audio_time + GAME_FRAME_AUDIO_INTERVAL);
      (void) SignalAtTime(g_GAME_FRAME_STATE.cue, GetAudioTime() + GAME_FRAME_AUDIO_INTERVAL);
      (void) WaitSignal(g_GAME_FRAME_STATE.cue_signal);
      g_GAME_FRAME_STATE.frame_number++;
      if(g_GAME_FRAME_STATE.frame_limit > g_GAME_FRAME_STATE.frame_progress)
        g_GAME_FRAME_STATE.frame_progress++;

      control_input = poll_control_input();
      if(((control_input & GAME_FRAME_START_BUTTON) != 0) && (G_FRONTEND_EXIT_REQUESTED == 1))
        g_GAME_FRAME_STATE.running = 0;

      if((g_FRONTEND_RENDER_STATE.menu.transition_active != 0) &&
         (g_FRONTEND_RENDER_STATE.menu.render_complete != 0))
        {
          control_input              = GAME_FRAME_START_BUTTON;
          g_GAME_FRAME_STATE.running = 0;
        }

      if(g_FRONTEND_INPUT_OVERRIDE != 0)
        g_GAME_FRAME_STATE.running = 0;

      if(g_RENDER_THREAD_STATE.simulation_update_suspended < 0)
        {
          store_current_display_buffer_descriptor_snapshot();
          fill_display_window(0);
          prepare_pending_video_window();
          fill_display_window(0);
          apply_gamma_grayscale_clut();
          /* ARM60 signed negation wraps here exactly as in the retail oracle. */
          fade_display_clut_to_black(0,
                                     GAME_FRAME_FADE_FRAME_COUNT,
                                     -g_RENDER_THREAD_STATE.simulation_update_suspended);
          g_RENDER_THREAD_STATE.simulation_update_suspended = 0;
        }
      else if(g_RENDER_THREAD_STATE.simulation_update_suspended > 0)
        {
          schedule_render_vbl_deadline(1);
          wait_for_render_vbl_deadline();
          fade_display_clut_from_black(0,
                                       GAME_FRAME_FADE_FRAME_COUNT,
                                       g_RENDER_THREAD_STATE.simulation_update_suspended);
          g_RENDER_THREAD_STATE.simulation_update_suspended = 0;
        }

      if(g_GAME_RUNTIME_STATE.flags_18 > 0)
        {
          stop_all_game_audio_channels();
          control_input                 = 0;
          g_GAME_FRAME_STATE.running    = 0;
          g_GAME_RUNTIME_STATE.flags_18 = 0;
        }

      if(g_GAME_FRAME_STATE.previous_game_frame_counter != (uint32) G_GAME_FRAME_COUNTER)
        {
          if((G_GAME_FRAME_COUNTER & 1) == 0)
            disable_control_input_thread();
        }
      else if((g_GAME_FRAME_STATE.frame_number & 1) == 0)
        disable_control_input_thread();

      g_GAME_FRAME_STATE.previous_game_frame_counter = (uint32) G_GAME_FRAME_COUNTER;
      signal_simulation_thread();
    }

  return control_input;
}
