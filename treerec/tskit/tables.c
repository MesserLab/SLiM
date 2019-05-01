/*
 * MIT License
 *
 * Copyright (c) 2019 Tskit Developers
 * Copyright (c) 2017-2018 University of Oxford
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <float.h>

#include <tskit/tables.h>

/* This is a flag for tsk_table_collection_init used by tsk_table_collection_load to
 * avoid allocating the table columns. It's defined internally for now as it's
 * not clear how this would be useful outside of tskit. */
#define TSK_NO_INIT_TABLES (1 << 30)

#define DEFAULT_SIZE_INCREMENT 1024
#define TABLE_SEP "-----------------------------------------\n"

typedef struct {
    const char *name;
    void **array_dest;
    tsk_size_t *len_dest;
    tsk_size_t len_offset;
    int type;
} read_table_col_t;

typedef struct {
    const char *name;
    void *array;
    tsk_size_t len;
    int type;
} write_table_col_t;

/* Returns true if adding the specifiec number of rows would result in overflow.
 * Tables can support indexes from 0 to INT32_MAX, and therefore have at most
 * INT32_MAX + 1 rows */
static bool
check_overflow(tsk_size_t current_size, tsk_size_t additional_rows)
{
    size_t new_size = (size_t) current_size + (size_t) additional_rows;
    return new_size > ((size_t) INT32_MAX) + 1;
}

static int
read_table_cols(kastore_t *store, read_table_col_t *read_cols, size_t num_cols)
{
    int ret = 0;
    size_t len;
    int type;
    size_t j;
    tsk_size_t last_len;

    /* Set all the size destinations to -1 so we can detect the first time we
     * read it. Therefore, destinations that are supposed to have the same
     * length will take the value of the first instance, and we check each
     * subsequent value against this. */
    for (j = 0; j < num_cols; j++) {
        *read_cols[j].len_dest = (tsk_size_t) -1;
    }
    for (j = 0; j < num_cols; j++) {
        ret = kastore_gets(store, read_cols[j].name, read_cols[j].array_dest,
                &len, &type);
        if (ret != 0) {
            ret = tsk_set_kas_error(ret);
            goto out;
        }
        last_len = *read_cols[j].len_dest;
        if (last_len == (tsk_size_t) -1) {
            *read_cols[j].len_dest = (tsk_size_t) (len - read_cols[j].len_offset);
        } else if ((last_len + read_cols[j].len_offset) != (tsk_size_t) len) {
            ret = TSK_ERR_FILE_FORMAT;
            goto out;
        }
        if (type != read_cols[j].type) {
            ret = TSK_ERR_FILE_FORMAT;
            goto out;
        }
    }
out:
    return ret;
}


static int
write_table_cols(kastore_t *store, write_table_col_t *write_cols, size_t num_cols)
{
    int ret = 0;
    size_t j;

    for (j = 0; j < num_cols; j++) {
        ret = kastore_puts(store, write_cols[j].name, write_cols[j].array,
                write_cols[j].len, write_cols[j].type, 0);
        if (ret != 0) {
            ret = tsk_set_kas_error(ret);
            goto out;
        }
    }
out:
    return ret;
}

/* Checks that the specified list of offsets is well-formed. */
static int
check_offsets(size_t num_rows, tsk_size_t *offsets,
        tsk_size_t length, bool check_length)
{
    int ret = TSK_ERR_BAD_OFFSET;
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
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    *column = tmp;
out:
    return ret;
}

/*************************
 * individual table
 *************************/

