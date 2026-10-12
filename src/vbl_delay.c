#include "vbl_delay.h"

#include "io_offset_request.h"

#include "graphics.h"

Item g_VBL_TIMER_IO_REQUEST;
Item g_GAME_VBL_IO_REQUEST;

Err
delay_vbls(int32 field_count_)
{
  return send_io_offset_request(g_VBL_TIMER_IO_REQUEST, field_count_);
}

Err
delay_game_vbls(int32 field_count_)
{
  return send_io_offset_request(g_GAME_VBL_IO_REQUEST, field_count_);
}

void
wait_for_vbls(int32 field_count_)
{
  Item io_request;

  io_request = GetVBLIOReq();
  send_io_offset_request(io_request, field_count_);
  DeleteItem(io_request);
}
