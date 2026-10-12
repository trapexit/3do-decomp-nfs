#include "frontend_overlay.h"

#include "bitmap_clip.h"
#include "ccb_manager.h"
#include "cel_access.h"
#include "cel_render.h"
#include "control_input.h"
#include "diagnostics.h"
#include "display_buffer_state.h"
#include "display_coordinate_state.h"
#include "file_information.h"
#include "file_load_options.h"
#include "file_save.h"
#include "fixed_math.h"
#include "formatted_output.h"
#include "frontend_control_input.h"
#include "frontend_input_wait.h"
#include "game_bootstrap.h"
#include "grayscale_palette.h"
#include "memory.h"
#include "pointer_array.h"
#include "random_number.h"
#include "rectangle_outline.h"
#include "shape_blit.h"
#include "shape_file.h"
#include "shape_lookup.h"
#include "shutdown_callbacks.h"
#include "simulation_thread_signal.h"
#include "sound_effect_state.h"
#include "text_render_state.h"
#include "tick_timer.h"
#include "trig_lookup.h"
#include "vbl_timer.h"
#include "vehicle_control_input.h"
#include "vehicle_drive_statistics.h"
#include "vehicle_selection_state.h"
#include "vehicle_simulation_state.h"
#include "video_window_state.h"
#include "window_fill.h"
#include "window_memory.h"

#include "stdio.h"
#include "string.h"

#define FRONTEND_OVERLAY_PATH "frontovl"
#define PLAYER_SKILLS_RESOURCE_NAME "pskills"
#define PLAYER_SKILLS_RESOURCE_NAME_BYTES 8
#define STARTUP_SPECIAL_SKILL_FLAG 0x80
#define STARTUP_SPECIAL_SKILL_FLAG_CLEAR_MASK 0x7F
#define FRONTEND_OVERLAY_SHARED_SELECTION_WORDS 3
#define FRONTEND_OVERLAY_DEFAULT_FLAGS 0
#define FRONTEND_OVERLAY_SERVICE_COUNT 62
#define STARTUP_VEHICLE_NAME_SHORT_BYTES 4
#define STARTUP_VEHICLE_NAME_LONG_BYTES 8

typedef void (*FrontendOverlayService)(void);

typedef struct FrontendOverlayInterface
{
  DisplayBufferDescriptor  *display_buffer;
  struct KernelBase        *kernel_base;
  int32                     flags;
  TextRenderState          *text_render_state;
  StartupConfiguration     *startup_configuration;
  uint8                    *driver_skill_table;
  VehicleDriveStatistics   *drive_statistics;
  ControlPadReader         *control_pad_reader;
  uint32                   *control_input_buttons;
  JoystickCalibrationState *joystick_calibration;
  uint32                   *random_state;
  int32                    *shared_selection_state;
  int32                    *shared_control_mode;
  int32                    *vehicle_control_button_masks;
  int32                    *active_player_index;
  int32                    *shared_control_value_a;
  int32                    *shared_control_value_b;
  FrontendOverlayService    services[FRONTEND_OVERLAY_SERVICE_COUNT];
} FrontendOverlayInterface;

extern FrontendOverlayInterface g_FRONTEND_OVERLAY_INTERFACE;

const char  G_STARTUP_VEHICLE_NAME_PREFIX_AL[STARTUP_VEHICLE_NAME_SHORT_BYTES]  = "Al";
const char  G_STARTUP_VEHICLE_NAME_PREFIX_CL[STARTUP_VEHICLE_NAME_SHORT_BYTES]  = "Cl";
const char  G_STARTUP_VEHICLE_NAME_PREFIX_CY[STARTUP_VEHICLE_NAME_SHORT_BYTES]  = "Cy";
const char  G_STARTUP_VEHICLE_NAME_PREFIX_TEST[STARTUP_VEHICLE_NAME_LONG_BYTES] = "Test";
const char *g_STARTUP_VEHICLE_NAME_PREFIXES[4] = { G_STARTUP_VEHICLE_NAME_PREFIX_AL,
                                                   G_STARTUP_VEHICLE_NAME_PREFIX_CL,
                                                   G_STARTUP_VEHICLE_NAME_PREFIX_CY,
                                                   G_STARTUP_VEHICLE_NAME_PREFIX_TEST };
