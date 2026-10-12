#include "post_race_results.h"

#include "frontend_name_entry.h"
#include "frontend_runtime_state.h"
#include "post_race_legacy.h"
#include "runtime_string.h"
#include "sdk_veneers.h"

extern int32
__rt_sdiv(int32 dividend_, int32 divisor_);

#define POST_RACE_LEADERBOARD_ENTRY_COUNT 3
#define POST_RACE_LEADERBOARD_SHIFT_START 1
#define POST_RACE_TRACK_RECORD_BYTE_COUNT 0x1C0
#define POST_RACE_EVENT_RECORD_BYTE_COUNT 0x70
#define POST_RACE_PROFILE_DATA_OFFSET 4
#define POST_RACE_NAME_ENTRY_MAXIMUM_LENGTH 10
#define POST_RACE_NAME_ENTRY_DISPLAY_WIDTH 30
#define POST_RACE_EVENT_BEST_ENTRY_OFFSET 0x54
#define POST_RACE_AGGREGATE_NAME_OFFSET 0x1A8
#define POST_RACE_AGGREGATE_TRACK_OFFSET 0x1B2
#define POST_RACE_AGGREGATE_VEHICLE_OFFSET 0x1B3
#define POST_RACE_AGGREGATE_PRIMARY_OFFSET 0x1B4
#define POST_RACE_AGGREGATE_ALTERNATE_OFFSET 0x1B8
#define POST_RACE_AGGREGATE_SCORE_OFFSET 0x1BC
#define POST_RACE_EVENT_PROGRESS_OFFSET 0x0C
#define POST_RACE_TIMING_EVENT_BYTE_COUNT 0xB0
#define POST_RACE_TIMING_BEST_PROGRESS_OFFSET 0x220
#define POST_RACE_HISTORY_ENTRY_OFFSET 0x704
#define POST_RACE_HISTORY_ENTRY_COUNT 10
#define POST_RACE_TIME_ENTRY_OFFSET 0x150
#define POST_RACE_SAVE_EVENT_BEST_ENTRY_OFFSET 0x58
#define POST_RACE_SAVE_AGGREGATE_NAME_OFFSET 0x1A8
#define POST_RACE_SAVE_AGGREGATE_TRACK_OFFSET 0x1B6
#define POST_RACE_SAVE_AGGREGATE_VEHICLE_OFFSET 0x1B7
#define POST_RACE_SAVE_AGGREGATE_PRIMARY_OFFSET 0x1B8
#define POST_RACE_SAVE_AGGREGATE_ALTERNATE_OFFSET 0x1BC
#define POST_RACE_SAVE_AGGREGATE_SCORE_OFFSET 0x1C0
#define POST_RACE_HISTORY_SHIFT_START 8
#define POST_RACE_TIME_SHIFT_START 1
#define POST_RACE_TRACK_INDEX_OFFSET 14
#define POST_RACE_DISTANCE_TABLE_COLUMN_COUNT 9
#define POST_RACE_SPEED_TIME_SCALE 100
#define POST_RACE_DISABLED_RACE_MODE 8
#define POST_RACE_SPEED_BONUS_SCALE 125000
#define POST_RACE_OPPONENT_BONUS_LIMIT 10
#define POST_RACE_OPPONENT_BONUS_SCALE 750000
#define POST_RACE_PROGRESS_ROUNDING 0x8000
#define POST_RACE_PROGRESS_FRACTION_BITS 16
#define POST_RACE_PROGRESS_BONUS_LIMIT 12
#define POST_RACE_PROGRESS_BONUS_SCALE 1000
#define POST_RACE_OBJECTIVE_SCORE_SCALE 1000000
#define POST_RACE_TRACK_DISPLAY_NAME_COUNT 4
#define POST_RACE_TRACK_DISPLAY_NAME_BYTES 8
#define POST_RACE_VEHICLE_DISPLAY_NAME_COUNT 8
#define POST_RACE_VEHICLE_DISPLAY_NAME_BYTES 8
#define POST_RACE_CORVETTE_DISPLAY_NAME_BYTES 12
#define POST_RACE_SHORT_VEHICLE_DISPLAY_NAME_BYTES 4
#define POST_RACE_CURRENT_RESULT ((PostRaceCurrentResult *) g_POST_RACE_CURRENT_RESULT)
#define POST_RACE_PLAYER_NAME g_FRONTEND_PLAYER_NAME

