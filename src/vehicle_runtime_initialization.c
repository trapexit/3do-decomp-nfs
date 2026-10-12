#include "camera_io.h"
#include "driving_warning_display.h"
#include "file_buffer_load.h"
#include "formatted_output.h"
#include "frontend_overlay.h"
#include "frontend_transition.h"
#include "game_bootstrap.h"
#include "game_data_path.h"
#include "game_resource_initialization.h"
#include "headered_file.h"
#include "object_pool.h"
#include "player_vehicle_state.h"
#include "pointer_array.h"
#include "simulation_resource_cleanup.h"
#include "string_utilities.h"
#include "vehicle_callbacks.h"
#include "vehicle_control_input.h"
#include "vehicle_drive_statistics.h"
#include "vehicle_parameter_loader.h"
#include "vehicle_render_resource_selection.h"
#include "vehicle_render_snapshot.h"
#include "vehicle_runtime_pool.h"
#include "vehicle_selection_state.h"
#include "vehicle_simulation_state.h"

#include "blockfile.h"
#include "operamath.h"

#include "stddef.h"
#include "string.h"


#define VEHICLE_RUNTIME_OBJECT_COUNT 7
#define VEHICLE_RUNTIME_PLAYER_INDEX 0
#define VEHICLE_RUNTIME_OPPONENT_INDEX 1
#define VEHICLE_RUNTIME_POLICE_INDEX 2
#define VEHICLE_RUNTIME_TRAFFIC_MODE 2
#define VEHICLE_RUNTIME_OPPONENT_MODE 1
#define VEHICLE_RUNTIME_POLICE_MODE 3
#define VEHICLE_RUNTIME_OBJECT_OPPONENT_FLAG 4
#define VEHICLE_RUNTIME_OBJECT_POLICE_FLAG 8
#define VEHICLE_RUNTIME_OBJECT_ACTIVE_FLAG 4
#define VEHICLE_RUNTIME_OBJECT_RELEASE_FLAG 8
#define VEHICLE_RUNTIME_SORT_KEY_OFFSET 0x14
#define VEHICLE_RUNTIME_RENDER_STATE_OFFSET 0x0C
#define VEHICLE_RUNTIME_INITIAL_TRACK_SEGMENT_COUNT 1000
#define VEHICLE_RUNTIME_PATH_BYTES 100
#define VEHICLE_RUNTIME_PATH_PREFIX "DriveData/CarData/"
#define VEHICLE_RUNTIME_FAMILY_SUFFIX ".BigSpecsFam"
#define VEHICLE_RUNTIME_TEXT_SUFFIX ".spec"
#define VEHICLE_RUNTIME_CACHE_SEPARATOR "/"
#define VEHICLE_RUNTIME_CACHE_SUFFIX ".SpecsBin"
#define VEHICLE_RUNTIME_PARAMETER_BYTES 0x350
#define VEHICLE_RUNTIME_DEFAULT_WIDTH_FIXED 0x50000
#define VEHICLE_RUNTIME_DEFAULT_HEIGHT_FIXED 0x28000
#define VEHICLE_RUNTIME_PLAYER_POSITION_FIXED 0x600000
#define VEHICLE_RUNTIME_PLAYER_TRACK_POSITION 16
#define VEHICLE_RUNTIME_INITIAL_FLAGS 3
#define VEHICLE_RUNTIME_ACTIVE_VALUE 1
#define VEHICLE_RUNTIME_SECONDARY_SPEC_INDEX 2
#define VEHICLE_RUNTIME_REDUCED_MODE 2
#define VEHICLE_RUNTIME_DISABLED_INDEX -1
#define VEHICLE_RUNTIME_INITIAL_CONTROL_VALUE 0x111EB
#define VEHICLE_RUNTIME_ART_TRACK_VALUE_COUNT 3
#define VEHICLE_RUNTIME_ART_FIXED_ONE 0x10000
#define VEHICLE_RUNTIME_ART_REDUCED_CURVE_FLAG 4
#define VEHICLE_RUNTIME_REDUCED_CURVE_SCALE 0xC000
#define VEHICLE_TRACK_FLICKER_PATH_BYTES 60
#define VEHICLE_TRACK_FLICKER_DATA_BYTES 194
#define VEHICLE_TRACK_FLICKER_TEST_PROFILE 3
#define VEHICLE_TRACK_FLICKER_VEHICLE_NUMBER_OFFSET 1
#define VEHICLE_TRACK_FLICKER_PATH_PREFIX "DriveData/tracks:"
#define VEHICLE_TRACK_FLICKER_EXTENSION ".flicker"
#define VEHICLE_TRACK_FLICKER_NUMBERED_FORMAT "%s%s%d%s"
#define VEHICLE_TRACK_FLICKER_TEST_FORMAT "%s%s%s"

