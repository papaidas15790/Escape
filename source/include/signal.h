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

#ifndef SIGNAL_H_
#define SIGNAL_H_

#include <esc/common.h>

#define SIG_COUNT			20

/* special signal-handler-addresses */
#define SIG_IGN				((fSignal)-3)			/* ignore signal */
#define SIG_DFL				((fSignal)-2)			/* reset to default behaviour */
#define SIG_ERR				-1						/* indicates an error */

/* the signals */
#define SIG_RET				-1						/* used to tell the kernel the addr of sigRet */
#define SIG_KILL			0
#define SIG_TERM			1
#define SIG_ILL_INSTR		2
#define SIG_SEGFAULT		3
#define SIG_PROC_DIED		4
#define SIG_THREAD_DIED		5
#define SIG_PIPE_CLOSED		6
#define SIG_CHILD_TERM		7
#define SIG_INTRPT			8
#define SIG_INTRPT_TIMER	9
#define SIG_INTRPT_KB		10
#define SIG_INTRPT_COM1		11
#define SIG_INTRPT_COM2		12
#define SIG_INTRPT_FLOPPY	13
#define SIG_INTRPT_CMOS		14
#define SIG_INTRPT_ATA1		15
#define SIG_INTRPT_ATA2		16
#define SIG_INTRPT_MOUSE	17
#define SIG_USR1			18
#define SIG_USR2			19

/* standarg signal-names */
#define SIGABRT				SIG_KILL
/* TODO */
#define SIGFPE				-1
#define SIGILL				SIG_ILL_INSTR
#define SIGINT				SIG_INTRPT
#define SIGSEGV				SIG_SEGFAULT
#define SIGTERM				SIG_TERM

/* the signature */
typedef void (*fSignal)(int);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Sets a handler-function for a specific signal
 *
 * @param signal the signal-number
 * @param handler the handler-function
 * @return 0 on success
 */
int setSigHandler(sig_t signal,fSignal handler) A_CHECKRET;

/**
 * Sends the given signal to all process (that have announced a handler)
 *
 * @param signal the signal
 * @return 0 on success
 */
int sendSignal(sig_t signal) A_CHECKRET;

/**
 * Sends the given signal to given process (interrupts can't be sended)
 *
 * @param pid the process-id
 * @param signal the signal
 * @return 0 on success
 */
int sendSignalTo(pid_t pid,sig_t signal) A_CHECKRET;

/**
 * The  signal  system call installs a new signal handler for#
 * signal signum.  The signal handler is set to handler which
 * may be a user specified function, or one of the following:
 * 	SIG_IGN - Ignore the signal.
 * 	SIG_DFL - Reset the signal to its default behavior.
 *
 * @param sig the signal
 * @param handler the new handler-function
 * @return the previous handler-function
 */
fSignal signal(int sig,fSignal handler);

/**
 * raise()  sends  a  signal  to  the current process.  It is
 * equivalent to kill(getpid(),sig)
 *
 * @param sig the signal
 * @return zero on success
 */
int raise(int sig);

#ifdef __cplusplus
}
#endif

#endif /* SIGNAL_H_ */