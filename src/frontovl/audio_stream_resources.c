#include "audio_stream_resources.h"

#include "audio_sample.h"
#include "byte_range.h"
#include "disk_stream_veneers.h"
#include "frontend_audio_control.h"
#include "frontend_audio_state.h"
#include "frontend_sound.h"
#include "item_cleanup.h"
#include "runtime_memory.h"
#include "runtime_string.h"
#include "runtime_thread.h"
#include "sdk_veneers.h"

#include "audio.h"
#include "kernel.h"

#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

#define AUDIO_STREAM_RELEASED_STATE 15
#define AUDIO_STREAM_INITIALIZED_STATE 11
#define AUDIO_STREAM_STOPPED_STATE 12
#define AUDIO_STREAM_PLAYING_STATE 13
#define AUDIO_STREAM_SIGNAL_COUNT 5
#define AUDIO_STREAM_BUFFER_COUNT 2
#define AUDIO_STREAM_START_SIGNAL_INDEX 1
#define AUDIO_STREAM_STOP_SIGNAL_INDEX 2
#define AUDIO_STREAM_RAMP_SIGNAL_INDEX 3
#define AUDIO_STREAM_EXIT_SIGNAL_INDEX 4
#define MUSIC_RESTORE_DURATION 60
#define SPEECH_MUSIC_DUCK_AMPLITUDE 0x3FFF
#define FULL_AUDIO_AMPLITUDE 0x7FFF
#define AUDIO_STREAM_FILE_HEADER_SIZE 0x94
#define AUDIO_STREAM_ATTACHMENT_TAG_COUNT 2
#define AUDIO_STREAM_RAMP_INTERVAL_TICKS 10
#define AUDIO_STREAM_FADE_OUT_STEP_COUNT 48
#define AUDIO_STREAM_PATH_PREFIX_BYTE_COUNT 16
#define AUDIO_STREAM_PATH_VARIANT_INDEX 6
#define AUDIO_STREAM_VARIANT_A_LOOP_OFFSET 0x18F38
#define AUDIO_STREAM_VARIANT_B_LOOP_OFFSET 0x18EC4
#define AUDIO_STREAM_VARIANT_C_LOOP_OFFSET 0x31A90
#define AUDIO_STREAM_RESOURCE_BYTE_COUNT 0x100
#define AUDIO_STREAM_TASK_STACK_BYTE_COUNT 0x1388
#define FRONTEND_MUSIC_VOICE_INDEX 2
#define FRONTEND_SPEECH_VOICE_INDEX 1
#define SPEECH_TASK_PRIORITY_OFFSET 10
#define RUNTIME_ALLOCATION_FLAGS 0
#define AUDIO_STREAM_BUFFER_ALIGNMENT_MASK (~3)
#define AUDIO_STREAM_RESTORE_WAIT_TICKS 24
#define EXTRA_MUSIC_BUFFER_ALLOCATION_NAME "ExtraMusicArcM"
#define MUSIC_AUDIO_STREAM_RESOURCES ((AudioStreamResources *) g_FRONTEND_MUSIC_STATE)
#define SPEECH_AUDIO_STREAM_RESOURCES ((AudioStreamResources *) g_FRONTEND_SPEECH_STREAM)

typedef struct AudioStreamResources
{
  uint8   reserved_before_samples[0x64];
  Item    samples[2];
  Item    stream_samples[2];
  Item    sample_attachments[2];
  Item    stream_attachments[2];
  Item    buffer_cues[2];
  uint32  buffer_cue_signals[2];
  Item    completion_cue;
  uint32  completion_cue_signal;
  Item    instrument;
  Item    amplitude_knob;
  int32   current_amplitude;
  int32   amplitude_step;
  int32   target_amplitude;
  void   *read_buffers[2];
  void   *sample_buffers[2];
  uint32  read_byte_count;
  uint32  buffer_byte_count;
  uint32  task_signals[AUDIO_STREAM_SIGNAL_COUNT];
  uint8   reserved_before_state[4];
  int32   state;
  int32   loop_enabled;
  int32   loop_offset;
  int32   task_running;
  Stream *disk_stream;
  Item    task;
  int32   active_buffer;
  uint8   final_buffer_pending;
  uint8   expand_buffers_requested;
  uint8   restore_buffers_requested;
  uint8   expanded_buffers_active;
} AudioStreamResources;

static const char G_AUDIO_STREAM_PATH_PREFIX[AUDIO_STREAM_PATH_PREFIX_BYTE_COUNT] =
  "DriveData/aiff/";

COMPILE_TIME_SIZE_ASSERT(audio_stream_sample_offset,
                         offsetof(AudioStreamResources, samples) == 0x64);
COMPILE_TIME_SIZE_ASSERT(audio_stream_attachment_offset,
                         offsetof(AudioStreamResources, sample_attachments) == 0x74);
COMPILE_TIME_SIZE_ASSERT(audio_stream_stream_sample_offset,
                         offsetof(AudioStreamResources, stream_samples) == 0x6C);
COMPILE_TIME_SIZE_ASSERT(audio_stream_stream_attachment_offset,
                         offsetof(AudioStreamResources, stream_attachments) == 0x7C);
COMPILE_TIME_SIZE_ASSERT(audio_stream_buffer_cue_offset,
                         offsetof(AudioStreamResources, buffer_cues) == 0x84);
