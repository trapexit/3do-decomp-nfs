#include "simulation_snapshot.h"

#include "driving_warning_display.h"
#include "game_loop.h"
#include "game_resource_cleanup.h"
#include "random_number.h"
#include "render_bounds.h"
#include "render_thread_signal.h"
#include "track_position.h"
#include "vehicle_simulation_state.h"

#include "operamath.h"

#define COMPILE_TIME_SIZE_ASSERT(name, expression) typedef char name[(expression) ? 1 : -1]
#define SIMULATION_SOURCE_BYTES 0x4C8
#define SIMULATION_RENDER_STATE_BYTES 0x108
#define SIMULATION_MODE_CONFIGURATION_BYTES 0x48
#define SIMULATION_VIEW_CONFIGURATION_BYTES 0x20
#define SIMULATION_SECONDARY_VIEW_CONFIGURATION 2
#define SIMULATION_MODE_FALLBACK 2
#define SIMULATION_MODE_CYCLE_RESET 4
#define SIMULATION_MODE_FIVE_TARGET 5
#define SIMULATION_MODE_SIX_TARGET 6
#define SIMULATION_MODE_ALTERNATE_TRANSITION 7
#define SIMULATION_MODE_NINE 9
#define SIMULATION_MODE_TRANSITION_DELAY 60
#define SIMULATION_TARGET_TRACK_GAP_MAXIMUM 12
#define SIMULATION_MODE_FIVE_TRACK_GAP_MINIMUM (-10)
#define SIMULATION_MODE_SIX_TRACK_GAP_MINIMUM (-1)
#define SIMULATION_TRACK_INTERPOLATION_RANGE 0x600
#define SIMULATION_TRACK_SAMPLE_DISTANCE 12
#define SIMULATION_TRACK_ANGLE_SCALE 0x400
#define SIMULATION_TRACK_WIDTH_SCALE 0x2000
#define SIMULATION_TRACK_REFERENCE_SHIFT 10
#define SIMULATION_TRACK_CENTER_DIVISOR 2
#define SIMULATION_TRACK_OFFSET_FRAMES 2
#define SIMULATION_TRACK_LATERAL_OFFSET 1
#define SIMULATION_TRACK_HALF_LATERAL_OFFSET 2
#define SIMULATION_FIXED8_SHIFT 8
#define SIMULATION_VERTICAL_CURVE_SHIFT 9
#define SIMULATION_INCLINE_REDUCTION_SHIFT 2
#define SIMULATION_SIGN_SHIFT 31
#define SIMULATION_TRANSITION_SHIFT_MASK 0xFF
#define SIMULATION_ANGLE_HALF_TURN 0x00800000
#define SIMULATION_ANGLE_FULL_TURN 0x01000000
#define SIMULATION_SURFACE_CLEARANCE 0x60000
#define SIMULATION_VECTOR_COMPONENT_MASK 0x00FFFC00
#define SIMULATION_RENDER_VERTICAL_MARGIN 0x28
#define SIMULATION_CAMERA_VERTICAL_ADJUSTMENT 0x78
#define SIMULATION_RENDER_LIMIT_FIXED16_SHIFT 16
#define SIMULATION_VIEW_ANGLE_DIVISOR 2
#define REPLAY_VIEW_RANDOM_RANGE 10
#define REPLAY_VIEW_TARGET_TRACK_GAP_MAXIMUM 5
#define REPLAY_VIEW_DUAL_TARGET_MODE_SIX_MINIMUM 6
#define REPLAY_VIEW_SPECIAL_REQUEST_FIRST 214
#define REPLAY_VIEW_SPECIAL_REQUEST_LAST 247
#define REPLAY_VIEW_MODE_ZERO_REQUEST_FIRST 219
#define REPLAY_VIEW_MODE_ZERO_REQUEST_LAST 227
#define REPLAY_VIEW_SPECIAL_MODE_CHANCE_MINIMUM 8
#define REPLAY_VIEW_GENERIC_MODE_COUNT 3
#define REPLAY_VIEW_MODE_ZERO_RANDOM_RANGE 4
#define REPLAY_VIEW_ROTATION_RANDOM_MINIMUM 5
#define REPLAY_VIEW_ROTATION_RANDOM_RANGE 32
#define REPLAY_VIEW_ROTATION_STEP 0x10000
#define REPLAY_VIEW_ROTATION_BIAS 0x100000
#define REPLAY_VIEW_ROTATION_WRAP 0x1000000
#define READ_SIGNED_BIG_ENDIAN_INT16(value_)                                                       \
  ((int32) (int16) ((((uint32) ((const uint8 *) (value_))[0]) << 8) |                              \
                    (uint32) ((const uint8 *) (value_))[1]))

#define LOOKUP_ROTATION_COMPONENT_A(angle_) CosF16((frac16) (angle_))
#define LOOKUP_ROTATION_COMPONENT_B(angle_) SinF16((frac16) (angle_))

extern int32 g_SIMULATION_UPDATE_SUSPENDED;
extern void
update_player_vehicle_state(void *state_);

