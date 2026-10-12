#include "fixed_math.h"
#include "simulation_snapshot.h"
#include "sound_spatial.h"
#include "track_position.h"
#include "trig_lookup.h"
#include "vehicle_dynamics_state.h"

#include "stddef.h"

#define VEHICLE_ORIENTATION_INPUT_SHIFT 8
#define VEHICLE_ORIENTATION_ANGLE_SHIFT 14
#define VEHICLE_ORIENTATION_SMOOTHING 4
#define TRACK_SURFACE_OFFSET_SCALE 2
#define TRACK_BIG_ENDIAN_WORD_SHIFT 8
#define VEHICLE_TRACK_POSITION_SHIFT 12
#define VEHICLE_TRACK_POSITION_SCALE 136
#define VEHICLE_TRACK_HEIGHT_TOLERANCE 64
#define VEHICLE_TRACK_FALL_ACCELERATION (-32137)
#define VEHICLE_TRACK_MAXIMUM_SEPARATION 0x8000
#define VEHICLE_TRACK_HEIGHT_SHIFT 8
#define VEHICLE_TRACK_HEIGHT_SCALE_SHIFT 9
#define VEHICLE_TRACK_HEIGHT_ONE 0x10000
#define VEHICLE_TRACK_HEIGHT_ONE_HALF 0x18000
#define VEHICLE_TRACK_RECOVERY_STEP 983
#define VEHICLE_TRACK_ANGLE_SHIFT 14
#define VEHICLE_TRACK_MOTION_SHIFT 8
#define VEHICLE_TRACK_FORCED_FALL (-0xA0000)
#define VEHICLE_TRACK_RECOVERY_DELAY 11
#define VEHICLE_TRACK_RECOVERY_START (-2)
#define VEHICLE_TRACK_RECOVERY_SOUND 84
#define VEHICLE_TRACK_GRAVITY_SCALE 2511
#define VEHICLE_TRACK_ASCENDING_SCALE 0x2666
#define VEHICLE_TRACK_DESCENDING_SCALE 0x9999
#define VEHICLE_TRACK_SOUND_CLASS 4
#define VEHICLE_TRACK_SOUND_VARIANT 1
#define VEHICLE_TRACK_SOUND_DISTANCE 1
#define VEHICLE_TRACK_SOUND_GAIN_SHIFT 4

extern void
request_sound_effect(int32 sound_id_);

void
initialize_vehicle_surface_vectors(SimulationSource *source_)
{
  Vector3 longitudinal_vector;
  Vector3 lateral_vector;

  longitudinal_vector.x = source_->position.x;
  longitudinal_vector.y = source_->position.y;
  longitudinal_vector.z = source_->position.z;
  rotate_coordinates_fixed16(0,
                             source_->longitudinal_axle_span,
                             -source_->direction.y,
                             &longitudinal_vector.x,
                             &longitudinal_vector.z);
  longitudinal_vector.y = 0;
  rotate_coordinates_fixed16(-source_->lateral_axle_span,
                             0,
                             -source_->direction.y,
                             &lateral_vector.x,
                             &lateral_vector.z);
  lateral_vector.y = 0;

  source_->surface_longitudinal_vector.x = longitudinal_vector.x;
  source_->surface_longitudinal_vector.y = longitudinal_vector.y;
  source_->surface_longitudinal_vector.z = longitudinal_vector.z;
  source_->surface_lateral_vector.x      = lateral_vector.x;
  source_->surface_lateral_vector.y      = lateral_vector.y;
  source_->surface_lateral_vector.z      = lateral_vector.z;
}

