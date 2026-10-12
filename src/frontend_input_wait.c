#include "frontend_input_wait.h"

#include "io_offset_request.h"
#include "legacy_io_wait.h"

#include "graphics.h"
#include "io.h"

int32 g_FRONTEND_WAIT_LAYOUT_PREFIX;
Item  g_FRONTEND_WAIT_IO_REQUEST;
int32 g_FRONTEND_WAIT_PENDING;
int32 g_FRONTEND_WAIT_ALTERNATE_MODE;

Item
initialize_frontend_wait_io_request(void)
{
  g_FRONTEND_WAIT_IO_REQUEST = GetVBLIOReq();
  return g_FRONTEND_WAIT_IO_REQUEST;
}

void
wait_for_frontend_input(void)
{
  if(g_FRONTEND_WAIT_PENDING != 0)
    wait_for_io_completion(g_FRONTEND_WAIT_IO_REQUEST);
  else
    send_io_offset_request(g_FRONTEND_WAIT_IO_REQUEST, 1);

  g_FRONTEND_WAIT_PENDING = 0;
}

int32
begin_frontend_input_wait(void)
{
  (void) send_io_offset_request_async(g_FRONTEND_WAIT_IO_REQUEST, 1);
  g_FRONTEND_WAIT_PENDING = 1;
  return 1;
}

Err
wait_one_frontend_vbl(void)
{
  return send_io_offset_request(g_FRONTEND_WAIT_IO_REQUEST, 1);
}

Err
wait_one_frontend_vbl_secondary(void)
{
  return send_io_offset_request(g_FRONTEND_WAIT_IO_REQUEST, 1);
}

Err
wait_one_frontend_vbl_tertiary(void)
{
  return send_io_offset_request(g_FRONTEND_WAIT_IO_REQUEST, 1);
}

int32
toggle_frontend_wait_alternate_mode(void)
{
  g_FRONTEND_WAIT_ALTERNATE_MODE ^= 1;
  return g_FRONTEND_WAIT_ALTERNATE_MODE;
}
