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

#include <esc/common.h>
#include <esc/proc.h>
#include <esc/messages.h>
#include <esc/cmdargs.h>
#include <esc/dir.h>
#include <ipc/proto/vterm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <env.h>
#include <string>

#define KEYMAP_DIR		"/etc/keymaps"

static void usage(const char *name) {
	fprintf(stderr,"Usage: %s [--set <name>]\n",name);
	exit(EXIT_FAILURE);
}

int main(int argc,const char **argv) {
	char *kmname = NULL;

	int res = ca_parse(argc,argv,CA_NO_FREE,"set=s",&kmname);
	if(res < 0) {
		fprintf(stderr,"Invalid arguments: %s\n",ca_error(res));
		usage(argv[0]);
	}
	if(ca_hasHelp())
		usage(argv[0]);

	ipc::VTerm vterm(std::env::get("TERM").c_str());

	/* set keymap? */
	if(kmname != NULL) {
		std::string keymap = std::string(KEYMAP_DIR) + "/" + kmname;
		vterm.setKeymap(keymap);
		printf("Successfully changed keymap to '%s'\n",keymap.c_str());
	}
	/* list all keymaps */
	else {
		sFileInfo curInfo;
		std::string keymap = vterm.getKeymap();
		if(stat(keymap.c_str(),&curInfo) < 0)
			printe("Unable to stat current keymap (%s)",keymap.c_str());

		sDirEntry e;
		DIR *dir = opendir(KEYMAP_DIR);
		if(!dir)
			error("Unable to open '%s'",KEYMAP_DIR);
		while(readdir(dir,&e)) {
			if(strcmp(e.name,".") != 0 && strcmp(e.name,"..") != 0) {
				char fpath[MAX_PATH_LEN];
				snprintf(fpath,sizeof(fpath),"%s/%s",KEYMAP_DIR,e.name);
				sFileInfo finfo;
				if(stat(fpath,&finfo) < 0)
					printe("Unable to stat '%s'",fpath);

				if(finfo.inodeNo == curInfo.inodeNo && finfo.device == curInfo.device)
					printf("* %s\n",e.name);
				else
					printf("  %s\n",e.name);
			}
		}
		closedir(dir);
	}
	return EXIT_SUCCESS;
}