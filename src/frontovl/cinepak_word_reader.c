#include "types.h"
#define CINEPAK_WORD_ALIGNMENT_MASK 3
#define CINEPAK_WORD_BYTE_2_INDEX 2
#define CINEPAK_WORD_BYTE_3_INDEX 3

unsigned long
read_cinepak_chunk_word(const unsigned char *bytes_)
{
  unsigned long        word;
  const unsigned char *source;
  unsigned char       *destination;

  if(((unsigned long) bytes_ & CINEPAK_WORD_ALIGNMENT_MASK) == 0)
    return *(const unsigned long *) bytes_;

  source      = bytes_;
  destination = (unsigned char *) &word;
  destination[0] = source[0];
  destination[1] = source[1];
  destination[CINEPAK_WORD_BYTE_2_INDEX] = source[CINEPAK_WORD_BYTE_2_INDEX];
  destination[CINEPAK_WORD_BYTE_3_INDEX] = source[CINEPAK_WORD_BYTE_3_INDEX];
  return word;
}


unsigned long
read_cinepak_frame_word(const unsigned char *bytes_)
{
  unsigned long        word;
  const unsigned char *source;
  unsigned char       *destination;

  if(((unsigned long) bytes_ & CINEPAK_WORD_ALIGNMENT_MASK) == 0)
    return *(const unsigned long *) bytes_;

  source      = bytes_;
  destination = (unsigned char *) &word;
  destination[0] = source[0];
  destination[1] = source[1];
  destination[CINEPAK_WORD_BYTE_2_INDEX] = source[CINEPAK_WORD_BYTE_2_INDEX];
  destination[CINEPAK_WORD_BYTE_3_INDEX] = source[CINEPAK_WORD_BYTE_3_INDEX];
  return word;
}
