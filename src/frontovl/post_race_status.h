#pragma once

#include "types.h"

void
format_post_race_time(int32 frame_count_, char *destination_);
void
format_ranked_post_race_status(char *event_time_destination_,
                               char *overall_time_destination_,
                               char *per_frame_speed_destination_,
                               char *per_second_speed_destination_);
void
format_post_race_status(char *time_destination_,
                        char *per_frame_speed_destination_,
                        char *per_second_speed_destination_);
