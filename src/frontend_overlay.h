#pragma once

#include "types.h"

typedef void (*FrontendOverlayEntry)(int32 mode_, void *interface_);

#define STARTUP_ARGUMENT_BUFFER_BYTES 32

extern int32       g_FRONTEND_OVERLAY_MODE;
extern int32       g_FRONTEND_OVERLAY_RELOAD_REQUESTED;
extern int32       g_FRONTEND_OVERLAY_BYPASS;
extern char        g_STARTUP_ARGUMENT_BUFFER[STARTUP_ARGUMENT_BUFFER_BYTES];
extern const char  G_STARTUP_VEHICLE_NAME_PREFIX_AL[4];
extern const char  G_STARTUP_VEHICLE_NAME_PREFIX_CL[4];
extern const char  G_STARTUP_VEHICLE_NAME_PREFIX_CY[4];
extern const char  G_STARTUP_VEHICLE_NAME_PREFIX_TEST[8];
extern const char *g_STARTUP_VEHICLE_NAME_PREFIXES[4];
extern int32       g_FRONTEND_OVERLAY_SHARED_SELECTION_STATE[3];
extern int32       g_FRONTEND_OVERLAY_SHARED_CONTROL_MODE;
extern int32       g_FRONTEND_OVERLAY_SHARED_CONTROL_VALUE_A;
extern int32       g_FRONTEND_OVERLAY_SHARED_CONTROL_VALUE_B;
extern int32       g_PLAYER_SKILLS_RESOURCE_REQUESTED;

FrontendOverlayEntry
execute_loaded_overlay_and_resolve_entry(void *image_);
void
run_frontend_overlay(int32 flags_);