typedef struct VehicleRuntimeInitializationState
{
  uint8                         reserved_000[0x0C];
  Vector3                       current_position;
  Vector3                       current_vector;
  int32                         current_value_a;
  int32                         current_value_b;
  uint8                         reserved_02_c[0x24];
  int32                         track_position;
  int32                         state_054;
  int32                         state_058;
  int32                         state_05_c;
  int32                         state_060;
  int32                         state_064;
  int32                         state_068;
  int32                         state_06_c;
  int32                         state_070;
  int32                         state_074;
  int32                         render_width;
  int32                         render_height;
  uint8                         reserved_080[8];
  void                         *self;
  uint8                         reserved_08_c[0x34];
  VehicleRuntimeRenderMetadata *render_metadata;
  uint8                         render_metadata_storage[0x94];
  int32                         state_158;
  int32                         state_15_c;
  uint8                         reserved_160[0x28];
  uint32                        status_flags;
  uint8                         reserved_18_c[0x1C4];
  uint8                         state_350;
  uint8                         reserved_351[0x5B];
  int32                         state_3_ac;
  int32                         state_3_b0;
  uint8                         reserved_3_b4[0x34];
  int32                         state_3_e8;
  uint8                         reserved_3_ec[8];
  int32                         state_3_f4;
  int32                         state_3_f8;
  int32                         state_3_fc;
  int32                         state_400;
  int32                         state_404;
  int32                         state_408;
  int32                         state_40_c;
  uint8                         reserved_410[0x0C];
  int32                         state_41_c;
  int32                         state_420;
  int32                         state_424;
  int32                         state_428;
  uint8                         reserved_42_c[4];
  int32                         performance_reduction_mode;
  int32                         state_434;
  int32                         state_438;
  int32                         state_43_c;
  int32                         state_440;
  int32                         state_444;
  int32                         state_448;
  int32                         state_44_c;
  int32                         state_450;
  int32                         state_454;
  int32                         state_458;
  uint8                         reserved_45_c[8];
  int32                         state_464;
  uint8                         reserved_468[4];
  int32                         state_46_c;
  uint8                         reserved_470[4];
  int32                         control_value_a;
  int32                         control_value_b;
  int32                         state_47_c;
  uint8                         reserved_480[0x34];
  void                         *vehicle_spec_auxiliary_a;
  void                         *vehicle_spec_auxiliary_b;
  VehicleParameters            *vehicle_parameters;
  void                         *state_update_callback;
  void                         *render_update_callback;
  int32                         slot_index;
  uint32                        runtime_flags;
  int32                         configuration_mode;
  void                         *render_resource_state;
  int32                         render_detail_level;
  void                         *render_object;
  uint8                       **render_shape_pointers;
} VehicleRuntimeInitializationState;

typedef struct VehicleRuntimeArtSlotState
{
  uint8 reserved_00[0x4C];
  int32 render_width;
  int32 render_height;
  int32 render_depth;
  int32 initialized;
  uint8 reserved_5_c[0x10];
} VehicleRuntimeArtSlotState;

typedef char VehicleRuntimeObjectStateSize[(sizeof(VehicleRuntimeObjectState) == 0x4E4) ? 1 : -1];
typedef char VehicleRuntimeObjectFlagsOffset
  [(offsetof(VehicleRuntimeObjectState, runtime_flags) == 0x4CC) ? 1 : -1];
typedef char VehicleRuntimeObjectRenderWidthOffset
  [(offsetof(VehicleRuntimeObjectState, render_width) == 0x78) ? 1 : -1];
typedef char VehicleRuntimeObjectRenderHeightOffset
  [(offsetof(VehicleRuntimeObjectState, render_height) == 0x7C) ? 1 : -1];
typedef char VehicleRuntimeObjectRenderMetadataOffset
  [(offsetof(VehicleRuntimeObjectState, render_metadata) == 0xC0) ? 1 : -1];
