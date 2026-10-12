#include "headered_file.h"

#include "cel_access.h"
#include "file_buffer_load.h"
#include "file_load_options.h"

#include "mem.h"

#define HEADERED_POINTER_ARRAY_TAG 0x77777777
#define HEADERED_CEL_AACL_TAG 0x4141434C
#define HEADERED_CEL_CCB_TAG 0x43434220
#define HEADERED_CEL_PDAT_TAG 0x50444154
#define HEADERED_CEL_PLUT_TAG 0x504C5554
#define HEADERED_CEL_SOURCE_FLAG 0x00800000
#define HEADERED_CEL_DEFAULT_MASK 0xBFEE7FFF
#define HEADERED_CEL_DEFAULT_FLAGS 0x38084000
#define HEADERED_CEL_ALT_MASK 0x9FEE7FFF
#define HEADERED_CEL_ALT_FLAGS 0x18084000
#define HEADERED_CEL_RECORD_BYTES 8
#define HEADERED_CEL_CCB_BYTES 12
#define HEADERED_CEL_AACL_BYTES 16
#define HEADERED_POINTER_HEADER_WORDS 2
#define HEADERED_AACL_CEL_OFFSET_WORD 2
#define HEADERED_AACL_PLUT_OFFSET_WORD 3

typedef struct HeaderedCelRecord
{
  uint32 tag;
  int32  byte_count;
} HeaderedCelRecord;

static CCB *
_resolve_headered_cel_offsets(HeaderedCelRecord *record_, void **metadata_, int32 selector_)
{
  CCB  *cel;
  void *pixel_data;
  void *plut;
  int32 found_cel_source_flag;
  int32 finished;

  cel                   = NULL;
  pixel_data            = NULL;
  plut                  = NULL;
  found_cel_source_flag = 0;
  finished              = 0;
  if(metadata_ != NULL)
    *metadata_ = NULL;

  while(finished == 0)
    {
      if(record_->tag == HEADERED_CEL_AACL_TAG)
        {
          cel  = (CCB *) ((uint8 *) record_ + HEADERED_CEL_AACL_BYTES +
                          ((int32 *) record_)[HEADERED_AACL_CEL_OFFSET_WORD]);
          plut = (uint8 *) record_ + HEADERED_CEL_AACL_BYTES +
                 ((int32 *) record_)[HEADERED_AACL_PLUT_OFFSET_WORD];
          if(metadata_ != NULL)
            *metadata_ = plut;
          finished = 1;
        }
      else if(record_->tag == HEADERED_CEL_CCB_TAG)
        {
          cel = (CCB *) ((uint8 *) record_ + HEADERED_CEL_CCB_BYTES);
          if((cel->ccb_Flags & HEADERED_CEL_SOURCE_FLAG) != 0)
            found_cel_source_flag = 1;
        }
      else if(record_->tag == HEADERED_CEL_PDAT_TAG)
        {
          pixel_data = (uint8 *) record_ + HEADERED_CEL_RECORD_BYTES;
          if((found_cel_source_flag == 0) || (plut != NULL))
            finished = 1;
        }
      else if(record_->tag == HEADERED_CEL_PLUT_TAG)
        {
          plut = (uint8 *) record_ + HEADERED_CEL_CCB_BYTES;
          if(metadata_ != NULL)
            *metadata_ = plut;
          if(pixel_data != NULL)
            finished = 1;
        }

      record_ = (HeaderedCelRecord *) ((uint8 *) record_ + record_->byte_count);
    }

  if(cel != NULL)
    {
      cel->ccb_SourcePtr = pixel_data;
      cel->ccb_PLUTPtr   = plut;
      if(selector_ == 0)
        cel->ccb_Flags = (cel->ccb_Flags & HEADERED_CEL_DEFAULT_MASK) | HEADERED_CEL_DEFAULT_FLAGS;
      else
        {
          cel->ccb_Flags = (cel->ccb_Flags & HEADERED_CEL_ALT_MASK) | HEADERED_CEL_ALT_FLAGS;
          set_cel_dimension_log2(cel);
        }
    }

  return cel;
}

void *
resolve_headered_file_offsets(void *file_data_, int32 selector_)
{
  void   *previous_entry;
  uint32 *words;
  uint32  index;

  words = file_data_;
  if(words[0] == HEADERED_POINTER_ARRAY_TAG)
    {
      previous_entry = NULL;
      for(index = HEADERED_POINTER_HEADER_WORDS; index <= words[1] + 1; index++)
        {
          if(words[index] == 0)
            words[index] = (uint32) previous_entry;
          else
            {
              previous_entry =
                resolve_headered_file_offsets((uint8 *) file_data_ + words[index], selector_);
              words[index] = (uint32) previous_entry;
            }
        }
      return words + HEADERED_POINTER_HEADER_WORDS;
    }

  if((words[0] == HEADERED_CEL_CCB_TAG) || (words[0] == HEADERED_CEL_PLUT_TAG))
    {
      CCB *cel;

      cel = _resolve_headered_cel_offsets((HeaderedCelRecord *) file_data_, NULL, selector_);
      if(cel != NULL)
        return cel;
    }

  return file_data_;
}

void *
load_headered_file_with_default_options(const char *path_)
{
  void *file_data;

  ScavengeMem();
  file_data = load_file_with_options(path_, 0);
  ScavengeMem();
  if(file_data == NULL)
    return NULL;

  resolve_headered_file_offsets(file_data, 0);
  return file_data;
}

void *
load_headered_resource_file(const char *path_, void *resource_)
{
  void *file_data;

  if(resource_ == NULL)
    file_data = load_file_with_options(path_, 0);
  else
    file_data = load_file_into_buffer_quiet(path_, resource_);
  if(file_data == NULL)
    return NULL;

  resolve_headered_file_offsets(file_data, 0);
  return file_data;
}