void
initialize_alternate_vehicle_surface_vectors(SimulationSource *source_)
{
  Vector3 longitudinal_vector;
  Vector3 lateral_vector;

  longitudinal_vector.x = source_->alternate_surface_origin.x;
  longitudinal_vector.y = source_->alternate_surface_origin.y;
  longitudinal_vector.z = source_->alternate_surface_origin.z;
  rotate_coordinates_fixed16(0,
                             source_->longitudinal_axle_span,
                             -source_->alternate_heading,
                             &longitudinal_vector.x,
                             &longitudinal_vector.z);
  longitudinal_vector.y = 0;
  rotate_coordinates_fixed16(-source_->lateral_axle_span,
                             0,
                             -source_->alternate_heading,
                             &lateral_vector.x,
                             &lateral_vector.z);
  lateral_vector.y = 0;

  source_->surface_longitudinal_vector.x = longitudinal_vector.x;
  source_->surface_longitudinal_vector.y = longitudinal_vector.y;
  source_->surface_longitudinal_vector.z = longitudinal_vector.z;
  source_->surface_lateral_vector.x      = lateral_vector.x;
  source_->surface_lateral_vector.y      = lateral_vector.y;
  source_->surface_lateral_vector.z      = lateral_vector.z;
}

void
project_vehicle_surface_vectors(SimulationSource *source_, uint8 use_alternate_state_)
{
  const TrackSegment *segment;
  const uint8        *segment_bytes;
  Vector3             plane_origin;
  Vector3             plane_offset_point;
  Vector3             next_plane_origin;
  int32               surface_offset_x;
  int32               surface_offset_y;
  int32               surface_offset_z;

  if(use_alternate_state_ != 0)
    {
      initialize_alternate_vehicle_surface_vectors(source_);
      source_->alternate_surface_projection_origin.x = source_->alternate_surface_origin.x;
      source_->alternate_surface_projection_origin.y = source_->alternate_surface_origin.y;
      source_->alternate_surface_projection_origin.z = source_->alternate_surface_origin.z;
      segment       = &g_TRACK_SEGMENTS[source_->alternate_track_segment_index];
      segment_bytes = (const uint8 *) segment;
      surface_offset_x =
        (int32) (int16) (((uint16) segment_bytes[offsetof(TrackSegment, surface_offset_x)]
                          << TRACK_BIG_ENDIAN_WORD_SHIFT) |
                         segment_bytes[offsetof(TrackSegment, surface_offset_x) + 1]);
      surface_offset_y =
        (int32) (int16) (((uint16) segment_bytes[offsetof(TrackSegment, surface_offset_y)]
                          << TRACK_BIG_ENDIAN_WORD_SHIFT) |
                         segment_bytes[offsetof(TrackSegment, surface_offset_y) + 1]);
      surface_offset_z =
        (int32) (int16) (((uint16) segment_bytes[offsetof(TrackSegment, surface_offset_z)]
                          << TRACK_BIG_ENDIAN_WORD_SHIFT) |
                         segment_bytes[offsetof(TrackSegment, surface_offset_z) + 1]);
      plane_origin.x       = segment->surface_reference.x;
      plane_origin.y       = segment->surface_reference.y;
      plane_origin.z       = segment->surface_reference.z;
      plane_offset_point.x = (plane_origin.x + (surface_offset_x * TRACK_SURFACE_OFFSET_SCALE));
      plane_offset_point.y = (plane_origin.y + (surface_offset_y * TRACK_SURFACE_OFFSET_SCALE));
      plane_offset_point.z = (plane_origin.z + (surface_offset_z * TRACK_SURFACE_OFFSET_SCALE));
      next_plane_origin.x  = (segment + 1)->surface_reference.x;
      next_plane_origin.y  = (segment + 1)->surface_reference.y;
      next_plane_origin.z  = (segment + 1)->surface_reference.z;
      project_vectors_onto_plane(&source_->surface_longitudinal_vector,
                                 &source_->surface_lateral_vector,
                                 &source_->alternate_surface_projection_origin,
                                 &plane_origin,
                                 &plane_offset_point,
                                 &next_plane_origin);
      return;
    }

  initialize_vehicle_surface_vectors(source_);
  source_->surface_projection_origin.x = source_->position.x;
  source_->surface_projection_origin.y = source_->position.y;
  source_->surface_projection_origin.z = source_->position.z;
  segment                              = &g_TRACK_SEGMENTS[source_->track_segment_index];
  segment_bytes                        = (const uint8 *) segment;
  surface_offset_x =
    (int32) (int16) (((uint16) segment_bytes[offsetof(TrackSegment, surface_offset_x)]
                      << TRACK_BIG_ENDIAN_WORD_SHIFT) |
                     segment_bytes[offsetof(TrackSegment, surface_offset_x) + 1]);
  surface_offset_y =
    (int32) (int16) (((uint16) segment_bytes[offsetof(TrackSegment, surface_offset_y)]
                      << TRACK_BIG_ENDIAN_WORD_SHIFT) |
                     segment_bytes[offsetof(TrackSegment, surface_offset_y) + 1]);
  surface_offset_z =
    (int32) (int16) (((uint16) segment_bytes[offsetof(TrackSegment, surface_offset_z)]
                      << TRACK_BIG_ENDIAN_WORD_SHIFT) |
                     segment_bytes[offsetof(TrackSegment, surface_offset_z) + 1]);
  plane_origin.x       = segment->surface_reference.x;
  plane_origin.y       = segment->surface_reference.y;
  plane_origin.z       = segment->surface_reference.z;
  plane_offset_point.x = (plane_origin.x + (surface_offset_x * TRACK_SURFACE_OFFSET_SCALE));
  plane_offset_point.y = (plane_origin.y + (surface_offset_y * TRACK_SURFACE_OFFSET_SCALE));
  plane_offset_point.z = (plane_origin.z + (surface_offset_z * TRACK_SURFACE_OFFSET_SCALE));
  next_plane_origin.x  = (segment + 1)->surface_reference.x;
  next_plane_origin.y  = (segment + 1)->surface_reference.y;
  next_plane_origin.z  = (segment + 1)->surface_reference.z;
  project_vectors_onto_plane(&source_->surface_longitudinal_vector,
                             &source_->surface_lateral_vector,
                             &source_->surface_projection_origin,
                             &plane_origin,
                             &plane_offset_point,
                             &next_plane_origin);
}

