#pragma once

#include "kernel.h"
#include "types.h"

#define SHUTDOWN_CALLBACK_CAPACITY 32

typedef void (*ShutdownCallback)(void);

extern ShutdownCallback g_SHUTDOWN_CALLBACKS[SHUTDOWN_CALLBACK_CAPACITY];
extern int32            g_SHUTDOWN_MODE;

void
run_shutdown_callbacks(void);
void
register_shutdown_callback(ShutdownCallback callback_);
