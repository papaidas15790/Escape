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

#ifndef EXT2_DIR_H_
#define EXT2_DIR_H_

#include <esc/common.h>
#include "ext2.h"

/**
 * Creates a directory with given name in given directory
 *
 * @param e the ext2-data
 * @param u the user
 * @param dir the directory
 * @param name the name of the new directory
 * @return 0 on success
 */
int ext2_dir_create(sExt2 *e,sFSUser *u,sExt2CInode *dir,const char *name);

/**
 * Finds the inode-number to the entry <name> in <dir>
 *
 * @param e the ext2-data
 * @param dir the directory
 * @param name the name of the entry to find
 * @param nameLen the length of the name
 * @return the inode-number or < 0
 */
inode_t ext2_dir_find(sExt2 *e,sExt2CInode *dir,const char *name,size_t nameLen);

/**
 * Finds the inode-number to the entry <name> in the given buffer
 *
 * @param buffer the buffer with all directory-entries
 * @param bufSize the size of the buffer
 * @param name the name of the entry to find
 * @param nameLen the length of the name
 * @return the inode-number or < 0
 */
inode_t ext2_dir_findIn(sExt2DirEntry *buffer,size_t bufSize,const char *name,size_t nameLen);

/**
 * Removes the directory with given name from the given directory. It is required that
 * the directory is empty!
 *
 * @param e the ext2-data
 * @param u the user
 * @param dir the directory
 * @param name the name of the directory to remove
 * @return 0 on success
 */
int ext2_dir_delete(sExt2 *e,sFSUser *u,sExt2CInode *dir,const char *name);

#endif /* EXT2_DIR_H_ */