void
update_vehicle_track_motion(SimulationSource *source_, int32 update_enabled_)
{
  int32 surface_height;
  int32 predicted_height;
  int32 height_difference;
  int32 sine;
  int32 sound_counter;
  int32 sound_gain;

  project_vehicle_surface_vectors(source_, 0);
  surface_height                  = source_->surface_projection_origin.y;
  g_CURRENT_VEHICLE_TRACK_SEGMENT = source_->track_segment_index;
  predicted_height = (source_->position.y + ((source_->motion.y >> VEHICLE_TRACK_POSITION_SHIFT) *
                                             VEHICLE_TRACK_POSITION_SCALE));
  if(update_enabled_ == 0)
    {
      source_->position.y      = surface_height;
      source_->motion.y        = 0;
      source_->track_gravity.z = 0;
      source_->track_gravity.x = 0;
      return;
    }

  if(surface_height < (predicted_height - VEHICLE_TRACK_HEIGHT_TOLERANCE))
    {
      source_->motion.y += VEHICLE_TRACK_FALL_ACCELERATION;
      height_difference = (predicted_height - surface_height);
      if(height_difference <= VEHICLE_TRACK_MAXIMUM_SEPARATION)
        {
          source_->surface_height_scale =
            (((VEHICLE_TRACK_MAXIMUM_SEPARATION - height_difference) >> VEHICLE_TRACK_HEIGHT_SHIFT)
             << VEHICLE_TRACK_HEIGHT_SCALE_SHIFT);
          align_vehicle_orientation_to_surface(source_, 0);
          source_->warning_display_pending = 1;
          surface_height                   = predicted_height;
        }
      else
        {
          g_VEHICLE_RESTORE_SURFACE_STATE_PENDING = 1;
          source_->surface_contact_active         = 0;
          source_->warning_display_pending        = 0;
          source_->recovery_sound_counter++;
          align_vehicle_orientation_to_surface(source_,
                                               source_->recovery_sound_counter *
                                                 VEHICLE_TRACK_RECOVERY_STEP);
          surface_height = predicted_height;
        }
    }
  else
    {
      if(surface_height > (predicted_height + VEHICLE_TRACK_HEIGHT_TOLERANCE))
        {
          height_difference = (surface_height - predicted_height);
          if(height_difference <= VEHICLE_TRACK_MAXIMUM_SEPARATION)
            {
              source_->surface_height_scale =
                (((((VEHICLE_TRACK_MAXIMUM_SEPARATION - height_difference) >>
                    VEHICLE_TRACK_HEIGHT_SHIFT)
                   << VEHICLE_TRACK_HEIGHT_SCALE_SHIFT) >>
                  1) +
                 VEHICLE_TRACK_HEIGHT_ONE);
            }
          else
            {
              source_->surface_height_scale = VEHICLE_TRACK_HEIGHT_ONE_HALF;
            }
        }
      else
        {
          source_->surface_height_scale = VEHICLE_TRACK_HEIGHT_ONE;
        }

      align_vehicle_orientation_to_surface(source_, 0);
      sine              = sine_1024(source_->direction.x >> VEHICLE_TRACK_ANGLE_SHIFT);
      source_->motion.y = -((sine >> VEHICLE_TRACK_MOTION_SHIFT) *
                            (source_->pitch_motion_scale >> VEHICLE_TRACK_MOTION_SHIFT));
      sine              = sine_1024(source_->direction.z >> VEHICLE_TRACK_ANGLE_SHIFT);
      source_->motion.y -= ((sine >> VEHICLE_TRACK_MOTION_SHIFT) *
                            (source_->longitudinal_acceleration >> VEHICLE_TRACK_MOTION_SHIFT));
      if(g_VEHICLE_SURFACE_SOUND_MODE != 0)
        source_->motion.y = VEHICLE_TRACK_FORCED_FALL;

      if(source_->recovery_sound_counter < VEHICLE_TRACK_RECOVERY_DELAY)
        {
          if(source_->recovery_sound_counter >= 0)
            {
              source_->recovery_sound_counter = 0;
            }
          else
            {
              sound_counter                   = source_->recovery_sound_counter;
              source_->recovery_sound_counter = (sound_counter + 1);
              if(sound_counter == VEHICLE_TRACK_RECOVERY_START)
                {
                  sound_gain = source_->motion.y;
                  if(sound_gain <= 0)
                    sound_gain = -sound_gain;
                  play_spatial_sound_effect_unscaled(-1,
                                                     VEHICLE_TRACK_SOUND_CLASS,
                                                     VEHICLE_TRACK_SOUND_VARIANT,
                                                     sound_gain,
                                                     VEHICLE_TRACK_SOUND_DISTANCE,
                                                     0);
                  g_VEHICLE_SURFACE_SOUND_PAN  = 0;
                  g_VEHICLE_SURFACE_SOUND_GAIN = VEHICLE_TRACK_MAXIMUM_SEPARATION
                                              << VEHICLE_TRACK_SOUND_GAIN_SHIFT;
                }
            }
        }
      else
        {
          request_sound_effect(VEHICLE_TRACK_RECOVERY_SOUND);
          source_->recovery_sound_counter = VEHICLE_TRACK_RECOVERY_START;
          sound_counter                   = source_->recovery_sound_counter;
          source_->recovery_sound_counter = (sound_counter + 1);
          if(sound_counter == VEHICLE_TRACK_RECOVERY_START)
            {
              sound_gain = source_->motion.y;
              if(sound_gain <= 0)
                sound_gain = -sound_gain;
              play_spatial_sound_effect_unscaled(-1,
                                                 VEHICLE_TRACK_SOUND_CLASS,
                                                 VEHICLE_TRACK_SOUND_VARIANT,
                                                 sound_gain,
                                                 VEHICLE_TRACK_SOUND_DISTANCE,
                                                 0);
              g_VEHICLE_SURFACE_SOUND_PAN  = 0;
              g_VEHICLE_SURFACE_SOUND_GAIN = VEHICLE_TRACK_MAXIMUM_SEPARATION
                                          << VEHICLE_TRACK_SOUND_GAIN_SHIFT;
            }
        }

      source_->warning_display_pending = 1;
      if(g_VEHICLE_RESTORE_SURFACE_STATE_PENDING == 1)
        {
          if(source_->surface_index != -1)
            source_->surface_contact_active = 1;
          g_VEHICLE_RESTORE_SURFACE_STATE_PENDING = 0;
        }
    }

  sine                     = sine_1024(source_->direction.x >> VEHICLE_TRACK_ANGLE_SHIFT);
  source_->track_gravity.z = ((sine >> VEHICLE_TRACK_MOTION_SHIFT) * VEHICLE_TRACK_GRAVITY_SCALE);
  source_->track_gravity.y = 0;
  source_->track_gravity.x = 0;
  if(source_->motion.y > 0)
    {
      source_->track_gravity.z =
        multiply_fixed16(source_->track_gravity.z, VEHICLE_TRACK_ASCENDING_SCALE);
    }
  else if(source_->motion.y < 0)
    {
      source_->track_gravity.z =
        multiply_fixed16(source_->track_gravity.z, VEHICLE_TRACK_DESCENDING_SCALE);
    }
  source_->position.y = surface_height;
}