int32                 g_SIMULATION_MODE_TRANSITION_COOLDOWN;
int32                 g_SIMULATION_MODE_CYCLE_AVAILABLE = 1;
int32                 g_SIMULATION_MODE_ZERO_SCALE;
int32                 g_SIMULATION_VECTOR_Z_ADJUSTMENT;
SimulationRenderState g_SIMULATION_RENDER_STATE;
uint32                g_SIMULATION_SECONDARY_DEPTH_SCALE_TABLE[SIMULATION_DEPTH_SCALE_TABLE_COUNT];
SimulationModeConfiguration
  g_SIMULATION_MODE_CONFIGURATIONS[SIMULATION_MODE_CONFIGURATION_COUNT] = {
    { 0, { -0x6666, 0xE666, 0 }, 0, 0, 1, 1, 0, 0, 1, 0, -1, 0, { 0, 0, 0 }, 0 },
    { 1,
      { 0, 0x20000, -0x50000 },
      0x140000,
      0,
      2,
      2,
      1,
      3,
      0,
      0x20,
      -1,
      0,
      { 0, 0x18000, -0x30000 },
      1 },
    { 1, { 0, 0x30000, -0x80000 }, 0x140000, 0, 3, 9, 0, 3, 0, 0x20, -1, 0, { 0, 0, 0 }, 0 },
    { 1, { 0, 0x40000, -0x100000 }, 0x140000, 1, 4, 9, 1, 5, 0, 0x20, -1, 0, { 0, 0, 0 }, 0 },
    { 1, { 0, 0x3CCCC, -0x80000 }, 0x50000, 1, 5, 9, 1, 5, 0, 0x20, -1, 0, { 0, 0, 0 }, 0 },
    { 1,
      { 0, 0x3CCCC, -0x80000 },
      0x140000,
      1,
      6,
      9,
      1,
      5,
      0,
      0x20,
      -1,
      0,
      { 0, 0x18000, -0x30000 },
      1 },
    { 1,
      { 0, 0x30000, -0x63333 },
      0x50000,
      1,
      9,
      9,
      1,
      5,
      0,
      0x20,
      -1,
      0,
      { 0, 0x18000, -0x30000 },
      1 },
    { 1,
      { -0x6666, 0xE666, 0 },
      0x140000,
      0,
      0,
      0,
      0,
      5,
      0,
      0x5A,
      0,
      -0x38000,
      { 0x60000, 0x80000, -0x260000 },
      1 },
    { 1, { 0, 0x30000, -0xC0000 }, 0, 0, 9, 9, 0, 0, 1, 0x20, -1, 0, { 0, 0, 0 }, 0 },
    { 1, { -0x6666, 0xE666, 0 }, 0x140000, 0, 0, 0, 0, 4, 0, 0x28, 0, -0x38000, { 0, 0, 0 }, 0 },
    { 1,
      { 0, 0x60000, -0x100000 },
      0xC0000,
      1,
      9,
      9,
      1,
      5,
      0,
      0x20,
      -1,
      0,
      { 0, 0x18000, -0x30000 },
      1 }
  };
uint32 *g_SIMULATION_PRIMARY_DEPTH_SCALE_TABLE;
SimulationViewConfiguration
  g_SIMULATION_VIEW_CONFIGURATIONS[SIMULATION_VIEW_CONFIGURATION_COUNT] = {
    { 0x46, 0x140, 0, -0x24, 0x140, 0xA2, 0x10000, NULL },
    { 0x46, 0x140, 0, 0, 0x140, 0xF0, 0x10000, NULL },
    { 0x23, 0x40, 0, 0x17, 0x40, 0x8C, 0x8000, NULL }
  };

COMPILE_TIME_SIZE_ASSERT(SimulationSourceSize, sizeof(SimulationSource) == SIMULATION_SOURCE_BYTES);
COMPILE_TIME_SIZE_ASSERT(SimulationRenderStateSize,
                         sizeof(SimulationRenderState) == SIMULATION_RENDER_STATE_BYTES);
COMPILE_TIME_SIZE_ASSERT(SimulationModeConfigurationSize,
                         sizeof(SimulationModeConfiguration) ==
                           SIMULATION_MODE_CONFIGURATION_BYTES);
COMPILE_TIME_SIZE_ASSERT(SimulationViewConfigurationSize,
                         sizeof(SimulationViewConfiguration) ==
                           SIMULATION_VIEW_CONFIGURATION_BYTES);
typedef char
  SimulationModeConfigurationsSize[(sizeof(g_SIMULATION_MODE_CONFIGURATIONS) == 0x318) ? 1 : -1];
typedef char
  SimulationViewConfigurationsSize[(sizeof(g_SIMULATION_VIEW_CONFIGURATIONS) == 0x60) ? 1 : -1];

void
initialize_simulation_state(SimulationRenderState  *state_,
                            const Vector3          *position_,
                            const Vector3          *vector_,
                            const SimulationSource *source_,
                            int32                   mode_)
{
  state_->position_source = position_;
  if(position_ == NULL)
    {
      state_->base_position.x = 0;
      state_->base_position.y = 0;
      state_->base_position.z = 0;
    }
  else
    state_->base_position = *position_;

  state_->vector_source = vector_;
  if(vector_ == NULL)
    {
      state_->base_vector.x = 0;
      state_->base_vector.y = 0;
      state_->base_vector.z = 0;
    }
  else
    {
      state_->base_vector    = *vector_;
      state_->current_vector = state_->base_vector;
    }

  state_->position_offset.x = 0;
  state_->position_offset.y = 0;
  state_->position_offset.z = 0;
  state_->vector_offset.x   = 0;
  state_->vector_offset.y   = 0;
  state_->vector_offset.z   = 0;
  if(source_ == NULL)
    state_->track_segment_index = 0;
  else
    state_->track_segment_index = source_->track_segment_index;

  state_->mode_elapsed   = 0;
  state_->requested_mode = mode_;
  state_->position_flags = 1;
  state_->source         = source_;
  set_simulation_mode(state_, mode_);
}

