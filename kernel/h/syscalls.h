/**
 * @version		$Id$
 * @author		Nils Asmussen <nils@script-solution.de>
 * @copyright	2008 Nils Asmussen
 */

#ifndef SYSCALLS_H_
#define SYSCALLS_H_

#include "common.h"
#include "intrpt.h"

/* the user-stack within a syscall */
typedef struct {
	u32 number;		/* = error-code */
	u32 arg1;	/* = ret-val 1 */
	u32 arg2;	/* = ret-val 2 */
	u32 arg3;
	u32 arg4;
} tSysCallStack;

/**
 * Handles the syscall for the given interrupt-stack
 *
 * @param stack the pointer to the syscall-stack
 */
void sysc_handle(tSysCallStack *stack);

#endif /* SYSCALLS_H_ */
