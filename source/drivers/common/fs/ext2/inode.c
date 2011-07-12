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
#include <esc/endian.h>
#include <esc/proc.h>
#include <stdio.h>
#include <string.h>
#include <errors.h>

#include "ext2.h"
#include "../blockcache.h"
#include "../fstime.h"
#include "inode.h"
#include "superblock.h"
#include "inodecache.h"
#include "bitmap.h"

/**
 * Performs the actual get-block-request. If <req> is true, it will allocate a new block, if
 * necessary. In this case cnode may be changed. Otherwise no changes will be made.
 */
static block_t ext2_inode_doGetDataBlock(sExt2 *e,sExt2CInode *cnode,block_t block,bool req);
/**
 * Puts a new block in cblock->buffer if cblock->buffer[index] is 0. Marks the cblock dirty,
 * if necessary. Sets <added> to true or false, depending on whether a block was allocated.
 */
static int ext2_inode_extend(sExt2 *e,sExt2CInode *cnode,sCBlock *cblock,size_t index,bool *added);

int ext2_inode_create(sExt2 *e,sFSUser *u,sExt2CInode *dirNode,sExt2CInode **ino,bool isDir) {
	size_t i;
	time_t now;
	sExt2CInode *cnode;

	/* request inode */
	inode_t inodeNo = ext2_bm_allocInode(e,dirNode,isDir);
	if(inodeNo == 0)
		return ERR_INO_ALLOC;
	cnode = ext2_icache_request(e,inodeNo,IMODE_WRITE);
	if(cnode == NULL) {
		ext2_bm_freeInode(e,inodeNo,isDir);
		return ERR_INO_REQ_FAILED;
	}

	/* init inode */
	cnode->inode.uid = cputole16(u->uid);
	cnode->inode.gid = cputole16(u->gid);
	if(isDir) {
		/* drwxr-xr-x */
		cnode->inode.mode = cputole16(EXT2_S_IFDIR |
			EXT2_S_IRUSR | EXT2_S_IRGRP | EXT2_S_IROTH |
			EXT2_S_IXUSR | EXT2_S_IXGRP | EXT2_S_IXOTH |
			EXT2_S_IWUSR);
	}
	else {
		/* -rw-r--r-- */
		cnode->inode.mode = cputole16(EXT2_S_IFREG |
			EXT2_S_IRUSR | EXT2_S_IWUSR | EXT2_S_IRGRP | EXT2_S_IROTH);
	}
	cnode->inode.linkCount = cputole16(0);
	cnode->inode.size = cputole32(0);
	cnode->inode.singlyIBlock = cputole32(0);
	cnode->inode.doublyIBlock = cputole32(0);
	cnode->inode.triplyIBlock = cputole32(0);
	for(i = 0; i < EXT2_DIRBLOCK_COUNT; i++)
		cnode->inode.dBlocks[i] = cputole32(0);
	cnode->inode.blocks = cputole32(0);
	now = cputole32(timestamp());
	cnode->inode.accesstime = now;
	cnode->inode.createtime = now;
	cnode->inode.modifytime = now;
	cnode->inode.deletetime = cputole32(0);
	ext2_icache_markDirty(cnode);
	*ino = cnode;
	return 0;
}

int ext2_inode_chmod(sExt2 *e,sFSUser *u,inode_t inodeNo,mode_t mode) {
	mode_t oldMode;
	sExt2CInode *cnode = ext2_icache_request(e,inodeNo,IMODE_WRITE);
	if(cnode == NULL)
		return ERR_INO_REQ_FAILED;

	/* root can chmod everything; others can only chmod their own files */
	if(u->uid != le16tocpu(cnode->inode.uid) && u->uid != ROOT_UID)
		return ERR_NO_PERM;

	oldMode = le16tocpu(cnode->inode.mode);
	cnode->inode.mode = cputole16((oldMode & ~EXT2_S_PERMS) | (mode & EXT2_S_PERMS));
	ext2_icache_markDirty(cnode);
	ext2_icache_release(cnode);
	return 0;
}

