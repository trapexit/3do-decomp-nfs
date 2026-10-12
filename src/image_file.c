#include "image_file.h"

#include "cel_access.h"
#include "diagnostics.h"
#include "display_graphics.h"
#include "file_load_options.h"
#include "memory.h"

#include "string.h"

#define IMAGE_CHUNK_COPYRIGHT 0x43505952
#define IMAGE_CHUNK_CREDITS 0x43524454
#define IMAGE_CHUNK_DESCRIPTION 0x44455343
#define IMAGE_CHUNK_CONTROL 0x494D4147
#define IMAGE_CHUNK_KEYWORDS 0x4B575244
#define IMAGE_CHUNK_PIXELS 0x50444154
#define IMAGE_CHUNK_VDL 0x56444C20
#define IMAGE_CHUNK_EXTRA 0x58545241
#define IMAGE_DEFAULT_BYTES 0x25800
#define IMAGE_FILE_MEMORY_FLAGS 0x260
#define IMAGE_PIXEL_HEADER_BYTES 8
#define IMAGE_PIXEL_BYTES 2

static int32
_get_image_destination_byte_count(void)
{
  int32 byte_count;

  if(g_DISPLAY_WIDTH == 0)
    byte_count = IMAGE_DEFAULT_BYTES;
  else
    byte_count = g_DISPLAY_WIDTH * g_DISPLAY_HEIGHT * IMAGE_PIXEL_BYTES;

  return ((byte_count + g_VRAM_PAGE_BYTES - 1) / g_VRAM_PAGE_BYTES) * g_VRAM_PAGE_BYTES;
}

void *
load_image(const char *path_, uint8 *destination_, VdlChunk **vdl_, struct ScreenContext *screen_)
{
  CelChunkHeader *chunk;
  CelChunkHeader *cursor;
  ImageCC        *image_control;
  PixelChunk     *pixel_chunk;
  VdlChunk       *vdl_chunk;
  uint8          *file_data;
  uint8          *vdl_copy;
  int32           destination_byte_count;
  int32           file_byte_count;
  int32           pixel_byte_count;
  int32           supplied_destination;
  uint32          chunk_type;

  (void) screen_;
  image_control          = NULL;
  pixel_chunk            = NULL;
  vdl_chunk              = NULL;
  destination_byte_count = _get_image_destination_byte_count();
  file_data       = (uint8 *) load_file_with_options_with_error(path_, IMAGE_FILE_MEMORY_FLAGS);
  file_byte_count = (int32) get_memory_payload_byte_count(file_data);
  cursor          = (CelChunkHeader *) file_data;

  chunk = advance_cel_chunk(&chunk_type, &cursor, &file_byte_count);
  while(chunk != NULL)
    {
      if(chunk_type == IMAGE_CHUNK_CONTROL)
        {
          if(image_control == NULL)
            image_control = (ImageCC *) chunk;
        }
      else if(chunk_type == IMAGE_CHUNK_PIXELS)
        {
          if(pixel_chunk == NULL)
            pixel_chunk = (PixelChunk *) chunk;
        }
      else if(chunk_type == IMAGE_CHUNK_VDL)
        {
          if(vdl_chunk == NULL)
            vdl_chunk = (VdlChunk *) chunk;
        }
      else if((chunk_type != IMAGE_CHUNK_KEYWORDS) && (chunk_type != IMAGE_CHUNK_COPYRIGHT) &&
              (chunk_type != IMAGE_CHUNK_CREDITS) && (chunk_type != IMAGE_CHUNK_DESCRIPTION) &&
              (chunk_type != IMAGE_CHUNK_EXTRA))
        {
          fatal_error("loadimage - INVALID CHUNK TYPE '%.4s' IN FILE '%s'\n", chunk, path_);
        }

      chunk = advance_cel_chunk(&chunk_type, &cursor, &file_byte_count);
    }

  if(vdl_chunk != NULL)
    {
      if(vdl_ == NULL)
        {
          fatal_error("loadimage - VDL CHUNK FOUND IN FILE '%s', "
                      "NO WAY TO RETURN IT\n",
                      path_);
        }
      else
        {
          vdl_copy = (uint8 *) allocate_named_memory(path_, (uint32) vdl_chunk->chunk_size, 0);
          memcpy(vdl_copy, vdl_chunk, (size_t) vdl_chunk->chunk_size);
          *vdl_ = (VdlChunk *) vdl_copy;
        }
    }

  pixel_byte_count = IMAGE_DEFAULT_BYTES;
  if(image_control != NULL)
    pixel_byte_count = image_control->bytesperrow * image_control->h;

  if(pixel_chunk == NULL)
    {
      fatal_error("loadimage - NO PIXEL DATA CHUNK IN IMAGE FILE '%s'\n", path_);
    }
  if(pixel_byte_count > destination_byte_count)
    {
      fatal_error("Image too big for destination buffer in file %s\n", path_);
    }

  supplied_destination = 1;
  if(destination_ == NULL)
    {
      supplied_destination = 0;
      destination_         = file_data;
    }

  move_bytes((const uint8 *) pixel_chunk + IMAGE_PIXEL_HEADER_BYTES,
             destination_,
             (uint32) pixel_byte_count);
  if(destination_byte_count > pixel_byte_count)
    {
      clear_bytes(destination_ + pixel_byte_count, destination_byte_count - pixel_byte_count);
    }

  if(supplied_destination != 0)
    free_memory(file_data);
  resize_memory(destination_, pixel_byte_count);
  return destination_;
}

void *
load_image_default(const char *path_)
{
  return load_image(path_, NULL, NULL, NULL);
}

void
unload_image(void *image_)
{
  if(image_ != NULL)
    free_memory(image_);
}

void
unload_image_handle(void *image_)
{
  if(image_ != NULL)
    unload_image(image_);
}