COMPILE_TIME_SIZE_ASSERT(audio_stream_completion_cue_offset,
                         offsetof(AudioStreamResources, completion_cue) == 0x94);
COMPILE_TIME_SIZE_ASSERT(audio_stream_instrument_offset,
                         offsetof(AudioStreamResources, instrument) == 0x9C);
COMPILE_TIME_SIZE_ASSERT(audio_stream_amplitude_knob_offset,
                         offsetof(AudioStreamResources, amplitude_knob) == 0xA0);
COMPILE_TIME_SIZE_ASSERT(audio_stream_current_amplitude_offset,
                         offsetof(AudioStreamResources, current_amplitude) == 0xA4);
COMPILE_TIME_SIZE_ASSERT(audio_stream_buffer_offset,
                         offsetof(AudioStreamResources, sample_buffers) == 0xB8);
COMPILE_TIME_SIZE_ASSERT(audio_stream_read_buffer_offset,
                         offsetof(AudioStreamResources, read_buffers) == 0xB0);
COMPILE_TIME_SIZE_ASSERT(audio_stream_read_byte_count_offset,
                         offsetof(AudioStreamResources, read_byte_count) == 0xC0);
COMPILE_TIME_SIZE_ASSERT(audio_stream_byte_count_offset,
                         offsetof(AudioStreamResources, buffer_byte_count) == 0xC4);
COMPILE_TIME_SIZE_ASSERT(audio_stream_task_signal_offset,
                         offsetof(AudioStreamResources, task_signals) == 0xC8);
COMPILE_TIME_SIZE_ASSERT(audio_stream_state_offset, offsetof(AudioStreamResources, state) == 0xE0);
COMPILE_TIME_SIZE_ASSERT(audio_stream_loop_enabled_offset,
                         offsetof(AudioStreamResources, loop_enabled) == 0xE4);
COMPILE_TIME_SIZE_ASSERT(audio_stream_task_running_offset,
                         offsetof(AudioStreamResources, task_running) == 0xEC);
COMPILE_TIME_SIZE_ASSERT(audio_stream_task_offset, offsetof(AudioStreamResources, task) == 0xF4);
COMPILE_TIME_SIZE_ASSERT(audio_stream_active_buffer_offset,
                         offsetof(AudioStreamResources, active_buffer) == 0xF8);
COMPILE_TIME_SIZE_ASSERT(audio_stream_size, sizeof(AudioStreamResources) == 0x100);

void
initialize_audio_stream_resources(void *stream_state_)
{
  AudioStreamResources *resources;
  int32                 signal_index;

  resources                            = (AudioStreamResources *) stream_state_;
  resources->state                     = AUDIO_STREAM_INITIALIZED_STATE;
  resources->final_buffer_pending      = 0;
  resources->sample_buffers[0]         = NULL;
  resources->active_buffer             = -1;
  resources->sample_buffers[1]         = NULL;
  resources->buffer_byte_count         = 0;
  resources->expand_buffers_requested  = 0;
  resources->restore_buffers_requested = 0;
  resources->expanded_buffers_active   = 0;

  for(signal_index = 0; signal_index < AUDIO_STREAM_SIGNAL_COUNT; signal_index++)
    {
      resources->task_signals[signal_index] = AllocSignal(0);
    }

  OpenAudioFolio();
  resources->completion_cue        = CreateSizedItem(MKNODEID(AUDIONODE, AUDIO_CUE_NODE), NULL, 0);
  resources->completion_cue_signal = GetCueSignal(resources->completion_cue);

  resources->sample_attachments[0] =
    AttachSample(resources->instrument, resources->samples[0], NULL);
  resources->sample_attachments[1] =
    AttachSample(resources->instrument, resources->samples[1], NULL);

  resources->buffer_cues[0]        = CreateSizedItem(MKNODEID(AUDIONODE, AUDIO_CUE_NODE), NULL, 0);
  resources->buffer_cues[1]        = CreateSizedItem(MKNODEID(AUDIONODE, AUDIO_CUE_NODE), NULL, 0);
  resources->buffer_cue_signals[0] = GetCueSignal(resources->buffer_cues[0]);
  resources->buffer_cue_signals[1] = GetCueSignal(resources->buffer_cues[1]);

  LinkAttachments(resources->sample_attachments[0], resources->sample_attachments[1]);
  LinkAttachments(resources->sample_attachments[1], resources->sample_attachments[0]);
  MonitorAttachment(resources->sample_attachments[0], resources->buffer_cues[0], CUE_AT_END);
  MonitorAttachment(resources->sample_attachments[1], resources->buffer_cues[1], CUE_AT_END);
}

void
release_audio_stream_resources(void *stream_state_)
{
  AudioStreamResources *resources;
  int32                 signal_index;

  resources = (AudioStreamResources *) stream_state_;

  if(resources->completion_cue > 0)
    {
      DeleteItem(resources->completion_cue);
    }
  if(resources->buffer_cues[0] > 0)
    {
      DeleteItem(resources->buffer_cues[0]);
    }
  if(resources->buffer_cues[1] > 0)
    {
      DeleteItem(resources->buffer_cues[1]);
    }
  if(resources->sample_attachments[0] > 0)
    {
      DetachSample(resources->sample_attachments[0]);
    }
  if(resources->sample_attachments[1] > 0)
    {
      DetachSample(resources->sample_attachments[1]);
    }

  for(signal_index = 0; signal_index < AUDIO_STREAM_SIGNAL_COUNT; signal_index++)
    {
      FreeSignal(resources->task_signals[signal_index]);
    }
  resources->state = AUDIO_STREAM_RELEASED_STATE;
}

