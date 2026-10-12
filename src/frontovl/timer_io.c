#include "timer_io.h"

#include "device.h"
#include "io.h"
#include "item.h"
#include "kernel.h"
#include "kernelnodes.h"
#include "tags.h"

#define TIMER_DEVICE_NAME "timer"

enum
{
  TIMER_IO_TAG_COUNT = 2
};

Item
create_frontend_timer_io_request(void)
{
  TagArg tags[TIMER_IO_TAG_COUNT];
  Item   timer_device;

  timer_device = FindNamedItem(MKNODEID(KERNELNODE, DEVICENODE), TIMER_DEVICE_NAME);
  if(timer_device < 0)
    return timer_device;

  if(IsItemOpened(KernelBase->kb_CurrentTask->t.n_Item, timer_device) != 0)
    {
      timer_device = OpenItem(timer_device, NULL);
      if(timer_device < 0)
        return timer_device;
    }

  tags[0].ta_Tag = CREATEIOREQ_TAG_DEVICE;
  tags[0].ta_Arg = (void *) timer_device;
  tags[1].ta_Tag = TAG_END;

  return CreateSizedItem(MKNODEID(KERNELNODE, IOREQNODE), tags, 0);
}
