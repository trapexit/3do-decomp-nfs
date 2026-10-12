#include "memory_status.h"

#include "mem.h"


uint32 g_SYSTEM_FREE_MEMORY_BYTES;
uint32 g_TASK_FREE_MEMORY_BYTES;

void
update_free_memory_counts(void)
{
  MemInfo memory_info;

  (void) ScavengeMem();
  availMem(&memory_info, MEMTYPE_DRAM);
  g_SYSTEM_FREE_MEMORY_BYTES = memory_info.minfo_SysFree;
  g_TASK_FREE_MEMORY_BYTES   = memory_info.minfo_TaskFree;
}
