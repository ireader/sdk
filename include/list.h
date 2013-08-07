#ifndef _list_h_
#define _list_h_

struct list_head
{
	struct list_head *next, *prev;
};

inline void list_insert_after(struct list_head *new, struct list_head *head)
{
	struct list_head *prev, *next;
	prev = head;
	next = head->next;

	new->prev = prev;
	new->next = next;
	next->prev = new;
	prev->next = new;
}

inline void list_insert_before(struct list_head *new, struct list_head *head)
{
	struct list_head *prev, *next;
	prev = head->prev;
	next = head;

	new->prev = prev;
	new->next = next;
	next->prev = new;
	prev->next = new;
}

inline void list_remove(struct list_head *head)
{
	struct list_head *prev, *next;
	prev = head->prev;
	next = head->next;

	prev->next = next;
	next->prev = prev;

	head->prev = head->next = 0;
}

inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}

#define LIST_INIT_HEAD(list) do { (list)->next = (list)->prev = (list); } while (0)

#define list_entry(ptr, type, member) \
	(type*)((char*)ptr-(unsigned long)(&((type*)0)->member))

#define list_for_each(pos, head) \
	for(pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_safe(pos, n, head) \
	for(pos = (head)->next, n = pos->next; pos != (head); pos = n, n=pos->next)

#endif /* !_list_h_ */