int ext2_inode_chown(sExt2 *e,sFSUser *u,inode_t inodeNo,uid_t uid,gid_t gid) {
	uid_t oldUid;
	gid_t oldGid;
	sExt2CInode *cnode = ext2_icache_request(e,inodeNo,IMODE_WRITE);
	if(cnode == NULL)
		return ERR_INO_REQ_FAILED;

	/* root can chown everything; others can only chown their own files */
	oldUid = le16tocpu(cnode->inode.uid);
	oldGid = le16tocpu(cnode->inode.gid);
	if(u->uid != oldUid && u->uid != ROOT_UID)
		return ERR_NO_PERM;
	if(u->uid != ROOT_UID) {
		/* users can't change the owner */
		if(uid != (uid_t)-1 && uid != oldUid && uid != u->uid)
			return ERR_NO_PERM;
		/* users can change the group only to a group they're a member of */
		if(gid != (gid_t)-1 && gid != oldGid && gid != u->gid && !isingroup(u->pid,gid))
			return ERR_NO_PERM;
	}

	if(uid != (uid_t)-1)
		cnode->inode.uid = cputole16(uid);
	if(gid != (gid_t)-1)
		cnode->inode.gid = cputole16(gid);
	ext2_icache_markDirty(cnode);
	ext2_icache_release(cnode);
	return 0;
}

int ext2_inode_destroy(sExt2 *e,sExt2CInode *cnode) {
	int res;
	/* free inode, clear it and ensure that it get's written back to disk */
	if((res = ext2_bm_freeInode(e,cnode->inodeNo,S_ISDIR(le16tocpu(cnode->inode.mode)))) < 0)
		return res;
	/* just set the delete-time and reset link-count. the block-numbers in the inode
	 * are still present, so that it may be possible to restore the file, if the blocks
	 * have not been overwritten in the meantime. */
	cnode->inode.deletetime = cputole32(timestamp());
	cnode->inode.linkCount = cputole16(0);
	ext2_icache_markDirty(cnode);
	return 0;
}

block_t ext2_inode_reqDataBlock(sExt2 *e,sExt2CInode *cnode,block_t block) {
	return ext2_inode_doGetDataBlock(e,cnode,block,true);
}

block_t ext2_inode_getDataBlock(sExt2 *e,const sExt2CInode *cnode,block_t block) {
	return ext2_inode_doGetDataBlock(e,(sExt2CInode*)cnode,block,false);
}

