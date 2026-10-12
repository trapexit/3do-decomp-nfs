#include "game_resource_cleanup.h"

#include "memory.h"

#define GAME_RESOURCE_REPLAY_UPDATE_COUNT_INITIAL 1
#define GAME_RESOURCE_REPLAY_VIEW_CONTEXT_DEFAULT 4
#define GAME_RESOURCE_REPLAY_PAUSE_MODE_DEFAULT 2
#define GAME_RESOURCE_REPLAY_SEQUENCE_INDEX_INITIAL (-2)
#define GAME_RESOURCE_REPLAY_CONTROL_MODE_INITIAL 6

GameResourceState g_GAME_RESOURCE_STATE = { { 0 },
                                            0,
                                            { 0 },
                                            0,
                                            { 0 },
                                            GAME_RESOURCE_REPLAY_UPDATE_COUNT_INITIAL,
                                            0,
                                            0,
                                            0,
                                            1,
                                            GAME_RESOURCE_REPLAY_VIEW_CONTEXT_DEFAULT,
                                            GAME_RESOURCE_REPLAY_PAUSE_MODE_DEFAULT,
                                            0,
                                            { 0 },
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            GAME_RESOURCE_REPLAY_SEQUENCE_INDEX_INITIAL,
                                            0,
                                            0,
                                            NULL,
                                            0,
                                            0,
                                            NULL,
                                            NULL,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            GAME_RESOURCE_REPLAY_CONTROL_MODE_INITIAL,
                                            NULL,
                                            NULL,
                                            0,
                                            0,
                                            { 0 },
                                            { NULL },
                                            { 0 },
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0,
                                            0 };

#define COMPILE_TIME_SIZE_ASSERT(name, expression) typedef char name[(expression) ? 1 : -1]

COMPILE_TIME_SIZE_ASSERT(GameResourceStateSize, sizeof(g_GAME_RESOURCE_STATE) == 0x110);

void
release_game_resource_allocations(void)
{
  if(g_GAME_RESOURCE_STATE.control_value_history != NULL)
    free_memory(g_GAME_RESOURCE_STATE.control_value_history);
  if(g_GAME_RESOURCE_STATE.control_nibble_history != NULL)
    free_memory(g_GAME_RESOURCE_STATE.control_nibble_history);
  if(g_GAME_RESOURCE_STATE.allocation_third != NULL)
    free_memory(g_GAME_RESOURCE_STATE.allocation_third);
  if(g_GAME_RESOURCE_STATE.allocation_fourth != NULL)
    free_memory(g_GAME_RESOURCE_STATE.allocation_fourth);
  g_GAME_RESOURCE_STATE.driving_warning_art_family = NULL;
}
