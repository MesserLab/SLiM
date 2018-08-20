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
#include <float.h>

#include "tables.h"
#include "uuid.h"
#include "kastore.h"

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

typedef struct {
    const char *name;
    void **array_dest;
    table_size_t *len_dest;
    table_size_t len_offset;
    int type;
} read_table_col_t;

static int
read_table_cols(kastore_t *store, read_table_col_t *read_cols, size_t num_cols)
{
    int ret = 0;
    size_t len;
    int type;
    size_t j;
    table_size_t last_len;

    /* Set all the size destinations to -1 so we can detect the first time we
     * read it. Therefore, destinations that are supposed to have the same
     * length will take the value of the first instance, and we check each
     * subsequent value against this. */
    for (j = 0; j < num_cols; j++) {
        *read_cols[j].len_dest = (table_size_t) -1;
    }
    for (j = 0; j < num_cols; j++) {
        ret = kastore_gets(store, read_cols[j].name, read_cols[j].array_dest,
                &len, &type);
        if (ret != 0) {
            ret = msp_set_kas_error(ret);
            goto out;
        }
        last_len = *read_cols[j].len_dest;
        if (last_len == (table_size_t) -1) {
            *read_cols[j].len_dest = (table_size_t) (len - read_cols[j].len_offset);
        } else if ((last_len + read_cols[j].len_offset) != (table_size_t) len) {
            ret = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        if (type != read_cols[j].type) {
            ret = MSP_ERR_FILE_FORMAT;
            goto out;
        }
    }
out:
    return ret;
}

typedef struct {
    const char *name;
    void *array;
    table_size_t len;
    int type;
} write_table_col_t;

static int
write_table_cols(kastore_t *store, write_table_col_t *write_cols, size_t num_cols)
{
    int ret = 0;
    size_t j;

    for (j = 0; j < num_cols; j++) {
        ret = kastore_puts(store, write_cols[j].name, write_cols[j].array,
                write_cols[j].len, write_cols[j].type, 0);
        if (ret != 0) {
            ret = msp_set_kas_error(ret);
            goto out;
        }
    }
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
        ret = expand_column((void **) &self->individual, new_size, sizeof(individual_id_t));
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
            self->time, self->population, self->individual,
            self->metadata, self->metadata_offset);
}

int WARN_UNUSED
node_table_set_columns(node_table_t *self, size_t num_rows, uint32_t *flags, double *time,
        population_id_t *population, individual_id_t *individual, const char *metadata,
        uint32_t *metadata_offset)
{
    int ret;

    ret = node_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = node_table_append_columns(self, num_rows, flags, time, population, individual,
            metadata, metadata_offset);
out:
    return ret;
}

int
node_table_append_columns(node_table_t *self, size_t num_rows, uint32_t *flags, double *time,
        population_id_t *population, individual_id_t *individual, const char *metadata,
        uint32_t *metadata_offset)
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
    memcpy(self->time + self->num_rows, time, num_rows * sizeof(double));
    memcpy(self->flags + self->num_rows, flags, num_rows * sizeof(uint32_t));
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
    if (individual == NULL) {
        /* Set individual to NULL_INDIVIDUAL (-1) if not specified */
        memset(self->individual + self->num_rows, 0xff,
                num_rows * sizeof(individual_id_t));
    } else {
        memcpy(self->individual + self->num_rows, individual,
                num_rows * sizeof(individual_id_t));
    }
    self->num_rows += (table_size_t) num_rows;
    self->metadata_offset[self->num_rows] = self->metadata_length;
out:
    return ret;
}

static node_id_t
node_table_add_row_internal(node_table_t *self, uint32_t flags, double time,
        population_id_t population, individual_id_t individual,
        const char *metadata, table_size_t metadata_length)
{
    assert(self->num_rows < self->max_rows);
    assert(self->metadata_length + metadata_length <= self->max_metadata_length);
    memcpy(self->metadata + self->metadata_length, metadata, metadata_length);
    self->flags[self->num_rows] = flags;
    self->time[self->num_rows] = time;
    self->population[self->num_rows] = population;
    self->individual[self->num_rows] = individual;
    self->metadata_offset[self->num_rows + 1] = self->metadata_length + metadata_length;
    self->metadata_length += metadata_length;
    self->num_rows++;
    return (node_id_t) self->num_rows - 1;
}

node_id_t
node_table_add_row(node_table_t *self, uint32_t flags, double time,
        population_id_t population, individual_id_t individual,
        const char *metadata, size_t metadata_length)
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
    ret = node_table_add_row_internal(self, flags, time, population, individual,
            metadata, (table_size_t) metadata_length);
out:
    return ret;
}

int WARN_UNUSED
node_table_clear(node_table_t *self)
{
    return node_table_truncate(self, 0);
}

int
node_table_truncate(node_table_t *self, size_t num_rows)
{
    int ret = 0;
    table_size_t n = (table_size_t) num_rows;

    if (n > self->num_rows) {
        ret = MSP_ERR_BAD_TABLE_POSITION;
        goto out;
    }
    self->num_rows = n;
    self->metadata_length = self->metadata_offset[n];
out:
    return ret;
}

