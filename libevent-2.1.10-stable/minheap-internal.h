/*
 * Copyright (c) 2007-2012 Niels Provos and Nick Mathewson
 *
 * Copyright (c) 2006 Maxim Yegorushkin <maxim.yegorushkin@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef MINHEAP_INTERNAL_H_INCLUDED_
#define MINHEAP_INTERNAL_H_INCLUDED_

#include "event2/event-config.h"
#include "evconfig-private.h"
#include "event2/event.h"
#include "event2/event_struct.h"
#include "event2/util.h"
#include "util-internal.h"
#include "mm-internal.h"

typedef struct min_heap
{
	struct event** p;	//指向struct event*的一维数组
	size_t nSize;	//原名是n，表示堆中已有元素数量
	size_t aCapacity;	//原名是a，表示堆结构的总容量
	//size_t n, a;
} min_heap_t;

static inline void	     min_heap_ctor_(min_heap_t* s);
static inline void	     min_heap_dtor_(min_heap_t* s);
static inline void	     min_heap_elem_init_(struct event* e);
static inline int	     min_heap_elt_is_top_(const struct event *e);
static inline int	     min_heap_empty_(min_heap_t* s);
static inline size_t	     min_heap_size_(min_heap_t* s);
static inline struct event*  min_heap_top_(min_heap_t* s);
static inline int	     min_heap_reserve_(min_heap_t* s, size_t n);
static inline int	     min_heap_push_(min_heap_t* s, struct event* e);
static inline struct event*  min_heap_pop_(min_heap_t* s);
static inline int	     min_heap_adjust_(min_heap_t *s, struct event* e);
static inline int	     min_heap_erase_(min_heap_t* s, struct event* e);
static inline void	     min_heap_shift_up_(min_heap_t* s, size_t hole_index, struct event* e);
static inline void	     min_heap_shift_up_unconditional_(min_heap_t* s, size_t hole_index, struct event* e);
static inline void	     min_heap_shift_down_(min_heap_t* s, size_t hole_index, struct event* e);

#define min_heap_elem_greater(a, b) \
	(evutil_timercmp(&(a)->ev_timeout, &(b)->ev_timeout, >))

//构造函数，全元素初始化为0
void min_heap_ctor_(min_heap_t* s) { s->p = 0; s->nSize = 0; s->aCapacity = 0; }
//析构函数，释放堆内存
void min_heap_dtor_(min_heap_t* s) { if (s->p) mm_free(s->p); }
//初始化event超时结构中的堆索引
void min_heap_elem_init_(struct event* e) { e->ev_timeout_pos.min_heap_idx = EV_SIZE_MAX; }
//堆是否有元素
int min_heap_empty_(min_heap_t* s) { return 0 == s->nSize; }
//堆中元素数量
size_t min_heap_size_(min_heap_t* s) { return s->nSize; }
struct event* min_heap_top_(min_heap_t* s) { return s->nSize ? *s->p : 0; }

int min_heap_push_(min_heap_t* s, struct event* e)
{
	if (min_heap_reserve_(s, s->nSize + 1))
		return -1;
	min_heap_shift_up_(s, s->nSize++, e);
	return 0;
}

struct event* min_heap_pop_(min_heap_t* s)
{
	if (s->nSize)
	{
		struct event* e = *s->p;
		min_heap_shift_down_(s, 0, s->p[--s->nSize]);
		e->ev_timeout_pos.min_heap_idx = EV_SIZE_MAX;
		return e;
	}
	return 0;
}

int min_heap_elt_is_top_(const struct event *e)
{
	return e->ev_timeout_pos.min_heap_idx == 0;
}

//在堆中删除事件e
int min_heap_erase_(min_heap_t* s, struct event* e)
{
	if (EV_SIZE_MAX != e->ev_timeout_pos.min_heap_idx)
	{
		struct event *last = s->p[--s->nSize];
		//待删除元素的父节点
		size_t parent = (e->ev_timeout_pos.min_heap_idx - 1) / 2;
		/* we replace e with the last element in the heap.  We might need to
		   shift it upward if it is less than its parent, or downward if it is
		   greater than one or both its children. Since the children are known
		   to be less than the parent, it can't need to shift both up and
		   down. */
		if (e->ev_timeout_pos.min_heap_idx > 0 && min_heap_elem_greater(s->p[parent], last))
			min_heap_shift_up_unconditional_(s, e->ev_timeout_pos.min_heap_idx, last);
		else
			min_heap_shift_down_(s, e->ev_timeout_pos.min_heap_idx, last);
		e->ev_timeout_pos.min_heap_idx = EV_SIZE_MAX;
		return 0;
	}
	return -1;
}

