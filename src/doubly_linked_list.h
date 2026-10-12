#pragma once

#include "types.h"

typedef struct DoublyLinkedNode        DoublyLinkedNode;
typedef struct DoublyLinkedList        DoublyLinkedList;
typedef struct DoublyLinkedListStorage DoublyLinkedListStorage;

struct DoublyLinkedNode
{
  DoublyLinkedNode *next;
  DoublyLinkedNode *previous;
};

struct DoublyLinkedList
{
  DoublyLinkedNode *head_sentinel;
  DoublyLinkedNode *tail_sentinel;
};

struct DoublyLinkedListStorage
{
  DoublyLinkedList list;
  DoublyLinkedNode head_sentinel;
  uint8            reserved_10[4];
  DoublyLinkedNode tail_sentinel;
  uint8            reserved_1_c[4];
  void            *allocation;
};

void
insert_list_node_before(DoublyLinkedNode *anchor_, DoublyLinkedNode *node_);
void
insert_list_node_after(DoublyLinkedNode *anchor_, DoublyLinkedNode *node_);
void
prepend_list_node(DoublyLinkedList *list_, DoublyLinkedNode *node_);
void
remove_list_node(DoublyLinkedNode *node_);
void
append_list_node(DoublyLinkedList *list_, DoublyLinkedNode *node_);
DoublyLinkedNode *
remove_first_list_node(DoublyLinkedList *list_);
DoublyLinkedNode *
remove_last_list_node(DoublyLinkedList *list_);
void
move_all_list_nodes(DoublyLinkedList *source_, DoublyLinkedList *destination_);
void
free_empty_list(DoublyLinkedList *list_);
DoublyLinkedNode *
find_list_tail_sentinel(const DoublyLinkedList *list_);
DoublyLinkedListStorage *
initialize_doubly_linked_list_storage(DoublyLinkedListStorage *storage_);
void
insert_list_node_sorted(DoublyLinkedList *list_,
                        DoublyLinkedNode *node_,
                        uint32            key_offset_,
                        uint8             ascending_);
void
sort_list_nodes_by_key(DoublyLinkedList *list_, uint32 key_offset_);
void
rebuild_list_sorted_by_key(DoublyLinkedListStorage *storage_, uint32 key_offset_, uint8 ascending_);
void
sort_list_nodes_by_two_keys(DoublyLinkedList *list_,
                            uint32            primary_key_offset_,
                            uint32            secondary_key_offset_);
