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
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "util.h"
#include "object_heap.h"

/* memory heap manager */

size_t
object_heap_get_num_allocated(object_heap_t *self)
{
    return self->size - self->top;
}

void
object_heap_print_state(object_heap_t *self, FILE *out)
{
    fprintf(out, "object heap %p::\n", (void *) self);
    fprintf(out, "\tsize = %d\n", (int) self->size);
    fprintf(out, "\ttop = %d\n", (int) self->top);
    fprintf(out, "\tblock_size = %d\n", (int) self->block_size);
    fprintf(out, "\tnum_blocks = %d\n", (int) self->num_blocks);
    fprintf(out, "\ttotal allocated = %d\n",
            (int) object_heap_get_num_allocated(self));
}

static void
object_heap_add_block(object_heap_t *self, char *mem_block)
{
    size_t j, index;

    for (j = 0; j < self->block_size; j++) {
        self->heap[j] = mem_block + j * self->object_size;
        if (self->init_object != NULL) {
            index = j + (self->num_blocks - 1) * self->block_size;
            self->init_object(self->heap[j], index);
        }
    }
    self->top = self->block_size;
}

int WARN_UNUSED
object_heap_expand(object_heap_t *self)
{
    int ret = -1;
    void *p;

    p = realloc(self->mem_blocks, (self->num_blocks + 1) * sizeof(void *));
    if (p == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    self->mem_blocks = p;
    p = malloc(self->block_size * self->object_size);
    if (p == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    self->mem_blocks[self->num_blocks] = p;
    self->num_blocks++;
    /* Now we increase the size of the heap. Since it is currently empty,
     * we avoid the copying cost of realloc and free before making a new
     * heap.
     */
    free(self->heap);
    self->heap = NULL;
    self->size += self->block_size;
    self->heap = malloc(self->size * sizeof(void *));
    if (self->heap == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    object_heap_add_block(self, p);
    ret = 0;
out:
    return ret;
}

/*
 * Returns the jth object in the memory buffers.
 */
inline void * WARN_UNUSED
object_heap_get_object(object_heap_t *self, size_t index)
{
    void *ret = NULL;
    size_t block, obj;

    block = index / self->block_size;
    obj = index % self->block_size;
    if (block < self->num_blocks && obj < self->block_size) {
        ret = self->mem_blocks[block] + obj * self->object_size;
    }
    return ret;
}

inline int WARN_UNUSED
object_heap_empty(object_heap_t *self)
{
    return self->top == 0;
}

inline void * WARN_UNUSED
object_heap_alloc_object(object_heap_t *self)
{
    void *ret = NULL;

    if (self->top > 0) {
        self->top--;
        ret = self->heap[self->top];
    }
    return ret;
}

inline void
object_heap_free_object(object_heap_t *self, void *obj)
{
    assert(self->top < self->size);
    self->heap[self->top] = obj;
    self->top++;
}

int WARN_UNUSED
object_heap_init(object_heap_t *self, size_t object_size, size_t block_size,
        void (*init_object)(void **, size_t))
{
    int ret = -1;

    memset(self, 0, sizeof(object_heap_t));
    self->block_size = block_size;
    self->size = block_size;
    self->object_size = object_size;
    self->init_object = init_object;
    self->num_blocks = 1;
    self->heap = malloc(self->size * sizeof(void *));
    self->mem_blocks = malloc(sizeof(void *));
    if (self->heap == NULL || self->mem_blocks == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    self->mem_blocks[0] = malloc(self->size * self->object_size);
    if (self->mem_blocks[0] == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    self->top = 0;
    object_heap_add_block(self, self->mem_blocks[0]);
    ret = 0;
out:
    return ret;
}

void
object_heap_free(object_heap_t *self)
{
    size_t j;

    if (self->mem_blocks != NULL) {
        for (j = 0; j < self->num_blocks; j++) {
            if (self->mem_blocks[j] != NULL) {
                free(self->mem_blocks[j]);
            }
        }
        free(self->mem_blocks);
    }
    if (self->heap != NULL) {
        free(self->heap);
    }
}
