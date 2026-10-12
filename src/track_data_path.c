#include "track_data_path.h"

#include "string_utilities.h"

void
set_track_data_path(const char *path_)
{
  copy_string(G_TRACK_DATA_PATH, path_);
}
