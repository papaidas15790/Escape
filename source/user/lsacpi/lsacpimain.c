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
#include <esc/arch/i586/acpi.h>
#include <esc/fsinterface.h>
#include <esc/dir.h>
#include <stdio.h>
#include <stdlib.h>

static void readTable(const char *name,sRSDT *table);

int main(void) {
	DIR *dir = opendir("/system/acpi");
	if(!dir)
		error("Unable to open /system/acpi");

	size_t i = 0;
	sDirEntry e;
	while(readdir(dir,&e)) {
		sRSDT table;
		if(strcmp(e.name,".") == 0 || strcmp(e.name,"..") == 0)
			continue;
		readTable(e.name,&table);
		printf("ACPI Table %zu:\n",i);
		printf("\tSignature: %.4s\n",&table.signature);
		printf("\tLength: %u\n",table.length);
		printf("\tRevision: %u\n",table.revision);
		printf("\tOEMID: %.6s\n",table.oemId);
		printf("\tOEMTableID: %.8s\n",table.oemTableId);
		i++;
	}

	closedir(dir);
	return EXIT_SUCCESS;
}

static void readTable(const char *name,sRSDT *table) {
	char path[MAX_PATH_LEN];
	snprintf(path,sizeof(path),"/system/acpi/%s",name);
	int fd = open(path,IO_READ);
	if(fd < 0)
		printe("open of %s failed",path);
	if(read(fd,table,sizeof(sRSDT)) != sizeof(sRSDT))
		printe("read of %s failed",path);
	close(fd);
}