#include "controller_input.h"
#include "frontend_audio_control.h"
#include "frontend_checkpoint.h"
#include "frontend_frame.h"
#include "frontend_overlay_configuration.h"
#include "frontend_runtime_state.h"
#include "frontend_tween_registry.h"
#include "kernel_base_reference.h"
#include "post_race_flow.h"
#include "race_state.h"
#include "random_number.h"
#include "runtime_folios.h"
#include "runtime_memory.h"
#include "sdk_veneers.h"
#include "tracked_memory.h"

#define FRONTEND_MODE_EVENT 1
#define FRONTEND_MODE_STANDARD 2
#define FRONTEND_MODE_FINAL 5
#define FRONTEND_MODE_PURSUIT 6
#define FRONTEND_MODE_RANKED 7
#define FRONTEND_SPLASH_VARIANTS 3
#define FRONTEND_EVENT_RESULT_STRIDE 0xB0
#define FRONTEND_EVENT_INDEX_OFFSET 5
#define FRONTEND_PLAYER_VEHICLE_INDEX_OFFSET 6
#define FRONTEND_DISABLE_SAVE_FLAG 2
#define FRONTEND_DISABLE_OPTION_FLAG 4
#define FRONTEND_SPLASH_SHAPE "load"
#define FRONTEND_SPLASH_PATH_BYTES 36
#define FRONTEND_DIABLO_SPLASH_PATH_BYTES 40

/* Keep retail-inlined event preparation in the mode dispatcher's source scope. */
#define prepare_frontend_event(configuration_)                                             \
  do                                                                                      \
    {                                                                                     \
      FrontendOverlayConfiguration *prepare_configuration = (configuration_);             \
      prepare_configuration->startup_configuration[FRONTEND_EVENT_INDEX_OFFSET] = 0;      \
      if(g_FRONTEND_DISPLAY_RESOURCE != NULL)                                             \
        {                                                                                 \
          free_primary_memory(g_FRONTEND_DISPLAY_RESOURCE);                               \
          g_FRONTEND_DISPLAY_RESOURCE = NULL;                                             \
        }                                                                                 \
                                                                                          \
      if(run_frontend_skill_selection() == 0)                                             \
        run_frontend_event_presentation(prepare_configuration->flags);                    \
    }                                                                                     \
  while(0)

typedef struct FrontendSplashPathStorage
{
  char supra[FRONTEND_SPLASH_VARIANTS][FRONTEND_SPLASH_PATH_BYTES];
  char diablo[FRONTEND_SPLASH_VARIANTS][FRONTEND_DIABLO_SPLASH_PATH_BYTES];
  char porsche911[FRONTEND_SPLASH_VARIANTS][FRONTEND_SPLASH_PATH_BYTES];
  char corvette[FRONTEND_SPLASH_VARIANTS][FRONTEND_SPLASH_PATH_BYTES];
  char ferrari512tr[FRONTEND_SPLASH_VARIANTS][FRONTEND_SPLASH_PATH_BYTES];
  char viper[FRONTEND_SPLASH_VARIANTS][FRONTEND_SPLASH_PATH_BYTES];
  char nsx[FRONTEND_SPLASH_VARIANTS][FRONTEND_SPLASH_PATH_BYTES];
  char rx7[FRONTEND_SPLASH_VARIANTS][FRONTEND_SPLASH_PATH_BYTES];
} FrontendSplashPathStorage;

static const FrontendSplashPathStorage G_FRONTEND_SPLASH_PATH_STORAGE = {
  { "frontend/display/loading/supra0.3sh",
    "frontend/display/loading/supra1.3sh",
    "frontend/display/loading/supra2.3sh" },
  { "frontend/display/loading/diablo0.3sh",
    "frontend/display/loading/diablo1.3sh",
    "frontend/display/loading/diablo2.3sh" },
  { "frontend/display/loading/9110.3sh",
    "frontend/display/loading/9111.3sh",
    "frontend/display/loading/9112.3sh" },
  { "frontend/display/loading/vette0.3sh",
    "frontend/display/loading/vette1.3sh",
    "frontend/display/loading/vette2.3sh" },
  { "frontend/display/loading/512tr0.3sh",
    "frontend/display/loading/512tr1.3sh",
    "frontend/display/loading/512tr2.3sh" },
  { "frontend/display/loading/viper0.3sh",
    "frontend/display/loading/viper1.3sh",
    "frontend/display/loading/viper2.3sh" },
  { "frontend/display/loading/nsx0.3sh",
    "frontend/display/loading/nsx1.3sh",
    "frontend/display/loading/nsx2.3sh" },
  { "frontend/display/loading/rx70.3sh",
    "frontend/display/loading/rx71.3sh",
    "frontend/display/loading/rx72.3sh" }
};