typedef struct PostRaceCurrentResult
{
  int32 reserved;
  int32 primary_score;
  int32 reserved_before_tertiary_score;
  int32 tertiary_score;
  int32 secondary_score;
} PostRaceCurrentResult;

typedef struct PostRaceProgressState
{
  int32 bonuses[POST_RACE_PROGRESS_BONUS_LIMIT];
  int32 player_name_entered;
} PostRaceProgressState;

typedef struct PostRaceTrackDisplayNameStorage
{
  char alpine[POST_RACE_TRACK_DISPLAY_NAME_BYTES];
  char coastal[POST_RACE_TRACK_DISPLAY_NAME_BYTES];
  char city[POST_RACE_TRACK_DISPLAY_NAME_BYTES];
  char test[POST_RACE_TRACK_DISPLAY_NAME_BYTES];
} PostRaceTrackDisplayNameStorage;

typedef struct PostRaceVehicleDisplayNameStorage
{
  char supra[POST_RACE_VEHICLE_DISPLAY_NAME_BYTES];
  char diablo[POST_RACE_VEHICLE_DISPLAY_NAME_BYTES];
  char porsche[POST_RACE_VEHICLE_DISPLAY_NAME_BYTES];
  char corvette[POST_RACE_CORVETTE_DISPLAY_NAME_BYTES];
  char ferarri[POST_RACE_VEHICLE_DISPLAY_NAME_BYTES];
  char viper[POST_RACE_VEHICLE_DISPLAY_NAME_BYTES];
  char nsx[POST_RACE_SHORT_VEHICLE_DISPLAY_NAME_BYTES];
  char rx7[POST_RACE_SHORT_VEHICLE_DISPLAY_NAME_BYTES];
} PostRaceVehicleDisplayNameStorage;

static const PostRaceTrackDisplayNameStorage G_POST_RACE_TRACK_DISPLAY_NAME_STORAGE = { "Alpine",
                                                                                        "Coastal",
                                                                                        "City",
                                                                                        "Test" };

static const PostRaceVehicleDisplayNameStorage G_POST_RACE_VEHICLE_DISPLAY_NAME_STORAGE = {
  "Supra", "Diablo", "Porsche", "Corvette", "Ferarri", "Viper", "NSX", "RX7"
};

extern char *POST_RACE_PLAYER_NAME;
int32        g_POST_RACE_EVENT_SCORE_FACTORS[3]                       = { 3975, 4770, 7155 };
int32        g_POST_RACE_PLAYER_SCORE_FACTORS[4]                      = { 10, 9, 7, 1 };
int32        g_POST_RACE_TRACK_SCORE_FACTORS[4]                       = { 108, 109, 110, 10 };
int32 g_POST_RACE_TARGET_SPEEDS[4][POST_RACE_LEADERBOARD_ENTRY_COUNT] = { { 25200, 36000, 54000 },
                                                                          { 23400, 37800, 52200 },
                                                                          { 18000, 25200, 39600 },
                                                                          { 10800, 14400, 18000 } };
int32 g_POST_RACE_MINIMUM_SPEEDS[4][POST_RACE_LEADERBOARD_ENTRY_COUNT] = { { 10800, 21600, 39600 },
                                                                           { 10800, 25200, 39600 },
                                                                           { 7200, 14400, 28800 },
                                                                           { 0, 3600, 7200 } };
int32 g_POST_RACE_PROGRESS_TARGETS[4][POST_RACE_LEADERBOARD_ENTRY_COUNT] = { { 79, 79, 79 },
                                                                             { 79, 79, 79 },
                                                                             { 79, 79, 79 },
                                                                             { 35, 56, 79 } };