static int
tsk_individual_table_expand_main_columns(tsk_individual_table_t *self,
        tsk_size_t additional_rows)
{
    int ret = 0;
    tsk_size_t increment = TSK_MAX(additional_rows, self->max_rows_increment);
    tsk_size_t new_size = self->max_rows + increment;

    if (check_overflow(self->max_rows, increment)) {
        ret = TSK_ERR_TABLE_OVERFLOW;
        goto out;
    }
    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column((void **) &self->flags, new_size, sizeof(tsk_flags_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->location_offset, new_size + 1,
                sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->metadata_offset, new_size + 1,
                sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        self->max_rows = new_size;
    }
out:
    return ret;
}

static int
tsk_individual_table_expand_location(tsk_individual_table_t *self, tsk_size_t additional_length)
{
    int ret = 0;
    tsk_size_t increment = TSK_MAX(additional_length, self->max_location_length_increment);
    tsk_size_t new_size = self->max_location_length + increment;

    if (check_overflow(self->location_length, increment)) {
        ret = TSK_ERR_COLUMN_OVERFLOW;
        goto out;
    }
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
tsk_individual_table_expand_metadata(tsk_individual_table_t *self, tsk_size_t additional_length)
{
    int ret = 0;
    tsk_size_t increment = TSK_MAX(additional_length,
            self->max_metadata_length_increment);
    tsk_size_t new_size = self->max_metadata_length + increment;

    if (check_overflow(self->metadata_length, increment)) {
        ret = TSK_ERR_COLUMN_OVERFLOW;
        goto out;
    }
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
tsk_individual_table_set_max_rows_increment(tsk_individual_table_t *self, tsk_size_t max_rows_increment)
{
    if (max_rows_increment == 0) {
        max_rows_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_rows_increment = (tsk_size_t) max_rows_increment;
    return 0;
}

int
tsk_individual_table_set_max_metadata_length_increment(tsk_individual_table_t *self,
        tsk_size_t max_metadata_length_increment)
{
    if (max_metadata_length_increment == 0) {
        max_metadata_length_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_metadata_length_increment = (tsk_size_t) max_metadata_length_increment;
    return 0;
}

int
tsk_individual_table_set_max_location_length_increment(tsk_individual_table_t *self,
        tsk_size_t max_location_length_increment)
{
    if (max_location_length_increment == 0) {
        max_location_length_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_location_length_increment = (tsk_size_t) max_location_length_increment;
    return 0;
}

int
tsk_individual_table_init(tsk_individual_table_t *self, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;

    memset(self, 0, sizeof(tsk_individual_table_t));
    /* Allocate space for one row initially, ensuring we always have valid pointers
     * even if the table is empty */
    self->max_rows_increment = 1;
    self->max_location_length_increment = 1;
    self->max_metadata_length_increment = 1;
    ret = tsk_individual_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_individual_table_expand_location(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->location_offset[0] = 0;
    ret = tsk_individual_table_expand_metadata(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->metadata_offset[0] = 0;
    self->max_rows_increment = DEFAULT_SIZE_INCREMENT;
    self->max_location_length_increment = DEFAULT_SIZE_INCREMENT;
    self->max_metadata_length_increment = DEFAULT_SIZE_INCREMENT;
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_individual_table_copy(tsk_individual_table_t *self, tsk_individual_table_t *dest,
        tsk_flags_t options)
{
    int ret = 0;

    if (! (options & TSK_NO_INIT)) {
        ret = tsk_individual_table_init(dest, 0);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_individual_table_set_columns(dest, self->num_rows, self->flags,
            self->location, self->location_offset, self->metadata, self->metadata_offset);
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_individual_table_set_columns(tsk_individual_table_t *self, tsk_size_t num_rows,
        tsk_flags_t *flags,
        double *location, tsk_size_t *location_offset,
        const char *metadata, tsk_size_t *metadata_offset)
{
    int ret;

    ret = tsk_individual_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_individual_table_append_columns(self, num_rows, flags, location, location_offset,
            metadata, metadata_offset);
out:
    return ret;
}

int
tsk_individual_table_append_columns(tsk_individual_table_t *self, tsk_size_t num_rows,
        tsk_flags_t *flags,
        double *location, tsk_size_t *location_offset,
        const char *metadata, tsk_size_t *metadata_offset)
{
    int ret;
    tsk_size_t j, metadata_length, location_length;

    if (flags == NULL) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    if ((location == NULL) != (location_offset == NULL)) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    if ((metadata == NULL) != (metadata_offset == NULL)) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    ret = tsk_individual_table_expand_main_columns(self, (tsk_size_t) num_rows);
    if (ret != 0) {
        goto out;
    }
    memcpy(self->flags + self->num_rows, flags, num_rows * sizeof(tsk_flags_t));
    if (location == NULL) {
        for (j = 0; j < num_rows; j++) {
            self->location_offset[self->num_rows + j + 1] = (tsk_size_t) self->location_length;
        }
    } else {
        ret = check_offsets(num_rows, location_offset, 0, false);
        if (ret != 0) {
            goto out;
        }
        for (j = 0; j < num_rows; j++) {
            self->location_offset[self->num_rows + j] =
                (tsk_size_t) self->location_length + location_offset[j];
        }
        location_length = location_offset[num_rows];
        ret = tsk_individual_table_expand_location(self, location_length);
        if (ret != 0) {
            goto out;
        }
        memcpy(self->location + self->location_length, location, location_length * sizeof(double));
        self->location_length += location_length;
    }
    if (metadata == NULL) {
        for (j = 0; j < num_rows; j++) {
            self->metadata_offset[self->num_rows + j + 1] = (tsk_size_t) self->metadata_length;
        }
    } else {
        ret = check_offsets(num_rows, metadata_offset, 0, false);
        if (ret != 0) {
            goto out;
        }
        for (j = 0; j < num_rows; j++) {
            self->metadata_offset[self->num_rows + j] =
                (tsk_size_t) self->metadata_length + metadata_offset[j];
        }
        metadata_length = metadata_offset[num_rows];
        ret = tsk_individual_table_expand_metadata(self, metadata_length);
        if (ret != 0) {
            goto out;
        }
        memcpy(self->metadata + self->metadata_length, metadata, metadata_length * sizeof(char));
        self->metadata_length += metadata_length;
    }
    self->num_rows += (tsk_size_t) num_rows;
    self->location_offset[self->num_rows] = self->location_length;
    self->metadata_offset[self->num_rows] = self->metadata_length;
out:
    return ret;
}

static tsk_id_t
tsk_individual_table_add_row_internal(tsk_individual_table_t *self, tsk_flags_t flags,
        double *location, tsk_size_t location_length,
        const char *metadata, tsk_size_t metadata_length)
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
    return (tsk_id_t) self->num_rows - 1;
}

tsk_id_t
tsk_individual_table_add_row(tsk_individual_table_t *self, tsk_flags_t flags,
        double *location, tsk_size_t location_length,
        const char *metadata, tsk_size_t metadata_length)
{
    int ret = 0;

    ret = tsk_individual_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_individual_table_expand_location(self, (tsk_size_t) location_length);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_individual_table_expand_metadata(self, (tsk_size_t) metadata_length);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_individual_table_add_row_internal(self, flags, location,
            (tsk_size_t) location_length, metadata, (tsk_size_t) metadata_length);
out:
    return ret;
}

int
tsk_individual_table_clear(tsk_individual_table_t *self)
{
    return tsk_individual_table_truncate(self, 0);
}

int
tsk_individual_table_truncate(tsk_individual_table_t *self, tsk_size_t num_rows)
{
    int ret = 0;

    if (num_rows > self->num_rows) {
        ret = TSK_ERR_BAD_TABLE_POSITION;
        goto out;
    }
    self->num_rows = num_rows;
    self->location_length = self->location_offset[num_rows];
    self->metadata_length = self->metadata_offset[num_rows];
out:
    return ret;
}

int
tsk_individual_table_free(tsk_individual_table_t *self)
{
    if (self->max_rows > 0) {
        tsk_safe_free(self->flags);
        tsk_safe_free(self->location);
        tsk_safe_free(self->location_offset);
        tsk_safe_free(self->metadata);
        tsk_safe_free(self->metadata_offset);
    }
    return 0;
}

void
tsk_individual_table_print_state(tsk_individual_table_t *self, FILE *out)
{
    tsk_size_t j, k;

    fprintf(out, TABLE_SEP);
    fprintf(out, "tsk_individual_tbl: %p:\n", (void *) self);
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
tsk_individual_table_get_row(tsk_individual_table_t *self, tsk_id_t index,
        tsk_individual_t *row)
{
    int ret = 0;

    if (index < 0 || index >= (tsk_id_t) self->num_rows) {
        ret = TSK_ERR_INDIVIDUAL_OUT_OF_BOUNDS;
        goto out;
    }
    row->id = (tsk_id_t) index;
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
tsk_individual_table_dump_text(tsk_individual_table_t *self, FILE *out)
{
    int ret = TSK_ERR_IO;
    tsk_size_t j, k;
    tsk_size_t metadata_len;
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
            err = fprintf(out, "%.*g", TSK_DBL_DECIMAL_DIG, self->location[k]);
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
tsk_individual_table_equals(tsk_individual_table_t *self, tsk_individual_table_t *other)
{
    bool ret = false;
    if (self->num_rows == other->num_rows
            && self->metadata_length == other->metadata_length) {
        ret = memcmp(self->flags, other->flags,
                    self->num_rows * sizeof(tsk_flags_t)) == 0
            && memcmp(self->location_offset, other->location_offset,
                    (self->num_rows + 1) * sizeof(tsk_size_t)) == 0
            && memcmp(self->location, other->location,
                    self->location_length * sizeof(double)) == 0
            && memcmp(self->metadata_offset, other->metadata_offset,
                    (self->num_rows + 1) * sizeof(tsk_size_t)) == 0
            && memcmp(self->metadata, other->metadata,
                    self->metadata_length * sizeof(char)) == 0;
    }
    return ret;
}

static int
tsk_individual_table_dump(tsk_individual_table_t *self, kastore_t *store)
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
tsk_individual_table_load(tsk_individual_table_t *self, kastore_t *store)
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
 * node table
 *************************/

static int
tsk_node_table_expand_main_columns(tsk_node_table_t *self, tsk_size_t additional_rows)
{
    int ret = 0;
    tsk_size_t increment = TSK_MAX(additional_rows, self->max_rows_increment);
    tsk_size_t new_size = self->max_rows + increment;

    if (check_overflow(self->max_rows, increment)) {
        ret = TSK_ERR_TABLE_OVERFLOW;
        goto out;
    }
    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column((void **) &self->flags, new_size, sizeof(tsk_flags_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->time, new_size, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->population, new_size, sizeof(tsk_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->individual, new_size, sizeof(tsk_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->metadata_offset, new_size + 1,
                sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        self->max_rows = new_size;
    }
out:
    return ret;
}

static int
tsk_node_table_expand_metadata(tsk_node_table_t *self, tsk_size_t additional_length)
{
    int ret = 0;
    tsk_size_t increment = TSK_MAX(additional_length,
            self->max_metadata_length_increment);
    tsk_size_t new_size = self->max_metadata_length + increment;

    if (check_overflow(self->metadata_length, increment)) {
        ret = TSK_ERR_COLUMN_OVERFLOW;
        goto out;
    }
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
tsk_node_table_set_max_rows_increment(tsk_node_table_t *self, tsk_size_t max_rows_increment)
{
    if (max_rows_increment == 0) {
        max_rows_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_rows_increment = max_rows_increment;
    return 0;
}

int
tsk_node_table_set_max_metadata_length_increment(tsk_node_table_t *self,
        tsk_size_t max_metadata_length_increment)
{
    if (max_metadata_length_increment == 0) {
        max_metadata_length_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_metadata_length_increment = max_metadata_length_increment;
    return 0;
}

int
tsk_node_table_init(tsk_node_table_t *self, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;

    memset(self, 0, sizeof(tsk_node_table_t));
    /* Allocate space for one row initially, ensuring we always have valid pointers
     * even if the table is empty */
    self->max_rows_increment = 1;
    self->max_metadata_length_increment = 1;
    ret = tsk_node_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_node_table_expand_metadata(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->metadata_offset[0] = 0;
    self->max_rows_increment = DEFAULT_SIZE_INCREMENT;
    self->max_metadata_length_increment = DEFAULT_SIZE_INCREMENT;
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_node_table_copy(tsk_node_table_t *self, tsk_node_table_t *dest, tsk_flags_t options)
{
    int ret = 0;

    if (! (options & TSK_NO_INIT)) {
        ret = tsk_node_table_init(dest, 0);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_node_table_set_columns(dest, self->num_rows, self->flags,
            self->time, self->population, self->individual,
            self->metadata, self->metadata_offset);
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_node_table_set_columns(tsk_node_table_t *self, tsk_size_t num_rows, tsk_flags_t *flags,
        double *time, tsk_id_t *population, tsk_id_t *individual,
        const char *metadata, tsk_size_t *metadata_offset)
{
    int ret;

    ret = tsk_node_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_node_table_append_columns(self, num_rows, flags, time, population, individual,
            metadata, metadata_offset);
out:
    return ret;
}

int
tsk_node_table_append_columns(tsk_node_table_t *self, tsk_size_t num_rows, tsk_flags_t *flags,
        double *time, tsk_id_t *population, tsk_id_t *individual,
        const char *metadata, tsk_size_t *metadata_offset)
{
    int ret;
    tsk_size_t j, metadata_length;

    if (flags == NULL || time == NULL) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    if ((metadata == NULL) != (metadata_offset == NULL)) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    ret = tsk_node_table_expand_main_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    memcpy(self->time + self->num_rows, time, num_rows * sizeof(double));
    memcpy(self->flags + self->num_rows, flags, num_rows * sizeof(tsk_flags_t));
    if (metadata == NULL) {
        for (j = 0; j < num_rows; j++) {
            self->metadata_offset[self->num_rows + j + 1] = self->metadata_length;
        }
    } else {
        ret = check_offsets(num_rows, metadata_offset, 0, false);
        if (ret != 0) {
            goto out;
        }
        for (j = 0; j < num_rows; j++) {
            self->metadata_offset[self->num_rows + j] =
                (tsk_size_t) self->metadata_length + metadata_offset[j];
        }
        metadata_length = metadata_offset[num_rows];
        ret = tsk_node_table_expand_metadata(self, metadata_length);
        if (ret != 0) {
            goto out;
        }
        memcpy(self->metadata + self->metadata_length, metadata, metadata_length * sizeof(char));
        self->metadata_length += metadata_length;
    }
    if (population == NULL) {
        /* Set population to NULL_POPULATION (-1) if not specified */
        memset(self->population + self->num_rows, 0xff,
                num_rows * sizeof(tsk_id_t));
    } else {
        memcpy(self->population + self->num_rows, population,
                num_rows * sizeof(tsk_id_t));
    }
    if (individual == NULL) {
        /* Set individual to NULL_INDIVIDUAL (-1) if not specified */
        memset(self->individual + self->num_rows, 0xff,
                num_rows * sizeof(tsk_id_t));
    } else {
        memcpy(self->individual + self->num_rows, individual,
                num_rows * sizeof(tsk_id_t));
    }
    self->num_rows += (tsk_size_t) num_rows;
    self->metadata_offset[self->num_rows] = self->metadata_length;
out:
    return ret;
}

static tsk_id_t
tsk_node_table_add_row_internal(tsk_node_table_t *self, tsk_flags_t flags, double time,
        tsk_id_t population, tsk_id_t individual,
        const char *metadata, tsk_size_t metadata_length)
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
    return (tsk_id_t) self->num_rows - 1;
}

tsk_id_t
tsk_node_table_add_row(tsk_node_table_t *self, tsk_flags_t flags, double time,
        tsk_id_t population, tsk_id_t individual,
        const char *metadata, tsk_size_t metadata_length)
{
    int ret = 0;

    ret = tsk_node_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_node_table_expand_metadata(self, metadata_length);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_node_table_add_row_internal(self, flags, time, population, individual,
            metadata, metadata_length);
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_node_table_clear(tsk_node_table_t *self)
{
    return tsk_node_table_truncate(self, 0);
}

int
tsk_node_table_truncate(tsk_node_table_t *self, tsk_size_t num_rows)
{
    int ret = 0;

    if (num_rows > self->num_rows) {
        ret = TSK_ERR_BAD_TABLE_POSITION;
        goto out;
    }
    self->num_rows = num_rows;
    self->metadata_length = self->metadata_offset[num_rows];
out:
    return ret;
}

int
tsk_node_table_free(tsk_node_table_t *self)
{
    if (self->max_rows > 0) {
        tsk_safe_free(self->flags);
        tsk_safe_free(self->time);
        tsk_safe_free(self->population);
        tsk_safe_free(self->individual);
        tsk_safe_free(self->metadata);
        tsk_safe_free(self->metadata_offset);
    }
    return 0;
}

void
tsk_node_table_print_state(tsk_node_table_t *self, FILE *out)
{
    size_t j, k;

    fprintf(out, TABLE_SEP);
    fprintf(out, "tsk_node_tbl: %p:\n", (void *) self);
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
tsk_node_table_dump_text(tsk_node_table_t *self, FILE *out)
{
    int ret = TSK_ERR_IO;
    size_t j;
    tsk_size_t metadata_len;
    int err;

    err = fprintf(out, "id\tis_sample\ttime\tpopulation\tindividual\tmetadata\n");
    if (err < 0) {
        goto out;
    }
    for (j = 0; j < self->num_rows; j++) {
        metadata_len = self->metadata_offset[j + 1] - self->metadata_offset[j];
        err = fprintf(out, "%d\t%d\t%f\t%d\t%d\t%.*s\n", (int) j,
                (int) (self->flags[j] & TSK_NODE_IS_SAMPLE),
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
tsk_node_table_equals(tsk_node_table_t *self, tsk_node_table_t *other)
{
    bool ret = false;
    if (self->num_rows == other->num_rows
            && self->metadata_length == other->metadata_length) {
        ret = memcmp(self->time, other->time,
                self->num_rows * sizeof(double)) == 0
            && memcmp(self->flags, other->flags,
                    self->num_rows * sizeof(tsk_flags_t)) == 0
            && memcmp(self->population, other->population,
                    self->num_rows * sizeof(tsk_id_t)) == 0
            && memcmp(self->individual, other->individual,
                    self->num_rows * sizeof(tsk_id_t)) == 0
            && memcmp(self->metadata_offset, other->metadata_offset,
                    (self->num_rows + 1) * sizeof(tsk_size_t)) == 0
            && memcmp(self->metadata, other->metadata,
                    self->metadata_length * sizeof(char)) == 0;
    }
    return ret;
}

int
tsk_node_table_get_row(tsk_node_table_t *self, tsk_id_t index, tsk_node_t *row)
{
    int ret = 0;
    if (index < 0 || index >= (tsk_id_t) self->num_rows) {
        ret = TSK_ERR_NODE_OUT_OF_BOUNDS;
        goto out;
    }
    row->id = (tsk_id_t) index;
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
tsk_node_table_dump(tsk_node_table_t *self, kastore_t *store)
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
tsk_node_table_load(tsk_node_table_t *self, kastore_t *store)
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
tsk_edge_table_expand_columns(tsk_edge_table_t *self, size_t additional_rows)
{
    int ret = 0;
    tsk_size_t increment = TSK_MAX(
        (tsk_size_t) additional_rows, self->max_rows_increment);
    tsk_size_t new_size = self->max_rows + increment;

    if (check_overflow(self->max_rows, increment)) {
        ret = TSK_ERR_TABLE_OVERFLOW;
        goto out;
    }
    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column((void **) &self->left, new_size, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->right, new_size, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->parent, new_size, sizeof(tsk_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->child, new_size, sizeof(tsk_id_t));
        if (ret != 0) {
            goto out;
        }
        self->max_rows = new_size;
    }
out:
    return ret;
}

int
tsk_edge_table_set_max_rows_increment(tsk_edge_table_t *self, tsk_size_t max_rows_increment)
{
    if (max_rows_increment == 0) {
        max_rows_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_rows_increment = max_rows_increment;
    return 0;
}

int
tsk_edge_table_init(tsk_edge_table_t *self, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;

    memset(self, 0, sizeof(tsk_edge_table_t));

    /* Allocate space for one row initially, ensuring we always have valid pointers
     * even if the table is empty */
    self->max_rows_increment = 1;
    ret = tsk_edge_table_expand_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->max_rows_increment = DEFAULT_SIZE_INCREMENT;
out:
    return ret;
}

tsk_id_t
tsk_edge_table_add_row(tsk_edge_table_t *self, double left, double right, tsk_id_t parent,
        tsk_id_t child)
{
    int ret = 0;

    ret = tsk_edge_table_expand_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->left[self->num_rows] = left;
    self->right[self->num_rows] = right;
    self->parent[self->num_rows] = parent;
    self->child[self->num_rows] = child;
    ret = (tsk_id_t) self->num_rows;
    self->num_rows++;
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_edge_table_copy(tsk_edge_table_t *self, tsk_edge_table_t *dest, tsk_flags_t options)
{
    int ret = 0;

    if (! (options & TSK_NO_INIT)) {
        ret = tsk_edge_table_init(dest, 0);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_edge_table_set_columns(dest, self->num_rows, self->left, self->right,
            self->parent, self->child);
out:
    return ret;
}

int
tsk_edge_table_set_columns(tsk_edge_table_t *self,
        tsk_size_t num_rows, double *left, double *right, tsk_id_t *parent, tsk_id_t *child)
{
    int ret = 0;

    ret = tsk_edge_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_edge_table_append_columns(self, num_rows, left, right, parent, child);
out:
    return ret;
}

int
tsk_edge_table_append_columns(tsk_edge_table_t *self,
        tsk_size_t num_rows, double *left, double *right, tsk_id_t *parent, tsk_id_t *child)
{
    int ret;

    if (left == NULL || right == NULL || parent == NULL || child == NULL) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    ret = tsk_edge_table_expand_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    memcpy(self->left + self->num_rows, left, num_rows * sizeof(double));
    memcpy(self->right + self->num_rows, right, num_rows * sizeof(double));
    memcpy(self->parent + self->num_rows, parent, num_rows * sizeof(tsk_id_t));
    memcpy(self->child + self->num_rows, child, num_rows * sizeof(tsk_id_t));
    self->num_rows += num_rows;
out:
    return ret;
}

int
tsk_edge_table_clear(tsk_edge_table_t *self)
{
    return tsk_edge_table_truncate(self, 0);
}

int
tsk_edge_table_truncate(tsk_edge_table_t *self, tsk_size_t num_rows)
{
    int ret = 0;

    if (num_rows > self->num_rows) {
        ret = TSK_ERR_BAD_TABLE_POSITION;
        goto out;
    }
    self->num_rows = num_rows;
out:
    return ret;
}

int
tsk_edge_table_free(tsk_edge_table_t *self)
{
    if (self->max_rows > 0) {
        tsk_safe_free(self->left);
        tsk_safe_free(self->right);
        tsk_safe_free(self->parent);
        tsk_safe_free(self->child);
    }
    return 0;
}

void
tsk_edge_table_print_state(tsk_edge_table_t *self, FILE *out)
{
    int ret;

    fprintf(out, TABLE_SEP);
    fprintf(out, "edge_table: %p:\n", (void *) self);
    fprintf(out, "num_rows          = %d\tmax= %d\tincrement = %d)\n",
            (int) self->num_rows, (int) self->max_rows, (int) self->max_rows_increment);
    fprintf(out, TABLE_SEP);
    ret = tsk_edge_table_dump_text(self, out);
    assert(ret == 0);
}

int
tsk_edge_table_dump_text(tsk_edge_table_t *self, FILE *out)
{
    tsk_size_t j;
    int ret = TSK_ERR_IO;
    int err;

    err = fprintf(out, "id\tleft\tright\tparent\tchild\n");
    if (err < 0) {
        goto out;
    }
    for (j = 0; j < self->num_rows; j++) {
        err = fprintf(out, "%d\t%.3f\t%.3f\t%d\t%d\n", j, self->left[j], self->right[j],
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
tsk_edge_table_equals(tsk_edge_table_t *self, tsk_edge_table_t *other)
{
    bool ret = false;
    if (self->num_rows == other->num_rows) {
        ret = memcmp(self->left, other->left,
                self->num_rows * sizeof(double)) == 0
            && memcmp(self->right, other->right,
                    self->num_rows * sizeof(double)) == 0
            && memcmp(self->parent, other->parent,
                    self->num_rows * sizeof(tsk_id_t)) == 0
            && memcmp(self->child, other->child,
                    self->num_rows * sizeof(tsk_id_t)) == 0;
    }
    return ret;
}

int
tsk_edge_table_get_row(tsk_edge_table_t *self, tsk_id_t index, tsk_edge_t *row)
{
    int ret = 0;
    if (index < 0 || index >= (tsk_id_t) self->num_rows) {
        ret = TSK_ERR_EDGE_OUT_OF_BOUNDS;
        goto out;
    }
    row->id = (tsk_id_t) index;
    row->left = self->left[index];
    row->right = self->right[index];
    row->parent = self->parent[index];
    row->child = self->child[index];
out:
    return ret;
}

static int
tsk_edge_table_dump(tsk_edge_table_t *self, kastore_t *store)
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
tsk_edge_table_load(tsk_edge_table_t *self, kastore_t *store)
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
tsk_site_table_expand_main_columns(tsk_site_table_t *self, tsk_size_t additional_rows)
{
    int ret = 0;
    tsk_size_t increment = TSK_MAX(additional_rows, self->max_rows_increment);
    tsk_size_t new_size = self->max_rows + increment;

    if (check_overflow(self->max_rows, increment)) {
        ret = TSK_ERR_TABLE_OVERFLOW;
        goto out;
    }
    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column((void **) &self->position, new_size, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->ancestral_state_offset, new_size + 1,
                sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->metadata_offset, new_size + 1,
                sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        self->max_rows = new_size;
    }
out:
    return ret;
}

static int
tsk_site_table_expand_ancestral_state(tsk_site_table_t *self, size_t additional_length)
{
    int ret = 0;
    tsk_size_t increment = (tsk_size_t) TSK_MAX(additional_length,
            self->max_ancestral_state_length_increment);
    tsk_size_t new_size = self->max_ancestral_state_length + increment;

    if (check_overflow(self->ancestral_state_length, increment)) {
        ret = TSK_ERR_COLUMN_OVERFLOW;
        goto out;
    }
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
tsk_site_table_expand_metadata(tsk_site_table_t *self, size_t additional_length)
{
    int ret = 0;
    tsk_size_t increment = (tsk_size_t) TSK_MAX(additional_length,
            self->max_metadata_length_increment);
    tsk_size_t new_size = self->max_metadata_length + increment;

    if (check_overflow(self->metadata_length, increment)) {
        ret = TSK_ERR_COLUMN_OVERFLOW;
        goto out;
    }
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
tsk_site_table_set_max_rows_increment(tsk_site_table_t *self, tsk_size_t max_rows_increment)
{
    if (max_rows_increment == 0) {
        max_rows_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_rows_increment = max_rows_increment;
    return 0;
}

int
tsk_site_table_set_max_metadata_length_increment(tsk_site_table_t *self,
        tsk_size_t max_metadata_length_increment)
{
    if (max_metadata_length_increment == 0) {
        max_metadata_length_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_metadata_length_increment = max_metadata_length_increment;
    return 0;
}

int
tsk_site_table_set_max_ancestral_state_length_increment(tsk_site_table_t *self,
        tsk_size_t max_ancestral_state_length_increment)
{
    if (max_ancestral_state_length_increment == 0) {
        max_ancestral_state_length_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_ancestral_state_length_increment = max_ancestral_state_length_increment;
    return 0;
}

int
tsk_site_table_init(tsk_site_table_t *self, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;

    memset(self, 0, sizeof(tsk_site_table_t));

    /* Allocate space for one row initially, ensuring we always have valid pointers
     * even if the table is empty */
    self->max_rows_increment = 1;
    self->max_ancestral_state_length_increment = 1;
    self->max_metadata_length_increment = 1;
    ret = tsk_site_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_site_table_expand_ancestral_state(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_site_table_expand_metadata(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->ancestral_state_offset[0] = 0;
    self->metadata_offset[0] = 0;
    self->max_rows_increment = DEFAULT_SIZE_INCREMENT;
    self->max_ancestral_state_length_increment = DEFAULT_SIZE_INCREMENT;
    self->max_metadata_length_increment = DEFAULT_SIZE_INCREMENT;
out:
    return ret;
}

tsk_id_t
tsk_site_table_add_row(tsk_site_table_t *self, double position,
        const char *ancestral_state, tsk_size_t ancestral_state_length,
        const char *metadata, tsk_size_t metadata_length)
{
    int ret = 0;
    tsk_size_t ancestral_state_offset, metadata_offset;

    ret = tsk_site_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->position[self->num_rows] = position;

    ancestral_state_offset = (tsk_size_t) self->ancestral_state_length;
    assert(self->ancestral_state_offset[self->num_rows] == ancestral_state_offset);
    ret = tsk_site_table_expand_ancestral_state(self, ancestral_state_length);
    if (ret != 0) {
        goto out;
    }
    self->ancestral_state_length += ancestral_state_length;
    memcpy(self->ancestral_state + ancestral_state_offset, ancestral_state,
            ancestral_state_length);
    self->ancestral_state_offset[self->num_rows + 1] = self->ancestral_state_length;

    metadata_offset = (tsk_size_t) self->metadata_length;
    assert(self->metadata_offset[self->num_rows] == metadata_offset);
    ret = tsk_site_table_expand_metadata(self, metadata_length);
    if (ret != 0) {
        goto out;
    }
    self->metadata_length += metadata_length;
    memcpy(self->metadata + metadata_offset, metadata, metadata_length);
    self->metadata_offset[self->num_rows + 1] = self->metadata_length;

    ret = (tsk_id_t) self->num_rows;
    self->num_rows++;
out:
    return ret;
}

int
tsk_site_table_append_columns(tsk_site_table_t *self, tsk_size_t num_rows, double *position,
        const char *ancestral_state, tsk_size_t *ancestral_state_offset,
        const char *metadata, tsk_size_t *metadata_offset)
{
    int ret = 0;
    tsk_size_t j, ancestral_state_length, metadata_length;

    if (position == NULL || ancestral_state == NULL || ancestral_state_offset == NULL) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    if ((metadata == NULL) != (metadata_offset == NULL)) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }

    ret = tsk_site_table_expand_main_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    memcpy(self->position + self->num_rows, position, num_rows * sizeof(double));

    /* Metadata column */
    if (metadata == NULL) {
        for (j = 0; j < num_rows; j++) {
            self->metadata_offset[self->num_rows + j + 1] = self->metadata_length;
        }
    } else {
        ret = check_offsets(num_rows, metadata_offset, 0, false);
        if (ret != 0) {
            goto out;
        }
        metadata_length = metadata_offset[num_rows];
        ret = tsk_site_table_expand_metadata(self, metadata_length);
        if (ret != 0) {
            goto out;
        }
        memcpy(self->metadata + self->metadata_length, metadata,
                metadata_length * sizeof(char));
        for (j = 0; j < num_rows; j++) {
            self->metadata_offset[self->num_rows + j] =
                self->metadata_length + metadata_offset[j];
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
    ret = tsk_site_table_expand_ancestral_state(self, ancestral_state_length);
    if (ret != 0) {
        goto out;
    }
    memcpy(self->ancestral_state + self->ancestral_state_length, ancestral_state,
            ancestral_state_length * sizeof(char));
    for (j = 0; j < num_rows; j++) {
        self->ancestral_state_offset[self->num_rows + j] =
            self->ancestral_state_length + ancestral_state_offset[j];
    }
    self->ancestral_state_length += ancestral_state_length;
    self->ancestral_state_offset[self->num_rows + num_rows] = self->ancestral_state_length;

    self->num_rows += num_rows;
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_site_table_copy(tsk_site_table_t *self, tsk_site_table_t *dest, tsk_flags_t options)
{
    int ret = 0;

    if (! (options & TSK_NO_INIT)) {
        ret = tsk_site_table_init(dest, 0);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_site_table_set_columns(dest, self->num_rows, self->position,
            self->ancestral_state, self->ancestral_state_offset,
            self->metadata, self->metadata_offset);
out:
    return ret;
}

int
tsk_site_table_set_columns(tsk_site_table_t *self, tsk_size_t num_rows, double *position,
        const char *ancestral_state, tsk_size_t *ancestral_state_offset,
        const char *metadata, tsk_size_t *metadata_offset)
{
    int ret = 0;

    ret = tsk_site_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_site_table_append_columns(self, num_rows, position, ancestral_state,
            ancestral_state_offset, metadata, metadata_offset);
out:
    return ret;
}

bool
tsk_site_table_equals(tsk_site_table_t *self, tsk_site_table_t *other)
{
    bool ret = false;
    if (self->num_rows == other->num_rows
            && self->ancestral_state_length == other->ancestral_state_length
            && self->metadata_length == other->metadata_length) {
        ret = memcmp(self->position, other->position,
                self->num_rows * sizeof(double)) == 0
            && memcmp(self->ancestral_state_offset, other->ancestral_state_offset,
                    (self->num_rows + 1) * sizeof(tsk_size_t)) == 0
            && memcmp(self->ancestral_state, other->ancestral_state,
                    self->ancestral_state_length * sizeof(char)) == 0
            && memcmp(self->metadata_offset, other->metadata_offset,
                    (self->num_rows + 1) * sizeof(tsk_size_t)) == 0
            && memcmp(self->metadata, other->metadata,
                    self->metadata_length * sizeof(char)) == 0;
    }
    return ret;
}

int
tsk_site_table_clear(tsk_site_table_t *self)
{
    return tsk_site_table_truncate(self, 0);
}

int
tsk_site_table_truncate(tsk_site_table_t *self, tsk_size_t num_rows)
{
    int ret = 0;

    if (num_rows > self->num_rows) {
        ret = TSK_ERR_BAD_TABLE_POSITION;
        goto out;
    }
    self->num_rows = num_rows;
    self->ancestral_state_length = self->ancestral_state_offset[num_rows];
    self->metadata_length = self->metadata_offset[num_rows];
out:
    return ret;
}

int
tsk_site_table_free(tsk_site_table_t *self)
{
    if (self->max_rows > 0) {
        tsk_safe_free(self->position);
        tsk_safe_free(self->ancestral_state);
        tsk_safe_free(self->ancestral_state_offset);
        tsk_safe_free(self->metadata);
        tsk_safe_free(self->metadata_offset);
    }
    return 0;
}

void
tsk_site_table_print_state(tsk_site_table_t *self, FILE *out)
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
    ret = tsk_site_table_dump_text(self, out);
    assert(ret == 0);

    assert(self->ancestral_state_offset[0] == 0);
    assert(self->ancestral_state_length
            == self->ancestral_state_offset[self->num_rows]);
    assert(self->metadata_offset[0] == 0);
    assert(self->metadata_length == self->metadata_offset[self->num_rows]);
}

int
tsk_site_table_get_row(tsk_site_table_t *self, tsk_id_t index, tsk_site_t *row)
{
    int ret = 0;

    if (index < 0 || index >= (tsk_id_t) self->num_rows) {
        ret = TSK_ERR_SITE_OUT_OF_BOUNDS;
        goto out;
    }
    row->id = (tsk_id_t) index;
    row->position = self->position[index];
    row->ancestral_state_length = self->ancestral_state_offset[index + 1]
        - self->ancestral_state_offset[index];
    row->ancestral_state = self->ancestral_state + self->ancestral_state_offset[index];
    row->metadata_length = self->metadata_offset[index + 1]
        - self->metadata_offset[index];
    row->metadata = self->metadata + self->metadata_offset[index];
    /* This struct has a placeholder for mutations. Probably should be separate
     * structs for this (tsk_site_table_row_t?) */
    row->mutations_length = 0;
    row->mutations = NULL;
out:
    return ret;
}

int
tsk_site_table_dump_text(tsk_site_table_t *self, FILE *out)
{
    size_t j;
    int ret = TSK_ERR_IO;
    int err;
    tsk_size_t ancestral_state_len, metadata_len;

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
tsk_site_table_dump(tsk_site_table_t *self, kastore_t *store)
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
tsk_site_table_load(tsk_site_table_t *self, kastore_t *store)
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
tsk_mutation_table_expand_main_columns(tsk_mutation_table_t *self, size_t additional_rows)
{
    int ret = 0;
    tsk_size_t increment = (tsk_size_t) TSK_MAX(additional_rows, self->max_rows_increment);
    tsk_size_t new_size = self->max_rows + increment;


    if (check_overflow(self->max_rows, increment)) {
        ret = TSK_ERR_TABLE_OVERFLOW;
        goto out;
    }
    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column((void **) &self->site, new_size, sizeof(tsk_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->node, new_size, sizeof(tsk_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->parent, new_size, sizeof(tsk_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->derived_state_offset, new_size + 1,
                sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->metadata_offset, new_size + 1,
                sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        self->max_rows = new_size;
    }
out:
    return ret;
}

static int
tsk_mutation_table_expand_derived_state(tsk_mutation_table_t *self, size_t additional_length)
{
    int ret = 0;
    tsk_size_t increment = (tsk_size_t) TSK_MAX(additional_length,
            self->max_derived_state_length_increment);
    tsk_size_t new_size = self->max_derived_state_length + increment;

    if (check_overflow(self->derived_state_length, increment)) {
        ret = TSK_ERR_COLUMN_OVERFLOW;
        goto out;
    }
    if ((self->derived_state_length + additional_length)
            > self->max_derived_state_length) {
        ret = expand_column((void **) &self->derived_state, new_size, sizeof(char));
        if (ret != 0) {
            goto out;
        }
        self->max_derived_state_length = (tsk_size_t) new_size;
    }
out:
    return ret;
}

static int
tsk_mutation_table_expand_metadata(tsk_mutation_table_t *self, size_t additional_length)
{
    int ret = 0;
    tsk_size_t increment = (tsk_size_t) TSK_MAX(additional_length,
            self->max_metadata_length_increment);
    tsk_size_t new_size = self->max_metadata_length + increment;

    if (check_overflow(self->metadata_length, increment)) {
        ret = TSK_ERR_COLUMN_OVERFLOW;
        goto out;
    }
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
tsk_mutation_table_set_max_rows_increment(tsk_mutation_table_t *self, tsk_size_t max_rows_increment)
{
    if (max_rows_increment == 0) {
        max_rows_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_rows_increment = max_rows_increment;
    return 0;
}

int
tsk_mutation_table_set_max_metadata_length_increment(tsk_mutation_table_t *self,
        tsk_size_t max_metadata_length_increment)
{
    if (max_metadata_length_increment == 0) {
        max_metadata_length_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_metadata_length_increment = max_metadata_length_increment;
    return 0;
}

int
tsk_mutation_table_set_max_derived_state_length_increment(tsk_mutation_table_t *self,
        tsk_size_t max_derived_state_length_increment)
{
    if (max_derived_state_length_increment == 0) {
        max_derived_state_length_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_derived_state_length_increment = max_derived_state_length_increment;
    return 0;
}

int
tsk_mutation_table_init(tsk_mutation_table_t *self, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;

    memset(self, 0, sizeof(tsk_mutation_table_t));

    /* Allocate space for one row initially, ensuring we always have valid pointers
     * even if the table is empty */
    self->max_rows_increment = 1;
    self->max_derived_state_length_increment = 1;
    self->max_metadata_length_increment = 1;
    ret = tsk_mutation_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_mutation_table_expand_derived_state(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_mutation_table_expand_metadata(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->derived_state_offset[0] = 0;
    self->metadata_offset[0] = 0;
    self->max_rows_increment = DEFAULT_SIZE_INCREMENT;
    self->max_derived_state_length_increment = DEFAULT_SIZE_INCREMENT;
    self->max_metadata_length_increment = DEFAULT_SIZE_INCREMENT;
out:
    return ret;
}

tsk_id_t
tsk_mutation_table_add_row(tsk_mutation_table_t *self, tsk_id_t site, tsk_id_t node,
        tsk_id_t parent,
        const char *derived_state, tsk_size_t derived_state_length,
        const char *metadata, tsk_size_t metadata_length)
{
    tsk_size_t derived_state_offset, metadata_offset;
    int ret;

    ret = tsk_mutation_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->site[self->num_rows] = site;
    self->node[self->num_rows] = node;
    self->parent[self->num_rows] = parent;

    derived_state_offset = self->derived_state_length;
    assert(self->derived_state_offset[self->num_rows] == derived_state_offset);
    ret = tsk_mutation_table_expand_derived_state(self, derived_state_length);
    if (ret != 0) {
        goto out;
    }
    self->derived_state_length += derived_state_length;
    memcpy(self->derived_state + derived_state_offset, derived_state,
            derived_state_length);
    self->derived_state_offset[self->num_rows + 1] = self->derived_state_length;

    metadata_offset = self->metadata_length;
    assert(self->metadata_offset[self->num_rows] == metadata_offset);
    ret = tsk_mutation_table_expand_metadata(self, metadata_length);
    if (ret != 0) {
        goto out;
    }
    self->metadata_length += metadata_length;
    memcpy(self->metadata + metadata_offset, metadata, metadata_length);
    self->metadata_offset[self->num_rows + 1] = self->metadata_length;

    ret = (tsk_id_t) self->num_rows;
    self->num_rows++;
out:
    return ret;
}

int
tsk_mutation_table_append_columns(tsk_mutation_table_t *self, tsk_size_t num_rows,
        tsk_id_t *site, tsk_id_t *node, tsk_id_t *parent,
        const char *derived_state, tsk_size_t *derived_state_offset,
        const char *metadata, tsk_size_t *metadata_offset)
{
    int ret = 0;
    tsk_size_t j, derived_state_length, metadata_length;

    if (site  == NULL || node == NULL || derived_state == NULL
            || derived_state_offset == NULL) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    if ((metadata == NULL) != (metadata_offset == NULL)) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }

    ret = tsk_mutation_table_expand_main_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    memcpy(self->site + self->num_rows, site, num_rows * sizeof(tsk_id_t));
    memcpy(self->node + self->num_rows, node, num_rows * sizeof(tsk_id_t));
    if (parent == NULL) {
        /* If parent is NULL, set all parents to the null mutation */
        memset(self->parent + self->num_rows, 0xff, num_rows * sizeof(tsk_id_t));
    } else {
        memcpy(self->parent + self->num_rows, parent, num_rows * sizeof(tsk_id_t));
    }

    /* Metadata column */
    if (metadata == NULL) {
        for (j = 0; j < num_rows; j++) {
            self->metadata_offset[self->num_rows + j + 1] = self->metadata_length;
        }
    } else {
        ret = check_offsets(num_rows, metadata_offset, 0, false);
        if (ret != 0) {
            goto out;
        }
        metadata_length = metadata_offset[num_rows];
        ret = tsk_mutation_table_expand_metadata(self, metadata_length);
        if (ret != 0) {
            goto out;
        }
        memcpy(self->metadata + self->metadata_length, metadata,
                metadata_length * sizeof(char));
        for (j = 0; j < num_rows; j++) {
            self->metadata_offset[self->num_rows + j] =
                self->metadata_length + metadata_offset[j];
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
    ret = tsk_mutation_table_expand_derived_state(self, derived_state_length);
    if (ret != 0) {
        goto out;
    }
    memcpy(self->derived_state + self->derived_state_length, derived_state,
            derived_state_length * sizeof(char));
    for (j = 0; j < num_rows; j++) {
        self->derived_state_offset[self->num_rows + j] =
            self->derived_state_length + derived_state_offset[j];
    }
    self->derived_state_length += derived_state_length;
    self->derived_state_offset[self->num_rows + num_rows] = self->derived_state_length;

    self->num_rows += num_rows;
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_mutation_table_copy(tsk_mutation_table_t *self, tsk_mutation_table_t *dest,
        tsk_flags_t options)
{
    int ret = 0;

    if (! (options & TSK_NO_INIT)) {
        ret = tsk_mutation_table_init(dest, 0);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_mutation_table_set_columns(dest, self->num_rows,
            self->site, self->node, self->parent,
            self->derived_state, self->derived_state_offset,
            self->metadata, self->metadata_offset);
out:
    return ret;
}

int
tsk_mutation_table_set_columns(tsk_mutation_table_t *self, tsk_size_t num_rows, tsk_id_t *site,
        tsk_id_t *node, tsk_id_t *parent,
        const char *derived_state, tsk_size_t *derived_state_offset,
        const char *metadata, tsk_size_t *metadata_offset)
{
    int ret = 0;

    ret = tsk_mutation_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_mutation_table_append_columns(self, num_rows, site, node, parent,
            derived_state, derived_state_offset, metadata, metadata_offset);
out:
    return ret;
}

bool
tsk_mutation_table_equals(tsk_mutation_table_t *self, tsk_mutation_table_t *other)
{
    bool ret = false;
    if (self->num_rows == other->num_rows
            && self->derived_state_length == other->derived_state_length
            && self->metadata_length == other->metadata_length) {
        ret = memcmp(self->site, other->site, self->num_rows * sizeof(tsk_id_t)) == 0
            && memcmp(self->node, other->node, self->num_rows * sizeof(tsk_id_t)) == 0
            && memcmp(self->parent, other->parent,
                    self->num_rows * sizeof(tsk_id_t)) == 0
            && memcmp(self->derived_state_offset, other->derived_state_offset,
                    (self->num_rows + 1) * sizeof(tsk_size_t)) == 0
            && memcmp(self->derived_state, other->derived_state,
                    self->derived_state_length * sizeof(char)) == 0
            && memcmp(self->metadata_offset, other->metadata_offset,
                    (self->num_rows + 1) * sizeof(tsk_size_t)) == 0
            && memcmp(self->metadata, other->metadata,
                    self->metadata_length * sizeof(char)) == 0;
    }
    return ret;
}

int
tsk_mutation_table_clear(tsk_mutation_table_t *self)
{
    return tsk_mutation_table_truncate(self, 0);
}

int
tsk_mutation_table_truncate(tsk_mutation_table_t *mutations, tsk_size_t num_rows)
{
    int ret = 0;

    if (num_rows > mutations->num_rows) {
        ret = TSK_ERR_BAD_TABLE_POSITION;
        goto out;
    }
    mutations->num_rows = num_rows;
    mutations->derived_state_length = mutations->derived_state_offset[num_rows];
    mutations->metadata_length = mutations->metadata_offset[num_rows];
out:
    return ret;
}

int
tsk_mutation_table_free(tsk_mutation_table_t *self)
{
    if (self->max_rows > 0) {
        tsk_safe_free(self->node);
        tsk_safe_free(self->site);
        tsk_safe_free(self->parent);
        tsk_safe_free(self->derived_state);
        tsk_safe_free(self->derived_state_offset);
        tsk_safe_free(self->metadata);
        tsk_safe_free(self->metadata_offset);
    }
    return 0;
}

void
tsk_mutation_table_print_state(tsk_mutation_table_t *self, FILE *out)
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
    ret = tsk_mutation_table_dump_text(self, out);
    assert(ret == 0);
    assert(self->derived_state_offset[0] == 0);
    assert(self->derived_state_length
            == self->derived_state_offset[self->num_rows]);
    assert(self->metadata_offset[0] == 0);
    assert(self->metadata_length
            == self->metadata_offset[self->num_rows]);
}

int
tsk_mutation_table_get_row(tsk_mutation_table_t *self, tsk_id_t index, tsk_mutation_t *row)
{
    int ret = 0;
    if (index < 0 || index >= (tsk_id_t) self->num_rows) {
        ret = TSK_ERR_MUTATION_OUT_OF_BOUNDS;
        goto out;
    }
    row->id = (tsk_id_t) index;
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
tsk_mutation_table_dump_text(tsk_mutation_table_t *self, FILE *out)
{
    int ret = TSK_ERR_IO;
    int err;
    tsk_size_t j, derived_state_len, metadata_len;

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
tsk_mutation_table_dump(tsk_mutation_table_t *self, kastore_t *store)
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
tsk_mutation_table_load(tsk_mutation_table_t *self, kastore_t *store)
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
tsk_migration_table_expand(tsk_migration_table_t *self, size_t additional_rows)
{
    int ret = 0;
    tsk_size_t increment = TSK_MAX(
            (tsk_size_t) additional_rows, self->max_rows_increment);
    tsk_size_t new_size = self->max_rows + increment;

    if (check_overflow(self->max_rows, increment)) {
        ret = TSK_ERR_TABLE_OVERFLOW;
        goto out;
    }
    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column((void **) &self->left, new_size, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->right, new_size, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->node, new_size, sizeof(tsk_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->source, new_size, sizeof(tsk_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->dest, new_size, sizeof(tsk_id_t));
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
tsk_migration_table_set_max_rows_increment(tsk_migration_table_t *self, tsk_size_t max_rows_increment)
{
    if (max_rows_increment == 0) {
        max_rows_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_rows_increment = max_rows_increment;
    return 0;
}

int
tsk_migration_table_init(tsk_migration_table_t *self, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;

    memset(self, 0, sizeof(tsk_migration_table_t));

    /* Allocate space for one row initially, ensuring we always have valid pointers
     * even if the table is empty */
    self->max_rows_increment = 1;
    ret = tsk_migration_table_expand(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->max_rows_increment = DEFAULT_SIZE_INCREMENT;
out:
    return ret;
}

int
tsk_migration_table_append_columns(tsk_migration_table_t *self, tsk_size_t num_rows, double *left,
        double *right, tsk_id_t *node, tsk_id_t *source, tsk_id_t *dest,
        double *time)
{
    int ret;

    ret = tsk_migration_table_expand(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    if (left == NULL || right == NULL || node == NULL || source == NULL
            || dest == NULL || time == NULL) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    memcpy(self->left + self->num_rows, left, num_rows * sizeof(double));
    memcpy(self->right + self->num_rows, right, num_rows * sizeof(double));
    memcpy(self->node + self->num_rows, node, num_rows * sizeof(tsk_id_t));
    memcpy(self->source + self->num_rows, source, num_rows * sizeof(tsk_id_t));
    memcpy(self->dest + self->num_rows, dest, num_rows * sizeof(tsk_id_t));
    memcpy(self->time + self->num_rows, time, num_rows * sizeof(double));
    self->num_rows += num_rows;
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_migration_table_copy(tsk_migration_table_t *self, tsk_migration_table_t *dest,
        tsk_flags_t options)
{
    int ret = 0;

    if (! (options & TSK_NO_INIT)) {
        ret = tsk_migration_table_init(dest, 0);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_migration_table_set_columns(dest, self->num_rows,
            self->left, self->right, self->node,
            self->source, self->dest, self->time);
out:
    return ret;
}

int
tsk_migration_table_set_columns(tsk_migration_table_t *self, tsk_size_t num_rows, double *left,
        double *right, tsk_id_t *node, tsk_id_t *source, tsk_id_t *dest,
        double *time)
{
    int ret;

    ret = tsk_migration_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_migration_table_append_columns(self, num_rows, left, right, node, source,
            dest, time);
out:
    return ret;
}

tsk_id_t
tsk_migration_table_add_row(tsk_migration_table_t *self, double left, double right,
        tsk_id_t node, tsk_id_t source, tsk_id_t dest, double time)
{
    int ret = 0;

    ret = tsk_migration_table_expand(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->left[self->num_rows] = left;
    self->right[self->num_rows] = right;
    self->node[self->num_rows] = node;
    self->source[self->num_rows] = source;
    self->dest[self->num_rows] = dest;
    self->time[self->num_rows] = time;
    ret = (tsk_id_t) self->num_rows;
    self->num_rows++;
out:
    return ret;
}

int
tsk_migration_table_clear(tsk_migration_table_t *self)
{
    return tsk_migration_table_truncate(self, 0);
}

int
tsk_migration_table_truncate(tsk_migration_table_t *self, tsk_size_t num_rows)
{
    int ret = 0;

    if (num_rows > self->num_rows) {
        ret = TSK_ERR_BAD_TABLE_POSITION;
        goto out;
    }
    self->num_rows = num_rows;
out:
    return ret;
}

int
tsk_migration_table_free(tsk_migration_table_t *self)
{
    if (self->max_rows > 0) {
        tsk_safe_free(self->left);
        tsk_safe_free(self->right);
        tsk_safe_free(self->node);
        tsk_safe_free(self->source);
        tsk_safe_free(self->dest);
        tsk_safe_free(self->time);
    }
    return 0;
}

void
tsk_migration_table_print_state(tsk_migration_table_t *self, FILE *out)
{
    int ret;

    fprintf(out, TABLE_SEP);
    fprintf(out, "migration_table: %p:\n", (void *) self);
    fprintf(out, "num_rows = %d\tmax= %d\tincrement = %d)\n",
            (int) self->num_rows, (int) self->max_rows, (int) self->max_rows_increment);
    fprintf(out, TABLE_SEP);
    ret = tsk_migration_table_dump_text(self, out);
    assert(ret == 0);
}

int
tsk_migration_table_get_row(tsk_migration_table_t *self, tsk_id_t index, tsk_migration_t *row)
{
    int ret = 0;
    if (index < 0 || index >= (tsk_id_t) self->num_rows) {
        ret = TSK_ERR_MIGRATION_OUT_OF_BOUNDS;
        goto out;
    }
    row->id = (tsk_id_t) index;
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
tsk_migration_table_dump_text(tsk_migration_table_t *self, FILE *out)
{
    size_t j;
    int ret = TSK_ERR_IO;
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
tsk_migration_table_equals(tsk_migration_table_t *self, tsk_migration_table_t *other)
{
    bool ret = false;
    if (self->num_rows == other->num_rows) {
        ret = memcmp(self->left, other->left,
                self->num_rows * sizeof(double)) == 0
            && memcmp(self->right, other->right,
                    self->num_rows * sizeof(double)) == 0
            && memcmp(self->node, other->node,
                    self->num_rows * sizeof(tsk_id_t)) == 0
            && memcmp(self->source, other->source,
                    self->num_rows * sizeof(tsk_id_t)) == 0
            && memcmp(self->dest, other->dest,
                    self->num_rows * sizeof(tsk_id_t)) == 0
            && memcmp(self->time, other->time,
                    self->num_rows * sizeof(double)) == 0;
    }
    return ret;
}

static int
tsk_migration_table_dump(tsk_migration_table_t *self, kastore_t *store)
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
tsk_migration_table_load(tsk_migration_table_t *self, kastore_t *store)
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
 * population table
 *************************/

static int
tsk_population_table_expand_main_columns(tsk_population_table_t *self, tsk_size_t additional_rows)
{
    int ret = 0;
    tsk_size_t increment = TSK_MAX(additional_rows, self->max_rows_increment);
    tsk_size_t new_size = self->max_rows + increment;

    if (check_overflow(self->max_rows, increment)) {
        ret = TSK_ERR_TABLE_OVERFLOW;
        goto out;
    }
    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column((void **) &self->metadata_offset, new_size + 1,
                sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        self->max_rows = new_size;
    }
out:
    return ret;
}

static int
tsk_population_table_expand_metadata(tsk_population_table_t *self, tsk_size_t additional_length)
{
    int ret = 0;
    tsk_size_t increment = TSK_MAX(additional_length,
            self->max_metadata_length_increment);
    tsk_size_t new_size = self->max_metadata_length + increment;

    if (check_overflow(self->metadata_length, increment)) {
        ret = TSK_ERR_COLUMN_OVERFLOW;
        goto out;
    }
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
tsk_population_table_set_max_rows_increment(tsk_population_table_t *self, tsk_size_t max_rows_increment)
{
    if (max_rows_increment == 0) {
        max_rows_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_rows_increment = max_rows_increment;
    return 0;
}

int
tsk_population_table_set_max_metadata_length_increment(tsk_population_table_t *self,
        tsk_size_t max_metadata_length_increment)
{
    if (max_metadata_length_increment == 0) {
        max_metadata_length_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_metadata_length_increment = max_metadata_length_increment;
    return 0;
}

int
tsk_population_table_init(tsk_population_table_t *self, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;

    memset(self, 0, sizeof(tsk_population_table_t));
    /* Allocate space for one row initially, ensuring we always have valid pointers
     * even if the table is empty */
    self->max_rows_increment = 1;
    self->max_metadata_length_increment = 1;
    ret = tsk_population_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_population_table_expand_metadata(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->metadata_offset[0] = 0;
    self->max_rows_increment = DEFAULT_SIZE_INCREMENT;
    self->max_metadata_length_increment = DEFAULT_SIZE_INCREMENT;
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_population_table_copy(tsk_population_table_t *self, tsk_population_table_t *dest,
        tsk_flags_t options)
{
    int ret = 0;

    if (! (options & TSK_NO_INIT)) {
        ret = tsk_population_table_init(dest, 0);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_population_table_set_columns(dest, self->num_rows,
            self->metadata, self->metadata_offset);
out:
    return ret;
}

int
tsk_population_table_set_columns(tsk_population_table_t *self, tsk_size_t num_rows,
        const char *metadata, tsk_size_t *metadata_offset)
{
    int ret;

    ret = tsk_population_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_population_table_append_columns(self, num_rows, metadata, metadata_offset);
out:
    return ret;
}

int
tsk_population_table_append_columns(tsk_population_table_t *self, tsk_size_t num_rows,
        const char *metadata, tsk_size_t *metadata_offset)
{
    int ret;
    tsk_size_t j, metadata_length;

    if (metadata == NULL || metadata_offset == NULL) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    ret = tsk_population_table_expand_main_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }

    ret = check_offsets(num_rows, metadata_offset, 0, false);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < num_rows; j++) {
        self->metadata_offset[self->num_rows + j] =
            self->metadata_length + metadata_offset[j];
    }
    metadata_length = metadata_offset[num_rows];
    ret = tsk_population_table_expand_metadata(self, metadata_length);
    if (ret != 0) {
        goto out;
    }
    memcpy(self->metadata + self->metadata_length, metadata,
            metadata_length * sizeof(char));
    self->metadata_length += metadata_length;

    self->num_rows += num_rows;
    self->metadata_offset[self->num_rows] = self->metadata_length;
out:
    return ret;
}

static tsk_id_t
tsk_population_table_add_row_internal(tsk_population_table_t *self,
        const char *metadata, tsk_size_t metadata_length)
{
    int ret = 0;

    assert(self->num_rows < self->max_rows);
    assert(self->metadata_length + metadata_length <= self->max_metadata_length);
    memcpy(self->metadata + self->metadata_length, metadata, metadata_length);
    self->metadata_offset[self->num_rows + 1] = self->metadata_length + metadata_length;
    self->metadata_length += metadata_length;
    ret = (tsk_id_t) self->num_rows;
    self->num_rows++;
    return ret;
}

tsk_id_t
tsk_population_table_add_row(tsk_population_table_t *self,
        const char *metadata, tsk_size_t metadata_length)
{
    int ret = 0;

    ret = tsk_population_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_population_table_expand_metadata(self, metadata_length);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_population_table_add_row_internal(self, metadata, metadata_length);
out:
    return ret;
}

int
tsk_population_table_clear(tsk_population_table_t *self)
{
    return tsk_population_table_truncate(self, 0);
}

int
tsk_population_table_truncate(tsk_population_table_t *self, tsk_size_t num_rows)
{
    int ret = 0;

    if (num_rows > self->num_rows) {
        ret = TSK_ERR_BAD_TABLE_POSITION;
        goto out;
    }
    self->num_rows = num_rows;
    self->metadata_length = self->metadata_offset[num_rows];
out:
    return ret;
}

int
tsk_population_table_free(tsk_population_table_t *self)
{
    if (self->max_rows > 0) {
        tsk_safe_free(self->metadata);
        tsk_safe_free(self->metadata_offset);
    }
    return 0;
}

void
tsk_population_table_print_state(tsk_population_table_t *self, FILE *out)
{
    tsk_size_t j, k;

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
tsk_population_table_get_row(tsk_population_table_t *self, tsk_id_t index, tsk_population_t *row)
{
    int ret = 0;

    if (index < 0 || index >= (tsk_id_t) self->num_rows) {
        ret = TSK_ERR_POPULATION_OUT_OF_BOUNDS;
        goto out;
    }
    row->id = (tsk_id_t) index;
    row->metadata_length = self->metadata_offset[index + 1]
        - self->metadata_offset[index];
    row->metadata = self->metadata + self->metadata_offset[index];
out:
    return ret;
}

int
tsk_population_table_dump_text(tsk_population_table_t *self, FILE *out)
{
    int ret = TSK_ERR_IO;
    int err;
    tsk_size_t j;
    tsk_size_t metadata_len;

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
tsk_population_table_equals(tsk_population_table_t *self, tsk_population_table_t *other)
{
    bool ret = false;
    if (self->num_rows == other->num_rows
            && self->metadata_length == other->metadata_length) {
        ret = memcmp(self->metadata_offset, other->metadata_offset,
                    (self->num_rows + 1) * sizeof(tsk_size_t)) == 0
            && memcmp(self->metadata, other->metadata,
                    self->metadata_length * sizeof(char)) == 0;
    }
    return ret;
}

static int
tsk_population_table_dump(tsk_population_table_t *self, kastore_t *store)
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
tsk_population_table_load(tsk_population_table_t *self, kastore_t *store)
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
tsk_provenance_table_expand_main_columns(tsk_provenance_table_t *self, tsk_size_t additional_rows)
{
    int ret = 0;
    tsk_size_t increment = TSK_MAX(additional_rows, self->max_rows_increment);
    tsk_size_t new_size = self->max_rows + increment;

    if (check_overflow(self->max_rows, increment)) {
        ret = TSK_ERR_TABLE_OVERFLOW;
        goto out;
    }
    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column((void **) &self->timestamp_offset, new_size + 1,
                sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->record_offset, new_size + 1,
                sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        self->max_rows = new_size;
    }
out:
    return ret;
}

static int
tsk_provenance_table_expand_timestamp(tsk_provenance_table_t *self, tsk_size_t additional_length)
{
    int ret = 0;
    tsk_size_t increment = TSK_MAX(additional_length,
            self->max_timestamp_length_increment);
    tsk_size_t new_size = self->max_timestamp_length + increment;

    if (check_overflow(self->timestamp_length, increment)) {
        ret = TSK_ERR_COLUMN_OVERFLOW;
        goto out;
    }
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
tsk_provenance_table_expand_provenance(tsk_provenance_table_t *self, tsk_size_t additional_length)
{
    int ret = 0;
    tsk_size_t increment = TSK_MAX(additional_length,
            self->max_record_length_increment);
    tsk_size_t new_size = self->max_record_length + increment;

    if (check_overflow(self->record_length, increment)) {
        ret = TSK_ERR_COLUMN_OVERFLOW;
        goto out;
    }
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
tsk_provenance_table_set_max_rows_increment(tsk_provenance_table_t *self, tsk_size_t max_rows_increment)
{
    if (max_rows_increment == 0) {
        max_rows_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_rows_increment = max_rows_increment;
    return 0;
}

int
tsk_provenance_table_set_max_timestamp_length_increment(tsk_provenance_table_t *self,
        tsk_size_t max_timestamp_length_increment)
{
    if (max_timestamp_length_increment == 0) {
        max_timestamp_length_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_timestamp_length_increment = max_timestamp_length_increment;
    return 0;
}

int
tsk_provenance_table_set_max_record_length_increment(tsk_provenance_table_t *self,
        tsk_size_t max_record_length_increment)
{
    if (max_record_length_increment == 0) {
        max_record_length_increment = DEFAULT_SIZE_INCREMENT;
    }
    self->max_record_length_increment = max_record_length_increment;
    return 0;
}

int
tsk_provenance_table_init(tsk_provenance_table_t *self, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;

    memset(self, 0, sizeof(tsk_provenance_table_t));
    /* Allocate space for one row initially, ensuring we always have valid pointers
     * even if the table is empty */
    self->max_rows_increment = 1;
    self->max_timestamp_length_increment = 1;
    self->max_record_length_increment = 1;
    ret = tsk_provenance_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_provenance_table_expand_timestamp(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->timestamp_offset[0] = 0;
    ret = tsk_provenance_table_expand_provenance(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->record_offset[0] = 0;
    self->max_rows_increment = DEFAULT_SIZE_INCREMENT;
    self->max_timestamp_length_increment = DEFAULT_SIZE_INCREMENT;
    self->max_record_length_increment = DEFAULT_SIZE_INCREMENT;
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_provenance_table_copy(tsk_provenance_table_t *self, tsk_provenance_table_t *dest,
        tsk_flags_t options)
{
    int ret = 0;

    if (! (options & TSK_NO_INIT)) {
        ret = tsk_provenance_table_init(dest, 0);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_provenance_table_set_columns(dest, self->num_rows,
            self->timestamp, self->timestamp_offset,
            self->record, self->record_offset);
out:
    return ret;
}

int
tsk_provenance_table_set_columns(tsk_provenance_table_t *self, tsk_size_t num_rows,
        char *timestamp, tsk_size_t *timestamp_offset,
        char *record, tsk_size_t *record_offset)
{
    int ret;

    ret = tsk_provenance_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_provenance_table_append_columns(self, num_rows,
            timestamp, timestamp_offset, record, record_offset);
out:
    return ret;
}

int
tsk_provenance_table_append_columns(tsk_provenance_table_t *self, tsk_size_t num_rows,
        char *timestamp, tsk_size_t *timestamp_offset,
        char *record, tsk_size_t *record_offset)
{
    int ret;
    tsk_size_t j, timestamp_length, record_length;

    if (timestamp == NULL || timestamp_offset == NULL ||
            record == NULL || record_offset == NULL) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    ret = tsk_provenance_table_expand_main_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }

    ret = check_offsets(num_rows, timestamp_offset, 0, false);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < num_rows; j++) {
        self->timestamp_offset[self->num_rows + j] =
            self->timestamp_length + timestamp_offset[j];
    }
    timestamp_length = timestamp_offset[num_rows];
    ret = tsk_provenance_table_expand_timestamp(self, timestamp_length);
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
        self->record_offset[self->num_rows + j] = self->record_length + record_offset[j];
    }
    record_length = record_offset[num_rows];
    ret = tsk_provenance_table_expand_provenance(self, record_length);
    if (ret != 0) {
        goto out;
    }
    memcpy(self->record + self->record_length, record, record_length * sizeof(char));
    self->record_length += record_length;

    self->num_rows += num_rows;
    self->timestamp_offset[self->num_rows] = self->timestamp_length;
    self->record_offset[self->num_rows] = self->record_length;
out:
    return ret;
}

static tsk_id_t
tsk_provenance_table_add_row_internal(tsk_provenance_table_t *self,
        const char *timestamp, tsk_size_t timestamp_length,
        const char *record, tsk_size_t record_length)
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
    ret = (tsk_id_t) self->num_rows;
    self->num_rows++;
    return ret;
}

tsk_id_t
tsk_provenance_table_add_row(tsk_provenance_table_t *self,
        const char *timestamp, tsk_size_t timestamp_length,
        const char *record, tsk_size_t record_length)
{
    int ret = 0;

    ret = tsk_provenance_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_provenance_table_expand_timestamp(self, timestamp_length);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_provenance_table_expand_provenance(self, record_length);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_provenance_table_add_row_internal(self,
            timestamp, timestamp_length, record, record_length);
out:
    return ret;
}

int
tsk_provenance_table_clear(tsk_provenance_table_t *self)
{
    return tsk_provenance_table_truncate(self, 0);
}

int
tsk_provenance_table_truncate(tsk_provenance_table_t *self, tsk_size_t num_rows)
{
    int ret = 0;

    if (num_rows > self->num_rows) {
        ret = TSK_ERR_BAD_TABLE_POSITION;
        goto out;
    }
    self->num_rows = num_rows;
    self->timestamp_length = self->timestamp_offset[num_rows];
    self->record_length = self->record_offset[num_rows];
out:
    return ret;
}

int
tsk_provenance_table_free(tsk_provenance_table_t *self)
{
    if (self->max_rows > 0) {
        tsk_safe_free(self->timestamp);
        tsk_safe_free(self->timestamp_offset);
        tsk_safe_free(self->record);
        tsk_safe_free(self->record_offset);
    }
    return 0;
}

void
tsk_provenance_table_print_state(tsk_provenance_table_t *self, FILE *out)
{
    tsk_size_t j, k;

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
tsk_provenance_table_get_row(tsk_provenance_table_t *self, tsk_id_t index, tsk_provenance_t *row)
{
    int ret = 0;

    if (index < 0 || index >= (tsk_id_t) self->num_rows) {
        ret = TSK_ERR_PROVENANCE_OUT_OF_BOUNDS;
        goto out;
    }
    row->id = (tsk_id_t) index;
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
tsk_provenance_table_dump_text(tsk_provenance_table_t *self, FILE *out)
{
    int ret = TSK_ERR_IO;
    int err;
    tsk_size_t j, timestamp_len, record_len;

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
tsk_provenance_table_equals(tsk_provenance_table_t *self, tsk_provenance_table_t *other)
{
    bool ret = false;
    if (self->num_rows == other->num_rows
            && self->timestamp_length == other->timestamp_length) {
        ret = memcmp(self->timestamp_offset, other->timestamp_offset,
                    (self->num_rows + 1) * sizeof(tsk_size_t)) == 0
            && memcmp(self->timestamp, other->timestamp,
                    self->timestamp_length * sizeof(char)) == 0
            && memcmp(self->record_offset, other->record_offset,
                    (self->num_rows + 1) * sizeof(tsk_size_t)) == 0
            && memcmp(self->record, other->record,
                    self->record_length * sizeof(char)) == 0;
    }
    return ret;
}

static int
tsk_provenance_table_dump(tsk_provenance_table_t *self, kastore_t *store)
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
tsk_provenance_table_load(tsk_provenance_table_t *self, kastore_t *store)
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
    tsk_id_t parent;
    tsk_id_t child;
    double time;
} edge_sort_t;

typedef struct {
    /* Input tables. */
    tsk_node_table_t *nodes;
    tsk_edge_table_t *edges;
    tsk_site_table_t *sites;
    tsk_mutation_table_t *mutations;
    tsk_migration_table_t *migrations;
    /* Mapping from input site IDs to output site IDs */
    tsk_id_t *site_id_map;
} table_sorter_t;

static int
cmp_site(const void *a, const void *b) {
    const tsk_site_t *ia = (const tsk_site_t *) a;
    const tsk_site_t *ib = (const tsk_site_t *) b;
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
    const tsk_mutation_t *ia = (const tsk_mutation_t *) a;
    const tsk_mutation_t *ib = (const tsk_mutation_t *) b;
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
table_sorter_init(table_sorter_t *self, tsk_table_collection_t *tables,
        tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;

    memset(self, 0, sizeof(table_sorter_t));
    if (tables->migrations.num_rows != 0) {
        ret = TSK_ERR_SORT_MIGRATIONS_NOT_SUPPORTED;
        goto out;
    }
    ret = tsk_table_collection_check_integrity(tables, TSK_CHECK_OFFSETS);
    if (ret != 0) {
        goto out;
    }
    self->nodes = &tables->nodes;
    self->edges = &tables->edges;
    self->mutations = &tables->mutations;
    self->sites = &tables->sites;
    self->migrations = &tables->migrations;

    self->site_id_map = malloc(self->sites->num_rows * sizeof(tsk_id_t));
    if (self->site_id_map == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
out:
    return ret;
}

static int
table_sorter_sort_edges(table_sorter_t *self, tsk_size_t start)
{
    int ret = 0;
    edge_sort_t *e;
    tsk_size_t j, k;
    tsk_size_t n = self->edges->num_rows - start;
    edge_sort_t *sorted_edges = malloc(n * sizeof(*sorted_edges));

    if (sorted_edges == NULL) {
        ret = TSK_ERR_NO_MEMORY;
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
    tsk_safe_free(sorted_edges);
    return ret;
}

static int
table_sorter_sort_sites(table_sorter_t *self)
{
    int ret = 0;
    tsk_site_table_t copy;
    tsk_size_t j;
    tsk_size_t num_sites = self->sites->num_rows;
    tsk_site_t *sorted_sites = malloc(num_sites * sizeof(*sorted_sites));

    ret = tsk_site_table_copy(self->sites, &copy, 0);
    if (ret != 0) {
        goto out;
    }
    if (sorted_sites == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    for (j = 0; j < num_sites; j++) {
        ret = tsk_site_table_get_row(&copy, (tsk_id_t) j, sorted_sites + j);
        if (ret != 0) {
            goto out;
        }
    }

    /* Sort the sites by position */
    qsort(sorted_sites, self->sites->num_rows, sizeof(*sorted_sites), cmp_site);

    /* Build the mapping from old site IDs to new site IDs and copy back into the table */
    tsk_site_table_clear(self->sites);
    for (j = 0; j < num_sites; j++) {
        self->site_id_map[sorted_sites[j].id] = (tsk_id_t) j;
        ret = tsk_site_table_add_row(self->sites, sorted_sites[j].position,
                sorted_sites[j].ancestral_state, sorted_sites[j].ancestral_state_length,
                sorted_sites[j].metadata, sorted_sites[j].metadata_length);
        if (ret < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    tsk_safe_free(sorted_sites);
    tsk_site_table_free(&copy);
    return ret;
}

static int
table_sorter_sort_mutations(table_sorter_t *self)
{
    int ret = 0;
    tsk_size_t j;
    tsk_id_t parent, mapped_parent;
    tsk_size_t num_mutations = self->mutations->num_rows;
    tsk_mutation_table_t copy;
    tsk_mutation_t *sorted_mutations = malloc(num_mutations * sizeof(*sorted_mutations));
    tsk_id_t *mutation_id_map = malloc(num_mutations * sizeof(*mutation_id_map));

    ret = tsk_mutation_table_copy(self->mutations, &copy, 0);
    if (ret != 0) {
        goto out;
    }
    if (mutation_id_map == NULL || sorted_mutations == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }

    for (j = 0; j < num_mutations; j++) {
        ret = tsk_mutation_table_get_row(&copy, (tsk_id_t) j, sorted_mutations + j);
        if (ret != 0) {
            goto out;
        }
        sorted_mutations[j].site = self->site_id_map[sorted_mutations[j].site];
    }
    ret = tsk_mutation_table_clear(self->mutations);
    if (ret != 0) {
        goto out;
    }

    qsort(sorted_mutations, num_mutations, sizeof(*sorted_mutations), cmp_mutation);

    /* Make a first pass through the sorted mutations to build the ID map. */
    for (j = 0; j < num_mutations; j++) {
        mutation_id_map[sorted_mutations[j].id] = (tsk_id_t) j;
    }

    for (j = 0; j < num_mutations; j++) {
        mapped_parent = TSK_NULL;
        parent = sorted_mutations[j].parent;
        if (parent != TSK_NULL) {
            mapped_parent = mutation_id_map[parent];
        }
        ret = tsk_mutation_table_add_row(self->mutations,
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
    tsk_safe_free(mutation_id_map);
    tsk_safe_free(sorted_mutations);
    tsk_mutation_table_free(&copy);
    return ret;
}

static int
table_sorter_run(table_sorter_t *self, tsk_size_t edge_start)
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
    tsk_safe_free(self->site_id_map);
}


/*************************
 * segment overlapper
 *************************/

/* TODO: This should be renamed to tsk_segment_t when we move to tskit.
 * msprime can then #include this and also use it. msprime needs a different
 * segment definition, which it can continue to call 'segment_t' as it
 * doesn't export a C API. */
typedef struct _simplify_segment_t {
    double left;
    double right;
    struct _simplify_segment_t *next;
    tsk_id_t node;
} simplify_segment_t;

typedef struct _interval_list_t {
    double left;
    double right;
    struct _interval_list_t *next;
} interval_list_t;

typedef struct _mutation_id_list_t {
    tsk_id_t mutation;
    struct _mutation_id_list_t *next;
} mutation_id_list_t;

/* segment overlap finding algorithm */
typedef struct {
    /* The input segments. This buffer is sorted by the algorithm and we also
     * assume that there is space for an extra element at the end */
    simplify_segment_t *segments;
    size_t num_segments;
    size_t index;
    size_t num_overlapping;
    double left;
    double right;
    /* Output buffer */
    size_t max_overlapping;
    simplify_segment_t **overlapping;
} segment_overlapper_t;

typedef struct {
    tsk_id_t *samples;
    size_t num_samples;
    tsk_flags_t options;
    tsk_table_collection_t *tables;
    /* Keep a copy of the input tables */
    tsk_table_collection_t input_tables;
    /* State for topology */
    simplify_segment_t **ancestor_map_head;
    simplify_segment_t **ancestor_map_tail;
    tsk_id_t *node_id_map;
    bool *is_sample;
    /* Segments for a particular parent that are processed together */
    simplify_segment_t *segment_queue;
    size_t segment_queue_size;
    size_t max_segment_queue_size;
    segment_overlapper_t segment_overlapper;
    tsk_blkalloc_t segment_heap;
    /* Buffer for output edges. For each child we keep a linked list of
     * intervals, and also store the actual children that have been buffered. */
    tsk_blkalloc_t interval_list_heap;
    interval_list_t **child_edge_map_head;
    interval_list_t **child_edge_map_tail;
    tsk_id_t *buffered_children;
    size_t num_buffered_children;
    /* For each mutation, map its output node. */
    tsk_id_t *mutation_node_map;
    /* Map of input mutation IDs to output mutation IDs. */
    tsk_id_t *mutation_id_map;
    /* Map of input nodes to the list of input mutation IDs */
    mutation_id_list_t **node_mutation_list_map_head;
    mutation_id_list_t **node_mutation_list_map_tail;
    mutation_id_list_t *node_mutation_list_mem;
    /* When reducing topology, we need a map positions to their corresponding
     * sites.*/
    double *position_lookup;
} simplifier_t;

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

static int TSK_WARN_UNUSED
segment_overlapper_alloc(segment_overlapper_t *self)
{
    int ret = 0;

    memset(self, 0, sizeof(*self));
    self->max_overlapping = 8; /* Making sure we call realloc in tests */
    self->overlapping = malloc(self->max_overlapping * sizeof(*self->overlapping));
    if (self->overlapping == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
out:
    return ret;
}

static int
segment_overlapper_free(segment_overlapper_t *self)
{
    tsk_safe_free(self->overlapping);
    return 0;
}

/* Initialise the segment overlapper for use. Note that the segments
 * array must have space for num_segments + 1 elements!
 */
static int TSK_WARN_UNUSED
segment_overlapper_start(segment_overlapper_t *self, simplify_segment_t *segments,
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
            ret = TSK_ERR_NO_MEMORY;
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

static int TSK_WARN_UNUSED
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
            self->right = TSK_MIN(self->right, self->overlapping[j]->right);
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
                self->right = TSK_MIN(self->right, self->overlapping[j]->right);
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
    const tsk_id_t *ia = (const tsk_id_t *) a;
    const tsk_id_t *ib = (const tsk_id_t *) b;
    return (*ia > *ib) - (*ia < *ib);
}

static void
simplifier_check_state(simplifier_t *self)
{
    size_t j, k;
    simplify_segment_t *u;
    mutation_id_list_t *list_node;
    tsk_id_t site;
    interval_list_t *int_list;
    tsk_id_t child;
    double position, last_position;
    bool found;
    size_t num_intervals;

    for (j = 0; j < self->input_tables.nodes.num_rows; j++) {
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

    for (j = 0; j < self->input_tables.nodes.num_rows; j++) {
        last_position = -1;
        for (list_node = self->node_mutation_list_map_head[j]; list_node != NULL;
                list_node = list_node->next) {
            assert(self->input_tables.mutations.node[list_node->mutation] == (tsk_id_t) j);
            site = self->input_tables.mutations.site[list_node->mutation];
            position = self->input_tables.sites.position[site];
            assert(last_position <= position);
            last_position = position;
        }
    }

    /* check the buffered edges */
    for (j = 0; j < self->input_tables.nodes.num_rows; j++) {
        assert((self->child_edge_map_head[j] == NULL) ==
            (self->child_edge_map_tail[j] == NULL));
        if (self->child_edge_map_head[j] != NULL) {
            /* Make sure that the child is in our list */
            found = false;
            for (k = 0; k < self->num_buffered_children; k++) {
                if (self->buffered_children[k] == (tsk_id_t) j) {
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

static void
simplifier_print_state(simplifier_t *self, FILE *out)
{
    size_t j;
    simplify_segment_t *u;
    mutation_id_list_t *list_node;
    interval_list_t *int_list;
    tsk_id_t child;

    fprintf(out, "--simplifier state--\n");
    fprintf(out, "options:\n");
    fprintf(out, "\tfilter_unreferenced_sites: %d\n",
            !!(self->options & TSK_FILTER_SITES));
    fprintf(out, "\treduce_to_site_topology  : %d\n",
            !!(self->options & TSK_REDUCE_TO_SITE_TOPOLOGY));
    fprintf(out, "\tkeep_unary  : %d\n",
            !!(self->options & TSK_KEEP_UNARY));

    fprintf(out, "===\nInput tables\n==\n");
    tsk_table_collection_print_state(&self->input_tables, out);
    fprintf(out, "===\nOutput tables\n==\n");
    tsk_table_collection_print_state(self->tables, out);
    fprintf(out, "===\nmemory heaps\n==\n");
    fprintf(out, "segment_heap:\n");
    tsk_blkalloc_print_state(&self->segment_heap, out);
    fprintf(out, "interval_list_heap:\n");
    tsk_blkalloc_print_state(&self->interval_list_heap, out);
    fprintf(out, "===\nancestors\n==\n");
    for (j = 0; j < self->input_tables.nodes.num_rows; j++) {
        fprintf(out, "%d:\t", (int) j);
        print_segment_chain(self->ancestor_map_head[j], out);
        fprintf(out, "\n");
    }
    fprintf(out, "===\nnode_id map (input->output)\n==\n");
    for (j = 0; j < self->input_tables.nodes.num_rows; j++) {
        if (self->node_id_map[j] != TSK_NULL) {
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
    for (j = 0; j < self->input_tables.mutations.num_rows; j++) {
        fprintf(out, "%d\t-> %d\n", (int) j, self->mutation_node_map[j]);
    }
    fprintf(out, "===\nnode mutation id list map\n==\n");
    for (j = 0; j < self->input_tables.nodes.num_rows; j++) {
        if (self->node_mutation_list_map_head[j] != NULL) {
            fprintf(out, "%d\t-> [", (int) j);
            for (list_node = self->node_mutation_list_map_head[j]; list_node != NULL;
                    list_node = list_node->next) {
                fprintf(out, "%d,", list_node->mutation);
            }
            fprintf(out, "]\n");
        }
    }
    if (!!(self->options & TSK_REDUCE_TO_SITE_TOPOLOGY)) {
        fprintf(out, "===\nposition_lookup\n==\n");
        for (j = 0; j < self->input_tables.sites.num_rows + 2; j++) {
            fprintf(out, "%d\t-> %f\n", (int) j, self->position_lookup[j]);
        }
    }
    simplifier_check_state(self);
}

static simplify_segment_t * TSK_WARN_UNUSED
simplifier_alloc_segment(simplifier_t *self, double left, double right, tsk_id_t node)
{
    simplify_segment_t *seg = NULL;

    seg = tsk_blkalloc_get(&self->segment_heap, sizeof(*seg));
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

static interval_list_t * TSK_WARN_UNUSED
simplifier_alloc_interval_list(simplifier_t *self, double left, double right)
{
    interval_list_t *x = NULL;

    x = tsk_blkalloc_get(&self->interval_list_heap, sizeof(*x));
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
static int TSK_WARN_UNUSED
simplifier_record_node(simplifier_t *self, tsk_id_t input_id, bool is_sample)
{
    int ret = 0;
    tsk_node_t node;
    tsk_flags_t flags;

    ret = tsk_node_table_get_row(&self->input_tables.nodes, (tsk_id_t) input_id, &node);
    if (ret != 0) {
        goto out;
    }
    /* Zero out the sample bit */
    flags = node.flags & (tsk_flags_t) ~TSK_NODE_IS_SAMPLE;
    if (is_sample) {
        flags |= TSK_NODE_IS_SAMPLE;
    }
    self->node_id_map[input_id] = (tsk_id_t) self->tables->nodes.num_rows;
    ret = tsk_node_table_add_row(&self->tables->nodes, flags,
            node.time, node.population, node.individual,
            node.metadata, node.metadata_length);
out:
    return ret;
}

/* Remove the mapping for the last recorded node. */
static int
simplifier_rewind_node(simplifier_t *self, tsk_id_t input_id, tsk_id_t output_id)
{
    self->node_id_map[input_id] = TSK_NULL;
    return tsk_node_table_truncate(&self->tables->nodes, (tsk_size_t) output_id);
}

static int
simplifier_flush_edges(simplifier_t *self, tsk_id_t parent, size_t *ret_num_edges)
{
    int ret = 0;
    size_t j;
    tsk_id_t child;
    interval_list_t *x;
    size_t num_edges = 0;

    qsort(self->buffered_children, self->num_buffered_children,
            sizeof(tsk_id_t), cmp_node_id);
    for (j = 0; j < self->num_buffered_children; j++) {
        child = self->buffered_children[j];
        for (x = self->child_edge_map_head[child]; x != NULL; x = x->next) {
            ret = tsk_edge_table_add_row(&self->tables->edges, x->left, x->right, parent, child);
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
    ret = tsk_blkalloc_reset(&self->interval_list_heap);
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
    size_t num_sites = self->input_tables.sites.num_rows;

    self->position_lookup = malloc((num_sites + 2) * sizeof(*self->position_lookup));
    if (self->position_lookup == NULL) {
        goto out;
    }
    self->position_lookup[0] = 0;
    self->position_lookup[num_sites + 1] = self->tables->sequence_length;
    memcpy(self->position_lookup + 1, self->input_tables.sites.position,
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
    size_t N = self->input_tables.sites.num_rows + 2;
    size_t left_index, right_index;
    bool skip = false;

    left_index = tsk_search_sorted(X, N, *left);
    right_index = tsk_search_sorted(X, N, *right);
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
simplifier_record_edge(simplifier_t *self, double left, double right, tsk_id_t child)
{
    int ret = 0;
    interval_list_t *tail, *x;
    bool skip;

    if (!!(self->options & TSK_REDUCE_TO_SITE_TOPOLOGY)) {
        skip = simplifier_map_reduced_coordinates(self, &left, &right);
        /* NOTE: we exit early here when reduce_coordindates has told us to
         * skip this edge, as it is not visible in the reduced tree sequence */
        if (skip) {
            goto out;
        }
    }

    tail = self->child_edge_map_tail[child];
    if (tail == NULL) {
        assert(self->num_buffered_children < self->input_tables.nodes.num_rows);
        self->buffered_children[self->num_buffered_children] = child;
        self->num_buffered_children++;
        x = simplifier_alloc_interval_list(self, left, right);
        if (x == NULL) {
            ret = TSK_ERR_NO_MEMORY;
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
                ret = TSK_ERR_NO_MEMORY;
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
    tsk_id_t node;
    mutation_id_list_t *list_node;
    size_t j;

    self->mutation_id_map = calloc(self->input_tables.mutations.num_rows,
            sizeof(tsk_id_t));
    self->mutation_node_map = calloc(self->input_tables.mutations.num_rows,
            sizeof(tsk_id_t));
    self->node_mutation_list_mem = malloc(self->input_tables.mutations.num_rows *
            sizeof(mutation_id_list_t));
    self->node_mutation_list_map_head = calloc(self->input_tables.nodes.num_rows,
            sizeof(mutation_id_list_t *));
    self->node_mutation_list_map_tail = calloc(self->input_tables.nodes.num_rows,
            sizeof(mutation_id_list_t *));
    if (self->mutation_id_map == NULL || self->mutation_node_map == NULL
            || self->node_mutation_list_mem == NULL
            || self->node_mutation_list_map_head == NULL
            || self->node_mutation_list_map_tail == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    memset(self->mutation_id_map, 0xff,
            self->input_tables.mutations.num_rows * sizeof(tsk_id_t));
    memset(self->mutation_node_map, 0xff,
            self->input_tables.mutations.num_rows * sizeof(tsk_id_t));

    for (j = 0; j < self->input_tables.mutations.num_rows; j++) {
        node = self->input_tables.mutations.node[j];
        list_node = self->node_mutation_list_mem + j;
        list_node->mutation = (tsk_id_t) j;
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

static int TSK_WARN_UNUSED
simplifier_add_ancestry(simplifier_t *self, tsk_id_t input_id, double left, double right,
        tsk_id_t output_id)
{
    int ret = 0;
    simplify_segment_t *tail = self->ancestor_map_tail[input_id];
    simplify_segment_t *x;

    assert(left < right);
    if (tail == NULL) {
        x = simplifier_alloc_segment(self, left, right, output_id);
        if (x == NULL) {
            ret = TSK_ERR_NO_MEMORY;
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
                ret = TSK_ERR_NO_MEMORY;
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
simplifier_init_samples(simplifier_t *self, tsk_id_t *samples)
{
    int ret = 0;
    size_t j;

    /* Go through the samples to check for errors. */
    for (j = 0; j < self->num_samples; j++) {
        if (samples[j] < 0 || samples[j] > (tsk_id_t) self->input_tables.nodes.num_rows) {
            ret = TSK_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        if (self->is_sample[samples[j]]) {
            ret = TSK_ERR_DUPLICATE_SAMPLE;
            goto out;
        }
        self->is_sample[samples[j]] = true;
        ret = simplifier_record_node(self, samples[j], true);
        if (ret < 0) {
            goto out;
        }
        ret = simplifier_add_ancestry(self, samples[j], 0, self->tables->sequence_length,
            (tsk_id_t) ret);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

static int
simplifier_init(simplifier_t *self, tsk_id_t *samples, size_t num_samples,
        tsk_table_collection_t *tables, tsk_flags_t options)
{
    int ret = 0;
    size_t num_nodes_alloc;

    memset(self, 0, sizeof(simplifier_t));
    self->num_samples = num_samples;
    self->options = options;
    self->tables = tables;

    /* TODO we can add a flag to skip these checks for when we know they are
     * unnecessary */
    /* TODO Current unit tests require TSK_CHECK_SITE_DUPLICATES but it's
     * debateable whether we need it. If we remove, we definitely need explicit
     * tests to ensure we're doing sensible things with duplicate sites.
     * (Particularly, re TSK_REDUCE_TO_SITE_TOPOLOGY.) */
    ret = tsk_table_collection_check_integrity(tables,
            TSK_CHECK_OFFSETS|TSK_CHECK_EDGE_ORDERING|TSK_CHECK_SITE_ORDERING|
            TSK_CHECK_SITE_DUPLICATES);
    if (ret != 0) {
        goto out;
    }

    ret = tsk_table_collection_copy(self->tables, &self->input_tables, 0);
    if (ret != 0) {
        goto out;
    }

    /* Take a copy of the input samples */
    self->samples = malloc(num_samples * sizeof(tsk_id_t));
    if (self->samples == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    memcpy(self->samples, samples, num_samples * sizeof(tsk_id_t));

    /* Allocate the heaps used for small objects-> Assuming 8K is a good chunk size */
    ret = tsk_blkalloc_init(&self->segment_heap, 8192);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_blkalloc_init(&self->interval_list_heap, 8192);
    if (ret != 0) {
        goto out;
    }
    ret = segment_overlapper_alloc(&self->segment_overlapper);
    if (ret != 0) {
        goto out;
    }
    /* Need to avoid malloc(0) so make sure we have at least 1. */
    num_nodes_alloc = 1 + tables->nodes.num_rows;
    /* Make the maps and set the intial state */
    self->ancestor_map_head = calloc(num_nodes_alloc, sizeof(simplify_segment_t *));
    self->ancestor_map_tail = calloc(num_nodes_alloc, sizeof(simplify_segment_t *));
    self->child_edge_map_head = calloc(num_nodes_alloc, sizeof(interval_list_t *));
    self->child_edge_map_tail = calloc(num_nodes_alloc, sizeof(interval_list_t *));
    self->node_id_map = malloc(num_nodes_alloc * sizeof(tsk_id_t));
    self->buffered_children = malloc(num_nodes_alloc * sizeof(tsk_id_t));
    self->is_sample = calloc(num_nodes_alloc, sizeof(bool));
    self->max_segment_queue_size = 64;
    self->segment_queue = malloc(self->max_segment_queue_size
            * sizeof(simplify_segment_t));
    if (self->ancestor_map_head == NULL || self->ancestor_map_tail == NULL
            || self->child_edge_map_head == NULL || self->child_edge_map_tail == NULL
            || self->node_id_map == NULL || self->is_sample == NULL
            || self->segment_queue == NULL || self->buffered_children == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    ret = tsk_table_collection_clear(self->tables);
    if (ret != 0) {
        goto out;
    }
    memset(self->node_id_map, 0xff, self->input_tables.nodes.num_rows * sizeof(tsk_id_t));
    ret = simplifier_init_samples(self, samples);
    if (ret != 0) {
        goto out;
    }
    ret = simplifier_init_sites(self);
    if (ret != 0) {
        goto out;
    }
    if (!!(self->options & TSK_REDUCE_TO_SITE_TOPOLOGY)) {
        ret = simplifier_init_position_lookup(self);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

static int
simplifier_free(simplifier_t *self)
{
    tsk_table_collection_free(&self->input_tables);
    tsk_blkalloc_free(&self->segment_heap);
    tsk_blkalloc_free(&self->interval_list_heap);
    segment_overlapper_free(&self->segment_overlapper);
    tsk_safe_free(self->samples);
    tsk_safe_free(self->ancestor_map_head);
    tsk_safe_free(self->ancestor_map_tail);
    tsk_safe_free(self->child_edge_map_head);
    tsk_safe_free(self->child_edge_map_tail);
    tsk_safe_free(self->node_id_map);
    tsk_safe_free(self->segment_queue);
    tsk_safe_free(self->is_sample);
    tsk_safe_free(self->mutation_id_map);
    tsk_safe_free(self->mutation_node_map);
    tsk_safe_free(self->node_mutation_list_mem);
    tsk_safe_free(self->node_mutation_list_map_head);
    tsk_safe_free(self->node_mutation_list_map_tail);
    tsk_safe_free(self->buffered_children);
    tsk_safe_free(self->position_lookup);
    return 0;
}

static int TSK_WARN_UNUSED
simplifier_enqueue_segment(simplifier_t *self, double left, double right, tsk_id_t node)
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
            ret = TSK_ERR_NO_MEMORY;
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

static int TSK_WARN_UNUSED
simplifier_merge_ancestors(simplifier_t *self, tsk_id_t input_id)
{
    int ret = 0;
    simplify_segment_t **X, *x;
    size_t j, num_overlapping, num_flushed_edges;
    double left, right, prev_right;
    tsk_id_t ancestry_node;
    tsk_id_t output_id = self->node_id_map[input_id];
    bool is_sample = output_id != TSK_NULL;
    bool keep_unary = !!(self->options & TSK_KEEP_UNARY);

    if (is_sample) {
        /* Free up the existing ancestry mapping. */
        x = self->ancestor_map_tail[input_id];
        assert(x->left == 0 && x->right == self->tables->sequence_length);
        self->ancestor_map_head[input_id] = NULL;
        self->ancestor_map_tail[input_id] = NULL;
    }

    ret = segment_overlapper_start(&self->segment_overlapper,
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
            } else if (keep_unary) {
                if (output_id == TSK_NULL) {
                    output_id = simplifier_record_node(self, input_id, false);
                }
                ret = simplifier_record_edge(self, left, right, ancestry_node);
                if (ret != 0) {
                    goto out;
                }
            }
        } else {
            if (output_id == TSK_NULL) {
                ret = simplifier_record_node(self, input_id, false);
                if (ret < 0) {
                    goto out;
                }
                output_id = (tsk_id_t) ret;
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
        if (keep_unary) {
            ancestry_node = output_id;
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
    if (output_id != TSK_NULL) {
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

static int TSK_WARN_UNUSED
simplifier_process_parent_edges(simplifier_t *self, tsk_id_t parent, size_t start,
        size_t end)
{
    int ret = 0;
    size_t j;
    simplify_segment_t *x;
    const tsk_edge_table_t *input_edges = &self->input_tables.edges;
    tsk_id_t child;
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
                        TSK_MAX(x->left, left), TSK_MIN(x->right, right), x->node);
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

static int TSK_WARN_UNUSED
simplifier_map_mutation_nodes(simplifier_t *self)
{
    int ret = 0;
    simplify_segment_t *seg;
    mutation_id_list_t *m_node;
    size_t input_node;
    tsk_id_t site;
    double position;

    for (input_node = 0; input_node < self->input_tables.nodes.num_rows; input_node++) {
        seg = self->ancestor_map_head[input_node];
        m_node = self->node_mutation_list_map_head[input_node];
        /* Co-iterate over the segments and mutations; mutations must be listed
         * in increasing order of site position */
        while (seg != NULL && m_node != NULL) {
            site = self->input_tables.mutations.site[m_node->mutation];
            position = self->input_tables.sites.position[site];
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

static int TSK_WARN_UNUSED
simplifier_output_sites(simplifier_t *self)
{
    int ret = 0;
    tsk_id_t input_site;
    tsk_id_t input_mutation, mapped_parent ,site_start, site_end;
    tsk_id_t num_input_sites = (tsk_id_t) self->input_tables.sites.num_rows;
    tsk_id_t num_input_mutations = (tsk_id_t) self->input_tables.mutations.num_rows;
    tsk_id_t input_parent, num_output_mutations, num_output_site_mutations;
    tsk_id_t mapped_node;
    bool keep_site;
    bool filter_sites = !!(self->options & TSK_FILTER_SITES);
    tsk_site_t site;
    tsk_mutation_t mutation;

    input_mutation = 0;
    num_output_mutations = 0;
    for (input_site = 0; input_site < num_input_sites; input_site++) {
        ret = tsk_site_table_get_row(&self->input_tables.sites, (tsk_id_t) input_site, &site);
        if (ret != 0) {
            goto out;
        }
        site_start = input_mutation;
        num_output_site_mutations = 0;
        while (input_mutation < num_input_mutations
                && self->input_tables.mutations.site[input_mutation] == site.id) {
            mapped_node = self->mutation_node_map[input_mutation];
            if (mapped_node != TSK_NULL) {
                input_parent = self->input_tables.mutations.parent[input_mutation];
                mapped_parent = TSK_NULL;
                if (input_parent != TSK_NULL) {
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
                if (self->mutation_id_map[input_mutation] != TSK_NULL) {
                    assert(self->tables->mutations.num_rows
                            == (size_t) self->mutation_id_map[input_mutation]);
                    mapped_node = self->mutation_node_map[input_mutation];
                    assert(mapped_node != TSK_NULL);
                    mapped_parent = self->input_tables.mutations.parent[input_mutation];
                    if (mapped_parent != TSK_NULL) {
                        mapped_parent = self->mutation_id_map[mapped_parent];
                    }
                    ret = tsk_mutation_table_get_row(&self->input_tables.mutations,
                            (tsk_id_t) input_mutation, &mutation);
                    if (ret != 0) {
                        goto out;
                    }
                    ret = tsk_mutation_table_add_row(&self->tables->mutations,
                            (tsk_id_t) self->tables->sites.num_rows,
                            mapped_node, mapped_parent,
                            mutation.derived_state, mutation.derived_state_length,
                            mutation.metadata, mutation.metadata_length);
                    if (ret < 0) {
                        goto out;
                    }
                }
            }
            ret = tsk_site_table_add_row(&self->tables->sites, site.position,
                    site.ancestral_state, site.ancestral_state_length,
                    site.metadata, site.metadata_length);
            if (ret < 0) {
                goto out;
            }
        }
        assert(num_output_mutations == (tsk_id_t) self->tables->mutations.num_rows);
        input_mutation = site_end;
    }
    assert(input_mutation == num_input_mutations);
    ret = 0;
out:
    return ret;
}

static int TSK_WARN_UNUSED
simplifier_finalise_references(simplifier_t *self)
{
    int ret = 0;
    tsk_size_t j;
    bool keep;
    tsk_size_t num_nodes = self->tables->nodes.num_rows;

    tsk_population_t pop;
    tsk_id_t pop_id;
    tsk_size_t num_populations = self->input_tables.populations.num_rows;
    tsk_id_t *node_population = self->tables->nodes.population;
    bool *population_referenced = calloc(num_populations, sizeof(*population_referenced));
    tsk_id_t *population_id_map = malloc(
            num_populations * sizeof(*population_id_map));
    bool filter_populations = !!(self->options & TSK_FILTER_POPULATIONS);

    tsk_individual_t ind;
    tsk_id_t ind_id;
    tsk_size_t num_individuals = self->input_tables.individuals.num_rows;
    tsk_id_t *node_individual = self->tables->nodes.individual;
    bool *individual_referenced = calloc(num_individuals, sizeof(*individual_referenced));
    tsk_id_t *individual_id_map = malloc(
            num_individuals * sizeof(*individual_id_map));
    bool filter_individuals = !!(self->options & TSK_FILTER_INDIVIDUALS);

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
    if (self->input_tables.migrations.num_rows != 0) {
        ret = TSK_ERR_SIMPLIFY_MIGRATIONS_NOT_SUPPORTED;
        goto out;
    }

    for (j = 0; j < num_nodes; j++) {
        pop_id = node_population[j];
        if (pop_id != TSK_NULL) {
            population_referenced[pop_id] = true;
        }
        ind_id = node_individual[j];
        if (ind_id != TSK_NULL) {
            individual_referenced[ind_id] = true;
        }
    }
    for (j = 0; j < num_populations; j++) {
        ret = tsk_population_table_get_row(&self->input_tables.populations, (tsk_id_t) j, &pop);
        if (ret != 0) {
            goto out;
        }
        keep = true;
        if (filter_populations && !population_referenced[j]) {
            keep = false;
        }
        population_id_map[j] = TSK_NULL;
        if (keep) {
            ret = tsk_population_table_add_row(&self->tables->populations,
                pop.metadata, pop.metadata_length);
            if (ret < 0) {
                goto out;
            }
            population_id_map[j] = (tsk_id_t) ret;
        }
    }

    for (j = 0; j < num_individuals; j++) {
        ret = tsk_individual_table_get_row(&self->input_tables.individuals, (tsk_id_t) j, &ind);
        if (ret != 0) {
            goto out;
        }
        keep = true;
        if (filter_individuals && !individual_referenced[j]) {
            keep = false;
        }
        individual_id_map[j] = TSK_NULL;
        if (keep) {
            ret = tsk_individual_table_add_row(&self->tables->individuals,
                ind.flags, ind.location, ind.location_length,
                ind.metadata, ind.metadata_length);
            if (ret < 0) {
                goto out;
            }
            individual_id_map[j] = (tsk_id_t) ret;
        }
    }

    /* Remap node IDs referencing the above */
    for (j = 0; j < num_nodes; j++) {
        pop_id = node_population[j];
        if (pop_id != TSK_NULL) {
            node_population[j] = population_id_map[pop_id];
        }
        ind_id = node_individual[j];
        if (ind_id != TSK_NULL) {
            node_individual[j] = individual_id_map[ind_id];
        }
    }

    ret = tsk_provenance_table_copy(&self->input_tables.provenances,
            &self->tables->provenances, TSK_NO_INIT);
    if (ret != 0) {
        goto out;
    }
out:
    tsk_safe_free(population_referenced);
    tsk_safe_free(individual_referenced);
    tsk_safe_free(population_id_map);
    tsk_safe_free(individual_id_map);
    return ret;
}

static int TSK_WARN_UNUSED
simplifier_run(simplifier_t *self, tsk_id_t *node_map)
{
    int ret = 0;
    size_t j, start;
    tsk_id_t parent, current_parent;
    const tsk_edge_table_t *input_edges = &self->input_tables.edges;
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
                self->input_tables.nodes.num_rows * sizeof(tsk_id_t));
    }
out:
    return ret;
}

/*************************
 * table_collection
 *************************/

typedef struct {
    tsk_id_t index;
    /* These are the sort keys in order */
    double first;
    double second;
    tsk_id_t third;
    tsk_id_t fourth;
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
tsk_table_collection_check_offsets(tsk_table_collection_t *self)
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
    ret = check_offsets(self->individuals.num_rows, self->individuals.metadata_offset,
            self->individuals.metadata_length, true);
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

static int
tsk_table_collection_check_edge_ordering(tsk_table_collection_t *self)
{
    int ret = 0;
    tsk_size_t j;
    tsk_id_t parent, last_parent, child, last_child;
    double left, last_left;
    const double *time = self->nodes.time;
    bool *parent_seen = calloc(self->nodes.num_rows, sizeof(bool));

    if (parent_seen == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    /* Just keeping compiler happy; these values don't matter. */
    last_left = 0;
    last_parent = 0;
    last_child = 0;
    for (j = 0; j < self->edges.num_rows; j++) {
        left = self->edges.left[j];
        parent = self->edges.parent[j];
        child = self->edges.child[j];
        if (parent_seen[parent]) {
            ret = TSK_ERR_EDGES_NONCONTIGUOUS_PARENTS;
            goto out;
        }
        if (j > 0) {
            /* Input data must sorted by (time[parent], parent, child, left). */
            if (time[parent] < time[last_parent]) {
                ret = TSK_ERR_EDGES_NOT_SORTED_PARENT_TIME;
                goto out;
            }
            if (time[parent] == time[last_parent]) {
                if (parent == last_parent) {
                    if (child < last_child) {
                        ret = TSK_ERR_EDGES_NOT_SORTED_CHILD;
                        goto out;
                    }
                    if (child == last_child) {
                        if (left == last_left) {
                            ret = TSK_ERR_DUPLICATE_EDGES;
                            goto out;
                        } else if (left < last_left) {
                            ret = TSK_ERR_EDGES_NOT_SORTED_LEFT;
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
    tsk_safe_free(parent_seen);
    return ret;
}


/* Checks the integrity of the table collection. What gets checked depends
 * on the flags values:
 * 0                             Check the integrity of ID & spatial references.
 * TSK_CHECK_OFFSETS             Check offsets for ragged columns.
 * TSK_CHECK_EDGE_ORDERING       Check edge ordering contraints for a tree sequence.
 * TSK_CHECK_SITE_ORDERING       Check that sites are in nondecreasing position order.
 * TSK_CHECK_SITE_DUPLICATES     Check for any duplicate site positions.
 * TSK_CHECK_MUTATION_ORDERING   Check mutation ordering contraints for a tree sequence.
 * TSK_CHECK_INDEXES             Check indexes exist & reference integrity.
 * TSK_CHECK_ALL                 All above checks.
 */
int TSK_WARN_UNUSED
tsk_table_collection_check_integrity(tsk_table_collection_t *self, tsk_flags_t options)
{
    int ret = TSK_ERR_GENERIC;
    tsk_size_t j;
    double left, right, position;
    double L = self->sequence_length;
    double *time = self->nodes.time;
    tsk_id_t parent, child;
    tsk_id_t parent_mut;
    tsk_id_t population;
    tsk_id_t individual;
    tsk_id_t num_nodes = (tsk_id_t) self->nodes.num_rows;
    tsk_id_t num_edges = (tsk_id_t) self->edges.num_rows;
    tsk_id_t num_sites = (tsk_id_t) self->sites.num_rows;
    tsk_id_t num_mutations = (tsk_id_t) self->mutations.num_rows;
    tsk_id_t num_populations = (tsk_id_t) self->populations.num_rows;
    tsk_id_t num_individuals = (tsk_id_t) self->individuals.num_rows;
    bool check_site_ordering = !!(options & TSK_CHECK_SITE_ORDERING);
    bool check_site_duplicates = !!(options & TSK_CHECK_SITE_DUPLICATES);
    bool check_mutation_ordering = !!(options & TSK_CHECK_MUTATION_ORDERING);

    if (self->sequence_length <= 0) {
        ret = TSK_ERR_BAD_SEQUENCE_LENGTH;
        goto out;
    }

    /* Nodes */
    for (j = 0; j < self->nodes.num_rows; j++) {
        population = self->nodes.population[j];
        if (population < TSK_NULL || population >= num_populations) {
            ret = TSK_ERR_POPULATION_OUT_OF_BOUNDS;
            goto out;
        }
        individual = self->nodes.individual[j];
        if (individual < TSK_NULL || individual >= num_individuals) {
            ret = TSK_ERR_INDIVIDUAL_OUT_OF_BOUNDS;
            goto out;
        }
    }

    /* Edges */
    for (j = 0; j < self->edges.num_rows; j++) {
        parent = self->edges.parent[j];
        child = self->edges.child[j];
        left = self->edges.left[j];
        right = self->edges.right[j];
        /* Node ID integrity */
        if (parent == TSK_NULL) {
            ret = TSK_ERR_NULL_PARENT;
            goto out;
        }
        if (parent < 0 || parent >= num_nodes) {
            ret = TSK_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        if (child == TSK_NULL) {
            ret = TSK_ERR_NULL_CHILD;
            goto out;
        }
        if (child < 0 || child >= num_nodes) {
            ret = TSK_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        /* Spatial requirements for edges */
        if (left < 0) {
            ret = TSK_ERR_LEFT_LESS_ZERO;
            goto out;
        }
        if (right > L) {
            ret = TSK_ERR_RIGHT_GREATER_SEQ_LENGTH;
            goto out;
        }
        if (left >= right) {
            ret = TSK_ERR_BAD_EDGE_INTERVAL;
            goto out;
        }
        /* time[child] must be < time[parent] */
        if (time[child] >= time[parent]) {
            ret = TSK_ERR_BAD_NODE_TIME_ORDERING;
            goto out;
        }
    }
    for (j = 0; j < self->sites.num_rows; j++) {
        position = self->sites.position[j];
        /* Spatial requirements */
        if (position < 0 || position >= L) {
            ret = TSK_ERR_BAD_SITE_POSITION;
            goto out;
        }
        if (j > 0) {
            if (check_site_duplicates && self->sites.position[j - 1] == position) {
                ret = TSK_ERR_DUPLICATE_SITE_POSITION;
                goto out;
            }
            if (check_site_ordering && self->sites.position[j - 1] > position) {
                ret = TSK_ERR_UNSORTED_SITES;
                goto out;
            }
        }
    }

    /* Mutations */
    for (j = 0; j < self->mutations.num_rows; j++) {
        if (self->mutations.site[j] < 0 || self->mutations.site[j] >= num_sites) {
            ret = TSK_ERR_SITE_OUT_OF_BOUNDS;
            goto out;
        }
        if (self->mutations.node[j] < 0 || self->mutations.node[j] >= num_nodes) {
            ret = TSK_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        parent_mut = self->mutations.parent[j];
        if (parent_mut < TSK_NULL || parent_mut >= num_mutations) {
            ret = TSK_ERR_MUTATION_OUT_OF_BOUNDS;
            goto out;
        }
        if (parent_mut == (tsk_id_t) j) {
            ret = TSK_ERR_MUTATION_PARENT_EQUAL;
            goto out;
        }
        if (check_mutation_ordering) {
            if (parent_mut != TSK_NULL) {
                /* Parents must be listed before their children */
                if (parent_mut > (tsk_id_t) j) {
                    ret = TSK_ERR_MUTATION_PARENT_AFTER_CHILD;
                    goto out;
                }
                if (self->mutations.site[parent_mut] != self->mutations.site[j]) {
                    ret = TSK_ERR_MUTATION_PARENT_DIFFERENT_SITE;
                    goto out;
                }
            }
            if (j > 0) {
                if (self->mutations.site[j - 1] > self->mutations.site[j]) {
                    ret = TSK_ERR_UNSORTED_MUTATIONS;
                    goto out;
                }
            }
        }
    }

    /* Migrations */
    for (j = 0; j < self->migrations.num_rows; j++) {
        if (self->migrations.node[j] < 0 || self->migrations.node[j] >= num_nodes) {
            ret = TSK_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        if (self->migrations.source[j] < 0
                || self->migrations.source[j] >= num_populations) {
            ret = TSK_ERR_POPULATION_OUT_OF_BOUNDS;
            goto out;
        }
        if (self->migrations.dest[j] < 0
                || self->migrations.dest[j] >= num_populations) {
            ret = TSK_ERR_POPULATION_OUT_OF_BOUNDS;
            goto out;
        }
        left = self->migrations.left[j];
        right = self->migrations.right[j];
        /* Spatial requirements */
        /* TODO it's a bit misleading to use the edge-specific errors here. */
        if (left < 0) {
            ret = TSK_ERR_LEFT_LESS_ZERO;
            goto out;
        }
        if (right > L) {
            ret = TSK_ERR_RIGHT_GREATER_SEQ_LENGTH;
            goto out;
        }
        if (left >= right) {
            ret = TSK_ERR_BAD_EDGE_INTERVAL;
            goto out;
        }
    }

    if (!!(options & TSK_CHECK_INDEXES)) {
        if (!tsk_table_collection_has_index(self, 0)) {
            ret = TSK_ERR_TABLES_NOT_INDEXED;
            goto out;
        }
        for (j = 0; j < self->edges.num_rows; j++) {
            if (self->indexes.edge_insertion_order[j] < 0 ||
                    self->indexes.edge_insertion_order[j] >= num_edges) {
                ret = TSK_ERR_EDGE_OUT_OF_BOUNDS;
                goto out;
            }
            if (self->indexes.edge_removal_order[j] < 0 ||
                    self->indexes.edge_removal_order[j] >= num_edges) {
                ret = TSK_ERR_EDGE_OUT_OF_BOUNDS;
                goto out;
            }
        }
    }

    ret = 0;
    if (!!(options & TSK_CHECK_OFFSETS)) {
        ret = tsk_table_collection_check_offsets(self);
        if (ret != 0) {
            goto out;
        }
    }
    if (!!(options & TSK_CHECK_EDGE_ORDERING)) {
        ret = tsk_table_collection_check_edge_ordering(self);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

void
tsk_table_collection_print_state(tsk_table_collection_t *self, FILE *out)
{
    fprintf(out, "Table collection state\n");
    fprintf(out, "sequence_length = %f\n", self->sequence_length);
    tsk_individual_table_print_state(&self->individuals, out);
    tsk_node_table_print_state(&self->nodes, out);
    tsk_edge_table_print_state(&self->edges, out);
    tsk_migration_table_print_state(&self->migrations, out);
    tsk_site_table_print_state(&self->sites, out);
    tsk_mutation_table_print_state(&self->mutations, out);
    tsk_population_table_print_state(&self->populations, out);
    tsk_provenance_table_print_state(&self->provenances, out);
}

int TSK_WARN_UNUSED
tsk_table_collection_init(tsk_table_collection_t *self, tsk_flags_t options)
{
    int ret = 0;
    memset(self, 0, sizeof(*self));
    if (! (options & TSK_NO_INIT_TABLES)) {
        /* Allocate all the tables with their default increments */
        ret = tsk_node_table_init(&self->nodes, 0);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_edge_table_init(&self->edges, 0);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_migration_table_init(&self->migrations, 0);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_site_table_init(&self->sites, 0);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_mutation_table_init(&self->mutations, 0);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_individual_table_init(&self->individuals, 0);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_population_table_init(&self->populations, 0);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_provenance_table_init(&self->provenances, 0);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

int
tsk_table_collection_free(tsk_table_collection_t *self)
{
    tsk_individual_table_free(&self->individuals);
    tsk_node_table_free(&self->nodes);
    tsk_edge_table_free(&self->edges);
    tsk_migration_table_free(&self->migrations);
    tsk_site_table_free(&self->sites);
    tsk_mutation_table_free(&self->mutations);
    tsk_population_table_free(&self->populations);
    tsk_provenance_table_free(&self->provenances);
    if (self->indexes.malloced_locally) {
        tsk_safe_free(self->indexes.edge_insertion_order);
        tsk_safe_free(self->indexes.edge_removal_order);
    }
    if (self->store != NULL) {
        kastore_close(self->store);
        free(self->store);
    }
    tsk_safe_free(self->file_uuid);
    return 0;
}

/* Returns true if all the tables and collection metadata are equal. Note
 * this does *not* consider the indexes, since these are derived from the
 * tables. We do not consider the file_uuids either, since this is a property of
 * the file that set of tables is stored in. */
bool
tsk_table_collection_equals(tsk_table_collection_t *self, tsk_table_collection_t *other)
{
    bool ret = self->sequence_length == other->sequence_length
        && tsk_individual_table_equals(&self->individuals, &other->individuals)
        && tsk_node_table_equals(&self->nodes, &other->nodes)
        && tsk_edge_table_equals(&self->edges, &other->edges)
        && tsk_migration_table_equals(&self->migrations, &other->migrations)
        && tsk_site_table_equals(&self->sites, &other->sites)
        && tsk_mutation_table_equals(&self->mutations, &other->mutations)
        && tsk_population_table_equals(&self->populations, &other->populations)
        && tsk_provenance_table_equals(&self->provenances, &other->provenances);
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_copy(tsk_table_collection_t *self, tsk_table_collection_t *dest,
        tsk_flags_t options)
{
    int ret = 0;
    size_t index_size;

    if (! (options & TSK_NO_INIT)) {
        ret = tsk_table_collection_init(dest, 0);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_node_table_copy(&self->nodes, &dest->nodes, TSK_NO_INIT);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_edge_table_copy(&self->edges, &dest->edges, TSK_NO_INIT);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_migration_table_copy(&self->migrations, &dest->migrations, TSK_NO_INIT);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_site_table_copy(&self->sites, &dest->sites, TSK_NO_INIT);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_mutation_table_copy(&self->mutations, &dest->mutations, TSK_NO_INIT);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_individual_table_copy(&self->individuals, &dest->individuals, TSK_NO_INIT);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_population_table_copy(&self->populations, &dest->populations, TSK_NO_INIT);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_provenance_table_copy(&self->provenances, &dest->provenances, TSK_NO_INIT);
    if (ret != 0) {
        goto out;
    }
    dest->sequence_length = self->sequence_length;
    if (tsk_table_collection_has_index(self, 0)) {
        tsk_table_collection_drop_index(dest, 0);
        index_size = self->edges.num_rows * sizeof(tsk_id_t);
        dest->indexes.edge_insertion_order = malloc(index_size);
        dest->indexes.edge_removal_order = malloc(index_size);
        dest->indexes.malloced_locally = true;
        if (dest->indexes.edge_insertion_order == NULL
                || dest->indexes.edge_removal_order == NULL) {
            ret = TSK_ERR_NO_MEMORY;
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
tsk_table_collection_has_index(tsk_table_collection_t *self, tsk_flags_t TSK_UNUSED(options))
{
    return self->indexes.edge_insertion_order != NULL
        && self->indexes.edge_removal_order != NULL;
}

int
tsk_table_collection_drop_index(tsk_table_collection_t *self, tsk_flags_t TSK_UNUSED(options))
{
    if (self->indexes.malloced_locally) {
        tsk_safe_free(self->indexes.edge_insertion_order);
        tsk_safe_free(self->indexes.edge_removal_order);
    }
    self->indexes.edge_insertion_order = NULL;
    self->indexes.edge_removal_order = NULL;
    return 0;
}

int TSK_WARN_UNUSED
tsk_table_collection_build_index(tsk_table_collection_t *self, tsk_flags_t TSK_UNUSED(options))
{
    int ret = TSK_ERR_GENERIC;
    size_t j;
    double *time = self->nodes.time;
    index_sort_t *sort_buff = NULL;
    tsk_id_t parent;

    /* For build indexes to make sense we must have referential integrity and
     * sorted edges */
    ret = tsk_table_collection_check_integrity(self, TSK_CHECK_EDGE_ORDERING);
    if (ret != 0) {
        goto out;
    }

    tsk_table_collection_drop_index(self, 0);
    self->indexes.malloced_locally = true;
    self->indexes.edge_insertion_order = malloc(self->edges.num_rows * sizeof(tsk_id_t));
    self->indexes.edge_removal_order = malloc(self->edges.num_rows * sizeof(tsk_id_t));
    sort_buff = malloc(self->edges.num_rows * sizeof(index_sort_t));
    if (self->indexes.edge_insertion_order == NULL
            || self->indexes.edge_removal_order == NULL
            || sort_buff == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }

    /* sort by left and increasing time to give us the order in which
     * records should be inserted */
    for (j = 0; j < self->edges.num_rows; j++) {
        sort_buff[j].index = (tsk_id_t ) j;
        sort_buff[j].first = self->edges.left[j];
        parent = self->edges.parent[j];
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
        sort_buff[j].index = (tsk_id_t ) j;
        sort_buff[j].first = self->edges.right[j];
        parent = self->edges.parent[j];
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
    tsk_safe_free(sort_buff);
    return ret;
}

static int TSK_WARN_UNUSED
tsk_table_collection_read_format_data(tsk_table_collection_t *self)
{
    int ret = 0;
    size_t len;
    uint32_t *version;
    int8_t *format_name, *uuid;
    double *L;

    ret = kastore_gets_int8(self->store, "format/name", &format_name, &len);
    if (ret != 0) {
        ret = tsk_set_kas_error(ret);
        goto out;
    }
    if (len != TSK_FILE_FORMAT_NAME_LENGTH) {
        ret = TSK_ERR_FILE_FORMAT;
        goto out;
    }
    if (memcmp(TSK_FILE_FORMAT_NAME, format_name, TSK_FILE_FORMAT_NAME_LENGTH) != 0) {
        ret = TSK_ERR_FILE_FORMAT;
        goto out;
    }

    ret = kastore_gets_uint32(self->store, "format/version", &version, &len);
    if (ret != 0) {
        ret = tsk_set_kas_error(ret);
        goto out;
    }
    if (len != 2) {
        ret = TSK_ERR_FILE_FORMAT;
        goto out;
    }
    if (version[0] < TSK_FILE_FORMAT_VERSION_MAJOR) {
        ret = TSK_ERR_FILE_VERSION_TOO_OLD;
        goto out;
    }
    if (version[0] > TSK_FILE_FORMAT_VERSION_MAJOR) {
        ret = TSK_ERR_FILE_VERSION_TOO_NEW;
        goto out;
    }

    ret = kastore_gets_float64(self->store, "sequence_length", &L, &len);
    if (ret != 0) {
        ret = tsk_set_kas_error(ret);
        goto out;
    }
    if (len != 1) {
        ret = TSK_ERR_FILE_FORMAT;
        goto out;
    }
    if (L[0] <= 0.0) {
        ret = TSK_ERR_BAD_SEQUENCE_LENGTH;
        goto out;
    }
    self->sequence_length = L[0];

    ret = kastore_gets_int8(self->store, "uuid", &uuid, &len);
    if (ret != 0) {
        ret = tsk_set_kas_error(ret);
        goto out;
    }
    if (len != TSK_UUID_SIZE) {
        ret = TSK_ERR_FILE_FORMAT;
        goto out;
    }

    /* Allow space for \0 so we can print it as a string */
    self->file_uuid = malloc(TSK_UUID_SIZE + 1);
    if (self->file_uuid == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    memcpy(self->file_uuid, uuid, TSK_UUID_SIZE);
    self->file_uuid[TSK_UUID_SIZE] = '\0';
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_table_collection_dump_indexes(tsk_table_collection_t *self, kastore_t *store)
{
    int ret = 0;
    write_table_col_t write_cols[] = {
        {"indexes/edge_insertion_order", NULL, self->edges.num_rows, KAS_INT32},
        {"indexes/edge_removal_order", NULL, self->edges.num_rows, KAS_INT32},
    };

    if (tsk_table_collection_has_index(self, 0)) {
        write_cols[0].array = self->indexes.edge_insertion_order;
        write_cols[1].array = self->indexes.edge_removal_order;
        ret = write_table_cols(store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
    }
    return ret;
}

/*
 * Returns true if the specified store has the specified key.
 * TODO remove when kastore_contains() is implemented:
 * https://github.com/tskit-dev/kastore/issues/76
 */
static bool
has_key(kastore_t *store, const char *key)
{
    void *dest;
    size_t len;
    int type;

    return kastore_gets(store, key, &dest, &len, &type) == 0;
}

static int TSK_WARN_UNUSED
tsk_table_collection_load_indexes(tsk_table_collection_t *self)
{
    int ret = 0;
    const char *insertion_order = "indexes/edge_insertion_order";
    const char *removal_order =  "indexes/edge_removal_order";
    read_table_col_t read_cols[] = {
        {insertion_order, (void **) &self->indexes.edge_insertion_order,
            &self->edges.num_rows, 0, KAS_INT32},
        {removal_order, (void **) &self->indexes.edge_removal_order,
            &self->edges.num_rows, 0, KAS_INT32},
    };

    if ((! has_key(self->store, insertion_order))
                && (! has_key(self->store, removal_order))) {
        /* If neither key is present the table is unindexed. */
        self->indexes.malloced_locally = true;
        self->indexes.edge_insertion_order = NULL;
        self->indexes.edge_removal_order = NULL;
    } else {
        /* If one or the other key is present it's an error which will be
         * handled by read_table_cols. */
        self->indexes.malloced_locally = false;
        ret = read_table_cols(self->store, read_cols,
                sizeof(read_cols) / sizeof(*read_cols));
    }
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_load(tsk_table_collection_t *self, const char *filename,
        tsk_flags_t options)
{
    int ret = 0;

    /* The semantics are slightly different here for load because we're using
     * direct pointers into the kastore memory for columns. From a client
     * perspective, however, this has the same effect so long as they are
     * not relying on the table column pointers persisting (which they
     * shouldn't). */
    if (!!(options & TSK_NO_INIT)) {
        tsk_table_collection_free(self);
    }
    ret = tsk_table_collection_init(self, TSK_NO_INIT_TABLES);
    if (ret != 0) {
        goto out;
    }
    self->store = calloc(1, sizeof(*self->store));
    if (self->store == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    ret = kastore_open(self->store, filename, "r", KAS_READ_ALL);
    if (ret != 0) {
        ret = tsk_set_kas_error(ret);
        goto out;
    }
    ret = tsk_table_collection_read_format_data(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_node_table_load(&self->nodes, self->store);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_edge_table_load(&self->edges, self->store);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_site_table_load(&self->sites, self->store);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_mutation_table_load(&self->mutations, self->store);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_migration_table_load(&self->migrations, self->store);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_individual_table_load(&self->individuals, self->store);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_population_table_load(&self->populations, self->store);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_provenance_table_load(&self->provenances, self->store);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_load_indexes(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_check_offsets(self);
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_table_collection_write_format_data(tsk_table_collection_t *self, kastore_t *store)
{
    int ret = 0;
    char format_name[TSK_FILE_FORMAT_NAME_LENGTH];
    char uuid[TSK_UUID_SIZE + 1]; // Must include space for trailing null.
    uint32_t version[2] = {
        TSK_FILE_FORMAT_VERSION_MAJOR, TSK_FILE_FORMAT_VERSION_MINOR};
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
    memcpy(format_name, TSK_FILE_FORMAT_NAME, sizeof(format_name));
    ret = write_table_cols(store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_dump(tsk_table_collection_t *self, const char *filename,
        tsk_flags_t options)
{
    int ret = 0;
    kastore_t store;

    ret = kastore_open(&store, filename, "w", 0);
    if (ret != 0) {
        ret = tsk_set_kas_error(ret);
        goto out;
    }
    /* By default we build indexes, if they are needed. Note that this will fail if
     * the tables aren't sorted. */
    if ((!(options & TSK_NO_BUILD_INDEXES))
            && (! tsk_table_collection_has_index(self, 0))) {
        ret = tsk_table_collection_build_index(self, 0);
        if (ret != 0) {
            goto out;
        }
    }

    /* All of these functions will set the kas_error internally, so we don't have
     * to modify the return value. */
    ret = tsk_table_collection_write_format_data(self, &store);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_node_table_dump(&self->nodes, &store);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_edge_table_dump(&self->edges, &store);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_site_table_dump(&self->sites, &store);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_migration_table_dump(&self->migrations, &store);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_mutation_table_dump(&self->mutations, &store);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_individual_table_dump(&self->individuals, &store);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_population_table_dump(&self->populations, &store);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_provenance_table_dump(&self->provenances, &store);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_dump_indexes(self, &store);
    if (ret != 0) {
        goto out;
    }
    ret = kastore_close(&store);
    if (ret != 0) {
        ret = tsk_set_kas_error(ret);
    }
out:
    /* It's safe to close a kastore twice. */
    if (ret != 0) {
        kastore_close(&store);
    }
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_simplify(tsk_table_collection_t *self,
        tsk_id_t *samples, tsk_size_t num_samples, tsk_flags_t options, tsk_id_t *node_map)
{
    int ret = 0;
    simplifier_t simplifier;
    tsk_id_t *local_samples = NULL;
    tsk_id_t u;

    if (samples == NULL) {
        /* Avoid issue with mallocing zero bytes */
        local_samples = malloc((1 + self->nodes.num_rows) * sizeof(*local_samples));
        if (local_samples == NULL) {
            ret = TSK_ERR_NO_MEMORY;
            goto out;
        }
        num_samples = 0;
        for (u = 0; u < (tsk_id_t) self->nodes.num_rows; u++) {
            if (!!(self->nodes.flags[u] & TSK_NODE_IS_SAMPLE)) {
                local_samples[num_samples] = u;
                num_samples++;
            }
        }
        samples = local_samples;
    }

    ret = simplifier_init(&simplifier, samples, (size_t) num_samples, self, options);
    if (ret != 0) {
        goto out;
    }
    ret = simplifier_run(&simplifier, node_map);
    if (ret != 0) {
        goto out;
    }
    if (!! (options & TSK_DEBUG)) {
        simplifier_print_state(&simplifier, stdout);
    }
    /* The indexes are invalidated now so drop them */
    ret = tsk_table_collection_drop_index(self, 0);
out:
    simplifier_free(&simplifier);
    tsk_safe_free(local_samples);
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_sort(tsk_table_collection_t *self, tsk_bookmark_t *start,
        tsk_flags_t options)
{
    int ret = 0;
    table_sorter_t sorter;
    tsk_size_t edge_start = 0;

    /* Must init the sorter before we check errors */
    ret = table_sorter_init(&sorter, self, options);
    if (ret != 0) {
        goto out;
    }
    if (start != NULL) {
        if (start->edges > self->edges.num_rows) {
            ret = TSK_ERR_EDGE_OUT_OF_BOUNDS;
            goto out;
        }
        edge_start = start->edges;

        /* For now, if migrations, sites or mutations are non-zero we get an error.
         * This should be fixed: https://github.com/tskit-dev/tskit/issues/101
         */
        if (start->migrations != 0 || start->sites != 0 || start->mutations != 0) {
            ret = TSK_ERR_SORT_OFFSET_NOT_SUPPORTED;
            goto out;
        }
    }
    ret = table_sorter_run(&sorter, edge_start);
    if (ret != 0) {
        goto out;
    }
    /* The indexes are invalidated now so drop them */
    ret = tsk_table_collection_drop_index(self, 0);
out:
    table_sorter_free(&sorter);
    return ret;
}

/*
 * Remove any sites with duplicate positions, retaining only the *first*
 * one. Assumes the tables have been sorted, throwing an error if not.
 */
int TSK_WARN_UNUSED
tsk_table_collection_deduplicate_sites(tsk_table_collection_t *self,
        tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_size_t j;
    /* Map of old site IDs to new site IDs. */
    tsk_id_t *site_id_map = NULL;
    tsk_site_table_t copy;
    tsk_site_t row, last_row;

    /* Early exit if there's 0 rows. We don't exit early for one row because
     * we would then skip error checking, making the semantics inconsistent. */
    if (self->sites.num_rows == 0) {
        return 0;
    }

    /* Must allocate the site table first for tsk_site_table_free to be safe */
    ret = tsk_site_table_copy(&self->sites, &copy, 0);
    if (ret != 0) {
        goto out;
    }
    /* Check everything except site duplicates (which we expect) and
     * edge indexes (which we don't use) */
    ret = tsk_table_collection_check_integrity(self,
            TSK_CHECK_ALL & ~TSK_CHECK_SITE_DUPLICATES & ~TSK_CHECK_INDEXES);
    if (ret != 0) {
        goto out;
    }

    site_id_map = malloc(copy.num_rows * sizeof(*site_id_map));
    if (site_id_map == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    ret = tsk_site_table_clear(&self->sites);
    if (ret != 0) {
        goto out;
    }

    last_row.position = -1;
    site_id_map[0] = 0;
    for (j = 0; j < copy.num_rows; j++) {
        ret = tsk_site_table_get_row(&copy, (tsk_id_t) j, &row);
        if (ret != 0) {
            goto out;
        }
        if (row.position != last_row.position) {
            ret = tsk_site_table_add_row(&self->sites, row.position, row.ancestral_state,
                row.ancestral_state_length, row.metadata, row.metadata_length);
            if (ret < 0) {
                goto out;
            }
        }
        site_id_map[j] = (tsk_id_t) self->sites.num_rows - 1;
        last_row = row;
    }

    if (self->sites.num_rows < copy.num_rows) {
        // Remap sites in the mutation table
        // (but only if there's been any changed sites)
        for (j = 0; j < self->mutations.num_rows; j++) {
            self->mutations.site[j] = site_id_map[self->mutations.site[j]];
        }
    }
    ret = 0;
out:
    tsk_site_table_free(&copy);
    tsk_safe_free(site_id_map);
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_compute_mutation_parents(tsk_table_collection_t *self,
        tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    const tsk_id_t *I, *O;
    const tsk_edge_table_t edges = self->edges;
    const tsk_node_table_t nodes = self->nodes;
    const tsk_site_table_t sites = self->sites;
    const tsk_mutation_table_t mutations = self->mutations;
    const tsk_id_t M = (tsk_id_t) edges.num_rows;
    tsk_id_t tj, tk;
    tsk_id_t *parent = NULL;
    tsk_id_t *bottom_mutation = NULL;
    tsk_id_t u;
    double left, right;
    tsk_id_t site;
    /* Using unsigned values here avoids potentially undefined behaviour */
    uint32_t j, mutation, first_mutation;

    /* Note that because we check everything here, any non-null mutation parents
     * will also be checked, even though they are about to be overwritten. To
     * ensure that his function always succeeds we must ensure that the
     * parent field is set to -1 first. */
    ret = tsk_table_collection_check_integrity(self, TSK_CHECK_ALL);
    if (ret != 0) {
        goto out;
    }
    parent = malloc(nodes.num_rows * sizeof(*parent));
    bottom_mutation = malloc(nodes.num_rows * sizeof(*bottom_mutation));
    if (parent == NULL || bottom_mutation == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    memset(parent, 0xff, nodes.num_rows * sizeof(*parent));
    memset(bottom_mutation, 0xff, nodes.num_rows * sizeof(*bottom_mutation));
    memset(mutations.parent, 0xff, self->mutations.num_rows * sizeof(tsk_id_t));

    I = self->indexes.edge_insertion_order;
    O = self->indexes.edge_removal_order;
    tj = 0;
    tk = 0;
    site = 0;
    mutation = 0;
    left = 0;
    while (tj < M || left < self->sequence_length) {
        while (tk < M && edges.right[O[tk]] == left) {
            parent[edges.child[O[tk]]] = TSK_NULL;
            tk++;
        }
        while (tj < M && edges.left[I[tj]] == left) {
            parent[edges.child[I[tj]]] = edges.parent[I[tj]];
            tj++;
        }
        right = self->sequence_length;
        if (tj < M) {
            right = TSK_MIN(right, edges.left[I[tj]]);
        }
        if (tk < M) {
            right = TSK_MIN(right, edges.right[O[tk]]);
        }

        /* Tree is now ready. We look at each site on this tree in turn */
        while (site < (tsk_id_t) sites.num_rows && sites.position[site] < right) {
            /* Create a mapping from mutations to nodes. If we see more than one
             * mutation at a node, the previously seen one must be the parent
             * of the current since we assume they are in order. */
            first_mutation = mutation;
            while (mutation < mutations.num_rows && mutations.site[mutation] == site) {
                u = mutations.node[mutation];
                if (bottom_mutation[u] != TSK_NULL) {
                    mutations.parent[mutation] = bottom_mutation[u];
                }
                bottom_mutation[u] = (tsk_id_t) mutation;
                mutation++;
            }
            /* Make the common case of 1 mutation fast */
            if (mutation > first_mutation + 1) {
                /* If we have more than one mutation, compute the parent for each
                 * one by traversing up the tree until we find a node that has a
                 * mutation. */
                for (j = first_mutation; j < mutation; j++) {
                    if (mutations.parent[j] == TSK_NULL) {
                        u = parent[mutations.node[j]];
                        while (u != TSK_NULL && bottom_mutation[u] == TSK_NULL) {
                            u = parent[u];
                        }
                        if (u != TSK_NULL) {
                            mutations.parent[j] = bottom_mutation[u];
                        }
                    }
                }
            }
            /* Reset the mapping for the next site */
            for (j = first_mutation; j < mutation; j++) {
                u = mutations.node[j];
                bottom_mutation[u] = TSK_NULL;
                /* Check that we haven't violated the sortedness property */
                if (mutations.parent[j] > (tsk_id_t) j) {
                    ret = TSK_ERR_MUTATION_PARENT_AFTER_CHILD;
                    goto out;
                }
            }
            site++;
        }
        /* Move on to the next tree */
        left = right;
    }

out:
    tsk_safe_free(parent);
    tsk_safe_free(bottom_mutation);
    return ret;
}

int
tsk_table_collection_record_num_rows(tsk_table_collection_t *self,
        tsk_bookmark_t *position)
{
    position->individuals = self->individuals.num_rows;
    position->nodes = self->nodes.num_rows;
    position->edges = self->edges.num_rows;
    position->migrations = self->migrations.num_rows;
    position->sites = self->sites.num_rows;
    position->mutations = self->mutations.num_rows;
    position->populations = self->populations.num_rows;
    position->provenances = self->provenances.num_rows;
    return 0;
}

int TSK_WARN_UNUSED
tsk_table_collection_truncate(tsk_table_collection_t *tables,
        tsk_bookmark_t *position)
{
    int ret = 0;

    ret = tsk_table_collection_drop_index(tables, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_individual_table_truncate(&tables->individuals, position->individuals);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_node_table_truncate(&tables->nodes, position->nodes);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_edge_table_truncate(&tables->edges, position->edges);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_migration_table_truncate(&tables->migrations, position->migrations);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_site_table_truncate(&tables->sites, position->sites);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_mutation_table_truncate(&tables->mutations, position->mutations);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_population_table_truncate(&tables->populations, position->populations);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_provenance_table_truncate(&tables->provenances, position->provenances);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_clear(tsk_table_collection_t *self)
{
    tsk_bookmark_t start;

    memset(&start, 0, sizeof(start));
    return tsk_table_collection_truncate(self, &start);
}

static int
cmp_edge_cl(const void *a, const void *b) {
    const tsk_edge_t *ia = (const tsk_edge_t *) a;
    const tsk_edge_t *ib = (const tsk_edge_t *) b;
    int ret = (ia->child > ib->child) - (ia->child < ib->child);
    if (ret == 0)  {
        ret = (ia->left > ib->left) - (ia->left < ib->left);
    }
    return ret;
}

/* Squash the edges in the specified array in place. The output edges will
 * be sorted by (child_id, left).
 */
int TSK_WARN_UNUSED
tsk_squash_edges(tsk_edge_t *edges, size_t num_edges, size_t *num_output_edges)
{
    int ret = 0;
    size_t j, k, l;
    tsk_edge_t e;

    qsort(edges, num_edges, sizeof(tsk_edge_t), cmp_edge_cl);
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