void
stop_audio_stream_playback(void *stream_state_)
{
  AudioStreamResources *resources;

  resources = (AudioStreamResources *) stream_state_;
  if(resources->state == AUDIO_STREAM_PLAYING_STATE)
    {
      StopInstrument(resources->instrument, NULL);
      if(resources->disk_stream != NULL)
        {
          CLOSE_DISK_STREAM_VENEER(resources->disk_stream);
        }
    }

  resources->state = AUDIO_STREAM_STOPPED_STATE;
  if(stream_state_ == g_FRONTEND_SPEECH_STREAM)
    {
      ramp_frontend_music_volume(MUSIC_RESTORE_DURATION, FULL_AUDIO_AMPLITUDE);
    }
}

void
start_audio_stream_playback(void *stream_state_)
{
  AudioStreamResources *resources;
  TagArg                attachment_tags[AUDIO_STREAM_ATTACHMENT_TAG_COUNT];

  resources = (AudioStreamResources *) stream_state_;
  if(resources->state == AUDIO_STREAM_PLAYING_STATE)
    {
      stop_audio_stream_playback(stream_state_);
    }

  resources->disk_stream = open_runtime_disk_stream((char *) stream_state_, 0);
  if(resources->disk_stream == NULL)
    {
      goto stop_instrument;
    }

  if(seek_runtime_disk_stream(resources->disk_stream, AUDIO_STREAM_FILE_HEADER_SIZE, SEEK_SET) < 0)
    {
      goto close_stream;
    }
  if(read_runtime_disk_stream(resources->disk_stream,
                              (char *) resources->read_buffers[0],
                              resources->read_byte_count) == 0)
    {
      goto close_stream;
    }
  if(read_runtime_disk_stream(resources->disk_stream,
                              (char *) resources->read_buffers[1],
                              resources->read_byte_count) == 0)
    {
      goto close_stream;
    }
  if(StartInstrument(resources->instrument, NULL) != 0)
    {
      goto close_stream;
    }

  if((stream_state_ == g_FRONTEND_MUSIC_STATE) && (g_FRONTEND_PLAYBACK_ENABLED == 0))
    {
      pause_frontend_audio_stream();
    }

  attachment_tags[0].ta_Tag = AF_TAG_CLEAR_FLAGS;
  attachment_tags[0].ta_Arg = (void *) AF_ATTF_FATLADYSINGS;
  attachment_tags[1].ta_Tag = TAG_END;
  SetAudioItemInfo(resources->sample_attachments[0], attachment_tags);
  SetAudioItemInfo(resources->sample_attachments[1], attachment_tags);

  resources->state                = AUDIO_STREAM_PLAYING_STATE;
  resources->final_buffer_pending = 0;
  resources->active_buffer        = -1;
  return;

close_stream:
  CLOSE_DISK_STREAM_VENEER(resources->disk_stream);
stop_instrument:
  StopInstrument(resources->instrument, NULL);
}

int32
advance_audio_stream_volume_ramp_down(void *stream_state_)
{
  AudioStreamResources *resources;
  int32                 amplitude;

  resources = (AudioStreamResources *) stream_state_;
  amplitude = resources->current_amplitude - resources->amplitude_step;
  if(amplitude < resources->target_amplitude)
    {
      amplitude = resources->target_amplitude;
    }
  TweakRawKnob(resources->amplitude_knob, amplitude);
  resources->current_amplitude = amplitude;
  return amplitude;
}

int32
advance_audio_stream_volume_ramp_up(void *stream_state_)
{
  AudioStreamResources *resources;
  int32                 amplitude;

  resources = (AudioStreamResources *) stream_state_;
  amplitude = resources->current_amplitude - resources->amplitude_step;
  if(amplitude > resources->target_amplitude)
    {
      amplitude = resources->target_amplitude;
    }
  TweakRawKnob(resources->amplitude_knob, amplitude);
  resources->current_amplitude = amplitude;
  return amplitude;
}

void
service_audio_stream_volume_ramp(void *stream_state_)
{
  AudioStreamResources *resources;
  int32                 amplitude;
  uint32                next_update_time;

  resources = (AudioStreamResources *) stream_state_;
  if(resources->amplitude_step > 0)
    {
      amplitude = advance_audio_stream_volume_ramp_down(stream_state_);
    }
  else
    {
      amplitude = advance_audio_stream_volume_ramp_up(stream_state_);
    }
  if(amplitude == resources->target_amplitude)
    {
      return;
    }

  next_update_time = GetAudioTime() + AUDIO_STREAM_RAMP_INTERVAL_TICKS;
  SignalAtTime(resources->completion_cue, next_update_time);
}