PostRaceProgressState g_POST_RACE_PROGRESS_STATE                         = {
  { 1, 49, 196, 441, 784, 1225, 1764, 2401, 3136, 3969, 4900, 5929 },
  0
};
const char *g_POST_RACE_TRACK_DISPLAY_NAMES[POST_RACE_TRACK_DISPLAY_NAME_COUNT] = {
  G_POST_RACE_TRACK_DISPLAY_NAME_STORAGE.alpine,
  G_POST_RACE_TRACK_DISPLAY_NAME_STORAGE.coastal,
  G_POST_RACE_TRACK_DISPLAY_NAME_STORAGE.city,
  G_POST_RACE_TRACK_DISPLAY_NAME_STORAGE.test
};
const char *g_POST_RACE_VEHICLE_DISPLAY_NAMES[POST_RACE_VEHICLE_DISPLAY_NAME_COUNT] = {
  G_POST_RACE_VEHICLE_DISPLAY_NAME_STORAGE.supra,
  G_POST_RACE_VEHICLE_DISPLAY_NAME_STORAGE.diablo,
  G_POST_RACE_VEHICLE_DISPLAY_NAME_STORAGE.porsche,
  G_POST_RACE_VEHICLE_DISPLAY_NAME_STORAGE.corvette,
  G_POST_RACE_VEHICLE_DISPLAY_NAME_STORAGE.ferarri,
  G_POST_RACE_VEHICLE_DISPLAY_NAME_STORAGE.viper,
  G_POST_RACE_VEHICLE_DISPLAY_NAME_STORAGE.nsx,
  G_POST_RACE_VEHICLE_DISPLAY_NAME_STORAGE.rx7
};
int32 g_POST_RACE_DISTANCE_TABLE[3][POST_RACE_DISTANCE_TABLE_COLUMN_COUNT] = {
  { 78, 87, 78, 81, 87, 81, 76, 76, 100 },
  { 77, 86, 77, 80, 86, 80, 75, 75, 100 },
  { 91, 100, 91, 94, 100, 94, 89, 89, 100 }
};

typedef char
  PostRaceEventScoreFactorsSizeCheck[sizeof(g_POST_RACE_EVENT_SCORE_FACTORS) == 12 ? 1 : -1];
typedef char
  PostRacePlayerScoreFactorsSizeCheck[sizeof(g_POST_RACE_PLAYER_SCORE_FACTORS) == 16 ? 1 : -1];
typedef char
  PostRaceTrackScoreFactorsSizeCheck[sizeof(g_POST_RACE_TRACK_SCORE_FACTORS) == 16 ? 1 : -1];
typedef char PostRaceTargetSpeedsSizeCheck[sizeof(g_POST_RACE_TARGET_SPEEDS) == 48 ? 1 : -1];
typedef char PostRaceMinimumSpeedsSizeCheck[sizeof(g_POST_RACE_MINIMUM_SPEEDS) == 48 ? 1 : -1];
typedef char PostRaceProgressTargetsSizeCheck[sizeof(g_POST_RACE_PROGRESS_TARGETS) == 48 ? 1 : -1];
typedef char PostRaceProgressStateSizeCheck[sizeof(g_POST_RACE_PROGRESS_STATE) == 52 ? 1 : -1];
typedef char PostRaceTrackDisplayNameStorageSizeCheck
  [sizeof(G_POST_RACE_TRACK_DISPLAY_NAME_STORAGE) == 32 ? 1 : -1];
typedef char PostRaceVehicleDisplayNameStorageSizeCheck
  [sizeof(G_POST_RACE_VEHICLE_DISPLAY_NAME_STORAGE) == 60 ? 1 : -1];
typedef char
  PostRaceTrackDisplayNamesSizeCheck[sizeof(g_POST_RACE_TRACK_DISPLAY_NAMES) == 16 ? 1 : -1];
typedef char
  PostRaceVehicleDisplayNamesSizeCheck[sizeof(g_POST_RACE_VEHICLE_DISPLAY_NAMES) == 32 ? 1 : -1];
typedef char PostRaceDistanceTableSizeCheck[sizeof(g_POST_RACE_DISTANCE_TABLE) == 108 ? 1 : -1];

void
copy_post_race_result_entry(PostRaceResultEntry *entries_,
                            int32                source_index_,
                            int32                destination_index_)
{
  PostRaceResultEntry *destination;
  PostRaceResultEntry *source;

  if(destination_index_ >= POST_RACE_LEADERBOARD_ENTRY_COUNT)
    {
      return;
    }

  source      = &entries_[source_index_];
  destination = &entries_[destination_index_];
  copy_null_terminated_string(destination->name, source->name);
  destination->vehicle_index   = source->vehicle_index;
  destination->score_divisor   = source->score_divisor;
  destination->secondary_score = source->secondary_score;
  destination->tertiary_score  = source->tertiary_score;
  destination->track_index     = source->track_index;
}

