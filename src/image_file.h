#pragma once

#include "form3do.h"

struct ScreenContext;

void *
load_image(const char *path_, uint8 *destination_, VdlChunk **vdl_, struct ScreenContext *screen_);
void *
load_image_default(const char *path_);
void
unload_image(void *image_);
void
unload_image_handle(void *image_);
