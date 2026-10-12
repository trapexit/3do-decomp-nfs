#include "control_subscriber.h"
#include "data_stream_state.h"
#include "sdk_veneers.h"
#include "streaming_thread.h"
#include "streaming_thread_stack.h"

#include "kernel.h"
#include "task.h"

#define CONTROL_SUBSCRIBER_STACK_SIZE 4096
#define CONTROL_SUBSCRIBER_THREAD_NAME "Ctrl"
#define CONTROL_SUBSCRIBER_NO_MEMORY -992
#define CONTROL_SUBSCRIBER_NO_SIGNAL -991
#define CONTROL_SUBSCRIBER_SIGNAL_ERROR -990
#define CONTROL_CHUNK_TEXT_SIZE 5

static const char G_CONTROL_CUE_COUNT_FORMAT[] = "File has %ld cues:\n";
static const char G_CONTROL_CUE_FORMAT[]       = "%s/%s:  %ld & %ld\n";
static const char G_CONTROL_CUE_WARNING_FORMAT[] =
  "Warning:  file specified %ld cues, but we only could save %ld of them.\n";

int32
create_control_subscriber(ControlSubscriberContext **context_out_,
                          DSStreamCBPtr              stream_,
                          int32                      priority_adjustment_)
{
  CtrlContext              *standard;
  ControlSubscriberContext *context;
  uint32                    received_signals;
  int32                     status;

  context = allocate_memory_pool_entry(g_CONTROL_SUBSCRIBER_CONTEXT_POOL);
  if(context == NULL)
    {
      return CONTROL_SUBSCRIBER_NO_MEMORY;
    }

  standard                = &context->standard;
  standard->creatorTask   = CURRENTTASK->t.n_Item;
  standard->streamCBPtr   = stream_;
  standard->creatorSignal = AllocSignal(0);
  if(standard->creatorSignal == 0)
    {
      status = CONTROL_SUBSCRIBER_NO_SIGNAL;
      goto cleanup;
    }

  status = create_streaming_thread((StreamingThreadEntry) run_control_subscriber_service_thread,
                                   CONTROL_SUBSCRIBER_STACK_SIZE,
                                   (int32) CURRENTTASK->t.n_Priority + priority_adjustment_,
                                   CONTROL_SUBSCRIBER_THREAD_NAME,
                                   &standard->threadStackBlock,
                                   0,
                                   context);
  if(status > 0)
    {
      standard->threadItem = status;
    }
  else
    {
      goto cleanup;
    }

  received_signals = WaitSignal(standard->creatorSignal);
  if(received_signals != standard->creatorSignal)
    {
      return CONTROL_SUBSCRIBER_SIGNAL_ERROR;
    }

  FreeSignal(standard->creatorSignal);
  status = standard->creatorStatus;
  if(status >= 0)
    {
      *context_out_ = context;
      return status;
    }

cleanup:
  if(standard->threadStackBlock != NULL)
    {
      free_streaming_thread_stack(standard->threadStackBlock);
    }
  return_memory_pool_entry(g_CONTROL_SUBSCRIBER_CONTEXT_POOL, context);
  return status;
}

int32
dispose_control_subscriber(ControlSubscriberContext *context_)
{
  CtrlContext *standard;

  if(context_ != NULL)
    {
      standard = &context_->standard;
      if(standard->threadItem > 0)
        {
          dispose_streaming_thread(standard->threadItem);
        }
      if(standard->threadStackBlock != NULL)
        {
          free_streaming_thread_stack(standard->threadStackBlock);
        }
      return_memory_pool_entry(g_CONTROL_SUBSCRIBER_CONTEXT_POOL, context_);
    }
  return 0;
}

void
load_control_cue_anchors(ControlSubscriberContext *context_,
                         int32                     chunk_time_,
                         int32                     cue_anchor_count_,
                         const ControlCueAnchor   *cue_anchors_)
{
  char  chunk_text[CONTROL_CHUNK_TEXT_SIZE];
  char  chunk_subtype_text[CONTROL_CHUNK_TEXT_SIZE];
  int32 anchor_index;

  (void) chunk_time_;
  printf(G_CONTROL_CUE_COUNT_FORMAT, cue_anchor_count_);
  context_->cue_anchor_count = 0;
  anchor_index               = 0;
  while(anchor_index != cue_anchor_count_ && anchor_index < CONTROL_CUE_ANCHOR_COUNT)
    {
      context_->cue_anchors[anchor_index] = cue_anchors_[anchor_index];
      RUNTIME_MEMCPY_VENEER(chunk_text,
                            &cue_anchors_[anchor_index].chunk_type,
                            CONTROL_CHUNK_TEXT_SIZE - 1);
      chunk_text[CONTROL_CHUNK_TEXT_SIZE - 1] = 0;
      RUNTIME_MEMCPY_VENEER(chunk_subtype_text,
                            &cue_anchors_[anchor_index].chunk_subtype,
                            CONTROL_CHUNK_TEXT_SIZE - 1);
      chunk_subtype_text[CONTROL_CHUNK_TEXT_SIZE - 1] = 0;
      printf(G_CONTROL_CUE_FORMAT,
             chunk_text,
             chunk_subtype_text,
             cue_anchors_[anchor_index].primary_base_time,
             cue_anchors_[anchor_index].alternate_base_time);
      context_->cue_anchor_count++;
      anchor_index = context_->cue_anchor_count;
    }
  if(cue_anchor_count_ > CONTROL_CUE_ANCHOR_COUNT)
    {
      printf(G_CONTROL_CUE_WARNING_FORMAT, cue_anchor_count_, CONTROL_CUE_ANCHOR_COUNT);
    }
}

void
dispatch_due_control_cues(ControlSubscriberContext *context_,
                          int32                     unused_time_,
                          ControlCueRequest        *cue_requests_,
                          int32                     cue_request_count_)
{
  ControlCueAnchor  *anchor;
  ControlCueRequest *request;
  ControlCueCallback callback;
  uint32             current_stream_time;
  int32              base_stream_time;
  int32              due_stream_time;
  int32              request_index;
  int32              anchor_index;
  int32              status;

  (void) unused_time_;
  status =
    get_data_stream_clock((DataStreamState *) context_->standard.streamCBPtr, &current_stream_time);
  if(status < 0)
    {
      return;
    }

  request = cue_requests_;
  for(request_index = 0; request_index < cue_request_count_; request_index++, request++)
    {
      for(anchor_index = 0; anchor_index < context_->cue_anchor_count; anchor_index++)
        {
          anchor = &context_->cue_anchors[anchor_index];
          if(anchor->chunk_type != request->chunk_type ||
             anchor->chunk_subtype != request->chunk_subtype)
            {
              continue;
            }

          if(request->use_alternate_base_time != 0)
            {
              base_stream_time = anchor->alternate_base_time;
            }
          else
            {
              base_stream_time = anchor->primary_base_time;
            }
          due_stream_time = request->trigger_time_offset + base_stream_time;
          if(due_stream_time <= (int32) current_stream_time && request->callback != NULL)
            {
              callback          = request->callback;
              request->callback = NULL;
              callback(request->callback_argument, (int32) current_stream_time - base_stream_time);
            }
          break;
        }
    }
}