void
copy_post_race_history_entry(uint8 *records_, int32 source_index_, int32 destination_index_)
{
  PostRaceResultEntry *destination;
  PostRaceResultEntry *source;

  if(destination_index_ >= POST_RACE_HISTORY_ENTRY_COUNT)
    {
      return;
    }

  source      = (PostRaceResultEntry *) (records_ + POST_RACE_HISTORY_ENTRY_OFFSET +
                                         source_index_ * (int32) sizeof(PostRaceResultEntry));
  destination = (PostRaceResultEntry *) (records_ + POST_RACE_HISTORY_ENTRY_OFFSET +
                                         destination_index_ * (int32) sizeof(PostRaceResultEntry));
  copy_null_terminated_string(destination->name, source->name);
  destination->track_index     = source->track_index;
  destination->vehicle_index   = source->vehicle_index;
  destination->score_divisor   = source->score_divisor;
  destination->secondary_score = source->secondary_score;
  destination->tertiary_score  = source->tertiary_score;
  destination->class_index     = source->class_index;
}

void
copy_post_race_time_entry(uint8 *records_, int32 source_index_, int32 destination_index_)
{
  PostRaceResultEntry *destination;
  PostRaceResultEntry *source;

  if(destination_index_ >= POST_RACE_LEADERBOARD_ENTRY_COUNT)
    {
      return;
    }

  source      = (PostRaceResultEntry *) (records_ + POST_RACE_TIME_ENTRY_OFFSET +
                                         source_index_ * (int32) sizeof(PostRaceResultEntry));
  destination = (PostRaceResultEntry *) (records_ + POST_RACE_TIME_ENTRY_OFFSET +
                                         destination_index_ * (int32) sizeof(PostRaceResultEntry));
  copy_null_terminated_string(destination->name, source->name);
  destination->vehicle_index   = source->vehicle_index;
  destination->score_divisor   = source->score_divisor;
  destination->secondary_score = source->secondary_score;
  destination->tertiary_score  = source->tertiary_score;
  destination->track_index     = source->track_index;
}

int32
calculate_race_average_speed(int32 elapsed_time_, int32 vehicle_index_, int32 track_index_)
{
  int32 distance;

  distance = g_POST_RACE_DISTANCE_TABLE[track_index_][vehicle_index_];
  return __rt_sdiv(distance, elapsed_time_ * POST_RACE_SPEED_TIME_SCALE);
}

int32
calculate_post_race_score(int32 starting_event_)
{
  int32 average_speed;
  int32 base_score;
  int32 event_index;
  int32 objective_score;
  int32 opponent_bonus;
  int32 opponent_speed;
  int32 progress;
  int32 progress_bonus;
  int32 progress_difference;
  int32 progress_target;
  int32 speed_bonus;
  int32 speed_target;
  int32 track_index;

  base_score     = 0;
  speed_bonus    = 0;
  opponent_bonus = 0;
  progress_bonus = 0;
  track_index    = RACE_CONFIGURATION->skill_level;

  event_index = 0;
  while(event_index <= RACE_CONFIGURATION->event_index - starting_event_)
    {
      base_score += g_POST_RACE_EVENT_SCORE_FACTORS[event_index] *
                    g_POST_RACE_PLAYER_SCORE_FACTORS[RACE_CONFIGURATION->track_index] *
                    g_POST_RACE_TRACK_SCORE_FACTORS[track_index];
      ++event_index;
    }

  if(starting_event_ == 0)
    {
      average_speed = calculate_race_average_speed(RACE_TIMING_STATE->primary_time,
                                                   RACE_CONFIGURATION->player_vehicle_index,
                                                   track_index);
      speed_target =
        g_POST_RACE_TARGET_SPEEDS[RACE_CONFIGURATION->track_index][RACE_CONFIGURATION->event_index];
      if(average_speed < speed_target)
        {
          speed_bonus =
            ((speed_target - g_POST_RACE_MINIMUM_SPEEDS[RACE_CONFIGURATION->track_index]
                                                       [RACE_CONFIGURATION->event_index]) /
             ((speed_target - average_speed) * POST_RACE_SPEED_TIME_SCALE)) *
            POST_RACE_SPEED_BONUS_SCALE;
        }

      if(RACE_CONFIGURATION->opponent_vehicle_index != POST_RACE_DISABLED_RACE_MODE)
        {
          opponent_speed = calculate_race_average_speed(RACE_TIMING_STATE->secondary_time,
                                                        RACE_CONFIGURATION->opponent_vehicle_index,
                                                        track_index);
          if((uint32) RACE_TIMING_STATE->primary_time <
               (uint32) RACE_TIMING_STATE->secondary_time &&
             average_speed < opponent_speed)
            {
              opponent_bonus =
                (opponent_speed / ((opponent_speed - average_speed) * POST_RACE_SPEED_TIME_SCALE));
              if(opponent_bonus > POST_RACE_OPPONENT_BONUS_LIMIT)
                {
                  opponent_bonus = POST_RACE_OPPONENT_BONUS_LIMIT;
                }
              opponent_bonus *= POST_RACE_OPPONENT_BONUS_SCALE;
            }
        }
    }

  progress =
    (RACE_TIMING_STATE->progress + POST_RACE_PROGRESS_ROUNDING) >> POST_RACE_PROGRESS_FRACTION_BITS;
  progress_target =
    g_POST_RACE_PROGRESS_TARGETS[RACE_CONFIGURATION->track_index][RACE_CONFIGURATION->event_index];
  if(progress_target < progress)
    {
      progress_difference = progress - progress_target;
      if(progress_difference > POST_RACE_PROGRESS_BONUS_LIMIT)
        {
          progress_difference = POST_RACE_PROGRESS_BONUS_LIMIT;
        }
      progress_bonus =
        g_POST_RACE_PROGRESS_STATE.bonuses[progress_difference] * POST_RACE_PROGRESS_BONUS_SCALE;
    }

  objective_score = (RACE_TIMING_STATE->completed_objective_count +
                     RACE_TIMING_STATE->bonus_objective_count - RACE_TIMING_STATE->penalty_count) *
                    POST_RACE_OBJECTIVE_SCORE_SCALE;
  return base_score + speed_bonus + opponent_bonus + progress_bonus + objective_score;
}