typedef char VehicleRuntimeObjectRenderResourceStateOffset
  [(offsetof(VehicleRuntimeObjectState, render_resource_state) == 0x4D4) ? 1 : -1];
typedef char VehicleRuntimeArtSlotStateSize[(sizeof(VehicleRuntimeArtSlotState) == 0x6C) ? 1 : -1];
typedef char VehicleRuntimeArtCollisionRadiusOffset
  [(offsetof(VehicleRuntimeRenderMetadata, collision_radius) == 0x70) ? 1 : -1];
typedef char VehicleRuntimeArtHalfDimensionsOffset
  [(offsetof(VehicleRuntimeRenderMetadata, half_render_height) == 0x74) ? 1 : -1];
typedef char
  VehicleRuntimeArtScaleOffset[(offsetof(SimulationSource, art_acceleration_scales) == 0x310) ? 1 :
                                                                                                -1];
typedef char VehicleRuntimeArtReciprocalOffset
  [(offsetof(SimulationSource, art_acceleration_reciprocals) == 0x328) ? 1 : -1];
typedef char VehicleRuntimeArtFixedValueOffset
  [(offsetof(SimulationSource, art_trailing_fixed_value) == 0x340) ? 1 : -1];
typedef char VehicleRuntimeArtIntegerValueOffset
  [(offsetof(SimulationSource, art_trailing_integer_value) == 0x4A0) ? 1 : -1];
typedef char
  VehicleRuntimeInitializationStateSize[(sizeof(VehicleRuntimeInitializationState) == 0x4E4) ? 1 :
                                                                                               -1];
typedef char VehicleRuntimeInitializationMarkerOffset
  [(offsetof(VehicleRuntimeInitializationState, state_350) == 0x350) ? 1 : -1];
typedef char VehicleRuntimeInitializationParametersOffset
  [(offsetof(VehicleRuntimeInitializationState, vehicle_parameters) == 0x4BC) ? 1 : -1];
typedef char VehicleRuntimeControllerRenderCountOffset
  [(offsetof(VehicleRuntimeControllerState, render_object_count) == 0xB0) ? 1 : -1];
typedef char VehicleRuntimeControllerSoundCompleteOffset
  [(offsetof(VehicleRuntimeControllerState, sound_update_complete) == 0xE0) ? 1 : -1];
typedef char VehicleRuntimeControllerRoadRenderStateOffset
  [(offsetof(VehicleRuntimeControllerState, road_render_state) == 0xE4) ? 1 : -1];
typedef char VehicleRuntimeControllerTextModeOffset
  [(offsetof(VehicleRuntimeControllerState, vehicle_specs_text_mode) == 0xEC) ? 1 : -1];
typedef char VehicleRuntimeControllerFamilyOffset
  [(offsetof(VehicleRuntimeControllerState, vehicle_specs_family) == 0xF4) ? 1 : -1];
typedef char
  VehicleRuntimeControllerStateSize[(sizeof(VehicleRuntimeControllerState) == 0x104) ? 1 : -1];

uint8 g_VEHICLE_TRACK_FLICKER_DATA[VEHICLE_TRACK_FLICKER_DATA_BYTES];
uint8 g_VEHICLE_TRACK_FLICKER_PADDING[2];

void *
load_vehicle_track_flicker_data(void)
{
  char path[VEHICLE_TRACK_FLICKER_PATH_BYTES];

  if(g_TRACK_INDEX == VEHICLE_TRACK_FLICKER_TEST_PROFILE)
    {
      sprintf(path,
              VEHICLE_TRACK_FLICKER_TEST_FORMAT,
              VEHICLE_TRACK_FLICKER_PATH_PREFIX,
              g_STARTUP_VEHICLE_NAME_PREFIXES[g_TRACK_INDEX],
              VEHICLE_TRACK_FLICKER_EXTENSION);
    }
  else
    {
      sprintf(path,
              VEHICLE_TRACK_FLICKER_NUMBERED_FORMAT,
              VEHICLE_TRACK_FLICKER_PATH_PREFIX,
              g_STARTUP_VEHICLE_NAME_PREFIXES[g_TRACK_INDEX],
              g_EVENT_INDEX + VEHICLE_TRACK_FLICKER_VEHICLE_NUMBER_OFFSET,
              VEHICLE_TRACK_FLICKER_EXTENSION);
    }

  if(load_file_into_buffer_quiet(path, g_VEHICLE_TRACK_FLICKER_DATA) == NULL)
    {
      memset(g_VEHICLE_TRACK_FLICKER_DATA, 0, sizeof(g_VEHICLE_TRACK_FLICKER_DATA));
    }
  return g_VEHICLE_TRACK_FLICKER_DATA;
}

