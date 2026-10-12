#include "types.h"

/* These linker-visible leaf entry points have no direct C callers. */
int32
return_one_leaf_primary(void);
void
empty_leaf_primary(void);
void
empty_leaf_secondary(void);
void
empty_leaf_tertiary(void);
void
empty_leaf_quaternary(void);
int32
return_one_leaf_secondary(void);
void
empty_leaf_quinary(void);
void
empty_leaf_senary(void);
void
empty_leaf_septenary(void);

int32
return_one_leaf_primary(void)
{
  return 1;
}

void
empty_leaf_primary(void)
{
}

void
empty_leaf_secondary(void)
{
}

void
empty_leaf_tertiary(void)
{
}

void
empty_leaf_quaternary(void)
{
}

int32
return_one_leaf_secondary(void)
{
  return 1;
}

void
empty_leaf_quinary(void)
{
}

void
empty_leaf_senary(void)
{
}

void
empty_leaf_septenary(void)
{
}