void
save_post_race_results(uint8 *records_, int32 starting_event_, int32 skip_best_records_)
{
  PostRaceResultEntry   *best_event_entry;
  PostRaceResultEntry   *event_leaderboard;
  PostRaceResultEntry   *history_entry;
  PostRaceResultEntry   *time_entries;
  PostRaceResultEntry   *time_entry;
  PostRaceProgressState *progress_state;
  uint8                 *event_block;
  uint8                 *track_block;
  uint8                 *profile_data;
  uint8                 *timing_bytes;
  int32                  aggregate_changed;
  int32                  aggregate_progress;
  int32                  best_event_changed;
  int32                  best_progress;
  int32                  calculated_score;
  int32                  event_progress;
  int32                  history_changed;
  int32                  history_index;
  int32                  leaderboard_changed;
  int32                  leaderboard_index;
  int32                  record_changed;
  int32                  shift_index;
  int32                  time_changed;
  int32                  time_index;
  uint8                  track_index;

  track_block  = records_ + RACE_CONFIGURATION->track_index * POST_RACE_TRACK_RECORD_BYTE_COUNT;
  profile_data = track_block + POST_RACE_PROFILE_DATA_OFFSET;
  event_block  = track_block + RACE_CONFIGURATION->event_index * POST_RACE_EVENT_RECORD_BYTE_COUNT;
  event_leaderboard = (PostRaceResultEntry *) (event_block + POST_RACE_PROFILE_DATA_OFFSET);
  calculated_score  = calculate_post_race_score(starting_event_);
  track_index       = RACE_CONFIGURATION->skill_level;
  progress_state    = &g_POST_RACE_PROGRESS_STATE;
  progress_state->player_name_entered = POST_RACE_PLAYER_NAME[0] != 0;

  leaderboard_changed = 0;
  leaderboard_index   = 0;
  if(starting_event_ == 0)
    {
      leaderboard_index = POST_RACE_LEADERBOARD_ENTRY_COUNT - 1;
      while(leaderboard_index >= 0 && (event_leaderboard[leaderboard_index].score_divisor == 0 ||
                                       event_leaderboard[leaderboard_index].score_divisor >
                                         POST_RACE_CURRENT_RESULT->primary_score))
        {
          --leaderboard_index;
        }
      ++leaderboard_index;
      if(leaderboard_index < POST_RACE_LEADERBOARD_ENTRY_COUNT)
        {
          shift_index = POST_RACE_LEADERBOARD_SHIFT_START;
          while(shift_index >= leaderboard_index)
            {
              copy_post_race_result_entry(event_leaderboard, shift_index, shift_index + 1);
              --shift_index;
            }
          event_leaderboard[leaderboard_index].score_divisor =
            POST_RACE_CURRENT_RESULT->primary_score;
          event_leaderboard[leaderboard_index].secondary_score =
            POST_RACE_CURRENT_RESULT->secondary_score;
          event_leaderboard[leaderboard_index].tertiary_score =
            POST_RACE_CURRENT_RESULT->tertiary_score;
          event_leaderboard[leaderboard_index].vehicle_index =
            RACE_CONFIGURATION->player_vehicle_index;
          event_leaderboard[leaderboard_index].track_index = track_index;
          leaderboard_changed                              = 1;
        }
    }

  best_event_entry = (PostRaceResultEntry *) (event_block + POST_RACE_SAVE_EVENT_BEST_ENTRY_OFFSET);
  best_event_changed = skip_best_records_ == 0 &&
                       best_event_entry->tertiary_score < POST_RACE_CURRENT_RESULT->tertiary_score;
  if(best_event_changed != 0)
    {
      best_event_entry->score_divisor   = POST_RACE_CURRENT_RESULT->primary_score;
      best_event_entry->secondary_score = POST_RACE_CURRENT_RESULT->secondary_score;
      best_event_entry->tertiary_score  = POST_RACE_CURRENT_RESULT->tertiary_score;
      best_event_entry->vehicle_index   = RACE_CONFIGURATION->player_vehicle_index;
      best_event_entry->track_index     = track_index;
    }

  history_index = 0;
  while(history_index < POST_RACE_HISTORY_ENTRY_COUNT)
    {
      history_entry = (PostRaceResultEntry *) (records_ + POST_RACE_HISTORY_ENTRY_OFFSET +
                                               history_index * (int32) sizeof(PostRaceResultEntry));
      if(history_entry->score_divisor < calculated_score)
        {
          break;
        }
      ++history_index;
    }
  history_changed = history_index < POST_RACE_HISTORY_ENTRY_COUNT;
  if(history_changed != 0)
    {
      shift_index = POST_RACE_HISTORY_SHIFT_START;
      while(shift_index >= history_index)
        {
          copy_post_race_history_entry(records_, shift_index, shift_index + 1);
          --shift_index;
        }
      history_entry = (PostRaceResultEntry *) (records_ + POST_RACE_HISTORY_ENTRY_OFFSET +
                                               history_index * (int32) sizeof(PostRaceResultEntry));
      history_entry->class_index     = track_index;
      history_entry->track_index     = RACE_CONFIGURATION->track_index;
      history_entry->vehicle_index   = RACE_CONFIGURATION->player_vehicle_index;
      history_entry->score_divisor   = calculated_score;
      history_entry->secondary_score = RACE_TIMING_STATE->primary_time;
      history_entry->tertiary_score  = RACE_TIMING_STATE->alternate_time;
    }

  time_changed = 0;
  time_index   = 0;
  time_entries = (PostRaceResultEntry *) (profile_data + POST_RACE_TIME_ENTRY_OFFSET);
  if(starting_event_ == 0)
    {
      while(time_index < POST_RACE_LEADERBOARD_ENTRY_COUNT &&
            time_entries[time_index].score_divisor != 0 &&
            time_entries[time_index].score_divisor <= RACE_TIMING_STATE->primary_time)
        {
          ++time_index;
        }
      if(time_index < POST_RACE_LEADERBOARD_ENTRY_COUNT)
        {
          shift_index = POST_RACE_TIME_SHIFT_START;
          while(shift_index >= time_index)
            {
              copy_post_race_time_entry(profile_data, shift_index, shift_index + 1);
              --shift_index;
            }
          time_entry                  = &time_entries[time_index];
          time_entry->vehicle_index   = RACE_CONFIGURATION->player_vehicle_index;
          time_entry->track_index     = track_index;
          time_entry->score_divisor   = RACE_TIMING_STATE->primary_time;
          time_entry->secondary_score = RACE_TIMING_STATE->alternate_time;
          time_entry->tertiary_score  = RACE_TIMING_STATE->progress;
          time_changed                = 1;
        }
    }

  aggregate_changed = 0;
  if(skip_best_records_ == 0)
    {
      timing_bytes = (uint8 *) RACE_TIMING_STATE;
      event_progress =
        *(int32 *) (timing_bytes +
                    RACE_CONFIGURATION->event_index * POST_RACE_TIMING_EVENT_BYTE_COUNT +
                    POST_RACE_EVENT_PROGRESS_OFFSET);
      best_progress = *(int32 *) (timing_bytes + POST_RACE_TIMING_BEST_PROGRESS_OFFSET);
      if(event_progress < best_progress)
        {
          event_progress = best_progress;
        }
      *(int32 *) (timing_bytes + POST_RACE_TIMING_BEST_PROGRESS_OFFSET) = event_progress;
      aggregate_progress = *(int32 *) (track_block + POST_RACE_SAVE_AGGREGATE_SCORE_OFFSET);
      if(aggregate_progress < event_progress)
        {
          track_block[POST_RACE_SAVE_AGGREGATE_VEHICLE_OFFSET] =
            RACE_CONFIGURATION->player_vehicle_index;
          track_block[POST_RACE_SAVE_AGGREGATE_TRACK_OFFSET] = track_index;
          *(int32 *) (track_block + POST_RACE_SAVE_AGGREGATE_PRIMARY_OFFSET) =
            RACE_TIMING_STATE->primary_time;
          *(int32 *) (track_block + POST_RACE_SAVE_AGGREGATE_ALTERNATE_OFFSET) =
            RACE_TIMING_STATE->alternate_time;
          *(int32 *) (track_block + POST_RACE_SAVE_AGGREGATE_SCORE_OFFSET) =
            RACE_TIMING_STATE->progress;
          aggregate_changed = 1;
        }
    }

  record_changed = best_event_changed != 0 || aggregate_changed != 0;
  if(leaderboard_changed != 0 || best_event_changed != 0 || history_changed != 0 ||
     time_changed != 0 || aggregate_changed != 0)
    {
      if(progress_state->player_name_entered == 0)
        {
          POST_RACE_PLAYER_NAME[0] = 0;
          enter_frontend_name(POST_RACE_PLAYER_NAME,
                              POST_RACE_NAME_ENTRY_MAXIMUM_LENGTH,
                              POST_RACE_NAME_ENTRY_DISPLAY_WIDTH,
                              leaderboard_changed != 0 || time_changed != 0,
                              record_changed,
                              history_changed);
          progress_state->player_name_entered = 1;
        }
      if(leaderboard_changed != 0)
        {
          copy_null_terminated_string(event_leaderboard[leaderboard_index].name,
                                      POST_RACE_PLAYER_NAME);
        }
      if(best_event_changed != 0)
        {
          copy_null_terminated_string(best_event_entry->name, POST_RACE_PLAYER_NAME);
        }
      if(history_changed != 0)
        {
          copy_null_terminated_string(history_entry->name, POST_RACE_PLAYER_NAME);
        }
      if(time_changed != 0)
        {
          copy_null_terminated_string(time_entries[time_index].name, POST_RACE_PLAYER_NAME);
        }
      if(aggregate_changed != 0)
        {
          copy_null_terminated_string((char *) (track_block + POST_RACE_SAVE_AGGREGATE_NAME_OFFSET),
                                      POST_RACE_PLAYER_NAME);
        }
    }
}