void
initialize_simulation_state_from_source(SimulationRenderState  *state_,
                                        const SimulationSource *source_,
                                        int32                   mode_)
{
  initialize_simulation_state(state_, &source_->position, &source_->direction, source_, mode_);
  state_->track_segment_index = source_->track_segment_index;
  set_simulation_mode(state_, state_->mode);
}

void
set_simulation_track_segment(int32 track_segment_index_)
{
  if(g_SIMULATION_RENDER_STATE.track_segment_index == (uint32) track_segment_index_)
    return;

  g_SIMULATION_RENDER_STATE.track_segment_index = track_segment_index_;
  g_SIMULATION_RENDER_STATE.position_flags |= 1;
}

void
initialize_replay_simulation_view(int32 view_context_, int32 request_code_)
{
  int32 mode_six_available;
  int32 mode_five_available;
  int32 rotation;

  (void) view_context_;
  mode_six_available =
    is_mode_six_vehicle_track_gap_in_range(0, REPLAY_VIEW_TARGET_TRACK_GAP_MAXIMUM);
  mode_five_available =
    is_mode_five_vehicle_track_gap_in_range(0, REPLAY_VIEW_TARGET_TRACK_GAP_MAXIMUM);

  if(mode_six_available != 0)
    {
      if(mode_five_available != 0)
        {
          if(scale_by_random_fraction(REPLAY_VIEW_RANDOM_RANGE) >=
             REPLAY_VIEW_DUAL_TARGET_MODE_SIX_MINIMUM)
            {
              g_SIMULATION_RENDER_STATE.requested_mode = SIMULATION_MODE_SIX_TARGET;
              initialize_simulation_state_from_source(
                &g_SIMULATION_RENDER_STATE,
                &g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_MODE_SIX_TARGET]
                   ->simulation_source,
                SIMULATION_MODE_SIX_TARGET);
            }
          else
            {
              g_SIMULATION_RENDER_STATE.requested_mode = SIMULATION_MODE_FIVE_TARGET;
              initialize_simulation_state_from_source(
                &g_SIMULATION_RENDER_STATE,
                &g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_MODE_FIVE_TARGET]
                   ->simulation_source,
                SIMULATION_MODE_FIVE_TARGET);
            }
        }
      else
        {
          g_SIMULATION_RENDER_STATE.requested_mode = SIMULATION_MODE_SIX_TARGET;
          initialize_simulation_state_from_source(
            &g_SIMULATION_RENDER_STATE,
            &g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_MODE_SIX_TARGET]
               ->simulation_source,
            SIMULATION_MODE_SIX_TARGET);
        }
    }
  else if(mode_five_available != 0)
    {
      g_SIMULATION_RENDER_STATE.requested_mode = SIMULATION_MODE_FIVE_TARGET;
      initialize_simulation_state_from_source(
        &g_SIMULATION_RENDER_STATE,
        &g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_MODE_FIVE_TARGET]->simulation_source,
        SIMULATION_MODE_FIVE_TARGET);
    }
  else
    {
      if((request_code_ >= REPLAY_VIEW_SPECIAL_REQUEST_FIRST) &&
         (request_code_ <= REPLAY_VIEW_SPECIAL_REQUEST_LAST) &&
         (scale_by_random_fraction(REPLAY_VIEW_RANDOM_RANGE) >=
          REPLAY_VIEW_SPECIAL_MODE_CHANCE_MINIMUM))
        g_SIMULATION_RENDER_STATE.requested_mode = 1;
      else if((request_code_ >= REPLAY_VIEW_MODE_ZERO_REQUEST_FIRST) &&
              (request_code_ <= REPLAY_VIEW_MODE_ZERO_REQUEST_LAST))
        g_SIMULATION_RENDER_STATE.requested_mode =
          scale_by_random_fraction(REPLAY_VIEW_MODE_ZERO_RANDOM_RANGE);
      else
        g_SIMULATION_RENDER_STATE.requested_mode =
          scale_by_random_fraction(REPLAY_VIEW_GENERIC_MODE_COUNT) + 1;

      initialize_simulation_state_from_source(
        &g_SIMULATION_RENDER_STATE,
        &g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_PLAYER]->simulation_source,
        (int32) g_SIMULATION_RENDER_STATE.requested_mode);
    }

  if((g_GAME_RESOURCE_STATE.sound_effect_request_range_count >= 0) &&
     (g_SIMULATION_RENDER_STATE.requested_mode > 0) &&
     (scale_by_random_fraction(REPLAY_VIEW_RANDOM_RANGE) > REPLAY_VIEW_ROTATION_RANDOM_MINIMUM))
    {
      rotation =
        (scale_by_random_fraction(REPLAY_VIEW_ROTATION_RANDOM_RANGE) * REPLAY_VIEW_ROTATION_STEP) -
        REPLAY_VIEW_ROTATION_BIAS;
      if(rotation < 0)
        rotation += REPLAY_VIEW_ROTATION_WRAP;
      g_GAME_RESOURCE_STATE.replay_vehicle_sprite_rotation = rotation;
    }
  else
    g_GAME_RESOURCE_STATE.replay_vehicle_sprite_rotation = 0;
}