int32
read_audio_stream_buffer(void *stream_state_, void *buffer_, int32 byte_count_)
{
  AudioStreamResources *resources;
  int32                 bytes_read;
  int32                 remaining_bytes;
  int32                 seek_offset;

  resources  = (AudioStreamResources *) stream_state_;
  bytes_read = read_runtime_disk_stream(resources->disk_stream, (char *) buffer_, byte_count_);
  if(bytes_read == byte_count_)
    {
      return byte_count_;
    }

  if(resources->loop_enabled == 0)
    {
      clear_byte_range((uint8 *) buffer_ + bytes_read, byte_count_ - bytes_read);
      return bytes_read;
    }

  seek_offset = resources->loop_offset;
  if(seek_offset == 0)
    {
      seek_offset = AUDIO_STREAM_FILE_HEADER_SIZE;
    }
  if(seek_runtime_disk_stream(resources->disk_stream, seek_offset, SEEK_SET) < 0)
    {
      return 0;
    }

  remaining_bytes = byte_count_ - bytes_read;
  if(read_runtime_disk_stream(resources->disk_stream,
                              (char *) buffer_ + bytes_read,
                              remaining_bytes) == remaining_bytes)
    {
      return byte_count_;
    }
  return 0;
}

void
mark_audio_stream_final_buffer(void *stream_state_, int32 buffer_index_)
{
  AudioStreamResources *resources;
  TagArg                attachment_tags[AUDIO_STREAM_ATTACHMENT_TAG_COUNT];

  resources                       = (AudioStreamResources *) stream_state_;
  resources->final_buffer_pending = 1;
  resources->active_buffer        = buffer_index_;

  attachment_tags[0].ta_Tag = AF_TAG_SET_FLAGS;
  attachment_tags[0].ta_Arg = (void *) AF_ATTF_FATLADYSINGS;
  attachment_tags[1].ta_Tag = TAG_END;
  SetAudioItemInfo(buffer_index_ == 0 ? resources->sample_attachments[0] :
                                        resources->sample_attachments[1],
                   attachment_tags);
}

int32
rebuild_audio_stream_attachment_chain(void *stream_state_, int32 current_buffer_index_)
{
  AudioStreamResources *resources;
  int32                 next_buffer_index;

  resources                           = (AudioStreamResources *) stream_state_;
  resources->expand_buffers_requested = 0;

  resources->stream_samples[0] =
    create_stereo_44100_sample(resources->sample_buffers[0], resources->buffer_byte_count);
  resources->stream_samples[1] =
    create_stereo_44100_sample(resources->sample_buffers[1], resources->buffer_byte_count);
  resources->stream_attachments[0] =
    AttachSample(resources->instrument, resources->stream_samples[0], NULL);
  resources->stream_attachments[1] =
    AttachSample(resources->instrument, resources->stream_samples[1], NULL);

  MonitorAttachment(resources->sample_attachments[0], 0, 0);
  MonitorAttachment(resources->sample_attachments[1], 0, 0);
  LinkAttachments(resources->sample_attachments[0], 0);
  LinkAttachments(resources->sample_attachments[1], 0);
  LinkAttachments(resources->sample_attachments[0], resources->stream_attachments[0]);
  LinkAttachments(resources->stream_attachments[0], resources->sample_attachments[1]);
  LinkAttachments(resources->sample_attachments[1], resources->stream_attachments[1]);
  LinkAttachments(resources->stream_attachments[1], resources->sample_attachments[0]);
  MonitorAttachment(resources->stream_attachments[0], resources->buffer_cues[0], CUE_AT_END);
  MonitorAttachment(resources->stream_attachments[1], resources->buffer_cues[1], CUE_AT_END);

  next_buffer_index = current_buffer_index_ + 1;
  if(next_buffer_index > 1)
    {
      next_buffer_index = 0;
    }
  return read_audio_stream_buffer(stream_state_,
                                  resources->sample_buffers[next_buffer_index],
                                  resources->buffer_byte_count);
}

void
release_music_stream_buffer_resources(void)
{
  if(MUSIC_AUDIO_STREAM_RESOURCES->expanded_buffers_active == 0)
    {
      return;
    }

  DetachSample(MUSIC_AUDIO_STREAM_RESOURCES->stream_attachments[0]);
  MUSIC_AUDIO_STREAM_RESOURCES->stream_attachments[0] = 0;
  DetachSample(MUSIC_AUDIO_STREAM_RESOURCES->stream_attachments[1]);
  MUSIC_AUDIO_STREAM_RESOURCES->stream_attachments[1] = 0;
  UnloadSample(MUSIC_AUDIO_STREAM_RESOURCES->stream_samples[0]);
  MUSIC_AUDIO_STREAM_RESOURCES->stream_samples[0] = 0;
  UnloadSample(MUSIC_AUDIO_STREAM_RESOURCES->stream_samples[1]);
  MUSIC_AUDIO_STREAM_RESOURCES->stream_samples[1] = 0;
  free_runtime_memory(MUSIC_AUDIO_STREAM_RESOURCES->sample_buffers[0]);
  free_runtime_memory(MUSIC_AUDIO_STREAM_RESOURCES->sample_buffers[1]);
}