char                       *g_FRONTEND_PLAYER_NAME;
char                       *g_FRONTEND_SKILL_HEADING;
void                       *g_FRONTEND_DISPLAY_CONTEXT;
void                       *g_POST_RACE_CURRENT_RESULT;
MemoryDisplayInputCallback *g_MEMORY_DISPLAY_INPUT_CALLBACK_SLOT;
int32                      *g_POST_RACE_RESULT_SLOT;
void                       *g_FRONTEND_RESERVED_RUNTIME_LINK;
void                       *g_FRONTEND_JOYSTICK_CALIBRATION_STATE;
int32                      *g_FRONTEND_ACTIVE_PLAYER_INDEX;
int32                      *g_FRONTEND_SHARED_CONTROL_VALUE_A;
int32                      *g_FRONTEND_SHARED_CONTROL_VALUE_B;
RaceConfiguration          *g_RACE_CONFIGURATION;
uint8                      *g_RACE_PARTICIPANT_RECORDS;
RaceTimingState            *g_RACE_TIMING_STATE;
int32                       g_POST_RACE_SAVE_ENABLED = 1;

const char *g_FRONTEND_SPLASH_PATHS[24] = {
  G_FRONTEND_SPLASH_PATH_STORAGE.supra[0],        G_FRONTEND_SPLASH_PATH_STORAGE.supra[1],
  G_FRONTEND_SPLASH_PATH_STORAGE.supra[2],        G_FRONTEND_SPLASH_PATH_STORAGE.diablo[0],
  G_FRONTEND_SPLASH_PATH_STORAGE.diablo[1],       G_FRONTEND_SPLASH_PATH_STORAGE.diablo[2],
  G_FRONTEND_SPLASH_PATH_STORAGE.porsche911[0],   G_FRONTEND_SPLASH_PATH_STORAGE.porsche911[1],
  G_FRONTEND_SPLASH_PATH_STORAGE.porsche911[2],   G_FRONTEND_SPLASH_PATH_STORAGE.corvette[0],
  G_FRONTEND_SPLASH_PATH_STORAGE.corvette[1],     G_FRONTEND_SPLASH_PATH_STORAGE.corvette[2],
  G_FRONTEND_SPLASH_PATH_STORAGE.ferrari512tr[0], G_FRONTEND_SPLASH_PATH_STORAGE.ferrari512tr[1],
  G_FRONTEND_SPLASH_PATH_STORAGE.ferrari512tr[2], G_FRONTEND_SPLASH_PATH_STORAGE.viper[0],
  G_FRONTEND_SPLASH_PATH_STORAGE.viper[1],        G_FRONTEND_SPLASH_PATH_STORAGE.viper[2],
  G_FRONTEND_SPLASH_PATH_STORAGE.nsx[0],          G_FRONTEND_SPLASH_PATH_STORAGE.nsx[1],
  G_FRONTEND_SPLASH_PATH_STORAGE.nsx[2],          G_FRONTEND_SPLASH_PATH_STORAGE.rx7[0],
  G_FRONTEND_SPLASH_PATH_STORAGE.rx7[1],          G_FRONTEND_SPLASH_PATH_STORAGE.rx7[2]
};

typedef char FrontendSplashPathsSizeCheck[sizeof(g_FRONTEND_SPLASH_PATHS) == 96 ? 1 : -1];
typedef char
  FrontendSplashPathStorageSizeCheck[sizeof(G_FRONTEND_SPLASH_PATH_STORAGE) == 876 ? 1 : -1];

typedef struct KernelBase *FrontendKernelBasePointer;

extern uint32                   *g_FRONTEND_RANDOM_VALUE_DESTINATION;
extern FrontendKernelBasePointer KernelBase;


static void
_install_runtime_links(FrontendOverlayConfiguration *configuration_);
static void
_run_frontend_mode(FrontendOverlayConfiguration *configuration_, int32 mode_);

