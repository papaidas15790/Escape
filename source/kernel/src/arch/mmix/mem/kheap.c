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
#include <sys/mem/kheap.h>
#include <sys/mem/paging.h>
#include <sys/mem/pmem.h>

uintptr_t kheap_allocAreas(void) {
	if(pmem_getFreeFrames(MM_DEF) < 1)
		return 0;
	return DIR_MAPPED_SPACE | (pmem_allocate() * PAGE_SIZE);
}

uintptr_t kheap_allocSpace(size_t count) {
	/* if its just one page, take a frame from the pmem-stack */
	if(count == 1)
		return kheap_allocAreas();
	/* otherwise we have to use contiguous physical memory */
	if(pmem_getFreeFrames(MM_CONT) < count)
		return 0;
	return DIR_MAPPED_SPACE | (pmem_allocateContiguous(count,1) * PAGE_SIZE);
}
