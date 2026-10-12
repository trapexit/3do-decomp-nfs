#include "distance_sample.h"

#define DISTANCE_SAMPLE_STEP 200
#define DISTANCE_SAMPLE_CENTER_OFFSET 100

int32
get_distance_sample_value(const DistanceSampleTable *table_, int32 distance_)
{
  int32 index;
  int32 sample_remainder;

  sample_remainder = ((distance_ + DISTANCE_SAMPLE_CENTER_OFFSET) % DISTANCE_SAMPLE_STEP);
  index =
    ((distance_ - sample_remainder + DISTANCE_SAMPLE_CENTER_OFFSET - table_->samples[0].distance) /
     DISTANCE_SAMPLE_STEP);
  if(index <= 0)
    index = 0;
  else if(index >= table_->sample_count)
    index = table_->sample_count - 1;

  return table_->samples[index].value;
}
