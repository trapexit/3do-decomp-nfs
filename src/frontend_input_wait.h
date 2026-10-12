#pragma once

#include "kernel.h"
#include "types.h"

extern Item  g_FRONTEND_WAIT_IO_REQUEST;
extern int32 g_FRONTEND_WAIT_PENDING;
extern int32 g_FRONTEND_WAIT_ALTERNATE_MODE;

Item
initialize_frontend_wait_io_request(void);
void
wait_for_frontend_input(void);
int32
begin_frontend_input_wait(void);
Err
wait_one_frontend_vbl(void);
Err
wait_one_frontend_vbl_secondary(void);
Err
wait_one_frontend_vbl_tertiary(void);
int32
toggle_frontend_wait_alternate_mode(void);
