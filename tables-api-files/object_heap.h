/*
** Copyright (C) 2015 University of Oxford
**
** This file is part of msprime.
**
** msprime is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** msprime is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with msprime.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef OBJECT_HEAP_H
#define OBJECT_HEAP_H

#include <stdio.h>
#include <string.h>
#include <assert.h>

/* #include "msprime.h" */

typedef struct {
    size_t object_size;
    size_t block_size; /* number of objects in a block */
    size_t top;
    size_t size;
    size_t num_blocks;
    void **heap;
    char **mem_blocks;
    void (*init_object)(void **obj, size_t index);
} object_heap_t;

extern size_t object_heap_get_num_allocated(object_heap_t *self);
extern void object_heap_print_state(object_heap_t *self, FILE *out);
extern int object_heap_expand(object_heap_t *self);
extern void * object_heap_get_object(object_heap_t *self, size_t index);
extern int object_heap_empty(object_heap_t *self);
extern void * object_heap_alloc_object(object_heap_t *self);
extern void object_heap_free_object(object_heap_t *self, void *obj);
extern int object_heap_init(object_heap_t *self, size_t object_size, size_t block_size,
        void (*init_object)(void **, size_t));
extern void object_heap_free(object_heap_t *self);

#endif