void
restore_audio_stream_attachment_chain(void *stream_state_, int32 current_buffer_index_)
{
  AudioStreamResources *resources;
  int32                 next_buffer_index;

  resources                            = (AudioStreamResources *) stream_state_;
  resources->restore_buffers_requested = 0;

  MonitorAttachment(resources->stream_attachments[0], 0, 0);
  MonitorAttachment(resources->stream_attachments[1], 0, 0);
  LinkAttachments(resources->sample_attachments[0], 0);
  LinkAttachments(resources->sample_attachments[1], 0);
  DetachSample(resources->stream_attachments[0]);
  resources->stream_attachments[0] = 0;
  DetachSample(resources->stream_attachments[1]);
  resources->stream_attachments[1] = 0;
  UnloadSample(resources->stream_samples[0]);
  resources->stream_samples[0] = 0;
  UnloadSample(resources->stream_samples[1]);
  resources->stream_samples[1] = 0;

  LinkAttachments(resources->sample_attachments[0], resources->sample_attachments[1]);
  LinkAttachments(resources->sample_attachments[1], resources->sample_attachments[0]);
  MonitorAttachment(resources->sample_attachments[0], resources->buffer_cues[0], CUE_AT_END);
  MonitorAttachment(resources->sample_attachments[1], resources->buffer_cues[1], CUE_AT_END);

  next_buffer_index = current_buffer_index_ + 1;
  if(next_buffer_index > 1)
    {
      next_buffer_index = 0;
    }
  RUNTIME_MEMCPY_VENEER(resources->read_buffers[current_buffer_index_],
                        resources->sample_buffers[next_buffer_index],
                        resources->buffer_byte_count);
  free_runtime_memory(resources->sample_buffers[0]);
  free_runtime_memory(resources->sample_buffers[1]);
  resources->sample_buffers[0] = NULL;
  resources->sample_buffers[1] = NULL;

  read_audio_stream_buffer(stream_state_,
                           (uint8 *) resources->read_buffers[current_buffer_index_] +
                             resources->buffer_byte_count,
                           resources->read_byte_count - resources->buffer_byte_count);
  resources->buffer_byte_count = 0;
}

void
service_audio_stream_buffer(void *stream_state_, int32 buffer_index_)
{
  AudioStreamResources *resources;
  int32                 bytes_read;

  resources = (AudioStreamResources *) stream_state_;
  if(resources->final_buffer_pending != 0)
    {
      if(resources->active_buffer != buffer_index_)
        {
          clear_byte_range(resources->read_buffers[buffer_index_], resources->read_byte_count);
        }
      else
        {
          stop_audio_stream_playback(stream_state_);
        }
      return;
    }

  if(resources->expand_buffers_requested != 0)
    {
      rebuild_audio_stream_attachment_chain(stream_state_, buffer_index_);
      resources->expanded_buffers_active = 1;
    }
  if(resources->restore_buffers_requested != 0)
    {
      restore_audio_stream_attachment_chain(stream_state_, buffer_index_);
      resources->expanded_buffers_active = 0;
      return;
    }

  bytes_read = read_audio_stream_buffer(stream_state_,
                                        resources->read_buffers[buffer_index_],
                                        resources->read_byte_count);
  if(bytes_read < (int32) resources->read_byte_count)
    {
      mark_audio_stream_final_buffer(stream_state_, buffer_index_);
      return;
    }
  if(resources->expanded_buffers_active == 0)
    {
      return;
    }
  read_audio_stream_buffer(stream_state_,
                           resources->sample_buffers[buffer_index_],
                           resources->buffer_byte_count);
}

void
run_audio_stream_task(void *stream_state_)
{
  AudioStreamResources *resources;
  uint32                signal_mask;
  uint32                received_signals;
  int32                 signal_index;

  resources   = (AudioStreamResources *) stream_state_;
  signal_mask = 0;
  for(signal_index = 0; signal_index < AUDIO_STREAM_SIGNAL_COUNT; signal_index++)
    {
      signal_mask |= resources->task_signals[signal_index];
    }
  signal_mask |= resources->completion_cue_signal;
  signal_mask |= resources->buffer_cue_signals[0];
  signal_mask |= resources->buffer_cue_signals[1];

  resources->state        = AUDIO_STREAM_STOPPED_STATE;
  resources->task_running = 1;
  while(resources->task_running != 0)
    {
      received_signals = WaitSignal(signal_mask);
      if((received_signals & resources->completion_cue_signal) != 0)
        {
          service_audio_stream_volume_ramp(stream_state_);
          if(resources->current_amplitude == 0)
            {
              stop_audio_stream_playback(stream_state_);
            }
        }
      if((received_signals & resources->task_signals[AUDIO_STREAM_START_SIGNAL_INDEX]) != 0)
        {
          start_audio_stream_playback(stream_state_);
        }
      if((received_signals & resources->task_signals[AUDIO_STREAM_STOP_SIGNAL_INDEX]) != 0)
        {
          stop_audio_stream_playback(stream_state_);
        }
      if((received_signals & resources->buffer_cue_signals[0]) != 0)
        {
          service_audio_stream_buffer(stream_state_, 0);
        }
      if((received_signals & resources->buffer_cue_signals[1]) != 0)
        {
          service_audio_stream_buffer(stream_state_, 1);
        }
      if((received_signals & resources->task_signals[AUDIO_STREAM_RAMP_SIGNAL_INDEX]) != 0)
        {
          service_audio_stream_volume_ramp(stream_state_);
          if(resources->current_amplitude == 0)
            {
              stop_audio_stream_playback(stream_state_);
            }
        }
      if((received_signals & resources->task_signals[AUDIO_STREAM_EXIT_SIGNAL_INDEX]) != 0)
        {
          stop_audio_stream_playback(stream_state_);
          resources->task_running = 0;
        }
    }
}

