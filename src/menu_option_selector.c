#include "menu_option_selector.h"

void
reset_menu_option_selector(MenuOptionSelector *selector_)
{
  selector_->selected_index = 0;
  selector_->selected_value = selector_->options[0];
}