static block_t ext2_inode_doGetDataBlock(sExt2 *e,sExt2CInode *cnode,block_t block,bool req) {
	block_t i;
	size_t blockSize,blocksPerBlock,blperBlSq;
	uint bmode = req ? BMODE_WRITE : BMODE_READ;
	bool added = false;
	sCBlock *cblock;

	/* Note that we don't have to mark the inode dirty here if blocks are added
	 * because ext2_file_write() does it for us */

	/* direct block */
	if(block < EXT2_DIRBLOCK_COUNT) {
		/* alloc a new block if necessary */
		i = le32tocpu(cnode->inode.dBlocks[block]);
		if(req && i == 0) {
			i = ext2_bm_allocBlock(e,cnode);
			cnode->inode.dBlocks[block] = cputole32(i);
			if(i != 0) {
				uint32_t blocks = le32tocpu(cnode->inode.blocks);
				cnode->inode.blocks = cputole32(blocks + EXT2_BLKS_TO_SECS(e,1));
			}
		}
		return i;
	}

	/* singly indirect */
	block -= EXT2_DIRBLOCK_COUNT;
	blockSize = EXT2_BLK_SIZE(e);
	blocksPerBlock = blockSize / sizeof(block_t);
	if(block < blocksPerBlock) {
		added = false;
		i = le32tocpu(cnode->inode.singlyIBlock);
		/* no singly-indirect-block present yet? */
		if(i == 0) {
			if(!req)
				return 0;
			i = ext2_bm_allocBlock(e,cnode);
			cnode->inode.singlyIBlock = cputole32(i);
			if(i == 0)
				return 0;
			cnode->inode.blocks = cputole32(le32tocpu(cnode->inode.blocks) + EXT2_BLKS_TO_SECS(e,1));
			added = true;
		}

		cblock = bcache_request(&e->blockCache,i,bmode);
		if(cblock == NULL)
			return 0;
		if(added) {
			memclear(cblock->buffer,EXT2_BLK_SIZE(e));
			bcache_markDirty(cblock);
		}
		if(req && ext2_inode_extend(e,cnode,cblock,block,&added) != 1) {
			bcache_release(cblock);
			return 0;
		}
		i = le32tocpu(*((block_t*)(cblock->buffer) + block));
		bcache_release(cblock);
		return i;
	}

	/* TODO we have to verify if this is all correct here... */

	/* doubly indirect */
	block -= blocksPerBlock;
	blperBlSq = blocksPerBlock * blocksPerBlock;
	if(block < blperBlSq) {
		added = false;
		i = le32tocpu(cnode->inode.doublyIBlock);
		/* no doubly-indirect-block present yet? */
		if(i == 0) {
			if(!req)
				return 0;
			i = ext2_bm_allocBlock(e,cnode);
			cnode->inode.doublyIBlock = cputole32(i);
			if(i == 0)
				return 0;
			added = true;
			cnode->inode.blocks = cputole32(le32tocpu(cnode->inode.blocks) + EXT2_BLKS_TO_SECS(e,1));
		}

		/* read the first block with block-numbers of the indirect blocks */
		cblock = bcache_request(&e->blockCache,i,bmode);
		if(cblock == NULL)
			return 0;
		if(added) {
			memclear(cblock->buffer,EXT2_BLK_SIZE(e));
			bcache_markDirty(cblock);
		}
		if(req && ext2_inode_extend(e,cnode,cblock,block / blocksPerBlock,&added) != 1) {
			bcache_release(cblock);
			return 0;
		}
		i = le32tocpu(*((block_t*)(cblock->buffer) + block / blocksPerBlock));
		bcache_release(cblock);

		/* may happen if we should not request new blocks */
		if(i == 0)
			return 0;

		/* read the indirect block */
		cblock = bcache_request(&e->blockCache,i,bmode);
		if(cblock == NULL)
			return 0;
		if(added) {
			memclear(cblock->buffer,EXT2_BLK_SIZE(e));
			bcache_markDirty(cblock);
		}
		if(req && ext2_inode_extend(e,cnode,cblock,block % blocksPerBlock,&added) != 1) {
			bcache_release(cblock);
			return 0;
		}
		i = le32tocpu(*((block_t*)(cblock->buffer) + block % blocksPerBlock));
		bcache_release(cblock);

		return i;
	}

	/* triply indirect */
	block -= blperBlSq;

	added = false;
	i = le32tocpu(cnode->inode.triplyIBlock);
	/* no triply-indirect-block present yet? */
	if(i == 0) {
		if(!req)
			return 0;
		i = ext2_bm_allocBlock(e,cnode);
		cnode->inode.triplyIBlock = cputole32(i);
		if(i == 0)
			return 0;
		added = true;
		cnode->inode.blocks = cputole32(le32tocpu(cnode->inode.blocks) + EXT2_BLKS_TO_SECS(e,1));
	}

	/* read the first block with block-numbers of the indirect blocks of indirect-blocks */
	cblock = bcache_request(&e->blockCache,i,bmode);
	if(cblock == NULL)
		return 0;
	if(added) {
		memclear(cblock->buffer,EXT2_BLK_SIZE(e));
		bcache_markDirty(cblock);
	}
	if(req && ext2_inode_extend(e,cnode,cblock,block / blperBlSq,&added) != 1) {
		bcache_release(cblock);
		return 0;
	}
	i = le32tocpu(*((block_t*)(cblock->buffer) + block / blperBlSq));
	bcache_release(cblock);

	if(i == 0)
		return 0;

	/* read the indirect block of indirect blocks */
	block %= blperBlSq;
	cblock = bcache_request(&e->blockCache,i,bmode);
	if(cblock == NULL)
		return 0;
	if(added) {
		memclear(cblock->buffer,EXT2_BLK_SIZE(e));
		bcache_markDirty(cblock);
	}
	if(req && ext2_inode_extend(e,cnode,cblock,block / blocksPerBlock,&added) != 1) {
		bcache_release(cblock);
		return 0;
	}
	i = le32tocpu(*((block_t*)(cblock->buffer) + block / blocksPerBlock));
	bcache_release(cblock);

	if(i == 0)
		return 0;

	/* read the indirect block */
	cblock = bcache_request(&e->blockCache,i,bmode);
	if(cblock == NULL)
		return 0;
	if(added) {
		memclear(cblock->buffer,EXT2_BLK_SIZE(e));
		bcache_markDirty(cblock);
	}
	if(req && ext2_inode_extend(e,cnode,cblock,block % blocksPerBlock,&added) != 1) {
		bcache_release(cblock);
		return 0;
	}
	i = le32tocpu(*((block_t*)(cblock->buffer) + block % blocksPerBlock));
	bcache_release(cblock);

	return i;
}

