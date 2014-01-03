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

#include <esc/common.h>
#include <esc/io.h>
#include <fs/blockcache.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "iso9660.h"
#include "rw.h"
#include "dir.h"

/* calcuates an imaginary inode-number from block-number and offset in the directory-entries */
#define GET_INODENO(blockNo,blockSize,offset) (((blockNo) * (blockSize)) + (offset))

/**
 * Checks whether <user> matches <disk>
 */
static bool iso_dir_match(const char *user,const char *disk,size_t userLen,size_t diskLen);

inode_t iso_dir_resolve(sISO9660 *h,A_UNUSED sFSUser *u,const char *path,uint flags) {
	size_t extLoc,extSize;
	const char *p = path;
	ssize_t pos;
	inode_t res;
	sCBlock *blk;
	size_t i,off,blockSize = ISO_BLK_SIZE(h);

	while(*p == '/')
		p++;

	extLoc = h->primary.data.primary.rootDir.extentLoc.littleEndian;
	extSize = h->primary.data.primary.rootDir.extentSize.littleEndian;
	res = ISO_ROOT_DIR_ID(h);

	pos = strchri(p,'/');
	while(*p) {
		const sISODirEntry *e;
		i = 0;
		off = 0;
		blk = bcache_request(&h->blockCache,extLoc,BMODE_READ);
		if(blk == NULL)
			return -ENOBUFS;

		e = (const sISODirEntry*)blk->buffer;
		while((uintptr_t)e < (uintptr_t)blk->buffer + blockSize) {
			/* continue with next block? */
			if(e->length == 0) {
				off += blockSize;
				if(off >= extSize)
					break;
				bcache_release(blk);
				blk = bcache_request(&h->blockCache,extLoc + ++i,BMODE_READ);
				if(blk == NULL)
					return -ENOBUFS;
				e = (const sISODirEntry*)blk->buffer;
				continue;
			}

			if(iso_dir_match(p,e->name,pos,e->nameLen)) {
				p += pos;

				/* skip slashes */
				while(*p == '/')
					p++;
				/* "/" at the end is optional */
				if(!*p)
					break;

				/* move to childs of this node */
				pos = strchri(p,'/');
				if((e->flags & ISO_FILEFL_DIR) == 0) {
					bcache_release(blk);
					return -ENOTDIR;
				}
				extLoc = e->extentLoc.littleEndian;
				extSize = e->extentSize.littleEndian;
				break;
			}
			e = (const sISODirEntry*)((uintptr_t)e + e->length);
		}
		/* no match? */
		if((uintptr_t)e >= (uintptr_t)blk->buffer + blockSize || e->length == 0) {
			bcache_release(blk);
			if(flags & IO_CREATE)
				return -EROFS;
			return -ENOENT;
		}
		res = GET_INODENO(extLoc + i,blockSize,(uintptr_t)e - (uintptr_t)blk->buffer);
		bcache_release(blk);
	}
	return res;
}

static bool iso_dir_match(const char *user,const char *disk,size_t userLen,size_t diskLen) {
	size_t rpos;
	if(*disk == ISO_FILENAME_THIS)
		return userLen == 1 && strcmp(user,".") == 0;
	if(*disk == ISO_FILENAME_PARENT)
		return userLen == 2 && strcmp(user,"..") == 0;
	/* don't compare volume sequence no */
	rpos = MIN(diskLen,(size_t)strchri(disk,';'));
	if(disk[rpos] != ';')
		return userLen == diskLen && strncasecmp(disk,user,userLen) == 0;
	return userLen == rpos && strncasecmp(disk,user,userLen) == 0;
}