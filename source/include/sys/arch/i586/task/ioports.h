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

#ifndef I586_IOPORTS_H_
#define I586_IOPORTS_H_

#include <sys/common.h>
#include <sys/task/proc.h>

/**
 * Requests some IO-ports for the given process. Will not replace the IO-Map in TSS!
 *
 * @param p the process
 * @param start the start-port
 * @param count the number of ports
 * @return the error-code or 0
 */
int ioports_request(sProc *p,uint16_t start,size_t count);

/**
 * Releases some IO-ports for the given process. Will not replace the IO-Map in TSS!
 *
 * @param p the process
 * @param start the start-port
 * @param count the number of ports
 * @return the error-code or 0
 */
int ioports_release(sProc *p,uint16_t start,size_t count);


#if DEBUGGING

/**
 * Prints the given IO-map
 *
 * @param map the io-map
 */
void ioports_print(const uint8_t *map);

#endif

#endif /* I586_IOPORTS_H_ */