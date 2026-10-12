/* Frontovl wrapper for reclaiming reclaimable kernel memory. */

#include "scavenge_kernel_memory.h"

#include "mem.h"

int32
_ScavengeMem(void);

int32
scavenge_kernel_memory(void)
{
  return _ScavengeMem();
}