void
reset_vehicle_runtime_objects(void)
{
  VehicleRuntimeControllerState *controller;

  controller                      = (VehicleRuntimeControllerState *) &g_VEHICLE_RUNTIME_POOL_STATE;
  controller->vehicle_record_pool = NULL;
  controller->audio_channels_stopped = 0;
  if(controller->active_record_list == NULL)
    {
      controller->active_record_list = initialize_doubly_linked_list_storage(NULL);
    }

  initialize_game_resource_state();
  controller->road_render_state                   = 0;
  controller->sound_update_complete               = 0;
  g_GAME_RUNTIME_STATE.track_segment_count        = VEHICLE_RUNTIME_INITIAL_TRACK_SEGMENT_COUNT;
  g_GAME_RUNTIME_STATE.result                     = 0;
  g_GAME_RUNTIME_STATE.race_finish_elapsed_frames = 0;
  g_GAME_RUNTIME_STATE.flags_18                   = 0;
  g_GAME_RUNTIME_STATE.flags_14                   = 0;
  g_GAME_RUNTIME_STATE.statistics_track_segment_threshold = 0;
  g_GAME_RUNTIME_STATE.statistics_frame_window            = 0;
  g_GAME_RUNTIME_STATE.statistics_capture_complete        = 0;
  controller->render_update_pending                       = 0;
  controller->render_update_ready                         = 0;
}

void
insert_vehicle_runtime_object(VehicleRuntimeObjectState *object_)
{
  object_->runtime_flags |= VEHICLE_RUNTIME_OBJECT_ACTIVE_FLAG;
  object_->runtime_flags &= ~VEHICLE_RUNTIME_OBJECT_RELEASE_FLAG;
  insert_list_node_sorted(&g_VEHICLE_RUNTIME_POOL_STATE.active_record_list->list,
                          (DoublyLinkedNode *) object_,
                          VEHICLE_RUNTIME_SORT_KEY_OFFSET,
                          1);
}

void
deactivate_vehicle_runtime_object(VehicleRuntimeObjectState *object_)
{
  object_->runtime_flags &= ~VEHICLE_RUNTIME_OBJECT_ACTIVE_FLAG;
}

void
queue_vehicle_runtime_object_release(VehicleRuntimeObjectState *object_)
{
  object_->runtime_flags &= ~VEHICLE_RUNTIME_OBJECT_ACTIVE_FLAG;
  object_->runtime_flags |= VEHICLE_RUNTIME_OBJECT_RELEASE_FLAG;
}

void
initialize_vehicle_runtime_art_state(const VehicleDynamicRenderObject *resource_,
                                     SimulationSource                 *source_)
{
  MatrixControllerState        *controller;
  VehicleRuntimeRenderMetadata *metadata;
  int32                         index;
  int32                         value;

  controller = (MatrixControllerState *) source_->refresh_state;
  metadata   = (VehicleRuntimeRenderMetadata *) controller;
  initialize_matrix_controller_state(resource_->primary_header_value,
                                     resource_->secondary_header_value,
                                     (void *) resource_,
                                     controller);

  source_->controller_position_offset = metadata->half_render_depth;
  metadata->collision_radius =
    (int32) calculate_vector3_magnitude_64((const Vector3 *) &metadata->half_render_height);

  for(index = 0; index < VEHICLE_RUNTIME_ART_TRACK_VALUE_COUNT; index += 1)
    {
      value                                 = resource_->configuration_values[index];
      source_->track_speed_reference[index] = value;
      source_->track_speed_scale[index]     = DivSF16(VEHICLE_RUNTIME_ART_FIXED_ONE, value);
      source_->track_speed_response[index] =
        resource_->configuration_values[index + VEHICLE_RUNTIME_ART_TRACK_VALUE_COUNT];
    }

  for(index = 0; index < VEHICLE_DYNAMIC_CONFIGURATION_VALUE_COUNT; index += 1)
    {
      value                                   = resource_->scaled_token_values[index];
      source_->art_acceleration_scales[index] = value;
      if(value != 0)
        source_->art_acceleration_reciprocals[index] =
          DivSF16(VEHICLE_RUNTIME_ART_FIXED_ONE, value);
      else
        source_->art_acceleration_reciprocals[index] = 0;
    }

  source_->art_trailing_fixed_value   = resource_->trailing_fixed_value;
  source_->art_trailing_integer_value = resource_->trailing_integer_value;
  if(((source_->status_flags & VEHICLE_RUNTIME_ART_REDUCED_CURVE_FLAG) != 0) &&
     (g_STARTUP_CONFIGURATION.performance_reduction_enabled != 0))
    {
      for(index = 0; index < VEHICLE_DYNAMIC_CURVE_VALUE_COUNT; index += 1)
        source_->speed_acceleration_table[index] =
          MulUF16(resource_->curve_values[index], VEHICLE_RUNTIME_REDUCED_CURVE_SCALE);
    }
  else
    {
      for(index = 0; index < VEHICLE_DYNAMIC_CURVE_VALUE_COUNT; index += 1)
        source_->speed_acceleration_table[index] = resource_->curve_values[index];
    }

  source_->render_height_offset = metadata->half_render_height;
  source_->lateral_position     = 0;
  initialize_matrix3x3_identity(&source_->track_transform);
  initialize_matrix3x3_identity((Matrix3x3 *) source_->reserved_37_c);
}

