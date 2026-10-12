#include "kernel_diagnostics.h"

#include "debug.h"

void
print_unknown_control_chunk(const char *format_, const char *chunk_name_, uint32 chunk_type_)
{
  kprintf(format_, chunk_name_, chunk_type_);
}
