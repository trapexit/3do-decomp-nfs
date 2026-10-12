#pragma once

#include "kernel.h"
#include "types.h"

typedef struct CameraIoState
{
  Item  io_request;
  void *io_request_object;
  Item  device;
  Item  reply_port;
} CameraIoState;

extern CameraIoState g_CAMERA_IO_STATE;

int32
initialize_camera_io(void);
