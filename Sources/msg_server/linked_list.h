#ifndef linked_list_h
#define linked_list_h

#include <stdatomic.h>
#include <stdbool.h>

typedef struct list_node_instance_t *list_node_t;

list_node_t list_insert(_Atomic(list_node_t) *head, void *data);
size_t list_count(_Atomic(list_node_t) *head);
list_node_t list_find(_Atomic(list_node_t) *head, void *criteria, bool (*comparator)(const void *, const void *));
void *list_data(list_node_t node);
#endif // linked_list_h
