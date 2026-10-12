#include "doubly_linked_list.h"

#include "memory.h"

#define DOUBLY_LINKED_LIST_ALLOCATION_NAME "ListPtr"

void
insert_list_node_before(DoublyLinkedNode *anchor_, DoublyLinkedNode *node_)
{
  node_->previous         = anchor_->previous;
  node_->next             = anchor_;
  anchor_->previous->next = node_;
  anchor_->previous       = node_;
}

void
insert_list_node_after(DoublyLinkedNode *anchor_, DoublyLinkedNode *node_)
{
  anchor_->next->previous = node_;
  node_->next             = anchor_->next;
  anchor_->next           = node_;
  node_->previous         = anchor_;
}

void
prepend_list_node(DoublyLinkedList *list_, DoublyLinkedNode *node_)
{
  insert_list_node_after(list_->head_sentinel, node_);
}

void
remove_list_node(DoublyLinkedNode *node_)
{
  DoublyLinkedNode *next;
  DoublyLinkedNode *previous;

  next           = node_->next;
  previous       = node_->previous;
  previous->next = next;
  next->previous = previous;
}

void
append_list_node(DoublyLinkedList *list_, DoublyLinkedNode *node_)
{
  insert_list_node_before(list_->tail_sentinel, node_);
}

DoublyLinkedNode *
remove_first_list_node(DoublyLinkedList *list_)
{
  DoublyLinkedNode *node;

  node = list_->head_sentinel->next;
  if(node == list_->tail_sentinel)
    {
      return NULL;
    }
  remove_list_node(node);
  return node;
}

DoublyLinkedNode *
remove_last_list_node(DoublyLinkedList *list_)
{
  DoublyLinkedNode *node;

  if(list_->head_sentinel->next == list_->tail_sentinel)
    {
      return NULL;
    }
  node = list_->tail_sentinel->previous;
  remove_list_node(node);
  return node;
}

void
move_all_list_nodes(DoublyLinkedList *source_, DoublyLinkedList *destination_)
{
  DoublyLinkedNode *next;
  DoublyLinkedNode *node;

  node = source_->head_sentinel->next;
  while(node->next != NULL)
    {
      next = node->next;
      remove_list_node(node);
      append_list_node(destination_, node);
      node = next;
    }
}

void
free_empty_list(DoublyLinkedList *list_)
{
  if(list_->head_sentinel->next == list_->tail_sentinel)
    {
      free_memory(list_);
    }
}

DoublyLinkedNode *
find_list_tail_sentinel(const DoublyLinkedList *list_)
{
  DoublyLinkedNode *node;

  node = list_->head_sentinel->next;
  while(node->next != NULL)
    node = node->next;

  return node;
}

DoublyLinkedListStorage *
initialize_doubly_linked_list_storage(DoublyLinkedListStorage *storage_)
{
  if(storage_ == NULL)
    {
      storage_ =
        (DoublyLinkedListStorage *) allocate_named_memory(DOUBLY_LINKED_LIST_ALLOCATION_NAME,
                                                          sizeof(DoublyLinkedListStorage),
                                                          0);
      if(storage_ == NULL)
        return NULL;
      storage_->allocation = NULL;
    }

  storage_->list.head_sentinel     = &storage_->head_sentinel;
  storage_->list.tail_sentinel     = &storage_->tail_sentinel;
  storage_->head_sentinel.next     = &storage_->tail_sentinel;
  storage_->tail_sentinel.previous = &storage_->head_sentinel;
  storage_->head_sentinel.previous = NULL;
  storage_->tail_sentinel.next     = NULL;
  return storage_;
}

void
insert_list_node_sorted(DoublyLinkedList *list_,
                        DoublyLinkedNode *node_,
                        uint32            key_offset_,
                        uint8             ascending_)
{
  DoublyLinkedNode *current;
  int32             current_key;
  int32             node_key;

  current  = list_->head_sentinel->next;
  node_key = *(int32 *) ((uint8 *) node_ + key_offset_);
  while(current->next != NULL)
    {
      current_key = *(int32 *) ((uint8 *) current + key_offset_);
      if((ascending_ && (current_key > node_key)) || ((!ascending_) && (current_key < node_key)))
        {
          insert_list_node_before(current, node_);
          return;
        }
      current = current->next;
    }
  append_list_node(list_, node_);
}

void
sort_list_nodes_by_key(DoublyLinkedList *list_, uint32 key_offset_)
{
  DoublyLinkedNode *current;
  DoublyLinkedNode *next;
  int32             current_key;
  int32             next_key;

  current = list_->head_sentinel->next;
  if(current == NULL)
    return;
  next = current->next;
  if((next == NULL) || (next->next == NULL))
    return;

  while(next->next != NULL)
    {
      current_key = *(int32 *) ((uint8 *) current + key_offset_);
      next_key    = *(int32 *) ((uint8 *) next + key_offset_);
      if(next_key < current_key)
        {
          remove_list_node(next);
          insert_list_node_before(current, next);
          if(next == list_->head_sentinel->next)
            next = current->next;
          else
            current = next->previous;
        }
      else
        {
          current = next;
          next    = next->next;
        }
    }
}

void
rebuild_list_sorted_by_key(DoublyLinkedListStorage *storage_, uint32 key_offset_, uint8 ascending_)
{
  DoublyLinkedListStorage temporary;
  DoublyLinkedNode       *next;
  DoublyLinkedNode       *node;

  node = storage_->list.head_sentinel->next;
  (void) initialize_doubly_linked_list_storage(&temporary);
  temporary.list.head_sentinel->next           = storage_->list.head_sentinel->next;
  temporary.list.tail_sentinel->previous       = storage_->list.tail_sentinel->previous;
  temporary.list.head_sentinel->next->previous = temporary.list.head_sentinel;
  temporary.list.tail_sentinel->previous->next = temporary.list.tail_sentinel;
  (void) initialize_doubly_linked_list_storage(storage_);

  while(node->next != NULL)
    {
      next = node->next;
      insert_list_node_sorted(&storage_->list, node, key_offset_, ascending_);
      node = next;
    }
}

void
sort_list_nodes_by_two_keys(DoublyLinkedList *list_,
                            uint32            primary_key_offset_,
                            uint32            secondary_key_offset_)
{
  DoublyLinkedNode *current;
  DoublyLinkedNode *next;
  int32             current_primary_key;
  int32             current_secondary_key;
  int32             next_primary_key;
  int32             next_secondary_key;
  int32             out_of_order;
  int32             swapped;

  do
    {
      swapped = 0;
      current = list_->head_sentinel->next;
      next    = current->next;
      if(next == NULL)
        return;
      while(next->next != NULL)
        {
          current_primary_key = *(int32 *) ((uint8 *) current + primary_key_offset_);
          next_primary_key    = *(int32 *) ((uint8 *) next + primary_key_offset_);
          out_of_order        = (current_primary_key > next_primary_key);
          if(current_primary_key == next_primary_key)
            {
              current_secondary_key = *(int32 *) ((uint8 *) current + secondary_key_offset_);
              next_secondary_key    = *(int32 *) ((uint8 *) next + secondary_key_offset_);
              out_of_order          = (current_secondary_key > next_secondary_key);
            }

          if(out_of_order != 0)
            {
              remove_list_node(next);
              insert_list_node_before(current, next);
              swapped = 1;
              next    = current->next;
            }
          else
            {
              current = next;
              next    = next->next;
            }
        }
    }
  while(swapped != 0);
}
