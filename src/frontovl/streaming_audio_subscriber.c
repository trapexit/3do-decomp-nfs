#include "streaming_audio_subscriber.h"

#include "runtime_error.h"
#include "runtime_memory.h"
#include "sdk_veneers.h"
#include "stream_message_items.h"

#include "kernel.h"
#include "msgport.h"

MemoryPool *g_S_AUDIO_CONTEXT_POOL = NULL;

int32
close_streaming_audio_subscriber(void)
{
  delete_memory_pool(g_S_AUDIO_CONTEXT_POOL);
  g_S_AUDIO_CONTEXT_POOL = NULL;
  return 0;
}

int32
handle_streaming_audio_opening(void *context_, SubscriberMsgPtr message_)
{
  (void) context_;
  (void) message_;
  return 0;
}

int32
handle_streaming_audio_end_of_file(void *context_, SubscriberMsgPtr message_)
{
  (void) context_;
  (void) message_;
  return 0;
}

void
run_streaming_audio_service_thread(int32 argument_count_, StreamingAudioContext *context_)
{
  SubscriberMsgPtr message;
  uint32           awaited_signals;
  uint32           received_signals;
  int32            channel_number;
  int32            status;
  boolean          running;

  (void) argument_count_;

  status = initialize_streaming_audio_thread(context_);
  if(status != 0)
    {
      RUNTIME_EXIT_VENEER(0);
      return;
    }

  running = 1;
  while(running)
    {
      awaited_signals  = (context_->request_port_signal | context_->all_buffer_signals);
      received_signals = WaitSignal(awaited_signals);

      if((received_signals & context_->request_port_signal) != 0)
        {
          while(poll_for_stream_message(context_->request_port,
                                        NULL,
                                        NULL,
                                        (void **) &message,
                                        &status))
            {
              switch(message->whatToDo)
                {
                  case kStreamOpData:
                    status = process_streaming_audio_data(context_, message);
                    break;

                  case kStreamOpGetChan:
                    status = get_streaming_audio_channel_status(context_, message);
                    break;

                  case kStreamOpSetChan:
                    status = set_streaming_audio_channel_status(context_, message);
                    break;

                  case kStreamOpControl:
                    status = dispatch_streaming_audio_control(context_, message);
                    break;

                  case kStreamOpSync:
                    status = synchronize_streaming_audio_channels(context_, message);
                    break;

                  case kStreamOpOpening:
                    status = handle_streaming_audio_opening(context_, message);
                    break;

                  case kStreamOpClosing:
                    status  = close_streaming_audio_channels(context_, message);
                    running = 0;
                    break;

                  case kStreamOpStop:
                    status = stop_streaming_audio_channels(context_, message);
                    break;

                  case kStreamOpStart:
                    status = start_streaming_audio_channels(context_, message);
                    break;

                  case kStreamOpEOF:
                    status = handle_streaming_audio_end_of_file(context_, message);
                    break;

                  case kStreamOpAbort:
                    status  = abort_streaming_audio_channels(context_, message);
                    running = 0;
                    break;

                  default:
                    break;
                }

              if(message->whatToDo != kStreamOpData)
                {
                  status = ReplyMsg(message->msgItem, status, message, sizeof(SubscriberMsg));
                  if(status < 0)
                    {
                      runtime_debug_printf("Failure in %s: $%lx\n",
                                           "SAudioSubscriberThread() - ReplyMsg()",
                                           status);
                      print_runtime_error(status);
                      RUNTIME_EXIT_VENEER(0);
                      return;
                    }
                }
            }
        }

      if((received_signals & context_->all_buffer_signals) != 0)
        handle_completed_audio_buffers(context_, received_signals);
    }

  for(channel_number = 0; channel_number < STREAMING_AUDIO_CHANNEL_COUNT; channel_number++)
    {
      flush_streaming_audio_channel(context_, channel_number);
      if(context_->channels[channel_number].buffer_pool != NULL)
        {
          delete_memory_pool(context_->channels[channel_number].buffer_pool);
        }
    }

  free_runtime_memory(context_->template_array);
  context_->thread_item = 0;
  RUNTIME_EXIT_VENEER(0);
}
