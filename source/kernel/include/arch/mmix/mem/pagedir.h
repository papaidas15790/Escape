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

#pragma once

#include <arch/mmix/mem/addrspace.h>
#include <mem/physmemareas.h>
#include <mem/layout.h>
#include <assert.h>
#include <common.h>
#include <string.h>

class PageDir : public PageDirBase {
	friend class PageDirBase;

public:
	typedef ulong pte_t;

	explicit PageDir() : PageDirBase(), addrSpace(), rv(), ptables() {
	}

	/* not used */
	PageTables *getPageTables() {
		return NULL;
	}

	uint64_t getRV() const {
		return rv;
	}
	void setRV(uint64_t nrv) {
		rv = nrv;
	}
	AddressSpace *getAddrSpace() {
		return addrSpace;
	}
	void setAddrSpace(AddressSpace *aspace) {
		addrSpace = aspace;
	}

private:
	static void clearTC() {
		asm volatile ("SYNC 6");
	}
	static void updateTC(uint64_t key) {
		asm volatile ("LDVTS %0,%0,0" : "+r"(key));
	}
	static void setrV(uint64_t rv) {
		asm volatile ("PUT rV,%0" : : "r"(rv));
	}

	static size_t getPageCountOf(uint64_t *pt,size_t level);
	static void printPageTable(OStream &os,ulong seg,uintptr_t addr,uint64_t *pt,size_t level,ulong indent);
	static void printPTE(OStream &os,uint64_t pte);

	uint64_t *getPT(uintptr_t virt,bool create,PageTables::Allocator &alloc) const;
	uint64_t getPTE(uintptr_t virt) const;
	size_t removePts(uint64_t pageNo,uint64_t c,ulong level,ulong depth,PageTables::Allocator &alloc);
	size_t remEmptyPts(uintptr_t virt,PageTables::Allocator &alloc);
	void tcRemPT(uintptr_t virt);

private:
	AddressSpace *addrSpace;
	uint64_t rv;
	ulong ptables;
	static PageDir firstCon;
};

inline void PageTables::flushAddr(uintptr_t,bool) {
	// not used on mmix
}

inline void PageTables::flushPT(uintptr_t) {
	// not used on mmix
}

inline uintptr_t PageDirBase::getPhysAddr() const {
	const PageDir *pdir = static_cast<const PageDir*>(this);
	return pdir->rv & 0xFFFFFFE000;
}

inline void PageDirBase::makeFirst() {
	PageDir *pdir = static_cast<PageDir*>(this);
	pdir->addrSpace = PageDir::firstCon.addrSpace;
	pdir->rv = PageDir::firstCon.rv;
	pdir->ptables = PageDir::firstCon.ptables;
}

inline uintptr_t PageDirBase::makeAccessible(A_UNUSED uintptr_t phys,size_t pages) {
	assert(phys == 0);
	return DIR_MAP_AREA | (PhysMemAreas::alloc(pages) * PAGE_SIZE);
}

inline bool PageDirBase::isInUserSpace(uintptr_t virt,size_t count) {
	return virt + count <= DIR_MAP_AREA && virt + count >= virt;
}

inline bool PageDirBase::isPresent(uintptr_t virt) const {
	const PageDir *pdir = static_cast<const PageDir*>(this);
	uint64_t pte = pdir->getPTE(virt);
	return pte & PTE_EXISTS;
}

inline frameno_t PageDirBase::getFrameNo(uintptr_t virt) const {
	const PageDir *pdir = static_cast<const PageDir*>(this);
	uint64_t pte = pdir->getPTE(virt);
	assert(pte & PTE_EXISTS);
	return PTE_FRAMENO(pte);
}

inline uintptr_t PageDirBase::getAccess(frameno_t frame) {
	return frame * PAGE_SIZE | DIR_MAP_AREA;
}

inline void PageDirBase::removeAccess(A_UNUSED frameno_t frame) {
	/* nothing to do */
}

inline void PageDirBase::copyToFrame(frameno_t frame,const void *src) {
	memcpy((void*)(frame * PAGE_SIZE | DIR_MAP_AREA),src,PAGE_SIZE);
}

inline void PageDirBase::copyFromFrame(frameno_t frame,void *dst) {
	memcpy(dst,(void*)(frame * PAGE_SIZE | DIR_MAP_AREA),PAGE_SIZE);
}
