#pragma once

#include "event.h"
#include "kernel.h"

typedef struct EventUtilityState
{
  int32                control_pad_count;
  int32                mouse_count;
  ControlPadEventData *control_pads;
  MouseEventData      *mice;
  Item                 event_port_item;
  Item                 message_item;
  Item                 reply_port_item;
  Task                *owner_task;
} EventUtilityState;

extern EventUtilityState    g_EVENT_UTILITY_STATE;
extern ConfigurationRequest g_EVENT_UTILITY_CONFIGURATION_REQUEST;
