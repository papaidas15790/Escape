/**
 * $Id$
 * Copyright (C) 2008 - 2011 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <sys/common.h>
#include <sys/mem/sllnodes.h>
#include <sys/task/event.h>
#include <sys/task/sched.h>
#include <sys/task/thread.h>
#include <esc/sllist.h>
#include <sys/video.h>
#include <assert.h>

#define MAX_WAIT_COUNT		1024
#define MAX_WAKEUPS			8

typedef struct {
	sWait *begin;
	sWait *last;
} sWaitList;

static sWait *ev_doWait(sThread *t,size_t evi,evobj_t object,sWait **begin,sWait *prev);
static sWait *ev_allocWait(void);
static void ev_freeWait(sWait *w);
static const char *ev_getName(size_t evi);

static sWait waits[MAX_WAIT_COUNT];
static sWait *waitFree;
static sWaitList evlists[EV_COUNT];

void ev_init(void) {
	size_t i;
	for(i = 0; i < EV_COUNT; i++) {
		evlists[i].begin = NULL;
		evlists[i].last = NULL;
	}

	waitFree = waits;
	waitFree->next = NULL;
	for(i = 1; i < MAX_WAIT_COUNT; i++) {
		waits[i].next = waitFree;
		waitFree = waits + i;
	}
}

bool ev_waitsFor(tid_t tid,uint events) {
	sThread *t = thread_getById(tid);
	return t->events & events;
}

bool ev_wait(tid_t tid,size_t evi,evobj_t object) {
	sThread *t = thread_getById(tid);
	sWait *w = t->waits;
	while(w && w->tnext)
		w = w->tnext;
	return ev_doWait(t,evi,object,&t->waits,w) != NULL;
}

bool ev_waitObjects(tid_t tid,const sWaitObject *objects,size_t objCount) {
	size_t i,e;
	sThread *t = thread_getById(tid);
	sWait *w = t->waits;
	while(w && w->tnext)
		w = w->tnext;

	for(i = 0; i < objCount; i++) {
		uint events = objects[i].events;
		if(events == 0)
			sched_setBlocked(t);
		else {
			for(e = 0; events && e < EV_COUNT; e++) {
				if(events & (1 << e)) {
					w = ev_doWait(t,e,objects[i].object,&t->waits,w);
					if(w == NULL) {
						ev_removeThread(tid);
						return false;
					}
					events &= ~(1 << e);
				}
			}
		}
	}
	return true;
}

void ev_wakeup(size_t evi,evobj_t object) {
	tid_t tids[MAX_WAKEUPS];
	sWaitList *list = evlists + evi;
	sWait *w = list->begin;
	size_t i = 0;
	while(w != NULL) {
		if(w->object == 0 || w->object == object) {
			if(i < MAX_WAKEUPS)
				tids[i++] = w->tid;
			else {
				/* all slots in use, so remove this threads and start from the beginning to find
				 * more. hopefully, this will happen nearly never :) */
				for(; i > 0; i--)
					ev_removeThread(tids[i - 1]);
				w = list->begin;
				continue;
			}
		}
		w = w->next;
	}
	for(; i > 0; i--)
		ev_removeThread(tids[i - 1]);
}

void ev_wakeupm(uint events,evobj_t object) {
	size_t e;
	for(e = 0; events && e < EV_COUNT; e++) {
		if(events & (1 << e)) {
			ev_wakeup(e,object);
			events &= ~(1 << e);
		}
	}
}

bool ev_wakeupThread(tid_t tid,uint events) {
	sThread *t = thread_getById(tid);
	if(t->events & events) {
		ev_removeThread(tid);
		return true;
	}
	return false;
}

void ev_removeThread(tid_t tid) {
	sThread *t = thread_getById(tid);
	if(t->events) {
		sWait *w = t->waits;
		while(w != NULL) {
			sWait *nw = w->tnext;
			if(w->prev)
				w->prev->next = w->next;
			else
				evlists[w->evi].begin = w->next;
			if(w->next)
				w->next->prev = w->prev;
			else
				evlists[w->evi].last = w->prev;
			ev_freeWait(w);
			w = nw;
		}
		t->waits = NULL;
		t->events = 0;
		sched_setReady(t);
	}
}

void ev_printEvMask(uint mask) {
	uint e;
	for(e = 0; e < EV_COUNT; e++) {
		if(mask & (1 << e))
			vid_printf("%s ",ev_getName(e));
	}
}

void ev_print(void) {
	size_t e;
	vid_printf("Eventlists:\n");
	for(e = 0; e < EV_COUNT; e++) {
		sWaitList *list = evlists + e;
		sWait *w = list->begin;
		vid_printf("\t%s:\n",ev_getName(e));
		while(w != NULL) {
			sThread *t = thread_getById(w->tid);
			vid_printf("\t\tthread=%d (%d:%s), object=%x",
					t->tid,t->proc->pid,t->proc->command,w->object);
			inode_t nodeNo = vfs_node_getNo((sVFSNode*)w->object);
			if(vfs_node_isValid(nodeNo))
				vid_printf("(%s)",vfs_node_getPath(nodeNo));
			vid_printf("\n");
			w = w->next;
		}
	}
}

static sWait *ev_doWait(sThread *t,size_t evi,evobj_t object,sWait **begin,sWait *prev) {
	sWaitList *list = evlists + evi;
	sWait *w = ev_allocWait();
	if(!w)
		return NULL;
	w->tid = t->tid;
	w->evi = evi;
	w->object = object;
	/* insert into list */
	w->next = NULL;
	w->prev = list->last;
	if(list->last)
		list->last->next = w;
	else
		list->begin = w;
	list->last = w;
	/* add to thread */
	t->events |= 1 << evi;
	if(prev)
		prev->tnext = w;
	else
		*begin = w;
	w->tnext = NULL;
	sched_setBlocked(t);
	return w;
}

static sWait *ev_allocWait(void) {
	sWait *res = waitFree;
	if(res == NULL)
		return NULL;
	waitFree = waitFree->next;
	return res;
}

static void ev_freeWait(sWait *w) {
	w->next = waitFree;
	waitFree = w;
}

static const char *ev_getName(size_t evi) {
	static const char *names[] = {
		"CLIENT",
		"RECEIVED_MSG",
		"CHILD_DIED",
		"DATA_READABLE",
		"UNLOCK_SH",
		"PIPE_FULL",
		"PIPE_EMPTY",
		"VM86_READY",
		"REQ_REPLY",
		"SWAP_DONE",
		"SWAP_WORK",
		"SWAP_FREE",
		"VMM_DONE",
		"THREAD_DIED",
		"USER1",
		"USER2",
		"REQ_FREE",
		"UNLOCK_EX",
	};
	return names[evi];
}