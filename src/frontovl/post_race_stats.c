#include "post_race_stats.h"

#include "controller_input.h"
#include "frontend_frame.h"
#include "frontend_sound.h"
#include "post_race_legacy.h"
#include "race_state.h"
#include "runtime_memory.h"
#include "sdk_veneers.h"
#include "tracked_memory.h"

#define POST_RACE_STATS_SHAPE_PATH "frontend/display/wall.3sh"
#define POST_RACE_STATS_BACKGROUND_SHAPE "bgnd"
#define POST_RACE_STATS_PRIMARY_FORMAT "%-10s %5d"
#define POST_RACE_STATS_DETAIL_FORMAT "%-6s %-7s %-6s"
#define POST_RACE_STATS_ENTRY_COUNT 10
#define POST_RACE_STATS_ENTRY_OFFSET 0x704
#define POST_RACE_STATS_SCORE_SCALE 1000
#define POST_RACE_STATS_FIRST_X 29
#define POST_RACE_STATS_SECOND_X 155
#define POST_RACE_STATS_FIRST_Y 44
#define POST_RACE_STATS_LINE_HEIGHT 12
#define POST_RACE_STATS_TEXT_BYTE_COUNT 80
#define POST_RACE_STATS_INPUT_FRAME_LIMIT 900
#define POST_RACE_STATS_CONFIRM_DELAY 120
#define POST_RACE_STATS_SHAPE_POSITION_WORD 3
#define POST_RACE_STATS_SHAPE_X_SHIFT 16

typedef struct PostRaceVehicleNameStorage
{
  char supra[8];
  char diablo[8];
  char porsche911[4];
  char corvette[8];
  char ferrari512tr[8];
  char viper[8];
  char nsx[4];
  char rx7[8];
  char clock[8];
} PostRaceVehicleNameStorage;

typedef struct PostRaceTrackNameStorage
{
  char names[4][8];
} PostRaceTrackNameStorage;

typedef struct PostRaceClassNameStorage
{
  char names[3][4];
} PostRaceClassNameStorage;

static const PostRaceVehicleNameStorage G_POST_RACE_VEHICLE_NAME_STORAGE = {
  "Supra", "Diablo", "911", "ZR-1", "512TR", "Viper", "NSX", "RX-7", "Clock"
};
static const PostRaceTrackNameStorage G_POST_RACE_TRACK_NAME_STORAGE = {
  { "Alpine", "Coastal", "City", "Test" }
};
static const PostRaceClassNameStorage G_POST_RACE_CLASS_NAME_STORAGE = { { "Nov", "Int", "Pro" } };

const char *g_POST_RACE_VEHICLE_NAMES[9] = {
  G_POST_RACE_VEHICLE_NAME_STORAGE.supra,        G_POST_RACE_VEHICLE_NAME_STORAGE.diablo,
  G_POST_RACE_VEHICLE_NAME_STORAGE.porsche911,   G_POST_RACE_VEHICLE_NAME_STORAGE.corvette,
  G_POST_RACE_VEHICLE_NAME_STORAGE.ferrari512tr, G_POST_RACE_VEHICLE_NAME_STORAGE.viper,
  G_POST_RACE_VEHICLE_NAME_STORAGE.nsx,          G_POST_RACE_VEHICLE_NAME_STORAGE.rx7,
  G_POST_RACE_VEHICLE_NAME_STORAGE.clock
};
const char *g_POST_RACE_TRACK_NAMES[4] = { G_POST_RACE_TRACK_NAME_STORAGE.names[0],
                                           G_POST_RACE_TRACK_NAME_STORAGE.names[1],
                                           G_POST_RACE_TRACK_NAME_STORAGE.names[2],
                                           G_POST_RACE_TRACK_NAME_STORAGE.names[3] };
const char *g_POST_RACE_CLASS_NAMES[3] = { G_POST_RACE_CLASS_NAME_STORAGE.names[0],
                                           G_POST_RACE_CLASS_NAME_STORAGE.names[1],
                                           G_POST_RACE_CLASS_NAME_STORAGE.names[2] };

