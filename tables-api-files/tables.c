/*
** Copyright (C) 2017 University of Oxford
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

#include <gsl/gsl_math.h>

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
check_offsets(size_t num_rows, table_size_t *offsets)
{
    int ret = MSP_ERR_BAD_OFFSET;
    size_t j;

    if (offsets[0] != 0) {
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
    table_size_t increment = GSL_MAX(additional_rows, self->max_rows_increment);
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
    table_size_t increment = GSL_MAX(additional_length,
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

int
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
        ret = check_offsets(num_rows, metadata_offset);
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
    fprintf(out, "index\tflags\tis_sample\ttime\tpopulation\tmetadata_offset\tmetadata\n");
    for (j = 0; j < self->num_rows; j++) {
        fprintf(out, "%d\t%d\t%d\t%f\t%d\t%d\t", (int) j, self->flags[j],
                (int) (self->flags[j] & MSP_NODE_IS_SAMPLE), self->time[j],
                (int) self->population[j], self->metadata_offset[j]);
        for (k = self->metadata_offset[j]; k < self->metadata_offset[j + 1]; k++) {
            fprintf(out, "%c", self->metadata[k]);
        }
        fprintf(out, "\n");
    }
    assert(self->metadata_offset[0] == 0);
    assert(self->metadata_offset[self->num_rows] == self->metadata_length);
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
    size_t increment = GSL_MAX(additional_rows, self->max_rows_increment);
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
    size_t j;

    fprintf(out, TABLE_SEP);
    fprintf(out, "edge_table: %p:\n", (void *) self);
    fprintf(out, "num_rows          = %d\tmax= %d\tincrement = %d)\n",
            (int) self->num_rows, (int) self->max_rows, (int) self->max_rows_increment);
    fprintf(out, TABLE_SEP);
    fprintf(out, "index\tleft\tright\tparent\tchild\n");
    for (j = 0; j < self->num_rows; j++) {
        fprintf(out, "%d\t%.3f\t%.3f\t%d\t%d\t", (int) j, self->left[j], self->right[j],
                self->parent[j], self->child[j]);
        fprintf(out, "\n");
    }
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
    table_size_t increment = (table_size_t) GSL_MAX(additional_rows, self->max_rows_increment);
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
    table_size_t increment = (table_size_t) GSL_MAX(additional_length,
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
    table_size_t increment = (table_size_t) GSL_MAX(additional_length,
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
        ret = check_offsets(num_rows, metadata_offset);
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
    ret = check_offsets(num_rows, ancestral_state_offset);
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

int
site_table_set_columns(site_table_t *self, size_t num_rows, double *position,
        const char *ancestral_state, table_size_t *ancestral_state_length,
        const char *metadata, table_size_t *metadata_length)
{
    int ret = 0;

    ret = site_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = site_table_append_columns(self, num_rows, position, ancestral_state,
            ancestral_state_length, metadata, metadata_length);
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
    table_size_t j, k;

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
    fprintf(out, "index\tposition\tancestral_state_offset\tancestral_state\t"
            "metadata_offset\tmetadata\n");
    for (j = 0; j < self->num_rows; j++) {
        fprintf(out, "%d\t%f\t%d\t", (int) j, self->position[j],
                self->ancestral_state_offset[j]);
        for (k = self->ancestral_state_offset[j];
                k < self->ancestral_state_offset[j + 1]; k++) {
            fprintf(out, "%c", self->ancestral_state[k]);
        }
        fprintf(out, "\t%d\t", self->metadata_offset[j]);
        for (k = self->metadata_offset[j]; k < self->metadata_offset[j + 1]; k++) {
            fprintf(out, "%c", self->metadata[k]);
        }
        fprintf(out, "\n");
    }

    assert(self->ancestral_state_offset[0] == 0);
    assert(self->ancestral_state_length
            == self->ancestral_state_offset[self->num_rows]);
    assert(self->metadata_offset[0] == 0);
    assert(self->metadata_length == self->metadata_offset[self->num_rows]);
}

/*************************
 * mutation table
 *************************/