void
run_music_stream_task(void)
{
  AudioStreamResources *resources;

  resources = MUSIC_AUDIO_STREAM_RESOURCES;
  if(resources == NULL)
    {
      return;
    }
  initialize_audio_stream_resources(resources);
  run_audio_stream_task(resources);
  release_audio_stream_resources(resources);
  release_music_stream_buffer_resources();
}

void
configure_frontend_audio_stream(const char *path_)
{
  int32 loop_offset;

  loop_offset = 0;
  RUNTIME_MEMCPY_VENEER(MUSIC_AUDIO_STREAM_RESOURCES,
                        G_AUDIO_STREAM_PATH_PREFIX,
                        AUDIO_STREAM_PATH_PREFIX_BYTE_COUNT);
  append_runtime_string((char *) MUSIC_AUDIO_STREAM_RESOURCES, path_);

  if(path_[AUDIO_STREAM_PATH_VARIANT_INDEX] == 'A')
    {
      loop_offset = AUDIO_STREAM_VARIANT_A_LOOP_OFFSET;
    }
  else if(path_[AUDIO_STREAM_PATH_VARIANT_INDEX] == 'B')
    {
      loop_offset = AUDIO_STREAM_VARIANT_B_LOOP_OFFSET;
    }
  else if(path_[AUDIO_STREAM_PATH_VARIANT_INDEX] == 'C')
    {
      loop_offset = AUDIO_STREAM_VARIANT_C_LOOP_OFFSET;
    }
  MUSIC_AUDIO_STREAM_RESOURCES->loop_enabled = 1;
  MUSIC_AUDIO_STREAM_RESOURCES->loop_offset  = loop_offset;

  while(MUSIC_AUDIO_STREAM_RESOURCES->state == AUDIO_STREAM_INITIALIZED_STATE)
    {
      synchronize_runtime_frame();
    }
  SendSignal(MUSIC_AUDIO_STREAM_RESOURCES->task,
             MUSIC_AUDIO_STREAM_RESOURCES->task_signals[AUDIO_STREAM_START_SIGNAL_INDEX]);
}

void
fade_out_frontend_audio_stream(void)
{
  MUSIC_AUDIO_STREAM_RESOURCES->target_amplitude = 0;
  MUSIC_AUDIO_STREAM_RESOURCES->amplitude_step =
    MUSIC_AUDIO_STREAM_RESOURCES->current_amplitude / AUDIO_STREAM_FADE_OUT_STEP_COUNT;
  SendSignal(MUSIC_AUDIO_STREAM_RESOURCES->task,
             MUSIC_AUDIO_STREAM_RESOURCES->task_signals[AUDIO_STREAM_RAMP_SIGNAL_INDEX]);
}

void
ramp_frontend_music_volume(int32 duration_, int32 target_amplitude_)
{
  AudioStreamResources *resources;
  int32                 ramp_step_count;

  ramp_step_count           = duration_ / AUDIO_STREAM_RAMP_INTERVAL_TICKS;
  resources                 = MUSIC_AUDIO_STREAM_RESOURCES;
  resources->amplitude_step = (resources->current_amplitude - target_amplitude_) / ramp_step_count;
  MUSIC_AUDIO_STREAM_RESOURCES->target_amplitude = target_amplitude_;
  SendSignal(MUSIC_AUDIO_STREAM_RESOURCES->task,
             MUSIC_AUDIO_STREAM_RESOURCES->task_signals[AUDIO_STREAM_RAMP_SIGNAL_INDEX]);
}

void
initialize_frontend_audio_stream(void)
{
  AudioStreamResources *resources;
  Item                  amplitude_knob;

  resources = (AudioStreamResources *) allocate_runtime_memory("Stream",
                                                               AUDIO_STREAM_RESOURCE_BYTE_COUNT,
                                                               RUNTIME_ALLOCATION_FLAGS);
  g_FRONTEND_MUSIC_STATE = (FrontendMusicState *) resources;
  if(resources == NULL)
    {
      return;
    }

  resources->state = AUDIO_STREAM_INITIALIZED_STATE;
  MUSIC_AUDIO_STREAM_RESOURCES->read_buffers[0] =
    allocate_runtime_memory("Stream_RAM",
                            g_FRONTEND_MUSIC_BUFFER_BYTE_COUNT / AUDIO_STREAM_BUFFER_COUNT,
                            RUNTIME_ALLOCATION_FLAGS);
  MUSIC_AUDIO_STREAM_RESOURCES->read_buffers[1] =
    allocate_runtime_memory("Stream_RAM",
                            g_FRONTEND_MUSIC_BUFFER_BYTE_COUNT / AUDIO_STREAM_BUFFER_COUNT,
                            RUNTIME_ALLOCATION_FLAGS);
  MUSIC_AUDIO_STREAM_RESOURCES->instrument =
    g_FRONTEND_SOUND_VOICES[FRONTEND_MUSIC_VOICE_INDEX].instrument;
  amplitude_knob = GrabKnob(MUSIC_AUDIO_STREAM_RESOURCES->instrument, "Amplitude");
  MUSIC_AUDIO_STREAM_RESOURCES->amplitude_knob = amplitude_knob;
  TweakKnob(amplitude_knob, FULL_AUDIO_AMPLITUDE);
  MUSIC_AUDIO_STREAM_RESOURCES->current_amplitude = FULL_AUDIO_AMPLITUDE;
  MUSIC_AUDIO_STREAM_RESOURCES->samples[0] =
    create_stereo_44100_sample(MUSIC_AUDIO_STREAM_RESOURCES->read_buffers[0],
                               g_FRONTEND_MUSIC_BUFFER_BYTE_COUNT / AUDIO_STREAM_BUFFER_COUNT);
  MUSIC_AUDIO_STREAM_RESOURCES->samples[1] =
    create_stereo_44100_sample(MUSIC_AUDIO_STREAM_RESOURCES->read_buffers[1],
                               g_FRONTEND_MUSIC_BUFFER_BYTE_COUNT / AUDIO_STREAM_BUFFER_COUNT);
  MUSIC_AUDIO_STREAM_RESOURCES->read_byte_count =
    g_FRONTEND_MUSIC_BUFFER_BYTE_COUNT / AUDIO_STREAM_BUFFER_COUNT;
  MUSIC_AUDIO_STREAM_RESOURCES->buffer_byte_count =
    g_FRONTEND_MUSIC_BUFFER_BYTE_COUNT / AUDIO_STREAM_BUFFER_COUNT;
  MUSIC_AUDIO_STREAM_RESOURCES->task =
    create_runtime_thread("Streaming",
                          KernelBase->kb_CurrentTask->t.n_Priority,
                          run_music_stream_task,
                          AUDIO_STREAM_TASK_STACK_BYTE_COUNT);
}

