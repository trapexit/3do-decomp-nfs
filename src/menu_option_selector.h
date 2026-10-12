#pragma once

#include "types.h"

typedef struct MenuOptionSelector
{
  uint8 reserved[0x24];
  int32 selected_value;
  int32 selected_index;
  int32 options[1];
} MenuOptionSelector;

void
reset_menu_option_selector(MenuOptionSelector *selector_);