/* Needle's hybrid entry detector recognizes the literal "int main(" token. */
int
main(int32 mode_, FrontendOverlayConfiguration *configuration_)
{
  int32 splash_index;

  _install_runtime_links(configuration_);
  if(mode_ == 0)
    return 0;

  initialize_memory_tracker();
  reset_frontend_tween_registry();
  initialize_controller_input();
  initialize_runtime_folios();
  generate_frontend_random_values();

  if((configuration_->flags & FRONTEND_DISABLE_SAVE_FLAG) != 0)
    g_POST_RACE_SAVE_ENABLED = 0;
  if((configuration_->flags & FRONTEND_DISABLE_OPTION_FLAG) != 0)
    g_FRONTEND_PRESENTATION_ENABLED = 0;

  _run_frontend_mode(configuration_, mode_);
  shutdown_controller_input();
  g_FRONTEND_GAMMA_PALETTE_ENABLED = 0;
  splash_index = (configuration_->startup_configuration[FRONTEND_PLAYER_VEHICLE_INDEX_OFFSET] *
                    FRONTEND_SPLASH_VARIANTS +
                  random_below(FRONTEND_SPLASH_VARIANTS));
  display_frontend_splash(g_FRONTEND_SPLASH_PATHS[splash_index], FRONTEND_SPLASH_SHAPE);
  release_all_primary_allocations();
  release_all_secondary_allocations();
  return 0;
}

static void
_install_runtime_links(FrontendOverlayConfiguration *configuration_)
{
  *G_KERNEL_BASE_REFERENCE   = configuration_->kernel_base;
  g_RUNTIME_MEMORY_SERVICES  = (RuntimeMemoryServices *) configuration_;
  g_RACE_CONFIGURATION       = (RaceConfiguration *) configuration_->startup_configuration;
  g_RACE_PARTICIPANT_RECORDS = (uint8 *) configuration_->driver_skill_table;
  g_FRONTEND_DISPLAY_CONTEXT = configuration_->text_render_state;
  g_RACE_TIMING_STATE        = (RaceTimingState *) configuration_->drive_statistics;
  g_MEMORY_DISPLAY_INPUT_CALLBACK_SLOT =
    (MemoryDisplayInputCallback *) configuration_->control_pad_reader;
  g_CONTROLLER_AUXILIARY_CONTROL_BITS   = (uint32 *) configuration_->control_input_buttons;
  g_FRONTEND_JOYSTICK_CALIBRATION_STATE = configuration_->joystick_calibration;
  g_FRONTEND_RANDOM_VALUE_DESTINATION   = configuration_->random_state;
  g_FRONTEND_PLAYER_NAME                = (char *) configuration_->shared_selection_state;
  g_FRONTEND_ACTIVE_PLAYER_INDEX        = configuration_->active_player_index;
  g_FRONTEND_SKILL_HEADING              = (char *) configuration_->shared_control_mode;
  g_POST_RACE_RESULT_SLOT               = (int32 *) configuration_->vehicle_control_button_masks;
  g_FRONTEND_SHARED_CONTROL_VALUE_A     = configuration_->shared_control_value_a;
  g_FRONTEND_SHARED_CONTROL_VALUE_B     = configuration_->shared_control_value_b;
  g_POST_RACE_CURRENT_RESULT = (configuration_->drive_statistics +
                                configuration_->startup_configuration[FRONTEND_EVENT_INDEX_OFFSET] *
                                  FRONTEND_EVENT_RESULT_STRIDE);
}

static void
_run_frontend_mode(FrontendOverlayConfiguration *configuration_, int32 mode_)
{
  int32 flow_result = 0;

  switch(mode_)
    {
      case FRONTEND_MODE_EVENT:
        configuration_->startup_configuration[FRONTEND_EVENT_INDEX_OFFSET] = 0;
        run_frontend_event_presentation(configuration_->flags);
        break;
      case FRONTEND_MODE_STANDARD:
        flow_result = run_ranked_checkpoint_flow();
        if(flow_result == 0)
          prepare_frontend_event(configuration_);
        break;
      case FRONTEND_MODE_FINAL:
        shutdown_runtime_loader();
        run_final_post_race_flow();
        prepare_frontend_event(configuration_);
        break;
      case FRONTEND_MODE_PURSUIT:
        flow_result = run_pursuit_post_race_flow();
        if(flow_result == 0)
          prepare_frontend_event(configuration_);
        break;
      case FRONTEND_MODE_RANKED:
        flow_result = run_ranked_post_race_flow();
        if(flow_result == 0)
          prepare_frontend_event(configuration_);
        break;
      default:
        break;
    }
}
