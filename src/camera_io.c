#include "camera_io.h"

#include "device.h"
#include "io.h"
#include "item.h"
#include "msgport.h"

#define CAMERA_DEVICE_NAME "camera"
#define CAMERA_REPLY_PORT_NAME "reply port"
#define CAMERA_REPLY_PORT_TAG_COUNT 2
#define CAMERA_IO_REQUEST_TAG_COUNT 3
#define CAMERA_IO_REQUEST_DEVICE_TAG_INDEX 0
#define CAMERA_IO_REQUEST_REPLY_PORT_TAG_INDEX 1
#define CAMERA_IO_REQUEST_END_TAG_INDEX 2

CameraIoState g_CAMERA_IO_STATE;

int32
initialize_camera_io(void)
{
  Item   device;
  Item   reply_port;
  Item   io_request;
  TagArg reply_port_tags[CAMERA_REPLY_PORT_TAG_COUNT];
  TagArg io_request_tags[CAMERA_IO_REQUEST_TAG_COUNT];

  if((KernelBase->kb_CPUFlags & KB_NODBGR) != 0)
    return 0;

  device                   = FindDevice(CAMERA_DEVICE_NAME);
  device                   = OpenItem(device, NULL);
  g_CAMERA_IO_STATE.device = device;
  if(device < 0)
    return device;

  reply_port_tags[0].ta_Tag = TAG_ITEM_NAME;
  reply_port_tags[0].ta_Arg = CAMERA_REPLY_PORT_NAME;
  reply_port_tags[1].ta_Tag = TAG_END;
  reply_port_tags[1].ta_Arg = NULL;
  reply_port = CreateSizedItem(MKNODEID(KERNELNODE, MSGPORTNODE), reply_port_tags, 0);
  g_CAMERA_IO_STATE.reply_port = reply_port;
  if(reply_port < 0)
    return reply_port;

  io_request_tags[CAMERA_IO_REQUEST_DEVICE_TAG_INDEX].ta_Tag     = CREATEIOREQ_TAG_DEVICE;
  io_request_tags[CAMERA_IO_REQUEST_DEVICE_TAG_INDEX].ta_Arg     = (void *) device;
  io_request_tags[CAMERA_IO_REQUEST_REPLY_PORT_TAG_INDEX].ta_Tag = CREATEIOREQ_TAG_REPLYPORT;
  io_request_tags[CAMERA_IO_REQUEST_REPLY_PORT_TAG_INDEX].ta_Arg = (void *) reply_port;
  io_request_tags[CAMERA_IO_REQUEST_END_TAG_INDEX].ta_Tag        = TAG_END;
  io_request_tags[CAMERA_IO_REQUEST_END_TAG_INDEX].ta_Arg        = NULL;
  io_request = CreateSizedItem(MKNODEID(KERNELNODE, IOREQNODE), io_request_tags, 0);
  g_CAMERA_IO_STATE.io_request = io_request;
  if(io_request < 0)
    return io_request;

  g_CAMERA_IO_STATE.io_request_object = LookupItem(io_request);
  return 1;
}
