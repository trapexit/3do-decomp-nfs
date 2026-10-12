#include "pause_game_shape.h"

#include "memory.h"
#include "shape_file.h"

static const char G_PAUSE_GAME_SHAPE_PATH[] = "frontend/display/pausegame.3sh";

void *
load_pause_game_shape(void)
{
  if(G_PAUSE_GAME_SHAPE == NULL)
    {
      G_PAUSE_GAME_SHAPE = load_shape_file_data_with_error(G_PAUSE_GAME_SHAPE_PATH, 0);
    }

  return G_PAUSE_GAME_SHAPE;
}

void
unload_pause_game_shape(void)
{
  release_frontend_input_buffer();
  free_memory(G_PAUSE_GAME_SHAPE);
  G_PAUSE_GAME_SHAPE = NULL;
}
