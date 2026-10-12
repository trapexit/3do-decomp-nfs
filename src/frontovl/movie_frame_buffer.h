#pragma once

#include "graphics.h"
#include "types.h"

#define VIDEO_WINDOW_RESERVED_TO_ROW_BYTES 0x18
#define VIDEO_WINDOW_RESERVED_TO_BITMAP 0x08

typedef struct RuntimeVideoWindow
{
  int32  width;
  int32  height;
  uint8  reserved_to_row_bytes[VIDEO_WINDOW_RESERVED_TO_ROW_BYTES];
  int32  row_byte_count;
  uint8  reserved_to_bitmap[VIDEO_WINDOW_RESERVED_TO_BITMAP];
  uint8 *bitmap;
} RuntimeVideoWindow;

typedef struct MovieImage
{
  void *base_address;
  int32 row_byte_count;
  void *frame_base_address;
  int32 width;
  int32 height;
  int32 x_position;
  int32 y_position;
  void *video_window;
} MovieImage;

#define MOVIE_FRAME_PLACEMENT_PADDING_SIZE 3

typedef struct MovieFramePlacement
{
  int32   x_position;
  int32   y_position;
  boolean centered;
  uint8   padding[MOVIE_FRAME_PLACEMENT_PADDING_SIZE];
} MovieFramePlacement;

typedef struct CinepakHeader
{
  uint32 subscriber_chunk_header[5];
  uint32 version;
  uint32 compression_type;
  uint32 height;
  uint32 width;
  uint32 scale;
  uint32 frame_count;
} CinepakHeader;

extern MovieFramePlacement g_MOVIE_FRAME_PLACEMENT;
extern RuntimeVideoWindow *g_MOVIE_VIDEO_WINDOW;

void
initialize_external_movie_frame_buffer(MovieImage          *image_,
                                       const CinepakHeader *header_,
                                       void                *frame_buffer_);
int32
allocate_movie_frame_buffer(MovieImage *image_, const CinepakHeader *header_);
int32
create_movie_video_window(MovieImage *image_, const CinepakHeader *header_);
void
delete_movie_video_window(void);
void
free_movie_frame_buffer(MovieImage *image_);
void
initialize_lr_form_movie_cel(const MovieImage *image_, CCB *cel_);
void
position_movie_frame_buffer(MovieImage *image_, const Bitmap *bitmap_);
void
configure_movie_frame_placement(int32 x_position_, int32 y_position_, boolean centered_);
