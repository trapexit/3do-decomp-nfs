#include "tire_smoke.h"

#include "ccb_manager.h"
#include "cel_access.h"
#include "pointer_array.h"
#include "render_bounds.h"
#include "road_render.h"
#include "simulation_common_art.h"
#include "simulation_snapshot.h"

#include "operamath.h"

#include "stddef.h"

#define TIRE_SMOKE_CAMERA_SHIFT_DEFAULT 10
#define TIRE_SMOKE_MINIMUM_DEPTH 1
#define TIRE_SMOKE_MAXIMUM_DEPTH 0x00007FFF
#define TIRE_SMOKE_VISIBILITY_DEPTH 0x00004CCC
#define TIRE_SMOKE_FIXED16_BITS 16
#define TIRE_SMOKE_LIFETIME_FIXED_ONE 0x00010000
#define TIRE_SMOKE_LIFETIME_SHIFT 8
#define TIRE_SMOKE_PROJECTION_SCALE_SHIFT 9
#define TIRE_SMOKE_SIZE_CURVE_BASE 0x00010CCC
#define TIRE_SMOKE_SIZE_CURVE_SHIFT 8
#define TIRE_SMOKE_POSITION_SCALE (-0x00000400)
#define TIRE_SMOKE_HORIZONTAL_SCALE_SHIFT 15
#define TIRE_SMOKE_VERTICAL_SCALE_SHIFT 11
#define TIRE_SMOKE_SHIFT_MASK 0x000000FF
#define TIRE_SMOKE_PIXEL_CONTROL 0x1F801F80
#define TIRE_SMOKE_INACTIVE_LIFETIME (-0x00010000)
#define TIRE_SMOKE_COMMON_ART_CEL_INDEX 4
#define TIRE_SMOKE_CCB_FLAGS_CLEAR_MASK 0xE0000000
#define TIRE_SMOKE_CCB_PIXC_CLEAR_MASK 0x00001C00
#define TIRE_SMOKE_RANDOM_MULTIPLIER 0x10004005U
#define TIRE_SMOKE_RANDOM_INCREMENT 0x00000011U
#define TIRE_SMOKE_RANDOM_POSITION_SHIFT 15
#define TIRE_SMOKE_RANDOM_SCALE_SHIFT 8
#define TIRE_SMOKE_RANDOM_POSITION_SCALE_SHIFT 7
#define TIRE_SMOKE_RANDOM_DRIFT_SHIFT 7
#define TIRE_SMOKE_RANDOM_INTERVAL_SHIFT 23
#define TIRE_SMOKE_RANDOM_INTERVAL_SCALE_SHIFT 15
#define TIRE_SMOKE_AGE_RATE 15
#define TIRE_SMOKE_RISE_PHASE_END 0x00004000
#define TIRE_SMOKE_RISE_PHASE_SHIFT 2
#define TIRE_SMOKE_RISE_SCALE_SHIFT 8
#define TIRE_SMOKE_RISE_POSITION_SHIFT 8
#define TIRE_SMOKE_DRIFT_POSITION_SHIFT 8
#define TIRE_SMOKE_SWIRL_ANGLE_SHIFT 9
#define TIRE_SMOKE_SWIRL_TIME_SHIFT 9
#define TIRE_SMOKE_SWIRL_COMPONENT_SHIFT 8

typedef struct TireSmokeProjectionWorkspace
{
  Point2  projected[TIRE_SMOKE_PARTICLE_COUNT];
  Vector3 transformed[TIRE_SMOKE_PARTICLE_COUNT];
} TireSmokeProjectionWorkspace;

typedef char SizeAssertTireSmokeRenderStateSize[(sizeof(TireSmokeRenderState) == 0x0C) ? 1 : -1];
typedef char
  SizeAssertTireSmokeParticleStateSize[(sizeof(TireSmokeParticleState) == 0x96C) ? 1 : -1];
typedef char
  SizeAssertTireSmokeLifetimesOffset[(offsetof(TireSmokeParticleState, lifetimes) == 0x4B0) ? 1 :
                                                                                              -1];
