#include "line_extension.h"

#define LINE_DIRECTION_COUNT 16
#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

enum
{
  LINE_DIRECTION_REVERSE_OFFSET = 8,
  LINE_DIRECTION_ROTATE_OFFSET  = 4,
  LINE_DIRECTION_STEEP_OFFSET   = 2
};

static const Point2 G_LINE_EXTENSION_DIRECTIONS[LINE_DIRECTION_COUNT] = {
  { 65536, 13107 }, { 58617, 29308 },   { 46340, 46340 },   { 29314, 58614 },
  { 0, 65536 },     { -29308, 58617 },  { -46340, 46340 },  { -58614, 29314 },
  { -65536, 0 },    { -58617, -29308 }, { -46340, -46340 }, { -29314, -58614 },
  { 0, -65536 },    { 29308, -58617 },  { 46340, -46340 },  { 58614, 0 }
};

COMPILE_TIME_SIZE_ASSERT(line_extension_directions_size,
                         sizeof(G_LINE_EXTENSION_DIRECTIONS) == 128);

void
extend_fixed16_line_segment(const Point2 *start_,
                            const Point2 *end_,
                            Point2       *extended_start_,
                            Point2       *extended_end_)
{
  int32         delta_x;
  int32         delta_y;
  int32         direction_index;
  int32         original_delta_x;
  const Point2 *direction;
  Point2        end;
  Point2        start;

  start.x         = start_->x;
  start.y         = start_->y;
  end.x           = end_->x;
  end.y           = end_->y;
  delta_x         = (int32) ((uint32) end.x - (uint32) start.x);
  delta_y         = (int32) ((uint32) end.y - (uint32) start.y);
  direction_index = 0;

  if(delta_y < 0)
    {
      direction_index += LINE_DIRECTION_REVERSE_OFFSET;
      delta_x = (int32) (0 - (uint32) delta_x);
      delta_y = (int32) (0 - (uint32) delta_y);
    }

  if(delta_x < 0)
    {
      direction_index += LINE_DIRECTION_ROTATE_OFFSET;
      original_delta_x = delta_x;
      delta_x          = delta_y;
      delta_y          = (int32) (0 - (uint32) original_delta_x);
    }

  if(delta_y < delta_x)
    {
      if(delta_x < (int32) ((uint32) delta_y << 1))
        direction_index++;
    }
  else
    {
      direction_index += LINE_DIRECTION_STEEP_OFFSET;
      if(delta_y >= (int32) ((uint32) delta_x << 1))
        direction_index++;
    }

  direction          = &G_LINE_EXTENSION_DIRECTIONS[direction_index];
  extended_start_->x = (int32) ((uint32) start.x - (uint32) direction->x);
  extended_start_->y = (int32) ((uint32) start.y - (uint32) direction->y);
  extended_end_->x   = (int32) ((uint32) end.x + (uint32) direction->x);
  extended_end_->y   = (int32) ((uint32) end.y + (uint32) direction->y);
}
