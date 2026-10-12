#include "control_subscriber.h"
#include "data_stream_requests.h"
#include "data_stream_state.h"
#include "kernel_diagnostics.h"
#include "sdk_veneers.h"

#include "datastreamdebug.h"

#define CONTROL_CUES_CHUNK_SUBTYPE 0x43554553
#define CONTROL_CHUNK_TEXT_SIZE 5

enum
{
  K_CONTROL_STREAM_END_STATE_MASK = 0x202
};

static const char G_CONTROL_FAILURE_FORMAT[]         = "Failure in %s: $%lx\n";
static const char G_CONTROL_GO_MARKER_OPERATION[]    = "ControlSubscriber - DSGoMarker() = ";
static const char G_CONTROL_MARKER_CHECK_OPERATION[] = "ControlSubscriber - DSIsMarker() = ";
static const char G_CONTROL_CLOCK_SYNC_OPERATION[]   = "ControlSubscriber - DSClockSync() = ";
static const char G_CONTROL_STOP_STREAM_OPERATION[]  = "ControlSubscriber - DSStopStream() =  ";
static const char G_UNKNOWN_CONTROL_CHUNK_FORMAT[] =
  "Control Subscriber unknown chunk %s (0x%lx)\n";

int32
process_control_data_chunk(SubscriberMsgPtr message_, ControlSubscriberContext *context_)
{
  ControlChunkPtr  control_chunk;
  ControlCueChunk *cue_chunk;
  DSRequestMsgPtr  request;
  boolean          is_marker;
  char             chunk_text[CONTROL_CHUNK_TEXT_SIZE];
  int32            status;

  control_chunk = (ControlChunkPtr) message_->msg.data.buffer;
  if(control_chunk->subChunkType == GOTO_CHUNK_SUBTYPE)
    {
      request = (DSRequestMsgPtr) allocate_memory_pool_entry(
        (MemoryPool *) context_->standard.dsReqMsgPool);
      if(request != NULL)
        {
          status = go_to_data_stream_marker(request->msgItem,
                                            request,
                                            context_->standard.streamCBPtr,
                                            control_chunk->u.marker.value,
                                            GOMARKER_ABSOLUTE);
        }
      else
        {
          status = kDSNoMsgErr;
        }
      if(status < 0)
        {
          printf(G_CONTROL_FAILURE_FORMAT, G_CONTROL_GO_MARKER_OPERATION, status);
          PrintfDSError(status);
          RUNTIME_EXIT_VENEER(0);
        }
      return status;
    }

  if(control_chunk->subChunkType == SYNC_CHUNK_SUBTYPE)
    {
      status = is_data_stream_marker((DataStreamState *) context_->standard.streamCBPtr,
                                     control_chunk->u.sync.value,
                                     &is_marker);
      if(status < 0)
        {
          printf(G_CONTROL_FAILURE_FORMAT, G_CONTROL_MARKER_CHECK_OPERATION, status);
          PrintfDSError(status);
          RUNTIME_EXIT_VENEER(0);
        }
      if(status != 0 || is_marker == 0)
        {
          return status;
        }

      status = set_data_stream_clock((DataStreamState *) context_->standard.streamCBPtr,
                                     control_chunk->time);
      if(status != 0)
        {
          return status;
        }
      status = synchronize_data_stream_clock(context_->standard.streamCBPtr,
                                             (MemoryPool *) context_->standard.subsMsgPool,
                                             control_chunk->time);
      if(status < 0)
        {
          printf(G_CONTROL_FAILURE_FORMAT, G_CONTROL_CLOCK_SYNC_OPERATION, status);
          PrintfDSError(status);
          RUNTIME_EXIT_VENEER(0);
        }
      return status;
    }

  if(control_chunk->subChunkType == STOP_CHUNK_SUBTYPE)
    {
      request = (DSRequestMsgPtr) allocate_memory_pool_entry(
        (MemoryPool *) context_->standard.dsReqMsgPool);
      if(request != NULL)
        {
          context_->standard.streamCBPtr->streamFlags |= K_CONTROL_STREAM_END_STATE_MASK;
          status = 0;
        }
      else
        {
          status = kDSNoMsgErr;
        }
      if(status < 0)
        {
          printf(G_CONTROL_FAILURE_FORMAT, G_CONTROL_STOP_STREAM_OPERATION, status);
          PrintfDSError(status);
          RUNTIME_EXIT_VENEER(0);
        }
      return status;
    }

  if(control_chunk->subChunkType == CONTROL_CUES_CHUNK_SUBTYPE)
    {
      cue_chunk = (ControlCueChunk *) control_chunk;
      if(cue_chunk->version == CONTROL_CUE_CHUNK_VERSION)
        {
          load_control_cue_anchors(context_,
                                   cue_chunk->time,
                                   cue_chunk->cue_anchor_count,
                                   cue_chunk->cue_anchors);
        }
      return 0;
    }

  RUNTIME_MEMCPY_VENEER(chunk_text, &control_chunk->subChunkType, CONTROL_CHUNK_TEXT_SIZE - 1);
  chunk_text[CONTROL_CHUNK_TEXT_SIZE - 1] = 0;
  print_unknown_control_chunk(G_UNKNOWN_CONTROL_CHUNK_FORMAT,
                              chunk_text,
                              control_chunk->subChunkType);
  return 0;
}
