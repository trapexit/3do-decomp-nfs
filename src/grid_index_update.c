#include "grid_index_update.h"

#include "track_position.h"

#define GRID_INDEX_SCALE 4
#define GRID_INDEX_NEGATIVE_ROUNDING 3
#define GRID_INDEX_SHIFT 2

int32
update_grid_index(void *state_, int32 *index_, int32 argument2_, int32 argument3_)
{
  int32 scaled_index;
  int32 updated_index;

  (void) argument2_;
  (void) argument3_;
  scaled_index = *index_ * GRID_INDEX_SCALE;
  adjust_track_segment_index((const Vector3 *) state_, &scaled_index);
  if(scaled_index < 0)
    scaled_index += GRID_INDEX_NEGATIVE_ROUNDING;
  updated_index = scaled_index >> GRID_INDEX_SHIFT;

  if(*index_ == updated_index)
    {
      *index_ = updated_index;
      return 0;
    }

  *index_ = updated_index;
  return 1;
}