void
update_simulation_state(SimulationRenderState *state_)
{
  SimulationModeConfiguration *configuration;
  const SimulationSource      *source;
  TrackSegment                *segment;
  TrackSegment                *sample_behind;
  TrackSegment                *sample_ahead;
  Vector3                      direction;
  Vector3                      boundary_offset;
  Vector3                      target_position;
  int32                        component_a;
  int32                        component_b;
  int32                        current_angle;
  int32                        next_angle;
  int32                        interpolation;
  int32                        remaining_interpolation;
  int32                        direction_x;
  int32                        direction_z;
  int32                        lateral_position;
  int32                        longitudinal_position;
  int32                        boundary_position;
  int32                        surface_height;
  int32                        transition_shift;

  if(g_RENDER_THREAD_STATE.simulation_update_suspended != 0)
    return;

  configuration = &g_SIMULATION_MODE_CONFIGURATIONS[state_->mode];
  if((state_->mode == SIMULATION_MODE_FIVE_TARGET) &&
     (is_mode_five_vehicle_track_gap_in_range(SIMULATION_MODE_FIVE_TRACK_GAP_MINIMUM,
                                              SIMULATION_TARGET_TRACK_GAP_MAXIMUM) == 0))
    {
      update_simulation_mode_selection(state_);
      return;
    }
  if((state_->mode == SIMULATION_MODE_SIX_TARGET) &&
     (is_mode_six_vehicle_track_gap_in_range(SIMULATION_MODE_SIX_TRACK_GAP_MINIMUM,
                                             SIMULATION_TARGET_TRACK_GAP_MAXIMUM) == 0))
    {
      update_simulation_mode_selection(state_);
      return;
    }

  source = state_->source;
  if(configuration->uses_track_geometry != 0)
    {
      segment       = &g_TRACK_SEGMENTS[source->track_segment_index];
      current_angle = READ_SIGNED_BIG_ENDIAN_INT16(&segment->heading);
      component_b = LOOKUP_ROTATION_COMPONENT_B(current_angle << SIMULATION_TRACK_REFERENCE_SHIFT);
      component_a = LOOKUP_ROTATION_COMPONENT_A(current_angle << SIMULATION_TRACK_REFERENCE_SHIFT);
      lateral_position =
        (((segment->surface_reference.z - source->position.z) >> SIMULATION_FIXED8_SHIFT) *
         (component_b >> SIMULATION_FIXED8_SHIFT)) -
        (((segment->surface_reference.x - source->position.x) >> SIMULATION_FIXED8_SHIFT) *
         (component_a >> SIMULATION_FIXED8_SHIFT));
      lateral_position -=
        (((int32) segment->midpoint_positive_width - (int32) segment->midpoint_negative_width) *
         SIMULATION_TRACK_WIDTH_SCALE) /
        SIMULATION_TRACK_CENTER_DIVISOR;

      if(configuration->uses_track_geometry == SIMULATION_TRACK_LATERAL_OFFSET)
        {
          configuration->transition_vector.x = -(lateral_position + (lateral_position >> 1));
          state_->interpolation_frames       = SIMULATION_TRACK_OFFSET_FRAMES;
        }
      else if(configuration->uses_track_geometry == SIMULATION_TRACK_HALF_LATERAL_OFFSET)
        {
          configuration->transition_vector.x = -lateral_position >> 1;
          state_->interpolation_frames       = SIMULATION_TRACK_OFFSET_FRAMES;
        }
    }

  if(state_->interpolation_frames != 0)
    {
      transition_shift = configuration->transition_shift & SIMULATION_TRANSITION_SHIFT_MASK;
      state_->transition_vector.x +=
        (configuration->transition_vector.x - state_->transition_vector.x) >> transition_shift;
      state_->transition_vector.y +=
        (configuration->transition_vector.y - state_->transition_vector.y) >> transition_shift;
      state_->transition_vector.z +=
        (configuration->transition_vector.z - state_->transition_vector.z) >> transition_shift;
      --state_->interpolation_frames;

      if((state_->mode == 0) ||
         (state_->transition_vector.z >= configuration->vertical_threshold) ||
         (state_->interpolation_frames == 0))
        {
          state_->transition_vector    = configuration->transition_vector;
          state_->interpolation_frames = 0;
          if(configuration->next_mode >= 0)
            {
              state_->requested_mode = configuration->next_mode;
              set_simulation_mode(state_, configuration->next_mode);
              return;
            }
        }
    }

  if(state_->mode == 0)
    {
      state_->vector_offset.x = (g_SIMULATION_MODE_ZERO_SCALE >> SIMULATION_FIXED8_SHIFT) *
                                (source->mode_zero_x_rotation >> SIMULATION_FIXED8_SHIFT);
      state_->vector_offset.z = (g_SIMULATION_MODE_ZERO_SCALE >> SIMULATION_FIXED8_SHIFT) *
                                (source->mode_zero_z_rotation >> SIMULATION_FIXED8_SHIFT);
      rotate_vector3_y_fixed8(&state_->transition_vector,
                              state_->vector_source->y,
                              &state_->position_offset);
      state_->base_position.x = state_->position_source->x + state_->position_offset.x;
      state_->base_position.y = state_->position_source->y + state_->position_offset.y;
      state_->base_position.z = state_->position_source->z + state_->position_offset.z;
      set_simulation_track_segment(source->track_segment_index);
    }
  else if(state_->mode == VEHICLE_SIMULATION_DISABLED_MODE)
    {
      state_->transition_vector = configuration->transition_vector;
      state_->position_offset   = state_->transition_vector;
      state_->base_position.x   = state_->position_source->x + state_->position_offset.x;
      state_->base_position.y   = state_->position_source->y + state_->position_offset.y;
      state_->base_position.z   = state_->position_source->z + state_->position_offset.z;
      state_->position_flags |=
        adjust_track_segment_index(&state_->base_position, (int32 *) &state_->track_segment_index);
    }
  else
    {
      interpolation           = source->track_interpolation >> SIMULATION_FIXED8_SHIFT;
      remaining_interpolation = SIMULATION_TRACK_INTERPOLATION_RANGE - interpolation;
      segment                 = &g_TRACK_SEGMENTS[source->track_segment_index];
      sample_behind           = segment - SIMULATION_TRACK_SAMPLE_DISTANCE;
      sample_ahead            = segment + SIMULATION_TRACK_SAMPLE_DISTANCE;

      direction_x =
        (interpolation *
           ((sample_ahead[1].surface_reference.x - sample_behind[1].surface_reference.x) >>
            SIMULATION_FIXED8_SHIFT) +
         remaining_interpolation *
           ((sample_ahead->surface_reference.x - sample_behind->surface_reference.x) >>
            SIMULATION_FIXED8_SHIFT)) /
        SIMULATION_TRACK_INTERPOLATION_RANGE;
      direction_z =
        (interpolation *
           ((sample_ahead[1].surface_reference.z - sample_behind[1].surface_reference.z) >>
            SIMULATION_FIXED8_SHIFT) +
         remaining_interpolation *
           ((sample_ahead->surface_reference.z - sample_behind->surface_reference.z) >>
            SIMULATION_FIXED8_SHIFT)) /
        SIMULATION_TRACK_INTERPOLATION_RANGE;

      current_angle =
        READ_SIGNED_BIG_ENDIAN_INT16(&segment->surface_incline) * SIMULATION_TRACK_ANGLE_SCALE;
      if(current_angle > SIMULATION_ANGLE_HALF_TURN)
        current_angle -= SIMULATION_ANGLE_FULL_TURN;
      next_angle = READ_SIGNED_BIG_ENDIAN_INT16(&(segment + 1)->surface_incline) *
                   SIMULATION_TRACK_ANGLE_SCALE;
      if(next_angle > SIMULATION_ANGLE_HALF_TURN)
        next_angle -= SIMULATION_ANGLE_FULL_TURN;
      current_angle = interpolation * (next_angle >> SIMULATION_FIXED8_SHIFT) +
                      remaining_interpolation * (current_angle >> SIMULATION_FIXED8_SHIFT);
      next_angle    = current_angle >> SIMULATION_INCLINE_REDUCTION_SHIFT;
      if(next_angle > 0)
        next_angle = (next_angle - (current_angle >> SIMULATION_SIGN_SHIFT)) >> 1;

      rotate_vector3_y_fixed8(&state_->transition_vector,
                              Atan2F16(direction_z, direction_x),
                              &state_->position_offset);
      state_->base_position.x = state_->position_source->x + state_->position_offset.x;
      component_b             = LOOKUP_ROTATION_COMPONENT_B(next_angle);
      state_->position_offset.y +=
        (state_->transition_vector.z >> SIMULATION_VERTICAL_CURVE_SHIFT) *
        (component_b >> SIMULATION_FIXED8_SHIFT);
      state_->base_position.y = state_->position_source->y + state_->position_offset.y;
      state_->base_position.z = state_->position_source->z + state_->position_offset.z;

      if(G_ACTIVE_SIMULATION_SOURCE->simulation_source.update_callback ==
         update_player_vehicle_state)
        {
          surface_height = calculate_track_surface_height((int32) state_->track_segment_index,
                                                          &state_->base_position) +
                           SIMULATION_SURFACE_CLEARANCE;
          if(surface_height < state_->base_position.y)
            state_->base_position.y = surface_height;
        }

      state_->position_flags |=
        adjust_track_segment_index(&state_->base_position, (int32 *) &state_->track_segment_index);
      segment           = &g_TRACK_SEGMENTS[state_->track_segment_index];
      boundary_offset.x = state_->base_position.x - segment->surface_reference.x;
      boundary_offset.z = state_->base_position.z - segment->surface_reference.z;
      current_angle =
        -READ_SIGNED_BIG_ENDIAN_INT16(&segment->heading) * SIMULATION_TRACK_ANGLE_SCALE;
      component_a       = LOOKUP_ROTATION_COMPONENT_A(current_angle);
      component_b       = LOOKUP_ROTATION_COMPONENT_B(current_angle);
      lateral_position  = (((boundary_offset.z >> SIMULATION_FIXED8_SHIFT) * component_b) +
                           ((boundary_offset.x >> SIMULATION_FIXED8_SHIFT) * component_a)) >>
                          SIMULATION_FIXED8_SHIFT;
      boundary_position = (int32) segment->positive_boundary_width * SIMULATION_TRACK_WIDTH_SCALE;
      if(lateral_position <= boundary_position)
        {
          boundary_position =
            -((int32) segment->negative_boundary_width * SIMULATION_TRACK_WIDTH_SCALE);
          if(lateral_position >= boundary_position)
            boundary_position = lateral_position;
        }

      if(boundary_position != lateral_position)
        {
          longitudinal_position =
            (((boundary_offset.z >> SIMULATION_FIXED8_SHIFT) * component_a) -
             ((boundary_offset.x >> SIMULATION_FIXED8_SHIFT) * component_b)) >>
            SIMULATION_FIXED8_SHIFT;
          boundary_offset.x = boundary_position;
          boundary_offset.y = 0;
          boundary_offset.z = longitudinal_position;
          rotate_vector3_y_fixed8(&boundary_offset,
                                  READ_SIGNED_BIG_ENDIAN_INT16(&segment->heading)
                                    << SIMULATION_TRACK_REFERENCE_SHIFT,
                                  &boundary_offset);
          state_->base_position.x = segment->surface_reference.x + boundary_offset.x;
          state_->base_position.z = segment->surface_reference.z + boundary_offset.z;
        }

      direction.x = direction_x;
      direction.y = 0;
      direction.z = direction_z;
      normalize_vector3_in_place(&direction);
      target_position.x =
        source->position.x +
        ((configuration->movement_scale * (direction.x >> SIMULATION_FIXED8_SHIFT)) >>
         SIMULATION_FIXED8_SHIFT);
      target_position.y =
        source->position.y +
        ((configuration->movement_scale * (direction.y >> SIMULATION_FIXED8_SHIFT)) >>
         SIMULATION_FIXED8_SHIFT);
      target_position.z =
        source->position.z +
        ((configuration->movement_scale * (direction.z >> SIMULATION_FIXED8_SHIFT)) >>
         SIMULATION_FIXED8_SHIFT);
      state_->vector_offset.y = Atan2F16(target_position.z - state_->base_position.z,
                                         target_position.x - state_->base_position.x) -
                                state_->vector_source->y;
      state_->vector_offset.x =
        -state_->vector_source->x -
        Atan2F16(calculate_vector3_planar_distance(&target_position, &state_->base_position),
                 target_position.y - state_->base_position.y);
      state_->vector_offset.z = -state_->vector_source->z;
    }

  if(state_->vector_source != NULL)
    {
      state_->base_vector.x =
        (state_->vector_source->x + state_->vector_offset.x) & SIMULATION_VECTOR_COMPONENT_MASK;
      state_->base_vector.z =
        (state_->vector_source->z + state_->vector_offset.z) & SIMULATION_VECTOR_COMPONENT_MASK;
      state_->base_vector.y =
        (state_->vector_source->y + state_->vector_offset.y) & SIMULATION_VECTOR_COMPONENT_MASK;
    }
  state_->current_position = state_->base_position;
  state_->current_vector   = state_->base_vector;
  if(state_->mode != 0)
    state_->current_vector.z += g_SIMULATION_VECTOR_Z_ADJUSTMENT;
}

