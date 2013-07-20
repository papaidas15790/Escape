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

#pragma once

#include <sys/common.h>
#include <esc/sllist.h>

class Thread;

class Terminator {
	friend class ThreadBase;

public:
	/**
	 * Inits the terminator
	 */
	static void init();

	/**
	 * The start-function of the terminator. Should be called by a dedicated thread.
	 */
	static void start();

private:
	/**
	 * Adds the given thread for termination. If all threads of the process have been terminated,
	 * the process will be terminated as well.
	 *
	 * @param t the thread
	 */
	static void addDead(Thread *t);

	static sSLList deadThreads;
	static klock_t termLock;
};