typedef char SizeAssertTireSmokeDepthOrderOffset
  [(offsetof(TireSmokeParticleState, depth_order) == 0x640) ? 1 : -1];
typedef char SizeAssertTireSmokeDriftOffsetsOffset
  [(offsetof(TireSmokeParticleState, drift_offsets) == 0x7D0) ? 1 : -1];
typedef char SizeAssertTireSmokeNextParticleOffset
  [(offsetof(TireSmokeParticleState, next_particle_index) == 0x960) ? 1 : -1];
typedef char
  SizeAssertTireSmokeProjectionWorkspaceSize[(sizeof(TireSmokeProjectionWorkspace) == 0x7D0) ? 1 :
                                                                                               -1];

const char  G_TIRE_SMOKE_CEL_PATH_1[TIRE_SMOKE_CEL_PATH_SIZE] = "DriveData/DriveArt/tiresmoke1.cel";
const char  G_TIRE_SMOKE_CEL_PATH_2[TIRE_SMOKE_CEL_PATH_SIZE] = "DriveData/DriveArt/tiresmoke2.cel";
const char  G_TIRE_SMOKE_CEL_PATH_3[TIRE_SMOKE_CEL_PATH_SIZE] = "DriveData/DriveArt/tiresmoke3.cel";
const char  G_TIRE_SMOKE_CEL_PATH_4[TIRE_SMOKE_CEL_PATH_SIZE] = "DriveData/DriveArt/tiresmoke4.cel";
const char *g_TIRE_SMOKE_CEL_PATHS[TIRE_SMOKE_CEL_COUNT]      = { G_TIRE_SMOKE_CEL_PATH_1,
                                                                  G_TIRE_SMOKE_CEL_PATH_2,
                                                                  G_TIRE_SMOKE_CEL_PATH_3,
                                                                  G_TIRE_SMOKE_CEL_PATH_4 };
TireSmokeRenderState   g_TIRE_SMOKE_RENDER_STATE;
TireSmokeParticleState g_TIRE_SMOKE_PARTICLES;
int32                  g_TIRE_SMOKE_PROJECTION_SHIFT = TIRE_SMOKE_CAMERA_SHIFT_DEFAULT;

void
clear_tire_smoke_cel_cache(void)
{
  G_TIRE_SMOKE_CEL = NULL;
}

void
initialize_tire_smoke_particles(TireSmokeParticleState *particles_)
{
  int32 particle_index;

  if(G_TIRE_SMOKE_CEL == NULL)
    {
      G_TIRE_SMOKE_CEL =
        (CCB *) get_headered_pointer_array_element((void *const *) g_SIMULATION_COMMON_ART,
                                                   TIRE_SMOKE_COMMON_ART_CEL_INDEX);
      set_cel_dimension_log2(G_TIRE_SMOKE_CEL);
      G_TIRE_SMOKE_CEL->ccb_Flags &= ~TIRE_SMOKE_CCB_FLAGS_CLEAR_MASK;
      G_TIRE_SMOKE_CEL->ccb_PIXC &= ~TIRE_SMOKE_CCB_PIXC_CLEAR_MASK;
    }

  for(particle_index = 0; particle_index < TIRE_SMOKE_PARTICLE_COUNT; particle_index += 1)
    {
      particles_->lifetimes[particle_index]   = TIRE_SMOKE_INACTIVE_LIFETIME;
      particles_->depth_order[particle_index] = particle_index;
    }
}

static uint32
_advance_tire_smoke_random_state(void)
{
  g_TIRE_SMOKE_RENDER_STATE.random_state =
    g_TIRE_SMOKE_RENDER_STATE.random_state * TIRE_SMOKE_RANDOM_MULTIPLIER +
    TIRE_SMOKE_RANDOM_INCREMENT;
  return g_TIRE_SMOKE_RENDER_STATE.random_state;
}