void
initialize_simulation_projection_tables(void)
{
  SimulationCameraConfiguration *camera_configuration;
  SimulationModeConfiguration   *mode_nine_configuration;
  SimulationModeConfiguration   *mode_zero_configuration;
  SimulationViewConfiguration   *primary_view_configuration;
  SimulationViewConfiguration   *secondary_view_configuration;
  SimulationViewConfiguration   *shared_view_configuration;
  int32                          half_view_angle;
  int32                          transition_height;

  primary_view_configuration = &g_SIMULATION_VIEW_CONFIGURATIONS[0];
  secondary_view_configuration =
    &g_SIMULATION_VIEW_CONFIGURATIONS[SIMULATION_SECONDARY_VIEW_CONFIGURATION];
  shared_view_configuration = &g_SIMULATION_VIEW_CONFIGURATIONS[1];
  mode_nine_configuration   = &g_SIMULATION_MODE_CONFIGURATIONS[SIMULATION_MODE_NINE];
  mode_zero_configuration   = &g_SIMULATION_MODE_CONFIGURATIONS[0];

  configure_render_bounds(
    (uint32) primary_view_configuration->horizontal_origin,
    (uint32) (primary_view_configuration->vertical_origin - SIMULATION_RENDER_VERTICAL_MARGIN));
  half_view_angle =
    (primary_view_configuration->view_angle_degrees / SIMULATION_VIEW_ANGLE_DIVISOR);
  configure_render_projection_degrees((uint32) half_view_angle,
                                      (uint32) half_view_angle,
                                      (uint32) primary_view_configuration->viewport_size,
                                      (uint32) primary_view_configuration->viewport_size);
  initialize_depth_scale_table();
  g_SIMULATION_PRIMARY_DEPTH_SCALE_TABLE =
    set_render_workspace_cursor(g_SIMULATION_SECONDARY_DEPTH_SCALE_TABLE);

  configure_render_bounds(
    (uint32) secondary_view_configuration->horizontal_origin,
    (uint32) (secondary_view_configuration->vertical_origin - SIMULATION_RENDER_VERTICAL_MARGIN));
  half_view_angle =
    (secondary_view_configuration->view_angle_degrees / SIMULATION_VIEW_ANGLE_DIVISOR);
  configure_render_projection_degrees((uint32) half_view_angle,
                                      (uint32) half_view_angle,
                                      (uint32) secondary_view_configuration->viewport_size,
                                      (uint32) secondary_view_configuration->viewport_size);
  initialize_depth_scale_table();

  primary_view_configuration->depth_scale_table   = g_SIMULATION_PRIMARY_DEPTH_SCALE_TABLE;
  shared_view_configuration->depth_scale_table    = g_SIMULATION_PRIMARY_DEPTH_SCALE_TABLE;
  secondary_view_configuration->depth_scale_table = g_SIMULATION_SECONDARY_DEPTH_SCALE_TABLE;

  camera_configuration = g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_PLAYER]
                           ->simulation_source.camera_configuration;
  transition_height    = camera_configuration->transition_height;
  mode_nine_configuration->transition_vector.y = transition_height;
  mode_zero_configuration->transition_vector.y = transition_height;
  primary_view_configuration->vertical_origin =
    (camera_configuration->vertical_origin - SIMULATION_CAMERA_VERTICAL_ADJUSTMENT);
}