static int
mutation_table_expand_main_columns(mutation_table_t *self, size_t additional_rows)
{
    int ret = 0;
    table_size_t increment = (table_size_t) GSL_MAX(additional_rows, self->max_rows_increment);
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
    table_size_t increment = (table_size_t) GSL_MAX(additional_length,
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
    table_size_t increment = (table_size_t) GSL_MAX(additional_length,
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
        ret = check_offsets(num_rows, metadata_offset);
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
    ret = check_offsets(num_rows, derived_state_offset);
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
    size_t j, k;

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
    fprintf(out,
            "index\tsite\tnode\tparent\tderived_state_offset\tderived_state\t"
            "metadata_offset\tmetadata\n");
    for (j = 0; j < self->num_rows; j++) {
        fprintf(out, "%d\t%d\t%d\t%d\t%d\t", (int) j, self->site[j], self->node[j],
                self->parent[j], self->derived_state_offset[j]);
        for (k = self->derived_state_offset[j];
                k < self->derived_state_offset[j + 1]; k++) {
            fprintf(out, "%c", self->derived_state[k]);
        }
        fprintf(out, "\t%d\t", self->metadata_offset[j]);
        for (k = self->metadata_offset[j]; k < self->metadata_offset[j + 1]; k++) {
            fprintf(out, "%c", self->metadata[k]);
        }
        fprintf(out, "\n");
    }

    assert(self->derived_state_offset[0] == 0);
    assert(self->derived_state_length
            == self->derived_state_offset[self->num_rows]);
    assert(self->metadata_offset[0] == 0);
    assert(self->metadata_length
            == self->metadata_offset[self->num_rows]);
}

/*************************
 * migration table
 *************************/

static int
migration_table_expand(migration_table_t *self, size_t additional_rows)
{
    int ret = 0;
    size_t increment = GSL_MAX(additional_rows, self->max_rows_increment);
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
    size_t j;

    fprintf(out, TABLE_SEP);
    fprintf(out, "migration_table: %p:\n", (void *) self);
    fprintf(out, "num_rows = %d\tmax= %d\tincrement = %d)\n",
            (int) self->num_rows, (int) self->max_rows, (int) self->max_rows_increment);
    fprintf(out, TABLE_SEP);
    fprintf(out, "index\tleft\tright\tnode\tsource\tdest\ttime\n");
    for (j = 0; j < self->num_rows; j++) {
        fprintf(out, "%d\t%.3f\t%.3f\t%d\t%d\t%d\t%f\n", (int) j, self->left[j],
                self->right[j], (int) self->node[j], (int) self->source[j],
                (int) self->dest[j], self->time[j]);
    }
}


/*************************
 * provenance table
 *************************/

static int
provenance_table_expand_main_columns(provenance_table_t *self, table_size_t additional_rows)
{
    int ret = 0;
    table_size_t increment = GSL_MAX(additional_rows, self->max_rows_increment);
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
    table_size_t increment = GSL_MAX(additional_length,
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
    table_size_t increment = GSL_MAX(additional_length,
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

    ret = check_offsets(num_rows, timestamp_offset);
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

    ret = check_offsets(num_rows, record_offset);
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
            sequence_length = GSL_MAX(sequence_length, edges->right[j]);
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
    num_nodes_alloc = GSL_MAX(max_alloc_block, 1 + nodes->num_rows);
    num_edges_alloc = GSL_MAX(max_alloc_block, 1 + edges->num_rows);

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
            r = GSL_MIN(r, H[h]->right);
            h++;
            simplifier_free_avl_node(self, node);
            avl_unlink_node(Q, node);
            node = node->next;
        }
        next_l = 0;
        if (node != NULL) {
            next_l = ((simplify_segment_t *) node->item)->left;
            r = GSL_MIN(r, next_l);
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
