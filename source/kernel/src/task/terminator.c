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
#include <sys/task/terminator.h>
#include <sys/task/proc.h>
#include <sys/task/thread.h>
#include <sys/task/event.h>
#include <sys/mem/sllnodes.h>
#include <sys/spinlock.h>
#include <esc/sllist.h>
#include <assert.h>

static sSLList deadThreads;
static klock_t termLock;

void term_init(void) {
	sll_init(&deadThreads,slln_allocNode,slln_freeNode);
}

void term_start(void) {
	sThread *t = thread_getRunning();
	spinlock_aquire(&termLock);
	while(1) {
		if(sll_length(&deadThreads) == 0) {
			ev_wait(t,EVI_TERMINATION,0);
			spinlock_release(&termLock);

			thread_switch();

			spinlock_aquire(&termLock);
		}

		while(sll_length(&deadThreads) > 0) {
			sThread *dt = (sThread*)sll_removeFirst(&deadThreads);
			/* release the lock while we're killing the thread; the process-module may use us
			 * in this time to add another thread */
			spinlock_release(&termLock);

			log_printf("Waiting for %d\n",dt->tid);
			while(!thread_beginTerm(dt))
				thread_switch();
			log_printf("Killing %d\n",dt->tid);
			proc_killThread(dt->tid);

			spinlock_aquire(&termLock);
		}
	}
}

void term_addDead(sThread *t) {
	spinlock_aquire(&termLock);
	/* ensure that we don't add a thread twice */
	if(!(t->flags & T_WILL_DIE)) {
		t->flags |= T_WILL_DIE;
		util_printEventTrace(util_getKernelStackTrace(),"Adding %d",t->tid);
		assert(sll_append(&deadThreads,t));
		ev_wakeup(EVI_TERMINATION,0);
	}
	spinlock_release(&termLock);
}