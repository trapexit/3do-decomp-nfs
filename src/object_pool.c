#include "object_pool.h"

#include "memory.h"

#define LINKED_RECORD_MEMORY_NAME "FreeObjList"
#define LINKED_RECORD_MEMORY_FLAGS 0x20

PooledListNode *
acquire_pooled_node(DoublyLinkedList *free_list_)
{
  return (PooledListNode *) remove_last_list_node(free_list_);
}

void
release_pooled_node(PooledListNode *node_)
{
  remove_list_node(&node_->links);
  append_list_node(node_->owner_list, &node_->links);
}

DoublyLinkedListStorage *
create_linked_record_pool(int32 record_bytes_, int32 record_count_, int32 flags_)
{
  int32                    index;
  DoublyLinkedListStorage *pool;
  uint8                   *record;

  (void) flags_;

  record = (uint8 *) allocate_named_memory(LINKED_RECORD_MEMORY_NAME,
                                           record_bytes_ * record_count_,
                                           LINKED_RECORD_MEMORY_FLAGS);
  if(record == NULL)
    return NULL;

  pool = initialize_doubly_linked_list_storage(NULL);
  if(pool == NULL)
    return NULL;

  pool->allocation = record;
  for(index = 0; index < record_count_; index++)
    {
      append_list_node(&pool->list, &((PooledListNode *) record)->links);
      ((PooledListNode *) record)->owner_list = &pool->list;
      record += record_bytes_;
    }
  return pool;
}

void
destroy_linked_record_pool(DoublyLinkedListStorage *pool_)
{
  free_memory(pool_->allocation);
  initialize_doubly_linked_list_storage(pool_);
  pool_->allocation = NULL;
}

void
release_all_pooled_nodes(DoublyLinkedList *list_)
{
  DoublyLinkedNode *next;
  DoublyLinkedNode *node;
  PooledListNode   *pooled_node;

  node = list_->head_sentinel->next;
  while(node->next != NULL)
    {
      next        = node->next;
      pooled_node = (PooledListNode *) node;
      remove_list_node(node);
      append_list_node(pooled_node->owner_list, node);
      node = next;
    }
}