void
rebuild_simulation_view_matrices(void)
{
  SimulationProjectionSource *source;
  Matrix3x3                   x_rotation;
  Matrix3x3                   y_rotation;
  Matrix3x3                   z_rotation;
  Matrix3x3                   combined_rotation;

  source = g_FRONTEND_RENDER_STATE.projection.source;
  initialize_matrix3x3_inverse_rotation_x(&x_rotation, source->projection_transform.x_rotation);
  initialize_matrix3x3_inverse_rotation_z(&z_rotation, source->projection_transform.z_rotation);
  initialize_matrix3x3_inverse_rotation_y(&y_rotation, source->projection_transform.y_rotation);
  multiply_matrix_3x3(&y_rotation, &z_rotation, &combined_rotation);
  multiply_matrix_3x3(&combined_rotation, &x_rotation, &source->primary_view_matrix);

  initialize_matrix3x3_inverse_rotation_y(&y_rotation, -SIMULATION_ANGLE_HALF_TURN);
  multiply_matrix_3x3(&source->primary_view_matrix,
                      &y_rotation,
                      &source->projection_transform.view_matrix);
}

void
configure_simulation_projection(SimulationProjectionSource *source_,
                                uint8                       use_secondary_projection_)
{
  int32 half_view_angle;

  if(use_secondary_projection_ == 0)
    {
      g_FRONTEND_RENDER_STATE.projection.projection = source_->primary_projection;
      g_FRONTEND_RENDER_STATE.projection.view_state = &source_->primary_view_matrix;
    }
  else
    {
      g_FRONTEND_RENDER_STATE.projection.projection = source_->secondary_projection;
      g_FRONTEND_RENDER_STATE.projection.view_state = &source_->projection_transform.view_matrix;
    }

  configure_render_bounds(
    (uint32) g_FRONTEND_RENDER_STATE.projection.projection->horizontal_origin,
    (uint32) (g_FRONTEND_RENDER_STATE.projection.projection->vertical_origin -
              SIMULATION_RENDER_VERTICAL_MARGIN));
  half_view_angle = (g_FRONTEND_RENDER_STATE.projection.projection->view_angle_degrees /
                     SIMULATION_VIEW_ANGLE_DIVISOR);
  configure_render_projection_degrees((uint32) half_view_angle,
                                      (uint32) half_view_angle,
                                      (uint32) g_FRONTEND_RENDER_STATE.projection.projection
                                        ->viewport_size,
                                      (uint32) g_FRONTEND_RENDER_STATE.projection.projection
                                        ->viewport_size);
  set_render_workspace_cursor(
    g_FRONTEND_RENDER_STATE.projection.projection->depth_scale_table);
  g_RENDER_BOUNDS.horizontal_limit <<= SIMULATION_RENDER_LIMIT_FIXED16_SHIFT;
  g_RENDER_BOUNDS.vertical_limit <<= SIMULATION_RENDER_LIMIT_FIXED16_SHIFT;
}

