#include "game_bootstrap.h"

int32                g_ANTI_LOCK_BRAKES_ENABLED = 1;
int32                g_TRACTION_CONTROL_ENABLED = 1;
int32                g_STARTUP_DEBUG_MODE;
int32                g_STARTUP_BUTTON_MODE;
int32                g_STARTUP_EVENT_MODE;
int32                g_STARTUP_HELP_MODE;
int32                g_STARTUP_UNUSED_SIGNED_LIMITS[2] = { -256, -65536 };
char                *g_STARTUP_ARGUMENT;
uint8                g_STARTUP_CONFIGURATION_RESERVED_PREFIX[0x84];
StartupConfiguration g_STARTUP_CONFIGURATION;
uint8                g_STARTUP_RENDER_FLAG;

typedef char StartupConfigurationReservedPrefixSize
  [(sizeof(g_STARTUP_CONFIGURATION_RESERVED_PREFIX) == 0x84) ? 1 : -1];
typedef char StartupUnusedSignedLimitsSize[(sizeof(g_STARTUP_UNUSED_SIGNED_LIMITS) == 8) ? 1 : -1];
