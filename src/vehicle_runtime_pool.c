#include "vehicle_runtime_pool.h"

#include "memory.h"
#include "object_pool.h"
#include "signal_status.h"

#define VEHICLE_RUNTIME_RECORD_BYTES 0x4e4
#define VEHICLE_RUNTIME_RECORD_COUNT 7
#define VEHICLE_RUNTIME_CLEARED_VALUE 0

const char G_TSUPRA_VEHICLE_RESOURCE_IDENTIFIER[8]  = "TSupra";
const char G_LDIABLO_VEHICLE_RESOURCE_IDENTIFIER[8] = "LDiablo";
const char G_P911_VEHICLE_RESOURCE_IDENTIFIER[8]    = "P911";
const char G_CZ_R1_VEHICLE_RESOURCE_IDENTIFIER[8]   = "CZR1";
const char G_F512_TR_VEHICLE_RESOURCE_IDENTIFIER[8] = "F512TR";
const char G_DVIPER_VEHICLE_RESOURCE_IDENTIFIER[8]  = "DVIPER";
const char G_ANSX_VEHICLE_RESOURCE_IDENTIFIER[8]    = "ANSX";
const char G_MR_X7_VEHICLE_RESOURCE_IDENTIFIER[8]   = "MRX7";

VehicleRuntimePoolState g_VEHICLE_RUNTIME_POOL_STATE = { { G_TSUPRA_VEHICLE_RESOURCE_IDENTIFIER,
                                                           G_LDIABLO_VEHICLE_RESOURCE_IDENTIFIER,
                                                           G_P911_VEHICLE_RESOURCE_IDENTIFIER,
                                                           G_CZ_R1_VEHICLE_RESOURCE_IDENTIFIER,
                                                           G_F512_TR_VEHICLE_RESOURCE_IDENTIFIER,
                                                           G_DVIPER_VEHICLE_RESOURCE_IDENTIFIER,
                                                           G_ANSX_VEHICLE_RESOURCE_IDENTIFIER,
                                                           G_MR_X7_VEHICLE_RESOURCE_IDENTIFIER },
                                                         VEHICLE_RUNTIME_CLEARED_VALUE,
                                                         { VEHICLE_RUNTIME_CLEARED_VALUE },
                                                         VEHICLE_RUNTIME_CLEARED_VALUE,
                                                         VEHICLE_RUNTIME_CLEARED_VALUE,
                                                         NULL,
                                                         { VEHICLE_RUNTIME_CLEARED_VALUE },
                                                         NULL,
                                                         NULL,
                                                         NULL,
                                                         { VEHICLE_RUNTIME_CLEARED_VALUE },
                                                         VEHICLE_RUNTIME_CLEARED_VALUE,
                                                         { VEHICLE_RUNTIME_CLEARED_VALUE },
                                                         VEHICLE_RUNTIME_CLEARED_VALUE,
                                                         VEHICLE_RUNTIME_CLEARED_VALUE,
                                                         VEHICLE_RUNTIME_CLEARED_VALUE,
                                                         { VEHICLE_RUNTIME_CLEARED_VALUE },
                                                         VEHICLE_RUNTIME_CLEARED_VALUE,
                                                         { VEHICLE_RUNTIME_CLEARED_VALUE },
                                                         NULL,
                                                         { VEHICLE_RUNTIME_CLEARED_VALUE } };
void                   *g_VEHICLE_RUNTIME_AUXILIARY_ALLOCATION;
int32                   g_VEHICLE_RUNTIME_SOUND_UPDATE_PENDING;

void
initialize_vehicle_runtime_pool(void)
{
  g_VEHICLE_RUNTIME_POOL_STATE.vehicle_record_pool =
    create_linked_record_pool(VEHICLE_RUNTIME_RECORD_BYTES, VEHICLE_RUNTIME_RECORD_COUNT, 0);
}

void
release_vehicle_runtime_objects(void)
{
  DoublyLinkedNode *next;
  DoublyLinkedNode *node;

  node = g_VEHICLE_RUNTIME_POOL_STATE.active_record_list->list.head_sentinel->next;
  while(node->next != NULL)
    {
      next = node->next;
      release_pooled_node((PooledListNode *) node);
      node = next;
    }
}

void
shutdown_vehicle_runtime_pool(void)
{
  if(g_VEHICLE_RUNTIME_AUXILIARY_ALLOCATION != NULL)
    free_memory(g_VEHICLE_RUNTIME_AUXILIARY_ALLOCATION);

  release_all_pooled_nodes(&g_VEHICLE_RUNTIME_POOL_STATE.active_record_list->list);
  destroy_linked_record_pool(g_VEHICLE_RUNTIME_POOL_STATE.vehicle_record_pool);
  free_empty_list(&g_VEHICLE_RUNTIME_POOL_STATE.active_record_list->list);
  free_empty_list(&g_VEHICLE_RUNTIME_POOL_STATE.vehicle_record_pool->list);
  g_VEHICLE_RUNTIME_POOL_STATE.active_record_list  = NULL;
  g_VEHICLE_RUNTIME_POOL_STATE.vehicle_record_pool = NULL;
  free_signal_statuses();
}
