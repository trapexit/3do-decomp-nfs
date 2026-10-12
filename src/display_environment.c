#include "display_environment.h"

#include "camera_io.h"

#include "device.h"
#include "io.h"
#include "item.h"

#define SPORT_DEVICE_NAME "SPORT"
#define SPORT_IO_REQUEST_COUNT 2
#define SPORT_IO_REQUEST_TAG_COUNT 2

Item  g_SPORT_IO_REQUESTS[SPORT_IO_REQUEST_COUNT];
Item  g_SPORT_DEVICE;
int32 g_SPORT_STATE_PADDING;

int32
initialize_display_environment(void)
{
  Item   device;
  Item   io_request;
  int32  index;
  TagArg io_request_tags[SPORT_IO_REQUEST_TAG_COUNT];

  device         = FindDevice(SPORT_DEVICE_NAME);
  device         = OpenItem(device, NULL);
  g_SPORT_DEVICE = device;
  if(device < 0)
    return device;

  io_request_tags[0].ta_Tag = CREATEIOREQ_TAG_DEVICE;
  io_request_tags[0].ta_Arg = (void *) device;
  io_request_tags[1].ta_Tag = TAG_END;
  io_request_tags[1].ta_Arg = NULL;

  index = SPORT_IO_REQUEST_COUNT;
  while(index > 0)
    {
      --index;
      io_request = CreateSizedItem(MKNODEID(KERNELNODE, IOREQNODE), io_request_tags, 0);
      g_SPORT_IO_REQUESTS[index] = io_request;
      if(io_request < 0)
        return io_request;
    }

  return 1;
}
