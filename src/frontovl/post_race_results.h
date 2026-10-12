#pragma once

#include "race_state.h"

void
copy_post_race_result_entry(PostRaceResultEntry *entries_,
                            int32                source_index_,
                            int32                destination_index_);
void
update_post_race_records(void *event_records_);
void
copy_post_race_history_entry(uint8 *records_, int32 source_index_, int32 destination_index_);
int32
calculate_race_average_speed(int32 elapsed_time_, int32 vehicle_index_, int32 track_index_);
int32
calculate_post_race_score(int32 starting_event_);
void
copy_post_race_time_entry(uint8 *records_, int32 source_index_, int32 destination_index_);
void
save_post_race_results(uint8 *records_, int32 starting_event_, int32 skip_best_records_);
