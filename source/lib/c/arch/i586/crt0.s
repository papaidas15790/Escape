#
# $Id: crt0.s 808 2010-09-20 15:29:18Z nasmussen $
# Copyright (C) 2008 - 2009 Nils Asmussen
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

#include <esc/syscalls.h>
#include <arch/i586/syscalls.s>

.section .text

.global _start
.global sigRetFunc
.extern main
.extern exit
.extern init_tls
.extern _init

#  Initial stack:
#  +------------------+  <- top
#  |     arguments    |
#  |        ...       |  not present for threads
#  +------------------+
#  |       argv       |  not present for threads
#  +------------------+
#  |       argc       |  or the argument for threads
#  +------------------+
#  |     TLSSize      |  0 if not present
#  +------------------+
#  |     TLSStart     |  0 if not present
#  +------------------+
#  |    entryPoint    |  0 for initial thread, thread-entrypoint for others
#  +------------------+

.ifndef SHAREDLIB
_start:
	# mark the beginning of the call-trace
	mov		$0,%ebp
	# call init_tls(entryPoint,TLSStart,TLSSize)
	call	init_tls
	# remove args from stack
	add		$12,%esp
	# it returns the entrypoint; 0 if we're the initial thread
	test	%eax,%eax
	je		initialThread
	# we're an additional thread, so call the desired function
	call	*%eax
	jmp		threadExit

	# initial thread calls main
initialThread:
	.extern __libc_init
	call	__libc_init
	# call function in .init-section
	call _init
	# finally, call main
	call	main

threadExit:
	push	%eax
	call	exit
	# just to be sure
	1: jmp		1b

# all signal-handler return to this "function"
sigRetFunc:
	mov		$SYSCALL_ACKSIG,%eax
	int		$SYSCALL_IRQ
	# never reached
.endif