int32
assign_vehicle_runtime_object_art(VehicleRuntimeObjectState *object_, int32 slot_index_)
{
  VehicleRuntimeArtSlotState *previous_slot;
  VehicleRuntimeArtSlotState *slot;
  VehicleDynamicRenderObject *resource;

  slot = (VehicleRuntimeArtSlotState *) &g_CAR_RENDER_RESOURCE_RECORDS[slot_index_];
  if(slot->initialized != 0)
    return 0;

  previous_slot = (VehicleRuntimeArtSlotState *) object_->render_resource_state;
  if(previous_slot != NULL)
    previous_slot->initialized = 0;

  object_->render_resource_state               = slot;
  object_->render_width                        = slot->render_width;
  object_->render_height                       = slot->render_height;
  slot->initialized                            = 1;
  object_->render_metadata->half_render_height = slot->render_height >> 1;
  object_->render_metadata->half_render_depth  = slot->render_depth >> 1;
  object_->render_metadata->half_render_width  = slot->render_width >> 1;

  resource = g_VEHICLE_DYNAMIC_RENDER_OBJECTS[slot_index_];
  initialize_vehicle_runtime_art_state(
    resource,
    (SimulationSource *) ((uint8 *) object_ + VEHICLE_RUNTIME_RENDER_STATE_OFFSET));
  return 1;
}