int
node_table_free(node_table_t *self)
{
    if (self->max_rows > 0) {
        msp_safe_free(self->flags);
        msp_safe_free(self->time);
        msp_safe_free(self->population);
        msp_safe_free(self->individual);
        msp_safe_free(self->metadata);
        msp_safe_free(self->metadata_offset);
    }
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
    fprintf(out, "id\tflags\ttime\tpopulation\tindividual\tmetadata_offset\tmetadata\n");
    for (j = 0; j < self->num_rows; j++) {
        fprintf(out, "%d\t%d\t%f\t%d\t%d\t%d\t", (int) j, self->flags[j], self->time[j],
                (int) self->population[j], self->individual[j], self->metadata_offset[j]);
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
    int ret = MSP_ERR_IO;
    size_t j;
    table_size_t metadata_len;
    int err;

    err = fprintf(out, "id\tis_sample\ttime\tpopulation\tindividual\tmetadata\n");
    if (err < 0) {
        goto out;
    }
    for (j = 0; j < self->num_rows; j++) {
        metadata_len = self->metadata_offset[j + 1] - self->metadata_offset[j];
        err = fprintf(out, "%d\t%d\t%f\t%d\t%d\t%.*s\n", (int) j,
                (int) (self->flags[j] & MSP_NODE_IS_SAMPLE),
                self->time[j], self->population[j], self->individual[j],
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
node_table_equals(node_table_t *self, node_table_t *other)
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
            && memcmp(self->individual, other->individual,
                    self->num_rows * sizeof(individual_id_t)) == 0
            && memcmp(self->metadata_offset, other->metadata_offset,
                    (self->num_rows + 1) * sizeof(table_size_t)) == 0
            && memcmp(self->metadata, other->metadata,
                    self->metadata_length * sizeof(char)) == 0;
    }
    return ret;
}

int
node_table_get_row(node_table_t *self, size_t index, node_t *row)
{
    int ret = 0;
    if (index >= self->num_rows) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    row->id = (node_id_t) index;
    row->flags = self->flags[index];
    row->time = self->time[index];
    row->population = self->population[index];
    row->individual = self->individual[index];
    row->metadata_length = self->metadata_offset[index + 1]
        - self->metadata_offset[index];
    row->metadata = self->metadata + self->metadata_offset[index];
out:
    return ret;
}

static int
node_table_dump(node_table_t *self, kastore_t *store)
{
    write_table_col_t write_cols[] = {
        {"nodes/time", (void *) self->time, self->num_rows, KAS_FLOAT64},
        {"nodes/flags", (void *) self->flags, self->num_rows, KAS_UINT32},
        {"nodes/population", (void *) self->population, self->num_rows, KAS_INT32},
        {"nodes/individual", (void *) self->individual, self->num_rows, KAS_INT32},
        {"nodes/metadata", (void *) self->metadata, self->metadata_length, KAS_UINT8},
        {"nodes/metadata_offset", (void *) self->metadata_offset, self->num_rows + 1,
            KAS_UINT32},
    };
    return write_table_cols(store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
}

static int
node_table_load(node_table_t *self, kastore_t *store)
{
    read_table_col_t read_cols[] = {
        {"nodes/time", (void **) &self->time, &self->num_rows, 0, KAS_FLOAT64},
        {"nodes/flags", (void **) &self->flags, &self->num_rows, 0, KAS_UINT32},
        {"nodes/population", (void **) &self->population, &self->num_rows, 0,
            KAS_INT32},
        {"nodes/individual", (void **) &self->individual, &self->num_rows, 0,
            KAS_INT32},
        {"nodes/metadata", (void **) &self->metadata, &self->metadata_length, 0,
            KAS_UINT8},
        {"nodes/metadata_offset", (void **) &self->metadata_offset, &self->num_rows,
            1, KAS_UINT32},
    };
    return read_table_cols(store, read_cols, sizeof(read_cols) / sizeof(*read_cols));
}

/*************************
 * edge table
 *************************/

static int
edge_table_expand_columns(edge_table_t *self, size_t additional_rows)
{
    int ret = 0;
    table_size_t increment = MSP_MAX(
        (table_size_t) additional_rows, self->max_rows_increment);
    table_size_t new_size = self->max_rows + increment;

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
    self->max_rows_increment = (table_size_t) max_rows_increment;
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
    self->num_rows += (table_size_t) num_rows;
out:
    return ret;
}

int
edge_table_clear(edge_table_t *self)
{
    return edge_table_truncate(self, 0);
}

int
edge_table_truncate(edge_table_t *self, size_t num_rows)
{
    int ret = 0;
    table_size_t n = (table_size_t) num_rows;

    if (n > self->num_rows) {
        ret = MSP_ERR_BAD_TABLE_POSITION;
        goto out;
    }
    self->num_rows = n;
out:
    return ret;
}

int
edge_table_free(edge_table_t *self)
{
    if (self->max_rows > 0) {
        msp_safe_free(self->left);
        msp_safe_free(self->right);
        msp_safe_free(self->parent);
        msp_safe_free(self->child);
    }
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
edge_table_equals(edge_table_t *self, edge_table_t *other)
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

int
edge_table_get_row(edge_table_t *self, size_t index, edge_t *row)
{
    int ret = 0;
    if (index >= self->num_rows) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    row->id = (edge_id_t) index;
    row->left = self->left[index];
    row->right = self->right[index];
    row->parent = self->parent[index];
    row->child = self->child[index];
out:
    return ret;
}

static int
edge_table_dump(edge_table_t *self, kastore_t *store)
{
    write_table_col_t write_cols[] = {
        {"edges/left", (void *) self->left, self->num_rows, KAS_FLOAT64},
        {"edges/right", (void *) self->right, self->num_rows, KAS_FLOAT64},
        {"edges/parent", (void *) self->parent, self->num_rows, KAS_INT32},
        {"edges/child", (void *) self->child, self->num_rows, KAS_INT32},
    };
    return write_table_cols(store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
}

static int
edge_table_load(edge_table_t *self, kastore_t *store)
{
    read_table_col_t read_cols[] = {
        {"edges/left", (void **) &self->left, &self->num_rows, 0, KAS_FLOAT64},
        {"edges/right", (void **) &self->right, &self->num_rows, 0, KAS_FLOAT64},
        {"edges/parent", (void **) &self->parent, &self->num_rows, 0, KAS_INT32},
        {"edges/child", (void **) &self->child, &self->num_rows, 0, KAS_INT32},
    };
    return read_table_cols(store, read_cols, sizeof(read_cols) / sizeof(*read_cols));
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
site_table_equals(site_table_t *self, site_table_t *other)
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
    return site_table_truncate(self, 0);
}

int
site_table_truncate(site_table_t *self, size_t num_rows)
{
    int ret = 0;
    table_size_t n = (table_size_t) num_rows;
    if (n > self->num_rows) {
        ret = MSP_ERR_BAD_TABLE_POSITION;
        goto out;
    }
    self->num_rows = n;
    self->ancestral_state_length = self->ancestral_state_offset[n];
    self->metadata_length = self->metadata_offset[n];
out:
    return ret;
}

int
site_table_free(site_table_t *self)
{
    if (self->max_rows > 0) {
        msp_safe_free(self->position);
        msp_safe_free(self->ancestral_state);
        msp_safe_free(self->ancestral_state_offset);
        msp_safe_free(self->metadata);
        msp_safe_free(self->metadata_offset);
    }
    return 0;
}

void
site_table_print_state(site_table_t *self, FILE *out)
{
    int ret;

    fprintf(out, TABLE_SEP);
    fprintf(out, "site_table: %p:\n", (void *) self);
    fprintf(out, "num_rows = %d\t(max= %d\tincrement = %d)\n",
            (int) self->num_rows, (int) self->max_rows, (int) self->max_rows_increment);
    fprintf(out, "ancestral_state_length = %d\t(max= %d\tincrement = %d)\n",
            (int) self->ancestral_state_length,
            (int) self->max_ancestral_state_length,
            (int) self->max_ancestral_state_length_increment);
    fprintf(out, "metadata_length = %d(\tmax= %d\tincrement = %d)\n",
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
site_table_get_row(site_table_t *self, size_t index, site_t *row)
{
    int ret = 0;
    if (index >= self->num_rows) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    row->id = (site_id_t) index;
    row->position = self->position[index];
    row->ancestral_state_length = self->ancestral_state_offset[index + 1]
        - self->ancestral_state_offset[index];
    row->ancestral_state = self->ancestral_state + self->ancestral_state_offset[index];
    row->metadata_length = self->metadata_offset[index + 1]
        - self->metadata_offset[index];
    row->metadata = self->metadata + self->metadata_offset[index];
    /* This struct has a placeholder for mutations. Probably should be separate
     * structs for this (site_table_row_t?) */
    row->mutations_length = 0;
    row->mutations = NULL;
out:
    return ret;
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

static int
site_table_dump(site_table_t *self, kastore_t *store)
{
    write_table_col_t write_cols[] = {
        {"sites/position", (void *) self->position, self->num_rows, KAS_FLOAT64},
        {"sites/ancestral_state", (void *) self->ancestral_state,
            self->ancestral_state_length, KAS_UINT8},
        {"sites/ancestral_state_offset", (void *) self->ancestral_state_offset,
            self->num_rows + 1, KAS_UINT32},
        {"sites/metadata", (void *) self->metadata, self->metadata_length, KAS_UINT8},
        {"sites/metadata_offset", (void *) self->metadata_offset,
            self->num_rows + 1, KAS_UINT32},
    };
    return write_table_cols(store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
}

static int
site_table_load(site_table_t *self, kastore_t *store)
{
    read_table_col_t read_cols[] = {
        {"sites/position", (void **) &self->position, &self->num_rows, 0, KAS_FLOAT64},
        {"sites/ancestral_state", (void **) &self->ancestral_state,
            &self->ancestral_state_length, 0, KAS_UINT8},
        {"sites/ancestral_state_offset", (void **) &self->ancestral_state_offset,
            &self->num_rows, 1, KAS_UINT32},
        {"sites/metadata", (void **) &self->metadata,
            &self->metadata_length, 0, KAS_UINT8},
        {"sites/metadata_offset", (void **) &self->metadata_offset,
            &self->num_rows, 1, KAS_UINT32},
    };
    return read_table_cols(store, read_cols, sizeof(read_cols) / sizeof(*read_cols));
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
        self->max_metadata_length = new_size;
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
mutation_table_equals(mutation_table_t *self, mutation_table_t *other)
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
    return mutation_table_truncate(self, 0);
}

int
mutation_table_truncate(mutation_table_t *mutations, size_t num_rows)
{
    int ret = 0;
    table_size_t n = (table_size_t) num_rows;

    if (n > mutations->num_rows) {
        ret = MSP_ERR_BAD_TABLE_POSITION;
        goto out;
    }
    mutations->num_rows = n;
    mutations->derived_state_length = mutations->derived_state_offset[n];
    mutations->metadata_length = mutations->metadata_offset[n];
out:
    return ret;
}

int
mutation_table_free(mutation_table_t *self)
{
    if (self->max_rows > 0) {
        msp_safe_free(self->node);
        msp_safe_free(self->site);
        msp_safe_free(self->parent);
        msp_safe_free(self->derived_state);
        msp_safe_free(self->derived_state_offset);
        msp_safe_free(self->metadata);
        msp_safe_free(self->metadata_offset);
    }
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
mutation_table_get_row(mutation_table_t *self, size_t index, mutation_t *row)
{
    int ret = 0;
    if (index >= self->num_rows) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    row->id = (mutation_id_t) index;
    row->site = self->site[index];
    row->node = self->node[index];
    row->parent = self->parent[index];
    row->derived_state_length = self->derived_state_offset[index + 1]
        - self->derived_state_offset[index];
    row->derived_state = self->derived_state + self->derived_state_offset[index];
    row->metadata_length = self->metadata_offset[index + 1]
        - self->metadata_offset[index];
    row->metadata = self->metadata + self->metadata_offset[index];
out:
    return ret;
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

static int
mutation_table_dump(mutation_table_t *self, kastore_t *store)
{
    write_table_col_t write_cols[] = {
        {"mutations/site", (void *) self->site, self->num_rows, KAS_INT32},
        {"mutations/node", (void *) self->node, self->num_rows, KAS_INT32},
        {"mutations/parent", (void *) self->parent, self->num_rows, KAS_INT32},
        {"mutations/derived_state", (void *) self->derived_state,
            self->derived_state_length, KAS_UINT8},
        {"mutations/derived_state_offset", (void *) self->derived_state_offset,
            self->num_rows + 1, KAS_UINT32},
        {"mutations/metadata", (void *) self->metadata,
            self->metadata_length, KAS_UINT8},
        {"mutations/metadata_offset", (void *) self->metadata_offset,
            self->num_rows + 1, KAS_UINT32},
    };
    return write_table_cols(store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
}


static int
mutation_table_load(mutation_table_t *self, kastore_t *store)
{
    read_table_col_t read_cols[] = {
        {"mutations/site", (void **) &self->site, &self->num_rows, 0, KAS_INT32},
        {"mutations/node", (void **) &self->node, &self->num_rows, 0, KAS_INT32},
        {"mutations/parent", (void **) &self->parent, &self->num_rows, 0, KAS_INT32},
        {"mutations/derived_state", (void **) &self->derived_state,
            &self->derived_state_length, 0, KAS_UINT8},
        {"mutations/derived_state_offset", (void **) &self->derived_state_offset,
            &self->num_rows, 1, KAS_UINT32},
        {"mutations/metadata", (void **) &self->metadata,
            &self->metadata_length, 0, KAS_UINT8},
        {"mutations/metadata_offset", (void **) &self->metadata_offset,
            &self->num_rows, 1, KAS_UINT32},
    };
    return read_table_cols(store, read_cols, sizeof(read_cols) / sizeof(*read_cols));
}

/*************************
 * migration table
 *************************/

static int
migration_table_expand(migration_table_t *self, size_t additional_rows)
{
    int ret = 0;
    table_size_t increment = MSP_MAX(
            (table_size_t) additional_rows, self->max_rows_increment);
    table_size_t new_size = self->max_rows + increment;

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
    self->max_rows_increment = (table_size_t) max_rows_increment;
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
    self->num_rows += (table_size_t) num_rows;
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
    return migration_table_truncate(self, 0);
}

int
migration_table_truncate(migration_table_t *self, size_t num_rows)
{
    int ret = 0;
    table_size_t n = (table_size_t) num_rows;

    if (n > self->num_rows) {
        ret = MSP_ERR_BAD_TABLE_POSITION;
        goto out;
    }
    self->num_rows = n;
out:
    return ret;
}

int
migration_table_free(migration_table_t *self)
{
    if (self->max_rows > 0) {
        msp_safe_free(self->left);
        msp_safe_free(self->right);
        msp_safe_free(self->node);
        msp_safe_free(self->source);
        msp_safe_free(self->dest);
        msp_safe_free(self->time);
    }
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
migration_table_get_row(migration_table_t *self, size_t index, migration_t *row)
{
    int ret = 0;
    if (index >= self->num_rows) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    row->id = (migration_id_t) index;
    row->left = self->left[index];
    row->right = self->right[index];
    row->node = self->node[index];
    row->source = self->source[index];
    row->dest = self->dest[index];
    row->time = self->time[index];
out:
    return ret;
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

bool
migration_table_equals(migration_table_t *self, migration_table_t *other)
{
    bool ret = false;
    if (self->num_rows == other->num_rows) {
        ret = memcmp(self->left, other->left,
                self->num_rows * sizeof(double)) == 0
            && memcmp(self->right, other->right,
                    self->num_rows * sizeof(double)) == 0
            && memcmp(self->node, other->node,
                    self->num_rows * sizeof(node_id_t)) == 0
            && memcmp(self->source, other->source,
                    self->num_rows * sizeof(population_id_t)) == 0
            && memcmp(self->dest, other->dest,
                    self->num_rows * sizeof(population_id_t)) == 0
            && memcmp(self->time, other->time,
                    self->num_rows * sizeof(double)) == 0;
    }
    return ret;
}

static int
migration_table_dump(migration_table_t *self, kastore_t *store)
{
    write_table_col_t write_cols[] = {
        {"migrations/left", (void *) self->left, self->num_rows,  KAS_FLOAT64},
        {"migrations/right", (void *) self->right, self->num_rows,  KAS_FLOAT64},
        {"migrations/node", (void *) self->node, self->num_rows,  KAS_INT32},
        {"migrations/source", (void *) self->source, self->num_rows,  KAS_INT32},
        {"migrations/dest", (void *) self->dest, self->num_rows,  KAS_INT32},
        {"migrations/time", (void *) self->time, self->num_rows,  KAS_FLOAT64},
    };
    return write_table_cols(store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
}

static int
migration_table_load(migration_table_t *self, kastore_t *store)
{
    read_table_col_t read_cols[] = {
        {"migrations/left", (void **) &self->left, &self->num_rows, 0, KAS_FLOAT64},
        {"migrations/right", (void **) &self->right, &self->num_rows, 0, KAS_FLOAT64},
        {"migrations/node", (void **) &self->node, &self->num_rows, 0, KAS_INT32},
        {"migrations/source", (void **) &self->source, &self->num_rows, 0, KAS_INT32},
        {"migrations/dest", (void **) &self->dest, &self->num_rows, 0, KAS_INT32},
        {"migrations/time", (void **) &self->time, &self->num_rows, 0, KAS_FLOAT64},
    };
    return read_table_cols(store, read_cols, sizeof(read_cols) / sizeof(*read_cols));
}

/*************************
 * individual table
 *************************/

static int
individual_table_expand_main_columns(individual_table_t *self, table_size_t additional_rows)
{
    int ret = 0;
    table_size_t increment = MSP_MAX(additional_rows, self->max_rows_increment);
    table_size_t new_size = self->max_rows + increment;

    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column((void **) &self->flags, new_size, sizeof(uint32_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->location_offset, new_size + 1,
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
individual_table_expand_location(individual_table_t *self, table_size_t additional_length)
{
    int ret = 0;
    table_size_t increment = MSP_MAX(additional_length,
            self->max_location_length_increment);
    table_size_t new_size = self->max_location_length + increment;

    if ((self->location_length + additional_length) > self->max_location_length) {
        ret = expand_column((void **) &self->location, new_size, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        self->max_location_length = new_size;
    }
out:
    return ret;
}

static int
individual_table_expand_metadata(individual_table_t *self, table_size_t additional_length)
{
    int ret = 0;
    table_size_t increment = MSP_MAX(additional_length,
            self->max_metadata_length_increment);
    table_size_t new_size = self->max_metadata_length + increment;

    if ((self->metadata_length + additional_length) > self->max_metadata_length) {
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
individual_table_alloc(individual_table_t *self, size_t max_rows_increment,
        size_t max_location_length_increment, size_t max_metadata_length_increment)
{
    int ret = 0;

    memset(self, 0, sizeof(individual_table_t));
    if (max_rows_increment == 0) {
       max_rows_increment = DEFAULT_SIZE_INCREMENT;
    }
    if (max_location_length_increment == 0) {
        max_location_length_increment = DEFAULT_SIZE_INCREMENT;
    }
    if (max_metadata_length_increment == 0) {
        max_metadata_length_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_rows_increment = (table_size_t) max_rows_increment;
    self->max_location_length_increment = (table_size_t) max_location_length_increment;
    self->max_metadata_length_increment = (table_size_t) max_metadata_length_increment;
    self->max_rows = 0;
    self->num_rows = 0;
    self->max_location_length = 0;
    self->location_length = 0;
    self->max_metadata_length = 0;
    self->metadata_length = 0;
    ret = individual_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = individual_table_expand_location(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->location_offset[0] = 0;
    ret = individual_table_expand_metadata(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->metadata_offset[0] = 0;
out:
    return ret;
}

int WARN_UNUSED
individual_table_copy(individual_table_t *self, individual_table_t *dest)
{
    return individual_table_set_columns(dest, self->num_rows, self->flags,
            self->location, self->location_offset, self->metadata, self->metadata_offset);
}

int WARN_UNUSED
individual_table_set_columns(individual_table_t *self, size_t num_rows, uint32_t *flags,
        double *location, uint32_t *location_offset,
        const char *metadata, uint32_t *metadata_offset)
{
    int ret;

    ret = individual_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = individual_table_append_columns(self, num_rows, flags, location, location_offset,
            metadata, metadata_offset);
out:
    return ret;
}

int
individual_table_append_columns(individual_table_t *self, size_t num_rows, uint32_t *flags,
        double *location, uint32_t *location_offset, const char *metadata, uint32_t *metadata_offset)
{
    int ret;
    table_size_t j, metadata_length, location_length;

    if (flags == NULL) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    if ((location == NULL) != (location_offset == NULL)) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    if ((metadata == NULL) != (metadata_offset == NULL)) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    ret = individual_table_expand_main_columns(self, (table_size_t) num_rows);
    if (ret != 0) {
        goto out;
    }
    memcpy(self->flags + self->num_rows, flags, num_rows * sizeof(uint32_t));
    if (location == NULL) {
        for (j = 0; j < num_rows; j++) {
            self->location_offset[self->num_rows + j + 1] = (table_size_t) self->location_length;
        }
    } else {
        ret = check_offsets(num_rows, location_offset, 0, false);
        if (ret != 0) {
            goto out;
        }
        for (j = 0; j < num_rows; j++) {
            self->location_offset[self->num_rows + j] =
                (table_size_t) self->location_length + location_offset[j];
        }
        location_length = location_offset[num_rows];
        ret = individual_table_expand_location(self, location_length);
        if (ret != 0) {
            goto out;
        }
        memcpy(self->location + self->location_length, location, location_length * sizeof(double));
        self->location_length += location_length;
    }
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
        ret = individual_table_expand_metadata(self, metadata_length);
        if (ret != 0) {
            goto out;
        }
        memcpy(self->metadata + self->metadata_length, metadata, metadata_length * sizeof(char));
        self->metadata_length += metadata_length;
    }
    self->num_rows += (table_size_t) num_rows;
    self->location_offset[self->num_rows] = self->location_length;
    self->metadata_offset[self->num_rows] = self->metadata_length;
out:
    return ret;
}

static individual_id_t
individual_table_add_row_internal(individual_table_t *self, uint32_t flags, double *location,
        table_size_t location_length, const char *metadata, table_size_t metadata_length)
{
    assert(self->num_rows < self->max_rows);
    assert(self->metadata_length + metadata_length <= self->max_metadata_length);
    assert(self->location_length + location_length <= self->max_location_length);
    self->flags[self->num_rows] = flags;
    memcpy(self->location + self->location_length, location, location_length * sizeof(double));
    self->location_offset[self->num_rows + 1] = self->location_length + location_length;
    self->location_length += location_length;
    memcpy(self->metadata + self->metadata_length, metadata, metadata_length * sizeof(char));
    self->metadata_offset[self->num_rows + 1] = self->metadata_length + metadata_length;
    self->metadata_length += metadata_length;
    self->num_rows++;
    return (individual_id_t) self->num_rows - 1;
}

individual_id_t
individual_table_add_row(individual_table_t *self, uint32_t flags, double *location,
        size_t location_length, const char *metadata, size_t metadata_length)
{
    int ret = 0;

    ret = individual_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = individual_table_expand_location(self, (table_size_t) location_length);
    if (ret != 0) {
        goto out;
    }
    ret = individual_table_expand_metadata(self, (table_size_t) metadata_length);
    if (ret != 0) {
        goto out;
    }
    ret = individual_table_add_row_internal(self, flags, location,
            (table_size_t) location_length, metadata, (table_size_t) metadata_length);
out:
    return ret;
}

int
individual_table_clear(individual_table_t *self)
{
    return individual_table_truncate(self, 0);
}

int
individual_table_truncate(individual_table_t *self, size_t num_rows)
{
    int ret = 0;
    table_size_t n = (table_size_t) num_rows;

    if (n > self->num_rows) {
        ret = MSP_ERR_BAD_TABLE_POSITION;
        goto out;
    }
    self->num_rows = n;
    self->location_length = self->location_offset[n];
    self->metadata_length = self->metadata_offset[n];
out:
    return ret;
}

int
individual_table_free(individual_table_t *self)
{
    if (self->max_rows > 0) {
        msp_safe_free(self->flags);
        msp_safe_free(self->location);
        msp_safe_free(self->location_offset);
        msp_safe_free(self->metadata);
        msp_safe_free(self->metadata_offset);
    }
    return 0;
}

void
individual_table_print_state(individual_table_t *self, FILE *out)
{
    size_t j, k;

    fprintf(out, TABLE_SEP);
    fprintf(out, "individual_table: %p:\n", (void *) self);
    fprintf(out, "num_rows          = %d\tmax= %d\tincrement = %d)\n",
            (int) self->num_rows, (int) self->max_rows, (int) self->max_rows_increment);
    fprintf(out, "metadata_length = %d\tmax= %d\tincrement = %d)\n",
            (int) self->metadata_length,
            (int) self->max_metadata_length,
            (int) self->max_metadata_length_increment);
    fprintf(out, TABLE_SEP);
    /* We duplicate the dump_text code here because we want to output
     * the offset columns. */
    fprintf(out, "id\tflags\tlocation_offset\tlocation\t");
    fprintf(out, "metadata_offset\tmetadata\n");
    for (j = 0; j < self->num_rows; j++) {
        fprintf(out, "%d\t%d\t", (int) j, self->flags[j]);
        fprintf(out, "%d\t", self->location_offset[j]);
        for (k = self->location_offset[j]; k < self->location_offset[j + 1]; k++) {
            fprintf(out, "%f", self->location[k]);
            if (k + 1 < self->location_offset[j + 1]) {
                fprintf(out, ",");
            }
        }
        fprintf(out, "\t");
        fprintf(out, "%d\t", self->metadata_offset[j]);
        for (k = self->metadata_offset[j]; k < self->metadata_offset[j + 1]; k++) {
            fprintf(out, "%c", self->metadata[k]);
        }
        fprintf(out, "\n");
    }
}

int
individual_table_get_row(individual_table_t *self, size_t index, individual_t *row)
{
    int ret = 0;

    if (index >= self->num_rows) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    row->id = (individual_id_t) index;
    row->flags = self->flags[index];
    row->location_length = self->location_offset[index + 1]
        - self->location_offset[index];
    row->location = self->location + self->location_offset[index];
    row->metadata_length = self->metadata_offset[index + 1]
        - self->metadata_offset[index];
    row->metadata = self->metadata + self->metadata_offset[index];
    /* Also have referencing individuals here. Should this be a different struct?
     * See also site. */
    row->nodes_length = 0;
    row->nodes = NULL;
out:
    return ret;
}

int
individual_table_dump_text(individual_table_t *self, FILE *out)
{
    int ret = MSP_ERR_IO;
    size_t j, k;
    table_size_t metadata_len;
    int err;

    err = fprintf(out, "id\tflags\tlocation\tmetadata\n");
    if (err < 0) {
        goto out;
    }
    for (j = 0; j < self->num_rows; j++) {
        metadata_len = self->metadata_offset[j + 1] - self->metadata_offset[j];
        err = fprintf(out, "%d\t%d\t", (int) j, (int) self->flags[j]);
        if (err < 0) {
            goto out;
        }
        for (k = self->location_offset[j]; k < self->location_offset[j + 1]; k++) {
            err = fprintf(out, "%.*g", MSP_DBL_DECIMAL_DIG, self->location[k]);
            if (err < 0) {
                goto out;
            }
            if (k + 1 < self->location_offset[j + 1]) {
                err = fprintf(out, ",");
                if (err < 0) {
                    goto out;
                }
            }
        }
        err = fprintf(out, "\t%.*s\n",
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
individual_table_equals(individual_table_t *self, individual_table_t *other)
{
    bool ret = false;
    if (self->num_rows == other->num_rows
            && self->metadata_length == other->metadata_length) {
        ret = memcmp(self->flags, other->flags,
                    self->num_rows * sizeof(uint32_t)) == 0
            && memcmp(self->location_offset, other->location_offset,
                    (self->num_rows + 1) * sizeof(table_size_t)) == 0
            && memcmp(self->location, other->location,
                    self->location_length * sizeof(double)) == 0
            && memcmp(self->metadata_offset, other->metadata_offset,
                    (self->num_rows + 1) * sizeof(table_size_t)) == 0
            && memcmp(self->metadata, other->metadata,
                    self->metadata_length * sizeof(char)) == 0;
    }
    return ret;
}

static int
individual_table_dump(individual_table_t *self, kastore_t *store)
{
    write_table_col_t write_cols[] = {
        {"individuals/flags", (void *) self->flags, self->num_rows, KAS_UINT32},
        {"individuals/location", (void *) self->location, self->location_length, KAS_FLOAT64},
        {"individuals/location_offset", (void *) self->location_offset, self->num_rows + 1,
            KAS_UINT32},
        {"individuals/metadata", (void *) self->metadata, self->metadata_length, KAS_UINT8},
        {"individuals/metadata_offset", (void *) self->metadata_offset, self->num_rows + 1,
            KAS_UINT32},
    };
    return write_table_cols(store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
}

static int
individual_table_load(individual_table_t *self, kastore_t *store)
{
    read_table_col_t read_cols[] = {
        {"individuals/flags", (void **) &self->flags, &self->num_rows, 0, KAS_UINT32},
        {"individuals/location", (void **) &self->location, &self->location_length, 0,
            KAS_FLOAT64},
        {"individuals/location_offset", (void **) &self->location_offset, &self->num_rows,
            1, KAS_UINT32},
        {"individuals/metadata", (void **) &self->metadata, &self->metadata_length, 0,
            KAS_UINT8},
        {"individuals/metadata_offset", (void **) &self->metadata_offset, &self->num_rows,
            1, KAS_UINT32},
    };
    return read_table_cols(store, read_cols, sizeof(read_cols) / sizeof(*read_cols));
}


/*************************
 * population table
 *************************/

static int
population_table_expand_main_columns(population_table_t *self, table_size_t additional_rows)
{
    int ret = 0;
    table_size_t increment = MSP_MAX(additional_rows, self->max_rows_increment);
    table_size_t new_size = self->max_rows + increment;

    if ((self->num_rows + additional_rows) > self->max_rows) {
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
population_table_expand_metadata(population_table_t *self, table_size_t additional_length)
{
    int ret = 0;
    table_size_t increment = MSP_MAX(additional_length,
            self->max_metadata_length_increment);
    table_size_t new_size = self->max_metadata_length + increment;

    if ((self->metadata_length + additional_length) > self->max_metadata_length) {
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
population_table_alloc(population_table_t *self, size_t max_rows_increment,
        size_t max_metadata_length_increment)
{
    int ret = 0;

    memset(self, 0, sizeof(population_table_t));
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
    ret = population_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = population_table_expand_metadata(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->metadata_offset[0] = 0;
out:
    return ret;
}

int WARN_UNUSED
population_table_copy(population_table_t *self, population_table_t *dest)
{
    return population_table_set_columns(dest, self->num_rows,
            self->metadata, self->metadata_offset);
}

int
population_table_set_columns(population_table_t *self, size_t num_rows,
        const char *metadata, uint32_t *metadata_offset)
{
    int ret;

    ret = population_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = population_table_append_columns(self, num_rows, metadata, metadata_offset);
out:
    return ret;
}

int
population_table_append_columns(population_table_t *self, size_t num_rows,
        const char *metadata, uint32_t *metadata_offset)
{
    int ret;
    table_size_t j, metadata_length;

    if (metadata == NULL || metadata_offset == NULL) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    ret = population_table_expand_main_columns(self, (table_size_t) num_rows);
    if (ret != 0) {
        goto out;
    }

    ret = check_offsets(num_rows, metadata_offset, 0, false);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < num_rows; j++) {
        self->metadata_offset[self->num_rows + j] =
            (table_size_t) self->metadata_length + metadata_offset[j];
    }
    metadata_length = metadata_offset[num_rows];
    ret = population_table_expand_metadata(self, metadata_length);
    if (ret != 0) {
        goto out;
    }
    memcpy(self->metadata + self->metadata_length, metadata,
            metadata_length * sizeof(char));
    self->metadata_length += metadata_length;

    self->num_rows += (table_size_t) num_rows;
    self->metadata_offset[self->num_rows] = self->metadata_length;
out:
    return ret;
}

static population_id_t
population_table_add_row_internal(population_table_t *self,
        const char *metadata, table_size_t metadata_length)
{
    int ret = 0;

    assert(self->num_rows < self->max_rows);
    assert(self->metadata_length + metadata_length <= self->max_metadata_length);
    memcpy(self->metadata + self->metadata_length, metadata, metadata_length);
    self->metadata_offset[self->num_rows + 1] = self->metadata_length + metadata_length;
    self->metadata_length += metadata_length;
    ret = (population_id_t) self->num_rows;
    self->num_rows++;
    return ret;
}

population_id_t
population_table_add_row(population_table_t *self,
        const char *metadata, size_t metadata_length)
{
    int ret = 0;

    ret = population_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = population_table_expand_metadata(self, (table_size_t) metadata_length);
    if (ret != 0) {
        goto out;
    }
    ret = population_table_add_row_internal(self,
            metadata, (table_size_t) metadata_length);
out:
    return ret;
}

int
population_table_clear(population_table_t *self)
{
    return population_table_truncate(self, 0);
}

int
population_table_truncate(population_table_t *self, size_t num_rows)
{
    int ret = 0;
    table_size_t n = (table_size_t) num_rows;

    if (n > self->num_rows) {
        ret = MSP_ERR_BAD_TABLE_POSITION;
        goto out;
    }
    self->num_rows = n;
    self->metadata_length = self->metadata_offset[n];
out:
    return ret;
}

int
population_table_free(population_table_t *self)
{
    if (self->max_rows > 0) {
        msp_safe_free(self->metadata);
        msp_safe_free(self->metadata_offset);
    }
    return 0;
}

void
population_table_print_state(population_table_t *self, FILE *out)
{
    size_t j, k;

    fprintf(out, TABLE_SEP);
    fprintf(out, "population_table: %p:\n", (void *) self);
    fprintf(out, "num_rows          = %d\tmax= %d\tincrement = %d)\n",
            (int) self->num_rows, (int) self->max_rows, (int) self->max_rows_increment);
    fprintf(out, "metadata_length  = %d\tmax= %d\tincrement = %d)\n",
            (int) self->metadata_length,
            (int) self->max_metadata_length,
            (int) self->max_metadata_length_increment);
    fprintf(out, TABLE_SEP);
    fprintf(out, "index\tmetadata_offset\tmetadata\n");
    for (j = 0; j < self->num_rows; j++) {
        fprintf(out, "%d\t%d\t", (int) j, self->metadata_offset[j]);
        for (k = self->metadata_offset[j]; k < self->metadata_offset[j + 1]; k++) {
            fprintf(out, "%c", self->metadata[k]);
        }
        fprintf(out, "\n");
    }
    assert(self->metadata_offset[0] == 0);
    assert(self->metadata_offset[self->num_rows] == self->metadata_length);
}

int
population_table_get_row(population_table_t *self, size_t index, tmp_population_t *row)
{
    int ret = 0;
    if (index >= self->num_rows) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    row->id = (population_id_t) index;
    row->metadata_length = self->metadata_offset[index + 1]
        - self->metadata_offset[index];
    row->metadata = self->metadata + self->metadata_offset[index];
out:
    return ret;
}

int
population_table_dump_text(population_table_t *self, FILE *out)
{
    int ret = MSP_ERR_IO;
    int err;
    size_t j;
    table_size_t metadata_len;

    err = fprintf(out, "metadata\n");
    if (err < 0) {
        goto out;
    }
    for (j = 0; j < self->num_rows; j++) {
        metadata_len = self->metadata_offset[j + 1] - self->metadata_offset[j];
        err = fprintf(out, "%.*s\n", metadata_len,
                self->metadata + self->metadata_offset[j]);
        if (err < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

bool
population_table_equals(population_table_t *self, population_table_t *other)
{
    bool ret = false;
    if (self->num_rows == other->num_rows
            && self->metadata_length == other->metadata_length) {
        ret = memcmp(self->metadata_offset, other->metadata_offset,
                    (self->num_rows + 1) * sizeof(table_size_t)) == 0
            && memcmp(self->metadata, other->metadata,
                    self->metadata_length * sizeof(char)) == 0;
    }
    return ret;
}

static int
population_table_dump(population_table_t *self, kastore_t *store)
{
    write_table_col_t write_cols[] = {
        {"populations/metadata", (void *) self->metadata,
            self->metadata_length, KAS_UINT8},
        {"populations/metadata_offset", (void *) self->metadata_offset,
            self->num_rows+ 1, KAS_UINT32},
    };
    return write_table_cols(store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
}

static int
population_table_load(population_table_t *self, kastore_t *store)
{
    read_table_col_t read_cols[] = {
        {"populations/metadata", (void **) &self->metadata,
            &self->metadata_length, 0, KAS_UINT8},
        {"populations/metadata_offset", (void **) &self->metadata_offset,
            &self->num_rows, 1, KAS_UINT32},
    };
    return read_table_cols(store, read_cols, sizeof(read_cols) / sizeof(*read_cols));
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
        ret = expand_column((void **) &self->timestamp, new_size, sizeof(char));
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
        ret = expand_column((void **) &self->record, new_size, sizeof(char));
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
    assert(self->timestamp_length + timestamp_length <= self->max_timestamp_length);
    memcpy(self->timestamp + self->timestamp_length, timestamp, timestamp_length);
    self->timestamp_offset[self->num_rows + 1] = self->timestamp_length + timestamp_length;
    self->timestamp_length += timestamp_length;
    assert(self->record_length + record_length <= self->max_record_length);
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
    return provenance_table_truncate(self, 0);
}

int
provenance_table_truncate(provenance_table_t *self, size_t num_rows)
{
    int ret = 0;
    table_size_t n = (table_size_t) num_rows;

    if (n > self->num_rows) {
        ret = MSP_ERR_BAD_TABLE_POSITION;
        goto out;
    }
    self->num_rows = n;
    self->timestamp_length = self->timestamp_offset[n];
    self->record_length = self->record_offset[n];
out:
    return ret;
}

int
provenance_table_free(provenance_table_t *self)
{
    if (self->max_rows > 0) {
        msp_safe_free(self->timestamp);
        msp_safe_free(self->timestamp_offset);
        msp_safe_free(self->record);
        msp_safe_free(self->record_offset);
    }
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

int
provenance_table_get_row(provenance_table_t *self, size_t index, provenance_t *row)
{
    int ret = 0;
    if (index >= self->num_rows) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    row->id = (provenance_id_t) index;
    row->timestamp_length = self->timestamp_offset[index + 1]
        - self->timestamp_offset[index];
    row->timestamp = self->timestamp + self->timestamp_offset[index];
    row->record_length = self->record_offset[index + 1]
        - self->record_offset[index];
    row->record = self->record + self->record_offset[index];
out:
    return ret;
}

int
provenance_table_dump_text(provenance_table_t *self, FILE *out)
{
    int ret = MSP_ERR_IO;
    int err;
    size_t j;
    table_size_t timestamp_len, record_len;

    err = fprintf(out, "record\ttimestamp\n");
    if (err < 0) {
        goto out;
    }
    for (j = 0; j < self->num_rows; j++) {
        record_len = self->record_offset[j + 1] -
            self->record_offset[j];
        timestamp_len = self->timestamp_offset[j + 1] - self->timestamp_offset[j];
        err = fprintf(out, "%.*s\t%.*s\n", record_len, self->record + self->record_offset[j],
                timestamp_len, self->timestamp + self->timestamp_offset[j]);
        if (err < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

bool
provenance_table_equals(provenance_table_t *self, provenance_table_t *other)
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

static int
provenance_table_dump(provenance_table_t *self, kastore_t *store)
{
    write_table_col_t write_cols[] = {
        {"provenances/timestamp", (void *) self->timestamp,
            self->timestamp_length, KAS_UINT8},
        {"provenances/timestamp_offset", (void *) self->timestamp_offset,
            self->num_rows+ 1, KAS_UINT32},
        {"provenances/record", (void *) self->record,
            self->record_length, KAS_UINT8},
        {"provenances/record_offset", (void *) self->record_offset,
            self->num_rows + 1, KAS_UINT32},
    };
    return write_table_cols(store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
}

static int
provenance_table_load(provenance_table_t *self, kastore_t *store)
{
    read_table_col_t read_cols[] = {
        {"provenances/timestamp", (void **) &self->timestamp,
            &self->timestamp_length, 0, KAS_UINT8},
        {"provenances/timestamp_offset", (void **) &self->timestamp_offset,
            &self->num_rows, 1, KAS_UINT32},
        {"provenances/record", (void **) &self->record,
            &self->record_length, 0, KAS_UINT8},
        {"provenances/record_offset", (void **) &self->record_offset,
            &self->num_rows, 1, KAS_UINT32},
    };
    return read_table_cols(store, read_cols, sizeof(read_cols) / sizeof(*read_cols));
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
    int ret = (ia->position > ib->position) - (ia->position < ib->position);
    if (ret == 0) {
        /* Within a particular position sort by ID.  This ensures that relative ordering
         * of multiple sites at the same position is maintained; the redundant sites
         * will get compacted down by clean_tables(), but in the meantime if the order
         * of the redundant sites changes it will cause the sort order of mutations to
         * be corrupted, as the mutations will follow their sites. */
        ret = (ia->id > ib->id) - (ia->id < ib->id);
    }
    return ret;
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
table_sorter_alloc(table_sorter_t *self, table_collection_t *tables,
        int MSP_UNUSED(flags))
{
    int ret = 0;

    memset(self, 0, sizeof(table_sorter_t));
    if (tables == NULL) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    ret = table_collection_check_integrity(tables, MSP_CHECK_OFFSETS);
    if (ret != 0) {
        goto out;
    }
    self->nodes = tables->nodes;
    self->edges = tables->edges;
    self->mutations = tables->mutations;
    self->sites = tables->sites;
    self->migrations = tables->migrations;

    self->site_id_map = malloc(self->sites->num_rows * sizeof(site_id_t));
    if (self->site_id_map == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
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
    site_table_t copy;
    table_size_t j;
    table_size_t num_sites = self->sites->num_rows;
    site_t *sorted_sites = malloc(num_sites * sizeof(*sorted_sites));

    ret = site_table_alloc(&copy, num_sites, self->sites->ancestral_state_length,
            self->sites->metadata_length);
    if (ret != 0) {
        goto out;
    }
    ret = site_table_copy(self->sites, &copy);
    if (ret != 0) {
        goto out;
    }
    if (sorted_sites == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    for (j = 0; j < num_sites; j++) {
        ret = site_table_get_row(&copy, j, sorted_sites + j);
        if (ret != 0) {
            goto out;
        }
    }

    /* Sort the sites by position */
    qsort(sorted_sites, self->sites->num_rows, sizeof(*sorted_sites), cmp_site);

    /* Build the mapping from old site IDs to new site IDs and copy back into the table */
    site_table_clear(self->sites);
    for (j = 0; j < num_sites; j++) {
        self->site_id_map[sorted_sites[j].id] = (site_id_t) j;
        ret = site_table_add_row(self->sites, sorted_sites[j].position,
                sorted_sites[j].ancestral_state, sorted_sites[j].ancestral_state_length,
                sorted_sites[j].metadata, sorted_sites[j].metadata_length);
        if (ret < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    msp_safe_free(sorted_sites);
    site_table_free(&copy);
    return ret;
}

static int
table_sorter_sort_mutations(table_sorter_t *self)
{
    int ret = 0;
    size_t j;
    mutation_id_t parent, mapped_parent;
    size_t num_mutations = self->mutations->num_rows;
    mutation_table_t copy;
    mutation_t *sorted_mutations = malloc(num_mutations * sizeof(*sorted_mutations));
    mutation_id_t *mutation_id_map = malloc(num_mutations * sizeof(*mutation_id_map));

    ret = mutation_table_alloc(&copy, num_mutations,
            self->mutations->derived_state_length,
            self->mutations->metadata_length);
    if (ret != 0) {
        goto out;
    }
    ret = mutation_table_copy(self->mutations, &copy);
    if (ret != 0) {
        goto out;
    }
    if (mutation_id_map == NULL || sorted_mutations == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }

    for (j = 0; j < num_mutations; j++) {
        ret = mutation_table_get_row(&copy, j, sorted_mutations + j);
        if (ret != 0) {
            goto out;
        }
        sorted_mutations[j].site = self->site_id_map[sorted_mutations[j].site];
    }
    ret = mutation_table_clear(self->mutations);
    if (ret != 0) {
        goto out;
    }

    qsort(sorted_mutations, num_mutations, sizeof(*sorted_mutations), cmp_mutation);

    /* Make a first pass through the sorted mutations to build the ID map. */
    for (j = 0; j < num_mutations; j++) {
        mutation_id_map[sorted_mutations[j].id] = (mutation_id_t) j;
    }

    for (j = 0; j < num_mutations; j++) {
        mapped_parent = MSP_NULL_MUTATION;
        parent = sorted_mutations[j].parent;
        if (parent != MSP_NULL_MUTATION) {
            mapped_parent = mutation_id_map[parent];
        }
        ret = mutation_table_add_row(self->mutations,
            sorted_mutations[j].site,
            sorted_mutations[j].node,
            mapped_parent,
            sorted_mutations[j].derived_state,
            sorted_mutations[j].derived_state_length,
            sorted_mutations[j].metadata,
            sorted_mutations[j].metadata_length);
        if (ret < 0) {
            goto out;
        }
    }
    ret = 0;

out:
    msp_safe_free(mutation_id_map);
    msp_safe_free(sorted_mutations);
    mutation_table_free(&copy);
    return ret;
}

static int
table_sorter_run(table_sorter_t *self, size_t edge_start)
{
    int ret = 0;

    ret = table_sorter_sort_edges(self, edge_start);
    if (ret != 0) {
        goto out;
    }
    ret = table_sorter_sort_sites(self);
    if (ret != 0) {
        goto out;
    }
    ret = table_sorter_sort_mutations(self);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

static void
table_sorter_free(table_sorter_t *self)
{
    msp_safe_free(self->site_id_map);
}


/*************************
 * segment overlapper
 *************************/

static int
cmp_segment(const void *a, const void *b) {
    const simplify_segment_t *ia = (const simplify_segment_t *) a;
    const simplify_segment_t *ib = (const simplify_segment_t *) b;
    int ret = (ia->left > ib->left) - (ia->left < ib->left);
    /* Break ties using the node */
    if (ret == 0)  {
        ret = (ia->node > ib->node) - (ia->node < ib->node);
    }
    return ret;
}

int WARN_UNUSED
segment_overlapper_alloc(segment_overlapper_t *self)
{
    int ret = 0;

    memset(self, 0, sizeof(*self));
    self->max_overlapping = 8; /* Making sure we call realloc in tests */
    self->overlapping = malloc(self->max_overlapping * sizeof(*self->overlapping));
    if (self->overlapping == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
out:
    return ret;
}

int
segment_overlapper_free(segment_overlapper_t *self)
{
    msp_safe_free(self->overlapping);
    return 0;
}

/* Initialise the segment overlapper for use. Note that the segments
 * array must have space for num_segments + 1 elements!
 */
int WARN_UNUSED
segment_overlapper_init(segment_overlapper_t *self, simplify_segment_t *segments,
        size_t num_segments)
{
    int ret = 0;
    simplify_segment_t *sentinel;
    void *p;

    if (self->max_overlapping < num_segments) {
        self->max_overlapping = num_segments;
        p = realloc(self->overlapping,
                self->max_overlapping * sizeof(*self->overlapping));
        if (p == NULL) {
            ret = MSP_ERR_NO_MEMORY;
            goto out;
        }
        self->overlapping = p;

    }
    self->segments = segments;
    self->num_segments = num_segments;
    self->index = 0;
    self->num_overlapping = 0;
    self->left = 0;
    self->right = DBL_MAX;

    /* Sort the segments in the buffer by left coordinate */
    qsort(self->segments, self->num_segments, sizeof(simplify_segment_t), cmp_segment);
    /* NOTE! We are assuming that there's space for another element on the end
     * here. This is to insert a sentinel which simplifies the logic. */
    sentinel = self->segments + self->num_segments;
    sentinel->left = DBL_MAX;
out:
    return ret;
}

int WARN_UNUSED
segment_overlapper_next(segment_overlapper_t *self,
        double *left, double *right, simplify_segment_t ***overlapping,
        size_t *num_overlapping)
{
    int ret = 0;
    size_t j, k;
    size_t n = self->num_segments;
    simplify_segment_t *S = self->segments;

    if (self->index < n) {
        self->left = self->right;
        /* Remove any elements of X with right <= left */
        k = 0;
        for (j = 0; j < self->num_overlapping; j++) {
            if (self->overlapping[j]->right > self->left) {
                self->overlapping[k] = self->overlapping[j];
                k++;
            }
        }
        self->num_overlapping = k;
        if (k == 0) {
            self->left = S[self->index].left;
        }
        while (self->index < n && S[self->index].left == self->left) {
            assert(self->num_overlapping < self->max_overlapping);
            self->overlapping[self->num_overlapping] = &S[self->index];
            self->num_overlapping++;
            self->index++;
        }
        self->index--;
        self->right = S[self->index + 1].left;
        for (j = 0; j < self->num_overlapping; j++) {
            self->right = MSP_MIN(self->right, self->overlapping[j]->right);
        }
        assert(self->left < self->right);
        self->index++;
        ret = 1;
    } else {
        self->left = self->right;
        self->right = DBL_MAX;
        k = 0;
        for (j = 0; j < self->num_overlapping; j++) {
            if (self->overlapping[j]->right > self->left) {
                self->right = MSP_MIN(self->right, self->overlapping[j]->right);
                self->overlapping[k] = self->overlapping[j];
                k++;
            }
        }
        self->num_overlapping = k;
        if (k > 0) {
            ret = 1;
        }
    }

    *left = self->left;
    *right = self->right;
    *overlapping = self->overlapping;
    *num_overlapping = self->num_overlapping;
    return ret;
}

/*************************
 * simplifier
 *************************/

static int
cmp_node_id(const void *a, const void *b) {
    const node_id_t *ia = (const node_id_t *) a;
    const node_id_t *ib = (const node_id_t *) b;
    return (*ia > *ib) - (*ia < *ib);
}

static void
simplifier_check_state(simplifier_t *self)
{
    size_t j, k;
    simplify_segment_t *u;
    mutation_id_list_t *list_node;
    site_id_t site;
    interval_list_t *int_list;
    node_id_t child;
    double position, last_position;
    bool found;
    size_t num_intervals;

    for (j = 0; j < self->input_tables.nodes->num_rows; j++) {
        assert((self->ancestor_map_head[j] == NULL) ==
                (self->ancestor_map_tail[j] == NULL));
        for (u = self->ancestor_map_head[j]; u != NULL; u = u->next) {
            assert(u->left < u->right);
            if (u->next != NULL) {
                assert(u->right <= u->next->left);
                if (u->right == u->next->left) {
                    assert(u->node != u->next->node);
                }
            } else {
                assert(u == self->ancestor_map_tail[j]);
            }
        }
    }

    for (j = 0; j < self->segment_queue_size; j++) {
        assert(self->segment_queue[j].left < self->segment_queue[j].right);
    }

    for (j = 0; j < self->input_tables.nodes->num_rows; j++) {
        last_position = -1;
        for (list_node = self->node_mutation_list_map_head[j]; list_node != NULL;
                list_node = list_node->next) {
            assert(self->input_tables.mutations->node[list_node->mutation] == (node_id_t) j);
            site = self->input_tables.mutations->site[list_node->mutation];
            position = self->input_tables.sites->position[site];
            assert(last_position <= position);
            last_position = position;
        }
    }

    /* check the buffered edges */
    for (j = 0; j < self->input_tables.nodes->num_rows; j++) {
        assert((self->child_edge_map_head[j] == NULL) ==
            (self->child_edge_map_tail[j] == NULL));
        if (self->child_edge_map_head[j] != NULL) {
            /* Make sure that the child is in our list */
            found = false;
            for (k = 0; k < self->num_buffered_children; k++) {
                if (self->buffered_children[k] == (node_id_t) j) {
                    found = true;
                    break;
                }
            }
            assert(found);
        }
    }
    num_intervals = 0;
    for (j = 0; j < self->num_buffered_children; j++) {
        child = self->buffered_children[j];
        assert(self->child_edge_map_head[child] != NULL);
        for (int_list = self->child_edge_map_head[child]; int_list != NULL;
                int_list = int_list->next) {
            assert(int_list->left < int_list->right);
            if (int_list->next != NULL) {
                assert(int_list->right < int_list->next->left);
            }
            num_intervals++;
        }
    }
    assert(num_intervals ==
        self->interval_list_heap.total_allocated / (sizeof(interval_list_t)));
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
    simplify_segment_t *u;
    mutation_id_list_t *list_node;
    interval_list_t *int_list;
    node_id_t child;

    fprintf(out, "--simplifier state--\n");
    fprintf(out, "flags:\n");
    fprintf(out, "\tfilter_unreferenced_sites: %d\n",
            !!(self->flags & MSP_FILTER_SITES));
    fprintf(out, "\treduce_to_site_topology  : %d\n",
            !!(self->flags & MSP_REDUCE_TO_SITE_TOPOLOGY));

    fprintf(out, "===\nInput tables\n==\n");
    table_collection_print_state(&self->input_tables, out);
    fprintf(out, "===\nOutput tables\n==\n");
    table_collection_print_state(self->tables, out);
    fprintf(out, "===\nmemory heaps\n==\n");
    fprintf(out, "segment_heap:\n");
    block_allocator_print_state(&self->segment_heap, out);
    fprintf(out, "interval_list_heap:\n");
    block_allocator_print_state(&self->interval_list_heap, out);
    fprintf(out, "===\nancestors\n==\n");
    for (j = 0; j < self->input_tables.nodes->num_rows; j++) {
        fprintf(out, "%d:\t", (int) j);
        print_segment_chain(self->ancestor_map_head[j], out);
        fprintf(out, "\n");
    }
    fprintf(out, "===\nnode_id map (input->output)\n==\n");
    for (j = 0; j < self->input_tables.nodes->num_rows; j++) {
        if (self->node_id_map[j] != MSP_NULL_NODE) {
            fprintf(out, "%d->%d\n", (int) j, self->node_id_map[j]);
        }
    }
    fprintf(out, "===\nsegment queue\n==\n");
    for (j = 0; j < self->segment_queue_size; j++) {
        u = &self->segment_queue[j];
        fprintf(out, "(%f,%f->%d)", u->left, u->right, u->node);
        fprintf(out, "\n");
    }
    fprintf(out, "===\nbuffered children\n==\n");
    for (j = 0; j < self->num_buffered_children; j++) {
        child = self->buffered_children[j];
        fprintf(out, "%d -> ", (int) j);
        for (int_list = self->child_edge_map_head[child]; int_list != NULL;
                int_list = int_list->next) {
            fprintf(out, "(%f, %f), ", int_list->left, int_list->right);
        }
        fprintf(out, "\n");
    }
    fprintf(out, "===\nmutation node map\n==\n");
    for (j = 0; j < self->input_tables.mutations->num_rows; j++) {
        fprintf(out, "%d\t-> %d\n", (int) j, self->mutation_node_map[j]);
    }
    fprintf(out, "===\nnode mutation id list map\n==\n");
    for (j = 0; j < self->input_tables.nodes->num_rows; j++) {
        if (self->node_mutation_list_map_head[j] != NULL) {
            fprintf(out, "%d\t-> [", (int) j);
            for (list_node = self->node_mutation_list_map_head[j]; list_node != NULL;
                    list_node = list_node->next) {
                fprintf(out, "%d,", list_node->mutation);
            }
            fprintf(out, "]\n");
        }
    }
    if (!!(self->flags & MSP_REDUCE_TO_SITE_TOPOLOGY)) {
        fprintf(out, "===\nposition_lookup\n==\n");
        for (j = 0; j < self->input_tables.sites->num_rows + 2; j++) {
            fprintf(out, "%d\t-> %f\n", (int) j, self->position_lookup[j]);
        }
    }
    simplifier_check_state(self);
}

static simplify_segment_t * WARN_UNUSED
simplifier_alloc_segment(simplifier_t *self, double left, double right, node_id_t node)
{
    simplify_segment_t *seg = NULL;

    seg = block_allocator_get(&self->segment_heap, sizeof(*seg));
    if (seg == NULL) {
        goto out;
    }
    seg->next = NULL;
    seg->left = left;
    seg->right = right;
    seg->node = node;
out:
    return seg;
}

static interval_list_t * WARN_UNUSED
simplifier_alloc_interval_list(simplifier_t *self, double left, double right)
{
    interval_list_t *x = NULL;

    x = block_allocator_get(&self->interval_list_heap, sizeof(*x));
    if (x == NULL) {
        goto out;
    }
    x->next = NULL;
    x->left = left;
    x->right = right;
out:
    return x;
}

/* Add a new node to the output node table corresponding to the specified input id.
 * Returns the new ID. */
static int WARN_UNUSED
simplifier_record_node(simplifier_t *self, node_id_t input_id, bool is_sample)
{
    int ret = 0;
    node_t node;
    uint32_t flags;

    ret = node_table_get_row(self->input_tables.nodes, (size_t) input_id, &node);
    if (ret != 0) {
        goto out;
    }
    /* Zero out the sample bit */
    flags = node.flags & (uint32_t) ~MSP_NODE_IS_SAMPLE;
    if (is_sample) {
        flags |= MSP_NODE_IS_SAMPLE;
    }
    self->node_id_map[input_id] = (node_id_t) self->tables->nodes->num_rows;
    ret = node_table_add_row(self->tables->nodes, flags,
            node.time, node.population, node.individual,
            node.metadata, node.metadata_length);
out:
    return ret;
}

/* Remove the mapping for the last recorded node. */
static int
simplifier_rewind_node(simplifier_t *self, node_id_t input_id, node_id_t output_id)
{
    self->node_id_map[input_id] = MSP_NULL_NODE;
    return node_table_truncate(self->tables->nodes, (size_t) output_id);
}

static int
simplifier_flush_edges(simplifier_t *self, node_id_t parent, size_t *ret_num_edges)
{
    int ret = 0;
    size_t j;
    node_id_t child;
    interval_list_t *x;
    size_t num_edges = 0;

    qsort(self->buffered_children, self->num_buffered_children,
            sizeof(node_id_t), cmp_node_id);
    for (j = 0; j < self->num_buffered_children; j++) {
        child = self->buffered_children[j];
        for (x = self->child_edge_map_head[child]; x != NULL; x = x->next) {
            ret = edge_table_add_row(self->tables->edges, x->left, x->right, parent, child);
            if (ret < 0) {
                goto out;
            }
            num_edges++;
        }
        self->child_edge_map_head[child] = NULL;
        self->child_edge_map_tail[child] = NULL;
    }
    self->num_buffered_children = 0;
    *ret_num_edges = num_edges;
    ret = block_allocator_reset(&self->interval_list_heap);
out:
    return ret;
}

/* When we are reducing topology down to what is visible at the sites we need a
 * lookup table to find the closest site position for each edge. We do this with
 * a sorted array and binary search */
static int
simplifier_init_position_lookup(simplifier_t *self)
{
    int ret = 0;
    size_t num_sites = self->input_tables.sites->num_rows;

    self->position_lookup = malloc((num_sites + 2) * sizeof(*self->position_lookup));
    if (self->position_lookup == NULL) {
        goto out;
    }
    self->position_lookup[0] = 0;
    self->position_lookup[num_sites + 1] = self->tables->sequence_length;
    memcpy(self->position_lookup + 1, self->input_tables.sites->position,
            num_sites * sizeof(double));
out:
    return ret;
}
/*
 * Find the smallest site position index greater than or equal to left
 * and right, i.e., slide each endpoint of an interval to the right
 * until they hit a site position. If both left and right map to the
 * the same position then we discard this edge. We also discard an
 * edge if left = 0 and right is less than the first site position.
 */
static bool
simplifier_map_reduced_coordinates(simplifier_t *self, double *left, double *right)
{
    double *X = self->position_lookup;
    size_t N = self->input_tables.sites->num_rows + 2;
    size_t left_index, right_index;
    bool skip = false;

    left_index = msp_search_sorted(X, N, *left);
    right_index = msp_search_sorted(X, N, *right);
    if (left_index == right_index || (left_index == 0 && right_index == 1)) {
        skip = true;
    } else {
        /* Remap back to zero if the left end maps to the first site. */
        if (left_index == 1) {
            left_index = 0;
        }
        *left = X[left_index];
        *right = X[right_index];
    }
    return skip;
}

/* Records the specified edge for the current parent by buffering it */
static int
simplifier_record_edge(simplifier_t *self, double left, double right, node_id_t child)
{
    int ret = 0;
    interval_list_t *tail, *x;
    bool skip;

    if (!!(self->flags & MSP_REDUCE_TO_SITE_TOPOLOGY)) {
        skip = simplifier_map_reduced_coordinates(self, &left, &right);
        /* NOTE: we exit early here when reduce_coordindates has told us to
         * skip this edge, as it is not visible in the reduced tree sequence */
        if (skip) {
            goto out;
        }
    }

    tail = self->child_edge_map_tail[child];
    if (tail == NULL) {
        assert(self->num_buffered_children < self->input_tables.nodes->num_rows);
        self->buffered_children[self->num_buffered_children] = child;
        self->num_buffered_children++;
        x = simplifier_alloc_interval_list(self, left, right);
        if (x == NULL) {
            ret = MSP_ERR_NO_MEMORY;
            goto out;
        }
        self->child_edge_map_head[child] = x;
        self->child_edge_map_tail[child] = x;
    } else {
        if (tail->right == left) {
            tail->right = right;
        } else {
            x = simplifier_alloc_interval_list(self, left, right);
            if (x == NULL) {
                ret = MSP_ERR_NO_MEMORY;
                goto out;
            }
            tail->next = x;
            self->child_edge_map_tail[child] = x;
        }
    }
out:
    return ret;
}

static int
simplifier_init_sites(simplifier_t *self)
{
    int ret = 0;
    node_id_t node;
    mutation_id_list_t *list_node;
    size_t j;

    self->mutation_id_map = calloc(self->input_tables.mutations->num_rows,
            sizeof(mutation_id_t));
    self->mutation_node_map = calloc(self->input_tables.mutations->num_rows,
            sizeof(node_id_t));
    self->node_mutation_list_mem = malloc(self->input_tables.mutations->num_rows *
            sizeof(mutation_id_list_t));
    self->node_mutation_list_map_head = calloc(self->input_tables.nodes->num_rows,
            sizeof(mutation_id_list_t *));
    self->node_mutation_list_map_tail = calloc(self->input_tables.nodes->num_rows,
            sizeof(mutation_id_list_t *));
    if (self->mutation_id_map == NULL || self->mutation_node_map == NULL
            || self->node_mutation_list_mem == NULL
            || self->node_mutation_list_map_head == NULL
            || self->node_mutation_list_map_tail == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    memset(self->mutation_id_map, 0xff,
            self->input_tables.mutations->num_rows * sizeof(mutation_id_t));
    memset(self->mutation_node_map, 0xff,
            self->input_tables.mutations->num_rows * sizeof(node_id_t));

    for (j = 0; j < self->input_tables.mutations->num_rows; j++) {
        node = self->input_tables.mutations->node[j];
        list_node = self->node_mutation_list_mem + j;
        list_node->mutation = (mutation_id_t) j;
        list_node->next = NULL;
        if (self->node_mutation_list_map_head[node] == NULL) {
            self->node_mutation_list_map_head[node] = list_node;
        } else {
            self->node_mutation_list_map_tail[node]->next = list_node;
        }
        self->node_mutation_list_map_tail[node] = list_node;
    }
out:
    return ret;

}

static int WARN_UNUSED
simplifier_add_ancestry(simplifier_t *self, node_id_t input_id, double left, double right,
        node_id_t output_id)
{
    int ret = 0;
    simplify_segment_t *tail = self->ancestor_map_tail[input_id];
    simplify_segment_t *x;

    assert(left < right);
    if (tail == NULL) {
        x = simplifier_alloc_segment(self, left, right, output_id);
        if (x == NULL) {
            ret = MSP_ERR_NO_MEMORY;
            goto out;
        }
        self->ancestor_map_head[input_id] = x;
        self->ancestor_map_tail[input_id] = x;
    } else {
        if (tail->right == left && tail->node == output_id) {
            tail->right = right;
        } else {
            x = simplifier_alloc_segment(self, left, right, output_id);
            if (x == NULL) {
                ret = MSP_ERR_NO_MEMORY;
                goto out;
            }
            tail->next = x;
            self->ancestor_map_tail[input_id] = x;
        }
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
        if (samples[j] < 0 || samples[j] > (node_id_t) self->input_tables.nodes->num_rows) {
            ret = MSP_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        if (!(self->input_tables.nodes->flags[self->samples[j]] & MSP_NODE_IS_SAMPLE)) {
            ret = MSP_ERR_BAD_SAMPLES;
            goto out;
        }
        if (self->is_sample[samples[j]]) {
            ret = MSP_ERR_DUPLICATE_SAMPLE;
            goto out;
        }
        self->is_sample[samples[j]] = true;
        ret = simplifier_record_node(self, samples[j], true);
        if (ret < 0) {
            goto out;
        }
        ret = simplifier_add_ancestry(self, samples[j], 0, self->tables->sequence_length,
            (node_id_t) ret);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

int
simplifier_alloc(simplifier_t *self, node_id_t *samples, size_t num_samples,
        table_collection_t *tables, int flags)
{
    int ret = 0;
    size_t num_nodes_alloc;

    memset(self, 0, sizeof(simplifier_t));
    if (samples == NULL || tables == NULL) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    self->num_samples = num_samples;
    self->flags = flags;
    self->tables = tables;

    /* TODO we can add a flag to skip these checks for when we know they are
     * unnecessary */
    /* TODO Current unit tests require MSP_CHECK_SITE_DUPLICATES but it's
     * debateable whether we need it. If we remove, we definitely need explicit
     * tests to ensure we're doing sensible things with duplicate sites.
     * (Particularly, re MSP_REDUCE_TO_SITE_TOPOLOGY.) */
    ret = table_collection_check_integrity(tables,
            MSP_CHECK_OFFSETS|MSP_CHECK_EDGE_ORDERING|MSP_CHECK_SITE_ORDERING|
            MSP_CHECK_SITE_DUPLICATES);
    if (ret != 0) {
        goto out;
    }

    ret = table_collection_alloc(&self->input_tables, MSP_ALLOC_TABLES);
    if (ret != 0) {
        goto out;
    }
    ret = table_collection_copy(self->tables, &self->input_tables);
    if (ret != 0) {
        goto out;
    }

    /* Take a copy of the input samples */
    self->samples = malloc(num_samples * sizeof(node_id_t));
    if (self->samples == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    memcpy(self->samples, samples, num_samples * sizeof(node_id_t));

    /* Allocate the heaps used for small objects-> Assuming 8K is a good chunk size */
    ret = block_allocator_alloc(&self->segment_heap, 8192);
    if (ret != 0) {
        goto out;
    }
    ret = block_allocator_alloc(&self->interval_list_heap, 8192);
    if (ret != 0) {
        goto out;
    }
    ret = segment_overlapper_alloc(&self->segment_overlapper);
    if (ret != 0) {
        goto out;
    }
    /* Need to avoid malloc(0) so make sure we have at least 1. */
    num_nodes_alloc = 1 + tables->nodes->num_rows;
    /* Make the maps and set the intial state */
    self->ancestor_map_head = calloc(num_nodes_alloc, sizeof(simplify_segment_t *));
    self->ancestor_map_tail = calloc(num_nodes_alloc, sizeof(simplify_segment_t *));
    self->child_edge_map_head = calloc(num_nodes_alloc, sizeof(interval_list_t *));
    self->child_edge_map_tail = calloc(num_nodes_alloc, sizeof(interval_list_t *));
    self->node_id_map = malloc(num_nodes_alloc * sizeof(node_id_t));
    self->buffered_children = malloc(num_nodes_alloc * sizeof(node_id_t));
    self->is_sample = calloc(num_nodes_alloc, sizeof(bool));
    self->max_segment_queue_size = 64;
    self->segment_queue = malloc(self->max_segment_queue_size
            * sizeof(simplify_segment_t));
    if (self->ancestor_map_head == NULL || self->ancestor_map_tail == NULL
            || self->child_edge_map_head == NULL || self->child_edge_map_tail == NULL
            || self->node_id_map == NULL || self->is_sample == NULL
            || self->segment_queue == NULL || self->buffered_children == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    ret = table_collection_clear(self->tables);
    if (ret != 0) {
        goto out;
    }
    memset(self->node_id_map, 0xff, self->input_tables.nodes->num_rows * sizeof(node_id_t));
    ret = simplifier_init_samples(self, samples);
    if (ret != 0) {
        goto out;
    }
    ret = simplifier_init_sites(self);
    if (ret != 0) {
        goto out;
    }
    if (!!(self->flags & MSP_REDUCE_TO_SITE_TOPOLOGY)) {
        ret = simplifier_init_position_lookup(self);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

int
simplifier_free(simplifier_t *self)
{
    table_collection_free(&self->input_tables);
    block_allocator_free(&self->segment_heap);
    block_allocator_free(&self->interval_list_heap);
    segment_overlapper_free(&self->segment_overlapper);
    msp_safe_free(self->samples);
    msp_safe_free(self->ancestor_map_head);
    msp_safe_free(self->ancestor_map_tail);
    msp_safe_free(self->child_edge_map_head);
    msp_safe_free(self->child_edge_map_tail);
    msp_safe_free(self->node_id_map);
    msp_safe_free(self->segment_queue);
    msp_safe_free(self->is_sample);
    msp_safe_free(self->mutation_id_map);
    msp_safe_free(self->mutation_node_map);
    msp_safe_free(self->node_mutation_list_mem);
    msp_safe_free(self->node_mutation_list_map_head);
    msp_safe_free(self->node_mutation_list_map_tail);
    msp_safe_free(self->buffered_children);
    msp_safe_free(self->position_lookup);
    return 0;
}

static int WARN_UNUSED
simplifier_enqueue_segment(simplifier_t *self, double left, double right, node_id_t node)
{
    int ret = 0;
    simplify_segment_t *seg;
    void *p;

    assert(left < right);
    /* Make sure we always have room for one more segment in the queue so we
     * can put a tail sentinel on it */
    if (self->segment_queue_size == self->max_segment_queue_size - 1) {
        self->max_segment_queue_size *= 2;
        p = realloc(self->segment_queue,
                self->max_segment_queue_size * sizeof(*self->segment_queue));
        if (p == NULL) {
            ret = MSP_ERR_NO_MEMORY;
            goto out;
        }
        self->segment_queue = p;
    }
    seg = self->segment_queue + self->segment_queue_size;
    seg->left = left;
    seg->right = right;
    seg->node = node;
    self->segment_queue_size++;
out:
    return ret;
}

static int WARN_UNUSED
simplifier_merge_ancestors(simplifier_t *self, node_id_t input_id)
{
    int ret = 0;
    simplify_segment_t **X, *x;
    size_t j, num_overlapping, num_flushed_edges;
    double left, right, prev_right;
    node_id_t ancestry_node;
    node_id_t output_id = self->node_id_map[input_id];
    bool is_sample = output_id != MSP_NULL_NODE;

    if (is_sample) {
        /* Free up the existing ancestry mapping. */
        x = self->ancestor_map_tail[input_id];
        assert(x->left == 0 && x->right == self->tables->sequence_length);
        self->ancestor_map_head[input_id] = NULL;
        self->ancestor_map_tail[input_id] = NULL;
    }

    ret = segment_overlapper_init(&self->segment_overlapper,
            self->segment_queue, self->segment_queue_size);
    if (ret != 0) {
        goto out;
    }
    prev_right = 0;
    while ((ret = segment_overlapper_next(&self->segment_overlapper,
                    &left, &right, &X, &num_overlapping)) == 1) {
        assert(left < right);
        assert(num_overlapping > 0);
        if (num_overlapping == 1) {
            ancestry_node = X[0]->node;
            if (is_sample) {
                ret = simplifier_record_edge(self, left, right, ancestry_node);
                if (ret != 0) {
                    goto out;
                }
                ancestry_node = output_id;
            }
        } else {
            if (output_id == MSP_NULL_NODE) {
                ret = simplifier_record_node(self, input_id, false);
                if (ret < 0) {
                    goto out;
                }
                output_id = (node_id_t) ret;
            }
            ancestry_node = output_id;
            for (j = 0; j < num_overlapping; j++) {
                ret = simplifier_record_edge(self, left, right, X[j]->node);
                if (ret != 0) {
                    goto out;
                }
            }

        }
        if (is_sample && left != prev_right) {
            /* Fill in any gaps in ancestry for the sample */
            ret = simplifier_add_ancestry(self, input_id, prev_right, left, output_id);
            if (ret != 0) {
                goto out;
            }
        }
        ret = simplifier_add_ancestry(self, input_id, left, right, ancestry_node);
        if (ret != 0) {
            goto out;
        }
        prev_right = right;
    }
    /* Check for errors occuring in the loop condition */
    if (ret != 0) {
        goto out;
    }
    if (is_sample && prev_right != self->tables->sequence_length) {
        /* If a trailing gap exists in the sample ancestry, fill it in. */
        ret = simplifier_add_ancestry(self, input_id, prev_right,
                self->tables->sequence_length, output_id);
        if (ret != 0) {
            goto out;
        }
    }
    if (output_id != MSP_NULL_NODE) {
        ret = simplifier_flush_edges(self, output_id, &num_flushed_edges);
        if (ret != 0) {
            goto out;
        }
        if (num_flushed_edges == 0 && !is_sample) {
            ret = simplifier_rewind_node(self, input_id, output_id);
        }
    }
out:
    return ret;
}

static int WARN_UNUSED
simplifier_process_parent_edges(simplifier_t *self, node_id_t parent, size_t start,
        size_t end)
{
    int ret = 0;
    size_t j;
    simplify_segment_t *x;
    const edge_table_t *input_edges = self->input_tables.edges;
    node_id_t child;
    double left, right;

    /* Go through the edges and queue up ancestry segments for processing. */
    self->segment_queue_size = 0;
    for (j = start; j < end; j++) {
        assert(parent == input_edges->parent[j]);
        child = input_edges->child[j];
        left = input_edges->left[j];
        right = input_edges->right[j];
        for (x = self->ancestor_map_head[child]; x != NULL; x = x->next) {
            if (x->right > left && right > x->left) {
                ret = simplifier_enqueue_segment(self,
                        MSP_MAX(x->left, left), MSP_MIN(x->right, right), x->node);
                if (ret != 0) {
                    goto out;
                }
            }
        }
    }
    /* We can now merge the ancestral segments for the parent */
    ret = simplifier_merge_ancestors(self, parent);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

static int WARN_UNUSED
simplifier_map_mutation_nodes(simplifier_t *self)
{
    int ret = 0;
    simplify_segment_t *seg;
    mutation_id_list_t *m_node;
    size_t input_node;
    site_id_t site;
    double position;

    for (input_node = 0; input_node < self->input_tables.nodes->num_rows; input_node++) {
        seg = self->ancestor_map_head[input_node];
        m_node = self->node_mutation_list_map_head[input_node];
        /* Co-iterate over the segments and mutations; mutations must be listed
         * in increasing order of site position */
        while (seg != NULL && m_node != NULL) {
            site = self->input_tables.mutations->site[m_node->mutation];
            position = self->input_tables.sites->position[site];
            if (seg->left <= position && position < seg->right) {
                self->mutation_node_map[m_node->mutation] = seg->node;
                m_node = m_node->next;
            } else if (position >= seg->right) {
                seg = seg->next;
            } else {
                assert(position < seg->left);
                m_node = m_node->next;
            }
        }
    }
    return ret;
}

static int WARN_UNUSED
simplifier_output_sites(simplifier_t *self)
{
    int ret = 0;
    site_id_t input_site;
    mutation_id_t input_mutation, mapped_parent ,site_start, site_end;
    site_id_t num_input_sites = (site_id_t) self->input_tables.sites->num_rows;
    mutation_id_t num_input_mutations = (mutation_id_t) self->input_tables.mutations->num_rows;
    mutation_id_t input_parent, num_output_mutations, num_output_site_mutations;
    node_id_t mapped_node;
    bool keep_site;
    bool filter_sites = !!(self->flags & MSP_FILTER_SITES);
    site_t site;
    mutation_t mutation;

    input_mutation = 0;
    num_output_mutations = 0;
    for (input_site = 0; input_site < num_input_sites; input_site++) {
        ret = site_table_get_row(self->input_tables.sites, (size_t) input_site, &site);
        if (ret != 0) {
            goto out;
        }
        site_start = input_mutation;
        num_output_site_mutations = 0;
        while (input_mutation < num_input_mutations
                && self->input_tables.mutations->site[input_mutation] == site.id) {
            mapped_node = self->mutation_node_map[input_mutation];
            if (mapped_node != MSP_NULL_NODE) {
                input_parent = self->input_tables.mutations->parent[input_mutation];
                mapped_parent = MSP_NULL_MUTATION;
                if (input_parent != MSP_NULL_MUTATION) {
                    mapped_parent = self->mutation_id_map[input_parent];
                }
                self->mutation_id_map[input_mutation] = num_output_mutations;
                num_output_mutations++;
                num_output_site_mutations++;
            }
            input_mutation++;
        }
        site_end = input_mutation;

        keep_site = true;
        if (filter_sites && num_output_site_mutations == 0) {
            keep_site = false;
        }
        if (keep_site) {
            for (input_mutation = site_start; input_mutation < site_end; input_mutation++) {
                if (self->mutation_id_map[input_mutation] != MSP_NULL_MUTATION) {
                    assert(self->tables->mutations->num_rows
                            == (size_t) self->mutation_id_map[input_mutation]);
                    mapped_node = self->mutation_node_map[input_mutation];
                    assert(mapped_node != MSP_NULL_NODE);
                    mapped_parent = self->input_tables.mutations->parent[input_mutation];
                    if (mapped_parent != MSP_NULL_MUTATION) {
                        mapped_parent = self->mutation_id_map[mapped_parent];
                    }
                    ret = mutation_table_get_row(self->input_tables.mutations,
                            (size_t) input_mutation, &mutation);
                    if (ret != 0) {
                        goto out;
                    }
                    ret = mutation_table_add_row(self->tables->mutations,
                            (site_id_t) self->tables->sites->num_rows,
                            mapped_node, mapped_parent,
                            mutation.derived_state, mutation.derived_state_length,
                            mutation.metadata, mutation.metadata_length);
                    if (ret < 0) {
                        goto out;
                    }
                }
            }
            ret = site_table_add_row(self->tables->sites, site.position,
                    site.ancestral_state, site.ancestral_state_length,
                    site.metadata, site.metadata_length);
            if (ret < 0) {
                goto out;
            }
        }
        assert(num_output_mutations == (mutation_id_t) self->tables->mutations->num_rows);
        input_mutation = site_end;
    }
    assert(input_mutation == num_input_mutations);
    ret = 0;
out:
    return ret;
}

static int WARN_UNUSED
simplifier_finalise_references(simplifier_t *self)
{
    int ret = 0;
    table_size_t j;
    bool keep;
    table_size_t num_nodes = self->tables->nodes->num_rows;

    tmp_population_t pop;
    population_id_t pop_id;
    table_size_t num_populations = self->input_tables.populations->num_rows;
    population_id_t *node_population = self->tables->nodes->population;
    bool *population_referenced = calloc(num_populations, sizeof(*population_referenced));
    population_id_t *population_id_map = malloc(
            num_populations * sizeof(*population_id_map));
    bool filter_populations = !!(self->flags & MSP_FILTER_POPULATIONS);

    individual_t ind;
    individual_id_t ind_id;
    table_size_t num_individuals = self->input_tables.individuals->num_rows;
    individual_id_t *node_individual = self->tables->nodes->individual;
    bool *individual_referenced = calloc(num_individuals, sizeof(*individual_referenced));
    individual_id_t *individual_id_map = malloc(
            num_individuals * sizeof(*individual_id_map));
    bool filter_individuals = !!(self->flags & MSP_FILTER_INDIVIDUALS);

    if (population_referenced == NULL || population_id_map == NULL
            || individual_referenced == NULL || individual_id_map == NULL) {
        goto out;
    }

    /* TODO Migrations fit reasonably neatly into the pattern that we have here. We can
     * consider references to populations from migration objects in the same way
     * as from nodes, so that we only remove a population if its referenced by
     * neither. Mapping the population IDs in migrations is then easy. In principle
     * nodes are similar, but the semantics are slightly different because we've
     * already allocated all the nodes by their references from edges. We then
     * need to decide whether we remove migrations that reference unmapped nodes
     * or whether to add these nodes back in (probably the former is the correct
     * approach).*/
    if (self->input_tables.migrations->num_rows != 0) {
        ret = MSP_ERR_SIMPLIFY_MIGRATIONS_NOT_SUPPORTED;
        goto out;
    }

    for (j = 0; j < num_nodes; j++) {
        pop_id = node_population[j];
        if (pop_id != MSP_NULL_POPULATION) {
            population_referenced[pop_id] = true;
        }
        ind_id = node_individual[j];
        if (ind_id != MSP_NULL_POPULATION) {
            individual_referenced[ind_id] = true;
        }
    }
    for (j = 0; j < num_populations; j++) {
        ret = population_table_get_row(self->input_tables.populations, j, &pop);
        if (ret != 0) {
            goto out;
        }
        keep = true;
        if (filter_populations && !population_referenced[j]) {
            keep = false;
        }
        population_id_map[j] = MSP_NULL_POPULATION;
        if (keep) {
            ret = population_table_add_row(self->tables->populations,
                pop.metadata, pop.metadata_length);
            if (ret < 0) {
                goto out;
            }
            population_id_map[j] = (population_id_t) ret;
        }
    }

    for (j = 0; j < num_individuals; j++) {
        ret = individual_table_get_row(self->input_tables.individuals, j, &ind);
        if (ret != 0) {
            goto out;
        }
        keep = true;
        if (filter_individuals && !individual_referenced[j]) {
            keep = false;
        }
        individual_id_map[j] = MSP_NULL_POPULATION;
        if (keep) {
            ret = individual_table_add_row(self->tables->individuals,
                ind.flags, ind.location, ind.location_length,
                ind.metadata, ind.metadata_length);
            if (ret < 0) {
                goto out;
            }
            individual_id_map[j] = (individual_id_t) ret;
        }
    }

    /* Remap node IDs referencing the above */
    for (j = 0; j < num_nodes; j++) {
        pop_id = node_population[j];
        if (pop_id != MSP_NULL_POPULATION) {
            node_population[j] = population_id_map[pop_id];
        }
        ind_id = node_individual[j];
        if (ind_id != MSP_NULL_POPULATION) {
            node_individual[j] = individual_id_map[ind_id];
        }
    }

    ret = provenance_table_copy(self->input_tables.provenances, self->tables->provenances);
    if (ret != 0) {
        goto out;
    }
out:
    msp_safe_free(population_referenced);
    msp_safe_free(individual_referenced);
    msp_safe_free(population_id_map);
    msp_safe_free(individual_id_map);
    return ret;
}

int WARN_UNUSED
simplifier_run(simplifier_t *self, node_id_t *node_map)
{
    int ret = 0;
    size_t j, start;
    node_id_t parent, current_parent;
    const edge_table_t *input_edges = self->input_tables.edges;
    size_t num_edges = input_edges->num_rows;

    if (num_edges > 0) {
        start = 0;
        current_parent = input_edges->parent[0];
        for (j = 0; j < num_edges; j++) {
            parent = input_edges->parent[j];
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
    ret = simplifier_map_mutation_nodes(self);
    if (ret != 0) {
        goto out;
    }
    ret = simplifier_output_sites(self);
    if (ret != 0) {
        goto out;
    }
    ret = simplifier_finalise_references(self);
    if (ret != 0) {
        goto out;
    }
    if (node_map != NULL) {
        /* Finally, output the new IDs for the nodes, if required. */
        memcpy(node_map, self->node_id_map,
                self->input_tables.nodes->num_rows * sizeof(node_id_t));
    }
out:
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

    ret = check_offsets(self->nodes->num_rows, self->nodes->metadata_offset,
            self->nodes->metadata_length, true);
    if (ret != 0) {
        goto out;
    }
    ret = check_offsets(self->sites->num_rows, self->sites->ancestral_state_offset,
            self->sites->ancestral_state_length, true);
    if (ret != 0) {
        goto out;
    }
    ret = check_offsets(self->sites->num_rows, self->sites->metadata_offset,
            self->sites->metadata_length, true);
    if (ret != 0) {
        goto out;
    }
    ret = check_offsets(self->mutations->num_rows, self->mutations->derived_state_offset,
            self->mutations->derived_state_length, true);
    if (ret != 0) {
        goto out;
    }
    ret = check_offsets(self->mutations->num_rows, self->mutations->metadata_offset,
            self->mutations->metadata_length, true);
    if (ret != 0) {
        goto out;
    }
    ret = check_offsets(self->individuals->num_rows, self->individuals->metadata_offset,
            self->individuals->metadata_length, true);
    if (ret != 0) {
        goto out;
    }
    ret = check_offsets(self->provenances->num_rows, self->provenances->timestamp_offset,
            self->provenances->timestamp_length, true);
    if (ret != 0) {
        goto out;
    }
    ret = check_offsets(self->provenances->num_rows, self->provenances->record_offset,
            self->provenances->record_length, true);
    if (ret != 0) {
        goto out;
    }
    ret = 0;
out:
    return ret;
}

static int
table_collection_check_edge_ordering(table_collection_t *self)
{
    int ret = 0;
    table_size_t j;
    node_id_t parent, last_parent, child, last_child;
    double left, last_left;
    const double *time = self->nodes->time;
    bool *parent_seen = calloc(self->nodes->num_rows, sizeof(bool));

    if (parent_seen == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }

    for (j = 0; j < self->edges->num_rows; j++) {
        left = self->edges->left[j];
        parent = self->edges->parent[j];
        child = self->edges->child[j];
        if (parent_seen[parent]) {
            ret = MSP_ERR_EDGES_NONCONTIGUOUS_PARENTS;
            goto out;
        }
        if (j > 0) {
            /* Input data must sorted by (time[parent], parent, child, left). */
            if (time[parent] < time[last_parent]) {
                ret = MSP_ERR_EDGES_NOT_SORTED_PARENT_TIME;
                goto out;
            }
            if (time[parent] == time[last_parent]) {
                if (parent == last_parent) {
                    if (child < last_child) {
                        ret = MSP_ERR_EDGES_NOT_SORTED_CHILD;
                        goto out;
                    }
                    if (child == last_child) {
                        if (left == last_left) {
                            ret = MSP_ERR_DUPLICATE_EDGES;
                            goto out;
                        } else if (left < last_left) {
                            ret = MSP_ERR_EDGES_NOT_SORTED_LEFT;
                            goto out;
                        }
                    }
                } else {
                    parent_seen[last_parent] = true;
                }
            }
        }
        last_parent = parent;
        last_child = child;
        last_left = left;
    }
out:
    msp_safe_free(parent_seen);
    return ret;
}


/* Checks the integrity of the table collection. What gets checked depends
 * on the flags values:
 * 0                             Check the integrity of ID & spatial references.
 * MSP_CHECK_OFFSETS             Check offsets for ragged columns.
 * MSP_CHECK_EDGE_ORDERING       Check edge ordering contraints for a tree sequence.
 * MSP_CHECK_SITE_ORDERING       Check that sites are in nondecreasing position order.
 * MSP_CHECK_SITE_DUPLICATES     Check for any duplicate site positions.
 * MSP_CHECK_MUTATION_ORDERING   Check mutation ordering contraints for a tree sequence.
 * MSP_CHECK_INDEXES             Check indexes exist & reference integrity.
 * MSP_CHECK_ALL                 All above checks.
 */
int WARN_UNUSED
table_collection_check_integrity(table_collection_t *self, int flags)
{
    int ret = MSP_ERR_GENERIC;
    table_size_t j;
    double left, right, position;
    double L = self->sequence_length;
    double *time = self->nodes->time;
    node_id_t parent, child;
    mutation_id_t parent_mut;
    population_id_t population;
    individual_id_t individual;
    node_id_t num_nodes = (node_id_t) self->nodes->num_rows;
    edge_id_t num_edges = (edge_id_t) self->edges->num_rows;
    site_id_t num_sites = (site_id_t) self->sites->num_rows;
    mutation_id_t num_mutations = (mutation_id_t) self->mutations->num_rows;
    population_id_t num_populations = (population_id_t) self->populations->num_rows;
    individual_id_t num_individuals = (individual_id_t) self->individuals->num_rows;
    bool check_site_ordering = !!(flags & MSP_CHECK_SITE_ORDERING);
    bool check_site_duplicates = !!(flags & MSP_CHECK_SITE_DUPLICATES);
    bool check_mutation_ordering = !!(flags & MSP_CHECK_MUTATION_ORDERING);

    if (self->sequence_length <= 0) {
        ret = MSP_ERR_BAD_SEQUENCE_LENGTH;
        goto out;
    }

    /* Nodes */
    for (j = 0; j < self->nodes->num_rows; j++) {
        population = self->nodes->population[j];
        if (population < MSP_NULL_POPULATION || population >= num_populations) {
            ret = MSP_ERR_POPULATION_OUT_OF_BOUNDS;
            goto out;
        }
        individual = self->nodes->individual[j];
        if (individual < MSP_NULL_POPULATION || individual >= num_individuals) {
            ret = MSP_ERR_INDIVIDUAL_OUT_OF_BOUNDS;
            goto out;
        }
    }

    /* Edges */
    for (j = 0; j < self->edges->num_rows; j++) {
        parent = self->edges->parent[j];
        child = self->edges->child[j];
        left = self->edges->left[j];
        right = self->edges->right[j];
        /* Node ID integrity */
        if (parent == MSP_NULL_NODE) {
            ret = MSP_ERR_NULL_PARENT;
            goto out;
        }
        if (parent < 0 || parent >= num_nodes) {
            ret = MSP_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        if (child == MSP_NULL_NODE) {
            ret = MSP_ERR_NULL_CHILD;
            goto out;
        }
        if (child < 0 || child >= num_nodes) {
            ret = MSP_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        /* Spatial requirements for edges */
        if (left < 0) {
            ret = MSP_ERR_LEFT_LESS_ZERO;
            goto out;
        }
        if (right > L) {
            ret = MSP_ERR_RIGHT_GREATER_SEQ_LENGTH;
            goto out;
        }
        if (left >= right) {
            ret = MSP_ERR_BAD_EDGE_INTERVAL;
            goto out;
        }
        /* time[child] must be < time[parent] */
        if (time[child] >= time[parent]) {
            ret = MSP_ERR_BAD_NODE_TIME_ORDERING;
            goto out;
        }
    }
    for (j = 0; j < self->sites->num_rows; j++) {
        position = self->sites->position[j];
        /* Spatial requirements */
        if (position < 0 || position >= L) {
            ret = MSP_ERR_BAD_SITE_POSITION;
            goto out;
        }
        if (j > 0) {
            if (check_site_duplicates && self->sites->position[j - 1] == position) {
                ret = MSP_ERR_DUPLICATE_SITE_POSITION;
                goto out;
            }
            if (check_site_ordering && self->sites->position[j - 1] > position) {
                ret = MSP_ERR_UNSORTED_SITES;
                goto out;
            }
        }
    }

    /* Mutations */
    for (j = 0; j < self->mutations->num_rows; j++) {
        if (self->mutations->site[j] < 0 || self->mutations->site[j] >= num_sites) {
            ret = MSP_ERR_SITE_OUT_OF_BOUNDS;
            goto out;
        }
        if (self->mutations->node[j] < 0 || self->mutations->node[j] >= num_nodes) {
            ret = MSP_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        parent_mut = self->mutations->parent[j];
        if (parent_mut < MSP_NULL_MUTATION || parent_mut >= num_mutations) {
            ret = MSP_ERR_MUTATION_OUT_OF_BOUNDS;
            goto out;
        }
        if (parent_mut == (mutation_id_t) j) {
            ret = MSP_ERR_MUTATION_PARENT_EQUAL;
            goto out;
        }
        if (check_mutation_ordering) {
            if (parent_mut != MSP_NULL_MUTATION) {
                /* Parents must be listed before their children */
                if (parent_mut > (mutation_id_t) j) {
                    ret = MSP_ERR_MUTATION_PARENT_AFTER_CHILD;
                    goto out;
                }
                if (self->mutations->site[parent_mut] != self->mutations->site[j]) {
                    ret = MSP_ERR_MUTATION_PARENT_DIFFERENT_SITE;
                    goto out;
                }
            }
            if (j > 0) {
                if (self->mutations->site[j - 1] > self->mutations->site[j]) {
                    ret = MSP_ERR_UNSORTED_MUTATIONS;
                    goto out;
                }
            }
        }
    }

    /* Migrations */
    for (j = 0; j < self->migrations->num_rows; j++) {
        if (self->migrations->node[j] < 0 || self->migrations->node[j] >= num_nodes) {
            ret = MSP_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        if (self->migrations->source[j] < 0
                || self->migrations->source[j] >= num_populations) {
            ret = MSP_ERR_POPULATION_OUT_OF_BOUNDS;
            goto out;
        }
        if (self->migrations->dest[j] < 0
                || self->migrations->dest[j] >= num_populations) {
            ret = MSP_ERR_POPULATION_OUT_OF_BOUNDS;
            goto out;
        }
        left = self->migrations->left[j];
        right = self->migrations->right[j];
        /* Spatial requirements */
        /* TODO it's a bit misleading to use the edge-specific errors here. */
        if (left < 0) {
            ret = MSP_ERR_LEFT_LESS_ZERO;
            goto out;
        }
        if (right > L) {
            ret = MSP_ERR_RIGHT_GREATER_SEQ_LENGTH;
            goto out;
        }
        if (left >= right) {
            ret = MSP_ERR_BAD_EDGE_INTERVAL;
            goto out;
        }
    }

    if (!!(flags & MSP_CHECK_INDEXES)) {
        if (!table_collection_is_indexed(self)) {
            ret = MSP_ERR_TABLES_NOT_INDEXED;
            goto out;
        }
        for (j = 0; j < self->edges->num_rows; j++) {
            if (self->indexes.edge_insertion_order[j] < 0 ||
                    self->indexes.edge_insertion_order[j] >= num_edges) {
                ret = MSP_ERR_BAD_EDGE_INDEX;
                goto out;
            }
            if (self->indexes.edge_removal_order[j] < 0 ||
                    self->indexes.edge_removal_order[j] >= num_edges) {
                ret = MSP_ERR_BAD_EDGE_INDEX;
                goto out;
            }
        }
    }

    ret = 0;
    if (!!(flags & MSP_CHECK_OFFSETS)) {
        ret = table_collection_check_offsets(self);
        if (ret != 0) {
            goto out;
        }
    }
    if (!!(flags & MSP_CHECK_EDGE_ORDERING)) {
        ret = table_collection_check_edge_ordering(self);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

int
table_collection_print_state(table_collection_t *self, FILE *out)
{
    fprintf(out, "Table collection state\n");
    fprintf(out, "sequence_length = %f\n", self->sequence_length);
    individual_table_print_state(self->individuals, out);
    node_table_print_state(self->nodes, out);
    edge_table_print_state(self->edges, out);
    migration_table_print_state(self->migrations, out);
    site_table_print_state(self->sites, out);
    mutation_table_print_state(self->mutations, out);
    population_table_print_state(self->populations, out);
    provenance_table_print_state(self->provenances, out);
    return 0;
}

/* TODO make sequence_length a parameter here, and only support setting it
 * through this interface. That way, sequence_length is set at the initialisation
 * time and can be assumed to be fixed for ever after that. This might
 * finally get rid of the inferring sequence length stuff that we've
 * been plagued with forever.
 *
 * HOWEVER this disagrees with the usage of calling table_collection_alloc
 * before table_collection_load. Perhaps this is a good reason for allowing
 * the user to call table_collection_load directly without calling alloc
 * first. This is certainly more user-friendly.
 * */
int
table_collection_alloc(table_collection_t *self, int flags)
{
    int ret = 0;
    memset(self, 0, sizeof(*self));
    self->external_tables = false;
    self->individuals = calloc(1, sizeof(*self->individuals));
    self->nodes = calloc(1, sizeof(*self->nodes));
    self->edges = calloc(1, sizeof(*self->edges));
    self->migrations = calloc(1, sizeof(*self->migrations));
    self->sites = calloc(1, sizeof(*self->sites));
    self->mutations = calloc(1, sizeof(*self->mutations));
    self->mutations = calloc(1, sizeof(*self->mutations));
    self->populations = calloc(1, sizeof(*self->populations));
    self->provenances = calloc(1, sizeof(*self->provenances));
    if (self->individuals == NULL || self->nodes == NULL
            || self->edges == NULL || self->migrations == NULL
            || self->sites == NULL || self->mutations == NULL
            || self->populations == NULL || self->provenances == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    if (flags & MSP_ALLOC_TABLES) {
        /* Allocate all the tables with their default increments */
        ret = node_table_alloc(self->nodes, 0, 0);
        if (ret != 0) {
            goto out;
        }
        ret = edge_table_alloc(self->edges, 0);
        if (ret != 0) {
            goto out;
        }
        ret = migration_table_alloc(self->migrations, 0);
        if (ret != 0) {
            goto out;
        }
        ret = site_table_alloc(self->sites, 0, 0, 0);
        if (ret != 0) {
            goto out;
        }
        ret = mutation_table_alloc(self->mutations, 0, 0, 0);
        if (ret != 0) {
            goto out;
        }
        ret = individual_table_alloc(self->individuals, 0, 0, 0);
        if (ret != 0) {
            goto out;
        }
        ret = population_table_alloc(self->populations, 0, 0);
        if (ret != 0) {
            goto out;
        }
        ret = provenance_table_alloc(self->provenances, 0, 0, 0);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

static int
table_collection_free_tables(table_collection_t *self)
{
    if (self->individuals != NULL) {
        individual_table_free(self->individuals);
        free(self->individuals);
        self->individuals = NULL;
    }
    if (self->nodes != NULL) {
        node_table_free(self->nodes);
        free(self->nodes);
        self->nodes = NULL;
    }
    if (self->edges != NULL) {
        edge_table_free(self->edges);
        free(self->edges);
        self->edges = NULL;
    }
    if (self->migrations != NULL) {
        migration_table_free(self->migrations);
        free(self->migrations);
        self->migrations = NULL;
    }
    if (self->sites != NULL) {
        site_table_free(self->sites);
        free(self->sites);
        self->sites = NULL;
    }
    if (self->mutations != NULL) {
        mutation_table_free(self->mutations);
        free(self->mutations);
        self->mutations = NULL;
    }
    if (self->populations != NULL) {
        population_table_free(self->populations);
        free(self->populations);
        self->populations = NULL;
    }
    if (self->provenances != NULL) {
        provenance_table_free(self->provenances);
        free(self->provenances);
        self->provenances = NULL;
    }
    return 0;
}

int
table_collection_set_tables(table_collection_t *self,
        individual_table_t *individuals, node_table_t *nodes, edge_table_t *edges,
        migration_table_t *migrations, site_table_t *sites,
        mutation_table_t *mutations, population_table_t *populations,
        provenance_table_t *provenances)
{
    int ret = 0;

    if (individuals == NULL || nodes == NULL || edges == NULL
            || migrations == NULL || sites == NULL || mutations == NULL
            || populations == NULL || provenances == NULL) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    table_collection_free_tables(self);
    self->external_tables = true;
    self->individuals = individuals;
    self->nodes = nodes;
    self->edges = edges;
    self->migrations = migrations;
    self->sites = sites;
    self->mutations = mutations;
    self->mutations = mutations;
    self->populations = populations;
    self->provenances = provenances;
out:
    return ret;
}

int
table_collection_free(table_collection_t *self)
{
    int ret = 0;
    if (! self->external_tables) {
        table_collection_free_tables(self);
    }
    if (self->indexes.malloced_locally) {
        msp_safe_free(self->indexes.edge_insertion_order);
        msp_safe_free(self->indexes.edge_removal_order);
    }
    if (self->store != NULL) {
        kastore_close(self->store);
        free(self->store);
    }
    msp_safe_free(self->file_uuid);
    return ret;
}

/* Returns true if all the tables and collection metadata are equal. Note
 * this does *not* consider the indexes, since these are derived from the
 * tables. We do not consider the file_uuids either, since this is a property of
 * the file that set of tables is stored in. */
bool
table_collection_equals(table_collection_t *self, table_collection_t *other)
{
    bool ret = self->sequence_length == other->sequence_length
        && individual_table_equals(self->individuals, other->individuals)
        && node_table_equals(self->nodes, other->nodes)
        && edge_table_equals(self->edges, other->edges)
        && migration_table_equals(self->migrations, other->migrations)
        && site_table_equals(self->sites, other->sites)
        && mutation_table_equals(self->mutations, other->mutations)
        && population_table_equals(self->populations, other->populations)
        && provenance_table_equals(self->provenances, other->provenances);
    return ret;
}

int WARN_UNUSED
table_collection_copy(table_collection_t *self, table_collection_t *dest)
{
    int ret = 0;
    size_t index_size;

    ret = node_table_copy(self->nodes, dest->nodes);
    if (ret != 0) {
        goto out;
    }
    ret = edge_table_copy(self->edges, dest->edges);
    if (ret != 0) {
        goto out;
    }
    ret = migration_table_copy(self->migrations, dest->migrations);
    if (ret != 0) {
        goto out;
    }
    ret = site_table_copy(self->sites, dest->sites);
    if (ret != 0) {
        goto out;
    }
    ret = mutation_table_copy(self->mutations, dest->mutations);
    if (ret != 0) {
        goto out;
    }
    ret = individual_table_copy(self->individuals, dest->individuals);
    if (ret != 0) {
        goto out;
    }
    ret = population_table_copy(self->populations, dest->populations);
    if (ret != 0) {
        goto out;
    }
    ret = provenance_table_copy(self->provenances, dest->provenances);
    if (ret != 0) {
        goto out;
    }
    dest->sequence_length = self->sequence_length;
    if (table_collection_is_indexed(self)) {
        table_collection_drop_indexes(dest);
        index_size = self->edges->num_rows * sizeof(edge_id_t);
        dest->indexes.edge_insertion_order = malloc(index_size);
        dest->indexes.edge_removal_order = malloc(index_size);
        dest->indexes.malloced_locally = true;
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
    if (self->indexes.malloced_locally) {
        msp_safe_free(self->indexes.edge_insertion_order);
        msp_safe_free(self->indexes.edge_removal_order);
    }
    self->indexes.edge_insertion_order = NULL;
    self->indexes.edge_removal_order = NULL;
    return 0;
}

int WARN_UNUSED
table_collection_build_indexes(table_collection_t *self, int MSP_UNUSED(flags))
{
    int ret = MSP_ERR_GENERIC;
    size_t j;
    double *time = self->nodes->time;
    index_sort_t *sort_buff = NULL;
    node_id_t parent;

    table_collection_drop_indexes(self);
    self->indexes.malloced_locally = true;
    self->indexes.edge_insertion_order = malloc(self->edges->num_rows * sizeof(edge_id_t));
    self->indexes.edge_removal_order = malloc(self->edges->num_rows * sizeof(edge_id_t));
    if (self->indexes.edge_insertion_order == NULL
            || self->indexes.edge_removal_order == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }

    /* Alloc the sort buffer */
    sort_buff = malloc(self->edges->num_rows * sizeof(index_sort_t));
    if (sort_buff == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    /* TODO we should probably drop these checks and call check_integrity instead.
     * Do this when we're providing the Python API for build_indexes, so that
     * we can test it properly. */

    /* sort by left and increasing time to give us the order in which
     * records should be inserted */
    for (j = 0; j < self->edges->num_rows; j++) {
        sort_buff[j].index = (node_id_t ) j;
        sort_buff[j].first = self->edges->left[j];
        parent = self->edges->parent[j];
        if (parent == MSP_NULL_NODE) {
            ret = MSP_ERR_NULL_PARENT;
            goto out;
        }
        if (parent < 0 || parent >= (node_id_t) self->nodes->num_rows) {
            ret = MSP_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        sort_buff[j].second = time[parent];
        sort_buff[j].third = parent;
        sort_buff[j].fourth = self->edges->child[j];
    }
    qsort(sort_buff, self->edges->num_rows, sizeof(index_sort_t), cmp_index_sort);
    for (j = 0; j < self->edges->num_rows; j++) {
        self->indexes.edge_insertion_order[j] = sort_buff[j].index;
    }
    /* sort by right and decreasing parent time to give us the order in which
     * records should be removed. */
    for (j = 0; j < self->edges->num_rows; j++) {
        sort_buff[j].index = (node_id_t ) j;
        sort_buff[j].first = self->edges->right[j];
        parent = self->edges->parent[j];
        if (parent == MSP_NULL_NODE) {
            ret = MSP_ERR_NULL_PARENT;
            goto out;
        }
        if (parent < 0 || parent >= (node_id_t) self->nodes->num_rows) {
            ret = MSP_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        sort_buff[j].second = -time[parent];
        sort_buff[j].third = -parent;
        sort_buff[j].fourth = -self->edges->child[j];
    }
    qsort(sort_buff, self->edges->num_rows, sizeof(index_sort_t), cmp_index_sort);
    for (j = 0; j < self->edges->num_rows; j++) {
        self->indexes.edge_removal_order[j] = sort_buff[j].index;
    }
    ret = 0;
out:
    if (sort_buff != NULL) {
        free(sort_buff);
    }
    return ret;
}

static int WARN_UNUSED
table_collection_read_format_data(table_collection_t *self)
{
    int ret = 0;
    size_t len;
    uint32_t *version;
    int8_t *format_name, *uuid;
    double *L;

    ret = kastore_gets_int8(self->store, "format/name", &format_name, &len);
    if (ret != 0) {
        ret = msp_set_kas_error(ret);
        goto out;
    }
    if (len != MSP_FILE_FORMAT_NAME_LENGTH) {
        ret = MSP_ERR_FILE_FORMAT;
        goto out;
    }
    if (memcmp(MSP_FILE_FORMAT_NAME, format_name, MSP_FILE_FORMAT_NAME_LENGTH) != 0) {
        ret = MSP_ERR_FILE_FORMAT;
        goto out;
    }

    ret = kastore_gets_uint32(self->store, "format/version", &version, &len);
    if (ret != 0) {
        ret = msp_set_kas_error(ret);
        goto out;
    }
    if (len != 2) {
        ret = MSP_ERR_FILE_FORMAT;
        goto out;
    }
    if (version[0] < MSP_FILE_FORMAT_VERSION_MAJOR) {
        ret = MSP_ERR_FILE_VERSION_TOO_OLD;
        goto out;
    }
    if (version[0] > MSP_FILE_FORMAT_VERSION_MAJOR) {
        ret = MSP_ERR_FILE_VERSION_TOO_NEW;
        goto out;
    }

    ret = kastore_gets_float64(self->store, "sequence_length", &L, &len);
    if (ret != 0) {
        ret = msp_set_kas_error(ret);
        goto out;
    }
    if (len != 1) {
        ret = MSP_ERR_FILE_FORMAT;
        goto out;
    }
    if (L[0] <= 0.0) {
        ret = MSP_ERR_BAD_SEQUENCE_LENGTH;
        goto out;
    }
    self->sequence_length = L[0];

    ret = kastore_gets_int8(self->store, "uuid", &uuid, &len);
    if (ret != 0) {
        ret = msp_set_kas_error(ret);
        goto out;
    }
    if (len != TSK_UUID_SIZE) {
        ret = MSP_ERR_FILE_FORMAT;
        goto out;
    }

    /* Allow space for \0 so we can print it as a string */
    self->file_uuid = malloc(TSK_UUID_SIZE + 1);
    if (self->file_uuid == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    memcpy(self->file_uuid, uuid, TSK_UUID_SIZE);
    self->file_uuid[TSK_UUID_SIZE] = '\0';
out:
    return ret;
}

static int WARN_UNUSED
table_collection_dump_indexes(table_collection_t *self, kastore_t *store)
{
    int ret = 0;
    write_table_col_t write_cols[] = {
        {"indexes/edge_insertion_order", NULL, self->edges->num_rows, KAS_INT32},
        {"indexes/edge_removal_order", NULL, self->edges->num_rows, KAS_INT32},
    };

    if (! table_collection_is_indexed(self)) {
        ret = table_collection_build_indexes(self, 0);
        if (ret != 0) {
            goto out;
        }
    }
    write_cols[0].array = self->indexes.edge_insertion_order;
    write_cols[1].array = self->indexes.edge_removal_order;
    ret = write_table_cols(store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
out:
    return ret;
}

static int WARN_UNUSED
table_collection_load_indexes(table_collection_t *self)
{
    read_table_col_t read_cols[] = {
        {"indexes/edge_insertion_order", (void **) &self->indexes.edge_insertion_order,
            &self->edges->num_rows, 0, KAS_INT32},
        {"indexes/edge_removal_order", (void **) &self->indexes.edge_removal_order,
            &self->edges->num_rows, 0, KAS_INT32},
    };
    self->indexes.malloced_locally = false;
    return read_table_cols(self->store, read_cols, sizeof(read_cols) / sizeof(*read_cols));
}

int WARN_UNUSED
table_collection_load(table_collection_t *self, const char *filename, int MSP_UNUSED(flags))
{
    int ret = 0;

    assert(self->individuals != NULL);
    self->store = calloc(1, sizeof(*self->store));
    if (self->store == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    /* mmaping is inherently unsafe in terms of changes to the underlying file.
     * Without a great deal of extra effort catching SIGBUS here and transforming
     * it into an error return value, we can't be sure that this function won't
     * abort. Therefore, use the simple 'read in everything once' mode */
    ret = kastore_open(self->store, filename, "r", KAS_NO_MMAP);
    if (ret != 0) {
        ret = msp_set_kas_error(ret);
        goto out;
    }
    ret = table_collection_read_format_data(self);
    if (ret != 0) {
        goto out;
    }
    ret = node_table_load(self->nodes, self->store);
    if (ret != 0) {
        goto out;
    }
    ret = edge_table_load(self->edges, self->store);
    if (ret != 0) {
        goto out;
    }
    ret = site_table_load(self->sites, self->store);
    if (ret != 0) {
        goto out;
    }
    ret = mutation_table_load(self->mutations, self->store);
    if (ret != 0) {
        goto out;
    }
    ret = migration_table_load(self->migrations, self->store);
    if (ret != 0) {
        goto out;
    }
    ret = individual_table_load(self->individuals, self->store);
    if (ret != 0) {
        goto out;
    }
    ret = population_table_load(self->populations, self->store);
    if (ret != 0) {
        goto out;
    }
    ret = provenance_table_load(self->provenances, self->store);
    if (ret != 0) {
        goto out;
    }
    ret = table_collection_load_indexes(self);
    if (ret != 0) {
        goto out;
    }
    ret = table_collection_check_offsets(self);
out:
    return ret;
}

static int WARN_UNUSED
table_collection_write_format_data(table_collection_t *self, kastore_t *store)
{
    int ret = 0;
    char format_name[MSP_FILE_FORMAT_NAME_LENGTH];
    char uuid[TSK_UUID_SIZE + 1]; // Must include space for trailing null.
    uint32_t version[2] = {
        MSP_FILE_FORMAT_VERSION_MAJOR, MSP_FILE_FORMAT_VERSION_MINOR};
    write_table_col_t write_cols[] = {
        {"format/name", (void *) format_name, sizeof(format_name), KAS_INT8},
        {"format/version", (void *) version, 2, KAS_UINT32},
        {"sequence_length", (void *) &self->sequence_length, 1, KAS_FLOAT64},
        {"uuid", (void *) uuid, TSK_UUID_SIZE, KAS_INT8},
    };

    ret = tsk_generate_uuid(uuid, 0);
    if (ret != 0) {
        goto out;
    }
    /* This stupid dance is to workaround the fact that compilers won't allow
     * casts to discard the 'const' qualifier. */
    memcpy(format_name, MSP_FILE_FORMAT_NAME, sizeof(format_name));
    ret = write_table_cols(store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
out:
    return ret;
}

int WARN_UNUSED
table_collection_dump(table_collection_t *self, const char *filename, int MSP_UNUSED(flags))
{
    int ret = 0;
    kastore_t store;

    ret = kastore_open(&store, filename, "w", 0);
    if (ret != 0) {
        ret = msp_set_kas_error(ret);
        goto out;
    }
    ret = table_collection_write_format_data(self, &store);
    if (ret != 0) {
        goto out;
    }
    ret = node_table_dump(self->nodes, &store);
    if (ret != 0) {
        goto out;
    }
    ret = edge_table_dump(self->edges, &store);
    if (ret != 0) {
        goto out;
    }
    ret = site_table_dump(self->sites, &store);
    if (ret != 0) {
        goto out;
    }
    ret = migration_table_dump(self->migrations, &store);
    if (ret != 0) {
        goto out;
    }
    ret = mutation_table_dump(self->mutations, &store);
    if (ret != 0) {
        goto out;
    }
    ret = individual_table_dump(self->individuals, &store);
    if (ret != 0) {
        goto out;
    }
    ret = population_table_dump(self->populations, &store);
    if (ret != 0) {
        goto out;
    }
    ret = provenance_table_dump(self->provenances, &store);
    if (ret != 0) {
        goto out;
    }
    ret = table_collection_dump_indexes(self, &store);
    if (ret != 0) {
        goto out;
    }
    ret = kastore_close(&store);
out:
    if (ret != 0) {
        kastore_close(&store);
    }
    return ret;
}

int WARN_UNUSED
table_collection_simplify(table_collection_t *self,
        node_id_t *samples, size_t num_samples, int flags, node_id_t *node_map)
{
    int ret = 0;
    simplifier_t simplifier;

    ret = simplifier_alloc(&simplifier, samples, num_samples, self, flags);
    if (ret != 0) {
        goto out;
    }
    ret = simplifier_run(&simplifier, node_map);
    if (ret != 0) {
        goto out;
    }
    /* The indexes are invalidated now so drop them */
    ret = table_collection_drop_indexes(self);
out:
    simplifier_free(&simplifier);
    return ret;
}

int WARN_UNUSED
table_collection_sort(table_collection_t *self, size_t edge_start, int flags)
{
    int ret = 0;
    table_sorter_t sorter;

    ret = table_sorter_alloc(&sorter, self, flags);
    if (ret != 0) {
        goto out;
    }
    ret = table_sorter_run(&sorter, edge_start);
    if (ret != 0) {
        goto out;
    }
    /* The indexes are invalidated now so drop them */
    ret = table_collection_drop_indexes(self);
out:
    table_sorter_free(&sorter);
    return ret;
}

/*
 * Remove any sites with duplicate positions, retaining only the *first*
 * one. Assumes the tables have been sorted, throwing an error if not.
 */
int WARN_UNUSED
table_collection_deduplicate_sites(table_collection_t *self, int MSP_UNUSED(flags))
{
    int ret = 0;
    table_size_t j;
    /* Map of old site IDs to new site IDs. */
    site_id_t *site_id_map = NULL;
    site_table_t copy;
    site_t row, last_row;

    /* Must allocate the site table first for site_table_free to be safe */
    ret = site_table_alloc(&copy, 0, 0, 0);
    if (ret != 0) {
        goto out;
    }
    /* Check everything except site duplicates (which we expect) and
     * edge indexes (which we don't use) */
    ret = table_collection_check_integrity(self,
            MSP_CHECK_ALL & ~MSP_CHECK_SITE_DUPLICATES & ~MSP_CHECK_INDEXES);
    if (ret != 0) {
        goto out;
    }

    ret = site_table_copy(self->sites, &copy);
    if (ret != 0) {
        goto out;
    }
    site_id_map = malloc(copy.num_rows * sizeof(*site_id_map));
    if (site_id_map == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    ret = site_table_clear(self->sites);
    if (ret != 0) {
        goto out;
    }

    last_row.position = -1;
    site_id_map[0] = 0;
    for (j = 0; j < copy.num_rows; j++) {
        ret = site_table_get_row(&copy, j, &row);
        if (ret != 0) {
            goto out;
        }
        if (row.position != last_row.position) {
            ret = site_table_add_row(self->sites, row.position, row.ancestral_state,
                row.ancestral_state_length, row.metadata, row.metadata_length);
            if (ret < 0) {
                goto out;
            }
        }
        site_id_map[j] = (site_id_t) self->sites->num_rows - 1;
        last_row = row;
    }

    if (self->sites->num_rows < copy.num_rows) {
        // Remap sites in the mutation table
        // (but only if there's been any changed sites)
        for (j = 0; j < self->mutations->num_rows; j++) {
            self->mutations->site[j] = site_id_map[self->mutations->site[j]];
        }
    }
    ret = 0;
out:
    site_table_free(&copy);
    msp_safe_free(site_id_map);
    return ret;
}

int WARN_UNUSED
table_collection_compute_mutation_parents(table_collection_t *self, int MSP_UNUSED(flags))
{
    int ret = 0;
    const edge_id_t *I, *O;
    const edge_table_t edges = *self->edges;
    const node_table_t nodes = *self->nodes;
    const site_table_t sites = *self->sites;
    const mutation_table_t mutations = *self->mutations;
    const edge_id_t M = (edge_id_t) edges.num_rows;
    edge_id_t tj, tk;
    node_id_t *parent = NULL;
    mutation_id_t *bottom_mutation = NULL;
    node_id_t u;
    double left, right;
    site_id_t site;
    /* Using unsigned values here avoids potentially undefined behaviour */
    uint32_t j, mutation, first_mutation;

    /* Note that because we check everything here, any non-null mutation parents
     * will also be checked, even though they are about to be overwritten. To
     * ensure that his function always succeeds we must ensure that the
     * parent field is set to -1 first. */
    ret = table_collection_check_integrity(self, MSP_CHECK_ALL);
    if (ret != 0) {
        goto out;
    }
    parent = malloc(nodes.num_rows * sizeof(*parent));
    bottom_mutation = malloc(nodes.num_rows * sizeof(*bottom_mutation));
    if (parent == NULL || bottom_mutation == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    memset(parent, 0xff, nodes.num_rows * sizeof(*parent));
    memset(bottom_mutation, 0xff, nodes.num_rows * sizeof(*bottom_mutation));
    memset(mutations.parent, 0xff, self->mutations->num_rows * sizeof(mutation_id_t));

    I = self->indexes.edge_insertion_order;
    O = self->indexes.edge_removal_order;
    tj = 0;
    tk = 0;
    site = 0;
    mutation = 0;
    left = 0;
    while (tj < M || left < self->sequence_length) {
        while (tk < M && edges.right[O[tk]] == left) {
            parent[edges.child[O[tk]]] = MSP_NULL_NODE;
            tk++;
        }
        while (tj < M && edges.left[I[tj]] == left) {
            parent[edges.child[I[tj]]] = edges.parent[I[tj]];
            tj++;
        }
        right = self->sequence_length;
        if (tj < M) {
            right = MSP_MIN(right, edges.left[I[tj]]);
        }
        if (tk < M) {
            right = MSP_MIN(right, edges.right[O[tk]]);
        }

        /* Tree is now ready. We look at each site on this tree in turn */
        while (site < (site_id_t) sites.num_rows && sites.position[site] < right) {
            /* Create a mapping from mutations to nodes. If we see more than one
             * mutation at a node, the previously seen one must be the parent
             * of the current since we assume they are in order. */
            first_mutation = mutation;
            while (mutation < mutations.num_rows && mutations.site[mutation] == site) {
                u = mutations.node[mutation];
                if (bottom_mutation[u] != MSP_NULL_MUTATION) {
                    mutations.parent[mutation] = bottom_mutation[u];
                }
                bottom_mutation[u] = (mutation_id_t) mutation;
                mutation++;
            }
            /* Make the common case of 1 mutation fast */
            if (mutation > first_mutation + 1) {
                /* If we have more than one mutation, compute the parent for each
                 * one by traversing up the tree until we find a node that has a
                 * mutation. */
                for (j = first_mutation; j < mutation; j++) {
                    if (mutations.parent[j] == MSP_NULL_MUTATION) {
                        u = parent[mutations.node[j]];
                        while (u != MSP_NULL_NODE
                                && bottom_mutation[u] == MSP_NULL_MUTATION) {
                            u = parent[u];
                        }
                        if (u != MSP_NULL_NODE) {
                            mutations.parent[j] = bottom_mutation[u];
                        }
                    }
                }
            }
            /* Reset the mapping for the next site */
            for (j = first_mutation; j < mutation; j++) {
                u = mutations.node[j];
                bottom_mutation[u] = MSP_NULL_MUTATION;
                /* Check that we haven't violated the sortedness property */
                if (mutations.parent[j] > (mutation_id_t) j) {
                    ret = MSP_ERR_MUTATION_PARENT_AFTER_CHILD;
                    goto out;
                }
            }
            site++;
        }
        /* Move on to the next tree */
        left = right;
    }

out:
    msp_safe_free(parent);
    msp_safe_free(bottom_mutation);
    return ret;
}

/* Record the current "end" position of a table collection,
 * which is the current number of rows in each table.
 */
int
table_collection_record_position(table_collection_t *self,
        table_collection_position_t *position)
{
    position->individuals = self->individuals->num_rows;
    position->nodes = self->nodes->num_rows;
    position->edges = self->edges->num_rows;
    position->migrations = self->migrations->num_rows;
    position->sites = self->sites->num_rows;
    position->mutations = self->mutations->num_rows;
    position->populations = self->populations->num_rows;
    position->provenances = self->provenances->num_rows;
    return 0;
}

/* Reset to the previously recorded position. */
int WARN_UNUSED
table_collection_reset_position(table_collection_t *tables,
        table_collection_position_t *position)
{
    int ret = 0;

    ret = table_collection_drop_indexes(tables);
    if (ret != 0) {
        goto out;
    }
    ret = individual_table_truncate(tables->individuals, position->individuals);
    if (ret != 0) {
        goto out;
    }
    ret = node_table_truncate(tables->nodes, position->nodes);
    if (ret != 0) {
        goto out;
    }
    ret = edge_table_truncate(tables->edges, position->edges);
    if (ret != 0) {
        goto out;
    }
    ret = migration_table_truncate(tables->migrations, position->migrations);
    if (ret != 0) {
        goto out;
    }
    ret = site_table_truncate(tables->sites, position->sites);
    if (ret != 0) {
        goto out;
    }
    ret = mutation_table_truncate(tables->mutations, position->mutations);
    if (ret != 0) {
        goto out;
    }
    ret = population_table_truncate(tables->populations, position->populations);
    if (ret != 0) {
        goto out;
    }
    ret = provenance_table_truncate(tables->provenances, position->provenances);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

int WARN_UNUSED
table_collection_clear(table_collection_t *self)
{
    table_collection_position_t start;

    memset(&start, 0, sizeof(start));
    return table_collection_reset_position(self, &start);
}
