#pragma once

#include "audio.h"

Item
FindAudioDevice(TagArg *tags_);
Err
ControlAudioDevice(Item    device_,
                   void   *send_buffer_,
                   int32   send_byte_count_,
                   void   *receive_buffer_,
                   int32   receive_byte_count_,
                   TagArg *tags_);
int32
GetNumInstruments(Item instrument_template_);
frac16
GetMasterTuning(void);
int32
DSPGetMaxTicks(void);