void
update_simulation_mode_state(const struct TrackOrientationState *orientation_)
{
  SimulationRenderState *state;
  uint32                 current_mode;
  uint32                 requested_mode;

  if(g_SIMULATION_MODE_TRANSITION_COOLDOWN != 0)
    g_SIMULATION_MODE_TRANSITION_COOLDOWN--;

  state          = (SimulationRenderState *) g_FRONTEND_RENDER_STATE.projection.source;
  current_mode   = state->mode;
  requested_mode = state->requested_mode;
  if((current_mode == requested_mode) && (current_mode != 0) &&
     (current_mode != SIMULATION_MODE_NINE))
    {
      return;
    }

  if(is_track_orientation_within_bounds(orientation_) == 0)
    {
      g_SIMULATION_MODE_TRANSITION_COOLDOWN = SIMULATION_MODE_TRANSITION_DELAY;
      current_mode                          = state->mode;
      if((current_mode != 0) && (current_mode != SIMULATION_MODE_NINE))
        {
          return;
        }
      requested_mode = SIMULATION_MODE_FALLBACK;
    }
  else
    {
      requested_mode = state->requested_mode;
      if(state->mode == requested_mode)
        return;
      if(g_SIMULATION_MODE_TRANSITION_COOLDOWN != 0)
        return;
      if(requested_mode == 0)
        requested_mode = SIMULATION_MODE_NINE;
    }

  set_simulation_mode(state, (int32) requested_mode);
}