static int ext2_inode_extend(sExt2 *e,sExt2CInode *cnode,sCBlock *cblock,size_t index,bool *added) {
	block_t *blockNos = (block_t*)(cblock->buffer);
	if(le32tocpu(blockNos[index]) == 0) {
		block_t bno = ext2_bm_allocBlock(e,cnode);
		if(bno == 0)
			return 0;
		blockNos[index] = cputole32(bno);
		cnode->inode.blocks = cputole32(le32tocpu(cnode->inode.blocks) + EXT2_BLKS_TO_SECS(e,1));
		bcache_markDirty(cblock);
		*added = true;
	}
	else
		*added = false;
	return 1;
}

#if DEBUGGING

void ext2_inode_print(sExt2Inode *inode) {
	size_t i;
	printf("\tmode=0x%08x\n",le16tocpu(inode->mode));
	printf("\tuid=%d\n",le16tocpu(inode->uid));
	printf("\tgid=%d\n",le16tocpu(inode->gid));
	printf("\tsize=%d\n",le32tocpu(inode->size));
	printf("\taccesstime=%d\n",le32tocpu(inode->accesstime));
	printf("\tcreatetime=%d\n",le32tocpu(inode->createtime));
	printf("\tmodifytime=%d\n",le32tocpu(inode->modifytime));
	printf("\tdeletetime=%d\n",le32tocpu(inode->deletetime));
	printf("\tlinkCount=%d\n",le16tocpu(inode->linkCount));
	printf("\tblocks=%d\n",le32tocpu(inode->blocks));
	printf("\tflags=0x%08x\n",le32tocpu(inode->flags));
	printf("\tosd1=0x%08x\n",le32tocpu(inode->osd1));
	for(i = 0; i < EXT2_DIRBLOCK_COUNT; i++)
		printf("\tblock%d=%d\n",i,le32tocpu(inode->dBlocks[i]));
	printf("\tsinglyIBlock=%d\n",le32tocpu(inode->singlyIBlock));
	printf("\tdoublyIBlock=%d\n",le32tocpu(inode->doublyIBlock));
	printf("\ttriplyIBlock=%d\n",le32tocpu(inode->triplyIBlock));
	printf("\tgeneration=%d\n",le32tocpu(inode->generation));
	printf("\tfileACL=%d\n",le32tocpu(inode->fileACL));
	printf("\tdirACL=%d\n",le32tocpu(inode->dirACL));
	printf("\tfragAddr=%d\n",le32tocpu(inode->fragAddr));
	printf("\tosd2=0x%08x%08x%08x%08x\n",le16tocpu(inode->osd2[0]),le16tocpu(inode->osd2[1]),
			le16tocpu(inode->osd2[2]),le16tocpu(inode->osd2[3]));
}

#endif