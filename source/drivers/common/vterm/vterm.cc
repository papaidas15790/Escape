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
#include <esc/driver.h>
#include <esc/io.h>
#include <esc/debug.h>
#include <esc/proc.h>
#include <esc/mem.h>
#include <esc/thread.h>
#include <esc/messages.h>
#include <esc/sllist.h>
#include <esc/ringbuffer.h>
#include <usergroup/group.h>
#include <ipc/proto/ui.h>
#include <ipc/proto/file.h>
#include <ipc/proto/vterm.h>
#include <ipc/vtermdevice.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <memory>

#include <vterm/vtctrl.h>
#include <vterm/vtin.h>
#include <vterm/vtout.h>

static int vtermThread(void *vterm);
static void uimInputThread(ipc::UIEvents &uiev);
static int vtInit(int id,const char *name,uint cols,uint rows);
static void vtSetVideoMode(int mode);
static void vtUpdate(void);
static void vtSetCursor(sVTerm *vt);

static std::vector<ipc::Screen::Mode> modes;
static ipc::FrameBuffer *fb = NULL;
static sVTerm vterm;

class TUIVTermDevice : public ipc::VTermDevice {
public:
	explicit TUIVTermDevice(const char *name,mode_t mode,sVTerm *vt) : VTermDevice(name,mode,vt) {
		set(MSG_SCR_GETMODE,std::make_memfun(this,&TUIVTermDevice::getMode));
		set(MSG_SCR_GETMODES,std::make_memfun(this,&TUIVTermDevice::getModes));
		set(MSG_UIM_GETKEYMAP,std::make_memfun(this,&TUIVTermDevice::getKeymap));
		set(MSG_UIM_SETKEYMAP,std::make_memfun(this,&TUIVTermDevice::setKeymap));
	}

	virtual void setVideoMode(int mode) {
		vtSetVideoMode(mode);
	}

	virtual void update() {
		vtUpdate();
	}

	void getMode(ipc::IPCStream &is) {
		ipc::Screen::Mode mode = vterm.ui->getMode();
		is << 0 << mode << ipc::Reply();
	}

	void getModes(ipc::IPCStream &is) {
		size_t n;
		is >> n;

		is << modes.size() << ipc::Reply();
		if(n)
			is << ipc::ReplyData(modes.begin(),sizeof(ipc::Screen::Mode) * modes.size());
	}

	void getKeymap(ipc::IPCStream &is) {
		std::string keymap = vterm.ui->getKeymap();
		is << 0 << ipc::CString(keymap.c_str(),keymap.length()) << ipc::Reply();
	}

	void setKeymap(ipc::IPCStream &is) {
		ipc::CStringBuf<MAX_PATH_LEN> path;
		is >> path;
		vterm.ui->setKeymap(std::string(path.str()));
		is << 0 << ipc::Reply();
	}
};

static TUIVTermDevice *vtdev;

int main(int argc,char **argv) {
	if(argc < 4) {
		fprintf(stderr,"Usage: %s <cols> <rows> <name>\n",argv[0]);
		return EXIT_FAILURE;
	}

	char path[MAX_PATH_LEN];
	snprintf(path,sizeof(path),"/dev/%s",argv[3]);

	/* create device */
	vtdev = new TUIVTermDevice(path,0770,&vterm);

	/* we want to give only users that are in the ui-group access to this vterm */
	size_t gcount;
	sGroup *groups = group_parseFromFile(GROUPS_PATH,&gcount);
	sGroup *uigrp = group_getByName(groups,argv[3]);
	if(uigrp != NULL) {
		if(chown(path,ROOT_UID,uigrp->gid) < 0)
			printe("Unable to add ui-group to group-list");
	}
	else
		printe("Unable to find ui-group '%s'",argv[3]);
	group_free(groups);

	/* init vterms */
	int modeid = vtInit(vtdev->id(),argv[3],atoi(argv[1]),atoi(argv[2]));
	if(modeid < 0)
		error("Unable to init vterms");

	/* open uimng's input device */
	ipc::UIEvents uiev("/dev/uim-input",*vterm.ui);

	/* set video mode */
	vtSetVideoMode(modeid);

	/* now we're the active client. update screen */
	vtctrl_markScrDirty(&vterm);
	vtUpdate();

	/* start thread to handle the vterm */
	if(startthread(vtermThread,vtdev) < 0)
		error("Unable to start thread for vterm %s",path);

	/* receive input-events from uimanager in this thread */
	uimInputThread(uiev);

	/* clean up */
	delete vtdev;
	vtctrl_destroy(&vterm);
	return EXIT_SUCCESS;
}