char        g_STARTUP_ARGUMENT_BUFFER[STARTUP_ARGUMENT_BUFFER_BYTES];
int32       g_FRONTEND_OVERLAY_SHARED_SELECTION_STATE[FRONTEND_OVERLAY_SHARED_SELECTION_WORDS];
int32       g_FRONTEND_OVERLAY_SHARED_CONTROL_MODE;
int32       g_FRONTEND_OVERLAY_SHARED_CONTROL_VALUE_A;
int32       g_FRONTEND_OVERLAY_SHARED_CONTROL_VALUE_B;
int32       g_FRONTEND_OVERLAY_SHARED_RESERVED_STATE[5];
int32       g_PLAYER_SKILLS_RESOURCE_REQUESTED;
int32       g_FRONTEND_OVERLAY_MODE = 1;
int32       g_FRONTEND_OVERLAY_RELOAD_REQUESTED;
int32       g_FRONTEND_OVERLAY_BYPASS;
int32       g_FRONTEND_OVERLAY_RESERVED_STATE[2];

typedef char FrontendOverlaySharedReservedStateSize
  [(sizeof(g_FRONTEND_OVERLAY_SHARED_RESERVED_STATE) == 0x14) ? 1 : -1];
typedef char
  FrontendOverlayReservedStateSize[(sizeof(g_FRONTEND_OVERLAY_RESERVED_STATE) == 8) ? 1 : -1];

FrontendOverlayInterface g_FRONTEND_OVERLAY_INTERFACE = {
  &g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT,
  NULL,
  FRONTEND_OVERLAY_DEFAULT_FLAGS,
  &g_TEXT_RENDER_STATE,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  { (FrontendOverlayService) allocate_named_memory,
    (FrontendOverlayService) free_memory,
    (FrontendOverlayService) store_current_display_buffer_descriptor_snapshot,
    (FrontendOverlayService) fill_display_window,
    (FrontendOverlayService) wait_for_frontend_control_release,
    (FrontendOverlayService) free_memory_block_chain,
    (FrontendOverlayService) print_debug_message,
    (FrontendOverlayService) vsprintf,
    (FrontendOverlayService) require_shape,
    (FrontendOverlayService) draw_cel_header_at_embedded_position,
    (FrontendOverlayService) draw_rectangle_outline,
    (FrontendOverlayService) draw_transformed_cel_header,
    (FrontendOverlayService) draw_cel_header,
    (FrontendOverlayService) draw_text_at,
    (FrontendOverlayService) blit_shape_at,
    (FrontendOverlayService) load_file_with_options_with_error,
    (FrontendOverlayService) initialize_text_render_font,
    (FrontendOverlayService) set_text_render_colors,
    (FrontendOverlayService) prepare_pending_video_window,
    (FrontendOverlayService) advance_display_buffer,
    (FrontendOverlayService) finalize_allocated_ccb_batch,
    (FrontendOverlayService) wait_for_frontend_input,
    (FrontendOverlayService) 0,
    (FrontendOverlayService) wait_for_frontend_input,
    (FrontendOverlayService) get_current_tick,
    (FrontendOverlayService) get_elapsed_ticks,
    (FrontendOverlayService) load_shape_file_data_with_error,
    (FrontendOverlayService) blit_shape_at_alias,
    (FrontendOverlayService) draw_scaled_cel_frame,
    (FrontendOverlayService) load_file_with_options,
    (FrontendOverlayService) open_file_information_default,
    (FrontendOverlayService) close_file_information,
    (FrontendOverlayService) read_file_information,
    (FrontendOverlayService) save_file,
    (FrontendOverlayService) cosine_1024,
    (FrontendOverlayService) sine_1024,
    (FrontendOverlayService) multiply_fixed16,
    (FrontendOverlayService) draw_cel_header_to_quad,
    (FrontendOverlayService) allocate_window,
    (FrontendOverlayService) store_display_buffer_descriptor_snapshot,
    (FrontendOverlayService) free_window,
    (FrontendOverlayService) fade_display_clut_from_black,
    (FrontendOverlayService) fade_display_clut_to_black,
    (FrontendOverlayService) set_ccb_pixel_control,
    (FrontendOverlayService) file_exists,
    (FrontendOverlayService) fill_window_rectangle,
    (FrontendOverlayService) next_random_word,
    (FrontendOverlayService) prepare_default_game_mode,
    (FrontendOverlayService) get_file_size,
    (FrontendOverlayService) load_headered_file,
    (FrontendOverlayService) get_headered_pointer_array_element,
    (FrontendOverlayService) load_cel_with_error,
    (FrontendOverlayService) unload_cel,
    (FrontendOverlayService) get_current_display_buffer_descriptor,
    (FrontendOverlayService) get_previous_display_buffer_descriptor,
    (FrontendOverlayService) get_pending_video_window_index,
    (FrontendOverlayService) configure_bitmap_display_state,
    (FrontendOverlayService) apply_linear_grayscale_clut,
    (FrontendOverlayService) apply_gamma_grayscale_clut,
    (FrontendOverlayService) get_render_vbl_number,
    (FrontendOverlayService) configure_display_clip_rectangle,
    (FrontendOverlayService) initialize_joystick_calibration_state }
};

