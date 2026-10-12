#pragma once

#include "types.h"

#define TIMER_TASK_CALLBACK_COUNT 8

typedef void (*TimerTaskCallback)(void);

extern TimerTaskCallback g_TIMER_TASK_CALLBACKS[TIMER_TASK_CALLBACK_COUNT];

void
register_timer_task_callback(TimerTaskCallback callback_);
void
unregister_timer_task_callback(TimerTaskCallback callback_);
int32
count_active_timer_task_callbacks(void);