void
align_vehicle_orientation_to_surface(SimulationSource *source_, int32 longitudinal_height_offset_)
{
  int32 target_x_rotation;
  int32 target_z_rotation;
  int32 current_x_rotation;
  int32 current_z_rotation;
  int32 x_adjustment;
  int32 z_adjustment;
  int32 x_difference;
  int32 z_difference;

  target_x_rotation = calculate_signed_arcsine_ratio(
                        (longitudinal_height_offset_ - source_->surface_longitudinal_vector.y) >>
                          VEHICLE_ORIENTATION_INPUT_SHIFT,
                        source_->longitudinal_axle_span >> VEHICLE_ORIENTATION_INPUT_SHIFT)
                   << VEHICLE_ORIENTATION_ANGLE_SHIFT;
  target_z_rotation = calculate_signed_arcsine_ratio(
                        -source_->surface_lateral_vector.y >> VEHICLE_ORIENTATION_INPUT_SHIFT,
                        source_->lateral_axle_span >> VEHICLE_ORIENTATION_INPUT_SHIFT)
                   << VEHICLE_ORIENTATION_ANGLE_SHIFT;

  current_x_rotation = source_->direction.x;
  x_adjustment       = ((target_x_rotation - current_x_rotation) / VEHICLE_ORIENTATION_SMOOTHING);
  if(x_adjustment <= 0)
    x_adjustment = -x_adjustment;

  current_z_rotation = source_->direction.z;
  z_adjustment       = ((target_z_rotation - current_z_rotation) / VEHICLE_ORIENTATION_SMOOTHING);
  if(z_adjustment <= 0)
    z_adjustment = -z_adjustment;

  x_difference = (current_x_rotation - target_x_rotation);
  if(x_difference > x_adjustment)
    source_->direction.x = (current_x_rotation - x_adjustment);
  else if(x_difference < -x_adjustment)
    source_->direction.x = (current_x_rotation + x_adjustment);

  z_difference = (current_z_rotation - target_z_rotation);
  if(z_difference > z_adjustment)
    source_->direction.z = (current_z_rotation - z_adjustment);
  else if(z_difference < -z_adjustment)
    source_->direction.z = (current_z_rotation + z_adjustment);
}
