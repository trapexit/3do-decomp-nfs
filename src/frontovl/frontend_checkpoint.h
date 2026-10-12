#pragma once

#include "types.h"

extern void *g_FRONTEND_CHECKPOINT_SHAPES;
extern int32 g_FRONTEND_CHECKPOINT_SELECTION_X[6];
extern int32 g_FRONTEND_CHECKPOINT_SELECTION_Y[6];

int32
draw_frontend_checkpoint_menu(int32 selection_index_);
int32
run_frontend_checkpoint_menu(int32 timeout_seconds_);
int32
run_ranked_checkpoint_flow(void);
void
select_ranked_checkpoint_commentary_cue(char *cue_code_);
