
#ifndef _MIN_HEAP_H_
#define _MIN_HEAP_H_

#include "event2/event.h"
#include "event2/util.h"

typedef struct min_heap
{
	struct event** p;    //ָ��event*��������
	unsigned n, a;    //n��Ԫ�ظ�����a��ʾpָ���ڴ�ĳߴ�(�����������)
} min_heap_t;

static inline void           min_heap_ctor(min_heap_t* s);
static inline void           min_heap_dtor(min_heap_t* s);
static inline void           min_heap_elem_init(struct event* e);
static inline int            min_heap_elem_greater(struct event *a, struct event *b);
static inline int            min_heap_empty(min_heap_t* s);
static inline unsigned       min_heap_size(min_heap_t* s);
static inline struct event*  min_heap_top(min_heap_t* s);
static inline int            min_heap_reserve(min_heap_t* s, unsigned n);
static inline int            min_heap_push(min_heap_t* s, struct event* e);
static inline struct event*  min_heap_pop(min_heap_t* s);
static inline int            min_heap_erase(min_heap_t* s, struct event* e);
static inline void           min_heap_shift_up_(min_heap_t* s, unsigned hole_index, struct event* e);
static inline void           min_heap_shift_down_(min_heap_t* s, unsigned hole_index, struct event* e);

int min_heap_elem_greater(struct event *a, struct event *b)   //����
{
	return evutil_timercmp(&a->ev_timeout, &b->ev_timeout, >);
}

void min_heap_ctor(min_heap_t* s) { s->p = 0; s->n = 0; s->a = 0; }   //���캯�������ʼ��
void min_heap_dtor(min_heap_t* s) { free(s->p); }   //��������
void min_heap_elem_init(struct event* e) { e->min_heap_idx = -1; }
int min_heap_empty(min_heap_t* s) { return 0u == s->n; }
unsigned min_heap_size(min_heap_t* s) { return s->n; }
struct event* min_heap_top(min_heap_t* s) { return s->n ? *s->p : 0; }   //�Ѷ�Ԫ��

int min_heap_push(min_heap_t* s, struct event* e)   //���
{
	if (min_heap_reserve(s, s->n + 1))
		return -1;
	min_heap_shift_up_(s, s->n++, e);   //s->n++ֵ��ע��
	return 0;
}

struct event* min_heap_pop(min_heap_t* s)   //����
{
	if (s->n)
	{
		struct event* e = *s->p;
		min_heap_shift_down_(s, 0u, s->p[--s->n]);
		e->min_heap_idx = -1;
		return e;
	}
	return 0;
}

int min_heap_erase(min_heap_t* s, struct event* e)   //�Ӷ���ɾ��ĳ�¼�
{
	if (((unsigned int)-1) != e->min_heap_idx)   //�ж��±�
	{
		struct event *last = s->p[--s->n];
		unsigned parent = (e->min_heap_idx - 1) / 2;
		/* we replace e with the last element in the heap.  We might need to
		shift it upward if it is less than its parent, or downward if it is
		greater than one or both its children. Since the children are known
		to be less than the parent, it can't need to shift both up and
		down. */
		if (e->min_heap_idx > 0 && min_heap_elem_greater(s->p[parent], last))
			min_heap_shift_up_(s, e->min_heap_idx, last);  //���һ��С����ֱ��shift_up
		else
			min_heap_shift_down_(s, e->min_heap_idx, last);
		e->min_heap_idx = -1;
		return 0;
	}
	return -1;
}

int min_heap_reserve(min_heap_t* s, unsigned n)  //nԪ�ظ���
{
	if (s->a < n)
	{
		struct event** p;
		unsigned a = s->a ? s->a * 2 : 8;   //�����Ҫô��ʼ��Ϊ8��Ҫôÿ�γ��Զ�
		if (a < n)   //�Ծɲ��㣬�����n
			a = n;
		if (!(p = (struct event**)realloc(s->p, a * sizeof *p)))
			return -1;
		s->p = p;
		s->a = a;
	}
	return 0;
}

void min_heap_shift_up_(min_heap_t* s, unsigned hole_index, struct event* e)  //��ͻ�ƿڿ�ʼ����shift���ҵ�����λ�ò���e
{
	unsigned parent = (hole_index - 1) / 2;
	while (hole_index && min_heap_elem_greater(s->p[parent], e))   //ֻ�д��ڸ��ڵ��ѭ��
	{
		(s->p[hole_index] = s->p[parent])->min_heap_idx = hole_index;  //����λ��
		hole_index = parent;
		parent = (hole_index - 1) / 2;
	}
	(s->p[hole_index] = e)->min_heap_idx = hole_index;
}

void min_heap_shift_down_(min_heap_t* s, unsigned hole_index, struct event* e)  //ɾ��ͻ�ƿڣ���ͻ�ƿڿ�ʼ����shift��
{
	unsigned min_child = 2 * (hole_index + 1);   //���㿪ʼ������0��1��2���ɣ�min_child���±�Ϊ�Һ����±�
	while (min_child <= s->n)   //�ѵ��µ�ÿ��ȡ������������С���Ǹ�����Ϊ��Ҫ��֤��parent����λ�õ�childҪ������һ��childС
	{                   //���min_child==s->n��������ʽһ��Ϊ�棬min_child -= 1���������Ǳ�֤�ѵ�ĩβ����ֻ�����ӣ����Һ��ӣ��������min_child����-1��
		min_child -= min_child == s->n || min_heap_elem_greater(s->p[min_child], s->p[min_child - 1]); //�ҳ�����child�н�С��
		if (!(min_heap_elem_greater(e, s->p[min_child])))  //e>p[min_child]����ѭ��
			break;
		(s->p[hole_index] = s->p[min_child])->min_heap_idx = hole_index;
		hole_index = min_child;
		min_child = 2 * (hole_index + 1);  //min_child���±꣬��������ֱ����2*(hole_Index+1)�����û���Һ��ӣ�����±�ָ����һ��������Һ��ӣ���Ҫ�������1����
	}
	min_heap_shift_up_(s, hole_index, e);//������ƶ�������һ����ȱ���������µĽڵ�嵽��ȱ�����������봦��
}    //һ��ʼ���ھ���ΪʲôҪ�������봦������������������ʵ�������㣬ֱ�� (s->p[hole_index] = e)->min_heap_idx = hole_index;����
	 //��Ϊ����ѭ���е�if(!condition)�Ѿ���֤e operator> parent�ˣ���������shift_up��
#endif /* _MIN_HEAP_H_ */