void
emit_tire_smoke_particles(TireSmokeParticleState *particles_,
                          const SimulationSource *source_,
                          int32                   elapsed_time_)
{
  const MatrixControllerState *controller;
  Vector3                     *position;
  uint32                       random_value;
  int32                        particle_index;
  int32                        random_offset;

  if(g_TIRE_SMOKE_RENDER_STATE.active_render_count != 0)
    return;

  particles_->emission_accumulator += elapsed_time_;
  if(particles_->emission_accumulator <= particles_->emission_interval)
    return;

  controller = (const MatrixControllerState *) source_->refresh_state;
  do
    {
      particle_index = particles_->next_particle_index;
      random_value   = _advance_tire_smoke_random_state();
      particles_->drift_offsets[particle_index] =
        ((int32) (random_value >> TIRE_SMOKE_RANDOM_POSITION_SHIFT) >>
         TIRE_SMOKE_RANDOM_SCALE_SHIFT)
        << TIRE_SMOKE_RANDOM_DRIFT_SHIFT;

      position    = &particles_->positions[particle_index];
      *position   = controller->position;
      position->z = -position->z;

      random_value  = _advance_tire_smoke_random_state();
      random_offset = ((int32) (TIRE_SMOKE_LIFETIME_FIXED_ONE -
                                (random_value >> TIRE_SMOKE_RANDOM_POSITION_SHIFT)) >>
                       TIRE_SMOKE_RANDOM_SCALE_SHIFT)
                   << TIRE_SMOKE_RANDOM_POSITION_SCALE_SHIFT;
      position->x += random_offset;
      random_value  = _advance_tire_smoke_random_state();
      random_offset = ((int32) (TIRE_SMOKE_LIFETIME_FIXED_ONE -
                                (random_value >> TIRE_SMOKE_RANDOM_POSITION_SHIFT)) >>
                       TIRE_SMOKE_RANDOM_SCALE_SHIFT)
                   << TIRE_SMOKE_RANDOM_POSITION_SCALE_SHIFT;
      position->y += random_offset;
      random_value  = _advance_tire_smoke_random_state();
      random_offset = ((int32) (TIRE_SMOKE_LIFETIME_FIXED_ONE -
                                (random_value >> TIRE_SMOKE_RANDOM_POSITION_SHIFT)) >>
                       TIRE_SMOKE_RANDOM_SCALE_SHIFT)
                   << TIRE_SMOKE_RANDOM_POSITION_SCALE_SHIFT;
      position->z += random_offset;

      particles_->lifetimes[particle_index] = 0;
      particle_index++;
      if(particle_index == TIRE_SMOKE_PARTICLE_COUNT)
        particle_index = 0;
      particles_->next_particle_index = particle_index;
      particles_->emission_accumulator -= particles_->emission_interval;

      random_value                  = _advance_tire_smoke_random_state();
      particles_->emission_interval = (int32) ((random_value >> TIRE_SMOKE_RANDOM_INTERVAL_SHIFT)
                                               << TIRE_SMOKE_RANDOM_INTERVAL_SCALE_SHIFT) >>
                                      TIRE_SMOKE_RANDOM_SCALE_SHIFT;
    }
  while(particles_->emission_accumulator > particles_->emission_interval);
}