void
configure_vehicle_runtime_object(VehicleRuntimeObjectState *object_,
                                 const char                *resource_identifier_,
                                 int32                      configuration_mode_)
{
  VehicleRuntimeControllerState *controller;
  /* Keep state out of sl, which a file-load yield restores as stack limit. */
  VehicleRuntimeInitializationState *volatile state;
  VehicleParameterState *parameter_state;
  char                   resource_path[VEHICLE_RUNTIME_PATH_BYTES];
  char                   cache_path[VEHICLE_RUNTIME_PATH_BYTES];

  controller      = (VehicleRuntimeControllerState *) &g_VEHICLE_RUNTIME_POOL_STATE;
  state           = (VehicleRuntimeInitializationState *) object_;
  parameter_state = (VehicleParameterState *) &state->current_position;

  state->runtime_flags      = VEHICLE_RUNTIME_INITIAL_FLAGS;
  state->configuration_mode = configuration_mode_;

  copy_string(resource_path, g_GAME_DATA_PATH);
  append_string(resource_path, VEHICLE_RUNTIME_PATH_PREFIX);
  append_string(resource_path, resource_identifier_);

  if(configuration_mode_ == VEHICLE_RUNTIME_OPPONENT_MODE)
    {
      /* Retail stores this handoff in the controller's opponent slot. */
      controller->opponent_object = object_;
    }
  if(configuration_mode_ == VEHICLE_RUNTIME_PLAYER_INDEX)
    {
      /* The player slot is the backing field for the active source handoff. */
      controller->player_object     = object_;
      state->state_update_callback  = (void *) update_player_vehicle_control_mode;
      state->render_update_callback = (void *) update_driving_warning_vehicle_state;
      state->current_position.z     = VEHICLE_RUNTIME_PLAYER_POSITION_FIXED;
      state->track_position         = VEHICLE_RUNTIME_PLAYER_TRACK_POSITION;

      copy_string(cache_path, resource_path);
      append_string(cache_path, VEHICLE_RUNTIME_FAMILY_SUFFIX);
      controller->vehicle_specs_family = load_headered_file_with_default_options(cache_path);

      if(controller->vehicle_specs_text_mode == 0)
        {
          state->vehicle_parameters = (VehicleParameters *) get_headered_pointer_array_element(
            (void *const *) controller->vehicle_specs_family,
            0);
          g_ANTI_LOCK_BRAKES_ENABLED = ((g_STARTUP_CONFIGURATION.anti_lock_brakes_enabled != 0) &&
                                        (state->vehicle_parameters->anti_lock_brakes_enabled > 0));
          g_TRACTION_CONTROL_ENABLED = ((g_STARTUP_CONFIGURATION.traction_control_enabled != 0) &&
                                        (state->vehicle_parameters->traction_control_enabled > 0));
          scale_vehicle_engine_torque_curve(state->vehicle_parameters);
        }

      state->vehicle_spec_auxiliary_a =
        get_headered_pointer_array_element((void *const *) controller->vehicle_specs_family, 1);
      state->vehicle_spec_auxiliary_b =
        get_headered_pointer_array_element((void *const *) controller->vehicle_specs_family,
                                           VEHICLE_RUNTIME_SECONDARY_SPEC_INDEX);

      if(controller->vehicle_specs_text_mode != 0)
        {
          copy_string(cache_path, resource_path);
          append_string(cache_path, VEHICLE_RUNTIME_TEXT_SUFFIX);
          load_vehicle_parameter_state(parameter_state, cache_path);
          if(initialize_camera_io() != 0)
            {
              memcpy(cache_path,
                     VEHICLE_RUNTIME_CACHE_SEPARATOR,
                     sizeof(VEHICLE_RUNTIME_CACHE_SEPARATOR));
              append_string(cache_path, resource_path);
              append_string(cache_path, VEHICLE_RUNTIME_CACHE_SUFFIX);
              (void) WriteMacFile(cache_path,
                                  state->vehicle_parameters,
                                  VEHICLE_RUNTIME_PARAMETER_BYTES);
            }
          scale_vehicle_engine_torque_curve(state->vehicle_parameters);
        }

      state->render_width  = state->vehicle_parameters->body_length;
      state->render_height = state->vehicle_parameters->body_width;
      load_driving_warning_display_resource((uint8 *) resource_path, 1);
      initialize_vehicle_dynamics_parameters(parameter_state);
    }
  else
    {
      state->state_update_callback  = (void *) default_vehicle_pre_update;
      state->render_update_callback = (void *) update_vehicle_simulation_transform;
      state->render_width           = VEHICLE_RUNTIME_DEFAULT_WIDTH_FIXED;
      state->render_height          = VEHICLE_RUNTIME_DEFAULT_HEIGHT_FIXED;
      state->current_position.z     = 0;
      state->track_position         = 0;
      state->state_3_e8             = 0;
      state->state_3_f4             = 0;
    }

  /* Retail compares the opponent handoff here, but does not consume the condition flags. */
  (void) (object_ == controller->opponent_object);

  state->state_070          = 0;
  state->current_position.x = 0;
  state->current_position.y = 0;
  state->state_054          = 0;
  state->state_058          = 0;
  state->state_05_c         = 0;
  state->state_060          = 0;
  state->state_064          = 0;
  state->state_068          = 0;
  state->state_06_c         = 0;
  state->state_074          = 0;
  state->state_450          = 0;
  state->state_454          = 0;
  state->state_458          = 0;
  state->state_40_c         = VEHICLE_RUNTIME_ACTIVE_VALUE;
  state->state_464          = 0;
  state->state_408          = VEHICLE_RUNTIME_ACTIVE_VALUE;
  state->state_46_c         = 0;
  state->current_value_a    = 0;
  state->state_3_f8         = 0;
  state->state_3_fc         = 0;
  state->state_400          = 0;
  state->state_43_c         = 0;
  state->performance_reduction_mode =
    (g_STARTUP_CONFIGURATION.performance_reduction_enabled == 0) ? 0 : VEHICLE_RUNTIME_REDUCED_MODE;
  state->state_440             = 0;
  state->state_434             = VEHICLE_RUNTIME_DISABLED_INDEX;
  state->state_438             = 0;
  state->state_444             = VEHICLE_RUNTIME_ACTIVE_VALUE;
  state->state_404             = VEHICLE_RUNTIME_DISABLED_INDEX;
  state->state_448             = 0;
  state->state_44_c            = 0;
  state->state_158             = VEHICLE_RUNTIME_DISABLED_INDEX;
  state->current_vector.y      = 0;
  state->state_15_c            = VEHICLE_RUNTIME_DISABLED_INDEX;
  state->current_value_b       = 0;
  state->state_47_c            = 0;
  state->control_value_a       = VEHICLE_RUNTIME_INITIAL_CONTROL_VALUE;
  state->state_41_c            = 0;
  state->control_value_b       = VEHICLE_RUNTIME_INITIAL_CONTROL_VALUE;
  state->current_vector.x      = 0;
  state->current_vector.z      = 0;
  state->state_3_ac            = 0;
  state->state_3_b0            = 0;
  state->state_420             = 0;
  state->state_424             = 0;
  state->state_428             = 0;
  state->render_resource_state = NULL;
  state->status_flags          = 0;
  state->self                  = state;
  state->render_metadata       = (VehicleRuntimeRenderMetadata *) state->render_metadata_storage;
  state->state_350             = 0;

  capture_vehicle_render_snapshot((VehicleRenderSnapshotState *) state);
}