static void uimInputThread(ipc::UIEvents &uiev) {
	/* read from uimanager and handle the keys */
	while(1) {
		ipc::UIEvents::Event ev;
		uiev >> ev;
		if(ev.type == ipc::UIEvents::Event::TYPE_KEYBOARD) {
			std::lock_guard<std::mutex> guard(*vterm.mutex);
			vtin_handleKey(&vterm,ev.d.keyb.keycode,ev.d.keyb.modifier,ev.d.keyb.character);
			vtUpdate();
		}
		vtdev->checkPending();
	}
}

static int vtermThread(void *arg) {
	TUIVTermDevice *dev = (TUIVTermDevice*)arg;
	dev->loop();
	return 0;
}

static int vtInit(int id,const char *name,uint cols,uint rows) {
	vterm.ui = new ipc::UI("/dev/uim-ctrl");
	modes = vterm.ui->getModes();

	/* find a suitable mode */
	ipc::Screen::Mode mode = vterm.ui->findTextModeIn(modes,cols,rows);

	/* open speaker */
	try {
		vterm.speaker = new ipc::Speaker("/dev/speaker");
	}
	catch(const std::exception &e) {
		/* ignore errors here. in this case we simply don't use it */
		printe("%s",e.what());
	}

	vterm.index = 0;
	vterm.sid = id;
	vterm.defForeground = LIGHTGRAY;
	vterm.defBackground = BLACK;
	vterm.setCursor = vtSetCursor;
	memcpy(vterm.name,name,MAX_VT_NAME_LEN + 1);
	if(!vtctrl_init(&vterm,&mode))
		return -ENOMEM;
	return mode.id;
}

static void vtUpdate(void) {
	/* if we should scroll, mark the whole screen (without title) as dirty */
	if(vterm.upScroll != 0) {
		vterm.upCol = 0;
		vterm.upRow = MIN(vterm.upRow,0);
		vterm.upHeight = vterm.rows - vterm.upRow;
		vterm.upWidth = vterm.cols;
	}

	if(vterm.upWidth > 0) {
		/* update content */
		assert(vterm.upCol + vterm.upWidth <= vterm.cols);
		assert(vterm.upRow + vterm.upHeight <= vterm.rows);
		size_t offset = vterm.upRow * vterm.cols * 2 + vterm.upCol * 2;
		char **lines = vterm.lines + vterm.firstVisLine + vterm.upRow;
		for(size_t i = 0; i < vterm.upHeight; ++i) {
			memcpy(fb->addr() + offset + i * vterm.cols * 2,
				lines[i] + vterm.upCol * 2,vterm.upWidth * 2);
		}

		vterm.ui->update(vterm.upCol,vterm.upRow,vterm.upWidth,vterm.upHeight);
	}
	vtSetCursor(&vterm);

	/* all synchronized now */
	vterm.upCol = vterm.cols;
	vterm.upRow = vterm.rows;
	vterm.upWidth = 0;
	vterm.upHeight = 0;
	vterm.upScroll = 0;
}

static void vtSetVideoMode(int mode) {
	for(auto it = modes.begin(); it != modes.end(); ++it) {
		if(it->id == mode) {
			/* rename old shm as a backup */
			ipc::FrameBuffer *fbtmp = fb;
			std::string tmpname;
			if(fbtmp) {
				tmpname = fbtmp->filename() + "-tmp";
				fbtmp->rename(tmpname);
			}

			/* try to set new mode */
			try {
				std::unique_ptr<ipc::FrameBuffer> nfb(
					new ipc::FrameBuffer(*it,vterm.name,ipc::Screen::MODE_TYPE_TUI,0644));
				vterm.ui->setMode(ipc::Screen::MODE_TYPE_TUI,it->id,vterm.name,true);
				fb = nfb.release();
			}
			catch(const std::default_error &e) {
				fb = fbtmp;
				if(fb)
					fb->rename(vterm.name);
				throw;
			}

			/* resize vterm if necessary */
			if(vterm.cols != it->cols || vterm.rows != it->rows) {
				if(!vtctrl_resize(&vterm,it->cols,it->rows)) {
					delete fb;
					fb = fbtmp;
					if(fb) {
						fb->rename(vterm.name);
						vterm.ui->setMode(ipc::Screen::MODE_TYPE_TUI,fb->mode().id,vterm.name,true);
					}
					VTHROWE("vtctrl_resize(" << it->cols << "," << it->rows << ")",-ENOMEM);
				}
			}

			delete fbtmp;
			return;
		}
	}
	VTHROW("Unable to find mode " << mode);
}

static void vtSetCursor(sVTerm *vt) {
	gpos_t x,y;
	if(vt->upScroll != 0 || vt->col != vt->lastCol || vt->row != vt->lastRow) {
		/* draw no cursor if it's not visible by setting it to an invalid location */
		if(vt->firstVisLine + vt->rows <= vt->currLine + vt->row) {
			x = vt->cols;
			y = vt->rows;
		}
		else {
			x = vt->col;
			y = vt->row;
		}
		vt->ui->setCursor(x,y);
		vt->lastCol = x;
		vt->lastRow = y;
	}
}
