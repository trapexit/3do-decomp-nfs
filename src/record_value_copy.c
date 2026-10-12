#include "record_value_copy.h"

#define RECORD_VALUE_GROUP_TYPE 3
#define RECORD_VALUE_NODE_TYPE_FIRST 0x1A
#define RECORD_VALUE_NODE_TYPE_SECOND 0x1B
#define RECORD_VALUE_NODE_TYPE_THIRD 0x1D

RecordValueBlock g_CURRENT_RECORD_VALUES;

void
copy_matching_record_values(const RecordValueGroup *group_)
{
  const RecordValueNode *node;
  int32                  value_index;

  if(group_->type != RECORD_VALUE_GROUP_TYPE)
    return;

  node = (const RecordValueNode *) (group_ + 1);
  while(node->byte_count != 0)
    {
      if((node->type == RECORD_VALUE_NODE_TYPE_FIRST) ||
         (node->type == RECORD_VALUE_NODE_TYPE_SECOND) ||
         (node->type == RECORD_VALUE_NODE_TYPE_THIRD))
        {
          for(value_index = 0; value_index < RECORD_VALUE_COUNT; value_index++)
            g_CURRENT_RECORD_VALUES.values[value_index] = node->values[value_index];
        }

      node = (const RecordValueNode *) ((const uint8 *) node + node->byte_count);
    }
}
