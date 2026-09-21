#ifndef KSHIM_LIST_H
#define KSHIM_LIST_H
#include <stddef.h>
struct list_head {
	struct list_head *next, *prev;
};
static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}
static inline void list_add_tail(struct list_head *entry, struct list_head *head)
{
	entry->prev = head->prev;
	entry->next = head;
	head->prev->next = entry;
	head->prev = entry;
}
static inline void list_del(struct list_head *entry)
{
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;
	entry->next = NULL;
	entry->prev = NULL;
}
#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr) - offsetof(type, member)))
#define list_for_each_entry(pos, head, member)                              \
	for (pos = list_entry((head)->next, __typeof__(*pos), member);      \
	     &pos->member != (head);                                        \
	     pos = list_entry(pos->member.next, __typeof__(*pos), member))
#endif
