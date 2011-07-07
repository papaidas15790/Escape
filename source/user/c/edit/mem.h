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

#ifndef MEM_H_
#define MEM_H_

#include <esc/common.h>
#include <esc/sllist.h>

/**
 * Calls malloc() and exits if no more memory is available
 *
 * @param size the number of bytes
 * @return the pointer to the memory
 */
void *emalloc(size_t size);

/**
 * Calls realloc() and exits if no more memory is available
 *
 * @param p the pointer to the memory
 * @param size the number of bytes
 * @return the pointer to the memory
 */
void *erealloc(void *p,size_t size);

/**
 * Calls strdup() and exits if no more memory is available
 *
 * @param s the string
 * @return the copy
 */
char *estrdup(const char *s);

/**
 * Calls strndup() and exits if no more memory is available
 *
 * @param s the string
 * @param n the number of characters to copy at max.
 * @return the copy
 */
char *estrndup(const char *s,size_t n);

/**
 * Calls sll_create() and exits if no more mem is available
 *
 * @return the list
 */
sSLList *esll_create(void);

/**
 * Calls sll_insert() and exits if no more mem is available
 *
 * @param list the list
 * @param data the data to append
 * @param index the position
 */
void esll_insert(sSLList *list,const void *data,size_t index);

/**
 * Calls sll_append() and exits if no more mem is available
 *
 * @param list the list
 * @param data the data to append
 */
void esll_append(sSLList *list,const void *data);

/**
 * Calls free()
 *
 * @param p the pointer to the memory
 */
void efree(void *p);

#endif /* MEM_H_ */
