#pragma once

#include "types.h"

struct KernelBase;

typedef struct FrontendOverlayConfiguration
{
  void              *display_buffer;
  struct KernelBase *kernel_base;
  uint32             flags;
  void              *text_render_state;
  uint8             *startup_configuration;
  void              *driver_skill_table;
  uint8             *drive_statistics;
  void              *control_pad_reader;
  void              *control_input_buttons;
  void              *joystick_calibration;
  uint32            *random_state;
  void              *shared_selection_state;
  int32             *active_player_index;
  void              *shared_control_mode;
  void              *vehicle_control_button_masks;
  int32             *shared_control_value_a;
  int32             *shared_control_value_b;
} FrontendOverlayConfiguration;

typedef char
  FrontendOverlayConfigurationSizeCheck[sizeof(FrontendOverlayConfiguration) == 0x44 ? 1 : -1];