void
release_frontend_audio_stream_resources(void)
{
  if(MUSIC_AUDIO_STREAM_RESOURCES->amplitude_knob > 0)
    {
      delete_resource_item(MUSIC_AUDIO_STREAM_RESOURCES->amplitude_knob);
    }
  if(MUSIC_AUDIO_STREAM_RESOURCES->samples[0] > 0)
    {
      UnloadSample(MUSIC_AUDIO_STREAM_RESOURCES->samples[0]);
    }
  if(MUSIC_AUDIO_STREAM_RESOURCES->samples[1] > 0)
    {
      UnloadSample(MUSIC_AUDIO_STREAM_RESOURCES->samples[1]);
    }
  free_runtime_memory(MUSIC_AUDIO_STREAM_RESOURCES->read_buffers[0]);
  free_runtime_memory(MUSIC_AUDIO_STREAM_RESOURCES->read_buffers[1]);
  free_runtime_memory(MUSIC_AUDIO_STREAM_RESOURCES);
  g_FRONTEND_MUSIC_STATE = NULL;
}

void
stop_frontend_audio_stream(void)
{
  SendSignal(MUSIC_AUDIO_STREAM_RESOURCES->task,
             MUSIC_AUDIO_STREAM_RESOURCES->task_signals[AUDIO_STREAM_EXIT_SIGNAL_INDEX]);
  while(MUSIC_AUDIO_STREAM_RESOURCES->state != AUDIO_STREAM_RELEASED_STATE)
    {
      synchronize_runtime_frame();
    }
  release_frontend_audio_stream_resources();
}

void
run_speech_stream_task(void)
{
  AudioStreamResources *resources;

  resources = SPEECH_AUDIO_STREAM_RESOURCES;
  if(resources == NULL)
    {
      return;
    }
  initialize_audio_stream_resources(resources);
  run_audio_stream_task(resources);
  release_audio_stream_resources(resources);
}

void
configure_speech_stream(const char *path_)
{
  RUNTIME_MEMCPY_VENEER(SPEECH_AUDIO_STREAM_RESOURCES,
                        G_AUDIO_STREAM_PATH_PREFIX,
                        AUDIO_STREAM_PATH_PREFIX_BYTE_COUNT);
  append_runtime_string((char *) SPEECH_AUDIO_STREAM_RESOURCES, path_);
  SPEECH_AUDIO_STREAM_RESOURCES->loop_enabled = 0;

  while(SPEECH_AUDIO_STREAM_RESOURCES->state == AUDIO_STREAM_INITIALIZED_STATE)
    {
      synchronize_runtime_frame();
    }
  ramp_frontend_music_volume(MUSIC_RESTORE_DURATION, SPEECH_MUSIC_DUCK_AMPLITUDE);
  SendSignal(SPEECH_AUDIO_STREAM_RESOURCES->task,
             SPEECH_AUDIO_STREAM_RESOURCES->task_signals[AUDIO_STREAM_START_SIGNAL_INDEX]);
}

