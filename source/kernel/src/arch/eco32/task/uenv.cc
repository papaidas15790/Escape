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
#include <sys/task/uenv.h>
#include <sys/task/thread.h>
#include <sys/task/event.h>
#include <sys/mem/vmm.h>
#include <sys/mem/paging.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#define KEYBOARD_BASE		0xF0200000
#define KEYBOARD_CTRL		0
#define KEYBOARD_IEN		0x02

static void uenv_startSignalHandler(Thread *t,sIntrptStackFrame *stack,int sig,fSignal handler);
static uint32_t *uenv_addArgs(Thread *t,uint32_t *sp,uintptr_t tentryPoint,bool newThread);

void uenv_handleSignal(Thread *t,sIntrptStackFrame *stack) {
	int sig;
	fSignal handler;
	int res = sig_checkAndStart(t->tid,&sig,&handler);
	if(res == SIG_CHECK_CUR)
		uenv_startSignalHandler(t,stack,sig,handler);
	else if(res == SIG_CHECK_OTHER)
		Thread::switchAway();
}

int uenv_finishSignalHandler(sIntrptStackFrame *stack,int signal) {
	uint32_t *regs;
	uint32_t *sp = (uint32_t*)stack->r[29];
	memcpy(stack->r,sp,REG_COUNT * sizeof(uint32_t));
	/* reenable device-interrupts */
	switch(signal) {
		case SIG_INTRPT_KB:
			regs = (uint32_t*)KEYBOARD_BASE;
			regs[KEYBOARD_CTRL] |= KEYBOARD_IEN;
			break;
		/* not necessary for disk here; the device will reenable interrupts as soon as a new
		 * command is started */
	}
	return 0;
}

bool uenv_setupProc(int argc,const char *args,A_UNUSED size_t argsSize,const sStartupInfo *info,
		uintptr_t entryPoint,A_UNUSED int fd) {
	uint32_t *sp;
	char **argv;
	Thread *t = Thread::getRunning();
	sIntrptStackFrame *frame = t->getIntrptStack();

	/*
	 * Initial stack:
	 * +------------------+  <- top
	 * |     arguments    |
	 * |        ...       |
	 * +------------------+
	 * |       argv       |
	 * +------------------+
	 * |       argc       |
	 * +------------------+
	 * |     TLSStart     |  (0 if not present)
	 * +------------------+
	 * |     TLSSize      |  (0 if not present)
	 * +------------------+
	 * |    EntryPoint    |  (0 for initial thread, thread-entrypoint for others)
	 * +------------------+
	 */

	/* get esp */
	t->getStackRange(NULL,(uintptr_t*)&sp,0);

	/* copy arguments on the user-stack (4byte space) */
	sp--;
	argv = NULL;
	if(argc > 0) {
		char *str;
		int i;
		size_t len;
		argv = (char**)(sp - argc);
		/* space for the argument-pointer */
		sp -= argc;
		/* start for the arguments */
		str = (char*)sp;
		for(i = 0; i < argc; i++) {
			/* start <len> bytes backwards */
			len = strlen(args) + 1;
			str -= len;
			/* store arg-pointer and copy arg */
			argv[i] = str;
			memcpy(str,args,len);
			/* to next */
			args += len;
		}
		/* ensure that we don't overwrites the characters */
		sp = (uint32_t*)(((uintptr_t)str & ~(sizeof(uint32_t) - 1)) - sizeof(uint32_t));
	}

	/* store argc and argv */
	*sp-- = (uintptr_t)argv;
	*sp-- = argc;
	/* add TLS args and entrypoint; use prog-entry here because its always the entry of the
	 * program, not the dynamic-linker */
	sp = uenv_addArgs(t,sp,info->progEntry,false);

	/* set entry-point and stack-pointer */
	frame->r[29] = (uint32_t)sp;
	frame->r[30] = entryPoint - 4; /* we'll skip the trap-instruction for syscalls */
	return true;
}

uint32_t *uenv_setupThread(const void *arg,uintptr_t tentryPoint) {
	uint32_t *sp;
	Thread *t = Thread::getRunning();

	/*
	 * Initial stack:
	 * +------------------+  <- top
	 * |       arg        |
	 * +------------------+
	 * |     TLSStart     |  (0 if not present)
	 * +------------------+
	 * |     TLSSize      |  (0 if not present)
	 * +------------------+
	 * |    EntryPoint    |  (0 for initial thread, thread-entrypoint for others)
	 * +------------------+
	 */

	/* get esp */
	t->getStackRange(NULL,(uintptr_t*)&sp,0);
	sp--;

	/* put arg on stack */
	*sp-- = (uintptr_t)arg;
	/* add TLS args and entrypoint */
	return uenv_addArgs(t,sp,tentryPoint,true);
}

static void uenv_startSignalHandler(Thread *t,sIntrptStackFrame *stack,int sig,fSignal handler) {
	uint32_t *sp = (uint32_t*)stack->r[29];
	if(!paging_isInUserSpace((uintptr_t)(sp - REG_COUNT),REG_COUNT * sizeof(uint32_t))) {
		proc_segFault();
		/* never reached */
		assert(false);
	}

	/* if we've not entered the kernel by a trap, we have to decrease $30, because when returning
	 * from the signal, we'll always enter it by a trap, so that $30 will be increased */
	if(stack->irqNo != 20)
		stack->r[30] -= 4;

	memcpy(sp - REG_COUNT,stack->r,REG_COUNT * sizeof(uint32_t));
	/* signal-number as arguments */
	stack->r[4] = sig;
	/* set new stack-pointer */
	stack->r[29] = (uint32_t)(sp - REG_COUNT);
	/* the process should continue here */
	stack->r[30] = (uint32_t)handler;
	/* and return here after handling the signal */
	stack->r[31] = t->proc->sigRetAddr;
}

static uint32_t *uenv_addArgs(Thread *t,uint32_t *sp,uintptr_t tentryPoint,bool newThread) {
	/* put address and size of the tls-region on the stack */
	uintptr_t tlsStart,tlsEnd;
	if(t->getTLSRange(&tlsStart,&tlsEnd)) {
		*sp-- = tlsStart;
		*sp-- = tlsEnd - tlsStart;
	}
	else {
		/* no tls */
		*sp-- = 0;
		*sp-- = 0;
	}

	*sp = newThread ? tentryPoint : 0;
	return sp;
}
