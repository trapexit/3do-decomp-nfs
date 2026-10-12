#pragma once

#include "doubly_linked_list.h"

typedef struct PooledListNode PooledListNode;

struct PooledListNode
{
  DoublyLinkedNode  links;
  DoublyLinkedList *owner_list;
};

PooledListNode *
acquire_pooled_node(DoublyLinkedList *free_list_);
void
release_pooled_node(PooledListNode *node_);
DoublyLinkedListStorage *
create_linked_record_pool(int32 record_bytes_, int32 record_count_, int32 flags_);
void
destroy_linked_record_pool(DoublyLinkedListStorage *pool_);
void
release_all_pooled_nodes(DoublyLinkedList *list_);
