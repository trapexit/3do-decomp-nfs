#pragma once

#include "movie_playback.h"

#include "types.h"

int32
initialize_frontend_movie_stream(void);
int32
play_frontend_movie_request(const char *shape_name_,
                            int32       request_value_,
                            int32       reserved0_,
                            int32       reserved1_,
                            int32       reserved2_);
int32
play_frontend_movie_stream(const char *shape_name_, MovieFrameCallback frame_callback_);
int32
start_frontend_movie_stream(char *path_);
void
set_frontend_movie_mode(int32 mode_);
