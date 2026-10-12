/* Private state layout used by the historical Input library EventUtility. */

#include "event_utility_state.h"

EventUtilityState    g_EVENT_UTILITY_STATE = { 0 };
ConfigurationRequest g_EVENT_UTILITY_CONFIGURATION_REQUEST;

typedef char EventUtilityStateSize[sizeof(g_EVENT_UTILITY_STATE) == 32 ? 1 : -1];
typedef char
  EventUtilityConfigurationRequestSize[sizeof(g_EVENT_UTILITY_CONFIGURATION_REQUEST) == 108 ? 1 :
                                                                                              -1];
