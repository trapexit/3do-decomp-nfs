#pragma once

#include "types.h"

struct FrontendMenuItem;
struct SimulationProjectionDescriptor;
struct SimulationProjectionSource;

typedef struct SimulationProjectionState
{
  struct SimulationProjectionSource     *source;
  struct SimulationProjectionDescriptor *projection;
  void                                  *view_state;
} SimulationProjectionState;

typedef struct FrontendMenuState
{
  uint8                    reserved_00[4];
  struct FrontendMenuItem *current_item;
  uint8                    reserved_08[5];
  uint8                    backdrop_enabled;
  uint8                    reserved_0_e[6];
  uint8                    mode;
  uint8                    reserved_15[3];
  int32                    reset;
  int32                    saved_simulation_mode;
  uint8                    render_reset_pending;
  uint8                    active;
  uint8                    render_complete;
  uint8                    transition_active;
} FrontendMenuState;

typedef union FrontendRenderState
{
  FrontendMenuState         menu;
  SimulationProjectionState projection;
} FrontendRenderState;

extern FrontendRenderState g_FRONTEND_RENDER_STATE;