void
initialize_vehicle_runtime_objects(void)
{
  VehicleRuntimeObjectState *object;
  void                      *vehicle_resource;
  int32                      index;

  reset_vehicle_runtime_objects();
  initialize_vehicle_runtime_pool();
  load_vehicle_render_resources(g_PLAYER_VEHICLE_INDEX, g_OPPONENT_VEHICLE_INDEX);

  vehicle_resource =
    (void *) g_VEHICLE_RUNTIME_POOL_STATE.vehicle_resource_identifiers[g_PLAYER_VEHICLE_INDEX];
  object = (VehicleRuntimeObjectState *) acquire_pooled_node(
    &g_VEHICLE_RUNTIME_POOL_STATE.vehicle_record_pool->list);
  configure_vehicle_runtime_object(object, vehicle_resource, VEHICLE_RUNTIME_PLAYER_INDEX);
  assign_vehicle_runtime_object_art(object, VEHICLE_RUNTIME_PLAYER_INDEX);
  insert_vehicle_runtime_object(object);
  g_VEHICLE_SIMULATION_STATES[VEHICLE_RUNTIME_PLAYER_INDEX] = (VehicleSimulationState *) object;
  object->slot_index                                        = VEHICLE_RUNTIME_PLAYER_INDEX;

  for(index = VEHICLE_RUNTIME_OPPONENT_INDEX; index < VEHICLE_RUNTIME_OBJECT_COUNT; index++)
    {
      object = (VehicleRuntimeObjectState *) acquire_pooled_node(
        &g_VEHICLE_RUNTIME_POOL_STATE.vehicle_record_pool->list);
      if(index == VEHICLE_RUNTIME_OPPONENT_INDEX)
        {
          configure_vehicle_runtime_object(object, vehicle_resource, VEHICLE_RUNTIME_OPPONENT_MODE);
          object->status_flags |= VEHICLE_RUNTIME_OBJECT_OPPONENT_FLAG;
        }
      else if(index == VEHICLE_RUNTIME_POLICE_INDEX)
        {
          configure_vehicle_runtime_object(object, vehicle_resource, VEHICLE_RUNTIME_POLICE_MODE);
          object->status_flags |= VEHICLE_RUNTIME_OBJECT_POLICE_FLAG;
        }
      else
        {
          configure_vehicle_runtime_object(object, vehicle_resource, VEHICLE_RUNTIME_TRAFFIC_MODE);
        }
      assign_vehicle_runtime_object_art(object, index);
      insert_vehicle_runtime_object(object);
      g_VEHICLE_SIMULATION_STATES[index] = (VehicleSimulationState *) object;
      object->slot_index                 = index;
    }
}
