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
#include <esc/fsinterface.h>
#include <esc/io.h>
#include <esc/dir.h>
#include <esc/cmdargs.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_DATE_LEN	64

static void stat_printDate(const char *title,time_t timestamp);
static const char *stat_getType(sFileInfo *info);

int main(int argc,char *argv[]) {
	sFileInfo info;
	char apath[MAX_PATH_LEN];

	if(argc != 2 || isHelpCmd(argc,argv)) {
		fprintf(stderr,"Usage: %s <file>\n",argv[0]);
		return EXIT_FAILURE;
	}

	abspath(apath,MAX_PATH_LEN,argv[1]);
	if(stat(apath,&info) < 0)
		error("Unable to read file-information for '%s'",apath);

	printf("'%s' points to:\n",apath);
	printf("%-15s%d\n","Inode:",info.inodeNo);
	printf("%-15s%d\n","Device:",info.device);
	printf("%-15s%s\n","Type:",stat_getType(&info));
	printf("%-15s%d Bytes\n","Size:",info.size);
	printf("%-15s%hd\n","Blocks:",info.blockCount);
	stat_printDate("Accessed:",info.accesstime);
	stat_printDate("Modified:",info.modifytime);
	stat_printDate("Created:",info.createtime);
	printf("%-15s%#ho\n","Mode:",info.mode);
	printf("%-15s%hd\n","GroupID:",info.gid);
	printf("%-15s%hd\n","UserID:",info.uid);
	printf("%-15s%hd\n","Hardlinks:",info.linkCount);
	printf("%-15s%hd\n","BlockSize:",info.blockSize);

	return EXIT_SUCCESS;
}

static void stat_printDate(const char *title,time_t timestamp) {
	static char dateStr[MAX_DATE_LEN];
	struct tm *date = gmtime(&timestamp);
	strftime(dateStr,sizeof(dateStr),"%a, %m/%d/%Y %H:%M:%S",date);
	printf("%-15s%s\n",title,dateStr);
}

static const char *stat_getType(sFileInfo *info) {
	if(S_ISDIR(info->mode))
		return "Directory";
	if(S_ISBLK(info->mode))
		return "Block-Device";
	if(S_ISCHR(info->mode))
		return "Character-Device";
	if(S_ISFIFO(info->mode))
		return "FIFO";
	if(S_ISLNK(info->mode))
		return "Link";
	if(S_ISSOCK(info->mode))
		return "Socket";
	return "Regular File";
}