#include "thread_creation.h"

#include "diagnostics.h"

#include "item.h"
#include "kernel.h"
#include "kernelnodes.h"
#include "operror.h"
#include "task.h"

#define THREAD_ERROR_TEXT_BYTE_COUNT 256
#define THREAD_MAX_QUANTUM 0x4000
#define THREAD_PRIORITY_TAG_INDEX 0
#define THREAD_QUANTUM_TAG_INDEX 1
#define THREAD_ENTRY_TAG_INDEX 2
#define THREAD_STACK_SIZE_TAG_INDEX 3
#define THREAD_STACK_POINTER_TAG_INDEX 4
#define THREAD_ARGUMENT_COUNT_TAG_INDEX 5
#define THREAD_ARGUMENT_POINTER_TAG_INDEX 6
#define THREAD_NAME_TAG_INDEX 7
#define THREAD_END_TAG_INDEX 8
#define THREAD_TAG_COUNT 9

void
create_thread_with_stack(const char          *name_,
                         int32                priority_offset_,
                         ThreadEntryPoint     entry_point_,
                         void                *stack_,
                         int32                stack_byte_count_,
                         ThreadCreationState *state_)
{
  char   error_text[THREAD_ERROR_TEXT_BYTE_COUNT];
  TagArg tags[THREAD_TAG_COUNT];

  tags[THREAD_PRIORITY_TAG_INDEX].ta_Tag         = TAG_ITEM_PRI;
  tags[THREAD_PRIORITY_TAG_INDEX].ta_Arg         = NULL;
  tags[THREAD_QUANTUM_TAG_INDEX].ta_Tag          = CREATETASK_TAG_MAXQ;
  tags[THREAD_QUANTUM_TAG_INDEX].ta_Arg          = (void *) (uint32) THREAD_MAX_QUANTUM;
  tags[THREAD_ENTRY_TAG_INDEX].ta_Tag            = CREATETASK_TAG_PC;
  tags[THREAD_ENTRY_TAG_INDEX].ta_Arg            = NULL;
  tags[THREAD_STACK_SIZE_TAG_INDEX].ta_Tag       = CREATETASK_TAG_STACKSIZE;
  tags[THREAD_STACK_SIZE_TAG_INDEX].ta_Arg       = NULL;
  tags[THREAD_STACK_POINTER_TAG_INDEX].ta_Tag    = CREATETASK_TAG_SP;
  tags[THREAD_STACK_POINTER_TAG_INDEX].ta_Arg    = NULL;
  tags[THREAD_ARGUMENT_COUNT_TAG_INDEX].ta_Tag   = CREATETASK_TAG_ARGC;
  tags[THREAD_ARGUMENT_COUNT_TAG_INDEX].ta_Arg   = NULL;
  tags[THREAD_ARGUMENT_POINTER_TAG_INDEX].ta_Tag = CREATETASK_TAG_ARGP;
  tags[THREAD_ARGUMENT_POINTER_TAG_INDEX].ta_Arg = NULL;
  tags[THREAD_NAME_TAG_INDEX].ta_Tag             = TAG_ITEM_NAME;
  tags[THREAD_NAME_TAG_INDEX].ta_Arg             = NULL;
  tags[THREAD_END_TAG_INDEX].ta_Tag              = TAG_END;
  tags[THREAD_END_TAG_INDEX].ta_Arg              = NULL;

  if(stack_ == NULL)
    fatal_error("spawnthread - YOU MUST PASS A STACK POINTER\n");

  state_->stack    = stack_;
  state_->priority = KernelBase->kb_CurrentTask->t.n_Priority + priority_offset_;
  tags[THREAD_PRIORITY_TAG_INDEX].ta_Arg      = (void *) (uint32) state_->priority;
  tags[THREAD_ENTRY_TAG_INDEX].ta_Arg         = (void *) entry_point_;
  tags[THREAD_STACK_SIZE_TAG_INDEX].ta_Arg    = (void *) (uint32) stack_byte_count_;
  tags[THREAD_STACK_POINTER_TAG_INDEX].ta_Arg = (uint8 *) state_->stack + stack_byte_count_;
  tags[THREAD_NAME_TAG_INDEX].ta_Arg          = (void *) name_;

  state_->item = CreateSizedItem(MKNODEID(KERNELNODE, TASKNODE), tags, 0);
  if(state_->item < 0)
    {
      GetSysErr(error_text, THREAD_ERROR_TEXT_BYTE_COUNT - 1, state_->item);
      fatal_error("spawnthread - CREATEITEM FOR THREAD FAILED\n"
                  "ERROR #%d: %s",
                  state_->item,
                  error_text);
    }
}