typedef char FrontendOverlayInterfaceSize[(sizeof(FrontendOverlayInterface) == 0x13C) ? 1 : -1];

void
run_frontend_overlay(int32 flags_)
{
  void                 *overlay_image;
  FrontendOverlayEntry  overlay_entry;
  StartupConfiguration *startup_configuration;
  uint8                 skill_flags;

  startup_configuration                    = &g_STARTUP_CONFIGURATION;
  g_FRONTEND_OVERLAY_INTERFACE.kernel_base = KernelBase;
  overlay_image = load_file_with_options_with_error(FRONTEND_OVERLAY_PATH, 0);
  overlay_entry = execute_loaded_overlay_and_resolve_entry(overlay_image);

  g_FRONTEND_OVERLAY_INTERFACE.display_buffer         = &g_DISPLAY_BUFFER_DESCRIPTOR_SNAPSHOT;
  g_FRONTEND_OVERLAY_INTERFACE.startup_configuration  = startup_configuration;
  g_FRONTEND_OVERLAY_INTERFACE.driver_skill_table     = g_VEHICLE_DRIVER_SKILL_TABLE;
  g_FRONTEND_OVERLAY_INTERFACE.flags                  = flags_;
  g_FRONTEND_OVERLAY_INTERFACE.text_render_state      = &g_TEXT_RENDER_STATE;
  g_FRONTEND_OVERLAY_INTERFACE.drive_statistics       = &g_VEHICLE_DRIVE_STATISTICS;
  g_FRONTEND_OVERLAY_INTERFACE.control_pad_reader     = &g_CONTROL_PAD_READER;
  g_FRONTEND_OVERLAY_INTERFACE.control_input_buttons  = &G_CONTROL_INPUT_BUTTONS;
  g_FRONTEND_OVERLAY_INTERFACE.joystick_calibration   = &g_JOYSTICK_CALIBRATION_STATE;
  g_FRONTEND_OVERLAY_INTERFACE.shared_selection_state = g_FRONTEND_OVERLAY_SHARED_SELECTION_STATE;
  g_FRONTEND_OVERLAY_INTERFACE.shared_control_mode    = &g_FRONTEND_OVERLAY_SHARED_CONTROL_MODE;
  g_FRONTEND_OVERLAY_INTERFACE.vehicle_control_button_masks = &g_VEHICLE_ACCELERATE_BUTTON_MASK;
  g_FRONTEND_OVERLAY_INTERFACE.random_state                 = g_RANDOM_STATE;
  g_FRONTEND_OVERLAY_INTERFACE.active_player_index          = &g_ACTIVE_PLAYER_INDEX;
  g_FRONTEND_OVERLAY_INTERFACE.shared_control_value_a = &g_FRONTEND_OVERLAY_SHARED_CONTROL_VALUE_A;
  g_FRONTEND_OVERLAY_INTERFACE.shared_control_value_b = &g_FRONTEND_OVERLAY_SHARED_CONTROL_VALUE_B;

  overlay_entry(g_FRONTEND_OVERLAY_MODE, &g_FRONTEND_OVERLAY_INTERFACE);

  skill_flags = startup_configuration->skill_level;
  if((skill_flags & STARTUP_SPECIAL_SKILL_FLAG) != 0)
    {
      memcpy(&g_VEHICLE_ACCELERATE_BUTTON_MASK,
             PLAYER_SKILLS_RESOURCE_NAME,
             PLAYER_SKILLS_RESOURCE_NAME_BYTES);
      g_PLAYER_SKILLS_RESOURCE_REQUESTED = 1;
      startup_configuration->skill_level = skill_flags & STARTUP_SPECIAL_SKILL_FLAG_CLEAR_MASK;
    }

  g_PLAYER_VEHICLE_INDEX   = startup_configuration->player_vehicle_index;
  g_OPPONENT_VEHICLE_INDEX = startup_configuration->opponent_vehicle_index;
  g_TRACK_INDEX            = startup_configuration->track_index;
  g_EVENT_INDEX            = startup_configuration->event_index;
  sprintf(g_STARTUP_ARGUMENT_BUFFER,
          "%s%d",
          g_STARTUP_VEHICLE_NAME_PREFIXES[startup_configuration->track_index],
          startup_configuration->event_index + 1);
  g_STARTUP_ARGUMENT = (char *) g_STARTUP_ARGUMENT_BUFFER;

  free_memory(overlay_image);
  store_current_display_buffer_descriptor_snapshot();
}
