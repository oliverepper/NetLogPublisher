#include "linked_list.h"
#include <stdatomic.h>
#include <stdlib.h>

struct list_node_instance_t {
  void *data;
  struct list_node_instance_t *next;
};

list_node_t list_insert(_Atomic(list_node_t) *head, void *data) {
  list_node_t new_node = calloc(1, sizeof(struct list_node_instance_t));
  if (!new_node)
    return NULL;

  new_node->data = data;

  do {
    new_node->next = atomic_load(head);
  } while (!atomic_compare_exchange_weak(head, &new_node->next, new_node));

  return new_node;
}

size_t list_count(_Atomic(list_node_t) *head) {
  size_t count = 0;
  list_node_t current = atomic_load(head);
  while (current) {
    count++;
    current = current->next;
  }

  return count;
}

list_node_t list_find(_Atomic(list_node_t) *head, void *criteria,
                      bool (*comparator)(const void *, const void *)) {
  list_node_t current = atomic_load(head);
  while (current) {
    if (comparator(criteria, current->data))
      return current;
    else
      current = current->next;
  }

  return NULL;
}

void *list_data(list_node_t node) {
  if (!node)
    return NULL;

  return node->data;
}

