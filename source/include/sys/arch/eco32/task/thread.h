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
#include <sys/task/timer.h>
#include <sys/cpu.h>

class Thread : public ThreadBase {
	friend class ThreadBase;
public:
	/**
	 * @return the frame mapped at KERNEL_STACK
	 */
	frameno_t getKernelStack() const {
		return kstackFrame;
	}

private:
	static void startup() asm("thread_startup");
	static bool save(ThreadRegs *saveArea) asm("thread_save");
	static bool resume(uintptr_t pageDir,const ThreadRegs *saveArea,frameno_t kstackFrame)
		asm("thread_resume");

	frameno_t kstackFrame;
	static Thread *cur;
};

inline size_t ThreadBase::getThreadFrmCnt() {
	return INITIAL_STACK_PAGES;
}

inline Thread *ThreadBase::getRunning() {
	return Thread::cur;
}

inline void ThreadBase::setRunning(Thread *t) {
	Thread::cur = t;
}

inline uint64_t ThreadBase::getTSC() {
	return CPU::rdtsc();
}

inline uint64_t ThreadBase::ticksPerSec() {
	return CPU::getSpeed();
}

inline uint64_t ThreadBase::getRuntime() const {
	if(state == Thread::RUNNING) {
		/* if the thread is running, we must take the time since the last scheduling of that thread
		 * into account. this is especially a problem with idle-threads */
		uint64_t cycles = CPU::rdtsc();
		return Timer::cyclesToTime(stats.runtime + (cycles - stats.cycleStart));
	}
	return Timer::cyclesToTime(stats.runtime);
}
