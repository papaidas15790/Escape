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
#include <sys/test.h>
#include <stdlib.h>

extern sTestModule tModRBuffer;
extern sTestModule tModPathTree;
extern sTestModule tModSList;
extern sTestModule tModDList;
extern sTestModule tModTreap;
extern sTestModule tModStream;
extern sTestModule tModRegex;

int main() {
	test_register(&tModRBuffer);
	test_register(&tModPathTree);
	test_register(&tModSList);
	test_register(&tModDList);
	test_register(&tModTreap);
	test_register(&tModStream);
	test_register(&tModRegex);
	test_start();
	return EXIT_SUCCESS;
}
