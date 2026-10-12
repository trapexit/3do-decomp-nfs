#include "movie_frame_buffer.h"

#include "runtime_memory.h"
#include "tracked_memory.h"

#define MOVIE_DIMENSION_ALIGNMENT 4
#define MOVIE_DIMENSION_ALIGNMENT_MASK (MOVIE_DIMENSION_ALIGNMENT - 1)
#define MOVIE_PIXEL_BYTE_COUNT 2
#define LR_FORM_PIXEL_BYTE_COUNT 4
#define MOVIE_CEL_HDX_ONE 0x00100000
#define MOVIE_CEL_VDY_ONE 0x00010000
#define MOVIE_CEL_PIXC 0x1F811F00
#define LR_FORM_ROW_PREFETCH_WORD_COUNT 2
#define VIDEO_WINDOW_BITMAP_DATA_OFFSET 0x10
#define DEFAULT_MOVIE_FRAME_POSITION 0
#define MOVIE_FRAME_NOT_CENTERED 0
#define MOVIE_FRAME_PLACEMENT_PADDING 0
#define MOVIE_CEL_FLAGS                                                                            \
  (CCB_LDSIZE | CCB_LDPRS | CCB_LDPPMP | CCB_CCBPRE | CCB_ACW | CCB_ACCW | CCB_SPABS | CCB_YOXY |  \
   CCB_LCE | CCB_ACE)

static const char   G_MOVIE_FRAME_BUFFER_ALLOCATION_NAME[] = "movieFrameBuff";
MovieFramePlacement g_MOVIE_FRAME_PLACEMENT                = { DEFAULT_MOVIE_FRAME_POSITION,
                                                               DEFAULT_MOVIE_FRAME_POSITION,
                                                               MOVIE_FRAME_NOT_CENTERED,
                                                               { MOVIE_FRAME_PLACEMENT_PADDING } };
RuntimeVideoWindow *g_MOVIE_VIDEO_WINDOW                   = NULL;

void
initialize_external_movie_frame_buffer(MovieImage          *image_,
                                       const CinepakHeader *header_,
                                       void                *frame_buffer_)
{
  image_->width =
    (header_->width + MOVIE_DIMENSION_ALIGNMENT_MASK) & ~MOVIE_DIMENSION_ALIGNMENT_MASK;
  image_->height =
    (header_->height + MOVIE_DIMENSION_ALIGNMENT_MASK) & ~MOVIE_DIMENSION_ALIGNMENT_MASK;
  image_->row_byte_count     = image_->width * MOVIE_PIXEL_BYTE_COUNT;
  image_->base_address       = NULL;
  image_->frame_base_address = frame_buffer_;
}

int32
allocate_movie_frame_buffer(MovieImage *image_, const CinepakHeader *header_)
{
  void *frame_buffer;

  image_->width =
    (header_->width + MOVIE_DIMENSION_ALIGNMENT_MASK) & ~MOVIE_DIMENSION_ALIGNMENT_MASK;
  image_->height =
    (header_->height + MOVIE_DIMENSION_ALIGNMENT_MASK) & ~MOVIE_DIMENSION_ALIGNMENT_MASK;
  image_->row_byte_count = image_->width * MOVIE_PIXEL_BYTE_COUNT;

  frame_buffer = ALLOCATE_NAMED_SECONDARY_MEMORY(image_->row_byte_count * image_->height,
                                                 G_MOVIE_FRAME_BUFFER_ALLOCATION_NAME);
  if(frame_buffer == NULL)
    {
      return -1;
    }

  image_->base_address       = frame_buffer;
  image_->frame_base_address = image_->base_address;
  return 0;
}

