/* Frontovl paired-CEL Cinepak crossfade configuration. */

#include "cinepak_cel_crossfade.h"

#include "hardware.h"

#define CINEPAK_CROSSFADE_COMPLEMENT 24
#define CINEPAK_CROSSFADE_UNAVAILABLE (-1)
#define CINEPAK_CROSSFADE_PIXC_MASK 0x1F001F00
#define CINEPAK_CROSSFADE_SECOND_PIXC_SHIFT 16
#define CINEPAK_CROSSFADE_BYTE_SHIFT 8
#define CINEPAK_CROSSFADE_PREVIOUS_HIGH_BYTE (-2)
#define CINEPAK_CROSSFADE_PREVIOUS_LOW_BYTE (-1)

uint32 g_CINEPAK_CROSSFADE_PIXC_VALUES[28] = {
  0x00000000, 0x00000300, 0x00000800, 0x00000200, 0x00001000, 0x00000B00, 0x00001800,
  0x00000100, 0x00001300, 0x00000A00, 0x00001B00, 0x00001F00, 0x00001200, 0x00001600,
  0x00001A00, 0x00000D00, 0x00001100, 0x00001500, 0x00001900, 0x00001D00, 0x00000300,
  0x00000200, 0x00000B00, 0x00000100, 0x00001300, 0x00000A00, 0x00001B00, 0x00001F00
};

uint8 g_CINEPAK_CROSSFADE_LEVEL_BYTES[52] = { 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
                                              0x01, 0x00, 0x02, 0x00, 0x02, 0x00, 0x03, 0x00, 0x03,
                                              0x00, 0x04, 0x00, 0x04, 0x00, 0x05, 0x00, 0x05, 0x00,
                                              0x06, 0x00, 0x06, 0x00, 0x07, 0x00, 0x07, 0x00, 0x08,
                                              0x00, 0x08, 0x00, 0x09, 0x00, 0x09, 0x00, 0x0A, 0x00,
                                              0x0A, 0x00, 0x0B, 0x00, 0x0B, 0x00, 0x00 };

typedef char
  CinepakCrossfadePixcValuesSize[sizeof(g_CINEPAK_CROSSFADE_PIXC_VALUES) == 112 ? 1 : -1];
typedef char CinepakCrossfadeLevelBytesSize[sizeof(g_CINEPAK_CROSSFADE_LEVEL_BYTES) == 52 ? 1 : -1];

void
configure_cinepak_crossfade_cels(CCB *first_cel_, CCB *second_cel_, int32 step_)
{
  uint8 *level_bytes;
  int32  complement;
  int32  current_level;
  int32  previous_level;
  uint32 pixc;

  level_bytes = &g_CINEPAK_CROSSFADE_LEVEL_BYTES[step_ << 1];
  current_level =
    (int32) (int16) (((uint16) level_bytes[0] << CINEPAK_CROSSFADE_BYTE_SHIFT) | level_bytes[1]);
  previous_level = (int32) (int16) (((uint16) level_bytes[CINEPAK_CROSSFADE_PREVIOUS_HIGH_BYTE]
                                     << CINEPAK_CROSSFADE_BYTE_SHIFT) |
                                    level_bytes[CINEPAK_CROSSFADE_PREVIOUS_LOW_BYTE]);
  if(current_level == CINEPAK_CROSSFADE_UNAVAILABLE)
    {
      first_cel_->ccb_Flags |= CCB_SKIP;
    }
  else
    {
      first_cel_->ccb_Flags &= ~CCB_SKIP;
      pixc = first_cel_->ccb_PIXC & ~CINEPAK_CROSSFADE_PIXC_MASK;
      pixc |=
        g_CINEPAK_CROSSFADE_PIXC_VALUES[current_level] |
        (g_CINEPAK_CROSSFADE_PIXC_VALUES[previous_level] << CINEPAK_CROSSFADE_SECOND_PIXC_SHIFT);
      first_cel_->ccb_PIXC = pixc;
    }

  complement  = CINEPAK_CROSSFADE_COMPLEMENT - step_;
  level_bytes = &g_CINEPAK_CROSSFADE_LEVEL_BYTES[complement << 1];
  current_level =
    (int32) (int16) (((uint16) level_bytes[0] << CINEPAK_CROSSFADE_BYTE_SHIFT) | level_bytes[1]);
  previous_level = (int32) (int16) (((uint16) level_bytes[CINEPAK_CROSSFADE_PREVIOUS_HIGH_BYTE]
                                     << CINEPAK_CROSSFADE_BYTE_SHIFT) |
                                    level_bytes[CINEPAK_CROSSFADE_PREVIOUS_LOW_BYTE]);
  if(current_level == CINEPAK_CROSSFADE_UNAVAILABLE)
    {
      second_cel_->ccb_Flags |= CCB_SKIP;
    }
  else
    {
      second_cel_->ccb_Flags &= ~CCB_SKIP;
      pixc = second_cel_->ccb_PIXC & ~CINEPAK_CROSSFADE_PIXC_MASK;
      pixc |=
        g_CINEPAK_CROSSFADE_PIXC_VALUES[current_level] |
        (g_CINEPAK_CROSSFADE_PIXC_VALUES[previous_level] << CINEPAK_CROSSFADE_SECOND_PIXC_SHIFT);
      second_cel_->ccb_PIXC = pixc;
    }
}
