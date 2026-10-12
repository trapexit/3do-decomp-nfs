#pragma once

#include "types.h"

typedef struct CcbTemplate
{
  uint32  flags;
  uint32  next_offset;
  uint32  source_offset;
  uint16 *palette;
  int32   x_position;
  int32   y_position;
  int32   horizontal_dx;
  int32   horizontal_dy;
  int32   vertical_dx;
  int32   vertical_dy;
  int32   horizontal_ddx;
  int32   horizontal_ddy;
  uint32  pixel_control;
  uint32  preamble_0;
  uint32  preamble_1;
} CcbTemplate;

typedef struct RenderCcb
{
  CcbTemplate fields;
  int32       width;
} RenderCcb;

typedef struct CcbManagerState
{
  uint32     flags;
  RenderCcb *allocation_start;
  RenderCcb *allocation_cursor;
} CcbManagerState;

extern CcbTemplate     g_CCB_TEMPLATE;
extern CcbManagerState g_CCB_MANAGER_STATE;

void
set_ccb_pixel_control(uint32 pixel_control_);
uint32
get_ccb_pixel_control(void);
void
set_ccb_special_pixel_control(uint32 pixel_control_);
uint32
get_ccb_special_pixel_control(void);
void
set_ccb_flags(uint32 flags_);
uint32
get_ccb_flags(void);
void
set_ccb_position_offset(int32 x_offset_, int32 y_offset_);
void
set_ccb_transform(int32 horizontal_dx_,
                  int32 horizontal_dy_,
                  int32 vertical_dx_,
                  int32 vertical_dy_,
                  int32 horizontal_ddx_,
                  int32 horizontal_ddy_);
void
initialize_ccb_manager(RenderCcb *buffer_, uint32 ccb_count_, uint32 mode_);
uint32
set_ccb_manager_mode(int32 mode_);
void
reset_ccb_chain_for_display(void);
void
finalize_current_ccb_chain(void);
void
finalize_allocated_ccb_batch(void);
RenderCcb *
finalize_ccb_batch(RenderCcb *last_ccb_);
RenderCcb *
allocate_template_ccb(void);
RenderCcb *
allocate_untransformed_ccb(void);
RenderCcb *
allocate_packed_ccb(const uint32 *cel_header_, uint16 *palette_);
RenderCcb *
allocate_partial_copy_ccb(const RenderCcb *source_ccb_);
RenderCcb *
allocate_previous_ccb_prefix(uint16 *palette_, uint32 horizontal_dx_);
RenderCcb *
allocate_cel_header_ccb(const uint32 *cel_header_, int32 x_offset_, int32 y_offset_);