int min_heap_adjust_(min_heap_t *s, struct event *e)
{
	if (EV_SIZE_MAX == e->ev_timeout_pos.min_heap_idx) {
		return min_heap_push_(s, e);
	} else {
		size_t parent = (e->ev_timeout_pos.min_heap_idx - 1) / 2;
		/* The position of e has changed; we shift it up or down
		 * as needed.  We can't need to do both. */
		if (e->ev_timeout_pos.min_heap_idx > 0 && min_heap_elem_greater(s->p[parent], e))
			min_heap_shift_up_unconditional_(s, e->ev_timeout_pos.min_heap_idx, e);
		else
			min_heap_shift_down_(s, e->ev_timeout_pos.min_heap_idx, e);
		return 0;
	}
}
//重分配函数，n表示堆需要的元素数量
int min_heap_reserve_(min_heap_t* s, size_t n)
{
	if (s->aCapacity < n)
	{
		struct event** p;
		size_t a = s->aCapacity ? s->aCapacity * 2 : 8;
		if (a < n)
			a = n;
		if (!(p = (struct event**)mm_realloc(s->p, a * sizeof *p)))
			return -1;
		s->p = p;
		s->aCapacity = a;
	}
	return 0;
}
//
void min_heap_shift_up_unconditional_(min_heap_t* s, size_t hole_index, struct event* e)
{
    size_t parent = (hole_index - 1) / 2;
    do
    {
	(s->p[hole_index] = s->p[parent])->ev_timeout_pos.min_heap_idx = hole_index;
	hole_index = parent;
	parent = (hole_index - 1) / 2;
    } while (hole_index && min_heap_elem_greater(s->p[parent], e));
    (s->p[hole_index] = e)->ev_timeout_pos.min_heap_idx = hole_index;
}

//向上调整hole_index，hole_index是一个“空洞”，e是待调整到合适位置的事件
void min_heap_shift_up_(min_heap_t* s, size_t hole_index, struct event* e)
{
    size_t parent = (hole_index - 1) / 2;
    while (hole_index && min_heap_elem_greater(s->p[parent], e))
    {
	//父节点下调
	(s->p[hole_index] = s->p[parent])->ev_timeout_pos.min_heap_idx = hole_index;
	//"空洞”上调
	hole_index = parent;
	parent = (hole_index - 1) / 2;
    }
    (s->p[hole_index] = e)->ev_timeout_pos.min_heap_idx = hole_index;
}
//向下调整hole_index
void min_heap_shift_down_(min_heap_t* s, size_t hole_index, struct event* e)
{
    size_t min_child = 2 * (hole_index + 1);
    while (min_child <= s->nSize)
	{
	min_child -= min_child == s->nSize || min_heap_elem_greater(s->p[min_child], s->p[min_child - 1]);
	if (!(min_heap_elem_greater(e, s->p[min_child])))
	    break;
	(s->p[hole_index] = s->p[min_child])->ev_timeout_pos.min_heap_idx = hole_index;
	hole_index = min_child;
	min_child = 2 * (hole_index + 1);
	}
    (s->p[hole_index] = e)->ev_timeout_pos.min_heap_idx = hole_index;
}

#endif /* MINHEAP_INTERNAL_H_INCLUDED_ */