typedef char PostRaceVehicleNamesSizeCheck[sizeof(g_POST_RACE_VEHICLE_NAMES) == 36 ? 1 : -1];
typedef char PostRaceTrackNamesSizeCheck[sizeof(g_POST_RACE_TRACK_NAMES) == 16 ? 1 : -1];
typedef char PostRaceClassNamesSizeCheck[sizeof(g_POST_RACE_CLASS_NAMES) == 12 ? 1 : -1];
typedef char
  PostRaceVehicleNameStorageSizeCheck[sizeof(G_POST_RACE_VEHICLE_NAME_STORAGE) == 64 ? 1 : -1];
typedef char
  PostRaceTrackNameStorageSizeCheck[sizeof(G_POST_RACE_TRACK_NAME_STORAGE) == 32 ? 1 : -1];
typedef char
  PostRaceClassNameStorageSizeCheck[sizeof(G_POST_RACE_CLASS_NAME_STORAGE) == 12 ? 1 : -1];

int32
display_post_race_stats(int32 timeout_mode_)
{
  char                 text[POST_RACE_STATS_TEXT_BYTE_COUNT];
  PostRaceResultEntry *entry;
  void                *background_shape;
  void                *display_shapes;
  int32                entry_index;
  int32                line_y;
  int32                input_result;

  initialize_frontend_display();
  display_shapes = load_runtime_file_data(POST_RACE_STATS_SHAPE_PATH, 0);
  track_primary_allocation(display_shapes);
  begin_runtime_frame();
  synchronize_runtime_frame();
  clear_runtime_display(0);
  finish_runtime_frame();

  background_shape =
    locate_runtime_shape(display_shapes, (const uint32 *) POST_RACE_STATS_BACKGROUND_SHAPE);
  blit_runtime_shape_at(background_shape,
                        ((int32 *) background_shape)[POST_RACE_STATS_SHAPE_POSITION_WORD] >>
                          POST_RACE_STATS_SHAPE_X_SHIFT,
                        (int16) ((int32 *) background_shape)[POST_RACE_STATS_SHAPE_POSITION_WORD]);

  entry_index = 0;
  do
    {
      entry = (PostRaceResultEntry *) (RACE_PARTICIPANT_RECORDS + POST_RACE_STATS_ENTRY_OFFSET +
                                       entry_index * (int32) sizeof(PostRaceResultEntry));
      if(entry->score_divisor == 0)
        {
          break;
        }
      runtime_format_string(text,
                            POST_RACE_STATS_PRIMARY_FORMAT,
                            entry->name,
                            entry->score_divisor / POST_RACE_STATS_SCORE_SCALE);
      line_y = POST_RACE_STATS_FIRST_Y + entry_index * POST_RACE_STATS_LINE_HEIGHT;
      draw_runtime_text(text, POST_RACE_STATS_FIRST_X, line_y);
      runtime_format_string(text,
                            POST_RACE_STATS_DETAIL_FORMAT,
                            g_POST_RACE_VEHICLE_NAMES[entry->vehicle_index],
                            g_POST_RACE_TRACK_NAMES[entry->track_index],
                            g_POST_RACE_CLASS_NAMES[entry->class_index]);
      draw_runtime_text(text, POST_RACE_STATS_SECOND_X, line_y);
      finish_runtime_frame();
      entry_index += 1;
    }
  while(entry_index < POST_RACE_STATS_ENTRY_COUNT);

  draw_frontend_stamp();
  finish_runtime_frame();
  present_runtime_frame();
  free_primary_memory(display_shapes);
  input_result = poll_controller_input(POST_RACE_STATS_INPUT_FRAME_LIMIT, 0, 0);
  if(input_result == 0)
    {
      run_frontend_credits(timeout_mode_);
    }
  else
    {
      play_confirmation_sound();
      SleepAudioTicks(POST_RACE_STATS_CONFIRM_DELAY);
    }
  return 1;
}
