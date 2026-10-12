#pragma once

#include "types.h"

extern char g_NVRAM_CONFIGURATION_PATH[16];
extern char g_NVRAM_HIGH_SCORES_PATH[16];
extern char g_NVRAM_INITIAL_HIGH_SCORES_PATH[16];

void
reset_frontend_configuration(void);
void
reset_frontend_high_scores(void);
int32
load_frontend_nvram(void);
