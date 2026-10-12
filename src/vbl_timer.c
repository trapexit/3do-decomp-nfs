#include "vbl_timer.h"

#include "graphics.h"

#define VBL_SIGN_BIT 0x80000000U

uint32 g_PREVIOUS_VBL_NUMBER = 0;
uint32 g_RENDER_VBL_DEADLINE = 0;

uint32
get_vbl_number(void)
{
  return GrafBase->gf_VBLNumber;
}

uint32
get_render_vbl_number(void)
{
  return GrafBase->gf_VBLNumber;
}

void
schedule_render_vbl_deadline(uint32 vbl_count_)
{
  g_RENDER_VBL_DEADLINE = (get_render_vbl_number() + vbl_count_);
}

void
wait_for_render_vbl_deadline(void)
{
  while(is_render_vbl_deadline_reached() == 0)
    {
    }
}

int32
is_render_vbl_deadline_reached(void)
{
  uint32 elapsed;

  elapsed = (get_render_vbl_number() - g_RENDER_VBL_DEADLINE);
  return elapsed < VBL_SIGN_BIT;
}

void
wait_for_render_vbl_count(uint32 vbl_count_)
{
  uint32 current_vbl;
  uint32 deadline;

  deadline = (get_render_vbl_number() + vbl_count_);
  do
    {
      current_vbl = get_render_vbl_number();
    }
  while((current_vbl - deadline) >= VBL_SIGN_BIT);
}

uint32
sample_vbl_delta(void)
{
  uint32 current_vbl_number;
  uint32 previous_vbl_number;

  current_vbl_number    = get_vbl_number();
  previous_vbl_number   = g_PREVIOUS_VBL_NUMBER;
  g_PREVIOUS_VBL_NUMBER = current_vbl_number;
  return current_vbl_number - previous_vbl_number;
}