int32
update_tire_smoke_particles(TireSmokeParticleState *particles_, int32 elapsed_time_)
{
  Vector3 *position;
  int32    active;
  int32    age;
  int32    age_delta;
  int32    index;
  int32    phase;
  int32    time_scale;

  if(g_TIRE_SMOKE_RENDER_STATE.active_render_count != 0)
    return 1;

  active     = 0;
  time_scale = elapsed_time_ >> TIRE_SMOKE_LIFETIME_SHIFT;
  age_delta  = time_scale * TIRE_SMOKE_AGE_RATE;
  for(index = 0; index < TIRE_SMOKE_PARTICLE_COUNT; index++)
    {
      age = particles_->lifetimes[index];
      if(age < 0)
        continue;

      age += age_delta;
      particles_->lifetimes[index] = age;
      if(age >= TIRE_SMOKE_LIFETIME_FIXED_ONE)
        {
          particles_->lifetimes[index] = TIRE_SMOKE_INACTIVE_LIFETIME;
          continue;
        }

      active   = 1;
      position = &particles_->positions[index];
      if(age < TIRE_SMOKE_RISE_PHASE_END)
        {
          phase = (age << TIRE_SMOKE_RISE_PHASE_SHIFT) >> TIRE_SMOKE_RISE_SCALE_SHIFT;
          position->y += ((time_scale * ((phase * phase) >> TIRE_SMOKE_RISE_SCALE_SHIFT)) >>
                          TIRE_SMOKE_RISE_SCALE_SHIFT)
                      << TIRE_SMOKE_RISE_POSITION_SHIFT;
        }
      else
        {
          position->y += time_scale << TIRE_SMOKE_DRIFT_POSITION_SHIFT;
          position->x +=
            time_scale * (particles_->drift_offsets[index] >> TIRE_SMOKE_RISE_SCALE_SHIFT);
          position->x +=
            (elapsed_time_ >> TIRE_SMOKE_SWIRL_TIME_SHIFT) *
            (SinF16(age << TIRE_SMOKE_SWIRL_ANGLE_SHIFT) >> TIRE_SMOKE_SWIRL_COMPONENT_SHIFT);
          position->z +=
            (elapsed_time_ >> TIRE_SMOKE_SWIRL_TIME_SHIFT) *
            (CosF16(age << TIRE_SMOKE_SWIRL_ANGLE_SHIFT) >> TIRE_SMOKE_SWIRL_COMPONENT_SHIFT);
        }
    }

  return active;
}

