#pragma once

#include "types.h"

void
initialize_audio_stream_resources(void *stream_state_);
void
release_audio_stream_resources(void *stream_state_);
void
stop_audio_stream_playback(void *stream_state_);
void
start_audio_stream_playback(void *stream_state_);
int32
advance_audio_stream_volume_ramp_down(void *stream_state_);
int32
advance_audio_stream_volume_ramp_up(void *stream_state_);
void
service_audio_stream_volume_ramp(void *stream_state_);
int32
read_audio_stream_buffer(void *stream_state_, void *buffer_, int32 byte_count_);
void
mark_audio_stream_final_buffer(void *stream_state_, int32 buffer_index_);
int32
rebuild_audio_stream_attachment_chain(void *stream_state_, int32 current_buffer_index_);
void
release_music_stream_buffer_resources(void);
void
restore_audio_stream_attachment_chain(void *stream_state_, int32 current_buffer_index_);
void
service_audio_stream_buffer(void *stream_state_, int32 buffer_index_);
void
run_audio_stream_task(void *stream_state_);
void
run_music_stream_task(void);
void
configure_frontend_audio_stream(const char *path_);
void
fade_out_frontend_audio_stream(void);
void
ramp_frontend_music_volume(int32 duration_, int32 target_amplitude_);
void
initialize_frontend_audio_stream(void);
void
release_frontend_audio_stream_resources(void);
void
stop_frontend_audio_stream(void);
void
run_speech_stream_task(void);
void
configure_speech_stream(const char *path_);
void
initialize_speech_stream_state(void);
void
release_speech_stream_resources(void);
void
stop_speech_stream_state(void);
void
expand_frontend_music_stream_buffers(int32 combined_buffer_byte_count_);
void
restore_frontend_music_stream_buffers(void);