void
update_post_race_records(void *event_records_)
{
  PostRaceResultEntry   *best_event_entry;
  PostRaceResultEntry   *leaderboard;
  PostRaceProgressState *progress_state;
  uint8                 *player_aggregate;
  uint8                 *timing_bytes;
  int32                  aggregate_changed;
  int32                  aggregate_progress;
  int32                  best_event_changed;
  int32                  best_progress;
  int32                  event_progress;
  int32                  insertion_index;
  int32                  leaderboard_changed;
  int32                  record_changed;
  int32                  shift_index;
  uint8                  track_index;

  leaderboard      = (PostRaceResultEntry *) event_records_;
  player_aggregate = RACE_PARTICIPANT_RECORDS +
                     RACE_CONFIGURATION->track_index * POST_RACE_TRACK_RECORD_BYTE_COUNT +
                     sizeof(int32);
  progress_state   = &g_POST_RACE_PROGRESS_STATE;
  progress_state->player_name_entered = POST_RACE_PLAYER_NAME[0] != 0;
  track_index = RACE_CONFIGURATION->skill_level;

  insertion_index = POST_RACE_LEADERBOARD_ENTRY_COUNT - 1;
  while(insertion_index >= 0 &&
        (leaderboard[insertion_index].score_divisor == 0 ||
         leaderboard[insertion_index].score_divisor > POST_RACE_CURRENT_RESULT->primary_score))
    {
      --insertion_index;
    }
  ++insertion_index;

  leaderboard_changed = 0;
  if(insertion_index < POST_RACE_LEADERBOARD_ENTRY_COUNT)
    {
      leaderboard_changed = 1;
      shift_index         = POST_RACE_LEADERBOARD_SHIFT_START;
      while(shift_index >= insertion_index)
        {
          copy_post_race_result_entry(leaderboard, shift_index, shift_index + 1);
          --shift_index;
        }
      leaderboard[insertion_index].score_divisor   = POST_RACE_CURRENT_RESULT->primary_score;
      leaderboard[insertion_index].secondary_score = POST_RACE_CURRENT_RESULT->secondary_score;
      leaderboard[insertion_index].tertiary_score  = POST_RACE_CURRENT_RESULT->tertiary_score;
      leaderboard[insertion_index].vehicle_index   = RACE_CONFIGURATION->player_vehicle_index;
      leaderboard[insertion_index].track_index     = track_index;
    }

  best_event_entry =
    (PostRaceResultEntry *) ((uint8 *) event_records_ + POST_RACE_EVENT_BEST_ENTRY_OFFSET);
  best_event_changed = best_event_entry->tertiary_score < POST_RACE_CURRENT_RESULT->tertiary_score;
  if(best_event_changed != 0)
    {
      best_event_entry->score_divisor   = POST_RACE_CURRENT_RESULT->primary_score;
      best_event_entry->secondary_score = POST_RACE_CURRENT_RESULT->secondary_score;
      best_event_entry->tertiary_score  = POST_RACE_CURRENT_RESULT->tertiary_score;
      best_event_entry->vehicle_index   = RACE_CONFIGURATION->player_vehicle_index;
      best_event_entry->track_index     = track_index;
    }

  timing_bytes = (uint8 *) RACE_TIMING_STATE;
  event_progress =
    *(int32 *) (timing_bytes + RACE_CONFIGURATION->event_index * POST_RACE_TIMING_EVENT_BYTE_COUNT +
                POST_RACE_EVENT_PROGRESS_OFFSET);
  best_progress = *(int32 *) (timing_bytes + POST_RACE_TIMING_BEST_PROGRESS_OFFSET);
  if(event_progress < best_progress)
    {
      event_progress = best_progress;
    }
  *(int32 *) (timing_bytes + POST_RACE_TIMING_BEST_PROGRESS_OFFSET) = event_progress;

  aggregate_progress = *(int32 *) (player_aggregate + POST_RACE_AGGREGATE_SCORE_OFFSET);
  aggregate_changed  = aggregate_progress < event_progress;
  if(aggregate_changed != 0)
    {
      player_aggregate[POST_RACE_AGGREGATE_VEHICLE_OFFSET] =
        RACE_CONFIGURATION->player_vehicle_index;
      player_aggregate[POST_RACE_AGGREGATE_TRACK_OFFSET] = track_index;
      *(int32 *) (player_aggregate + POST_RACE_AGGREGATE_PRIMARY_OFFSET) =
        RACE_TIMING_STATE->primary_time;
      *(int32 *) (player_aggregate + POST_RACE_AGGREGATE_ALTERNATE_OFFSET) =
        RACE_TIMING_STATE->alternate_time;
      *(int32 *) (player_aggregate + POST_RACE_AGGREGATE_SCORE_OFFSET) =
        RACE_TIMING_STATE->progress;
    }

  record_changed = best_event_changed != 0 || aggregate_changed != 0;
  if(leaderboard_changed != 0 || record_changed != 0)
    {
      if(progress_state->player_name_entered == 0)
        {
          POST_RACE_PLAYER_NAME[0] = 0;
          enter_frontend_name(POST_RACE_PLAYER_NAME,
                              POST_RACE_NAME_ENTRY_MAXIMUM_LENGTH,
                              POST_RACE_NAME_ENTRY_DISPLAY_WIDTH,
                              leaderboard_changed,
                              record_changed,
                              0);
          progress_state->player_name_entered = 1;
        }
      if(leaderboard_changed != 0)
        {
          copy_null_terminated_string(leaderboard[insertion_index].name, POST_RACE_PLAYER_NAME);
        }
      if(best_event_changed != 0)
        {
          copy_null_terminated_string(best_event_entry->name, POST_RACE_PLAYER_NAME);
        }
      if(aggregate_changed != 0)
        {
          copy_null_terminated_string((char *) (player_aggregate + POST_RACE_AGGREGATE_NAME_OFFSET),
                                      POST_RACE_PLAYER_NAME);
        }
    }
}
