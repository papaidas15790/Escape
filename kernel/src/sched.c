/**
 * @version		$Id$
 * @author		Nils Asmussen <nils@script-solution.de>
 * @copyright	2008 Nils Asmussen
 */

#include "../h/common.h"
#include "../h/sched.h"
#include "../h/proc.h"
#include "../h/video.h"
#include "../h/util.h"

/* the queue for all runnable (but not currently running) processes */
typedef struct tQueueNode tQueueNode;
struct tQueueNode {
	tQueueNode *next;
	tProc *p;
};

/* ready-queue stuff */
static tQueueNode readyQueue[PROC_COUNT];
static tQueueNode *rqFree;
static tQueueNode *rqFirst;
static tQueueNode *rqLast;

void sched_init(void) {
	s32 i;
	tQueueNode *node;
	/* put all on the free-queue */
	node = &readyQueue[PROC_COUNT - 1];
	node->next = NULL;
	node--;
	for(i = PROC_COUNT - 2; i >= 0; i--) {
		node->next = node + 1;
		node--;
	}
	/* all free atm */
	rqFree = &readyQueue[0];
	rqFirst = NULL;
	rqLast = NULL;
}

tProc *sched_perform(void) {
	tProc *p = proc_getRunning();
	/* put current in the ready-queue */
	p->state = ST_READY;
	sched_enqueueReady(p);

	/* get new process */
	p = sched_dequeueReady();
	/* TODO idle if there is no runnable process */
	p->state = ST_RUNNING;
	return p;
}

void sched_printReadyQueue(void) {
	tQueueNode *n = rqFirst;
	vid_printf("Ready-Queue: rqFirst=0x%x, rqLast=0x%x, rqFree=0x%x\n",rqFirst,rqLast,rqFree);
	while(n != NULL) {
		vid_printf("\t[0x%x]: p=0x%x, next=0x%x\n",n,n->p,n->next);
		n = n->next;
	}
}

void sched_enqueueReady(tProc *p) {
	tQueueNode *nn,*n;
	if(rqFree == NULL)
		panic("No free slots in the ready-queue!?");

	/* use first free node */
	nn = rqFree;
	rqFree = rqFree->next;
	nn->p = p;
	nn->next = NULL;

	/* put at the end of the ready-queue */
	n = rqFirst;
	if(n != NULL) {
		rqLast->next = nn;
		rqLast = nn;
	}
	else {
		rqFirst = nn;
		rqLast = nn;
	}
}

tProc *sched_dequeueReady(void) {
	tQueueNode *node;
	if(rqFirst == NULL)
		return NULL;

	/* put in free-queue & remove from ready-queue */
	node = rqFirst;
	rqFirst = rqFirst->next;
	if(rqFirst == NULL)
		rqLast = NULL;
	node->next = rqFree;
	rqFree = node;

	return node->p;
}

bool sched_dequeueProc(tProc *p) {
	tQueueNode *n = rqFirst,*l = NULL;
	while(n != NULL) {
		/* found it? */
		if(n->p == p) {
			/* dequeue */
			if(l == NULL)
				l = rqFirst = n->next;
			else
				l->next = n->next;
			if(n->next == NULL)
				rqLast = l;
			n->next = rqFree;
			rqFree = n;
			return true;
		}
		/* to next */
		l = n;
		n = n->next;
	}
	return false;
}