void
initialize_speech_stream_state(void)
{
  AudioStreamResources *resources;
  Item                  amplitude_knob;

  resources = (AudioStreamResources *) allocate_runtime_memory("Stream",
                                                               AUDIO_STREAM_RESOURCE_BYTE_COUNT,
                                                               RUNTIME_ALLOCATION_FLAGS);
  g_FRONTEND_SPEECH_STREAM = (SpeechStreamState *) resources;
  if(resources == NULL)
    {
      return;
    }

  resources->state = AUDIO_STREAM_INITIALIZED_STATE;
  SPEECH_AUDIO_STREAM_RESOURCES->read_buffers[0] =
    allocate_runtime_memory("Stream_RAM",
                            g_FRONTEND_SPEECH_BUFFER_BYTE_COUNT / AUDIO_STREAM_BUFFER_COUNT,
                            RUNTIME_ALLOCATION_FLAGS);
  SPEECH_AUDIO_STREAM_RESOURCES->read_buffers[1] =
    allocate_runtime_memory("Stream_RAM",
                            g_FRONTEND_SPEECH_BUFFER_BYTE_COUNT / AUDIO_STREAM_BUFFER_COUNT,
                            RUNTIME_ALLOCATION_FLAGS);
  SPEECH_AUDIO_STREAM_RESOURCES->instrument =
    g_FRONTEND_SOUND_VOICES[FRONTEND_SPEECH_VOICE_INDEX].instrument;
  amplitude_knob = GrabKnob(SPEECH_AUDIO_STREAM_RESOURCES->instrument, "Amplitude");
  SPEECH_AUDIO_STREAM_RESOURCES->amplitude_knob = amplitude_knob;
  TweakKnob(amplitude_knob, SPEECH_MUSIC_DUCK_AMPLITUDE);
  SPEECH_AUDIO_STREAM_RESOURCES->samples[0] =
    create_mono_22050_sample(SPEECH_AUDIO_STREAM_RESOURCES->read_buffers[0],
                             g_FRONTEND_SPEECH_BUFFER_BYTE_COUNT / AUDIO_STREAM_BUFFER_COUNT);
  SPEECH_AUDIO_STREAM_RESOURCES->samples[1] =
    create_mono_22050_sample(SPEECH_AUDIO_STREAM_RESOURCES->read_buffers[1],
                             g_FRONTEND_SPEECH_BUFFER_BYTE_COUNT / AUDIO_STREAM_BUFFER_COUNT);
  SPEECH_AUDIO_STREAM_RESOURCES->read_byte_count =
    g_FRONTEND_SPEECH_BUFFER_BYTE_COUNT / AUDIO_STREAM_BUFFER_COUNT;
  SPEECH_AUDIO_STREAM_RESOURCES->task =
    create_runtime_thread("SpeechStream",
                          KernelBase->kb_CurrentTask->t.n_Priority + SPEECH_TASK_PRIORITY_OFFSET,
                          run_speech_stream_task,
                          AUDIO_STREAM_TASK_STACK_BYTE_COUNT);
}

void
release_speech_stream_resources(void)
{
  if(SPEECH_AUDIO_STREAM_RESOURCES->amplitude_knob > 0)
    {
      delete_resource_item(SPEECH_AUDIO_STREAM_RESOURCES->amplitude_knob);
    }
  if(SPEECH_AUDIO_STREAM_RESOURCES->samples[0] > 0)
    {
      UnloadSample(SPEECH_AUDIO_STREAM_RESOURCES->samples[0]);
    }
  if(SPEECH_AUDIO_STREAM_RESOURCES->samples[1] > 0)
    {
      UnloadSample(SPEECH_AUDIO_STREAM_RESOURCES->samples[1]);
    }
  free_runtime_memory(SPEECH_AUDIO_STREAM_RESOURCES->read_buffers[0]);
  free_runtime_memory(SPEECH_AUDIO_STREAM_RESOURCES->read_buffers[1]);
  free_runtime_memory(SPEECH_AUDIO_STREAM_RESOURCES);
  g_FRONTEND_SPEECH_STREAM = NULL;
}

void
stop_speech_stream_state(void)
{
  SendSignal(SPEECH_AUDIO_STREAM_RESOURCES->task,
             SPEECH_AUDIO_STREAM_RESOURCES->task_signals[AUDIO_STREAM_EXIT_SIGNAL_INDEX]);
  while(SPEECH_AUDIO_STREAM_RESOURCES->state != AUDIO_STREAM_RELEASED_STATE)
    {
      synchronize_runtime_frame();
    }
  release_speech_stream_resources();
}

void
expand_frontend_music_stream_buffers(int32 combined_buffer_byte_count_)
{
  AudioStreamResources *resources;
  int32                 buffer_byte_count;

  resources = MUSIC_AUDIO_STREAM_RESOURCES;
  if(resources->expanded_buffers_active != 0 || resources->expand_buffers_requested != 0)
    {
      return;
    }

  buffer_byte_count =
    (combined_buffer_byte_count_ / AUDIO_STREAM_BUFFER_COUNT) & AUDIO_STREAM_BUFFER_ALIGNMENT_MASK;
  if(buffer_byte_count > (int32) resources->read_byte_count)
    {
      return;
    }

  resources->buffer_byte_count        = buffer_byte_count;
  resources->sample_buffers[0]        = allocate_runtime_memory(EXTRA_MUSIC_BUFFER_ALLOCATION_NAME,
                                                                buffer_byte_count,
                                                                RUNTIME_ALLOCATION_FLAGS);
  resources->sample_buffers[1]        = allocate_runtime_memory(EXTRA_MUSIC_BUFFER_ALLOCATION_NAME,
                                                                buffer_byte_count,
                                                                RUNTIME_ALLOCATION_FLAGS);
  resources->expand_buffers_requested = 1;
}

void
restore_frontend_music_stream_buffers(void)
{
  MUSIC_AUDIO_STREAM_RESOURCES->restore_buffers_requested = 1;
  while(MUSIC_AUDIO_STREAM_RESOURCES->expanded_buffers_active != 0)
    {
      SleepAudioTicks(AUDIO_STREAM_RESTORE_WAIT_TICKS);
    }
}