/* The oracle keeps sorting and CCB emission in one render-active boundary. */
void
render_tire_smoke_particles(TireSmokeParticleState *particles_, const Point2 *screen_offset_)
{
  SimulationProjectionSource  *source;
  TireSmokeProjectionWorkspace workspace;
  const Matrix3x3             *view_matrix;
  const CCB                   *cel_template;
  RenderCcb                   *ccb;
  Vector3                      camera_translation;
  int32                        curve_scale;
  int32                        depth;
  int32                        fade;
  int32                        index;
  int32                        ordered_index;
  int32                        particle_index;
  int32                        projection_scale;
  int32                        size;
  int32                        swap_index;
  uint32                       shift;

  g_TIRE_SMOKE_RENDER_STATE.active_render_count++;
  source = g_FRONTEND_RENDER_STATE.projection.source;
  shift  = (uint32) g_TIRE_SMOKE_PROJECTION_SHIFT;
  camera_translation.x =
    ((int32) (0U - (uint32) source->projection_transform.camera_position.x) >> shift);
  camera_translation.y =
    ((int32) (0U - (uint32) source->projection_transform.camera_position.y) >> shift);
  camera_translation.z =
    ((int32) (0U - (uint32) source->projection_transform.camera_position.z) >> shift);
  translate_shifted_vector3_array(TIRE_SMOKE_PARTICLE_COUNT,
                                  particles_->positions,
                                  shift,
                                  &camera_translation,
                                  workspace.transformed);
  view_matrix = (const Matrix3x3 *) g_FRONTEND_RENDER_STATE.projection.view_state;
  MulManyVec3Mat33_F16((vec3f16 *) workspace.transformed,
                       (vec3f16 *) workspace.transformed,
                       *(mat33f16 *) view_matrix,
                       TIRE_SMOKE_PARTICLE_COUNT);

  for(index = 0; index < TIRE_SMOKE_PARTICLE_COUNT; index++)
    {
      if(workspace.transformed[index].z < TIRE_SMOKE_MINIMUM_DEPTH)
        workspace.transformed[index].z = TIRE_SMOKE_MINIMUM_DEPTH;
      else if(workspace.transformed[index].z > TIRE_SMOKE_MAXIMUM_DEPTH)
        workspace.transformed[index].z = TIRE_SMOKE_MAXIMUM_DEPTH;
    }

  for(index = 1; index < TIRE_SMOKE_PARTICLE_COUNT; index++)
    {
      ordered_index = index;
      while(ordered_index > 0)
        {
          particle_index = particles_->depth_order[ordered_index];
          swap_index     = particles_->depth_order[ordered_index - 1];
          if(workspace.transformed[particle_index].z >= workspace.transformed[swap_index].z)
            {
              break;
            }

          particles_->depth_order[ordered_index - 1] = particle_index;
          particles_->depth_order[ordered_index]     = swap_index;
          ordered_index--;
        }
    }

  project_road_vertex_batch(TIRE_SMOKE_PARTICLE_COUNT, workspace.transformed, workspace.projected);
  for(index = 0; index < TIRE_SMOKE_PARTICLE_COUNT; index++)
    {
      workspace.transformed[index].x = workspace.projected[index].x;
      workspace.transformed[index].y = workspace.projected[index].y;
    }

  cel_template = g_TIRE_SMOKE_RENDER_STATE.cel;
  for(index = 0; index < TIRE_SMOKE_PARTICLE_COUNT; index++)
    {
      particle_index = particles_->depth_order[index];
      depth          = workspace.transformed[particle_index].z;
      if((particles_->lifetimes[particle_index] >= 0) &&
         ((int32) ((uint32) TIRE_SMOKE_VISIBILITY_DEPTH >> shift) < depth))
        {
          ccb               = allocate_untransformed_ccb();
          ccb->fields.flags = cel_template->ccb_Flags;
          projection_scale  = DivSF16((int32) ((uint32) g_RENDER_BOUNDS.horizontal_projection_scale
                                               << (TIRE_SMOKE_FIXED16_BITS - shift)),
                                      depth);
          fade = ((TIRE_SMOKE_LIFETIME_FIXED_ONE - particles_->lifetimes[particle_index]) >>
                  TIRE_SMOKE_LIFETIME_SHIFT);
          curve_scale =
            (int32) ((uint32) TIRE_SMOKE_SIZE_CURVE_BASE - ((uint32) fade * (uint32) fade));
          curve_scale >>= TIRE_SMOKE_SIZE_CURVE_SHIFT;
          size = (int32) ((uint32) (projection_scale >> TIRE_SMOKE_PROJECTION_SCALE_SHIFT) *
                          (uint32) curve_scale);
          size >>= TIRE_SMOKE_SIZE_CURVE_SHIFT;

          ccb->fields.source_offset = (uint32) cel_template->ccb_SourcePtr;
          ccb->fields.palette       = (uint16 *) cel_template->ccb_PLUTPtr;
          ccb->fields.x_position = (int32) ((uint32) workspace.transformed[particle_index].x +
                                            ((uint32) size * (uint32) TIRE_SMOKE_POSITION_SCALE) +
                                            (uint32) screen_offset_->x);
          ccb->fields.y_position = (int32) ((uint32) workspace.transformed[particle_index].y +
                                            ((uint32) size * (uint32) TIRE_SMOKE_POSITION_SCALE) +
                                            (uint32) screen_offset_->y);
          ccb->fields.horizontal_dx =
            ((int32) ((uint32) size << TIRE_SMOKE_HORIZONTAL_SCALE_SHIFT) >>
             ((uint32) cel_template->ccb_Width & TIRE_SMOKE_SHIFT_MASK));
          ccb->fields.horizontal_dy = 0;
          ccb->fields.vertical_dx   = 0;
          ccb->fields.vertical_dy   = ((int32) ((uint32) size << TIRE_SMOKE_VERTICAL_SCALE_SHIFT) >>
                                       ((uint32) cel_template->ccb_Height & TIRE_SMOKE_SHIFT_MASK));
          ccb->fields.horizontal_ddx = 0;
          ccb->fields.horizontal_ddy = 0;
          ccb->fields.pixel_control  = TIRE_SMOKE_PIXEL_CONTROL;
        }
    }

  g_TIRE_SMOKE_RENDER_STATE.active_render_count--;
}
