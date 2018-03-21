/*
** Copyright (C) 2017-2018 University of Oxford
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
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include <hdf5.h>

#include "util.h"
#include "tables.h"
#include "object_heap.h"


#define DEFAULT_SIZE_INCREMENT 1024

#define TABLE_SEP "-----------------------------------------\n"

static int
cmp_edge_cl(const void *a, const void *b) {
    const edge_t *ia = (const edge_t *) a;
    const edge_t *ib = (const edge_t *) b;
    int ret = (ia->child > ib->child) - (ia->child < ib->child);
    if (ret == 0)  {
        ret = (ia->left > ib->left) - (ia->left < ib->left);
    }
    return ret;
}

/* Squash the edges in the specified array in place. The output edges will
 * be sorted by (child_id, left).
 */
int WARN_UNUSED
squash_edges(edge_t *edges, size_t num_edges, size_t *num_output_edges)
{
    int ret = 0;
    size_t j, k, l;
    edge_t e;

    qsort(edges, num_edges, sizeof(edge_t), cmp_edge_cl);
    j = 0;
    l = 0;
    for (k = 1; k < num_edges; k++) {
        assert(edges[k - 1].parent == edges[k].parent);
        if (edges[k - 1].right != edges[k].left || edges[j].child != edges[k].child) {
            e = edges[j];
            e.right = edges[k - 1].right;
            edges[l] = e;
            j = k;
            l++;
        }
    }
    e = edges[j];
    e.right = edges[k - 1].right;
    edges[l] = e;
    *num_output_edges = l + 1;
    return ret;
}

