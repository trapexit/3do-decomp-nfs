/* Frontovl wrapper for loading an audio instrument template. */

#include "runtime_audio_template.h"

#include "audio.h"

enum
{
  AUDIO_LOAD_INSTRUMENT_TEMPLATE_VECTOR_OFFSET = 0x04
};

typedef Item (*AudioLoadInstrumentTemplateFunction)(char *name_, Item audio_device_);

extern void *AudioBase;

Item
load_runtime_instrument_template(char *name_, Item audio_device_)
{
  AudioLoadInstrumentTemplateFunction function;

  function = (*(AudioLoadInstrumentTemplateFunction *)
                ((uint8 *) AudioBase - AUDIO_LOAD_INSTRUMENT_TEMPLATE_VECTOR_OFFSET));
  return function(name_, audio_device_);
}
