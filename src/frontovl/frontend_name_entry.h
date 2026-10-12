#pragma once

#include "types.h"

int32
draw_frontend_name_entry(int32       uppercase_,
                         const char *text_,
                         int32       cursor_column_,
                         int32       cursor_row_,
                         int32       show_time_,
                         int32       show_speed_,
                         int32       show_score_);
int32
enter_frontend_name(char *destination_,
                    int32 maximum_length_,
                    int32 timeout_units_,
                    int32 show_time_,
                    int32 show_speed_,
                    int32 show_score_);
