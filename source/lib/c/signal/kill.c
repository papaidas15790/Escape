/**
 * $Id$
 * Copyright (C) 2008 - 2014 Nils Asmussen
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
#include <sys/proc.h>
#include <sys/stat.h>
#include <stdio.h>
#include <signal.h>

int kill(pid_t pid,int signal) {
	char path[MAX_PATH_LEN];
	snprintf(path,sizeof(path),"/sys/proc/%d",pid);
	int fd = open(path,O_WRONLY);
	if(fd < 0)
		return fd;
	int res = syscall2(SYSCALL_SENDSIG,fd,signal);
	close(fd);
	return res;
}