void
update_simulation_mode_selection(SimulationRenderState *state_)
{
  const SimulationModeConfiguration *configuration;
  const SimulationSource            *source;
  int32                              requested_mode;

  for(;;)
    {
      configuration = &g_SIMULATION_MODE_CONFIGURATIONS[state_->mode];
      if(G_FRONTEND_EXIT_REQUESTED == 1)
        requested_mode = configuration->exit_next_mode;
      else if((state_->mode != SIMULATION_MODE_SIX_TARGET) &&
              (is_mode_six_vehicle_track_gap_in_range(SIMULATION_MODE_SIX_TRACK_GAP_MINIMUM,
                                                      SIMULATION_TARGET_TRACK_GAP_MAXIMUM) != 0) &&
              (g_SIMULATION_MODE_CYCLE_AVAILABLE != 0))
        {
          requested_mode                    = SIMULATION_MODE_SIX_TARGET;
          g_SIMULATION_MODE_CYCLE_AVAILABLE = 0;
        }
      else if((state_->mode != SIMULATION_MODE_FIVE_TARGET) &&
              (is_mode_five_vehicle_track_gap_in_range(SIMULATION_MODE_FIVE_TRACK_GAP_MINIMUM,
                                                       SIMULATION_TARGET_TRACK_GAP_MAXIMUM) != 0) &&
              (g_SIMULATION_MODE_CYCLE_AVAILABLE != 0))
        {
          requested_mode                    = SIMULATION_MODE_FIVE_TARGET;
          g_SIMULATION_MODE_CYCLE_AVAILABLE = 0;
        }
      else
        requested_mode = configuration->normal_next_mode;

      state_->requested_mode = requested_mode;
      source                 = NULL;
      if(requested_mode == SIMULATION_MODE_FIVE_TARGET)
        {
          if(is_mode_five_vehicle_track_gap_in_range(SIMULATION_MODE_FIVE_TRACK_GAP_MINIMUM,
                                                     SIMULATION_TARGET_TRACK_GAP_MAXIMUM) != 0)
            source = &g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_MODE_FIVE_TARGET]
                        ->simulation_source;
        }
      else if(requested_mode == SIMULATION_MODE_SIX_TARGET)
        {
          if(is_mode_six_vehicle_track_gap_in_range(SIMULATION_MODE_SIX_TRACK_GAP_MINIMUM,
                                                    SIMULATION_TARGET_TRACK_GAP_MAXIMUM) != 0)
            source = &g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_MODE_SIX_TARGET]
                        ->simulation_source;
        }
      else if((requested_mode != SIMULATION_MODE_NINE) && (requested_mode != 0))
        source = &g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_PLAYER]->simulation_source;
      else if((g_SIMULATION_MODE_TRANSITION_COOLDOWN == 0) &&
              (is_track_orientation_within_bounds(
                 (const TrackOrientationState
                    *) &g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_PLAYER]
                   ->simulation_source) != 0))
        source = &g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_PLAYER]->simulation_source;

      if(source != NULL)
        {
          initialize_simulation_state_from_source(&g_SIMULATION_RENDER_STATE,
                                                  source,
                                                  requested_mode);
          break;
        }

      state_->mode = requested_mode;
    }

  if(state_->requested_mode == SIMULATION_MODE_CYCLE_RESET)
    g_SIMULATION_MODE_CYCLE_AVAILABLE = 1;
  set_simulation_mode(state_, state_->requested_mode);
}

void
set_simulation_mode(SimulationRenderState *state_, int32 mode_)
{
  const SimulationModeConfiguration *configuration;
  int32                              previous_mode;
  int32                              use_alternate_transition;

  if(mode_ == 0)
    {
      if(is_track_orientation_within_bounds((const TrackOrientationState *) state_->source) == 0)
        mode_ = SIMULATION_MODE_FALLBACK;
      else
        {
          g_DRIVING_WARNING_DISPLAY_STATE.dashboard_vertical_adjustment    = 0;
          g_DRIVING_WARNING_DISPLAY_STATE.dashboard_vertical_target        = 0;
          g_DRIVING_WARNING_DISPLAY_STATE.dashboard_vertical_return_target = 0;
        }
    }

  previous_mode        = state_->mode;
  state_->mode         = mode_;
  state_->mode_elapsed = 0;
  configuration        = &g_SIMULATION_MODE_CONFIGURATIONS[mode_];
  state_->view_configuration =
    &g_SIMULATION_VIEW_CONFIGURATIONS[configuration->view_configuration_index];
  state_->current_flag = (uint8) configuration->current_flag;
  state_->secondary_view_configuration =
    &g_SIMULATION_VIEW_CONFIGURATIONS[SIMULATION_SECONDARY_VIEW_CONFIGURATION];
  state_->position_flags       = 1;
  state_->interpolation_frames = configuration->interpolation_frames;
  if(state_->interpolation_frames == 0)
    state_->transition_vector = configuration->transition_vector;

  use_alternate_transition = configuration->alternate_transition_enabled;
  if(mode_ == previous_mode)
    use_alternate_transition = 0;
  if(g_SIMULATION_MODE_TRANSITION_COOLDOWN != 0)
    use_alternate_transition = 0;
  if(use_alternate_transition != 0)
    use_alternate_transition = is_track_orientation_within_bounds(
      (const TrackOrientationState *) &g_VEHICLE_SIMULATION_STATES[VEHICLE_SIMULATION_STATE_PLAYER]
        ->simulation_source);
  if((previous_mode != 0) && (mode_ != SIMULATION_MODE_ALTERNATE_TRANSITION))
    use_alternate_transition = 0;
  if(use_alternate_transition != 0)
    state_->transition_vector = configuration->alternate_transition_vector;

  state_->vector_offset.x = 0;
  state_->vector_offset.y = 0;
  state_->vector_offset.z = 0;
  update_simulation_state(state_);
  request_vehicle_render_sort_refresh();
}

void
capture_simulation_render_state(SimulationRenderState *state_)
{
  state_->render_mode                         = state_->mode;
  state_->render_position                     = state_->current_position;
  state_->render_vector                       = state_->current_vector;
  state_->render_flag                         = state_->current_flag;
  state_->render_track_segment_index          = state_->track_segment_index;
  state_->render_position_flags               = state_->position_flags;
  state_->render_view_configuration           = state_->view_configuration;
  state_->render_secondary_view_configuration = state_->secondary_view_configuration;
}
