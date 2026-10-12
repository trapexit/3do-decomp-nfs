#pragma once

#include "types.h"

#define FRONTEND_TWEEN_SLOT_COUNT 20

typedef void (*FrontendTweenCallback)(int32 horizontal_position_,
                                      int32 vertical_position_,
                                      void *first_argument_,
                                      void *second_argument_);

typedef struct FrontendTweenRegistryControl
{
  uint32 active_slot_mask;
  uint32 allocated_slot_mask;
} FrontendTweenRegistryControl;

extern FrontendTweenRegistryControl g_FRONTEND_TWEEN_REGISTRY_CONTROL;
extern int32                        g_FRONTEND_TWEEN_FRAMES_REMAINING[FRONTEND_TWEEN_SLOT_COUNT];
extern int32 g_FRONTEND_TWEEN_HORIZONTAL_POSITIONS[FRONTEND_TWEEN_SLOT_COUNT];
extern int32 g_FRONTEND_TWEEN_VERTICAL_POSITIONS[FRONTEND_TWEEN_SLOT_COUNT];
extern int32 g_FRONTEND_TWEEN_TARGET_HORIZONTAL_POSITIONS[FRONTEND_TWEEN_SLOT_COUNT];
extern int32 g_FRONTEND_TWEEN_TARGET_VERTICAL_POSITIONS[FRONTEND_TWEEN_SLOT_COUNT];
extern void *g_FRONTEND_TWEEN_FIRST_CALLBACK_ARGUMENTS[FRONTEND_TWEEN_SLOT_COUNT];
extern void *g_FRONTEND_TWEEN_SECOND_CALLBACK_ARGUMENTS[FRONTEND_TWEEN_SLOT_COUNT];
extern FrontendTweenCallback g_FRONTEND_TWEEN_CALLBACKS[FRONTEND_TWEEN_SLOT_COUNT];
extern uint32                g_FRONTEND_TWEEN_PREVIOUS_TICK;

void
reset_frontend_tween_registry(void);
int32
register_frontend_tween(int32                 horizontal_position_,
                        int32                 target_horizontal_position_,
                        int32                 vertical_position_,
                        int32                 target_vertical_position_,
                        int32                 duration_,
                        void                 *first_callback_argument_,
                        void                 *second_callback_argument_,
                        FrontendTweenCallback callback_);
void
release_frontend_tween(int32 slot_);
void
complete_frontend_tweens(uint32 slot_mask_);
int32
update_frontend_tweens(void);
