#pragma once

#include "geometry.h"

#include "graphics.h"
#include "types.h"

struct SimulationSource;

#define TIRE_SMOKE_CEL_COUNT 4
#define TIRE_SMOKE_CEL_PATH_SIZE 36
#define TIRE_SMOKE_PARTICLE_COUNT 100

typedef struct TireSmokeRenderState
{
  CCB   *cel;
  uint32 random_state;
  int32  active_render_count;
} TireSmokeRenderState;

typedef struct TireSmokeParticleState
{
  Vector3 positions[TIRE_SMOKE_PARTICLE_COUNT];
  int32   lifetimes[TIRE_SMOKE_PARTICLE_COUNT];
  int32   depth_order[TIRE_SMOKE_PARTICLE_COUNT];
  int32   drift_offsets[TIRE_SMOKE_PARTICLE_COUNT];
  int32   next_particle_index;
  int32   emission_accumulator;
  int32   emission_interval;
} TireSmokeParticleState;

extern TireSmokeRenderState   g_TIRE_SMOKE_RENDER_STATE;
extern TireSmokeParticleState g_TIRE_SMOKE_PARTICLES;
extern int32                  g_TIRE_SMOKE_PROJECTION_SHIFT;
extern const char             G_TIRE_SMOKE_CEL_PATH_1[TIRE_SMOKE_CEL_PATH_SIZE];
extern const char             G_TIRE_SMOKE_CEL_PATH_2[TIRE_SMOKE_CEL_PATH_SIZE];
extern const char             G_TIRE_SMOKE_CEL_PATH_3[TIRE_SMOKE_CEL_PATH_SIZE];
extern const char             G_TIRE_SMOKE_CEL_PATH_4[TIRE_SMOKE_CEL_PATH_SIZE];
extern const char            *g_TIRE_SMOKE_CEL_PATHS[TIRE_SMOKE_CEL_COUNT];

#define G_TIRE_SMOKE_CEL (g_TIRE_SMOKE_RENDER_STATE.cel)

void
clear_tire_smoke_cel_cache(void);
void
initialize_tire_smoke_particles(TireSmokeParticleState *particles_);
void
emit_tire_smoke_particles(TireSmokeParticleState        *particles_,
                          const struct SimulationSource *source_,
                          int32                          elapsed_time_);
int32
update_tire_smoke_particles(TireSmokeParticleState *particles_, int32 elapsed_time_);
void
render_tire_smoke_particles(TireSmokeParticleState *particles_, const Point2 *screen_offset_);
