#include "legacy_audio.h"

enum
{
  LEGACY_AUDIO_FIND_DEVICE_VECTOR_OFFSET      = 0x64,
  LEGACY_AUDIO_CONTROL_DEVICE_VECTOR_OFFSET   = 0x6C,
  LEGACY_AUDIO_INSTRUMENT_COUNT_VECTOR_OFFSET = 0x70,
  LEGACY_AUDIO_MASTER_TUNING_VECTOR_OFFSET    = 0x74,
  LEGACY_AUDIO_MAXIMUM_TICK_EXECUTION_OFFSET  = 1
};

typedef Item   (*LegacyAudioFindDeviceFunction)(TagArg *tags_);
typedef Err    (*LegacyAudioControlDeviceFunction)(Item    device_,
                                                   void   *send_buffer_,
                                                   int32   send_byte_count_,
                                                   void   *receive_buffer_,
                                                   int32   receive_byte_count_,
                                                   TagArg *tags_);
typedef int32  (*LegacyAudioInstrumentCountFunction)(Item instrument_template_);
typedef frac16 (*LegacyAudioMasterTuningFunction)(void);

extern void *AudioBase;

Item
FindAudioDevice(TagArg *tags_)
{
  LegacyAudioFindDeviceFunction function;

  /* Portfolio folio vectors are stored immediately before the folio base. */
  function = (*(LegacyAudioFindDeviceFunction *) ((uint8 *) AudioBase -
                                                  LEGACY_AUDIO_FIND_DEVICE_VECTOR_OFFSET));
  return function(tags_);
}

Err
ControlAudioDevice(Item    device_,
                   void   *send_buffer_,
                   int32   send_byte_count_,
                   void   *receive_buffer_,
                   int32   receive_byte_count_,
                   TagArg *tags_)
{
  LegacyAudioControlDeviceFunction function;

  function = (*(LegacyAudioControlDeviceFunction *) ((uint8 *) AudioBase -
                                                     LEGACY_AUDIO_CONTROL_DEVICE_VECTOR_OFFSET));
  return function(device_,
                  send_buffer_,
                  send_byte_count_,
                  receive_buffer_,
                  receive_byte_count_,
                  tags_);
}

int32
GetNumInstruments(Item instrument_template_)
{
  LegacyAudioInstrumentCountFunction function;

  function =
    (*(LegacyAudioInstrumentCountFunction *) ((uint8 *) AudioBase -
                                              LEGACY_AUDIO_INSTRUMENT_COUNT_VECTOR_OFFSET));
  return function(instrument_template_);
}

frac16
GetMasterTuning(void)
{
  LegacyAudioMasterTuningFunction function;

  function = (*(LegacyAudioMasterTuningFunction *) ((uint8 *) AudioBase -
                                                    LEGACY_AUDIO_MASTER_TUNING_VECTOR_OFFSET));
  return function();
}

int32
DSPGetMaxTicks(void)
{
  return DSPReadEO(LEGACY_AUDIO_MAXIMUM_TICK_EXECUTION_OFFSET);
}
