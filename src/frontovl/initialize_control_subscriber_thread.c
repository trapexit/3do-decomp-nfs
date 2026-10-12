#include "control_subscriber.h"
#include "sdk_veneers.h"
#include "stream_message_items.h"

#include "audio.h"
#include "kernel.h"
#include "nodes.h"

#define CONTROL_SUBSCRIBER_NO_MEMORY -992

int32
initialize_control_subscriber_thread(ControlSubscriberContext *context_)
{
  CtrlContext *standard;
  int32        channel_number;
  int32        signal_status;
  int32        status;

  standard                = &context_->standard;
  standard->creatorStatus = -1;
  standard->requestPort   = 0;
  standard->cueItem       = 0;
  standard->cueSignal     = 0;
  standard->fTimerRunning = 0;

  status = OpenAudioFolio();
  if(status < 0)
    {
      standard->creatorStatus = status;
      goto signal_creator;
    }

  standard->subsReplyPortSignal  = 0;
  standard->subsReplyPort        = 0;
  standard->subsMsgPool          = NULL;
  standard->dsReqReplyPort       = 0;
  standard->dsReqReplyPortSignal = 0;
  standard->dsReqMsgPool         = NULL;
  for(channel_number = 0; channel_number < CTRL_MAX_CHANNELS; channel_number++)
    {
      standard->channel[channel_number].status        = CHAN_ENABLED;
      standard->channel[channel_number].msgQueue.head = NULL;
      standard->channel[channel_number].msgQueue.tail = NULL;
    }
  context_->cue_anchor_count = 0;

  status = create_message_port(&standard->requestPortSignal);
  if(status <= 0)
    {
      goto signal_creator;
    }
  standard->requestPort = status;

  status = create_message_port(&standard->dsReqReplyPortSignal);
  if(status <= 0)
    {
      goto signal_creator;
    }
  standard->dsReqReplyPort = status;

  standard->creatorStatus = CONTROL_SUBSCRIBER_NO_MEMORY;
  standard->dsReqMsgPool =
    (MemPoolPtr) create_memory_pool(CTRL_NUM_DS_REQS_MSGS, sizeof(DSRequestMsg));
  if(standard->dsReqMsgPool == NULL)
    {
      goto signal_creator;
    }
  if(!fill_pool_with_stream_message_items((MemoryPool *) standard->dsReqMsgPool,
                                          standard->dsReqReplyPort))
    {
      goto signal_creator;
    }

  status = create_message_port(&standard->subsReplyPortSignal);
  if(status <= 0)
    {
      goto signal_creator;
    }
  standard->subsReplyPort = status;

  standard->subsMsgPool =
    (MemPoolPtr) create_memory_pool(CTRL_MAX_SUBS_MESSAGES, sizeof(SubscriberMsg));
  if(standard->subsMsgPool == NULL)
    {
      goto signal_creator;
    }
  if(!fill_pool_with_stream_message_items((MemoryPool *) standard->subsMsgPool,
                                          standard->subsReplyPort))
    {
      goto signal_creator;
    }

  status = CreateSizedItem(MKNODEID(AUDIONODE, AUDIO_CUE_NODE), NULL, 0);
  if(status <= 0)
    {
      goto signal_creator;
    }
  standard->cueItem   = status;
  standard->cueSignal = GetCueSignal(standard->cueItem);
  if(standard->cueSignal != 0)
    {
      standard->creatorStatus = 0;
    }

signal_creator:
  signal_status = SendSignal(standard->creatorTask, standard->creatorSignal);
  if(standard->creatorStatus < 0 || signal_status < 0)
    {
      return -1;
    }
  return 0;
}