/* Checks that the specified list of offsets is well-formed. */
static int
check_offsets(size_t num_rows, table_size_t *offsets, table_size_t length,
        bool check_length)
{
    int ret = MSP_ERR_BAD_OFFSET;
    size_t j;

    if (offsets[0] != 0) {
        goto out;
    }
    if (check_length && offsets[num_rows] != length) {
        goto out;
    }
    for (j = 0; j < num_rows; j++) {
        if (offsets[j] > offsets[j + 1]) {
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

static int
expand_column(void **column, size_t new_max_rows, size_t element_size)
{
    int ret = 0;
    void *tmp;

    tmp = realloc((void **) *column, new_max_rows * element_size);
    if (tmp == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    *column = tmp;
out:
    return ret;
}

/*************************
 * node table
 *************************/

static int
node_table_expand_main_columns(node_table_t *self, table_size_t additional_rows)
{
    int ret = 0;
    table_size_t increment = MSP_MAX(additional_rows, self->max_rows_increment);
    table_size_t new_size = self->max_rows + increment;

    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column((void **) &self->flags, new_size, sizeof(uint32_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->time, new_size, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->population, new_size, sizeof(population_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->metadata_offset, new_size + 1,
                sizeof(table_size_t));
        if (ret != 0) {
            goto out;
        }
        self->max_rows = new_size;
    }
out:
    return ret;
}

static int
node_table_expand_metadata(node_table_t *self, table_size_t additional_length)
{
    int ret = 0;
    table_size_t increment = MSP_MAX(additional_length,
            self->max_metadata_length_increment);
    table_size_t new_size = self->max_metadata_length + increment;

    if ((self->metadata_length + additional_length) > self->max_metadata_length) {
        ret = expand_column((void **) &self->metadata, new_size, sizeof(char *));
        if (ret != 0) {
            goto out;
        }
        self->max_metadata_length = new_size;
    }
out:
    return ret;
}

int
node_table_alloc(node_table_t *self, size_t max_rows_increment,
        size_t max_metadata_length_increment)
{
    int ret = 0;

    memset(self, 0, sizeof(node_table_t));
    if (max_rows_increment == 0) {
       max_rows_increment = DEFAULT_SIZE_INCREMENT;
    }
    if (max_metadata_length_increment == 0) {
        max_metadata_length_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_rows_increment = (table_size_t) max_rows_increment;
    self->max_metadata_length_increment = (table_size_t) max_metadata_length_increment;
    self->max_rows = 0;
    self->num_rows = 0;
    self->max_metadata_length = 0;
    self->metadata_length = 0;
    ret = node_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = node_table_expand_metadata(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->metadata_offset[0] = 0;
out:
    return ret;
}

int WARN_UNUSED
node_table_copy(node_table_t *self, node_table_t *dest)
{
    return node_table_set_columns(dest, self->num_rows, self->flags,
            self->time, self->population, self->metadata, self->metadata_offset);
}

int WARN_UNUSED
node_table_set_columns(node_table_t *self, size_t num_rows, uint32_t *flags, double *time,
        population_id_t *population, char *metadata, uint32_t *metadata_offset)
{
    int ret;

    ret = node_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = node_table_append_columns(self, num_rows, flags, time, population, metadata,
            metadata_offset);
out:
    return ret;
}

int
node_table_append_columns(node_table_t *self, size_t num_rows, uint32_t *flags, double *time,
        population_id_t *population, char *metadata, uint32_t *metadata_offset)
{
    int ret;
    table_size_t j, metadata_length;

    if (flags == NULL || time == NULL) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    if ((metadata == NULL) != (metadata_offset == NULL)) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    ret = node_table_expand_main_columns(self, (table_size_t) num_rows);
    if (ret != 0) {
        goto out;
    }
    memcpy(self->flags + self->num_rows, flags, num_rows * sizeof(uint32_t));
    memcpy(self->time + self->num_rows, time, num_rows * sizeof(double));
    if (metadata == NULL) {
        for (j = 0; j < num_rows; j++) {
            self->metadata_offset[self->num_rows + j + 1] = (table_size_t) self->metadata_length;
        }
    } else {
        ret = check_offsets(num_rows, metadata_offset, 0, false);
        if (ret != 0) {
            goto out;
        }
        for (j = 0; j < num_rows; j++) {
            self->metadata_offset[self->num_rows + j] =
                (table_size_t) self->metadata_length + metadata_offset[j];
        }
        metadata_length = metadata_offset[num_rows];
        ret = node_table_expand_metadata(self, metadata_length);
        if (ret != 0) {
            goto out;
        }
        memcpy(self->metadata + self->metadata_length, metadata, metadata_length * sizeof(char));
        self->metadata_length += metadata_length;
    }
    if (population == NULL) {
        /* Set population to NULL_POPULATION (-1) if not specified */
        memset(self->population + self->num_rows, 0xff,
                num_rows * sizeof(population_id_t));
    } else {
        memcpy(self->population + self->num_rows, population,
                num_rows * sizeof(population_id_t));
    }
    self->num_rows += (table_size_t) num_rows;
    self->metadata_offset[self->num_rows] = self->metadata_length;
out:
    return ret;
}

static node_id_t
node_table_add_row_internal(node_table_t *self, uint32_t flags, double time,
        population_id_t population, const char *metadata, table_size_t metadata_length)
{
    assert(self->num_rows < self->max_rows);
    assert(self->metadata_length + metadata_length <= self->max_metadata_length);
    memcpy(self->metadata + self->metadata_length, metadata, metadata_length);
    self->flags[self->num_rows] = flags;
    self->time[self->num_rows] = time;
    self->population[self->num_rows] = population;
    self->metadata_offset[self->num_rows + 1] = self->metadata_length + metadata_length;
    self->metadata_length += metadata_length;
    self->num_rows++;
    return (node_id_t) self->num_rows - 1;
}

node_id_t
node_table_add_row(node_table_t *self, uint32_t flags, double time,
        population_id_t population, const char *metadata, size_t metadata_length)
{
    int ret = 0;

    ret = node_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = node_table_expand_metadata(self, (table_size_t) metadata_length);
    if (ret != 0) {
        goto out;
    }
    ret = node_table_add_row_internal(self, flags, time, population, metadata,
            (table_size_t) metadata_length);
out:
    return ret;
}

int
node_table_clear(node_table_t *self)
{
    self->num_rows = 0;
    self->metadata_length = 0;
    return 0;
}

int
node_table_free(node_table_t *self)
{
    msp_safe_free(self->flags);
    msp_safe_free(self->time);
    msp_safe_free(self->population);
    msp_safe_free(self->metadata);
    msp_safe_free(self->metadata_offset);
    return 0;
}

void
node_table_print_state(node_table_t *self, FILE *out)
{
    size_t j, k;

    fprintf(out, TABLE_SEP);
    fprintf(out, "node_table: %p:\n", (void *) self);
    fprintf(out, "num_rows          = %d\tmax= %d\tincrement = %d)\n",
            (int) self->num_rows, (int) self->max_rows, (int) self->max_rows_increment);
    fprintf(out, "metadata_length = %d\tmax= %d\tincrement = %d)\n",
            (int) self->metadata_length,
            (int) self->max_metadata_length,
            (int) self->max_metadata_length_increment);
    fprintf(out, TABLE_SEP);
    /* We duplicate the dump_text code here for simplicity because we want to output
     * the flags column directly. */
    fprintf(out, "id\tflags\ttime\tpopulation\tmetadata_offset\tmetadata\n");
    for (j = 0; j < self->num_rows; j++) {
        fprintf(out, "%d\t%d\t%f\t%d\t%d\t", (int) j, self->flags[j], self->time[j],
                (int) self->population[j], self->metadata_offset[j]);
        for (k = self->metadata_offset[j]; k < self->metadata_offset[j + 1]; k++) {
            fprintf(out, "%c", self->metadata[k]);
        }
        fprintf(out, "\n");
    }
    assert(self->metadata_offset[0] == 0);
    assert(self->metadata_offset[self->num_rows] == self->metadata_length);
}

int
node_table_dump_text(node_table_t *self, FILE *out)
{
    int ret = 0;
    size_t j;
    table_size_t metadata_len;
    int err;

    err = fprintf(out, "id\tis_sample\ttime\tpopulation\tmetadata\n");
    if (err < 0) {
        goto out;
    }
    for (j = 0; j < self->num_rows; j++) {
        metadata_len = self->metadata_offset[j + 1] - self->metadata_offset[j];
        err = fprintf(out, "%d\t%d\t%f\t%d\t%.*s\n", (int) j,
                (int) (self->flags[j] & MSP_NODE_IS_SAMPLE),
                self->time[j], self->population[j],
                metadata_len, self->metadata + self->metadata_offset[j]);
        if (err < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

bool
node_table_equal(node_table_t *self, node_table_t *other)
{
    bool ret = false;
    if (self->num_rows == other->num_rows
            && self->metadata_length == other->metadata_length) {
        ret = memcmp(self->time, other->time,
                self->num_rows * sizeof(double)) == 0
            && memcmp(self->flags, other->flags,
                    self->num_rows * sizeof(uint32_t)) == 0
            && memcmp(self->population, other->population,
                    self->num_rows * sizeof(population_id_t)) == 0
            && memcmp(self->metadata_offset, other->metadata_offset,
                    (self->num_rows + 1) * sizeof(table_size_t)) == 0
            && memcmp(self->metadata, other->metadata,
                    self->metadata_length * sizeof(char)) == 0;
    }
    return ret;
}

/*************************
 * edge table
 *************************/

static int
edge_table_expand_columns(edge_table_t *self, size_t additional_rows)
{
    int ret = 0;
    size_t increment = MSP_MAX(additional_rows, self->max_rows_increment);
    size_t new_size = self->max_rows + increment;

    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column((void **) &self->left, new_size, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->right, new_size, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->parent, new_size, sizeof(node_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->child, new_size, sizeof(node_id_t));
        if (ret != 0) {
            goto out;
        }
        self->max_rows = new_size;
    }
out:
    return ret;
}

int
edge_table_alloc(edge_table_t *self, size_t max_rows_increment)
{
    int ret = 0;

    memset(self, 0, sizeof(edge_table_t));
    if (max_rows_increment == 0) {
        max_rows_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_rows_increment = max_rows_increment;
    self->max_rows = 0;
    self->num_rows = 0;
    ret = edge_table_expand_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

edge_id_t
edge_table_add_row(edge_table_t *self, double left, double right, node_id_t parent,
        node_id_t child)
{
    int ret = 0;

    ret = edge_table_expand_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->left[self->num_rows] = left;
    self->right[self->num_rows] = right;
    self->parent[self->num_rows] = parent;
    self->child[self->num_rows] = child;
    ret = (edge_id_t) self->num_rows;
    self->num_rows++;
out:
    return ret;
}

int WARN_UNUSED
edge_table_copy(edge_table_t *self, edge_table_t *dest)
{
    return edge_table_set_columns(dest, self->num_rows, self->left, self->right,
            self->parent, self->child);
}

int
edge_table_set_columns(edge_table_t *self,
        size_t num_rows, double *left, double *right, node_id_t *parent, node_id_t *child)
{
    int ret = 0;

    ret = edge_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = edge_table_append_columns(self, num_rows, left, right, parent, child);
out:
    return ret;
}

int
edge_table_append_columns(edge_table_t *self,
        size_t num_rows, double *left, double *right, node_id_t *parent, node_id_t *child)
{
    int ret;

    if (left == NULL || right == NULL || parent == NULL || child == NULL) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    ret = edge_table_expand_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    memcpy(self->left + self->num_rows, left, num_rows * sizeof(double));
    memcpy(self->right + self->num_rows, right, num_rows * sizeof(double));
    memcpy(self->parent + self->num_rows, parent, num_rows * sizeof(node_id_t));
    memcpy(self->child + self->num_rows, child, num_rows * sizeof(node_id_t));
    self->num_rows += num_rows;
out:
    return ret;
}

int
edge_table_clear(edge_table_t *self)
{
    self->num_rows = 0;
    return 0;
}

int
edge_table_free(edge_table_t *self)
{
    msp_safe_free(self->left);
    msp_safe_free(self->right);
    msp_safe_free(self->parent);
    msp_safe_free(self->child);
    return 0;
}

void
edge_table_print_state(edge_table_t *self, FILE *out)
{
    int ret;

    fprintf(out, TABLE_SEP);
    fprintf(out, "edge_table: %p:\n", (void *) self);
    fprintf(out, "num_rows          = %d\tmax= %d\tincrement = %d)\n",
            (int) self->num_rows, (int) self->max_rows, (int) self->max_rows_increment);
    fprintf(out, TABLE_SEP);
    ret = edge_table_dump_text(self, out);
    assert(ret == 0);
}

int
edge_table_dump_text(edge_table_t *self, FILE *out)
{
    size_t j;
    int ret = MSP_ERR_IO;
    int err;

    err = fprintf(out, "left\tright\tparent\tchild\n");
    if (err < 0) {
        goto out;
    }
    for (j = 0; j < self->num_rows; j++) {
        err = fprintf(out, "%.3f\t%.3f\t%d\t%d\n", self->left[j], self->right[j],
                self->parent[j], self->child[j]);
        if (err < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

bool
edge_table_equal(edge_table_t *self, edge_table_t *other)
{
    bool ret = false;
    if (self->num_rows == other->num_rows) {
        ret = memcmp(self->left, other->left,
                self->num_rows * sizeof(double)) == 0
            && memcmp(self->right, other->right,
                    self->num_rows * sizeof(double)) == 0
            && memcmp(self->parent, other->parent,
                    self->num_rows * sizeof(node_id_t)) == 0
            && memcmp(self->child, other->child,
                    self->num_rows * sizeof(node_id_t)) == 0;
    }
    return ret;
}


/*************************
 * site table
 *************************/

static int
site_table_expand_main_columns(site_table_t *self, size_t additional_rows)
{
    int ret = 0;
    table_size_t increment = (table_size_t) MSP_MAX(additional_rows, self->max_rows_increment);
    table_size_t new_size = self->max_rows + increment;

    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column((void **) &self->position, new_size, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->ancestral_state_offset, new_size + 1,
                sizeof(uint32_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->metadata_offset, new_size + 1,
                sizeof(uint32_t));
        if (ret != 0) {
            goto out;
        }
        self->max_rows = new_size;
    }
out:
    return ret;
}

static int
site_table_expand_ancestral_state(site_table_t *self, size_t additional_length)
{
    int ret = 0;
    table_size_t increment = (table_size_t) MSP_MAX(additional_length,
            self->max_ancestral_state_length_increment);
    table_size_t new_size = self->max_ancestral_state_length + increment;

    if ((self->ancestral_state_length + additional_length)
            > self->max_ancestral_state_length) {
        ret = expand_column((void **) &self->ancestral_state, new_size, sizeof(char));
        if (ret != 0) {
            goto out;
        }
        self->max_ancestral_state_length = new_size;
    }
out:
    return ret;
}

static int
site_table_expand_metadata(site_table_t *self, size_t additional_length)
{
    int ret = 0;
    table_size_t increment = (table_size_t) MSP_MAX(additional_length,
            self->max_metadata_length_increment);
    table_size_t new_size = self->max_metadata_length + increment;

    if ((self->metadata_length + additional_length)
            > self->max_metadata_length) {
        ret = expand_column((void **) &self->metadata, new_size, sizeof(char));
        if (ret != 0) {
            goto out;
        }
        self->max_metadata_length = new_size;
    }
out:
    return ret;
}

int
site_table_alloc(site_table_t *self, size_t max_rows_increment,
        size_t max_ancestral_state_length_increment,
        size_t max_metadata_length_increment)
{
    int ret = 0;

    memset(self, 0, sizeof(site_table_t));
    if (max_rows_increment == 0) {
        max_rows_increment = DEFAULT_SIZE_INCREMENT;
    }
    if (max_ancestral_state_length_increment == 0) {
        max_ancestral_state_length_increment = DEFAULT_SIZE_INCREMENT;
    }
    if (max_metadata_length_increment == 0) {
        max_metadata_length_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_rows_increment = (table_size_t) max_rows_increment;
    self->max_rows = 0;
    self->num_rows = 0;
    self->max_ancestral_state_length_increment =
        (table_size_t) max_ancestral_state_length_increment;
    self->max_ancestral_state_length = 0;
    self->ancestral_state_length = 0;
    self->max_metadata_length_increment = (table_size_t) max_metadata_length_increment;
    self->max_metadata_length = 0;
    self->metadata_length = 0;
    ret = site_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = site_table_expand_ancestral_state(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = site_table_expand_metadata(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->ancestral_state_offset[0] = 0;
    self->metadata_offset[0] = 0;
out:
    return ret;
}

site_id_t
site_table_add_row(site_table_t *self, double position,
        const char *ancestral_state, table_size_t ancestral_state_length,
        const char *metadata, table_size_t metadata_length)
{
    int ret = 0;
    table_size_t ancestral_state_offset, metadata_offset;

    ret = site_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->position[self->num_rows] = position;

    ancestral_state_offset = (table_size_t) self->ancestral_state_length;
    assert(self->ancestral_state_offset[self->num_rows] == ancestral_state_offset);
    ret = site_table_expand_ancestral_state(self, ancestral_state_length);
    if (ret != 0) {
        goto out;
    }
    self->ancestral_state_length += ancestral_state_length;
    memcpy(self->ancestral_state + ancestral_state_offset, ancestral_state,
            ancestral_state_length);
    self->ancestral_state_offset[self->num_rows + 1] = self->ancestral_state_length;

    metadata_offset = (table_size_t) self->metadata_length;
    assert(self->metadata_offset[self->num_rows] == metadata_offset);
    ret = site_table_expand_metadata(self, metadata_length);
    if (ret != 0) {
        goto out;
    }
    self->metadata_length += metadata_length;
    memcpy(self->metadata + metadata_offset, metadata, metadata_length);
    self->metadata_offset[self->num_rows + 1] = self->metadata_length;

    ret = (site_id_t) self->num_rows;
    self->num_rows++;
out:
    return ret;
}

int
site_table_append_columns(site_table_t *self, size_t num_rows, double *position,
        const char *ancestral_state, table_size_t *ancestral_state_offset,
        const char *metadata, table_size_t *metadata_offset)
{
    int ret = 0;
    table_size_t j, ancestral_state_length, metadata_length;

    if (position == NULL || ancestral_state == NULL || ancestral_state_offset == NULL) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    if ((metadata == NULL) != (metadata_offset == NULL)) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }

    ret = site_table_expand_main_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    memcpy(self->position + self->num_rows, position, num_rows * sizeof(double));

    /* Metadata column */
    if (metadata == NULL) {
        for (j = 0; j < num_rows; j++) {
            self->metadata_offset[self->num_rows + j + 1] = (table_size_t) self->metadata_length;
        }
    } else {
        ret = check_offsets(num_rows, metadata_offset, 0, false);
        if (ret != 0) {
            goto out;
        }
        metadata_length = metadata_offset[num_rows];
        ret = site_table_expand_metadata(self, metadata_length);
        if (ret != 0) {
            goto out;
        }
        memcpy(self->metadata + self->metadata_length, metadata,
                metadata_length * sizeof(char));
        for (j = 0; j < num_rows; j++) {
            self->metadata_offset[self->num_rows + j] =
                (table_size_t) self->metadata_length + metadata_offset[j];
        }
        self->metadata_length += metadata_length;
    }
    self->metadata_offset[self->num_rows + num_rows] = self->metadata_length;

    /* Ancestral state column */
    ret = check_offsets(num_rows, ancestral_state_offset, 0, false);
    if (ret != 0) {
        goto out;
    }
    ancestral_state_length = ancestral_state_offset[num_rows];
    ret = site_table_expand_ancestral_state(self, ancestral_state_length);
    if (ret != 0) {
        goto out;
    }
    memcpy(self->ancestral_state + self->ancestral_state_length, ancestral_state,
            ancestral_state_length * sizeof(char));
    for (j = 0; j < num_rows; j++) {
        self->ancestral_state_offset[self->num_rows + j] =
            (table_size_t) self->ancestral_state_length + ancestral_state_offset[j];
    }
    self->ancestral_state_length += ancestral_state_length;
    self->ancestral_state_offset[self->num_rows + num_rows] = self->ancestral_state_length;

    self->num_rows += (table_size_t) num_rows;
out:
    return ret;
}

int WARN_UNUSED
site_table_copy(site_table_t *self, site_table_t *dest)
{
    return site_table_set_columns(dest, self->num_rows, self->position,
            self->ancestral_state, self->ancestral_state_offset,
            self->metadata, self->metadata_offset);
}

int
site_table_set_columns(site_table_t *self, size_t num_rows, double *position,
        const char *ancestral_state, table_size_t *ancestral_state_offset,
        const char *metadata, table_size_t *metadata_offset)
{
    int ret = 0;

    ret = site_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = site_table_append_columns(self, num_rows, position, ancestral_state,
            ancestral_state_offset, metadata, metadata_offset);
out:
    return ret;
}

bool
site_table_equal(site_table_t *self, site_table_t *other)
{
    bool ret = false;
    if (self->num_rows == other->num_rows
            && self->ancestral_state_length == other->ancestral_state_length
            && self->metadata_length == other->metadata_length) {
        ret = memcmp(self->position, other->position,
                self->num_rows * sizeof(double)) == 0
            && memcmp(self->ancestral_state_offset, other->ancestral_state_offset,
                    (self->num_rows + 1) * sizeof(table_size_t)) == 0
            && memcmp(self->ancestral_state, other->ancestral_state,
                    self->ancestral_state_length * sizeof(char)) == 0
            && memcmp(self->metadata_offset, other->metadata_offset,
                    (self->num_rows + 1) * sizeof(table_size_t)) == 0
            && memcmp(self->metadata, other->metadata,
                    self->metadata_length * sizeof(char)) == 0;
    }
    return ret;
}

int
site_table_clear(site_table_t *self)
{
    self->num_rows = 0;
    self->ancestral_state_length = 0;
    self->ancestral_state_offset[0] = 0;
    self->metadata_length = 0;
    self->metadata_offset[0] = 0;
    return 0;
}

int
site_table_free(site_table_t *self)
{
    msp_safe_free(self->position);
    msp_safe_free(self->ancestral_state);
    msp_safe_free(self->ancestral_state_offset);
    msp_safe_free(self->metadata);
    msp_safe_free(self->metadata_offset);
    return 0;
}

void
site_table_print_state(site_table_t *self, FILE *out)
{
    int ret;

    fprintf(out, TABLE_SEP);
    fprintf(out, "site_table: %p:\n", (void *) self);
    fprintf(out, "num_rows = %d\tmax= %d\tincrement = %d)\n",
            (int) self->num_rows, (int) self->max_rows, (int) self->max_rows_increment);
    fprintf(out, "ancestral_state_length = %d\tmax= %d\tincrement = %d)\n",
            (int) self->ancestral_state_length,
            (int) self->max_ancestral_state_length,
            (int) self->max_ancestral_state_length_increment);
    fprintf(out, "metadata_length = %d\tmax= %d\tincrement = %d)\n",
            (int) self->metadata_length,
            (int) self->max_metadata_length,
            (int) self->max_metadata_length_increment);
    fprintf(out, TABLE_SEP);
    ret = site_table_dump_text(self, out);
    assert(ret == 0);

    assert(self->ancestral_state_offset[0] == 0);
    assert(self->ancestral_state_length
            == self->ancestral_state_offset[self->num_rows]);
    assert(self->metadata_offset[0] == 0);
    assert(self->metadata_length == self->metadata_offset[self->num_rows]);
}

int
site_table_dump_text(site_table_t *self, FILE *out)
{
    size_t j;
    int ret = MSP_ERR_IO;
    int err;
    table_size_t ancestral_state_len, metadata_len;

    err = fprintf(out, "id\tposition\tancestral_state\tmetadata\n");
    if (err < 0) {
        goto out;
    }
    for (j = 0; j < self->num_rows; j++) {
        ancestral_state_len = self->ancestral_state_offset[j + 1] -
            self->ancestral_state_offset[j];
        metadata_len = self->metadata_offset[j + 1] - self->metadata_offset[j];
        err = fprintf(out, "%d\t%f\t%.*s\t%.*s\n", (int) j, self->position[j],
                ancestral_state_len, self->ancestral_state + self->ancestral_state_offset[j],
                metadata_len, self->metadata + self->metadata_offset[j]);
        if (err < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

/*************************
 * mutation table
 *************************/

static int
mutation_table_expand_main_columns(mutation_table_t *self, size_t additional_rows)
{
    int ret = 0;
    table_size_t increment = (table_size_t) MSP_MAX(additional_rows, self->max_rows_increment);
    table_size_t new_size = self->max_rows + increment;

    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column((void **) &self->site, new_size, sizeof(site_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->node, new_size, sizeof(node_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->parent, new_size, sizeof(mutation_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->derived_state_offset, new_size + 1,
                sizeof(table_size_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->metadata_offset, new_size + 1,
                sizeof(table_size_t));
        if (ret != 0) {
            goto out;
        }
        self->max_rows = new_size;
    }
out:
    return ret;
}

static int
mutation_table_expand_derived_state(mutation_table_t *self, size_t additional_length)
{
    int ret = 0;
    table_size_t increment = (table_size_t) MSP_MAX(additional_length,
            self->max_derived_state_length_increment);
    table_size_t new_size = self->max_derived_state_length + increment;

    if ((self->derived_state_length + additional_length)
            > self->max_derived_state_length) {
        ret = expand_column((void **) &self->derived_state, new_size, sizeof(char));
        if (ret != 0) {
            goto out;
        }
        self->max_derived_state_length = (table_size_t) new_size;
    }
out:
    return ret;
}

static int
mutation_table_expand_metadata(mutation_table_t *self, size_t additional_length)
{
    int ret = 0;
    table_size_t increment = (table_size_t) MSP_MAX(additional_length,
            self->max_metadata_length_increment);
    table_size_t new_size = self->max_metadata_length + increment;

    if ((self->metadata_length + additional_length)
            > self->max_metadata_length) {
        ret = expand_column((void **) &self->metadata, new_size, sizeof(char));
        if (ret != 0) {
            goto out;
        }
        self->max_metadata_length = (table_size_t) new_size;
    }
out:
    return ret;
}

int
mutation_table_alloc(mutation_table_t *self, size_t max_rows_increment,
        size_t max_derived_state_length_increment,
        size_t max_metadata_length_increment)
{
    int ret = 0;

    memset(self, 0, sizeof(mutation_table_t));
    if (max_rows_increment == 0) {
        max_rows_increment = DEFAULT_SIZE_INCREMENT;
    }
    if (max_derived_state_length_increment == 0) {
        max_derived_state_length_increment = DEFAULT_SIZE_INCREMENT;
    }
    if (max_metadata_length_increment == 0) {
        max_metadata_length_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_rows_increment = (table_size_t) max_rows_increment;
    self->max_rows = 0;
    self->num_rows = 0;
    self->max_derived_state_length_increment =
        (table_size_t) max_derived_state_length_increment;
    self->max_derived_state_length = 0;
    self->derived_state_length = 0;
    self->max_metadata_length_increment =
        (table_size_t) max_metadata_length_increment;
    self->max_metadata_length = 0;
    self->metadata_length = 0;
    ret = mutation_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = mutation_table_expand_derived_state(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = mutation_table_expand_metadata(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->derived_state_offset[0] = 0;
    self->metadata_offset[0] = 0;
out:
    return ret;
}

mutation_id_t
mutation_table_add_row(mutation_table_t *self, site_id_t site, node_id_t node,
        mutation_id_t parent,
        const char *derived_state, table_size_t derived_state_length,
        const char *metadata, table_size_t metadata_length)
{
    table_size_t derived_state_offset, metadata_offset;
    int ret;

    ret = mutation_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->site[self->num_rows] = site;
    self->node[self->num_rows] = node;
    self->parent[self->num_rows] = parent;

    derived_state_offset = (table_size_t) self->derived_state_length;
    assert(self->derived_state_offset[self->num_rows] == derived_state_offset);
    ret = mutation_table_expand_derived_state(self, derived_state_length);
    if (ret != 0) {
        goto out;
    }
    self->derived_state_length += derived_state_length;
    memcpy(self->derived_state + derived_state_offset, derived_state,
            derived_state_length);
    self->derived_state_offset[self->num_rows + 1] = self->derived_state_length;

    metadata_offset = (table_size_t) self->metadata_length;
    assert(self->metadata_offset[self->num_rows] == metadata_offset);
    ret = mutation_table_expand_metadata(self, metadata_length);
    if (ret != 0) {
        goto out;
    }
    self->metadata_length += metadata_length;
    memcpy(self->metadata + metadata_offset, metadata, metadata_length);
    self->metadata_offset[self->num_rows + 1] = self->metadata_length;

    ret = (mutation_id_t) self->num_rows;
    self->num_rows++;
out:
    return ret;
}

int
mutation_table_append_columns(mutation_table_t *self, size_t num_rows, site_id_t *site,
        node_id_t *node, mutation_id_t *parent,
        const char *derived_state, table_size_t *derived_state_offset,
        const char *metadata, table_size_t *metadata_offset)
{
    int ret = 0;
    table_size_t j, derived_state_length, metadata_length;

    if (site  == NULL || node == NULL || derived_state == NULL
            || derived_state_offset == NULL) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    if ((metadata == NULL) != (metadata_offset == NULL)) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }

    ret = mutation_table_expand_main_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    memcpy(self->site + self->num_rows, site, num_rows * sizeof(site_id_t));
    memcpy(self->node + self->num_rows, node, num_rows * sizeof(node_id_t));
    if (parent == NULL) {
        /* If parent is NULL, set all parents to the null mutation */
        memset(self->parent + self->num_rows, 0xff, num_rows * sizeof(mutation_id_t));
    } else {
        memcpy(self->parent + self->num_rows, parent, num_rows * sizeof(mutation_id_t));
    }

    /* Metadata column */
    if (metadata == NULL) {
        for (j = 0; j < num_rows; j++) {
            self->metadata_offset[self->num_rows + j + 1] = (table_size_t) self->metadata_length;
        }
    } else {
        ret = check_offsets(num_rows, metadata_offset, 0, false);
        if (ret != 0) {
            goto out;
        }
        metadata_length = metadata_offset[num_rows];
        ret = mutation_table_expand_metadata(self, metadata_length);
        if (ret != 0) {
            goto out;
        }
        memcpy(self->metadata + self->metadata_length, metadata,
                metadata_length * sizeof(char));
        for (j = 0; j < num_rows; j++) {
            self->metadata_offset[self->num_rows + j] =
                (table_size_t) self->metadata_length + metadata_offset[j];
        }
        self->metadata_length += metadata_length;
    }
    self->metadata_offset[self->num_rows + num_rows] = self->metadata_length;

    /* Derived state column */
    ret = check_offsets(num_rows, derived_state_offset, 0, false);
    if (ret != 0) {
        goto out;
    }
    derived_state_length = derived_state_offset[num_rows];
    ret = mutation_table_expand_derived_state(self, derived_state_length);
    if (ret != 0) {
        goto out;
    }
    memcpy(self->derived_state + self->derived_state_length, derived_state,
            derived_state_length * sizeof(char));
    for (j = 0; j < num_rows; j++) {
        self->derived_state_offset[self->num_rows + j] =
            (table_size_t) self->derived_state_length + derived_state_offset[j];
    }
    self->derived_state_length += derived_state_length;
    self->derived_state_offset[self->num_rows + num_rows] = self->derived_state_length;

    self->num_rows += (table_size_t) num_rows;
out:
    return ret;
}

int WARN_UNUSED
mutation_table_copy(mutation_table_t *self, mutation_table_t *dest)
{
    return mutation_table_set_columns(dest, self->num_rows,
            self->site, self->node, self->parent,
            self->derived_state, self->derived_state_offset,
            self->metadata, self->metadata_offset);
}

int
mutation_table_set_columns(mutation_table_t *self, size_t num_rows, site_id_t *site,
        node_id_t *node, mutation_id_t *parent,
        const char *derived_state, table_size_t *derived_state_offset,
        const char *metadata, table_size_t *metadata_offset)
{
    int ret = 0;

    ret = mutation_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = mutation_table_append_columns(self, num_rows, site, node, parent,
            derived_state, derived_state_offset, metadata, metadata_offset);
out:
    return ret;
}

bool
mutation_table_equal(mutation_table_t *self, mutation_table_t *other)
{
    bool ret = false;
    if (self->num_rows == other->num_rows
            && self->derived_state_length == other->derived_state_length
            && self->metadata_length == other->metadata_length) {
        ret = memcmp(self->site, other->site, self->num_rows * sizeof(site_id_t)) == 0
            && memcmp(self->node, other->node, self->num_rows * sizeof(node_id_t)) == 0
            && memcmp(self->parent, other->parent,
                    self->num_rows * sizeof(mutation_id_t)) == 0
            && memcmp(self->derived_state_offset, other->derived_state_offset,
                    (self->num_rows + 1) * sizeof(table_size_t)) == 0
            && memcmp(self->derived_state, other->derived_state,
                    self->derived_state_length * sizeof(char)) == 0
            && memcmp(self->metadata_offset, other->metadata_offset,
                    (self->num_rows + 1) * sizeof(table_size_t)) == 0
            && memcmp(self->metadata, other->metadata,
                    self->metadata_length * sizeof(char)) == 0;
    }
    return ret;
}

int
mutation_table_clear(mutation_table_t *self)
{
    self->num_rows = 0;
    self->derived_state_length = 0;
    self->derived_state_offset[0] = 0;
    self->metadata_length = 0;
    self->metadata_offset[0] = 0;
    return 0;
}

int
mutation_table_free(mutation_table_t *self)
{
    msp_safe_free(self->node);
    msp_safe_free(self->site);
    msp_safe_free(self->parent);
    msp_safe_free(self->derived_state);
    msp_safe_free(self->derived_state_offset);
    msp_safe_free(self->metadata);
    msp_safe_free(self->metadata_offset);
    return 0;
}

void
mutation_table_print_state(mutation_table_t *self, FILE *out)
{
    int ret;

    fprintf(out, TABLE_SEP);
    fprintf(out, "mutation_table: %p:\n", (void *) self);
    fprintf(out, "num_rows = %d\tmax= %d\tincrement = %d)\n",
            (int) self->num_rows, (int) self->max_rows, (int) self->max_rows_increment);
    fprintf(out, "derived_state_length = %d\tmax= %d\tincrement = %d)\n",
            (int) self->derived_state_length,
            (int) self->max_derived_state_length,
            (int) self->max_derived_state_length_increment);
    fprintf(out, "metadata_length = %d\tmax= %d\tincrement = %d)\n",
            (int) self->metadata_length,
            (int) self->max_metadata_length,
            (int) self->max_metadata_length_increment);
    fprintf(out, TABLE_SEP);
    ret = mutation_table_dump_text(self, out);
    assert(ret == 0);
    assert(self->derived_state_offset[0] == 0);
    assert(self->derived_state_length
            == self->derived_state_offset[self->num_rows]);
    assert(self->metadata_offset[0] == 0);
    assert(self->metadata_length
            == self->metadata_offset[self->num_rows]);
}

int
mutation_table_dump_text(mutation_table_t *self, FILE *out)
{
    size_t j;
    int ret = MSP_ERR_IO;
    int err;
    table_size_t derived_state_len, metadata_len;

    err = fprintf(out, "id\tsite\tnode\tparent\tderived_state\tmetadata\n");
    if (err < 0) {
        goto out;
    }
    for (j = 0; j < self->num_rows; j++) {
        derived_state_len = self->derived_state_offset[j + 1] -
            self->derived_state_offset[j];
        metadata_len = self->metadata_offset[j + 1] - self->metadata_offset[j];
        err = fprintf(out, "%d\t%d\t%d\t%d\t%.*s\t%.*s\n", (int) j,
                self->site[j], self->node[j], self->parent[j],
                derived_state_len, self->derived_state + self->derived_state_offset[j],
                metadata_len, self->metadata + self->metadata_offset[j]);
        if (err < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

/*************************
 * migration table
 *************************/

static int
migration_table_expand(migration_table_t *self, size_t additional_rows)
{
    int ret = 0;
    size_t increment = MSP_MAX(additional_rows, self->max_rows_increment);
    size_t new_size = self->max_rows + increment;

    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column((void **) &self->left, new_size, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->right, new_size, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->node, new_size, sizeof(node_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->source, new_size, sizeof(population_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->dest, new_size, sizeof(population_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->time, new_size, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        self->max_rows = new_size;
    }
out:
    return ret;
}

int
migration_table_alloc(migration_table_t *self, size_t max_rows_increment)
{
    int ret = 0;

    memset(self, 0, sizeof(migration_table_t));
    if (max_rows_increment == 0) {
        max_rows_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_rows_increment = max_rows_increment;
    self->max_rows = 0;
    self->num_rows = 0;
    ret = migration_table_expand(self, 1);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

int
migration_table_append_columns(migration_table_t *self, size_t num_rows, double *left,
        double *right, node_id_t *node, population_id_t *source, population_id_t *dest,
        double *time)
{
    int ret;

    ret = migration_table_expand(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    if (left == NULL || right == NULL || node == NULL || source == NULL
            || dest == NULL || time == NULL) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    memcpy(self->left + self->num_rows, left, num_rows * sizeof(double));
    memcpy(self->right + self->num_rows, right, num_rows * sizeof(double));
    memcpy(self->node + self->num_rows, node, num_rows * sizeof(node_id_t));
    memcpy(self->source + self->num_rows, source, num_rows * sizeof(population_id_t));
    memcpy(self->dest + self->num_rows, dest, num_rows * sizeof(population_id_t));
    memcpy(self->time + self->num_rows, time, num_rows * sizeof(double));
    self->num_rows += num_rows;
out:
    return ret;
}

int WARN_UNUSED
migration_table_copy(migration_table_t *self, migration_table_t *dest)
{
    return migration_table_set_columns(dest, self->num_rows,
            self->left, self->right, self->node,
            self->source, self->dest, self->time);
}

int
migration_table_set_columns(migration_table_t *self, size_t num_rows, double *left,
        double *right, node_id_t *node, population_id_t *source, population_id_t *dest,
        double *time)
{
    int ret;

    ret = migration_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = migration_table_append_columns(self, num_rows, left, right, node, source,
            dest, time);
out:
    return ret;
}

migration_id_t
migration_table_add_row(migration_table_t *self, double left, double right,
        node_id_t node, population_id_t source, population_id_t dest, double time)
{
    int ret = 0;

    ret = migration_table_expand(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->left[self->num_rows] = left;
    self->right[self->num_rows] = right;
    self->node[self->num_rows] = node;
    self->source[self->num_rows] = source;
    self->dest[self->num_rows] = dest;
    self->time[self->num_rows] = time;
    ret = (migration_id_t) self->num_rows;
    self->num_rows++;
out:
    return ret;
}

int
migration_table_clear(migration_table_t *self)
{
    self->num_rows = 0;
    return 0;
}

int
migration_table_free(migration_table_t *self)
{
    msp_safe_free(self->left);
    msp_safe_free(self->right);
    msp_safe_free(self->node);
    msp_safe_free(self->source);
    msp_safe_free(self->dest);
    msp_safe_free(self->time);
    return 0;
}

void
migration_table_print_state(migration_table_t *self, FILE *out)
{
    int ret;

    fprintf(out, TABLE_SEP);
    fprintf(out, "migration_table: %p:\n", (void *) self);
    fprintf(out, "num_rows = %d\tmax= %d\tincrement = %d)\n",
            (int) self->num_rows, (int) self->max_rows, (int) self->max_rows_increment);
    fprintf(out, TABLE_SEP);
    ret = migration_table_dump_text(self, out);
    assert(ret == 0);
}

int
migration_table_dump_text(migration_table_t *self, FILE *out)
{
    size_t j;
    int ret = MSP_ERR_IO;
    int err;

    err = fprintf(out, "left\tright\tnode\tsource\tdest\ttime\n");
    if (err < 0) {
        goto out;
    }
    for (j = 0; j < self->num_rows; j++) {
        err = fprintf(out, "%.3f\t%.3f\t%d\t%d\t%d\t%f\n", self->left[j],
                self->right[j], (int) self->node[j], (int) self->source[j],
                (int) self->dest[j], self->time[j]);
        if (err < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}


/*************************
 * provenance table
 *************************/

static int
provenance_table_expand_main_columns(provenance_table_t *self, table_size_t additional_rows)
{
    int ret = 0;
    table_size_t increment = MSP_MAX(additional_rows, self->max_rows_increment);
    table_size_t new_size = self->max_rows + increment;

    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column((void **) &self->timestamp_offset, new_size + 1,
                sizeof(table_size_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->record_offset, new_size + 1,
                sizeof(table_size_t));
        if (ret != 0) {
            goto out;
        }
        self->max_rows = new_size;
    }
out:
    return ret;
}

static int
provenance_table_expand_timestamp(provenance_table_t *self, table_size_t additional_length)
{
    int ret = 0;
    table_size_t increment = MSP_MAX(additional_length,
            self->max_timestamp_length_increment);
    table_size_t new_size = self->max_timestamp_length + increment;

    if ((self->timestamp_length + additional_length) > self->max_timestamp_length) {
        ret = expand_column((void **) &self->timestamp, new_size, sizeof(char *));
        if (ret != 0) {
            goto out;
        }
        self->max_timestamp_length = new_size;
    }
out:
    return ret;
}

static int
provenance_table_expand_provenance(provenance_table_t *self, table_size_t additional_length)
{
    int ret = 0;
    table_size_t increment = MSP_MAX(additional_length,
            self->max_record_length_increment);
    table_size_t new_size = self->max_record_length + increment;

    if ((self->record_length + additional_length) > self->max_record_length) {
        ret = expand_column((void **) &self->record, new_size, sizeof(char *));
        if (ret != 0) {
            goto out;
        }
        self->max_record_length = new_size;
    }
out:
    return ret;
}

int
provenance_table_alloc(provenance_table_t *self, size_t max_rows_increment,
        size_t max_timestamp_length_increment, size_t max_record_length_increment)
{
    int ret = 0;

    memset(self, 0, sizeof(provenance_table_t));
    if (max_rows_increment == 0) {
       max_rows_increment = DEFAULT_SIZE_INCREMENT;
    }
    if (max_timestamp_length_increment == 0) {
        max_timestamp_length_increment = DEFAULT_SIZE_INCREMENT;
    }
    if (max_record_length_increment == 0) {
        max_record_length_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_rows_increment = (table_size_t) max_rows_increment;
    self->max_timestamp_length_increment = (table_size_t) max_timestamp_length_increment;
    self->max_record_length_increment = (table_size_t) max_record_length_increment;
    self->max_rows = 0;
    self->num_rows = 0;
    self->max_timestamp_length = 0;
    self->timestamp_length = 0;
    self->max_record_length = 0;
    self->record_length = 0;
    ret = provenance_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = provenance_table_expand_timestamp(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->timestamp_offset[0] = 0;
    ret = provenance_table_expand_provenance(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->record_offset[0] = 0;
out:
    return ret;
}

int WARN_UNUSED
provenance_table_copy(provenance_table_t *self, provenance_table_t *dest)
{
    return provenance_table_set_columns(dest, self->num_rows,
            self->timestamp, self->timestamp_offset,
            self->record, self->record_offset);
}

int
provenance_table_set_columns(provenance_table_t *self, size_t num_rows,
        char *timestamp, uint32_t *timestamp_offset,
        char *record, uint32_t *record_offset)
{
    int ret;

    ret = provenance_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = provenance_table_append_columns(self, num_rows,
            timestamp, timestamp_offset, record, record_offset);
out:
    return ret;
}

int
provenance_table_append_columns(provenance_table_t *self, size_t num_rows,
        char *timestamp, uint32_t *timestamp_offset,
        char *record, uint32_t *record_offset)
{
    int ret;
    table_size_t j, timestamp_length, record_length;

    if (timestamp == NULL || timestamp_offset == NULL ||
            record == NULL || record_offset == NULL) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    ret = provenance_table_expand_main_columns(self, (table_size_t) num_rows);
    if (ret != 0) {
        goto out;
    }

    ret = check_offsets(num_rows, timestamp_offset, 0, false);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < num_rows; j++) {
        self->timestamp_offset[self->num_rows + j] =
            (table_size_t) self->timestamp_length + timestamp_offset[j];
    }
    timestamp_length = timestamp_offset[num_rows];
    ret = provenance_table_expand_timestamp(self, timestamp_length);
    if (ret != 0) {
        goto out;
    }
    memcpy(self->timestamp + self->timestamp_length, timestamp,
            timestamp_length * sizeof(char));
    self->timestamp_length += timestamp_length;

    ret = check_offsets(num_rows, record_offset, 0, false);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < num_rows; j++) {
        self->record_offset[self->num_rows + j] =
            (table_size_t) self->record_length + record_offset[j];
    }
    record_length = record_offset[num_rows];
    ret = provenance_table_expand_provenance(self, record_length);
    if (ret != 0) {
        goto out;
    }
    memcpy(self->record + self->record_length, record, record_length * sizeof(char));
    self->record_length += record_length;

    self->num_rows += (table_size_t) num_rows;
    self->timestamp_offset[self->num_rows] = self->timestamp_length;
    self->record_offset[self->num_rows] = self->record_length;
out:
    return ret;
}

static provenance_id_t
provenance_table_add_row_internal(provenance_table_t *self,
        const char *timestamp, table_size_t timestamp_length,
        const char *record, table_size_t record_length)
{
    int ret = 0;

    assert(self->num_rows < self->max_rows);
    assert(self->timestamp_length + timestamp_length < self->max_timestamp_length);
    memcpy(self->timestamp + self->timestamp_length, timestamp, timestamp_length);
    self->timestamp_offset[self->num_rows + 1] = self->timestamp_length + timestamp_length;
    self->timestamp_length += timestamp_length;
    assert(self->record_length + record_length < self->max_record_length);
    memcpy(self->record + self->record_length, record, record_length);
    self->record_offset[self->num_rows + 1] = self->record_length + record_length;
    self->record_length += record_length;
    ret = (provenance_id_t) self->num_rows;
    self->num_rows++;
    return ret;
}

provenance_id_t
provenance_table_add_row(provenance_table_t *self,
        const char *timestamp, size_t timestamp_length,
        const char *record, size_t record_length)
{
    int ret = 0;

    ret = provenance_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = provenance_table_expand_timestamp(self, (table_size_t) timestamp_length);
    if (ret != 0) {
        goto out;
    }
    ret = provenance_table_expand_provenance(self, (table_size_t) record_length);
    if (ret != 0) {
        goto out;
    }
    ret = provenance_table_add_row_internal(self,
            timestamp, (table_size_t) timestamp_length,
            record, (table_size_t) record_length);
out:
    return ret;
}

int
provenance_table_clear(provenance_table_t *self)
{
    self->num_rows = 0;
    self->timestamp_length = 0;
    self->record_length = 0;
    return 0;
}

int
provenance_table_free(provenance_table_t *self)
{
    msp_safe_free(self->timestamp);
    msp_safe_free(self->timestamp_offset);
    msp_safe_free(self->record);
    msp_safe_free(self->record_offset);
    return 0;
}

void
provenance_table_print_state(provenance_table_t *self, FILE *out)
{
    size_t j, k;

    fprintf(out, TABLE_SEP);
    fprintf(out, "provenance_table: %p:\n", (void *) self);
    fprintf(out, "num_rows          = %d\tmax= %d\tincrement = %d)\n",
            (int) self->num_rows, (int) self->max_rows, (int) self->max_rows_increment);
    fprintf(out, "timestamp_length  = %d\tmax= %d\tincrement = %d)\n",
            (int) self->timestamp_length,
            (int) self->max_timestamp_length,
            (int) self->max_timestamp_length_increment);
    fprintf(out, "record_length = %d\tmax= %d\tincrement = %d)\n",
            (int) self->record_length,
            (int) self->max_record_length,
            (int) self->max_record_length_increment);
    fprintf(out, TABLE_SEP);
    fprintf(out, "index\ttimestamp_offset\ttimestamp\trecord_offset\tprovenance\n");
    for (j = 0; j < self->num_rows; j++) {
        fprintf(out, "%d\t%d\t", (int) j, self->timestamp_offset[j]);
        for (k = self->timestamp_offset[j]; k < self->timestamp_offset[j + 1]; k++) {
            fprintf(out, "%c", self->timestamp[k]);
        }
        fprintf(out, "\t%d\t", self->record_offset[j]);
        for (k = self->record_offset[j]; k < self->record_offset[j + 1]; k++) {
            fprintf(out, "%c", self->record[k]);
        }
        fprintf(out, "\n");
    }
    assert(self->timestamp_offset[0] == 0);
    assert(self->timestamp_offset[self->num_rows] == self->timestamp_length);
    assert(self->record_offset[0] == 0);
    assert(self->record_offset[self->num_rows] == self->record_length);
}

bool
provenance_table_equal(provenance_table_t *self, provenance_table_t *other)
{
    bool ret = false;
    if (self->num_rows == other->num_rows
            && self->timestamp_length == other->timestamp_length) {
        ret = memcmp(self->timestamp_offset, other->timestamp_offset,
                    (self->num_rows + 1) * sizeof(table_size_t)) == 0
            && memcmp(self->timestamp, other->timestamp,
                    self->timestamp_length * sizeof(char)) == 0
            && memcmp(self->record_offset, other->record_offset,
                    (self->num_rows + 1) * sizeof(table_size_t)) == 0
            && memcmp(self->record, other->record,
                    self->record_length * sizeof(char)) == 0;
    }
    return ret;
}

/*************************
 * sort_tables
 *************************/

typedef struct {
    double left;
    double right;
    node_id_t parent;
    node_id_t child;
    double time;
} edge_sort_t;

typedef struct {
    /* Input tables. */
    node_table_t *nodes;
    edge_table_t *edges;
    site_table_t *sites;
    mutation_table_t *mutations;
    migration_table_t *migrations;
    /* Mapping from input site IDs to output site IDs */
    site_id_t *site_id_map;
} table_sorter_t;

static int
cmp_site(const void *a, const void *b) {
    const site_t *ia = (const site_t *) a;
    const site_t *ib = (const site_t *) b;
    /* Compare sites by position */
    return (ia->position > ib->position) - (ia->position < ib->position);
}

static int
cmp_mutation(const void *a, const void *b) {
    const mutation_t *ia = (const mutation_t *) a;
    const mutation_t *ib = (const mutation_t *) b;
    /* Compare mutations by site */
    int ret = (ia->site > ib->site) - (ia->site < ib->site);
    if (ret == 0) {
        /* Within a particular site sort by ID. This ensures that relative ordering
         * within a site is maintained */
        ret = (ia->id > ib->id) - (ia->id < ib->id);
    }
    return ret;
}

static int
cmp_edge(const void *a, const void *b) {
    const edge_sort_t *ca = (const edge_sort_t *) a;
    const edge_sort_t *cb = (const edge_sort_t *) b;

    int ret = (ca->time > cb->time) - (ca->time < cb->time);
    /* If time values are equal, sort by the parent node */
    if (ret == 0) {
        ret = (ca->parent > cb->parent) - (ca->parent < cb->parent);
        /* If the parent nodes are equal, sort by the child ID. */
        if (ret == 0) {
            ret = (ca->child > cb->child) - (ca->child < cb->child);
            /* If the child nodes are equal, sort by the left coordinate. */
            if (ret == 0) {
                ret = (ca->left > cb->left) - (ca->left < cb->left);
            }
        }
    }
    return ret;
}

static int
table_sorter_alloc(table_sorter_t *self, node_table_t *nodes, edge_table_t *edges,
        site_table_t *sites, mutation_table_t *mutations, migration_table_t *migrations)
{
    int ret = 0;

    memset(self, 0, sizeof(table_sorter_t));
    if (nodes == NULL || edges == NULL) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    self->nodes = nodes;
    self->edges = edges;
    self->mutations = mutations;
    self->sites = sites;
    self->migrations = migrations;

    if (self->sites != NULL) {
        /* If you provide a site table, you must provide a mutation table (even if it is
         * empty */
        if (self->mutations == NULL) {
            ret = MSP_ERR_BAD_PARAM_VALUE;
            goto out;
        }
        self->site_id_map = malloc(sites->num_rows * sizeof(site_id_t));
        if (self->site_id_map == NULL) {
            ret = MSP_ERR_NO_MEMORY;
            goto out;
        }
    }
out:
    return ret;
}

static int
table_sorter_sort_edges(table_sorter_t *self, size_t start)
{
    int ret = 0;
    edge_sort_t *e;
    size_t j, k;
    size_t n = self->edges->num_rows - start;
    edge_sort_t *sorted_edges = malloc(n * sizeof(*sorted_edges));

    if (sorted_edges == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    for (j = 0; j < n; j++) {
        e = sorted_edges + j;
        k = start + j;
        e->left = self->edges->left[k];
        e->right = self->edges->right[k];
        e->parent = self->edges->parent[k];
        e->child = self->edges->child[k];
        if (e->parent >= (node_id_t) self->nodes->num_rows) {
            ret = MSP_ERR_OUT_OF_BOUNDS;
            goto out;
        }
        e->time = self->nodes->time[e->parent];
    }
    qsort(sorted_edges, n, sizeof(edge_sort_t), cmp_edge);
    /* Copy the edges back into the table. */
    for (j = 0; j < n; j++) {
        e = sorted_edges + j;
        k = start + j;
        self->edges->left[k] = e->left;
        self->edges->right[k] = e->right;
        self->edges->parent[k] = e->parent;
        self->edges->child[k] = e->child;
    }
out:
    msp_safe_free(sorted_edges);
    return ret;
}

static int
table_sorter_sort_sites(table_sorter_t *self)
{
    int ret = 0;
    table_size_t j, ancestral_state_offset, metadata_offset, length;
    site_t *sorted_sites = malloc(self->sites->num_rows * sizeof(*sorted_sites));
    char *ancestral_state_mem = malloc(self->sites->ancestral_state_length *
            sizeof(*ancestral_state_mem));
    char *metadata_mem = malloc(self->sites->metadata_length *
            sizeof(*metadata_mem));

    if (sorted_sites == NULL || ancestral_state_mem == NULL || metadata_mem == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    memcpy(ancestral_state_mem, self->sites->ancestral_state,
            self->sites->ancestral_state_length * sizeof(char));
    memcpy(metadata_mem, self->sites->metadata,
            self->sites->metadata_length * sizeof(char));
    for (j = 0; j < self->sites->num_rows; j++) {
        sorted_sites[j].id = (site_id_t) j;
        sorted_sites[j].position = self->sites->position[j];
        ancestral_state_offset = self->sites->ancestral_state_offset[j];
        length = self->sites->ancestral_state_offset[j + 1] - ancestral_state_offset;
        sorted_sites[j].ancestral_state_length = length;
        sorted_sites[j].ancestral_state = ancestral_state_mem + ancestral_state_offset;
        metadata_offset = self->sites->metadata_offset[j];
        length = self->sites->metadata_offset[j + 1] - metadata_offset;
        sorted_sites[j].metadata_length = length;
        sorted_sites[j].metadata = metadata_mem + metadata_offset;
    }

    /* Sort the sites by position */
    qsort(sorted_sites, self->sites->num_rows, sizeof(*sorted_sites), cmp_site);

    /* Build the mapping from old site IDs to new site IDs and copy back into the table */
    ancestral_state_offset = 0;
    metadata_offset = 0;
    for (j = 0; j < self->sites->num_rows; j++) {
        self->site_id_map[sorted_sites[j].id] = (site_id_t) j;
        self->sites->position[j] = sorted_sites[j].position;
        self->sites->ancestral_state_offset[j] = ancestral_state_offset;
        memcpy(self->sites->ancestral_state + ancestral_state_offset,
            sorted_sites[j].ancestral_state,
            sorted_sites[j].ancestral_state_length);
        ancestral_state_offset += sorted_sites[j].ancestral_state_length;
        self->sites->metadata_offset[j] = metadata_offset;
        memcpy(self->sites->metadata + metadata_offset,
            sorted_sites[j].metadata,
            sorted_sites[j].metadata_length);
        metadata_offset += sorted_sites[j].metadata_length;
    }
    self->sites->ancestral_state_offset[self->sites->num_rows] = ancestral_state_offset;
    self->sites->metadata_offset[self->sites->num_rows] = metadata_offset;
out:
    msp_safe_free(sorted_sites);
    msp_safe_free(ancestral_state_mem);
    msp_safe_free(metadata_mem);
    return ret;
}

static int
table_sorter_sort_mutations(table_sorter_t *self)
{
    int ret = 0;
    site_id_t site;
    node_id_t node;
    table_size_t j, derived_state_offset, metadata_offset, length;
    mutation_id_t parent, mapped_parent;
    mutation_t *sorted_mutations = malloc(self->mutations->num_rows *
            sizeof(*sorted_mutations));
    mutation_id_t *mutation_id_map = malloc(self->mutations->num_rows
            * sizeof(*mutation_id_map));
    char *derived_state_mem = malloc(self->mutations->derived_state_length
            * sizeof(*derived_state_mem));
    char *metadata_mem = malloc(self->mutations->metadata_length
            * sizeof(*metadata_mem));

    if (mutation_id_map == NULL || derived_state_mem == NULL
            || sorted_mutations == NULL || metadata_mem == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }

    memcpy(derived_state_mem, self->mutations->derived_state,
            self->mutations->derived_state_length * sizeof(*derived_state_mem));
    memcpy(metadata_mem, self->mutations->metadata,
            self->mutations->metadata_length * sizeof(*metadata_mem));
    for (j = 0; j < self->mutations->num_rows; j++) {
        site = self->mutations->site[j];
        if (site >= (site_id_t) self->sites->num_rows) {
            ret = MSP_ERR_OUT_OF_BOUNDS;
            goto out;
        }
        node = self->mutations->node[j];
        if (node >= (node_id_t) self->nodes->num_rows) {
            ret = MSP_ERR_OUT_OF_BOUNDS;
            goto out;
        }
        parent = self->mutations->parent[j];
        if (parent != MSP_NULL_MUTATION) {
            if (parent < 0 || parent >= (mutation_id_t) self->mutations->num_rows) {
                ret = MSP_ERR_MUTATION_OUT_OF_BOUNDS;
                goto out;
            }
        }
        sorted_mutations[j].id = (mutation_id_t) j;
        sorted_mutations[j].site = self->site_id_map[site];
        sorted_mutations[j].node = node;
        sorted_mutations[j].parent = self->mutations->parent[j];
        derived_state_offset = self->mutations->derived_state_offset[j];
        length = self->mutations->derived_state_offset[j + 1] - derived_state_offset;
        sorted_mutations[j].derived_state_length = length;
        sorted_mutations[j].derived_state = derived_state_mem + derived_state_offset;
        metadata_offset = self->mutations->metadata_offset[j];
        length = self->mutations->metadata_offset[j + 1] - metadata_offset;
        sorted_mutations[j].metadata_length = length;
        sorted_mutations[j].metadata = metadata_mem + metadata_offset;
    }

    qsort(sorted_mutations, self->mutations->num_rows, sizeof(*sorted_mutations),
        cmp_mutation);

    /* Make a first pass through the sorted mutations to build the ID map. */
    for (j = 0; j < self->mutations->num_rows; j++) {
        mutation_id_map[sorted_mutations[j].id] = (mutation_id_t) j;
    }
    derived_state_offset = 0;
    metadata_offset = 0;
    /* Copy the sorted mutations back into the table */
    for (j = 0; j < self->mutations->num_rows; j++) {
        self->mutations->site[j] = sorted_mutations[j].site;
        self->mutations->node[j] = sorted_mutations[j].node;
        mapped_parent = MSP_NULL_MUTATION;
        parent = sorted_mutations[j].parent;
        if (parent != MSP_NULL_MUTATION) {
            mapped_parent = mutation_id_map[parent];
        }
        self->mutations->parent[j] = mapped_parent;
        self->mutations->derived_state_offset[j] = derived_state_offset;
        memcpy(self->mutations->derived_state + derived_state_offset,
            sorted_mutations[j].derived_state,
            sorted_mutations[j].derived_state_length * sizeof(char));
        derived_state_offset += sorted_mutations[j].derived_state_length;
        self->mutations->metadata_offset[j] = metadata_offset;
        memcpy(self->mutations->metadata + metadata_offset,
            sorted_mutations[j].metadata,
            sorted_mutations[j].metadata_length * sizeof(char));
        metadata_offset += sorted_mutations[j].metadata_length;
    }
    self->mutations->derived_state_offset[self->mutations->num_rows] = derived_state_offset;
    self->mutations->metadata_offset[self->mutations->num_rows] = metadata_offset;
out:
    msp_safe_free(mutation_id_map);
    msp_safe_free(sorted_mutations);
    msp_safe_free(derived_state_mem);
    msp_safe_free(metadata_mem);
    return ret;
}

static int
table_sorter_run(table_sorter_t *self, size_t edge_start)
{
    int ret = 0;

    if (edge_start > self->edges->num_rows) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    ret = table_sorter_sort_edges(self, edge_start);
    if (ret != 0) {
        goto out;
    }
    if (self->sites != NULL) {
        ret = table_sorter_sort_sites(self);
        if (ret != 0) {
            goto out;
        }
        ret = table_sorter_sort_mutations(self);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

static void
table_sorter_free(table_sorter_t *self)
{
    msp_safe_free(self->site_id_map);
}

int
sort_tables(node_table_t *nodes, edge_table_t *edges, migration_table_t *migrations,
        site_table_t *sites, mutation_table_t *mutations, size_t edge_start)
{
    int ret = 0;
    table_sorter_t *sorter = NULL;

    sorter = malloc(sizeof(table_sorter_t));
    if (sorter == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    ret = table_sorter_alloc(sorter, nodes, edges, sites, mutations, migrations);
    if (ret != 0) {
        goto out;
    }
    ret = table_sorter_run(sorter, edge_start);
out:
    if (sorter != NULL) {
        table_sorter_free(sorter);
        free(sorter);
    }
    return ret;
}

/*************************
 * simplifier
 *************************/

/* For the segment priority queue we want to sort on the left
 * coordinate and to break ties we use the node */
static int
cmp_segment_queue(const void *a, const void *b) {
    const simplify_segment_t *ia = (const simplify_segment_t *) a;
    const simplify_segment_t *ib = (const simplify_segment_t *) b;
    int ret = (ia->left > ib->left) - (ia->left < ib->left);
    if (ret == 0)  {
        ret = (ia->node > ib->node) - (ia->node < ib->node);
    }
    return ret;
}

static int
cmp_mutation_position_map(const void *a, const void *b) {
    const mutation_position_map_t *ia = (const mutation_position_map_t *) a;
    const mutation_position_map_t *ib = (const mutation_position_map_t *) b;
    int ret = (ia->position > ib->position) - (ia->position < ib->position);
    return ret;
}

static void
simplifier_check_state(simplifier_t *self)
{
    size_t j;
    size_t total_segments = 0;
    size_t total_avl_nodes = 0;
    site_id_t site;
    avl_node_t *a;
    simplify_segment_t *u;
    mutation_node_list_t *mnl;
    mutation_position_map_t *mpm;

    for (j = 0; j < self->input_nodes.num_rows; j++) {
        for (u = self->ancestor_map[j]; u != NULL; u = u->next) {
            assert(u->left < u->right);
            if (u->next != NULL) {
                assert(u->right <= u->next->left);
            }
            total_segments++;
        }
        for (a = self->mutation_position_map[j].head; a != NULL; a = a->next) {
            mpm = (mutation_position_map_t *) a->item;
            assert(mpm->head != NULL);
            for (mnl = mpm->head; mnl != NULL; mnl = mnl->next) {
                site = self->input_mutations.site[mnl->mutation_id];
                assert(self->input_mutations.node[mnl->mutation_id] == (node_id_t) j);
                assert(self->input_sites.position[site] == mpm->position);
            }
        }
        total_avl_nodes += avl_count(&self->mutation_position_map[j]);
    }
    for (a = self->merge_queue.head; a != NULL; a = a->next) {
        total_avl_nodes++;
        for (u = (simplify_segment_t *) a->item; u != NULL; u = u->next) {
            assert(u->left < u->right);
            if (u->next != NULL) {
                assert(u->right <= u->next->left);
            }
            total_segments++;
        }
    }
    assert(total_segments == object_heap_get_num_allocated(&self->segment_heap));
    assert(total_avl_nodes == object_heap_get_num_allocated(&self->avl_node_heap));
}

static void
print_segment_chain(simplify_segment_t *head, FILE *out)
{
    simplify_segment_t *u;

    for (u = head; u != NULL; u = u->next) {
        fprintf(out, "(%f,%f->%d)", u->left, u->right, u->node);
    }
}

void
simplifier_print_state(simplifier_t *self, FILE *out)
{
    size_t j;
    avl_node_t *avl_node;
    simplify_segment_t *u;
    mutation_node_list_t *mnl;
    mutation_position_map_t *mpm;

    fprintf(out, "--simplifier state--\n");
    fprintf(out, "===\nInput nodes\n==\n");
    node_table_print_state(&self->input_nodes, out);
    fprintf(out, "===\nInput edges\n==\n");
    edge_table_print_state(&self->input_edges, out);
    fprintf(out, "===\nInput sites\n==\n");
    site_table_print_state(&self->input_sites, out);
    fprintf(out, "===\nInput mutations\n==\n");
    mutation_table_print_state(&self->input_mutations, out);
    fprintf(out, "===\nOutput tables\n==\n");
    node_table_print_state(self->nodes, out);
    edge_table_print_state(self->edges, out);
    site_table_print_state(self->sites, out);
    mutation_table_print_state(self->mutations, out);
    fprintf(out, "===\nmemory heaps\n==\n");
    fprintf(out, "segment_heap:\n");
    object_heap_print_state(&self->segment_heap, out);
    fprintf(out, "avl_node_heap:\n");
    object_heap_print_state(&self->avl_node_heap, out);
    fprintf(out, "===\nancestors\n==\n");
    for (j = 0; j < self->input_nodes.num_rows; j++) {
        if (self->ancestor_map[j] != NULL) {
            fprintf(out, "%d:\t", (int) j);
            print_segment_chain(self->ancestor_map[j], out);
            fprintf(out, "\n");
        }
    }
    fprintf(out, "===\nnode_id map (input->output)\n==\n");
    for (j = 0; j < self->input_nodes.num_rows; j++) {
        if (self->node_id_map[j] != MSP_NULL_NODE) {
            fprintf(out, "%d->%d\n", (int) j, self->node_id_map[j]);
        }
    }
    fprintf(out, "===\nmerge queue\n==\n");
    for (avl_node = self->merge_queue.head; avl_node != NULL; avl_node = avl_node->next) {
        u = (simplify_segment_t *) avl_node->item;
        print_segment_chain(u, out);
        fprintf(out, "\n");
    }
    fprintf(out, "===\nbuffered edges\n==\n");
    for (j = 0; j < self->num_buffered_edges; j++) {
        fprintf(out, "%f\t%f\t%d\t%d\n", self->edge_buffer[j].left,
                self->edge_buffer[j].right, self->edge_buffer[j].parent,
                self->edge_buffer[j].child);
    }
    fprintf(out, "===\nmutation position map\n==\n");
    for (j = 0; j < self->input_nodes.num_rows; j++) {
        if (avl_count(&self->mutation_position_map[j]) > 0) {
            fprintf(out, "node %d:\n", (int) j);
            for (avl_node = self->mutation_position_map[j].head;
                    avl_node != NULL; avl_node = avl_node->next) {
                mpm = (mutation_position_map_t *) avl_node->item;
                fprintf(out, "\t%f -> ", mpm->position);
                for (mnl = mpm->head; mnl != NULL; mnl = mnl->next) {
                    fprintf(out, "%d, ", mnl->mutation_id);
                }
                fprintf(out, "\n");
            }
            fprintf(out, "\n");
        }
    }
    fprintf(out, "===\nmutation node map\n==\n");
    for (j = 0; j < self->input_mutations.num_rows; j++) {
        fprintf(out, "%d\t-> %d\n", (int) j, self->mutation_node_map[j]);
    }
    simplifier_check_state(self);
}

static simplify_segment_t * WARN_UNUSED
simplifier_alloc_segment(simplifier_t *self, double left, double right, node_id_t node,
        simplify_segment_t *next)
{
    simplify_segment_t *seg = NULL;

    if (object_heap_empty(&self->segment_heap)) {
        if (object_heap_expand(&self->segment_heap) != 0) {
            goto out;
        }
    }
    seg = (simplify_segment_t *) object_heap_alloc_object(&self->segment_heap);
    if (seg == NULL) {
        goto out;
    }
    seg->next = next;
    seg->left = left;
    seg->right = right;
    seg->node = node;
out:
    return seg;
}

static inline void
simplifier_free_segment(simplifier_t *self, simplify_segment_t *seg)
{
    object_heap_free_object(&self->segment_heap, seg);
}

static inline avl_node_t * WARN_UNUSED
simplifier_alloc_avl_node(simplifier_t *self)
{
    avl_node_t *ret = NULL;

    if (object_heap_empty(&self->avl_node_heap)) {
        if (object_heap_expand(&self->avl_node_heap) != 0) {
            goto out;
        }
    }
    ret = (avl_node_t *) object_heap_alloc_object(&self->avl_node_heap);
out:
    return ret;
}

static inline void
simplifier_free_avl_node(simplifier_t *self, avl_node_t *node)
{
    object_heap_free_object(&self->avl_node_heap, node);
}

/* Add a new node to the output node table corresponding to the specified input id */
static int WARN_UNUSED
simplifier_record_node(simplifier_t *self, node_id_t input_id, bool is_sample)
{
    int ret = 0;
    table_size_t offset = self->input_nodes.metadata_offset[input_id];
    table_size_t length = self->input_nodes.metadata_offset[input_id + 1] - offset;
    uint32_t flags = self->input_nodes.flags[input_id];

    /* Zero out the sample bit */
    flags &= (uint32_t) ~MSP_NODE_IS_SAMPLE;
    if (is_sample) {
        flags |= MSP_NODE_IS_SAMPLE;
    }
    self->node_id_map[input_id] = (node_id_t) self->nodes->num_rows;
    ret = node_table_add_row_internal(self->nodes, flags,
            self->input_nodes.time[input_id], self->input_nodes.population[input_id],
            self->input_nodes.metadata + offset, length);
    if (ret < 0) {
        goto out;
    }
    ret = 0;
out:
    return ret;
}

static int
simplifier_flush_edges(simplifier_t *self)
{
    int ret = 0;
    size_t j, num_output_edges;
    edge_t e;

    if (self->num_buffered_edges > 0) {
        ret = squash_edges(self->edge_buffer, self->num_buffered_edges, &num_output_edges);
        if (ret != 0) {
            goto out;
        }
        /* Flush these edges to the table */
        for (j = 0; j < num_output_edges; j++) {
            e = self->edge_buffer[j];
            ret = edge_table_add_row(self->edges, e.left, e.right, e.parent, e.child);
            if (ret < 0) {
                goto out;
            }
        }
    }
    ret = 0;
    self->num_buffered_edges = 0;
out:
    return ret;
}

/* Records the specified edge in the output table */
static int
simplifier_record_edge(simplifier_t *self, double left, double right, node_id_t parent,
        node_id_t child)
{
    int ret = 0;
    edge_t *e;

    if (self->num_buffered_edges == self->max_buffered_edges - 1) {
        /* Grow the array. Use a doubling strategy here as we expect this
         * to usually be reasonably small */
        self->max_buffered_edges *= 2;
        e = realloc(self->edge_buffer, self->max_buffered_edges * sizeof(edge_t));
        if (e == NULL) {
            ret = MSP_ERR_NO_MEMORY;
            goto out;
        }
        self->edge_buffer = e;
    }
    assert(self->num_buffered_edges < self->max_buffered_edges);
    e = self->edge_buffer + self->num_buffered_edges;
    e->left = left;
    e->right = right;
    e->parent = parent;
    e->child = child;
    self->num_buffered_edges++;
out:
    return ret;
}

static int
simplifier_init_sites(simplifier_t *self)
{
    int ret = 0;
    size_t j, next_mutation_position_map;
    mutation_position_map_t *mpm, search;
    mutation_node_list_t *mnl;
    node_id_t node;
    site_id_t site;
    avl_node_t *avl_node;
    avl_tree_t *avl_tree;

    self->mutation_id_map = calloc(self->input_mutations.num_rows, sizeof(mutation_id_t));
    self->mutation_node_map = calloc(self->input_mutations.num_rows, sizeof(node_id_t));
    self->mutation_position_map = calloc(self->input_nodes.num_rows, sizeof(avl_tree_t));
    self->mutation_node_list_mem = calloc(self->input_mutations.num_rows,
            sizeof(mutation_node_list_t));
    self->mutation_position_map_mem = calloc(self->input_mutations.num_rows,
            sizeof(mutation_position_map_t));
    if (self->mutation_id_map == NULL || self->mutation_node_map == NULL
            || self->mutation_position_map == NULL
            || self->mutation_node_list_mem == NULL
            || self->mutation_position_map_mem == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    memset(self->mutation_id_map, 0xff,
            self->input_mutations.num_rows * sizeof(mutation_id_t));
    memset(self->mutation_node_map, 0xff,
            self->input_mutations.num_rows * sizeof(node_id_t));

    /* The mutation_position_map stores an AVL tree for each input node. Each of
     * these AVL trees maps the sites for mutations carrying the node in question
     * to their positions and list of mutations IDs */
    /* TODO: we should simplify this and remove the AVL trees. See the note above
     * in simplifier_record_mutations */
    for (j = 0; j < self->input_nodes.num_rows; j++) {
        avl_init_tree(self->mutation_position_map + j, cmp_mutation_position_map, NULL);
    }
    next_mutation_position_map = 0;
    for (j = 0; j < self->input_mutations.num_rows; j++) {
        site = self->input_mutations.site[j];
        node = self->input_mutations.node[j];

        avl_tree = self->mutation_position_map + node;
        /* If this position has already been inserted into the avl tree for this
         * node, add this mutation_id to its list. Otherwise alloc and insert a
         * new position into the list */
        search.position = self->input_sites.position[site];

        avl_node = avl_search(avl_tree, &search);
        if (avl_node == NULL) {
            assert(next_mutation_position_map < self->input_mutations.num_rows);
            mpm = self->mutation_position_map_mem + next_mutation_position_map;
            mpm->position = search.position;
            next_mutation_position_map++;
            avl_node = simplifier_alloc_avl_node(self);
            if (avl_node == NULL) {
                ret = MSP_ERR_NO_MEMORY;
                goto out;
            }
            avl_init_node(avl_node, mpm);
            avl_node = avl_insert_node(avl_tree, avl_node);
            assert(avl_node != NULL);
        } else {
            mpm = (mutation_position_map_t *) avl_node->item;
        }
        /* Insert this mutation at the head of the list for this position */
        mnl = self->mutation_node_list_mem + j;
        mnl->mutation_id = (mutation_id_t) j;
        mnl->next = mpm->head;
        mpm->head = mnl;
    }
out:
    return ret;
}

static int
simplifier_check_input(simplifier_t *self)
{
    int ret = MSP_ERR_GENERIC;
    node_id_t num_nodes = (node_id_t) self->nodes->num_rows;
    site_id_t num_sites;
    double *time = self->nodes->time;
    char *node_seen = NULL;
    node_id_t last_parent, parent, child;
    size_t j;

    node_seen = calloc((size_t) num_nodes, sizeof(char));
    /* Check the edges */
    last_parent = self->edges->parent[0];
    for (j = 0; j < self->edges->num_rows; j++) {
        if (self->edges->left[j] >= self->edges->right[j]) {
            ret = MSP_ERR_BAD_EDGE_INTERVAL;
            goto out;
        }
        parent = self->edges->parent[j];
        if (parent < 0 || parent >= num_nodes) {
            ret = MSP_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        if (parent != last_parent) {
            node_seen[last_parent] = 1;
            if (node_seen[parent] != 0) {
                ret = MSP_ERR_EDGES_NOT_SORTED_PARENT_TIME;
                goto out;
            }
            if (time[last_parent] > time[parent]) {
                ret = MSP_ERR_EDGES_NOT_SORTED_PARENT_TIME;
                goto out;
            }
            last_parent = parent;
        }
        /* Check the children */
        child = self->edges->child[j];
        if (child < 0 || child >= num_nodes) {
            ret = MSP_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        /* time[child] must be < time[parent] */
        if (time[child] >= time[parent]) {
            ret = MSP_ERR_BAD_NODE_TIME_ORDERING;
            goto out;
        }
    }
    /* Check the samples */
    for (j = 0; j < self->num_samples; j++) {
        if (self->samples[j] < 0 || self->samples[j] >= num_nodes) {
            ret = MSP_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        if (!(self->nodes->flags[self->samples[j]] & MSP_NODE_IS_SAMPLE)) {
            ret = MSP_ERR_BAD_SAMPLES;
            goto out;
        }

    }
    /* Check the sites */
    for (j = 0; j < self->sites->num_rows; j++) {
        if (self->sites->position[j] < 0
                || self->sites->position[j] >= self->sequence_length) {
            ret = MSP_ERR_BAD_SITE_POSITION;
            goto out;
        }
    }
    /* Check the mutations */
    num_sites = (site_id_t) self->sites->num_rows;
    for (j = 0; j < self->mutations->num_rows; j++) {
        if (self->mutations->site[j] < 0 || self->mutations->site[j] >= num_sites) {
            ret = MSP_ERR_SITE_OUT_OF_BOUNDS;
            goto out;
        }
        if (self->mutations->node[j] < 0 || self->mutations->node[j] >= num_nodes) {
            ret = MSP_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
    }
    ret = 0;
out:
    msp_safe_free(node_seen);
    return ret;
}

static int WARN_UNUSED
simplifier_insert_sample(simplifier_t *self, node_id_t sample_id)
{
    int ret = 0;

    assert(self->ancestor_map[sample_id] == NULL);
    self->ancestor_map[sample_id] = simplifier_alloc_segment(self, 0,
            self->sequence_length, (node_id_t) self->nodes->num_rows, NULL);
    if (self->ancestor_map[sample_id] == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    ret = simplifier_record_node(self, sample_id, true);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

static int
simplifier_init_samples(simplifier_t *self, node_id_t *samples)
{
    int ret = 0;
    size_t j;

    /* Go through the samples to check for errors. */
    for (j = 0; j < self->num_samples; j++) {
        if (samples[j] < 0 || samples[j] > (node_id_t) self->input_nodes.num_rows) {
            ret = MSP_ERR_OUT_OF_BOUNDS;
            goto out;
        }
        if (self->is_sample[samples[j]]) {
            ret = MSP_ERR_DUPLICATE_SAMPLE;
            goto out;
        }
        self->is_sample[samples[j]] = true;
        ret = simplifier_insert_sample(self, samples[j]);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

int
simplifier_alloc(simplifier_t *self, double sequence_length,
        node_id_t *samples, size_t num_samples,
        node_table_t *nodes, edge_table_t *edges, migration_table_t *migrations,
        site_table_t *sites, mutation_table_t *mutations,
        size_t max_buffered_edges, int flags)
{
    int ret = 0;
    size_t j, max_alloc_block, num_nodes_alloc, num_edges_alloc;

    memset(self, 0, sizeof(simplifier_t));
    self->num_samples = num_samples;
    self->flags = flags;
    self->nodes = nodes;
    self->edges = edges;
    self->sites = sites;
    self->mutations = mutations;

    if (nodes == NULL || edges == NULL || samples == NULL
            || sites == NULL || mutations == NULL || migrations == NULL) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    if (sequence_length == 0) {
        /* infer sequence length from the edges */
        sequence_length = 0.0;
        for (j = 0; j < edges->num_rows; j++) {
            sequence_length = MSP_MAX(sequence_length, edges->right[j]);
        }
        if (sequence_length <= 0.0) {
            ret = MSP_ERR_BAD_SEQUENCE_LENGTH;
            goto out;
        }
    }
    self->sequence_length = sequence_length;
    /* Take a copy of the input samples */
    self->samples = malloc(num_samples * sizeof(node_id_t));
    if (self->samples == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    memcpy(self->samples, samples, num_samples * sizeof(node_id_t));

    /* If we have more then 256K blocks or edges just allocate this much */
    max_alloc_block = 256 * 1024;
    /* Need to avoid malloc(0) so make sure we have at least 1. */
    num_nodes_alloc = MSP_MAX(max_alloc_block, 1 + nodes->num_rows);
    num_edges_alloc = MSP_MAX(max_alloc_block, 1 + edges->num_rows);

    /* TODO we can add a flag to skip these checks for when we know they are
     * unnecessary */
    ret = simplifier_check_input(self);
    if (ret != 0) {
        goto out;
    }

    /* Make a copy of the input nodes and clear the table ready for output */
    ret = node_table_alloc(&self->input_nodes, nodes->num_rows, nodes->metadata_length);
    if (ret != 0) {
        goto out;
    }
    ret = node_table_set_columns(&self->input_nodes, nodes->num_rows,
            nodes->flags, nodes->time, nodes->population,
            nodes->metadata, nodes->metadata_offset);
    if (ret != 0) {
        goto out;
    }
    ret = node_table_clear(self->nodes);
    if (ret != 0) {
        goto out;
    }
    /* Make a copy of the input edges and clear the input table, ready for output. */
    ret = edge_table_alloc(&self->input_edges, edges->num_rows);
    if (ret != 0) {
        goto out;
    }
    ret = edge_table_set_columns(&self->input_edges, edges->num_rows,
            edges->left, edges->right, edges->parent, edges->child);
    if (ret != 0) {
        goto out;
    }
    ret = edge_table_clear(self->edges);
    if (ret != 0) {
        goto out;
    }
    if (max_buffered_edges == 0) {
        max_buffered_edges = 1024;
    }
    self->max_buffered_edges = max_buffered_edges;
    self->num_buffered_edges = 0;
    self->edge_buffer = malloc(self->max_buffered_edges * sizeof(edge_t));
    if (self->edge_buffer == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }

    /* Make a copy of the input sites and clear the input table, ready for output. */
    ret = site_table_alloc(&self->input_sites, sites->num_rows,
            sites->ancestral_state_length, sites->metadata_length);
    if (ret != 0) {
        goto out;
    }
    ret = site_table_set_columns(&self->input_sites, sites->num_rows, sites->position,
            sites->ancestral_state, sites->ancestral_state_offset,
            sites->metadata, sites->metadata_offset);
    if (ret != 0) {
        goto out;
    }
    ret = site_table_clear(self->sites);
    if (ret != 0) {
        goto out;
    }

    /* Make a copy of the input mutations and clear the input table, ready for output. */
    ret = mutation_table_alloc(&self->input_mutations, mutations->num_rows,
            mutations->derived_state_length, mutations->metadata_length);
    if (ret != 0) {
        goto out;
    }
    ret = mutation_table_set_columns(&self->input_mutations, mutations->num_rows,
            mutations->site, mutations->node, mutations->parent,
            mutations->derived_state, mutations->derived_state_offset,
            mutations->metadata, mutations->metadata_offset);
    if (ret != 0) {
        goto out;
    }
    ret = mutation_table_clear(self->mutations);
    if (ret != 0) {
        goto out;
    }

    /* Allocate the heaps used for small objects. */
    /* TODO assuming that the number of edges is a good guess here. */
    ret = object_heap_init(&self->segment_heap, sizeof(simplify_segment_t),
            num_edges_alloc, NULL);
    if (ret != 0) {
        goto out;
    }
    ret = object_heap_init(&self->avl_node_heap, sizeof(avl_node_t),
            num_edges_alloc, NULL);
    if (ret != 0) {
        goto out;
    }
    /* Make the maps and set the intial state */
    self->ancestor_map = calloc(num_nodes_alloc, sizeof(simplify_segment_t *));
    self->node_id_map = malloc(num_nodes_alloc * sizeof(node_id_t));
    self->is_sample = calloc(num_nodes_alloc, sizeof(bool));
    if (self->ancestor_map == NULL || self->node_id_map == NULL
            || self->is_sample == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    memset(self->node_id_map, 0xff, self->input_nodes.num_rows * sizeof(node_id_t));
    self->nodes->num_rows = 0;
    avl_init_tree(&self->merge_queue, cmp_segment_queue, NULL);

    /* Allocate the segment buffers */
    self->segment_buffer_size = 64;
    self->segment_buffer = malloc(self->segment_buffer_size * sizeof(simplify_segment_t *));
    if (self->segment_buffer == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    ret = simplifier_init_sites(self);
    if (ret != 0) {
        goto out;
    }
    ret = simplifier_init_samples(self, samples);
out:
    return ret;
}

int
simplifier_free(simplifier_t *self)
{
    node_table_free(&self->input_nodes);
    edge_table_free(&self->input_edges);
    site_table_free(&self->input_sites);
    mutation_table_free(&self->input_mutations);
    object_heap_free(&self->segment_heap);
    object_heap_free(&self->avl_node_heap);
    msp_safe_free(self->samples);
    msp_safe_free(self->node_name_offset);
    msp_safe_free(self->ancestor_map);
    msp_safe_free(self->node_id_map);
    msp_safe_free(self->segment_buffer);
    msp_safe_free(self->edge_buffer);
    msp_safe_free(self->is_sample);
    msp_safe_free(self->mutation_id_map);
    msp_safe_free(self->mutation_node_map);
    msp_safe_free(self->mutation_position_map);
    msp_safe_free(self->mutation_node_list_mem);
    msp_safe_free(self->mutation_position_map_mem);
    return 0;
}

static simplify_segment_t **
simplifier_get_segment_buffer(simplifier_t *self, size_t size)
{
    simplify_segment_t **ret = NULL;

    if (self->segment_buffer_size < size) {
        self->segment_buffer_size = size;
        msp_safe_free(self->segment_buffer);
        self->segment_buffer = malloc(self->segment_buffer_size
                * sizeof(simplify_segment_t *));
        if (self->segment_buffer == NULL) {
            goto out;
        }
    }
    ret = self->segment_buffer;
out:
    return ret;
}

/* Defragging segment chains is not necessary for correctness and has no effect
 * on the output. However, it is an important performance optimisation (for
 * some large tree sequences simplification can take ~3X as long without
 * defgragging).
 */
static int WARN_UNUSED
simplifier_defrag_segment_chain(simplifier_t *self, node_id_t input_id)
{
    simplify_segment_t *x_prev, *x;

    x_prev = self->ancestor_map[input_id];
    assert(x_prev != NULL);
    x = x_prev->next;
    while (x != NULL) {
        if (x_prev->right == x->left && x_prev->node == x->node) {
            x_prev->right = x->right;
            x_prev->next = x->next;
            simplifier_free_segment(self, x);
            x = x_prev->next;
        } else {
            x_prev = x;
            x = x->next;
        }
    }
    return 0;
}

static int WARN_UNUSED
simplifier_priority_queue_insert(simplifier_t *self, avl_tree_t *Q, simplify_segment_t *u)
{
    int ret = 0;
    avl_node_t *node;

    assert(u != NULL);
    node = simplifier_alloc_avl_node(self);
    if (node == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    avl_init_node(node, u);
    node = avl_insert_node(Q, node);
    assert(node != NULL);
out:
    return ret;
}

static int
simplifier_record_mutations(simplifier_t *self, node_id_t input_id)
{
    int ret = 0;
    simplify_segment_t *seg = self->ancestor_map[input_id];
    avl_tree_t *avl_tree = &self->mutation_position_map[input_id];
    avl_node_t *a;
    mutation_position_map_t *mpm, search;
    mutation_node_list_t *mnl;
    /* The AVL tree is overkill here really. All we want is a sorted list of the
     * position -> [mutation_ids] for each input ID. We can then co-iterate over
     * the ancestry segments and the list of positions. This will be more efficient
     * unless we have huge numbers of mutations per node */

    if (avl_count(avl_tree) > 0) {
        while (seg != NULL) {
            search.position = seg->left;
            avl_search_closest(avl_tree, &search, &a);
            assert(a != NULL);
            mpm = (mutation_position_map_t *) a->item;
            if (mpm->position < seg->left) {
                a = a->next;
            }
            while (a != NULL
                    && ((mutation_position_map_t *) a->item)->position < seg->right) {
                mpm = (mutation_position_map_t *) a->item;
                assert(mpm->position >= seg->left && mpm->position < seg->right);
                for (mnl = mpm->head; mnl != NULL; mnl = mnl->next) {
                    /* Set the output node for this mutation to the segment's node */
                    self->mutation_node_map[mnl->mutation_id] = seg->node;
                }
                a = a->next;
            }
            seg = seg->next;
        }
    }
    return ret;
}

/* This function accounts for about half of the CPU usage currently when
 * operating on very large sets of edges produced by forward simulations.
 * The inner loop where we skip over segments that are strictly within the
 * interval seems to account for the majority of this cost. This suggests
 * that the current linked-list approach could perhaps be improved by
 * representing ancestors via an AVL tree (keyed by left coordinate), meaning
 * that we could avoid the cost of iterating over these linked lists. The
 * merge algorithm could probably be recast as iterating over several
 * AVL trees rather than a single priority queue.
 */
static int
simplifier_remove_ancestry(simplifier_t *self, double left, double right,
        node_id_t input_id)
{
    int ret = 0;
    simplify_segment_t *x, *y, *head, *last, *x_prev;

    head = self->ancestor_map[input_id];
    x = head;
    last = NULL;
    x_prev = NULL;  /* Keep the compiler happy */
    /* Skip the leading segments before left */
    while (x != NULL && x->right <= left) {
        last = x;
        x = x->next;
    }
    if (x != NULL && x->left < left) {
        /* The left edge of x overhangs. Insert a new segment for the excess. */
        y = simplifier_alloc_segment(self, x->left, left, x->node, NULL);
        if (y == NULL) {
            ret = MSP_ERR_NO_MEMORY;
            goto out;
        }
        x->left = left;
        if (last != NULL) {
            last->next = y;
        }
        last = y;
        if (x == head) {
            head = last;
        }
    }
    if (x != NULL && x->left < right) {
        /* x is the first segment within the target interval, so add it to the
         * output queue */
        ret = simplifier_priority_queue_insert(self, &self->merge_queue, x);
        if (ret != 0) {
            goto out;
        }
        /* Skip over segments strictly within the target interval */
        while (x != NULL && x->right <= right) {
            x_prev = x;
            x = x->next;
        }
        if (x != NULL && x->left < right) {
            /* We have an overhang on the right hand side. Create a new
             * segment for the overhang and terminate the output chain. */
            y = simplifier_alloc_segment(self, right, x->right, x->node, x->next);
            if (y == NULL) {
                ret = MSP_ERR_NO_MEMORY;
                goto out;
            }
            x->right = right;
            x->next = NULL;
            x = y;
        } else if (x_prev != NULL) {
            x_prev->next = NULL;
        }
    }
    /* x is the first segment in the new chain starting after right. */
    if (last == NULL) {
        head = x;
    } else {
        last->next = x;
    }
    self->ancestor_map[input_id] = head;
out:
    return ret;
}

static int WARN_UNUSED
simplifier_merge_ancestors(simplifier_t *self, node_id_t input_id)
{
    int ret = MSP_ERR_GENERIC;
    bool node_is_sample, output_node_used;
    bool defrag_required = false;
    node_id_t output_id;
    double l, r, next_l;
    uint32_t j, h;
    avl_node_t *node;
    simplify_segment_t head_sentinel;
    simplify_segment_t *x, *z, *alpha, *head;
    simplify_segment_t **H = NULL;
    avl_tree_t *Q = &self->merge_queue;

    H = simplifier_get_segment_buffer(self, avl_count(Q));
    if (H == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }

    /* check if the input ID is a sample */
    node_is_sample = self->is_sample[input_id];
    output_node_used = false;
    output_id = self->node_id_map[input_id];
    if (node_is_sample) {
        assert(output_id != MSP_NULL_NODE);
        x = self->ancestor_map[input_id];
        assert(x != NULL);
        assert(x->left == 0.0);
        assert(x->right == self->sequence_length);
        assert(x->node == output_id);
    } else {
        assert(output_id == MSP_NULL_NODE);
        assert(self->ancestor_map[input_id] == NULL);
        /* We allocate the output ID here to save looking it up
         * within the inner loop. However, we will not record this
         * unless the output_node_used flag is set */
        output_id = (node_id_t) self->nodes->num_rows;
    }

    head_sentinel.left = 0.0;
    head_sentinel.right = 0.0;
    head_sentinel.node = MSP_NULL_NODE;
    head_sentinel.next = NULL;
    head = &head_sentinel;
    z = head;
    while (avl_count(Q) > 0) {
        h = 0;
        node = Q->head;
        l = ((simplify_segment_t *) node->item)->left;
        r = self->sequence_length;
        while (node != NULL && ((simplify_segment_t *) node->item)->left == l) {
            H[h] = (simplify_segment_t *) node->item;
            r = MSP_MIN(r, H[h]->right);
            h++;
            simplifier_free_avl_node(self, node);
            avl_unlink_node(Q, node);
            node = node->next;
        }
        next_l = 0;
        if (node != NULL) {
            next_l = ((simplify_segment_t *) node->item)->left;
            r = MSP_MIN(r, next_l);
        }
        if (h == 1) {
            x = H[0];
            if (node != NULL && next_l < x->right) {
                alpha = simplifier_alloc_segment(self, x->left, next_l, x->node, NULL);
                if (alpha == NULL) {
                    ret = MSP_ERR_NO_MEMORY;
                    goto out;
                }
                x->left = next_l;
            } else {
                alpha = x;
                x = x->next;
                alpha->next = NULL;
            }
            if (x != NULL) {
                ret = simplifier_priority_queue_insert(self, Q, x);
                if (ret != 0) {
                    goto out;
                }
            }
            if (node_is_sample) {
                /* If we have a mapped node over an interval with no other segments,
                 * then we must record this edge as it joins internal samples */
                ret = simplifier_record_edge(self, alpha->left, alpha->right, output_id,
                        alpha->node);
                if (ret != 0) {
                    goto out;
                }
                /* The node on this segment must be remapped to the parent so that we
                 * have the correct nodes further up in the tree. */
                alpha->node = output_id;
            }
        } else {
            output_node_used = true;
            alpha = simplifier_alloc_segment(self, l, r, output_id, NULL);
            if (alpha == NULL) {
                ret = MSP_ERR_NO_MEMORY;
                goto out;
            }
            /* Create the record and update the priority queue */
            for (j = 0; j < h; j++) {
                x = H[j];
                ret = simplifier_record_edge(self, l, r, output_id, x->node);
                if (ret != 0) {
                    goto out;
                }
                if (x->right == r) {
                    simplifier_free_segment(self, x);
                    x = x->next;
                } else if (x->right > r) {
                    x->left = r;
                }
                if (x != NULL) {
                    ret = simplifier_priority_queue_insert(self, Q, x);
                    if (ret != 0) {
                        goto out;
                    }
                }
            }
        }

        /* Loop tail; integrate alpha into the global state */
        assert(alpha != NULL);
        assert(z != NULL);
        defrag_required |= z->right == alpha->left && z->node == alpha->node;
        z->next = alpha;
        z = alpha;
    }

    if (node_is_sample) {
        /* There is already ancestral material present for this node, which
         * is a sample. We can free the segments computed here because the
         * existing mapping covers the full region */
        x = head->next;
        while (x != NULL) {
            assert(x->node == self->node_id_map[input_id]);
            simplifier_free_segment(self ,x);
            x = x->next;
        }
    } else {
        self->ancestor_map[input_id] = head->next;
        if (output_node_used) {
            ret = simplifier_record_node(self, input_id, false);
            if (ret != 0) {
                goto out;
            }
            assert(output_id == self->node_id_map[input_id]);
        } else {
            assert(self->node_id_map[input_id] == MSP_NULL_NODE);
        }
    }
    if (defrag_required) {
        ret = simplifier_defrag_segment_chain(self, input_id);
        if (ret != 0) {
            goto out;
        }
    }
    ret = simplifier_flush_edges(self);
out:
    return ret;
}

static int WARN_UNUSED
simplifier_output_sites(simplifier_t *self)
{
    int ret = 0;
    site_id_t input_site;
    table_size_t ancestral_state_offset, ancestral_state_length;
    table_size_t derived_state_offset, derived_state_length;
    table_size_t metadata_offset, metadata_length;
    mutation_id_t input_mutation, mapped_parent ,site_start, site_end;
    site_id_t num_input_sites = (site_id_t) self->input_sites.num_rows;
    mutation_id_t num_input_mutations = (mutation_id_t) self->input_mutations.num_rows;
    mutation_id_t input_parent, num_output_mutations, num_output_site_mutations;
    node_id_t mapped_node;
    bool keep_mutation, keep_site;
    bool filter_zero_mutation_sites = (self->flags & MSP_FILTER_ZERO_MUTATION_SITES);
    char *derived_state, *ancestral_state;
    int cmp;

    input_mutation = 0;
    num_output_mutations = 0;
    for (input_site = 0; input_site < num_input_sites; input_site++) {
        site_start = input_mutation;
        num_output_site_mutations = 0;
        while (input_mutation < num_input_mutations
                && self->input_mutations.site[input_mutation] == input_site) {
            mapped_node = self->mutation_node_map[input_mutation];
            if (mapped_node != MSP_NULL_NODE) {
                input_parent = self->input_mutations.parent[input_mutation];
                mapped_parent = MSP_NULL_MUTATION;
                if (input_parent != MSP_NULL_MUTATION) {
                    mapped_parent = self->mutation_id_map[input_parent];
                }
                keep_mutation = true;
                if (mapped_parent == MSP_NULL_MUTATION) {
                    /* If there is no parent and the ancestral state is equal to the
                     * derived state, then we remove this mutation.
                     */
                    derived_state = self->input_mutations.derived_state +
                        self->input_mutations.derived_state_offset[input_mutation];
                    derived_state_length =
                        self->input_mutations.derived_state_offset[input_mutation + 1]
                        - self->input_mutations.derived_state_offset[input_mutation];
                    ancestral_state = self->input_sites.ancestral_state +
                        self->input_sites.ancestral_state_offset[input_site];
                    ancestral_state_length =
                        self->input_sites.ancestral_state_offset[input_site + 1]
                        - self->input_sites.ancestral_state_offset[input_site];
                    if (ancestral_state_length == derived_state_length) {
                        cmp = memcmp(derived_state, ancestral_state, derived_state_length);
                        if (cmp == 0) {
                            keep_mutation = false;
                        }
                    }
                }
                if (keep_mutation) {
                    self->mutation_id_map[input_mutation] = num_output_mutations;
                    num_output_mutations++;
                    num_output_site_mutations++;
                }
            }
            input_mutation++;
        }
        site_end = input_mutation;

        keep_site = true;
        if (filter_zero_mutation_sites && num_output_site_mutations == 0) {
            keep_site = false;
        }
        if (keep_site) {
            for (input_mutation = site_start; input_mutation < site_end; input_mutation++) {
                if (self->mutation_id_map[input_mutation] != MSP_NULL_MUTATION) {
                    assert(self->mutations->num_rows
                            == (size_t) self->mutation_id_map[input_mutation]);
                    mapped_node = self->mutation_node_map[input_mutation];
                    assert(mapped_node != MSP_NULL_NODE);
                    mapped_parent = self->input_mutations.parent[input_mutation];
                    if (mapped_parent != MSP_NULL_MUTATION) {
                        mapped_parent = self->mutation_id_map[mapped_parent];
                    }
                    derived_state_offset = self->input_mutations.derived_state_offset[
                        input_mutation];
                    derived_state_length = self->input_mutations.derived_state_offset[
                        input_mutation + 1] - derived_state_offset;
                    metadata_offset = self->input_mutations.metadata_offset[
                        input_mutation];
                    metadata_length = self->input_mutations.metadata_offset[
                        input_mutation + 1] - metadata_offset;
                    ret = mutation_table_add_row(self->mutations,
                            (site_id_t) self->sites->num_rows,
                            mapped_node, mapped_parent,
                            self->input_mutations.derived_state + derived_state_offset,
                            derived_state_length,
                            self->input_mutations.metadata + metadata_offset,
                            metadata_length);
                    if (ret < 0) {
                        goto out;
                    }
                }
            }
            ancestral_state_offset = self->input_sites.ancestral_state_offset[input_site];
            ancestral_state_length = self->input_sites.ancestral_state_offset[
                input_site + 1] - ancestral_state_offset;
            metadata_offset = self->input_sites.metadata_offset[input_site];
            metadata_length = self->input_sites.metadata_offset[input_site + 1]
                - metadata_offset;
            ret = site_table_add_row(self->sites,
                    self->input_sites.position[input_site],
                    self->input_sites.ancestral_state + ancestral_state_offset,
                    ancestral_state_length,
                    self->input_sites.metadata + metadata_offset,
                    metadata_length);
            if (ret < 0) {
                goto out;
            }

        }
        assert(num_output_mutations == (mutation_id_t) self->mutations->num_rows);
        input_mutation = site_end;
    }
    assert(input_mutation == num_input_mutations);
    ret = 0;
out:
    return ret;
}

static int WARN_UNUSED
simplifier_process_parent_edges(simplifier_t *self, node_id_t parent, size_t start,
        size_t end)
{
    int ret = 0;
    size_t j;
    node_id_t child;
    double left, right;

    /* Go through the edges and remove ancestry */
    for (j = start; j < end; j++) {
        assert(parent == self->input_edges.parent[j]);
        child = self->input_edges.child[j];
        if (self->ancestor_map[child] != NULL) {
            ret = simplifier_record_mutations(self, child);
            if (ret != 0) {
                goto out;
            }
            left = self->input_edges.left[j];
            right = self->input_edges.right[j];
            ret = simplifier_remove_ancestry(self, left, right, child);
            if (ret != 0) {
                goto out;
            }
        }
    }
    /* We can now merge the ancestral segments for the parent */
    ret = simplifier_merge_ancestors(self, parent);
    if (ret != 0) {
        goto out;
    }
    assert(avl_count(&self->merge_queue) == 0);
out:
    return ret;
}

int WARN_UNUSED
simplifier_run(simplifier_t *self, node_id_t *node_map)
{
    int ret = 0;
    size_t j, start;
    node_id_t parent, current_parent;
    size_t num_edges = self->input_edges.num_rows;

    if (num_edges > 0) {
        start = 0;
        current_parent = self->input_edges.parent[0];
        for (j = 0; j < num_edges; j++) {
            parent = self->input_edges.parent[j];
            if (parent != current_parent) {
                ret = simplifier_process_parent_edges(self, current_parent, start, j);
                if (ret != 0) {
                    goto out;
                }
                current_parent = parent;
                start = j;
            }
        }
        ret = simplifier_process_parent_edges(self, current_parent, start, num_edges);
        if (ret != 0) {
            goto out;
        }
    }
    /* Record any remaining mutations over the roots */
    for (j = 0; j < self->input_nodes.num_rows; j++) {
        if (self->ancestor_map[j] != NULL) {
            ret = simplifier_record_mutations(self, (node_id_t) j);
            if (ret != 0) {
                goto out;
            }
        }
    }
    ret = simplifier_output_sites(self);
    if (ret != 0) {
        goto out;
    }
    if (node_map != NULL) {
        /* Finally, output the new IDs for the nodes, if required. */
        memcpy(node_map, self->node_id_map, self->input_nodes.num_rows * sizeof(node_id_t));
    }
out:
    return ret;
}

/*************************
 * hdf5_file
 *************************/

typedef struct {
    table_collection_t *tables;
} hdf5_file_t;

static int
hdf5_file_alloc(hdf5_file_t *self, table_collection_t *tables)
{
    memset(self, 0, sizeof(*self));
    self->tables = tables;
    return 0;
}

static int
hdf5_file_free(hdf5_file_t *self)
{
    return 0;
}

/* Reads the metadata for the overall file and updates the basic
 * information in the hdf5_file.
 */
static int
hdf5_file_read_metadata(hdf5_file_t *self, hid_t file_id)
{
    int ret = MSP_ERR_HDF5;
    hid_t attr_id, dataspace_id;
    herr_t status;
    int rank;
    hsize_t dims;
    uint32_t version[2];

    attr_id = H5Aopen_by_name(file_id, "/", "format_version", H5P_DEFAULT, H5P_DEFAULT);
    if (attr_id < 0) {
        goto out;
    }
    dataspace_id = H5Aget_space(attr_id);
    if (dataspace_id < 0) {
        goto out;
    }
    rank = H5Sget_simple_extent_ndims(dataspace_id);
    if (rank != 1) {
        ret = MSP_ERR_FILE_FORMAT;
        goto out;
    }
    status = H5Sget_simple_extent_dims(dataspace_id, &dims, NULL);
    if (status < 0) {
        goto out;
    }
    if (dims != 2) {
        ret = MSP_ERR_FILE_FORMAT;
        goto out;
    }
    status = H5Aread(attr_id, H5T_NATIVE_UINT32, version);
    if (status < 0) {
        goto out;
    }
    status = H5Sclose(dataspace_id);
    if (status < 0) {
        goto out;
    }
    status = H5Aclose(attr_id);
    if (status < 0) {
        goto out;
    }

    attr_id = H5Aopen_by_name(file_id, "/", "sequence_length", H5P_DEFAULT, H5P_DEFAULT);
    if (attr_id < 0) {
        goto out;
    }
    dataspace_id = H5Aget_space(attr_id);
    if (dataspace_id < 0) {
        goto out;
    }
    rank = H5Sget_simple_extent_ndims(dataspace_id);
    if (rank != 1) {
        ret = MSP_ERR_FILE_FORMAT;
        goto out;
    }
    status = H5Sget_simple_extent_dims(dataspace_id, &dims, NULL);
    if (status < 0) {
        goto out;
    }
    if (dims != 1) {
        ret = MSP_ERR_FILE_FORMAT;
        goto out;
    }
    status = H5Aread(attr_id, H5T_NATIVE_DOUBLE, &self->tables->sequence_length);
    if (status < 0) {
        goto out;
    }
    status = H5Sclose(dataspace_id);
    if (status < 0) {
        goto out;
    }
    status = H5Aclose(attr_id);
    if (status < 0) {
        goto out;
    }

    /* Sanity check */
    if (version[0] < MSP_FILE_FORMAT_VERSION_MAJOR) {
        ret = MSP_ERR_FILE_VERSION_TOO_OLD;
        goto out;
    }
    if (version[0] > MSP_FILE_FORMAT_VERSION_MAJOR) {
        ret = MSP_ERR_FILE_VERSION_TOO_NEW;
        goto out;
    }
    if (self->tables->sequence_length <= 0.0) {
        ret = MSP_ERR_BAD_SEQUENCE_LENGTH;
        goto out;
    }
    ret = 0;
out:
    return ret;
}

/* Reads the groups within the HDF5 file to ensure that they exist.
 */
static int
hdf5_file_read_groups(hdf5_file_t *self, hid_t file_id)
{
    int ret = MSP_ERR_HDF5;
    htri_t exists;
    const char* groups[] = {
        "/edges",
        "/edges/indexes",
        "/nodes",
        "/sites",
        "/mutations",
        "/migrations",
        "/provenances",
    };
    size_t num_groups = sizeof(groups) / sizeof(const char *);
    size_t j;

    for (j = 0; j < num_groups; j++) {
        exists = H5Lexists(file_id, groups[j], H5P_DEFAULT);
        if (exists < 0) {
            goto out;
        }
        if (! exists) {
            ret = MSP_ERR_FILE_FORMAT;
            goto out;
        }
    }
    ret = 0;
out:
    return ret;

}

/* Reads the dimensions for each table and allocs it.
 */
static int
hdf5_file_read_dimensions(hdf5_file_t *self, hid_t file_id)
{
    int ret = MSP_ERR_HDF5;
    hid_t dataset_id, dataspace_id;
    herr_t status;
    htri_t exists;
    int rank;
    hsize_t dims;

    size_t num_nodes = 0;
    size_t num_edges = 0;
    size_t num_sites = 0;
    size_t num_mutations = 0;
    size_t num_migrations = 0;
    size_t num_provenances = 0;
    size_t ancestral_state_length = 0;
    size_t derived_state_length = 0;
    size_t site_metadata_length = 0;
    size_t mutation_metadata_length = 0;
    size_t node_metadata_length = 0;
    size_t provenance_timestamp_length = 0;
    size_t provenance_record_length = 0;

    struct _dimension_read {
        const char *name;
        size_t *dest;
    };
    struct _dimension_read fields[] = {
        {"/sites/position", &num_sites},
        {"/sites/ancestral_state", &ancestral_state_length},
        {"/sites/metadata", &site_metadata_length},
        {"/mutations/site", &num_mutations},
        {"/mutations/derived_state", &derived_state_length},
        {"/mutations/metadata", &mutation_metadata_length},
        {"/nodes/time", &num_nodes},
        {"/nodes/metadata", &node_metadata_length},
        {"/edges/left", &num_edges},
        {"/migrations/left", &num_migrations},
        {"/provenances/timestamp_offset", &num_provenances},
        {"/provenances/timestamp", &provenance_timestamp_length},
        {"/provenances/record", &provenance_record_length},
    };
    size_t num_fields = sizeof(fields) / sizeof(struct _dimension_read);
    size_t j;

    for (j = 0; j < num_fields; j++) {
        *fields[j].dest = 0;
        exists = H5Lexists(file_id, fields[j].name, H5P_DEFAULT);
        if (exists < 0) {
            goto out;
        }
        if (exists) {
            dataset_id = H5Dopen(file_id, fields[j].name, H5P_DEFAULT);
            if (dataset_id < 0) {
                ret = MSP_ERR_FILE_FORMAT;
                goto out;
            }
            dataspace_id = H5Dget_space(dataset_id);
            if (dataspace_id < 0) {
                goto out;
            }
            rank = H5Sget_simple_extent_ndims(dataspace_id);
            if (rank != 1) {
                ret = MSP_ERR_FILE_FORMAT;
                goto out;
            }
            status = H5Sget_simple_extent_dims(dataspace_id, &dims, NULL);
            if (status < 0) {
                goto out;
            }
            *fields[j].dest = (size_t) dims;
            status = H5Sclose(dataspace_id);
            if (status < 0) {
                goto out;
            }
            status = H5Dclose(dataset_id);
            if (status < 0) {
                goto out;
            }
        }
    }
    /* provenance is a special case because we have no simple columns. We must
     * have at least one row in the offsets col or we have an error. */
    if (num_provenances == 0) {
        goto out;
    }
    num_provenances -= 1;

    /* Allocate the tables with the correct sizes */
    ret = node_table_alloc(&self->tables->nodes, num_nodes, node_metadata_length);
    if (ret != 0) {
        goto out;
    }
    self->tables->nodes.num_rows = (table_size_t) num_nodes;
    self->tables->nodes.metadata_length = (table_size_t) node_metadata_length;

    ret = edge_table_alloc(&self->tables->edges, num_edges);
    if (ret != 0) {
        goto out;
    }
    self->tables->edges.num_rows = (table_size_t) num_edges;

    ret = migration_table_alloc(&self->tables->migrations, num_migrations);
    if (ret != 0) {
        goto out;
    }
    self->tables->migrations.num_rows = (table_size_t) num_migrations;

    ret = site_table_alloc(&self->tables->sites, num_sites, ancestral_state_length,
            site_metadata_length);
    if (ret != 0) {
        goto out;
    }
    self->tables->sites.num_rows = (table_size_t) num_sites;
    self->tables->sites.ancestral_state_length = (table_size_t) ancestral_state_length;
    self->tables->sites.metadata_length = (table_size_t) site_metadata_length;

    ret = mutation_table_alloc(&self->tables->mutations, num_mutations,
            derived_state_length, mutation_metadata_length);
    if (ret != 0) {
        goto out;
    }
    self->tables->mutations.num_rows = (table_size_t) num_mutations;
    self->tables->mutations.derived_state_length = (table_size_t) derived_state_length;
    self->tables->mutations.metadata_length = (table_size_t) mutation_metadata_length;

    ret = provenance_table_alloc(&self->tables->provenances, num_provenances,
            provenance_timestamp_length, provenance_record_length);
    self->tables->provenances.num_rows = (table_size_t) num_provenances;
    self->tables->provenances.timestamp_length = (table_size_t) provenance_timestamp_length;
    self->tables->provenances.record_length = (table_size_t) provenance_record_length;
    if (ret != 0) {
        goto out;
    }
    /* Alloc the indexes */
    self->tables->indexes.edge_insertion_order = malloc(num_edges * sizeof(edge_id_t));
    self->tables->indexes.edge_removal_order = malloc(num_edges * sizeof(edge_id_t));
    if (self->tables->indexes.edge_insertion_order == NULL
            || self->tables->indexes.edge_removal_order == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    ret = 0;
out:
    return ret;
}

static int
hdf5_file_check_dimensions(hdf5_file_t *self, hid_t file_id)
{
    int ret = MSP_ERR_HDF5;
    hid_t dataset_id, dataspace_id;
    herr_t status;
    int rank;
    hsize_t dims[2];
    htri_t exists;
    struct _dimension_check {
        const char *name;
        size_t size;
    };
    struct _dimension_check fields[] = {
        {"/sites/position", self->tables->sites.num_rows},
        {"/sites/ancestral_state", self->tables->sites.ancestral_state_length},
        {"/sites/ancestral_state_offset", self->tables->sites.num_rows + 1},
        {"/sites/metadata", self->tables->sites.metadata_length},
        {"/sites/metadata_offset", self->tables->sites.num_rows + 1},

        {"/mutations/site", self->tables->mutations.num_rows},
        {"/mutations/node", self->tables->mutations.num_rows},
        {"/mutations/parent", self->tables->mutations.num_rows},
        {"/mutations/derived_state", self->tables->mutations.derived_state_length},
        {"/mutations/derived_state_offset", self->tables->mutations.num_rows + 1},
        {"/mutations/metadata", self->tables->mutations.metadata_length},
        {"/mutations/metadata_offset", self->tables->mutations.num_rows + 1},

        {"/nodes/flags", self->tables->nodes.num_rows},
        {"/nodes/population", self->tables->nodes.num_rows},
        {"/nodes/metadata", self->tables->nodes.metadata_length},
        {"/nodes/metadata_offset", self->tables->nodes.num_rows + 1},
        {"/nodes/time", self->tables->nodes.num_rows},

        {"/edges/left", self->tables->edges.num_rows},
        {"/edges/right", self->tables->edges.num_rows},
        {"/edges/parent", self->tables->edges.num_rows},
        {"/edges/child", self->tables->edges.num_rows},
        {"/edges/indexes/insertion_order", self->tables->edges.num_rows},
        {"/edges/indexes/removal_order", self->tables->edges.num_rows},

        {"/migrations/left", self->tables->migrations.num_rows},
        {"/migrations/right", self->tables->migrations.num_rows},
        {"/migrations/node", self->tables->migrations.num_rows},
        {"/migrations/source", self->tables->migrations.num_rows},
        {"/migrations/dest", self->tables->migrations.num_rows},
        {"/migrations/time", self->tables->migrations.num_rows},

        {"/provenances/timestamp", self->tables->provenances.timestamp_length},
        {"/provenances/timestamp_offset", self->tables->provenances.num_rows + 1},
        {"/provenances/record", self->tables->provenances.record_length},
        {"/provenances/record_offset", self->tables->provenances.num_rows + 1},
    };
    size_t num_fields = sizeof(fields) / sizeof(struct _dimension_check);
    size_t j;

    /* Now go though the rest of the fields and make sure they have the
     * right sizes
     */
    for (j = 0; j < num_fields; j++) {
        exists = H5Lexists(file_id, fields[j].name, H5P_DEFAULT);
        if (exists < 0) {
            goto out;
        }
        dims[0] = 0;
        if (exists) {
            dataset_id = H5Dopen(file_id, fields[j].name, H5P_DEFAULT);
            if (dataset_id < 0) {
                goto out;
            }
            dataspace_id = H5Dget_space(dataset_id);
            if (dataspace_id < 0) {
                goto out;
            }
            rank = H5Sget_simple_extent_ndims(dataspace_id);
            if (rank != 1) {
                ret = MSP_ERR_FILE_FORMAT;
                goto out;
            }
            status = H5Sget_simple_extent_dims(dataspace_id, dims, NULL);
            if (status < 0) {
                goto out;
            }
            status = H5Sclose(dataspace_id);
            if (status < 0) {
                goto out;
            }
            status = H5Dclose(dataset_id);
            if (status < 0) {
                goto out;
            }
        }
        if (dims[0] != fields[j].size) {
            ret = MSP_ERR_FILE_FORMAT;
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

static int
hdf5_file_read_data(hdf5_file_t *self, hid_t file_id)
{
    herr_t status;
    int ret = MSP_ERR_HDF5;
    hid_t dataset_id;
    htri_t exists;
    struct _hdf5_field_read {
        const char *name;
        hid_t type;
        void *dest;
    };
    struct _hdf5_field_read fields[] = {
        {"/nodes/metadata", H5T_NATIVE_CHAR, self->tables->nodes.metadata},
        {"/nodes/metadata_offset", H5T_NATIVE_UINT32,
            self->tables->nodes.metadata_offset},
        {"/nodes/flags", H5T_NATIVE_UINT32, self->tables->nodes.flags},
        {"/nodes/population", H5T_NATIVE_INT32, self->tables->nodes.population},
        {"/nodes/time", H5T_NATIVE_DOUBLE, self->tables->nodes.time},

        {"/sites/position", H5T_NATIVE_DOUBLE, self->tables->sites.position},
        {"/sites/ancestral_state", H5T_NATIVE_CHAR, self->tables->sites.ancestral_state},
        {"/sites/ancestral_state_offset", H5T_NATIVE_UINT32,
            self->tables->sites.ancestral_state_offset},
        {"/sites/metadata", H5T_NATIVE_CHAR, self->tables->sites.metadata},
        {"/sites/metadata_offset", H5T_NATIVE_UINT32, self->tables->sites.metadata_offset},

        {"/mutations/site", H5T_NATIVE_INT32, self->tables->mutations.site},
        {"/mutations/node", H5T_NATIVE_INT32, self->tables->mutations.node},
        {"/mutations/parent", H5T_NATIVE_INT32, self->tables->mutations.parent},
        {"/mutations/derived_state", H5T_NATIVE_CHAR, self->tables->mutations.derived_state},
        {"/mutations/derived_state_offset", H5T_NATIVE_UINT32,
            self->tables->mutations.derived_state_offset},
        {"/mutations/metadata", H5T_NATIVE_CHAR, self->tables->mutations.metadata},
        {"/mutations/metadata_offset", H5T_NATIVE_UINT32,
            self->tables->mutations.metadata_offset},

        {"/edges/left", H5T_NATIVE_DOUBLE, self->tables->edges.left},
        {"/edges/right", H5T_NATIVE_DOUBLE, self->tables->edges.right},
        {"/edges/parent", H5T_NATIVE_INT32, self->tables->edges.parent},
        {"/edges/child", H5T_NATIVE_INT32, self->tables->edges.child},

        {"/edges/indexes/insertion_order", H5T_NATIVE_INT32,
            self->tables->indexes.edge_insertion_order},
        {"/edges/indexes/removal_order", H5T_NATIVE_INT32,
            self->tables->indexes.edge_removal_order},

        {"/migrations/left", H5T_NATIVE_DOUBLE, self->tables->migrations.left},
        {"/migrations/right", H5T_NATIVE_DOUBLE, self->tables->migrations.right},
        {"/migrations/node", H5T_NATIVE_INT32, self->tables->migrations.node},
        {"/migrations/source", H5T_NATIVE_INT32, self->tables->migrations.source},
        {"/migrations/dest", H5T_NATIVE_INT32, self->tables->migrations.dest},
        {"/migrations/time", H5T_NATIVE_DOUBLE, self->tables->migrations.time},

        {"/provenances/timestamp", H5T_NATIVE_CHAR, self->tables->provenances.timestamp},
        {"/provenances/timestamp_offset", H5T_NATIVE_UINT32,
            self->tables->provenances.timestamp_offset},
        {"/provenances/record", H5T_NATIVE_CHAR, self->tables->provenances.record},
        {"/provenances/record_offset", H5T_NATIVE_UINT32,
            self->tables->provenances.record_offset},
    };
    size_t num_fields = sizeof(fields) / sizeof(struct _hdf5_field_read);
    size_t j;

    for (j = 0; j < num_fields; j++) {
        exists = H5Lexists(file_id, fields[j].name, H5P_DEFAULT);
        if (exists < 0) {
            goto out;
        }
        if (exists) {
            dataset_id = H5Dopen(file_id, fields[j].name, H5P_DEFAULT);
            if (dataset_id < 0) {
                goto out;
            }
            status = H5Dread(dataset_id, fields[j].type, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                    fields[j].dest);
            if (status < 0) {
                goto out;
            }
            status = H5Dclose(dataset_id);
            if (status < 0) {
                goto out;
            }
        }
    }
    ret = 0;
out:
    return ret;
}

static int
hdf5_file_load(hdf5_file_t *self, const char *filename)
{
    int ret = MSP_ERR_GENERIC;
    herr_t status;
    hid_t file_id = -1;

    file_id = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file_id < 0) {
        ret = MSP_ERR_HDF5;
        goto out;
    }
    ret = hdf5_file_read_metadata(self, file_id);
    if (ret < 0) {
        goto out;
    }
    ret = hdf5_file_read_groups(self, file_id);
    if (ret < 0) {
        goto out;
    }
    ret = hdf5_file_read_dimensions(self, file_id);
    if (ret < 0) {
        goto out;
    }
    ret = hdf5_file_check_dimensions(self, file_id);
    if (ret < 0) {
        goto out;
    }
    ret = hdf5_file_read_data(self, file_id);
    if (ret != 0) {
        goto out;
    }

out:
    if (file_id >= 0) {
        status = H5Fclose(file_id);
        if (status < 0) {
            ret = MSP_ERR_HDF5;
        }
    }
    return ret;
}


static int
hdf5_file_write_hdf5_data(hdf5_file_t *self, hid_t file_id, int flags)
{
    herr_t ret = -1;
    herr_t status;
    hid_t group_id, dataset_id, dataspace_id, plist_id;
    hsize_t dim, chunk_size;
    struct _hdf5_field_write {
        const char *name;
        hid_t storage_type;
        hid_t memory_type;
        size_t size;
        void *source;
    };
    struct _hdf5_field_write fields[] = {
        {"/nodes/metadata",
            H5T_STD_I8LE, H5T_NATIVE_CHAR,
            self->tables->nodes.metadata_length, self->tables->nodes.metadata},
        {"/nodes/metadata_offset",
            H5T_STD_U32LE, H5T_NATIVE_UINT32,
            self->tables->nodes.num_rows + 1, self->tables->nodes.metadata_offset},
        {"/nodes/flags",
            H5T_STD_U32LE, H5T_NATIVE_UINT32,
            self->tables->nodes.num_rows, self->tables->nodes.flags},
        {"/nodes/population",
            H5T_STD_I32LE, H5T_NATIVE_INT32,
            self->tables->nodes.num_rows, self->tables->nodes.population},
        {"/nodes/time",
            H5T_IEEE_F64LE, H5T_NATIVE_DOUBLE,
            self->tables->nodes.num_rows, self->tables->nodes.time},

        {"/edges/left",
            H5T_IEEE_F64LE, H5T_NATIVE_DOUBLE,
            self->tables->edges.num_rows, self->tables->edges.left},
        {"/edges/right",
            H5T_IEEE_F64LE, H5T_NATIVE_DOUBLE,
            self->tables->edges.num_rows, self->tables->edges.right},
        {"/edges/parent",
            H5T_STD_I32LE, H5T_NATIVE_INT32,
            self->tables->edges.num_rows, self->tables->edges.parent},
        {"/edges/child",
            H5T_STD_I32LE, H5T_NATIVE_INT32,
            self->tables->edges.num_rows, self->tables->edges.child},

        {"/edges/indexes/insertion_order",
            H5T_STD_I32LE, H5T_NATIVE_INT32,
            self->tables->edges.num_rows, self->tables->indexes.edge_insertion_order},
        {"/edges/indexes/removal_order",
            H5T_STD_I32LE, H5T_NATIVE_INT32,
            self->tables->edges.num_rows, self->tables->indexes.edge_removal_order},

        {"/sites/position",
            H5T_IEEE_F64LE, H5T_NATIVE_DOUBLE,
            self->tables->sites.num_rows, self->tables->sites.position},
        {"/sites/ancestral_state",
            H5T_STD_I8LE, H5T_NATIVE_CHAR,
            self->tables->sites.ancestral_state_length, self->tables->sites.ancestral_state},
        {"/sites/ancestral_state_offset",
            H5T_STD_U32LE, H5T_NATIVE_UINT32,
            self->tables->sites.num_rows + 1, self->tables->sites.ancestral_state_offset},
        {"/sites/metadata",
            H5T_STD_I8LE, H5T_NATIVE_CHAR,
            self->tables->sites.metadata_length, self->tables->sites.metadata},
        {"/sites/metadata_offset",
            H5T_STD_U32LE, H5T_NATIVE_UINT32,
            self->tables->sites.num_rows + 1, self->tables->sites.metadata_offset},

        {"/mutations/site",
            H5T_STD_I32LE, H5T_NATIVE_INT32,
            self->tables->mutations.num_rows, self->tables->mutations.site},
        {"/mutations/node",
            H5T_STD_I32LE, H5T_NATIVE_INT32,
            self->tables->mutations.num_rows, self->tables->mutations.node},
        {"/mutations/parent",
            H5T_STD_I32LE, H5T_NATIVE_INT32,
            self->tables->mutations.num_rows, self->tables->mutations.parent},
        {"/mutations/derived_state",
            H5T_STD_I8LE, H5T_NATIVE_CHAR,
            self->tables->mutations.derived_state_length, self->tables->mutations.derived_state},
        {"/mutations/derived_state_offset",
            H5T_STD_U32LE, H5T_NATIVE_UINT32,
            self->tables->mutations.num_rows + 1, self->tables->mutations.derived_state_offset},
        {"/mutations/metadata",
            H5T_STD_I8LE, H5T_NATIVE_CHAR,
            self->tables->mutations.metadata_length, self->tables->mutations.metadata},
        {"/mutations/metadata_offset",
            H5T_STD_U32LE, H5T_NATIVE_UINT32,
            self->tables->mutations.num_rows + 1, self->tables->mutations.metadata_offset},

        {"/migrations/left",
            H5T_IEEE_F64LE, H5T_NATIVE_DOUBLE,
            self->tables->migrations.num_rows, self->tables->migrations.left},
        {"/migrations/right",
            H5T_IEEE_F64LE, H5T_NATIVE_DOUBLE,
            self->tables->migrations.num_rows, self->tables->migrations.right},
        {"/migrations/time",
            H5T_IEEE_F64LE, H5T_NATIVE_DOUBLE,
            self->tables->migrations.num_rows, self->tables->migrations.time},
        {"/migrations/node",
            H5T_STD_I32LE, H5T_NATIVE_INT32,
            self->tables->migrations.num_rows, self->tables->migrations.node},
        {"/migrations/source",
            H5T_STD_I32LE, H5T_NATIVE_INT32,
            self->tables->migrations.num_rows, self->tables->migrations.source},
        {"/migrations/dest",
            H5T_STD_I32LE, H5T_NATIVE_INT32,
            self->tables->migrations.num_rows, self->tables->migrations.dest},

        {"/provenances/timestamp",
            H5T_STD_I8LE, H5T_NATIVE_CHAR,
            self->tables->provenances.timestamp_length, self->tables->provenances.timestamp},
        {"/provenances/timestamp_offset",
            H5T_STD_U32LE, H5T_NATIVE_UINT32,
            self->tables->provenances.num_rows + 1, self->tables->provenances.timestamp_offset},
        {"/provenances/record",
            H5T_STD_I8LE, H5T_NATIVE_CHAR,
            self->tables->provenances.record_length, self->tables->provenances.record},
        {"/provenances/record_offset",
            H5T_STD_U32LE, H5T_NATIVE_UINT32,
            self->tables->provenances.num_rows + 1, self->tables->provenances.record_offset},
    };
    size_t num_fields = sizeof(fields) / sizeof(struct _hdf5_field_write);
    struct _hdf5_group_write {
        const char *name;
    };
    struct _hdf5_group_write groups[] = {
        {"/sites"},
        {"/mutations"},
        {"/nodes"},
        {"/edges"},
        {"/edges/indexes"},
        {"/migrations"},
        {"/provenances"},
    };
    size_t num_groups = sizeof(groups) / sizeof(struct _hdf5_group_write);
    size_t j;

    /* Create the groups */
    for (j = 0; j < num_groups; j++) {
        group_id = H5Gcreate(file_id, groups[j].name, H5P_DEFAULT, H5P_DEFAULT,
                H5P_DEFAULT);
        if (group_id < 0) {
            goto out;
        }
        status = H5Gclose(group_id);
        if (status < 0) {
            goto out;
        }
    }
    /* now write the datasets */
    for (j = 0; j < num_fields; j++) {
        dim = fields[j].size;
        /* Never create any 0-sized datasets. This causes all sorts of problems in older
         * versions of HDF5, and so we adopt the protocol of omitting the dataset if it
         * is of zero size.
         */
        if (dim > 0) {
            dataspace_id = H5Screate_simple(1, &dim, &dim);
            if (dataspace_id < 0) {
                goto out;
            }
            plist_id = H5Pcreate(H5P_DATASET_CREATE);
            if (plist_id < 0) {
                goto out;
            }
            /* Set the chunk size to the full size of the dataset since we
             * always read the full thing.
             */
            chunk_size = MSP_MAX(1, fields[j].size);
            status = H5Pset_chunk(plist_id, 1, &chunk_size);
            if (status < 0) {
                goto out;
            }
            if (fields[j].memory_type != H5T_NATIVE_DOUBLE) {
                /* For integer types, use the scale offset compression */
                status = H5Pset_scaleoffset(plist_id, H5Z_SO_INT,
                         H5Z_SO_INT_MINBITS_DEFAULT);
                if (status < 0) {
                    goto out;
                }
            }
            if (flags & MSP_DUMP_ZLIB_COMPRESSION) {
                /* Turn on byte shuffling to improve compression */
                status = H5Pset_shuffle(plist_id);
                if (status < 0) {
                    goto out;
                }
                /* Set zlib compression at level 9 (best compression) */
                status = H5Pset_deflate(plist_id, 9);
                if (status < 0) {
                    goto out;
                }
            }
            /* Turn on Fletcher32 checksums for integrity checks */
            status = H5Pset_fletcher32(plist_id);
            if (status < 0) {
                goto out;
            }
            dataset_id = H5Dcreate2(file_id, fields[j].name,
                    fields[j].storage_type, dataspace_id, H5P_DEFAULT,
                    plist_id, H5P_DEFAULT);
            if (fields[j].size > 0) {
                /* Don't write zero sized datasets to work-around problems
                 * with older versions of hdf5. */
                status = H5Dwrite(dataset_id, fields[j].memory_type, H5S_ALL,
                        H5S_ALL, H5P_DEFAULT, fields[j].source);
                if (status < 0) {
                    goto out;
                }
            }
            status = H5Dclose(dataset_id);
            if (status < 0) {
                goto out;
            }
            status = H5Pclose(plist_id);
            if (status < 0) {
                goto out;
            }
            status = H5Sclose(dataspace_id);
            if (status < 0) {
                goto out;
            }
        }
    }
    ret = 0;
out:
    return ret;
}

static int
hdf5_file_write_hdf5_metadata(hdf5_file_t *self, hid_t file_id)
{
    herr_t status = -1;
    hid_t attr_id, dataspace_id;
    hsize_t dims = 1;
    uint32_t version[2] = {
        MSP_FILE_FORMAT_VERSION_MAJOR, MSP_FILE_FORMAT_VERSION_MINOR};
    uint32_t unused_value = 0;

    struct _hdf5_metadata_write {
        const char *name;
        hid_t parent;
        hid_t storage_type;
        hid_t memory_type;
        size_t size;
        void *source;
    };
    struct _hdf5_metadata_write fields[] = {
        {"format_version", 0, H5T_STD_U32LE, H5T_NATIVE_UINT32, 2, version},
        {"sequence_length", 0, H5T_IEEE_F64LE, H5T_NATIVE_DOUBLE, 1,
            &self->tables->sequence_length},
        /* The sample size attribute is vestigial, and only included because
         * older versions of msprime give a better error condition when confronted
         * with a newer file format. Due to a bug in the way that this attribute
         * was loaded, versions of msprime pre 0.4.0 would complain about a missing
         * attribute rather than giving a File format error. */
        {"sample_size", 0, H5T_STD_U32LE, H5T_NATIVE_UINT32, 1, &unused_value},
    };
    size_t num_fields = sizeof(fields) / sizeof(struct _hdf5_metadata_write);
    size_t j;

    for (j = 0; j < num_fields; j++) {
        dims = fields[j].size;
        dataspace_id = H5Screate_simple(1, &dims, NULL);
        if (dataspace_id < 0) {
            status = (herr_t) dataspace_id;
            goto out;
        }
        attr_id = H5Acreate(file_id, fields[j].name,
                fields[j].storage_type, dataspace_id, H5P_DEFAULT,
                H5P_DEFAULT);
        if (attr_id < 0) {
            goto out;
        }
        status = H5Awrite(attr_id, fields[j].memory_type, fields[j].source);
        if (status < 0) {
            goto out;
        }
        status = H5Aclose(attr_id);
        if (status < 0) {
            goto out;
        }
        status = H5Sclose(dataspace_id);
        if (status < 0) {
            goto out;
        }
    }
 out:
    return status;
}

static int WARN_UNUSED
hdf5_file_dump(hdf5_file_t *self, const char *filename, int flags)
{
    int ret = MSP_ERR_HDF5;
    herr_t status;
    hid_t file_id = -1;

    file_id = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if (file_id < 0) {
        goto out;
    }
    status = hdf5_file_write_hdf5_metadata(self, file_id);
    if (status < 0) {
        goto out;
    }
    ret = hdf5_file_write_hdf5_data(self, file_id, flags);
    if (ret < 0) {
        goto out;
    }
    ret = 0;
out:
    if (file_id > 0) {
        status = H5Fclose(file_id);
        if (status < 0) {
            ret = MSP_ERR_HDF5;
        }
    }
    return ret;
}

/*************************
 * table_collection
 *************************/

typedef struct {
    node_id_t index;
    /* These are the sort keys in order */
    double first;
    double second;
    node_id_t third;
    node_id_t fourth;
} index_sort_t;

static int
cmp_index_sort(const void *a, const void *b) {
    const index_sort_t *ca = (const index_sort_t *) a;
    const index_sort_t *cb = (const index_sort_t *) b;
    int ret = (ca->first > cb->first) - (ca->first < cb->first);
    if (ret == 0) {
        ret = (ca->second > cb->second) - (ca->second < cb->second);
        if (ret == 0) {
            ret = (ca->third > cb->third) - (ca->third < cb->third);
            if (ret == 0) {
                ret = (ca->fourth > cb->fourth) - (ca->fourth < cb->fourth);
            }
        }
    }
    return ret;
}

static int
table_collection_check_offsets(table_collection_t *self)
{
    int ret = 0;

    ret = check_offsets(self->nodes.num_rows, self->nodes.metadata_offset,
            self->nodes.metadata_length, true);
    if (ret != 0) {
        goto out;
    }
    ret = check_offsets(self->sites.num_rows, self->sites.ancestral_state_offset,
            self->sites.ancestral_state_length, true);
    if (ret != 0) {
        goto out;
    }
    ret = check_offsets(self->sites.num_rows, self->sites.metadata_offset,
            self->sites.metadata_length, true);
    if (ret != 0) {
        goto out;
    }
    ret = check_offsets(self->mutations.num_rows, self->mutations.derived_state_offset,
            self->mutations.derived_state_length, true);
    if (ret != 0) {
        goto out;
    }
    ret = check_offsets(self->mutations.num_rows, self->mutations.metadata_offset,
            self->mutations.metadata_length, true);
    if (ret != 0) {
        goto out;
    }
    ret = check_offsets(self->provenances.num_rows, self->provenances.timestamp_offset,
            self->provenances.timestamp_length, true);
    if (ret != 0) {
        goto out;
    }
    ret = check_offsets(self->provenances.num_rows, self->provenances.record_offset,
            self->provenances.record_length, true);
    if (ret != 0) {
        goto out;
    }
    ret = 0;
out:
    return ret;
}

int
table_collection_print_state(table_collection_t *self, FILE *out)
{
    fprintf(out, "Table collection state\n");
    fprintf(out, "sequence_length = %f\n", self->sequence_length);
    node_table_print_state(&self->nodes, out);
    edge_table_print_state(&self->edges, out);
    migration_table_print_state(&self->migrations, out);
    site_table_print_state(&self->sites, out);
    mutation_table_print_state(&self->mutations, out);
    provenance_table_print_state(&self->provenances, out);
    return 0;
}

int
table_collection_alloc(table_collection_t *self, int flags)
{
    int ret = 0;
    memset(self, 0, sizeof(*self));
    if (flags & MSP_ALLOC_TABLES) {
        /* Allocate all the tables with their default increments */
        ret = node_table_alloc(&self->nodes, 0, 0);
        if (ret != 0) {
            goto out;
        }
        ret = edge_table_alloc(&self->edges, 0);
        if (ret != 0) {
            goto out;
        }
        ret = migration_table_alloc(&self->migrations, 0);
        if (ret != 0) {
            goto out;
        }
        ret = site_table_alloc(&self->sites, 0, 0, 0);
        if (ret != 0) {
            goto out;
        }
        ret = mutation_table_alloc(&self->mutations, 0, 0, 0);
        if (ret != 0) {
            goto out;
        }
        ret = provenance_table_alloc(&self->provenances, 0, 0, 0);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

int
table_collection_free(table_collection_t *self)
{
    int ret = 0;

    node_table_free(&self->nodes);
    edge_table_free(&self->edges);
    migration_table_free(&self->migrations);
    site_table_free(&self->sites);
    mutation_table_free(&self->mutations);
    provenance_table_free(&self->provenances);
    msp_safe_free(self->indexes.edge_insertion_order);
    msp_safe_free(self->indexes.edge_removal_order);
    return ret;
}

int WARN_UNUSED
table_collection_copy(table_collection_t *self, table_collection_t *dest)
{
    int ret = 0;
    size_t index_size;

    ret = node_table_copy(&self->nodes, &dest->nodes);
    if (ret != 0) {
        goto out;
    }
    ret = edge_table_copy(&self->edges, &dest->edges);
    if (ret != 0) {
        goto out;
    }
    ret = migration_table_copy(&self->migrations, &dest->migrations);
    if (ret != 0) {
        goto out;
    }
    ret = site_table_copy(&self->sites, &dest->sites);
    if (ret != 0) {
        goto out;
    }
    ret = mutation_table_copy(&self->mutations, &dest->mutations);
    if (ret != 0) {
        goto out;
    }
    ret = provenance_table_copy(&self->provenances, &dest->provenances);
    if (ret != 0) {
        goto out;
    }
    dest->sequence_length = self->sequence_length;
    if (table_collection_is_indexed(self)) {
        table_collection_drop_indexes(dest);
        index_size = self->edges.num_rows * sizeof(edge_id_t);
        dest->indexes.edge_insertion_order = malloc(index_size);
        dest->indexes.edge_removal_order = malloc(index_size);
        if (dest->indexes.edge_insertion_order == NULL
                || dest->indexes.edge_removal_order == NULL) {
            ret = MSP_ERR_NO_MEMORY;
            goto out;
        }
        memcpy(dest->indexes.edge_insertion_order, self->indexes.edge_insertion_order,
                index_size);
        memcpy(dest->indexes.edge_removal_order, self->indexes.edge_removal_order,
                index_size);
    }
out:
    return ret;
}

bool
table_collection_is_indexed(table_collection_t *self)
{
    return self->indexes.edge_insertion_order != NULL
        && self->indexes.edge_removal_order != NULL;
}

int
table_collection_drop_indexes(table_collection_t *self)
{
    msp_safe_free(self->indexes.edge_insertion_order);
    msp_safe_free(self->indexes.edge_removal_order);
    self->indexes.edge_insertion_order = NULL;
    self->indexes.edge_removal_order = NULL;
    return 0;
}

int WARN_UNUSED
table_collection_build_indexes(table_collection_t *self, int flags)
{
    int ret = MSP_ERR_GENERIC;
    size_t j;
    double *time = self->nodes.time;
    index_sort_t *sort_buff = NULL;
    node_id_t parent;

    /* Alloc the indexes. Free them first if they aren't NULL. */
    msp_safe_free(self->indexes.edge_insertion_order);
    msp_safe_free(self->indexes.edge_removal_order);
    self->indexes.edge_insertion_order = malloc(self->edges.num_rows * sizeof(edge_id_t));
    self->indexes.edge_removal_order = malloc(self->edges.num_rows * sizeof(edge_id_t));
    if (self->indexes.edge_insertion_order == NULL
            || self->indexes.edge_removal_order == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }

    /* Alloc the sort buffer */
    sort_buff = malloc(self->edges.num_rows * sizeof(index_sort_t));
    if (sort_buff == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }

    /* sort by left and increasing time to give us the order in which
     * records should be inserted */
    for (j = 0; j < self->edges.num_rows; j++) {
        sort_buff[j].index = (node_id_t ) j;
        sort_buff[j].first = self->edges.left[j];
        parent = self->edges.parent[j];
        if (parent == MSP_NULL_NODE) {
            ret = MSP_ERR_NULL_PARENT;
            goto out;
        }
        if (parent < 0 || parent >= (node_id_t) self->nodes.num_rows) {
            ret = MSP_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        sort_buff[j].second = time[parent];
        sort_buff[j].third = parent;
        sort_buff[j].fourth = self->edges.child[j];
    }
    qsort(sort_buff, self->edges.num_rows, sizeof(index_sort_t), cmp_index_sort);
    for (j = 0; j < self->edges.num_rows; j++) {
        self->indexes.edge_insertion_order[j] = sort_buff[j].index;
    }
    /* sort by right and decreasing parent time to give us the order in which
     * records should be removed. */
    for (j = 0; j < self->edges.num_rows; j++) {
        sort_buff[j].index = (node_id_t ) j;
        sort_buff[j].first = self->edges.right[j];
        parent = self->edges.parent[j];
        if (parent == MSP_NULL_NODE) {
            ret = MSP_ERR_NULL_PARENT;
            goto out;
        }
        if (parent < 0 || parent >= (node_id_t) self->nodes.num_rows) {
            ret = MSP_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        sort_buff[j].second = -time[parent];
        sort_buff[j].third = -parent;
        sort_buff[j].fourth = -self->edges.child[j];
    }
    qsort(sort_buff, self->edges.num_rows, sizeof(index_sort_t), cmp_index_sort);
    for (j = 0; j < self->edges.num_rows; j++) {
        self->indexes.edge_removal_order[j] = sort_buff[j].index;
    }
    ret = 0;
out:
    if (sort_buff != NULL) {
        free(sort_buff);
    }
    return ret;
}

int WARN_UNUSED
table_collection_load(table_collection_t *self, const char *filename, int flags)
{
    int ret = 0;
    hdf5_file_t hdf5_file;

    ret = hdf5_file_alloc(&hdf5_file, self);
    if (ret != 0) {
        goto out;
    }
    ret = hdf5_file_load(&hdf5_file, filename);
    if (ret != 0) {
        goto out;
    }
    ret = table_collection_check_offsets(self);
out:
    hdf5_file_free(&hdf5_file);
    return ret;
}

int WARN_UNUSED
table_collection_dump(table_collection_t *self, const char *filename, int flags)
{
    int ret = 0;
    hdf5_file_t hdf5_file;

    if (!table_collection_is_indexed(self)) {
        ret = table_collection_build_indexes(self, 0);
        if (ret != 0) {
            goto out;
        }
    }
    ret = hdf5_file_alloc(&hdf5_file, self);
    if (ret != 0) {
        goto out;
    }
    ret = hdf5_file_dump(&hdf5_file, filename, flags);
    if (ret != 0) {
        goto out;
    }
out:
    hdf5_file_free(&hdf5_file);
    return ret;
}

int WARN_UNUSED
table_collection_simplify(table_collection_t *self,
        node_id_t *samples, size_t num_samples, int flags, node_id_t *node_map)
{
    int ret = 0;
    simplifier_t simplifier;

    /* TODO the simplifier object should take a table collection as a parameter */
    ret = simplifier_alloc(&simplifier, self->sequence_length,
            samples, num_samples, &self->nodes, &self->edges, &self->migrations,
            &self->sites, &self->mutations, 0, flags);
    if (ret != 0) {
        goto out;
    }
    ret = simplifier_run(&simplifier, node_map);
    if (ret != 0) {
        goto out;
    }
out:
    simplifier_free(&simplifier);
    return ret;
}
