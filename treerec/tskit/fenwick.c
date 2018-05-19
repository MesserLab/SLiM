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

/*
 * Binary index tree (also known as a Fenwick tree) implementation.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "util.h"
#include "fenwick.h"

static void
fenwick_set_log_size(fenwick_t *self)
{
    size_t u = self->size;

    while (u != 0) {
        self->log_size = u;
        u -= (u & -u);
    }
}

static int WARN_UNUSED
fenwick_alloc_buffers(fenwick_t *self)
{
    int ret = -1;

    self->tree = NULL;
    self->values = NULL;
    self->tree = calloc((1 + self->size), sizeof(int64_t));
    if (self->tree == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    self->values = calloc((1 + self->size), sizeof(int64_t));
    if (self->values == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    fenwick_set_log_size(self);
    ret = 0;
out:
    return ret;
}

int WARN_UNUSED
fenwick_alloc(fenwick_t *self, size_t initial_size)
{
    self->size = initial_size;
    return fenwick_alloc_buffers(self);
}

int WARN_UNUSED
fenwick_expand(fenwick_t *self, size_t increment)
{
    int ret = MSP_ERR_NO_MEMORY;
    size_t j, n, k;
    void *p;

    p = realloc(self->tree, (1 + self->size + increment) * sizeof(int64_t));
    if (p == NULL) {
        goto out;
    }
    self->tree = p;
    p = realloc(self->values, (1 + self->size + increment) * sizeof(int64_t));
    if (p == NULL) {
        goto out;
    }
    self->values = p;

    self->size += increment;
    fenwick_set_log_size(self);
    for (j = self->size - increment + 1; j <= self->size; j++) {
        self->values[j] = 0;
        self->tree[j] = 0;
        n = j;
        k = 1;
        while (n % 2 == 0) {
            self->tree[j] += self->tree[j - k];
            k *= 2;
            n >>= 1;
        }
    }
    ret = 0;
out:
    return ret;
}

int
fenwick_free(fenwick_t *self)
{
    int ret = -1;

    if (self->tree != NULL) {
        free(self->tree);
        self->tree = NULL;
    }
    if (self->values != NULL) {
        free(self->values);
        self->values = NULL;
    }
    ret = 0;
    return ret;
}

size_t
fenwick_get_size(fenwick_t *self)
{
    return self->size;
}

int64_t
fenwick_get_total(fenwick_t *self)
{
    return fenwick_get_cumulative_sum(self, self->size);
}

void
fenwick_increment(fenwick_t *self, size_t index, int64_t value)
{
    size_t j = index;

    assert(0 < index && index <= self->size);
    self->values[index] += value;
    while (j <= self->size) {
        self->tree[j] += value;
        j += (j & -j);
    }
}

void
fenwick_set_value(fenwick_t *self, size_t index, int64_t value)
{
    int64_t v = value - self->values[index];

    fenwick_increment(self, index, v);
}

int64_t
fenwick_get_cumulative_sum(fenwick_t *self, size_t index)
{
    int64_t ret = 0;
    size_t j = index;

    assert(0 < index && index <= self->size);
    while (j > 0) {
        ret += self->tree[j];
        j -= (j & -j);
    }
    return ret;
}

int64_t
fenwick_get_value(fenwick_t *self, size_t index)
{
    assert(0 < index && index <= self->size);
    return self->values[index];
}


size_t
fenwick_find(fenwick_t *self, int64_t sum)
{
    size_t j = 0;
    size_t k;
    int64_t s = sum;
    size_t half = self->log_size;

    while (half > 0) {
        /* Skip non-existent entries */
        while (j + half > self->size) {
            half >>= 1;
        }
        k = j + half;
        if (s > self->tree[k]) {
            j = k;
            s -= self->tree[j];
        }
        half >>= 1;
    }
    return j + 1;
}
