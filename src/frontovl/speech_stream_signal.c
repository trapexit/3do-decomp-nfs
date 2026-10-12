#include "speech_stream_signal.h"

#include "frontend_audio_state.h"

#include "kernel.h"
#include "task.h"

int32
signal_speech_stream_stop(void)
{
  SpeechStreamState *stream;

  stream = g_FRONTEND_SPEECH_STREAM;
  return SendSignal(stream->task, stream->stop_signal);
}