int32
create_movie_video_window(MovieImage *image_, const CinepakHeader *header_)
{
  uint32 width;
  uint32 height;

  width  = (header_->width + MOVIE_DIMENSION_ALIGNMENT_MASK) & ~MOVIE_DIMENSION_ALIGNMENT_MASK;
  height = (header_->height + MOVIE_DIMENSION_ALIGNMENT_MASK) & ~MOVIE_DIMENSION_ALIGNMENT_MASK;
  g_MOVIE_VIDEO_WINDOW = (RuntimeVideoWindow *) create_runtime_video_window(width, height, 0);
  if(g_MOVIE_VIDEO_WINDOW == NULL)
    {
      return -1;
    }

  image_->width              = g_MOVIE_VIDEO_WINDOW->width;
  image_->height             = g_MOVIE_VIDEO_WINDOW->height;
  image_->row_byte_count     = g_MOVIE_VIDEO_WINDOW->row_byte_count;
  image_->video_window       = g_MOVIE_VIDEO_WINDOW;
  image_->base_address       = (g_MOVIE_VIDEO_WINDOW->bitmap + VIDEO_WINDOW_BITMAP_DATA_OFFSET);
  image_->frame_base_address = (g_MOVIE_VIDEO_WINDOW->bitmap + VIDEO_WINDOW_BITMAP_DATA_OFFSET);

  set_runtime_video_window(g_MOVIE_VIDEO_WINDOW);
  clear_runtime_display(0);
  finish_runtime_frame();
  return 0;
}

void
delete_movie_video_window(void)
{
  if(g_MOVIE_VIDEO_WINDOW != NULL)
    {
      delete_runtime_video_window(g_MOVIE_VIDEO_WINDOW);
      g_MOVIE_VIDEO_WINDOW = NULL;
    }
}

void
free_movie_frame_buffer(MovieImage *image_)
{
  void *frame_buffer;

  frame_buffer = image_->base_address;
  if(frame_buffer != NULL)
    {
      FREE_SECONDARY_MEMORY(frame_buffer);
      image_->base_address = NULL;
    }
}

void
initialize_lr_form_movie_cel(const MovieImage *image_, CCB *cel_)
{
  cel_->ccb_Flags   = MOVIE_CEL_FLAGS;
  cel_->ccb_NextPtr = NULL;
  cel_->ccb_PLUTPtr = NULL;
  cel_->ccb_XPos    = 0;
  cel_->ccb_YPos    = 0;
  cel_->ccb_HDX     = MOVIE_CEL_HDX_ONE;
  cel_->ccb_HDY     = 0;
  cel_->ccb_VDX     = 0;
  cel_->ccb_VDY     = MOVIE_CEL_VDY_ONE;
  cel_->ccb_HDDX    = 0;
  cel_->ccb_HDDY    = 0;
  cel_->ccb_PIXC    = MOVIE_CEL_PIXC;
  cel_->ccb_Width   = image_->width;
  cel_->ccb_Height  = image_->height;
  cel_->ccb_PRE0 = (((cel_->ccb_Height >> 1) - 1) << PRE0_VCNT_SHIFT) + PRE0_LINEAR + PRE0_BPP_16;
  cel_->ccb_PRE1 = ((image_->width - LR_FORM_ROW_PREFETCH_WORD_COUNT) << PRE1_WOFFSET10_SHIFT) +
                   (cel_->ccb_Width - 1) + PRE1_LRFORM;
  cel_->ccb_SourcePtr = (CelData *) image_->base_address;
}

void
position_movie_frame_buffer(MovieImage *image_, const Bitmap *bitmap_)
{
  int32 x_offset;
  int32 y_offset;

  if(g_MOVIE_FRAME_PLACEMENT.centered)
    {
      x_offset =
        (bitmap_->bm_Width - (image_->width + image_->x_position)) / MOVIE_PIXEL_BYTE_COUNT;
      y_offset =
        (bitmap_->bm_Height - (image_->height + image_->y_position)) / MOVIE_PIXEL_BYTE_COUNT;
    }
  else
    {
      x_offset = g_MOVIE_FRAME_PLACEMENT.x_position;
      y_offset = g_MOVIE_FRAME_PLACEMENT.y_position;
    }

  y_offset &= ~MOVIE_DIMENSION_ALIGNMENT_MASK;
  image_->frame_base_address = bitmap_->bm_Buffer + x_offset * LR_FORM_PIXEL_BYTE_COUNT +
                               bitmap_->bm_Width * MOVIE_PIXEL_BYTE_COUNT * y_offset;
  image_->row_byte_count     = bitmap_->bm_Width * MOVIE_PIXEL_BYTE_COUNT;
}

void
configure_movie_frame_placement(int32 x_position_, int32 y_position_, boolean centered_)
{
  g_MOVIE_FRAME_PLACEMENT.x_position = x_position_;
  g_MOVIE_FRAME_PLACEMENT.y_position = y_position_;
  g_MOVIE_FRAME_PLACEMENT.centered   = centered_;
}
