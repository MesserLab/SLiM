/*
 * MIT License
 *
 * Copyright (c) 2019-2025 Tskit Developers
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

#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>

#include <tskit/tables.h>

#define TABLE_SEP "-----------------------------------------\n"

#define TSK_COL_OPTIONAL (1 << 0)

typedef struct {
    const char *name;
    void **array_dest;
    int type;
    tsk_flags_t options;
} read_table_col_t;

typedef struct {
    const char *name;
    void **data_array_dest;
    tsk_size_t *data_len_dest;
    int data_type;
    tsk_size_t **offset_array_dest;
    tsk_flags_t options;
} read_table_ragged_col_t;

typedef struct {
    const char *name;
    void **array_dest;
    tsk_size_t *len_dest;
    int type;
    tsk_flags_t options;
} read_table_property_t;

typedef struct {
    const char *name;
    const void *array;
    tsk_size_t len;
    int type;
} write_table_col_t;

typedef struct {
    const char *name;
    const void *data_array;
    tsk_size_t data_len;
    int data_type;
    const tsk_size_t *offset_array;
    tsk_size_t num_rows;
} write_table_ragged_col_t;

/* Returns true if adding the specified number of rows would result in overflow.
 * Tables can support indexes from 0 to TSK_MAX_ID, and therefore could have at most
 * TSK_MAX_ID + 1 rows. However we limit to TSK_MAX_ID rows so that counts of rows
 * can fit in a tsk_id_t. */
static bool
check_table_overflow(tsk_size_t current_size, tsk_size_t additional_rows)
{
    tsk_size_t max_val = TSK_MAX_ID;
    return additional_rows > max_val || current_size > (max_val - additional_rows);
}

/* Returns true if adding the specified number of elements would result in overflow
 * of an offset column.
 */
static bool
check_offset_overflow(tsk_size_t current_size, tsk_size_t additional_elements)
{
    tsk_size_t max_val = TSK_MAX_SIZE;
    return additional_elements > max_val
           || current_size > (max_val - additional_elements);
}

#define TSK_NUM_ROWS_UNSET ((tsk_size_t) -1)
#define TSK_MAX_COL_NAME_LEN 64

static int
read_table_cols(kastore_t *store, tsk_size_t *num_rows, read_table_col_t *cols,
    tsk_flags_t TSK_UNUSED(flags))
{
    int ret = 0;
    size_t len;
    int type;
    read_table_col_t *col;

    for (col = cols; col->name != NULL; col++) {
        ret = kastore_containss(store, col->name);
        if (ret < 0) {
            ret = tsk_set_kas_error(ret);
            goto out;
        }
        if (ret == 1) {
            ret = kastore_gets(store, col->name, col->array_dest, &len, &type);
            if (ret != 0) {
                ret = tsk_set_kas_error(ret);
                goto out;
            }
            if (*num_rows == TSK_NUM_ROWS_UNSET) {
                *num_rows = (tsk_size_t) len;
            } else {
                if (*num_rows != (tsk_size_t) len) {
                    ret = tsk_trace_error(TSK_ERR_FILE_FORMAT);
                    goto out;
                }
            }
            if (type != col->type) {
                ret = tsk_trace_error(TSK_ERR_BAD_COLUMN_TYPE);
                goto out;
            }
        } else if (!(col->options & TSK_COL_OPTIONAL)) {
            ret = tsk_trace_error(TSK_ERR_REQUIRED_COL_NOT_FOUND);
            goto out;
        }
    }
out:
    return ret;
}

static int
cast_offset_array(read_table_ragged_col_t *col, uint32_t *source, tsk_size_t num_rows)
{
    int ret = 0;
    tsk_size_t len = num_rows + 1;
    tsk_size_t j;
    uint64_t *dest = tsk_malloc(len * sizeof(*dest));

    if (dest == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    *col->offset_array_dest = dest;
    for (j = 0; j < len; j++) {
        dest[j] = source[j];
    }
out:
    return ret;
}

static int
read_table_ragged_cols(kastore_t *store, tsk_size_t *num_rows,
    read_table_ragged_col_t *cols, tsk_flags_t TSK_UNUSED(flags))
{
    int ret = 0;
    size_t data_len = 0; // initial value unused, just to keep the compiler happy.
    size_t offset_len;
    int type;
    read_table_ragged_col_t *col;
    char offset_col_name[TSK_MAX_COL_NAME_LEN];
    bool data_col_present, offset_col_present;
    void *store_offset_array = NULL;
    tsk_size_t *offset_array;

    for (col = cols; col->name != NULL; col++) {
        ret = kastore_containss(store, col->name);
        if (ret < 0) {
            ret = tsk_set_kas_error(ret);
            goto out;
        }
        data_col_present = false;
        if (ret == 1) {
            ret = kastore_gets(store, col->name, col->data_array_dest, &data_len, &type);
            if (ret != 0) {
                ret = tsk_set_kas_error(ret);
                goto out;
            }
            if (type != col->data_type) {
                ret = tsk_trace_error(TSK_ERR_BAD_COLUMN_TYPE);
                goto out;
            }
            *col->data_len_dest = (tsk_size_t) data_len;
            data_col_present = true;
        } else if (!(col->options & TSK_COL_OPTIONAL)) {
            ret = tsk_trace_error(TSK_ERR_REQUIRED_COL_NOT_FOUND);
            goto out;
        }

        assert(strlen(col->name) + strlen("_offset") + 2 < sizeof(offset_col_name));
        strcpy(offset_col_name, col->name);
        strcat(offset_col_name, "_offset");

        ret = kastore_containss(store, offset_col_name);
        if (ret < 0) {
            ret = tsk_set_kas_error(ret);
            goto out;
        }
        offset_col_present = ret == 1;
        if (offset_col_present != data_col_present) {
            ret = tsk_trace_error(TSK_ERR_BOTH_COLUMNS_REQUIRED);
            goto out;
        }
        if (offset_col_present) {
            ret = kastore_gets(
                store, offset_col_name, &store_offset_array, &offset_len, &type);
            if (ret != 0) {
                ret = tsk_set_kas_error(ret);
                goto out;
            }
            /* A table with zero rows will still have an offset length of 1;
             * catching this here prevents underflows in the logic below */
            if (offset_len == 0) {
                ret = tsk_trace_error(TSK_ERR_FILE_FORMAT);
                goto out;
            }
            /* Some tables have only ragged columns */
            if (*num_rows == TSK_NUM_ROWS_UNSET) {
                *num_rows = (tsk_size_t) offset_len - 1;
            } else {
                if (*num_rows != (tsk_size_t) offset_len - 1) {
                    ret = tsk_trace_error(TSK_ERR_FILE_FORMAT);
                    goto out;
                }
            }
            if (type == KAS_UINT64) {
                *col->offset_array_dest = (uint64_t *) store_offset_array;
                store_offset_array = NULL;
            } else if (type == KAS_UINT32) {
                ret = cast_offset_array(col, (uint32_t *) store_offset_array, *num_rows);
                if (ret != 0) {
                    goto out;
                }
                tsk_safe_free(store_offset_array);
                store_offset_array = NULL;
            } else {
                ret = tsk_trace_error(TSK_ERR_BAD_COLUMN_TYPE);
                goto out;
            }
            offset_array = *col->offset_array_dest;
            if (offset_array[*num_rows] != (tsk_size_t) data_len) {
                ret = tsk_trace_error(TSK_ERR_BAD_OFFSET);
                goto out;
            }
        }
    }
out:
    tsk_safe_free(store_offset_array);
    return ret;
}

static int
read_table_properties(
    kastore_t *store, read_table_property_t *properties, tsk_flags_t TSK_UNUSED(flags))
{
    int ret = 0;
    size_t len;
    int type;
    read_table_property_t *property;

    for (property = properties; property->name != NULL; property++) {
        ret = kastore_containss(store, property->name);
        if (ret < 0) {
            ret = tsk_set_kas_error(ret);
            goto out;
        }
        if (ret == 1) {
            ret = kastore_gets(store, property->name, property->array_dest, &len, &type);
            if (ret != 0) {
                ret = tsk_set_kas_error(ret);
                assert(ret != 0); /* Tell static analysers that we're handling errors */
                goto out;
            }
            if (type != property->type) {
                ret = tsk_trace_error(TSK_ERR_BAD_COLUMN_TYPE);
                goto out;
            }
            *property->len_dest = (tsk_size_t) len;
        }
        assert(property->options & TSK_COL_OPTIONAL);
    }
out:
    return ret;
}

static int
read_table(kastore_t *store, tsk_size_t *num_rows, read_table_col_t *cols,
    read_table_ragged_col_t *ragged_cols, read_table_property_t *properties,
    tsk_flags_t options)
{
    int ret = 0;

    *num_rows = TSK_NUM_ROWS_UNSET;
    if (cols != NULL) {
        ret = read_table_cols(store, num_rows, cols, options);
        if (ret != 0) {
            goto out;
        }
    }
    if (ragged_cols != NULL) {
        ret = read_table_ragged_cols(store, num_rows, ragged_cols, options);
        if (ret != 0) {
            goto out;
        }
    }
    if (*num_rows == TSK_NUM_ROWS_UNSET) {
        ret = tsk_trace_error(TSK_ERR_FILE_FORMAT);
        goto out;
    }
    if (properties != NULL) {
        ret = read_table_properties(store, properties, options);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

static void
free_read_table_mem(read_table_col_t *cols, read_table_ragged_col_t *ragged_cols,
    read_table_property_t *properties)
{
    read_table_col_t *col;
    read_table_ragged_col_t *ragged_col;
    read_table_property_t *property;

    if (cols != NULL) {
        for (col = cols; col->name != NULL; col++) {
            tsk_safe_free(*(col->array_dest));
        }
    }
    if (ragged_cols != NULL) {
        for (ragged_col = ragged_cols; ragged_col->name != NULL; ragged_col++) {
            tsk_safe_free(*(ragged_col->data_array_dest));
            tsk_safe_free(*(ragged_col->offset_array_dest));
        }
    }
    if (properties != NULL) {
        for (property = properties; property->name != NULL; property++) {
            tsk_safe_free(*(property->array_dest));
        }
    }
}

static int
write_offset_col(
    kastore_t *store, const write_table_ragged_col_t *col, tsk_flags_t options)
{
    int ret = 0;
    char offset_col_name[TSK_MAX_COL_NAME_LEN];
    uint32_t *offset32 = NULL;
    tsk_size_t len = col->num_rows + 1;
    tsk_size_t j;
    int32_t put_flags = 0;
    int type;
    const void *data;
    bool needs_64 = col->offset_array[col->num_rows] > UINT32_MAX;

    assert(strlen(col->name) + strlen("_offset") + 2 < sizeof(offset_col_name));
    strcpy(offset_col_name, col->name);
    strcat(offset_col_name, "_offset");

    if (options & TSK_DUMP_FORCE_OFFSET_64 || needs_64) {
        type = KAS_UINT64;
        data = col->offset_array;
        put_flags = KAS_BORROWS_ARRAY;
    } else {
        offset32 = tsk_malloc(len * sizeof(*offset32));
        if (offset32 == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
        for (j = 0; j < len; j++) {
            offset32[j] = (uint32_t) col->offset_array[j];
        }
        type = KAS_UINT32;
        data = offset32;
        /* We've just allocated a temp buffer, so kas can't borrow so leave put_flags=0*/
    }
    ret = kastore_puts(store, offset_col_name, data, (size_t) len, type, put_flags);
    if (ret != 0) {
        ret = tsk_set_kas_error(ret);
        goto out;
    }
out:
    tsk_safe_free(offset32);
    return ret;
}

static int
write_table_ragged_cols(
    kastore_t *store, const write_table_ragged_col_t *write_cols, tsk_flags_t options)
{
    int ret = 0;
    const write_table_ragged_col_t *col;

    for (col = write_cols; col->name != NULL; col++) {
        ret = kastore_puts(store, col->name, col->data_array, (size_t) col->data_len,
            col->data_type, KAS_BORROWS_ARRAY);
        if (ret != 0) {
            ret = tsk_set_kas_error(ret);
            goto out;
        }
        ret = write_offset_col(store, col, options);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

static int
write_table_cols(kastore_t *store, const write_table_col_t *write_cols,
    tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    const write_table_col_t *col;

    for (col = write_cols; col->name != NULL; col++) {
        ret = kastore_puts(store, col->name, col->array, (size_t) col->len, col->type,
            KAS_BORROWS_ARRAY);
        if (ret != 0) {
            ret = tsk_set_kas_error(ret);
            goto out;
        }
    }
out:
    return ret;
}

static int
write_table(kastore_t *store, const write_table_col_t *cols,
    const write_table_ragged_col_t *ragged_cols, tsk_flags_t options)
{
    int ret = write_table_cols(store, cols, options);

    if (ret != 0) {
        goto out;
    }
    ret = write_table_ragged_cols(store, ragged_cols, options);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

/* Checks that the specified list of offsets is well-formed. */
static int
check_offsets(
    tsk_size_t num_rows, const tsk_size_t *offsets, tsk_size_t length, bool check_length)
{
    int ret = 0;
    tsk_size_t j;

    if (offsets[0] != 0) {
        ret = tsk_trace_error(TSK_ERR_BAD_OFFSET);
        goto out;
    }
    if (check_length && offsets[num_rows] != length) {
        ret = tsk_trace_error(TSK_ERR_BAD_OFFSET);
        goto out;
    }
    for (j = 0; j < num_rows; j++) {
        if (offsets[j] > offsets[j + 1]) {
            ret = tsk_trace_error(TSK_ERR_BAD_OFFSET);
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

static int
calculate_max_rows(tsk_size_t num_rows, tsk_size_t max_rows,
    tsk_size_t max_rows_increment, tsk_size_t additional_rows,
    tsk_size_t *ret_new_max_rows)
{
    tsk_size_t new_max_rows;
    int ret = 0;

    if (check_table_overflow(num_rows, additional_rows)) {
        ret = tsk_trace_error(TSK_ERR_TABLE_OVERFLOW);
        goto out;
    }

    if (num_rows + additional_rows <= max_rows) {
        new_max_rows = max_rows;
    } else {
        if (max_rows_increment == 0) {
            /* Doubling by default */
            new_max_rows = TSK_MIN(max_rows * 2, TSK_MAX_ID + (tsk_size_t) 1);
            /* Add some constraints to prevent very small allocations */
            if (new_max_rows < 1024) {
                new_max_rows = 1024;
            }
            /* Prevent allocating more than ~2 million additional rows unless needed*/
            if (new_max_rows - max_rows > 2097152) {
                new_max_rows = max_rows + 2097152;
            }
        } else {
            /* Use user increment value */
            if (check_table_overflow(max_rows, max_rows_increment)) {
                ret = tsk_trace_error(TSK_ERR_TABLE_OVERFLOW);
                goto out;
            }
            new_max_rows = max_rows + max_rows_increment;
        }
        new_max_rows = TSK_MAX(new_max_rows, num_rows + additional_rows);
    }
    *ret_new_max_rows = new_max_rows;
out:
    return ret;
}

static int
calculate_max_length(tsk_size_t current_length, tsk_size_t max_length,
    tsk_size_t max_length_increment, tsk_size_t additional_length,
    tsk_size_t *ret_new_max_length)
{
    tsk_size_t new_max_length;
    int ret = 0;

    if (check_offset_overflow(current_length, additional_length)) {
        ret = tsk_trace_error(TSK_ERR_COLUMN_OVERFLOW);
        goto out;
    }

    if (current_length + additional_length <= max_length) {
        new_max_length = max_length;
    } else {
        if (max_length_increment == 0) {
            /* Doubling by default */
            new_max_length = TSK_MIN(max_length * 2, TSK_MAX_SIZE);
            /* Add some constraints to prevent very small allocations */
            if (new_max_length < 65536) {
                new_max_length = 65536;
            }
            /* Prevent allocating more than 100MB additional unless needed*/
            if (new_max_length - max_length > 104857600) {
                new_max_length = max_length + 104857600;
            }
            new_max_length = TSK_MAX(new_max_length, current_length + additional_length);
        } else {
            /* Use user increment value */
            if (check_offset_overflow(max_length, max_length_increment)) {
                /* Here we could allocate to the maximum size.
                 * Instead we are erroring out as this is much easier to test.
                 * The cost is that (at most) the last "max_length_increment"-1
                 * bytes of the possible array space can't be used. */
                ret = tsk_trace_error(TSK_ERR_COLUMN_OVERFLOW);
                goto out;
            }
            new_max_length = max_length + max_length_increment;
        }
        new_max_length = TSK_MAX(new_max_length, current_length + additional_length);
    }
    *ret_new_max_length = new_max_length;
out:
    return ret;
}

static int
expand_column(void **column, tsk_size_t new_max_rows, size_t element_size)
{
    int ret = 0;
    void *tmp;

    tmp = tsk_realloc((void **) *column, new_max_rows * element_size);
    if (tmp == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    *column = tmp;
out:
    return ret;
}

static int
expand_ragged_column(tsk_size_t current_length, tsk_size_t additional_length,
    tsk_size_t max_length_increment, tsk_size_t *max_length, void **column,
    size_t element_size)
{
    int ret = 0;
    tsk_size_t new_max_length;

    ret = calculate_max_length(current_length, *max_length, max_length_increment,
        additional_length, &new_max_length);
    if (ret != 0) {
        goto out;
    }

    if (new_max_length > *max_length) {
        ret = expand_column(column, new_max_length, element_size);
        if (ret != 0) {
            goto out;
        }
        *max_length = new_max_length;
    }
out:
    return ret;
}

/* TODO rename to copy_string or replace_and_copy_string */
static int
replace_string(
    char **str, tsk_size_t *len, const char *new_str, const tsk_size_t new_len)
{
    int ret = 0;
    tsk_safe_free(*str);
    *str = NULL;
    *len = new_len;
    if (new_len > 0) {
        *str = tsk_malloc(new_len * sizeof(char));
        if (*str == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
        tsk_memcpy(*str, new_str, new_len * sizeof(char));
    }
out:
    return ret;
}

static int
takeset_string(char **str, tsk_size_t *len, char *new_str, const tsk_size_t new_len)
{
    tsk_safe_free(*str);
    *str = new_str;
    *len = new_len;
    return 0;
}

static int
alloc_empty_ragged_column(tsk_size_t num_rows, void **data_col, tsk_size_t **offset_col)
{
    int ret = 0;

    *data_col = tsk_malloc(1);
    *offset_col = tsk_calloc(num_rows + 1, sizeof(tsk_size_t));
    if (*data_col == NULL || *offset_col == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
out:
    return ret;
}

static int
check_ragged_column(tsk_size_t num_rows, void *data, tsk_size_t *offset)
{
    int ret = 0;
    if ((data == NULL) != (offset == NULL)) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    if (data != NULL) {
        ret = check_offsets(num_rows, offset, 0, false);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

static int
takeset_ragged_column(tsk_size_t num_rows, void *data, tsk_size_t *offset,
    void **data_dest, tsk_size_t **offset_dest, tsk_size_t *length_dest)
{
    int ret = 0;
    if (data == NULL) {
        ret = alloc_empty_ragged_column(num_rows, (void *) data_dest, offset_dest);
        if (ret != 0) {
            goto out;
        }
    } else {
        *data_dest = data;
        *offset_dest = offset;
    }
    *length_dest = (*offset_dest)[num_rows];
out:
    return ret;
}

static int
takeset_optional_id_column(tsk_size_t num_rows, tsk_id_t *input, tsk_id_t **dest)
{
    int ret = 0;
    tsk_size_t buffsize;
    tsk_id_t *buff;

    if (input == NULL) {
        buffsize = num_rows * sizeof(*buff);
        buff = tsk_malloc(buffsize);
        if (buff == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
        *dest = buff;
        tsk_memset(buff, 0xff, buffsize);
    } else {
        *dest = input;
    }
out:
    return ret;
}

static int
write_metadata_schema_header(
    FILE *out, const char *metadata_schema, tsk_size_t metadata_schema_length)
{
    const char *fmt = "#metadata_schema#\n"
                      "%.*s\n"
                      "#end#metadata_schema\n" TABLE_SEP;
    return fprintf(out, fmt, (int) metadata_schema_length, metadata_schema);
}

/* Utilities for in-place subsetting columns */

static tsk_size_t
count_true(tsk_size_t num_rows, const tsk_bool_t *restrict keep)
{
    tsk_size_t j;
    tsk_size_t count = 0;

    for (j = 0; j < num_rows; j++) {
        if (keep[j]) {
            count++;
        }
    }
    return count;
}

static void
keep_mask_to_id_map(
    tsk_size_t num_rows, const tsk_bool_t *restrict keep, tsk_id_t *restrict id_map)
{
    tsk_size_t j;
    tsk_id_t next_id = 0;

    for (j = 0; j < num_rows; j++) {
        id_map[j] = TSK_NULL;
        if (keep[j]) {
            id_map[j] = next_id;
            next_id++;
        }
    }
}

static tsk_size_t
subset_remap_id_column(tsk_id_t *restrict column, tsk_size_t num_rows,
    const tsk_bool_t *restrict keep, const tsk_id_t *restrict id_map)
{
    tsk_size_t j, k;
    tsk_id_t value;

    k = 0;
    for (j = 0; j < num_rows; j++) {
        if (keep[j]) {
            value = column[j];
            if (value != TSK_NULL) {
                value = id_map[value];
            }
            column[k] = value;
            k++;
        }
    }
    return k;
}

/* Trigger warning: C++ programmers should look away... This may be one of the
 * few cases where some macro funkiness is warranted, as these are exact
 * duplicates of the same function with just the type of the column
 * parameter changed. */

static tsk_size_t
subset_id_column(
    tsk_id_t *restrict column, tsk_size_t num_rows, const tsk_bool_t *restrict keep)
{
    tsk_size_t j, k;

    k = 0;
    for (j = 0; j < num_rows; j++) {
        if (keep[j]) {
            column[k] = column[j];
            k++;
        }
    }
    return k;
}

static tsk_size_t
subset_flags_column(
    tsk_flags_t *restrict column, tsk_size_t num_rows, const tsk_bool_t *restrict keep)
{
    tsk_size_t j, k;

    k = 0;
    for (j = 0; j < num_rows; j++) {
        if (keep[j]) {
            column[k] = column[j];
            k++;
        }
    }
    return k;
}

static tsk_size_t
subset_double_column(
    double *restrict column, tsk_size_t num_rows, const tsk_bool_t *restrict keep)
{
    tsk_size_t j, k;

    k = 0;
    for (j = 0; j < num_rows; j++) {
        if (keep[j]) {
            column[k] = column[j];
            k++;
        }
    }
    return k;
}

static tsk_size_t
subset_ragged_char_column(char *restrict data, tsk_size_t *restrict offset_col,
    tsk_size_t num_rows, const tsk_bool_t *restrict keep)
{
    tsk_size_t j, k, i, offset;

    k = 0;
    offset = 0;
    for (j = 0; j < num_rows; j++) {
        if (keep[j]) {
            offset_col[k] = offset;
            /* Note: Unclear whether it's worth calling memcpy instead here?
             * Need to be careful since the regions are overlapping */
            for (i = offset_col[j]; i < offset_col[j + 1]; i++) {
                data[offset] = data[i];
                offset++;
            }
            k++;
        }
    }
    offset_col[k] = offset;
    return offset;
}

static tsk_size_t
subset_ragged_double_column(double *restrict data, tsk_size_t *restrict offset_col,
    tsk_size_t num_rows, const tsk_bool_t *restrict keep)
{
    tsk_size_t j, k, i, offset;

    k = 0;
    offset = 0;
    for (j = 0; j < num_rows; j++) {
        if (keep[j]) {
            offset_col[k] = offset;
            /* Note: Unclear whether it's worth calling memcpy instead here?
             * Need to be careful since the regions are overlapping */
            for (i = offset_col[j]; i < offset_col[j + 1]; i++) {
                data[offset] = data[i];
                offset++;
            }
            k++;
        }
    }
    offset_col[k] = offset;
    return offset;
}

static tsk_size_t
subset_remap_ragged_id_column(tsk_id_t *restrict data, tsk_size_t *restrict offset_col,
    tsk_size_t num_rows, const tsk_bool_t *restrict keep,
    const tsk_id_t *restrict id_map)
{
    tsk_size_t j, k, i, offset;
    tsk_id_t di;

    k = 0;
    offset = 0;
    for (j = 0; j < num_rows; j++) {
        if (keep[j]) {
            offset_col[k] = offset;
            for (i = offset_col[j]; i < offset_col[j + 1]; i++) {
                di = data[i];
                if (di != TSK_NULL) {
                    di = id_map[di];
                }
                data[offset] = di;
                offset++;
            }
            k++;
        }
    }
    offset_col[k] = offset;
    return offset;
}

/*************************
 * reference sequence
 *************************/

int
tsk_reference_sequence_init(
    tsk_reference_sequence_t *self, tsk_flags_t TSK_UNUSED(options))
{
    tsk_memset(self, 0, sizeof(*self));
    return 0;
}

int
tsk_reference_sequence_free(tsk_reference_sequence_t *self)
{
    tsk_safe_free(self->data);
    tsk_safe_free(self->url);
    tsk_safe_free(self->metadata);
    tsk_safe_free(self->metadata_schema);
    return 0;
}

bool
tsk_reference_sequence_is_null(const tsk_reference_sequence_t *self)
{
    return self->data_length == 0 && self->url_length == 0 && self->metadata_length == 0
           && self->metadata_schema_length == 0;
}

bool
tsk_reference_sequence_equals(const tsk_reference_sequence_t *self,
    const tsk_reference_sequence_t *other, tsk_flags_t options)
{
    int ret
        = self->data_length == other->data_length
          && self->url_length == other->url_length
          && tsk_memcmp(self->data, other->data, self->data_length * sizeof(char)) == 0
          && tsk_memcmp(self->url, other->url, self->url_length * sizeof(char)) == 0;

    if (!(options & TSK_CMP_IGNORE_METADATA)) {
        ret = ret && self->metadata_length == other->metadata_length
              && self->metadata_schema_length == other->metadata_schema_length
              && tsk_memcmp(self->metadata, other->metadata,
                     self->metadata_length * sizeof(char))
                     == 0
              && tsk_memcmp(self->metadata_schema, other->metadata_schema,
                     self->metadata_schema_length * sizeof(char))
                     == 0;
    }
    return ret;
}

int
tsk_reference_sequence_copy(const tsk_reference_sequence_t *self,
    tsk_reference_sequence_t *dest, tsk_flags_t options)
{
    int ret = 0;

    if (!(options & TSK_NO_INIT)) {
        ret = tsk_reference_sequence_init(dest, 0);
        if (ret != 0) {
            goto out;
        }
    }

    if (tsk_reference_sequence_is_null(self)) {
        /* This is a simple way to get any input into the NULL state */
        tsk_reference_sequence_free(dest);
    } else {
        ret = tsk_reference_sequence_set_data(dest, self->data, self->data_length);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_reference_sequence_set_url(dest, self->url, self->url_length);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_reference_sequence_set_metadata(
            dest, self->metadata, self->metadata_length);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_reference_sequence_set_metadata_schema(
            dest, self->metadata_schema, self->metadata_schema_length);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

int
tsk_reference_sequence_set_data(
    tsk_reference_sequence_t *self, const char *data, tsk_size_t data_length)
{
    return replace_string(&self->data, &self->data_length, data, data_length);
}

int
tsk_reference_sequence_set_url(
    tsk_reference_sequence_t *self, const char *url, tsk_size_t url_length)
{
    return replace_string(&self->url, &self->url_length, url, url_length);
}

int
tsk_reference_sequence_set_metadata(
    tsk_reference_sequence_t *self, const char *metadata, tsk_size_t metadata_length)
{
    return replace_string(
        &self->metadata, &self->metadata_length, metadata, metadata_length);
}

int
tsk_reference_sequence_set_metadata_schema(tsk_reference_sequence_t *self,
    const char *metadata_schema, tsk_size_t metadata_schema_length)
{
    return replace_string(&self->metadata_schema, &self->metadata_schema_length,
        metadata_schema, metadata_schema_length);
}

int
tsk_reference_sequence_takeset_data(
    tsk_reference_sequence_t *self, char *data, tsk_size_t data_length)
{
    return takeset_string(&self->data, &self->data_length, data, data_length);
}

int
tsk_reference_sequence_takeset_metadata(
    tsk_reference_sequence_t *self, char *metadata, tsk_size_t metadata_length)
{
    return takeset_string(
        &self->metadata, &self->metadata_length, metadata, metadata_length);
}

/*************************
 * individual table
 *************************/

static void
tsk_individual_table_free_columns(tsk_individual_table_t *self)
{
    tsk_safe_free(self->flags);
    tsk_safe_free(self->location);
    tsk_safe_free(self->location_offset);
    tsk_safe_free(self->parents);
    tsk_safe_free(self->parents_offset);
    tsk_safe_free(self->metadata);
    tsk_safe_free(self->metadata_offset);
}

int
tsk_individual_table_free(tsk_individual_table_t *self)
{
    tsk_individual_table_free_columns(self);
    tsk_safe_free(self->metadata_schema);
    return 0;
}

static int
tsk_individual_table_expand_main_columns(
    tsk_individual_table_t *self, tsk_size_t additional_rows)
{
    int ret = 0;
    tsk_size_t new_max_rows;

    ret = calculate_max_rows(self->num_rows, self->max_rows, self->max_rows_increment,
        additional_rows, &new_max_rows);
    if (ret != 0) {
        goto out;
    }
    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column((void **) &self->flags, new_max_rows, sizeof(tsk_flags_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column(
            (void **) &self->location_offset, new_max_rows + 1, sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column(
            (void **) &self->parents_offset, new_max_rows + 1, sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column(
            (void **) &self->metadata_offset, new_max_rows + 1, sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        self->max_rows = new_max_rows;
    }
out:
    return ret;
}

static int
tsk_individual_table_expand_location(
    tsk_individual_table_t *self, tsk_size_t additional_length)
{
    return expand_ragged_column(self->location_length, additional_length,
        self->max_location_length_increment, &self->max_location_length,
        (void **) &self->location, sizeof(*self->location));
}

static int
tsk_individual_table_expand_parents(
    tsk_individual_table_t *self, tsk_size_t additional_length)
{
    return expand_ragged_column(self->parents_length, additional_length,
        self->max_parents_length_increment, &self->max_parents_length,
        (void **) &self->parents, sizeof(*self->parents));
}

static int
tsk_individual_table_expand_metadata(
    tsk_individual_table_t *self, tsk_size_t additional_length)
{
    return expand_ragged_column(self->metadata_length, additional_length,
        self->max_metadata_length_increment, &self->max_metadata_length,
        (void **) &self->metadata, sizeof(*self->metadata));
}

int
tsk_individual_table_set_max_rows_increment(
    tsk_individual_table_t *self, tsk_size_t max_rows_increment)
{
    self->max_rows_increment = max_rows_increment;
    return 0;
}

int
tsk_individual_table_set_max_metadata_length_increment(
    tsk_individual_table_t *self, tsk_size_t max_metadata_length_increment)
{
    self->max_metadata_length_increment = (tsk_size_t) max_metadata_length_increment;
    return 0;
}

int
tsk_individual_table_set_max_location_length_increment(
    tsk_individual_table_t *self, tsk_size_t max_location_length_increment)
{
    self->max_location_length_increment = (tsk_size_t) max_location_length_increment;
    return 0;
}

int
tsk_individual_table_set_max_parents_length_increment(
    tsk_individual_table_t *self, tsk_size_t max_parents_length_increment)
{
    self->max_parents_length_increment = (tsk_size_t) max_parents_length_increment;
    return 0;
}

int
tsk_individual_table_init(tsk_individual_table_t *self, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;

    tsk_memset(self, 0, sizeof(tsk_individual_table_t));
    /* Allocate space for one row initially, ensuring we always have valid pointers
     * even if the table is empty */
    self->max_rows_increment = 1;
    self->max_location_length_increment = 1;
    self->max_parents_length_increment = 1;
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
    ret = tsk_individual_table_expand_parents(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->parents_offset[0] = 0;
    ret = tsk_individual_table_expand_metadata(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->metadata_offset[0] = 0;
    self->max_rows_increment = 0;
    self->max_location_length_increment = 0;
    self->max_parents_length_increment = 0;
    self->max_metadata_length_increment = 0;
    tsk_individual_table_set_metadata_schema(self, NULL, 0);
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_individual_table_copy(const tsk_individual_table_t *self,
    tsk_individual_table_t *dest, tsk_flags_t options)
{
    int ret = 0;

    if (!(options & TSK_NO_INIT)) {
        ret = tsk_individual_table_init(dest, 0);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_individual_table_set_columns(dest, self->num_rows, self->flags,
        self->location, self->location_offset, self->parents, self->parents_offset,
        self->metadata, self->metadata_offset);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_individual_table_set_metadata_schema(
        dest, self->metadata_schema, self->metadata_schema_length);
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_individual_table_set_columns(tsk_individual_table_t *self, tsk_size_t num_rows,
    const tsk_flags_t *flags, const double *location, const tsk_size_t *location_offset,
    const tsk_id_t *parents, const tsk_size_t *parents_offset, const char *metadata,
    const tsk_size_t *metadata_offset)
{
    int ret;

    ret = tsk_individual_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_individual_table_append_columns(self, num_rows, flags, location,
        location_offset, parents, parents_offset, metadata, metadata_offset);
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_individual_table_takeset_columns(tsk_individual_table_t *self, tsk_size_t num_rows,
    tsk_flags_t *flags, double *location, tsk_size_t *location_offset, tsk_id_t *parents,
    tsk_size_t *parents_offset, char *metadata, tsk_size_t *metadata_offset)
{
    int ret = 0;

    /* We need to check all the inputs before we start freeing or taking memory */
    ret = check_ragged_column(num_rows, location, location_offset);
    if (ret != 0) {
        goto out;
    }
    ret = check_ragged_column(num_rows, parents, parents_offset);
    if (ret != 0) {
        goto out;
    }
    ret = check_ragged_column(num_rows, metadata, metadata_offset);
    if (ret != 0) {
        goto out;
    }

    tsk_individual_table_free_columns(self);
    self->num_rows = num_rows;
    self->max_rows = num_rows;

    if (flags == NULL) {
        /* Flags defaults to all zeros if not specified. The column is often
         * unused so this is a worthwhile optimisation. */
        self->flags = tsk_calloc(num_rows, sizeof(*self->flags));
        if (self->flags == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
    } else {
        self->flags = flags;
    }

    ret = takeset_ragged_column(num_rows, location, location_offset,
        (void *) &self->location, &self->location_offset, &self->location_length);
    if (ret != 0) {
        goto out;
    }
    ret = takeset_ragged_column(num_rows, parents, parents_offset,
        (void *) &self->parents, &self->parents_offset, &self->parents_length);
    if (ret != 0) {
        goto out;
    }
    ret = takeset_ragged_column(num_rows, metadata, metadata_offset,
        (void *) &self->metadata, &self->metadata_offset, &self->metadata_length);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

int
tsk_individual_table_append_columns(tsk_individual_table_t *self, tsk_size_t num_rows,
    const tsk_flags_t *flags, const double *location, const tsk_size_t *location_offset,
    const tsk_id_t *parents, const tsk_size_t *parents_offset, const char *metadata,
    const tsk_size_t *metadata_offset)
{
    int ret;
    tsk_size_t j, metadata_length, location_length, parents_length;

    if (flags == NULL) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    if ((location == NULL) != (location_offset == NULL)) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    if ((parents == NULL) != (parents_offset == NULL)) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    if ((metadata == NULL) != (metadata_offset == NULL)) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    ret = tsk_individual_table_expand_main_columns(self, (tsk_size_t) num_rows);
    if (ret != 0) {
        goto out;
    }
    tsk_memcpy(self->flags + self->num_rows, flags, num_rows * sizeof(tsk_flags_t));
    if (location == NULL) {
        for (j = 0; j < num_rows; j++) {
            self->location_offset[self->num_rows + j + 1]
                = (tsk_size_t) self->location_length;
        }
    } else {
        ret = check_offsets(num_rows, location_offset, 0, false);
        if (ret != 0) {
            goto out;
        }
        for (j = 0; j < num_rows; j++) {
            self->location_offset[self->num_rows + j]
                = (tsk_size_t) self->location_length + location_offset[j];
        }
        location_length = location_offset[num_rows];
        ret = tsk_individual_table_expand_location(self, location_length);
        if (ret != 0) {
            goto out;
        }
        tsk_memcpy(self->location + self->location_length, location,
            location_length * sizeof(double));
        self->location_length += location_length;
    }
    if (parents == NULL) {
        for (j = 0; j < num_rows; j++) {
            self->parents_offset[self->num_rows + j + 1]
                = (tsk_size_t) self->parents_length;
        }
    } else {
        ret = check_offsets(num_rows, parents_offset, 0, false);
        if (ret != 0) {
            goto out;
        }
        for (j = 0; j < num_rows; j++) {
            self->parents_offset[self->num_rows + j]
                = (tsk_size_t) self->parents_length + parents_offset[j];
        }
        parents_length = parents_offset[num_rows];
        ret = tsk_individual_table_expand_parents(self, parents_length);
        if (ret != 0) {
            goto out;
        }
        tsk_memcpy(self->parents + self->parents_length, parents,
            parents_length * sizeof(tsk_id_t));
        self->parents_length += parents_length;
    }
    if (metadata == NULL) {
        for (j = 0; j < num_rows; j++) {
            self->metadata_offset[self->num_rows + j + 1]
                = (tsk_size_t) self->metadata_length;
        }
    } else {
        ret = check_offsets(num_rows, metadata_offset, 0, false);
        if (ret != 0) {
            goto out;
        }
        for (j = 0; j < num_rows; j++) {
            self->metadata_offset[self->num_rows + j]
                = (tsk_size_t) self->metadata_length + metadata_offset[j];
        }
        metadata_length = metadata_offset[num_rows];
        ret = tsk_individual_table_expand_metadata(self, metadata_length);
        if (ret != 0) {
            goto out;
        }
        tsk_memcpy(self->metadata + self->metadata_length, metadata,
            metadata_length * sizeof(char));
        self->metadata_length += metadata_length;
    }
    self->num_rows += (tsk_size_t) num_rows;
    self->location_offset[self->num_rows] = self->location_length;
    self->parents_offset[self->num_rows] = self->parents_length;
    self->metadata_offset[self->num_rows] = self->metadata_length;
out:
    return ret;
}

static tsk_id_t
tsk_individual_table_add_row_internal(tsk_individual_table_t *self, tsk_flags_t flags,
    const double *location, tsk_size_t location_length, const tsk_id_t *parents,
    const tsk_size_t parents_length, const char *metadata, tsk_size_t metadata_length)
{
    tsk_bug_assert(self->num_rows < self->max_rows);
    tsk_bug_assert(self->parents_length + parents_length <= self->max_parents_length);
    tsk_bug_assert(self->metadata_length + metadata_length <= self->max_metadata_length);
    tsk_bug_assert(self->location_length + location_length <= self->max_location_length);
    self->flags[self->num_rows] = flags;
    tsk_memmove(self->location + self->location_length, location,
        location_length * sizeof(*self->location));
    self->location_offset[self->num_rows + 1] = self->location_length + location_length;
    self->location_length += location_length;
    tsk_memmove(self->parents + self->parents_length, parents,
        parents_length * sizeof(*self->parents));
    self->parents_offset[self->num_rows + 1] = self->parents_length + parents_length;
    self->parents_length += parents_length;
    tsk_memmove(self->metadata + self->metadata_length, metadata,
        metadata_length * sizeof(*self->metadata));
    self->metadata_offset[self->num_rows + 1] = self->metadata_length + metadata_length;
    self->metadata_length += metadata_length;
    self->num_rows++;
    return (tsk_id_t) self->num_rows - 1;
}

tsk_id_t
tsk_individual_table_add_row(tsk_individual_table_t *self, tsk_flags_t flags,
    const double *location, tsk_size_t location_length, const tsk_id_t *parents,
    tsk_size_t parents_length, const char *metadata, tsk_size_t metadata_length)
{
    tsk_id_t ret = 0;

    ret = tsk_individual_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_individual_table_expand_location(self, location_length);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_individual_table_expand_parents(self, parents_length);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_individual_table_expand_metadata(self, metadata_length);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_individual_table_add_row_internal(self, flags, location, location_length,
        parents, parents_length, metadata, metadata_length);
out:
    return ret;
}

static int
tsk_individual_table_update_row_rewrite(tsk_individual_table_t *self, tsk_id_t index,
    tsk_flags_t flags, const double *location, tsk_size_t location_length,
    const tsk_id_t *parents, tsk_size_t parents_length, const char *metadata,
    tsk_size_t metadata_length)
{
    int ret = 0;
    tsk_id_t j, ret_id;
    tsk_individual_table_t copy;
    tsk_size_t num_rows;
    tsk_id_t *rows = NULL;

    ret = tsk_individual_table_copy(self, &copy, 0);
    if (ret != 0) {
        goto out;
    }
    rows = tsk_malloc(self->num_rows * sizeof(*rows));
    if (rows == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    ret = tsk_individual_table_truncate(self, (tsk_size_t) index);
    tsk_bug_assert(ret == 0);
    ret_id = tsk_individual_table_add_row(self, flags, location, location_length,
        parents, parents_length, metadata, metadata_length);
    if (ret_id < 0) {
        ret = (int) ret_id;
        goto out;
    }
    num_rows = 0;
    for (j = index + 1; j < (tsk_id_t) copy.num_rows; j++) {
        rows[num_rows] = j;
        num_rows++;
    }
    ret = tsk_individual_table_extend(self, &copy, num_rows, rows, 0);
    if (ret != 0) {
        goto out;
    }
out:
    tsk_individual_table_free(&copy);
    tsk_safe_free(rows);
    return ret;
}

int
tsk_individual_table_update_row(tsk_individual_table_t *self, tsk_id_t index,
    tsk_flags_t flags, const double *location, tsk_size_t location_length,
    const tsk_id_t *parents, tsk_size_t parents_length, const char *metadata,
    tsk_size_t metadata_length)
{
    int ret = 0;
    tsk_individual_t current_row;

    ret = tsk_individual_table_get_row(self, index, &current_row);
    if (ret != 0) {
        goto out;
    }
    if (current_row.location_length == location_length
        && current_row.parents_length == parents_length
        && current_row.metadata_length == metadata_length) {
        self->flags[index] = flags;
        /* Note: important to use tsk_memmove here as we may be provided pointers
         * to the column memory as input via get_row */
        tsk_memmove(&self->location[self->location_offset[index]], location,
            location_length * sizeof(*location));
        tsk_memmove(&self->parents[self->parents_offset[index]], parents,
            parents_length * sizeof(*parents));
        tsk_memmove(&self->metadata[self->metadata_offset[index]], metadata,
            metadata_length * sizeof(*metadata));
    } else {
        ret = tsk_individual_table_update_row_rewrite(self, index, flags, location,
            location_length, parents, parents_length, metadata, metadata_length);
        if (ret != 0) {
            goto out;
        }
    }
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
        ret = tsk_trace_error(TSK_ERR_BAD_TABLE_POSITION);
        goto out;
    }
    self->num_rows = num_rows;
    self->location_length = self->location_offset[num_rows];
    self->parents_length = self->parents_offset[num_rows];
    self->metadata_length = self->metadata_offset[num_rows];
out:
    return ret;
}

int
tsk_individual_table_extend(tsk_individual_table_t *self,
    const tsk_individual_table_t *other, tsk_size_t num_rows,
    const tsk_id_t *row_indexes, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_id_t ret_id;
    tsk_size_t j;
    tsk_individual_t individual;

    if (self == other) {
        ret = tsk_trace_error(TSK_ERR_CANNOT_EXTEND_FROM_SELF);
        goto out;
    }

    /* We know how much to expand the non-ragged columns, so do it ahead of time */
    ret = tsk_individual_table_expand_main_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < num_rows; j++) {
        ret = tsk_individual_table_get_row(
            other, row_indexes == NULL ? (tsk_id_t) j : row_indexes[j], &individual);
        if (ret != 0) {
            goto out;
        }
        ret_id = tsk_individual_table_add_row(self, individual.flags,
            individual.location, individual.location_length, individual.parents,
            individual.parents_length, individual.metadata, individual.metadata_length);
        if (ret_id < 0) {
            ret = (int) ret_id;
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

void
tsk_individual_table_print_state(const tsk_individual_table_t *self, FILE *out)
{
    tsk_size_t j, k;

    fprintf(out, "\n" TABLE_SEP);
    fprintf(out, "tsk_individual_tbl: %p:\n", (const void *) self);
    fprintf(out, "num_rows          = %lld\tmax= %lld\tincrement = %lld)\n",
        (long long) self->num_rows, (long long) self->max_rows,
        (long long) self->max_rows_increment);
    fprintf(out, "metadata_length = %lld\tmax= %lld\tincrement = %lld)\n",
        (long long) self->metadata_length, (long long) self->max_metadata_length,
        (long long) self->max_metadata_length_increment);
    fprintf(out, TABLE_SEP);
    /* We duplicate the dump_text code here because we want to output
     * the offset columns. */
    write_metadata_schema_header(
        out, self->metadata_schema, self->metadata_schema_length);
    fprintf(out, "id\tflags\tlocation_offset\tlocation\t");
    fprintf(out, "parents_offset\tparents\t");
    fprintf(out, "metadata_offset\tmetadata\n");
    for (j = 0; j < self->num_rows; j++) {
        fprintf(out, "%lld\t%lld\t", (long long) j, (long long) self->flags[j]);
        fprintf(out, "%lld\t", (long long) self->location_offset[j]);
        for (k = self->location_offset[j]; k < self->location_offset[j + 1]; k++) {
            fprintf(out, "%f", self->location[k]);
            if (k + 1 < self->location_offset[j + 1]) {
                fprintf(out, ",");
            }
        }
        fprintf(out, "\t");
        fprintf(out, "%lld\t", (long long) self->parents_offset[j]);
        for (k = self->parents_offset[j]; k < self->parents_offset[j + 1]; k++) {
            fprintf(out, "%lld", (long long) self->parents[k]);
            if (k + 1 < self->parents_offset[j + 1]) {
                fprintf(out, ",");
            }
        }
        fprintf(out, "\t");
        fprintf(out, "%lld\t", (long long) self->metadata_offset[j]);
        for (k = self->metadata_offset[j]; k < self->metadata_offset[j + 1]; k++) {
            fprintf(out, "%c", self->metadata[k]);
        }
        fprintf(out, "\n");
    }
}

static inline void
tsk_individual_table_get_row_unsafe(
    const tsk_individual_table_t *self, tsk_id_t index, tsk_individual_t *row)
{
    row->id = (tsk_id_t) index;
    row->flags = self->flags[index];
    row->location_length
        = self->location_offset[index + 1] - self->location_offset[index];
    row->location = self->location + self->location_offset[index];
    row->parents_length = self->parents_offset[index + 1] - self->parents_offset[index];
    row->parents = self->parents + self->parents_offset[index];
    row->metadata_length
        = self->metadata_offset[index + 1] - self->metadata_offset[index];
    row->metadata = self->metadata + self->metadata_offset[index];
    /* Also have referencing individuals here. Should this be a different struct?
     * See also site. */
    row->nodes_length = 0;
    row->nodes = NULL;
}

int
tsk_individual_table_get_row(
    const tsk_individual_table_t *self, tsk_id_t index, tsk_individual_t *row)
{
    int ret = 0;

    if (index < 0 || index >= (tsk_id_t) self->num_rows) {
        ret = tsk_trace_error(TSK_ERR_INDIVIDUAL_OUT_OF_BOUNDS);
        goto out;
    }
    tsk_individual_table_get_row_unsafe(self, index, row);
out:
    return ret;
}

int
tsk_individual_table_set_metadata_schema(tsk_individual_table_t *self,
    const char *metadata_schema, tsk_size_t metadata_schema_length)
{
    return replace_string(&self->metadata_schema, &self->metadata_schema_length,
        metadata_schema, metadata_schema_length);
}

int
tsk_individual_table_dump_text(const tsk_individual_table_t *self, FILE *out)
{
    int ret = TSK_ERR_IO;
    tsk_size_t j, k;
    tsk_size_t metadata_len;
    int err;

    err = write_metadata_schema_header(
        out, self->metadata_schema, self->metadata_schema_length);
    if (err < 0) {
        goto out;
    }
    err = fprintf(out, "id\tflags\tlocation\tparents\tmetadata\n");
    if (err < 0) {
        goto out;
    }
    for (j = 0; j < self->num_rows; j++) {
        metadata_len = self->metadata_offset[j + 1] - self->metadata_offset[j];
        err = fprintf(out, "%lld\t%lld\t", (long long) j, (long long) self->flags[j]);
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
        err = fprintf(out, "\t");
        if (err < 0) {
            goto out;
        }
        for (k = self->parents_offset[j]; k < self->parents_offset[j + 1]; k++) {
            err = fprintf(out, "%lld", (long long) self->parents[k]);
            if (err < 0) {
                goto out;
            }
            if (k + 1 < self->parents_offset[j + 1]) {
                err = fprintf(out, ",");
                if (err < 0) {
                    goto out;
                }
            }
        }
        err = fprintf(out, "\t%.*s\n", (int) metadata_len,
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
tsk_individual_table_equals(const tsk_individual_table_t *self,
    const tsk_individual_table_t *other, tsk_flags_t options)
{
    bool ret
        = self->num_rows == other->num_rows
          && tsk_memcmp(self->flags, other->flags, self->num_rows * sizeof(tsk_flags_t))
                 == 0
          && tsk_memcmp(self->location_offset, other->location_offset,
                 (self->num_rows + 1) * sizeof(tsk_size_t))
                 == 0
          && tsk_memcmp(
                 self->location, other->location, self->location_length * sizeof(double))
                 == 0
          && tsk_memcmp(self->parents_offset, other->parents_offset,
                 (self->num_rows + 1) * sizeof(tsk_size_t))
                 == 0
          && tsk_memcmp(
                 self->parents, other->parents, self->parents_length * sizeof(tsk_id_t))
                 == 0;

    if (!(options & TSK_CMP_IGNORE_METADATA)) {
        ret = ret && self->metadata_length == other->metadata_length
              && self->metadata_schema_length == other->metadata_schema_length
              && tsk_memcmp(self->metadata_offset, other->metadata_offset,
                     (self->num_rows + 1) * sizeof(tsk_size_t))
                     == 0
              && tsk_memcmp(self->metadata, other->metadata,
                     self->metadata_length * sizeof(char))
                     == 0
              && tsk_memcmp(self->metadata_schema, other->metadata_schema,
                     self->metadata_schema_length * sizeof(char))
                     == 0;
    }
    return ret;
}

int
tsk_individual_table_keep_rows(tsk_individual_table_t *self, const tsk_bool_t *keep,
    tsk_flags_t TSK_UNUSED(options), tsk_id_t *ret_id_map)
{
    int ret = 0;
    const tsk_size_t current_num_rows = self->num_rows;
    tsk_size_t j, k, remaining_rows;
    tsk_id_t pk;
    tsk_id_t *id_map = ret_id_map;
    tsk_id_t *restrict parents = self->parents;
    tsk_size_t *restrict parents_offset = self->parents_offset;

    if (ret_id_map == NULL) {
        id_map = tsk_malloc(current_num_rows * sizeof(*id_map));
        if (id_map == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
    }

    keep_mask_to_id_map(current_num_rows, keep, id_map);

    /* See notes in tsk_mutation_table_keep_rows for possibilities
     * on making this more flexible */
    for (j = 0; j < current_num_rows; j++) {
        if (keep[j]) {
            for (k = parents_offset[j]; k < parents_offset[j + 1]; k++) {
                pk = parents[k];
                if (pk != TSK_NULL) {
                    if (pk < 0 || pk >= (tsk_id_t) current_num_rows) {
                        ret = tsk_trace_error(TSK_ERR_INDIVIDUAL_OUT_OF_BOUNDS);
                        ;
                        goto out;
                    }
                    if (id_map[pk] == TSK_NULL) {
                        ret = tsk_trace_error(TSK_ERR_KEEP_ROWS_MAP_TO_DELETED);
                        goto out;
                    }
                }
            }
        }
    }

    remaining_rows = subset_flags_column(self->flags, current_num_rows, keep);
    self->parents_length = subset_remap_ragged_id_column(
        self->parents, self->parents_offset, current_num_rows, keep, id_map);
    self->location_length = subset_ragged_double_column(
        self->location, self->location_offset, current_num_rows, keep);
    if (self->metadata_length > 0) {
        /* Implementation note: we special case metadata here because
         * it'll make the common-case of no metadata a bit faster, and
         * to also potentially support more general use of the
         * TSK_TABLE_NO_METADATA option. This is done for all the tables
         * but only commented on here. */
        self->metadata_length = subset_ragged_char_column(
            self->metadata, self->metadata_offset, current_num_rows, keep);
    }
    self->num_rows = remaining_rows;
out:
    if (ret_id_map == NULL) {
        tsk_safe_free(id_map);
    }
    return ret;
}

static int
tsk_individual_table_dump(
    const tsk_individual_table_t *self, kastore_t *store, tsk_flags_t options)
{
    const write_table_col_t write_cols[] = {
        { "individuals/flags", (void *) self->flags, self->num_rows,
            TSK_FLAGS_STORAGE_TYPE },
        { "individuals/metadata_schema", (void *) self->metadata_schema,
            self->metadata_schema_length, KAS_UINT8 },
        { .name = NULL },
    };
    const write_table_ragged_col_t ragged_cols[] = {
        { "individuals/location", (void *) self->location, self->location_length,
            KAS_FLOAT64, self->location_offset, self->num_rows },
        { "individuals/parents", (void *) self->parents, self->parents_length,
            TSK_ID_STORAGE_TYPE, self->parents_offset, self->num_rows },
        { "individuals/metadata", (void *) self->metadata, self->metadata_length,
            KAS_UINT8, self->metadata_offset, self->num_rows },
        { .name = NULL },
    };

    return write_table(store, write_cols, ragged_cols, options);
}

static int
tsk_individual_table_load(tsk_individual_table_t *self, kastore_t *store)
{
    int ret = 0;
    tsk_flags_t *flags = NULL;
    double *location = NULL;
    tsk_size_t *location_offset = NULL;
    tsk_id_t *parents = NULL;
    tsk_size_t *parents_offset = NULL;
    char *metadata = NULL;
    tsk_size_t *metadata_offset = NULL;
    char *metadata_schema = NULL;
    tsk_size_t num_rows, location_length, parents_length, metadata_length,
        metadata_schema_length;

    read_table_col_t cols[] = {
        { "individuals/flags", (void **) &flags, TSK_FLAGS_STORAGE_TYPE, 0 },
        { .name = NULL },
    };
    read_table_ragged_col_t ragged_cols[] = {
        { "individuals/location", (void **) &location, &location_length, KAS_FLOAT64,
            &location_offset, 0 },
        { "individuals/parents", (void **) &parents, &parents_length,
            TSK_ID_STORAGE_TYPE, &parents_offset, TSK_COL_OPTIONAL },
        { "individuals/metadata", (void **) &metadata, &metadata_length, KAS_UINT8,
            &metadata_offset, 0 },
        { .name = NULL },
    };
    read_table_property_t properties[] = {
        { "individuals/metadata_schema", (void **) &metadata_schema,
            &metadata_schema_length, KAS_UINT8, TSK_COL_OPTIONAL },
        { .name = NULL },
    };

    ret = read_table(store, &num_rows, cols, ragged_cols, properties, 0);
    if (ret != 0) {
        goto out;
    }
    if (metadata_schema != NULL) {
        ret = tsk_individual_table_set_metadata_schema(
            self, metadata_schema, metadata_schema_length);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_individual_table_takeset_columns(self, num_rows, flags, location,
        location_offset, parents, parents_offset, metadata, metadata_offset);
    if (ret != 0) {
        goto out;
    }
    flags = NULL;
    location = NULL;
    location_offset = NULL;
    parents = NULL;
    parents_offset = NULL;
    metadata = NULL;
    metadata_offset = NULL;

out:
    free_read_table_mem(cols, ragged_cols, properties);
    return ret;
}

/*************************
 * node table
 *************************/

static void
tsk_node_table_free_columns(tsk_node_table_t *self)
{
    tsk_safe_free(self->flags);
    tsk_safe_free(self->time);
    tsk_safe_free(self->population);
    tsk_safe_free(self->individual);
    tsk_safe_free(self->metadata);
    tsk_safe_free(self->metadata_offset);
}

int
tsk_node_table_free(tsk_node_table_t *self)
{
    tsk_node_table_free_columns(self);
    tsk_safe_free(self->metadata_schema);
    return 0;
}

static int
tsk_node_table_expand_main_columns(tsk_node_table_t *self, tsk_size_t additional_rows)
{
    int ret = 0;
    tsk_size_t new_max_rows;

    ret = calculate_max_rows(self->num_rows, self->max_rows, self->max_rows_increment,
        additional_rows, &new_max_rows);
    if (ret != 0) {
        goto out;
    }

    if (new_max_rows > self->max_rows) {
        ret = expand_column((void **) &self->flags, new_max_rows, sizeof(tsk_flags_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->time, new_max_rows, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->population, new_max_rows, sizeof(tsk_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->individual, new_max_rows, sizeof(tsk_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column(
            (void **) &self->metadata_offset, new_max_rows + 1, sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        self->max_rows = new_max_rows;
    }
out:
    return ret;
}

static int
tsk_node_table_expand_metadata(tsk_node_table_t *self, tsk_size_t additional_length)
{
    return expand_ragged_column(self->metadata_length, additional_length,
        self->max_metadata_length_increment, &self->max_metadata_length,
        (void **) &self->metadata, sizeof(*self->metadata));
}

int
tsk_node_table_set_max_rows_increment(
    tsk_node_table_t *self, tsk_size_t max_rows_increment)
{
    self->max_rows_increment = max_rows_increment;
    return 0;
}

int
tsk_node_table_set_max_metadata_length_increment(
    tsk_node_table_t *self, tsk_size_t max_metadata_length_increment)
{
    self->max_metadata_length_increment = max_metadata_length_increment;
    return 0;
}

int
tsk_node_table_init(tsk_node_table_t *self, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;

    tsk_memset(self, 0, sizeof(tsk_node_table_t));
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
    self->max_rows_increment = 0;
    self->max_metadata_length_increment = 0;
    tsk_node_table_set_metadata_schema(self, NULL, 0);
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_node_table_copy(
    const tsk_node_table_t *self, tsk_node_table_t *dest, tsk_flags_t options)
{
    int ret = 0;

    if (!(options & TSK_NO_INIT)) {
        ret = tsk_node_table_init(dest, 0);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_node_table_set_columns(dest, self->num_rows, self->flags, self->time,
        self->population, self->individual, self->metadata, self->metadata_offset);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_node_table_set_metadata_schema(
        dest, self->metadata_schema, self->metadata_schema_length);
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_node_table_set_columns(tsk_node_table_t *self, tsk_size_t num_rows,
    const tsk_flags_t *flags, const double *time, const tsk_id_t *population,
    const tsk_id_t *individual, const char *metadata, const tsk_size_t *metadata_offset)
{
    int ret;

    ret = tsk_node_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_node_table_append_columns(
        self, num_rows, flags, time, population, individual, metadata, metadata_offset);
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_node_table_takeset_columns(tsk_node_table_t *self, tsk_size_t num_rows,
    tsk_flags_t *flags, double *time, tsk_id_t *population, tsk_id_t *individual,
    char *metadata, tsk_size_t *metadata_offset)
{
    int ret = 0;

    /* We need to check all the inputs before we start freeing or taking memory */
    if (flags == NULL || time == NULL) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    ret = check_ragged_column(num_rows, metadata, metadata_offset);
    if (ret != 0) {
        goto out;
    }

    tsk_node_table_free_columns(self);
    self->num_rows = num_rows;
    self->max_rows = num_rows;
    self->flags = flags;
    self->time = time;

    ret = takeset_optional_id_column(num_rows, population, &self->population);
    if (ret != 0) {
        goto out;
    }
    ret = takeset_optional_id_column(num_rows, individual, &self->individual);
    if (ret != 0) {
        goto out;
    }

    ret = takeset_ragged_column(num_rows, metadata, metadata_offset,
        (void *) &self->metadata, &self->metadata_offset, &self->metadata_length);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

int
tsk_node_table_append_columns(tsk_node_table_t *self, tsk_size_t num_rows,
    const tsk_flags_t *flags, const double *time, const tsk_id_t *population,
    const tsk_id_t *individual, const char *metadata, const tsk_size_t *metadata_offset)
{
    int ret;
    tsk_size_t j, metadata_length;

    if (flags == NULL || time == NULL) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    if ((metadata == NULL) != (metadata_offset == NULL)) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    ret = tsk_node_table_expand_main_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    tsk_memcpy(self->time + self->num_rows, time, num_rows * sizeof(double));
    tsk_memcpy(self->flags + self->num_rows, flags, num_rows * sizeof(tsk_flags_t));
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
            self->metadata_offset[self->num_rows + j]
                = (tsk_size_t) self->metadata_length + metadata_offset[j];
        }
        metadata_length = metadata_offset[num_rows];
        ret = tsk_node_table_expand_metadata(self, metadata_length);
        if (ret != 0) {
            goto out;
        }
        tsk_memcpy(self->metadata + self->metadata_length, metadata,
            metadata_length * sizeof(char));
        self->metadata_length += metadata_length;
    }
    if (population == NULL) {
        /* Set population to NULL_POPULATION (-1) if not specified */
        tsk_memset(self->population + self->num_rows, 0xff, num_rows * sizeof(tsk_id_t));
    } else {
        tsk_memcpy(
            self->population + self->num_rows, population, num_rows * sizeof(tsk_id_t));
    }
    if (individual == NULL) {
        /* Set individual to NULL_INDIVIDUAL (-1) if not specified */
        tsk_memset(self->individual + self->num_rows, 0xff, num_rows * sizeof(tsk_id_t));
    } else {
        tsk_memcpy(
            self->individual + self->num_rows, individual, num_rows * sizeof(tsk_id_t));
    }
    self->num_rows += (tsk_size_t) num_rows;
    self->metadata_offset[self->num_rows] = self->metadata_length;
out:
    return ret;
}

static tsk_id_t
tsk_node_table_add_row_internal(tsk_node_table_t *self, tsk_flags_t flags, double time,
    tsk_id_t population, tsk_id_t individual, const char *metadata,
    tsk_size_t metadata_length)
{
    tsk_bug_assert(self->num_rows < self->max_rows);
    tsk_bug_assert(self->metadata_length + metadata_length <= self->max_metadata_length);
    tsk_memmove(self->metadata + self->metadata_length, metadata, metadata_length);
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
    tsk_id_t population, tsk_id_t individual, const char *metadata,
    tsk_size_t metadata_length)
{
    tsk_id_t ret = 0;

    ret = tsk_node_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_node_table_expand_metadata(self, metadata_length);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_node_table_add_row_internal(
        self, flags, time, population, individual, metadata, metadata_length);
out:
    return ret;
}

static int
tsk_node_table_update_row_rewrite(tsk_node_table_t *self, tsk_id_t index,
    tsk_flags_t flags, double time, tsk_id_t population, tsk_id_t individual,
    const char *metadata, tsk_size_t metadata_length)
{
    int ret = 0;
    tsk_id_t j, ret_id;
    tsk_node_table_t copy;
    tsk_size_t num_rows;
    tsk_id_t *rows = NULL;

    ret = tsk_node_table_copy(self, &copy, 0);
    if (ret != 0) {
        goto out;
    }
    rows = tsk_malloc(self->num_rows * sizeof(*rows));
    if (rows == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    ret = tsk_node_table_truncate(self, (tsk_size_t) index);
    tsk_bug_assert(ret == 0);
    ret_id = tsk_node_table_add_row(
        self, flags, time, population, individual, metadata, metadata_length);
    if (ret_id < 0) {
        ret = (int) ret_id;
        goto out;
    }
    num_rows = 0;
    for (j = index + 1; j < (tsk_id_t) copy.num_rows; j++) {
        rows[num_rows] = j;
        num_rows++;
    }
    ret = tsk_node_table_extend(self, &copy, num_rows, rows, 0);
    if (ret != 0) {
        goto out;
    }
out:
    tsk_node_table_free(&copy);
    tsk_safe_free(rows);
    return ret;
}

int
tsk_node_table_update_row(tsk_node_table_t *self, tsk_id_t index, tsk_flags_t flags,
    double time, tsk_id_t population, tsk_id_t individual, const char *metadata,
    tsk_size_t metadata_length)
{
    int ret = 0;
    tsk_node_t current_row;

    ret = tsk_node_table_get_row(self, index, &current_row);
    if (ret != 0) {
        goto out;
    }
    if (current_row.metadata_length == metadata_length) {
        self->flags[index] = flags;
        self->time[index] = time;
        self->population[index] = population;
        self->individual[index] = individual;
        /* Note: important to use tsk_memmove here as we may be provided pointers
         * to the column memory as input via get_row */
        tsk_memmove(&self->metadata[self->metadata_offset[index]], metadata,
            metadata_length * sizeof(*metadata));
    } else {
        ret = tsk_node_table_update_row_rewrite(
            self, index, flags, time, population, individual, metadata, metadata_length);
        if (ret != 0) {
            goto out;
        }
    }
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
        ret = tsk_trace_error(TSK_ERR_BAD_TABLE_POSITION);
        goto out;
    }
    self->num_rows = num_rows;
    self->metadata_length = self->metadata_offset[num_rows];
out:
    return ret;
}

int
tsk_node_table_extend(tsk_node_table_t *self, const tsk_node_table_t *other,
    tsk_size_t num_rows, const tsk_id_t *row_indexes, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_id_t ret_id;
    tsk_size_t j;
    tsk_node_t node;

    if (self == other) {
        ret = tsk_trace_error(TSK_ERR_CANNOT_EXTEND_FROM_SELF);
        goto out;
    }

    /* We know how much to expand the non-ragged columns, so do it ahead of time */
    ret = tsk_node_table_expand_main_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < num_rows; j++) {
        ret = tsk_node_table_get_row(
            other, row_indexes == NULL ? (tsk_id_t) j : row_indexes[j], &node);
        if (ret != 0) {
            goto out;
        }
        ret_id = tsk_node_table_add_row(self, node.flags, node.time, node.population,
            node.individual, node.metadata, node.metadata_length);
        if (ret_id < 0) {
            ret = (int) ret_id;
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

void
tsk_node_table_print_state(const tsk_node_table_t *self, FILE *out)
{
    tsk_size_t j, k;

    fprintf(out, "\n" TABLE_SEP);
    fprintf(out, "tsk_node_tbl: %p:\n", (const void *) self);
    fprintf(out, "num_rows          = %lld\tmax= %lld\tincrement = %lld)\n",
        (long long) self->num_rows, (long long) self->max_rows,
        (long long) self->max_rows_increment);
    fprintf(out, "metadata_length = %lld\tmax= %lld\tincrement = %lld)\n",
        (long long) self->metadata_length, (long long) self->max_metadata_length,
        (long long) self->max_metadata_length_increment);
    fprintf(out, TABLE_SEP);
    /* We duplicate the dump_text code here for simplicity because we want to output
     * the flags column directly. */
    write_metadata_schema_header(
        out, self->metadata_schema, self->metadata_schema_length);
    fprintf(out, "id\tflags\ttime\tpopulation\tindividual\tmetadata_offset\tmetadata\n");
    for (j = 0; j < self->num_rows; j++) {
        fprintf(out, "%lld\t%lld\t%f\t%lld\t%lld\t%lld\t", (long long) j,
            (long long) self->flags[j], self->time[j], (long long) self->population[j],
            (long long) self->individual[j], (long long) self->metadata_offset[j]);
        for (k = self->metadata_offset[j]; k < self->metadata_offset[j + 1]; k++) {
            fprintf(out, "%c", self->metadata[k]);
        }
        fprintf(out, "\n");
    }
    tsk_bug_assert(self->metadata_offset[0] == 0);
    tsk_bug_assert(self->metadata_offset[self->num_rows] == self->metadata_length);
}

int
tsk_node_table_set_metadata_schema(tsk_node_table_t *self, const char *metadata_schema,
    tsk_size_t metadata_schema_length)
{
    return replace_string(&self->metadata_schema, &self->metadata_schema_length,
        metadata_schema, metadata_schema_length);
}

int
tsk_node_table_dump_text(const tsk_node_table_t *self, FILE *out)
{
    int ret = TSK_ERR_IO;
    tsk_size_t j;
    tsk_size_t metadata_len;
    int err;

    err = write_metadata_schema_header(
        out, self->metadata_schema, self->metadata_schema_length);
    if (err < 0) {
        goto out;
    }
    err = fprintf(out, "id\tis_sample\ttime\tpopulation\tindividual\tmetadata\n");
    if (err < 0) {
        goto out;
    }
    for (j = 0; j < self->num_rows; j++) {
        metadata_len = self->metadata_offset[j + 1] - self->metadata_offset[j];
        err = fprintf(out, "%lld\t%lld\t%f\t%lld\t%lld\t%.*s\n", (long long) j,
            (long long) (self->flags[j] & TSK_NODE_IS_SAMPLE), self->time[j],
            (long long) self->population[j], (long long) self->individual[j],
            (int) metadata_len, self->metadata + self->metadata_offset[j]);
        if (err < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

bool
tsk_node_table_equals(
    const tsk_node_table_t *self, const tsk_node_table_t *other, tsk_flags_t options)
{
    bool ret
        = self->num_rows == other->num_rows
          && tsk_memcmp(self->time, other->time, self->num_rows * sizeof(double)) == 0
          && tsk_memcmp(self->flags, other->flags, self->num_rows * sizeof(tsk_flags_t))
                 == 0
          && tsk_memcmp(
                 self->population, other->population, self->num_rows * sizeof(tsk_id_t))
                 == 0
          && tsk_memcmp(
                 self->individual, other->individual, self->num_rows * sizeof(tsk_id_t))
                 == 0;
    if (!(options & TSK_CMP_IGNORE_METADATA)) {
        ret = ret && self->metadata_length == other->metadata_length
              && self->metadata_schema_length == other->metadata_schema_length
              && tsk_memcmp(self->metadata_offset, other->metadata_offset,
                     (self->num_rows + 1) * sizeof(tsk_size_t))
                     == 0
              && tsk_memcmp(self->metadata, other->metadata,
                     self->metadata_length * sizeof(char))
                     == 0
              && tsk_memcmp(self->metadata_schema, other->metadata_schema,
                     self->metadata_schema_length * sizeof(char))
                     == 0;
    }
    return ret;
}

static inline void
tsk_node_table_get_row_unsafe(
    const tsk_node_table_t *self, tsk_id_t index, tsk_node_t *row)
{
    row->id = (tsk_id_t) index;
    row->flags = self->flags[index];
    row->time = self->time[index];
    row->population = self->population[index];
    row->individual = self->individual[index];
    row->metadata_length
        = self->metadata_offset[index + 1] - self->metadata_offset[index];
    row->metadata = self->metadata + self->metadata_offset[index];
}

int
tsk_node_table_get_row(const tsk_node_table_t *self, tsk_id_t index, tsk_node_t *row)
{
    int ret = 0;

    if (index < 0 || index >= (tsk_id_t) self->num_rows) {
        ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
        goto out;
    }
    tsk_node_table_get_row_unsafe(self, index, row);
out:
    return ret;
}

int
tsk_node_table_keep_rows(tsk_node_table_t *self, const tsk_bool_t *keep,
    tsk_flags_t TSK_UNUSED(options), tsk_id_t *id_map)
{
    int ret = 0;
    tsk_size_t remaining_rows;

    if (id_map != NULL) {
        keep_mask_to_id_map(self->num_rows, keep, id_map);
    }

    remaining_rows = subset_flags_column(self->flags, self->num_rows, keep);
    subset_double_column(self->time, self->num_rows, keep);
    subset_id_column(self->population, self->num_rows, keep);
    subset_id_column(self->individual, self->num_rows, keep);
    if (self->metadata_length > 0) {
        self->metadata_length = subset_ragged_char_column(
            self->metadata, self->metadata_offset, self->num_rows, keep);
    }
    self->num_rows = remaining_rows;
    return ret;
}

static int
tsk_node_table_dump(const tsk_node_table_t *self, kastore_t *store, tsk_flags_t options)
{
    const write_table_col_t cols[] = {
        { "nodes/time", (void *) self->time, self->num_rows, KAS_FLOAT64 },
        { "nodes/flags", (void *) self->flags, self->num_rows, TSK_FLAGS_STORAGE_TYPE },
        { "nodes/population", (void *) self->population, self->num_rows,
            TSK_ID_STORAGE_TYPE },
        { "nodes/individual", (void *) self->individual, self->num_rows,
            TSK_ID_STORAGE_TYPE },
        { "nodes/metadata_schema", (void *) self->metadata_schema,
            self->metadata_schema_length, KAS_UINT8 },
        { .name = NULL },
    };
    const write_table_ragged_col_t ragged_cols[] = {
        { "nodes/metadata", (void *) self->metadata, self->metadata_length, KAS_UINT8,
            self->metadata_offset, self->num_rows },
        { .name = NULL },
    };

    return write_table(store, cols, ragged_cols, options);
}

static int
tsk_node_table_load(tsk_node_table_t *self, kastore_t *store)
{
    int ret = 0;
    char *metadata_schema = NULL;
    double *time = NULL;
    tsk_flags_t *flags = NULL;
    tsk_id_t *population = NULL;
    tsk_id_t *individual = NULL;
    char *metadata = NULL;
    tsk_size_t *metadata_offset = NULL;
    tsk_size_t num_rows, metadata_length, metadata_schema_length;
    read_table_col_t cols[] = {
        { "nodes/time", (void **) &time, KAS_FLOAT64, 0 },
        { "nodes/flags", (void **) &flags, TSK_FLAGS_STORAGE_TYPE, 0 },
        { "nodes/population", (void **) &population, TSK_ID_STORAGE_TYPE, 0 },
        { "nodes/individual", (void **) &individual, TSK_ID_STORAGE_TYPE, 0 },
        { .name = NULL },
    };
    read_table_ragged_col_t ragged_cols[] = {
        { "nodes/metadata", (void **) &metadata, &metadata_length, KAS_UINT8,
            &metadata_offset, 0 },
        { .name = NULL },
    };
    read_table_property_t properties[] = {
        { "nodes/metadata_schema", (void **) &metadata_schema, &metadata_schema_length,
            KAS_UINT8, TSK_COL_OPTIONAL },
        { .name = NULL },
    };

    ret = read_table(store, &num_rows, cols, ragged_cols, properties, 0);
    if (ret != 0) {
        goto out;
    }
    if (metadata_schema != NULL) {
        ret = tsk_node_table_set_metadata_schema(
            self, metadata_schema, metadata_schema_length);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_node_table_takeset_columns(
        self, num_rows, flags, time, population, individual, metadata, metadata_offset);
    if (ret != 0) {
        goto out;
    }
    flags = NULL;
    time = NULL;
    population = NULL;
    individual = NULL;
    metadata = NULL;
    metadata_offset = NULL;
out:
    free_read_table_mem(cols, ragged_cols, properties);
    return ret;
}

/*************************
 * edge table
 *************************/

static void
tsk_edge_table_free_columns(tsk_edge_table_t *self)
{
    tsk_safe_free(self->left);
    tsk_safe_free(self->right);
    tsk_safe_free(self->parent);
    tsk_safe_free(self->child);
    tsk_safe_free(self->metadata);
    tsk_safe_free(self->metadata_offset);
}

int
tsk_edge_table_free(tsk_edge_table_t *self)
{
    tsk_edge_table_free_columns(self);
    tsk_safe_free(self->metadata_schema);
    return 0;
}

static int
tsk_edge_table_has_metadata(const tsk_edge_table_t *self)
{
    return !(self->options & TSK_TABLE_NO_METADATA);
}

static int
tsk_edge_table_expand_main_columns(tsk_edge_table_t *self, tsk_size_t additional_rows)
{
    int ret = 0;
    tsk_size_t new_max_rows;

    ret = calculate_max_rows(self->num_rows, self->max_rows, self->max_rows_increment,
        additional_rows, &new_max_rows);
    if (ret != 0) {
        goto out;
    }
    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column((void **) &self->left, new_max_rows, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->right, new_max_rows, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->parent, new_max_rows, sizeof(tsk_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->child, new_max_rows, sizeof(tsk_id_t));
        if (ret != 0) {
            goto out;
        }
        if (tsk_edge_table_has_metadata(self)) {
            ret = expand_column(
                (void **) &self->metadata_offset, new_max_rows + 1, sizeof(tsk_size_t));
            if (ret != 0) {
                goto out;
            }
        }
        self->max_rows = new_max_rows;
    }
out:
    return ret;
}

static int
tsk_edge_table_expand_metadata(tsk_edge_table_t *self, tsk_size_t additional_length)
{
    return expand_ragged_column(self->metadata_length, additional_length,
        self->max_metadata_length_increment, &self->max_metadata_length,
        (void **) &self->metadata, sizeof(*self->metadata));
}

int
tsk_edge_table_set_max_rows_increment(
    tsk_edge_table_t *self, tsk_size_t max_rows_increment)
{
    self->max_rows_increment = max_rows_increment;
    return 0;
}

int
tsk_edge_table_set_max_metadata_length_increment(
    tsk_edge_table_t *self, tsk_size_t max_metadata_length_increment)
{
    self->max_metadata_length_increment = max_metadata_length_increment;
    return 0;
}

int
tsk_edge_table_init(tsk_edge_table_t *self, tsk_flags_t options)
{
    int ret = 0;

    tsk_memset(self, 0, sizeof(*self));
    self->options = options;

    /* Allocate space for one row initially, ensuring we always have valid
     * pointers even if the table is empty */
    self->max_rows_increment = 1;
    self->max_metadata_length_increment = 1;
    ret = tsk_edge_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    if (tsk_edge_table_has_metadata(self)) {
        ret = tsk_edge_table_expand_metadata(self, 1);
        if (ret != 0) {
            goto out;
        }
        self->metadata_offset[0] = 0;
    }
    self->max_rows_increment = 0;
    self->max_metadata_length_increment = 0;
    tsk_edge_table_set_metadata_schema(self, NULL, 0);
out:
    return ret;
}

tsk_id_t
tsk_edge_table_add_row(tsk_edge_table_t *self, double left, double right,
    tsk_id_t parent, tsk_id_t child, const char *metadata, tsk_size_t metadata_length)
{
    tsk_id_t ret = 0;

    if (metadata_length > 0 && !tsk_edge_table_has_metadata(self)) {
        ret = tsk_trace_error(TSK_ERR_METADATA_DISABLED);
        goto out;
    }

    ret = tsk_edge_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }

    tsk_bug_assert(self->num_rows < self->max_rows);
    self->left[self->num_rows] = left;
    self->right[self->num_rows] = right;
    self->parent[self->num_rows] = parent;
    self->child[self->num_rows] = child;

    if (tsk_edge_table_has_metadata(self)) {
        ret = tsk_edge_table_expand_metadata(self, metadata_length);
        if (ret != 0) {
            goto out;
        }
        tsk_bug_assert(
            self->metadata_length + metadata_length <= self->max_metadata_length);
        tsk_memmove(self->metadata + self->metadata_length, metadata, metadata_length);
        self->metadata_offset[self->num_rows + 1]
            = self->metadata_length + metadata_length;
        self->metadata_length += metadata_length;
    }
    ret = (tsk_id_t) self->num_rows;
    self->num_rows++;
out:
    return ret;
}

static int
tsk_edge_table_update_row_rewrite(tsk_edge_table_t *self, tsk_id_t index, double left,
    double right, tsk_id_t parent, tsk_id_t child, const char *metadata,
    tsk_size_t metadata_length)
{
    int ret = 0;
    tsk_id_t j, ret_id;
    tsk_edge_table_t copy;
    tsk_size_t num_rows;
    tsk_id_t *rows = NULL;

    ret = tsk_edge_table_copy(self, &copy, 0);
    if (ret != 0) {
        goto out;
    }
    rows = tsk_malloc(self->num_rows * sizeof(*rows));
    if (rows == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    ret = tsk_edge_table_truncate(self, (tsk_size_t) index);
    tsk_bug_assert(ret == 0);
    ret_id = tsk_edge_table_add_row(
        self, left, right, parent, child, metadata, metadata_length);
    if (ret_id < 0) {
        ret = (int) ret_id;
        goto out;
    }
    num_rows = 0;
    for (j = index + 1; j < (tsk_id_t) copy.num_rows; j++) {
        rows[num_rows] = j;
        num_rows++;
    }
    ret = tsk_edge_table_extend(self, &copy, num_rows, rows, 0);
    if (ret != 0) {
        goto out;
    }
out:
    tsk_edge_table_free(&copy);
    tsk_safe_free(rows);
    return ret;
}

int
tsk_edge_table_update_row(tsk_edge_table_t *self, tsk_id_t index, double left,
    double right, tsk_id_t parent, tsk_id_t child, const char *metadata,
    tsk_size_t metadata_length)
{
    int ret = 0;
    tsk_edge_t current_row;

    ret = tsk_edge_table_get_row(self, index, &current_row);
    if (ret != 0) {
        goto out;
    }
    if (current_row.metadata_length == metadata_length) {
        self->left[index] = left;
        self->right[index] = right;
        self->parent[index] = parent;
        self->child[index] = child;
        if (tsk_edge_table_has_metadata(self)) {
            /* Note: important to use tsk_memmove here as we may be provided pointers
             * to the column memory as input via get_row */
            tsk_memmove(&self->metadata[self->metadata_offset[index]], metadata,
                metadata_length * sizeof(*metadata));
        }
    } else {
        ret = tsk_edge_table_update_row_rewrite(
            self, index, left, right, parent, child, metadata, metadata_length);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_edge_table_copy(
    const tsk_edge_table_t *self, tsk_edge_table_t *dest, tsk_flags_t options)
{
    int ret = 0;
    char *metadata = NULL;
    tsk_size_t *metadata_offset = NULL;

    if (!(options & TSK_NO_INIT)) {
        ret = tsk_edge_table_init(dest, options);
        if (ret != 0) {
            goto out;
        }
    }

    /* We can't use TSK_TABLE_NO_METADATA in dest if metadata_length is non-zero.
     * This also captures the case where TSK_TABLE_NO_METADATA is set on this table.
     */
    if (self->metadata_length > 0 && !tsk_edge_table_has_metadata(dest)) {
        ret = tsk_trace_error(TSK_ERR_METADATA_DISABLED);
        goto out;
    }
    if (tsk_edge_table_has_metadata(dest)) {
        metadata = self->metadata;
        metadata_offset = self->metadata_offset;
    }
    ret = tsk_edge_table_set_columns(dest, self->num_rows, self->left, self->right,
        self->parent, self->child, metadata, metadata_offset);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_edge_table_set_metadata_schema(
        dest, self->metadata_schema, self->metadata_schema_length);
out:
    return ret;
}

int
tsk_edge_table_set_columns(tsk_edge_table_t *self, tsk_size_t num_rows,
    const double *left, const double *right, const tsk_id_t *parent,
    const tsk_id_t *child, const char *metadata, const tsk_size_t *metadata_offset)
{
    int ret = 0;

    ret = tsk_edge_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_edge_table_append_columns(
        self, num_rows, left, right, parent, child, metadata, metadata_offset);
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_edge_table_takeset_columns(tsk_edge_table_t *self, tsk_size_t num_rows, double *left,
    double *right, tsk_id_t *parent, tsk_id_t *child, char *metadata,
    tsk_size_t *metadata_offset)
{
    int ret = 0;

    /* We need to check all the inputs before we start freeing or taking memory */
    if (left == NULL || right == NULL || parent == NULL || child == NULL) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    if (metadata != NULL && !tsk_edge_table_has_metadata(self)) {
        ret = tsk_trace_error(TSK_ERR_METADATA_DISABLED);
        goto out;
    }
    ret = check_ragged_column(num_rows, metadata, metadata_offset);
    if (ret != 0) {
        goto out;
    }

    tsk_edge_table_free_columns(self);
    self->num_rows = num_rows;
    self->max_rows = num_rows;
    self->left = left;
    self->right = right;
    self->parent = parent;
    self->child = child;

    ret = takeset_ragged_column(num_rows, metadata, metadata_offset,
        (void *) &self->metadata, &self->metadata_offset, &self->metadata_length);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

int
tsk_edge_table_append_columns(tsk_edge_table_t *self, tsk_size_t num_rows,
    const double *left, const double *right, const tsk_id_t *parent,
    const tsk_id_t *child, const char *metadata, const tsk_size_t *metadata_offset)
{
    int ret;
    tsk_size_t j, metadata_length;

    if (left == NULL || right == NULL || parent == NULL || child == NULL) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    if ((metadata == NULL) != (metadata_offset == NULL)) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    if (metadata != NULL && !tsk_edge_table_has_metadata(self)) {
        ret = tsk_trace_error(TSK_ERR_METADATA_DISABLED);
        goto out;
    }

    ret = tsk_edge_table_expand_main_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    tsk_memcpy(self->left + self->num_rows, left, num_rows * sizeof(double));
    tsk_memcpy(self->right + self->num_rows, right, num_rows * sizeof(double));
    tsk_memcpy(self->parent + self->num_rows, parent, num_rows * sizeof(tsk_id_t));
    tsk_memcpy(self->child + self->num_rows, child, num_rows * sizeof(tsk_id_t));
    if (tsk_edge_table_has_metadata(self)) {
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
                self->metadata_offset[self->num_rows + j]
                    = (tsk_size_t) self->metadata_length + metadata_offset[j];
            }
            metadata_length = metadata_offset[num_rows];
            ret = tsk_edge_table_expand_metadata(self, metadata_length);
            if (ret != 0) {
                goto out;
            }
            tsk_memcpy(self->metadata + self->metadata_length, metadata,
                metadata_length * sizeof(char));
            self->metadata_length += metadata_length;
        }
        self->num_rows += num_rows;
        self->metadata_offset[self->num_rows] = self->metadata_length;
    } else {
        self->num_rows += num_rows;
    }
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
        ret = tsk_trace_error(TSK_ERR_BAD_TABLE_POSITION);
        goto out;
    }
    self->num_rows = num_rows;
    if (tsk_edge_table_has_metadata(self)) {
        self->metadata_length = self->metadata_offset[num_rows];
    }
out:
    return ret;
}

int
tsk_edge_table_extend(tsk_edge_table_t *self, const tsk_edge_table_t *other,
    tsk_size_t num_rows, const tsk_id_t *row_indexes, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_id_t ret_id;
    tsk_size_t j;
    tsk_edge_t edge;

    if (self == other) {
        ret = tsk_trace_error(TSK_ERR_CANNOT_EXTEND_FROM_SELF);
        goto out;
    }

    /* We know how much to expand the non-ragged columns, so do it ahead of time */
    ret = tsk_edge_table_expand_main_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < num_rows; j++) {
        ret = tsk_edge_table_get_row(
            other, row_indexes == NULL ? (tsk_id_t) j : row_indexes[j], &edge);
        if (ret != 0) {
            goto out;
        }
        ret_id = tsk_edge_table_add_row(self, edge.left, edge.right, edge.parent,
            edge.child, edge.metadata, edge.metadata_length);
        if (ret_id < 0) {
            ret = (int) ret_id;
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

static inline void
tsk_edge_table_get_row_unsafe(
    const tsk_edge_table_t *self, tsk_id_t index, tsk_edge_t *row)
{
    row->id = (tsk_id_t) index;
    row->left = self->left[index];
    row->right = self->right[index];
    row->parent = self->parent[index];
    row->child = self->child[index];
    if (tsk_edge_table_has_metadata(self)) {
        row->metadata_length
            = self->metadata_offset[index + 1] - self->metadata_offset[index];
        row->metadata = self->metadata + self->metadata_offset[index];
    } else {
        row->metadata_length = 0;
        row->metadata = NULL;
    }
}

int
tsk_edge_table_get_row(const tsk_edge_table_t *self, tsk_id_t index, tsk_edge_t *row)
{
    int ret = 0;

    if (index < 0 || index >= (tsk_id_t) self->num_rows) {
        ret = tsk_trace_error(TSK_ERR_EDGE_OUT_OF_BOUNDS);
        goto out;
    }
    tsk_edge_table_get_row_unsafe(self, index, row);
out:
    return ret;
}

void
tsk_edge_table_print_state(const tsk_edge_table_t *self, FILE *out)
{
    int ret;

    fprintf(out, "\n" TABLE_SEP);
    fprintf(out, "edge_table: %p:\n", (const void *) self);
    fprintf(out, "options         = 0x%X\n", self->options);
    fprintf(out, "num_rows        = %lld\tmax= %lld\tincrement = %lld)\n",
        (long long) self->num_rows, (long long) self->max_rows,
        (long long) self->max_rows_increment);
    fprintf(out, "metadata_length = %lld\tmax= %lld\tincrement = %lld)\n",
        (long long) self->metadata_length, (long long) self->max_metadata_length,
        (long long) self->max_metadata_length_increment);
    fprintf(out, TABLE_SEP);
    ret = tsk_edge_table_dump_text(self, out);
    tsk_bug_assert(ret == 0);
}

int
tsk_edge_table_set_metadata_schema(tsk_edge_table_t *self, const char *metadata_schema,
    tsk_size_t metadata_schema_length)
{
    return replace_string(&self->metadata_schema, &self->metadata_schema_length,
        metadata_schema, metadata_schema_length);
}

int
tsk_edge_table_dump_text(const tsk_edge_table_t *self, FILE *out)
{
    tsk_id_t j;
    int ret = TSK_ERR_IO;
    tsk_edge_t row;
    int err;

    err = write_metadata_schema_header(
        out, self->metadata_schema, self->metadata_schema_length);
    if (err < 0) {
        goto out;
    }
    err = fprintf(out, "id\tleft\tright\tparent\tchild\tmetadata\n");
    if (err < 0) {
        goto out;
    }
    for (j = 0; j < (tsk_id_t) self->num_rows; j++) {
        tsk_edge_table_get_row_unsafe(self, j, &row);
        err = fprintf(out, "%lld\t%.3f\t%.3f\t%lld\t%lld\t%.*s\n", (long long) j,
            row.left, row.right, (long long) row.parent, (long long) row.child,
            (int) row.metadata_length, row.metadata);
        if (err < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

bool
tsk_edge_table_equals(
    const tsk_edge_table_t *self, const tsk_edge_table_t *other, tsk_flags_t options)
{
    bool metadata_equal;
    bool ret
        = self->num_rows == other->num_rows
          && tsk_memcmp(self->left, other->left, self->num_rows * sizeof(double)) == 0
          && tsk_memcmp(self->right, other->right, self->num_rows * sizeof(double)) == 0
          && tsk_memcmp(self->parent, other->parent, self->num_rows * sizeof(tsk_id_t))
                 == 0
          && tsk_memcmp(self->child, other->child, self->num_rows * sizeof(tsk_id_t))
                 == 0;

    if (!(options & TSK_CMP_IGNORE_METADATA)) {
        ret = ret && self->metadata_schema_length == other->metadata_schema_length
              && tsk_memcmp(self->metadata_schema, other->metadata_schema,
                     self->metadata_schema_length * sizeof(char))
                     == 0;
        metadata_equal = false;
        if (self->metadata_length == other->metadata_length) {
            if (tsk_edge_table_has_metadata(self)
                && tsk_edge_table_has_metadata(other)) {
                metadata_equal
                    = tsk_memcmp(self->metadata_offset, other->metadata_offset,
                          (self->num_rows + 1) * sizeof(tsk_size_t))
                          == 0
                      && tsk_memcmp(self->metadata, other->metadata,
                             self->metadata_length * sizeof(char))
                             == 0;
            } else {
                /* The only way that the metadata lengths can be equal (which
                 * we've already tested) and either one or the other of the tables
                 * hasn't got metadata is if they are both zero length. */
                tsk_bug_assert(self->metadata_length == 0);
                metadata_equal = true;
            }
        }
        ret = ret && metadata_equal;
    }
    return ret;
}

int
tsk_edge_table_keep_rows(tsk_edge_table_t *self, const tsk_bool_t *keep,
    tsk_flags_t TSK_UNUSED(options), tsk_id_t *id_map)
{
    int ret = 0;
    tsk_size_t remaining_rows;

    if (id_map != NULL) {
        keep_mask_to_id_map(self->num_rows, keep, id_map);
    }
    remaining_rows = subset_double_column(self->left, self->num_rows, keep);
    subset_double_column(self->right, self->num_rows, keep);
    subset_id_column(self->parent, self->num_rows, keep);
    subset_id_column(self->child, self->num_rows, keep);
    if (self->metadata_length > 0) {
        tsk_bug_assert(!(self->options & TSK_TABLE_NO_METADATA));
        self->metadata_length = subset_ragged_char_column(
            self->metadata, self->metadata_offset, self->num_rows, keep);
    }
    self->num_rows = remaining_rows;
    return ret;
}

static int
tsk_edge_table_dump(const tsk_edge_table_t *self, kastore_t *store, tsk_flags_t options)
{
    int ret = 0;
    const write_table_col_t write_cols[] = {
        { "edges/left", (void *) self->left, self->num_rows, KAS_FLOAT64 },
        { "edges/right", (void *) self->right, self->num_rows, KAS_FLOAT64 },
        { "edges/parent", (void *) self->parent, self->num_rows, TSK_ID_STORAGE_TYPE },
        { "edges/child", (void *) self->child, self->num_rows, TSK_ID_STORAGE_TYPE },
        { "edges/metadata_schema", (void *) self->metadata_schema,
            self->metadata_schema_length, KAS_UINT8 },
        { .name = NULL },
    };
    const write_table_ragged_col_t ragged_cols[] = {
        { "edges/metadata", (void *) self->metadata, self->metadata_length, KAS_UINT8,
            self->metadata_offset, self->num_rows },
        { .name = NULL },
    };

    /* TODO when the general code has been updated to only write out the
     * column when the lenght of ragged columns is > 0 we can get rid of
     * this special case here and use write_table. */
    ret = write_table_cols(store, write_cols, options);
    if (ret != 0) {
        goto out;
    }
    if (tsk_edge_table_has_metadata(self)) {
        ret = write_table_ragged_cols(store, ragged_cols, options);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

static int
tsk_edge_table_load(tsk_edge_table_t *self, kastore_t *store)
{
    int ret = 0;
    char *metadata_schema = NULL;
    double *left = NULL;
    double *right = NULL;
    tsk_id_t *parent = NULL;
    tsk_id_t *child = NULL;
    char *metadata = NULL;
    tsk_size_t *metadata_offset = NULL;
    tsk_size_t num_rows, metadata_length, metadata_schema_length;

    read_table_col_t cols[] = {
        { "edges/left", (void **) &left, KAS_FLOAT64, 0 },
        { "edges/right", (void **) &right, KAS_FLOAT64, 0 },
        { "edges/parent", (void **) &parent, TSK_ID_STORAGE_TYPE, 0 },
        { "edges/child", (void **) &child, TSK_ID_STORAGE_TYPE, 0 },
        { .name = NULL },
    };
    read_table_ragged_col_t ragged_cols[] = {
        { "edges/metadata", (void **) &metadata, &metadata_length, KAS_UINT8,
            &metadata_offset, TSK_COL_OPTIONAL },
        { .name = NULL },
    };
    read_table_property_t properties[] = {
        { "edges/metadata_schema", (void **) &metadata_schema, &metadata_schema_length,
            KAS_UINT8, TSK_COL_OPTIONAL },
        { .name = NULL },
    };

    ret = read_table(store, &num_rows, cols, ragged_cols, properties, 0);
    if (ret != 0) {
        goto out;
    }
    if (metadata_schema != NULL) {
        ret = tsk_edge_table_set_metadata_schema(
            self, metadata_schema, metadata_schema_length);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_edge_table_takeset_columns(
        self, num_rows, left, right, parent, child, metadata, metadata_offset);
    if (ret != 0) {
        goto out;
    }
    left = NULL;
    right = NULL;
    parent = NULL;
    child = NULL;
    metadata = NULL;
    metadata_offset = NULL;
out:
    free_read_table_mem(cols, ragged_cols, properties);
    return ret;
}

int
tsk_edge_table_squash(tsk_edge_table_t *self)
{
    int k;
    int ret = 0;
    tsk_edge_t *edges = NULL;
    tsk_size_t num_output_edges;

    if (self->metadata_length > 0) {
        ret = tsk_trace_error(TSK_ERR_CANT_PROCESS_EDGES_WITH_METADATA);
        goto out;
    }

    edges = tsk_malloc(self->num_rows * sizeof(tsk_edge_t));
    if (edges == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    for (k = 0; k < (int) self->num_rows; k++) {
        edges[k].left = self->left[k];
        edges[k].right = self->right[k];
        edges[k].parent = self->parent[k];
        edges[k].child = self->child[k];
        edges[k].metadata_length = 0;
    }

    ret = tsk_squash_edges(edges, self->num_rows, &num_output_edges);
    if (ret != 0) {
        goto out;
    }
    tsk_edge_table_clear(self);
    tsk_bug_assert(num_output_edges <= self->max_rows);
    self->num_rows = num_output_edges;
    for (k = 0; k < (int) num_output_edges; k++) {
        self->left[k] = edges[k].left;
        self->right[k] = edges[k].right;
        self->parent[k] = edges[k].parent;
        self->child[k] = edges[k].child;
    }
out:
    tsk_safe_free(edges);
    return ret;
}

/*************************
 * site table
 *************************/

static void
tsk_site_table_free_columns(tsk_site_table_t *self)
{
    tsk_safe_free(self->position);
    tsk_safe_free(self->ancestral_state);
    tsk_safe_free(self->ancestral_state_offset);
    tsk_safe_free(self->metadata);
    tsk_safe_free(self->metadata_offset);
}

int
tsk_site_table_free(tsk_site_table_t *self)
{
    tsk_site_table_free_columns(self);
    tsk_safe_free(self->metadata_schema);
    return 0;
}

static int
tsk_site_table_expand_main_columns(tsk_site_table_t *self, tsk_size_t additional_rows)
{
    int ret = 0;
    tsk_size_t new_max_rows;

    ret = calculate_max_rows(self->num_rows, self->max_rows, self->max_rows_increment,
        additional_rows, &new_max_rows);
    if (ret != 0) {
        goto out;
    }
    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column((void **) &self->position, new_max_rows, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->ancestral_state_offset, new_max_rows + 1,
            sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column(
            (void **) &self->metadata_offset, new_max_rows + 1, sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        self->max_rows = new_max_rows;
    }
out:
    return ret;
}

static int
tsk_site_table_expand_ancestral_state(
    tsk_site_table_t *self, tsk_size_t additional_length)
{
    return expand_ragged_column(self->ancestral_state_length, additional_length,
        self->max_ancestral_state_length_increment, &self->max_ancestral_state_length,
        (void **) &self->ancestral_state, sizeof(*self->ancestral_state));
}

static int
tsk_site_table_expand_metadata(tsk_site_table_t *self, tsk_size_t additional_length)
{
    return expand_ragged_column(self->metadata_length, additional_length,
        self->max_metadata_length_increment, &self->max_metadata_length,
        (void **) &self->metadata, sizeof(*self->metadata));
}

int
tsk_site_table_set_max_rows_increment(
    tsk_site_table_t *self, tsk_size_t max_rows_increment)
{
    self->max_rows_increment = max_rows_increment;
    return 0;
}

int
tsk_site_table_set_max_metadata_length_increment(
    tsk_site_table_t *self, tsk_size_t max_metadata_length_increment)
{
    self->max_metadata_length_increment = max_metadata_length_increment;
    return 0;
}

int
tsk_site_table_set_max_ancestral_state_length_increment(
    tsk_site_table_t *self, tsk_size_t max_ancestral_state_length_increment)
{
    self->max_ancestral_state_length_increment = max_ancestral_state_length_increment;
    return 0;
}

int
tsk_site_table_init(tsk_site_table_t *self, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;

    tsk_memset(self, 0, sizeof(tsk_site_table_t));

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
    self->max_rows_increment = 0;
    self->max_ancestral_state_length_increment = 0;
    self->max_metadata_length_increment = 0;
    tsk_site_table_set_metadata_schema(self, NULL, 0);
out:
    return ret;
}

tsk_id_t
tsk_site_table_add_row(tsk_site_table_t *self, double position,
    const char *ancestral_state, tsk_size_t ancestral_state_length, const char *metadata,
    tsk_size_t metadata_length)
{
    tsk_id_t ret = 0;
    tsk_size_t ancestral_state_offset, metadata_offset;

    ret = tsk_site_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->position[self->num_rows] = position;

    ancestral_state_offset = (tsk_size_t) self->ancestral_state_length;
    tsk_bug_assert(
        self->ancestral_state_offset[self->num_rows] == ancestral_state_offset);
    ret = tsk_site_table_expand_ancestral_state(self, ancestral_state_length);
    if (ret != 0) {
        goto out;
    }
    self->ancestral_state_length += ancestral_state_length;
    tsk_memmove(self->ancestral_state + ancestral_state_offset, ancestral_state,
        ancestral_state_length);
    self->ancestral_state_offset[self->num_rows + 1] = self->ancestral_state_length;

    metadata_offset = (tsk_size_t) self->metadata_length;
    tsk_bug_assert(self->metadata_offset[self->num_rows] == metadata_offset);
    ret = tsk_site_table_expand_metadata(self, metadata_length);
    if (ret != 0) {
        goto out;
    }
    self->metadata_length += metadata_length;
    tsk_memmove(self->metadata + metadata_offset, metadata, metadata_length);
    self->metadata_offset[self->num_rows + 1] = self->metadata_length;

    ret = (tsk_id_t) self->num_rows;
    self->num_rows++;
out:
    return ret;
}

static int
tsk_site_table_update_row_rewrite(tsk_site_table_t *self, tsk_id_t index,
    double position, const char *ancestral_state, tsk_size_t ancestral_state_length,
    const char *metadata, tsk_size_t metadata_length)
{
    int ret = 0;
    tsk_id_t j, ret_id;
    tsk_site_table_t copy;
    tsk_size_t num_rows;
    tsk_id_t *rows = NULL;

    ret = tsk_site_table_copy(self, &copy, 0);
    if (ret != 0) {
        goto out;
    }
    rows = tsk_malloc(self->num_rows * sizeof(*rows));
    if (rows == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    ret = tsk_site_table_truncate(self, (tsk_size_t) index);
    tsk_bug_assert(ret == 0);
    ret_id = tsk_site_table_add_row(self, position, ancestral_state,
        ancestral_state_length, metadata, metadata_length);
    if (ret_id < 0) {
        ret = (int) ret_id;
        goto out;
    }
    num_rows = 0;
    for (j = index + 1; j < (tsk_id_t) copy.num_rows; j++) {
        rows[num_rows] = j;
        num_rows++;
    }
    ret = tsk_site_table_extend(self, &copy, num_rows, rows, 0);
    if (ret != 0) {
        goto out;
    }
out:
    tsk_site_table_free(&copy);
    tsk_safe_free(rows);
    return ret;
}

int
tsk_site_table_update_row(tsk_site_table_t *self, tsk_id_t index, double position,
    const char *ancestral_state, tsk_size_t ancestral_state_length, const char *metadata,
    tsk_size_t metadata_length)
{
    int ret = 0;
    tsk_site_t current_row;

    ret = tsk_site_table_get_row(self, index, &current_row);
    if (ret != 0) {
        goto out;
    }
    if (current_row.metadata_length == metadata_length
        && current_row.ancestral_state_length == ancestral_state_length) {
        self->position[index] = position;
        /* Note: important to use tsk_memmove here as we may be provided pointers
         * to the column memory as input via get_row */
        tsk_memmove(&self->ancestral_state[self->ancestral_state_offset[index]],
            ancestral_state, ancestral_state_length * sizeof(*ancestral_state));
        tsk_memmove(&self->metadata[self->metadata_offset[index]], metadata,
            metadata_length * sizeof(*metadata));
    } else {
        ret = tsk_site_table_update_row_rewrite(self, index, position, ancestral_state,
            ancestral_state_length, metadata, metadata_length);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

int
tsk_site_table_append_columns(tsk_site_table_t *self, tsk_size_t num_rows,
    const double *position, const char *ancestral_state,
    const tsk_size_t *ancestral_state_offset, const char *metadata,
    const tsk_size_t *metadata_offset)
{
    int ret = 0;
    tsk_size_t j, ancestral_state_length, metadata_length;

    if (position == NULL || ancestral_state == NULL || ancestral_state_offset == NULL) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    if ((metadata == NULL) != (metadata_offset == NULL)) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }

    ret = tsk_site_table_expand_main_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    tsk_memcpy(self->position + self->num_rows, position, num_rows * sizeof(double));

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
        tsk_memcpy(self->metadata + self->metadata_length, metadata,
            metadata_length * sizeof(char));
        for (j = 0; j < num_rows; j++) {
            self->metadata_offset[self->num_rows + j]
                = self->metadata_length + metadata_offset[j];
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
    tsk_memcpy(self->ancestral_state + self->ancestral_state_length, ancestral_state,
        ancestral_state_length * sizeof(char));
    for (j = 0; j < num_rows; j++) {
        self->ancestral_state_offset[self->num_rows + j]
            = self->ancestral_state_length + ancestral_state_offset[j];
    }
    self->ancestral_state_length += ancestral_state_length;
    self->ancestral_state_offset[self->num_rows + num_rows]
        = self->ancestral_state_length;

    self->num_rows += num_rows;
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_site_table_copy(
    const tsk_site_table_t *self, tsk_site_table_t *dest, tsk_flags_t options)
{
    int ret = 0;

    if (!(options & TSK_NO_INIT)) {
        ret = tsk_site_table_init(dest, 0);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_site_table_set_columns(dest, self->num_rows, self->position,
        self->ancestral_state, self->ancestral_state_offset, self->metadata,
        self->metadata_offset);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_site_table_set_metadata_schema(
        dest, self->metadata_schema, self->metadata_schema_length);
out:
    return ret;
}

int
tsk_site_table_set_columns(tsk_site_table_t *self, tsk_size_t num_rows,
    const double *position, const char *ancestral_state,
    const tsk_size_t *ancestral_state_offset, const char *metadata,
    const tsk_size_t *metadata_offset)
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

int
tsk_site_table_takeset_columns(tsk_site_table_t *self, tsk_size_t num_rows,
    double *position, char *ancestral_state, tsk_size_t *ancestral_state_offset,
    char *metadata, tsk_size_t *metadata_offset)
{
    int ret = 0;

    /* We need to check all the inputs before we start freeing or taking memory */
    if (position == NULL || ancestral_state == NULL || ancestral_state_offset == NULL) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    ret = check_ragged_column(num_rows, ancestral_state, ancestral_state_offset);
    if (ret != 0) {
        goto out;
    }
    ret = check_ragged_column(num_rows, metadata, metadata_offset);
    if (ret != 0) {
        goto out;
    }

    tsk_site_table_free_columns(self);
    self->num_rows = num_rows;
    self->max_rows = num_rows;
    self->position = position;

    ret = takeset_ragged_column(num_rows, ancestral_state, ancestral_state_offset,
        (void *) &self->ancestral_state, &self->ancestral_state_offset,
        &self->ancestral_state_length);
    if (ret != 0) {
        goto out;
    }
    ret = takeset_ragged_column(num_rows, metadata, metadata_offset,
        (void *) &self->metadata, &self->metadata_offset, &self->metadata_length);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

bool
tsk_site_table_equals(
    const tsk_site_table_t *self, const tsk_site_table_t *other, tsk_flags_t options)
{
    bool ret
        = self->num_rows == other->num_rows
          && self->ancestral_state_length == other->ancestral_state_length
          && tsk_memcmp(self->position, other->position, self->num_rows * sizeof(double))
                 == 0
          && tsk_memcmp(self->ancestral_state_offset, other->ancestral_state_offset,
                 (self->num_rows + 1) * sizeof(tsk_size_t))
                 == 0
          && tsk_memcmp(self->ancestral_state, other->ancestral_state,
                 self->ancestral_state_length * sizeof(char))
                 == 0;
    if (!(options & TSK_CMP_IGNORE_METADATA)) {
        ret = ret && self->metadata_length == other->metadata_length
              && self->metadata_schema_length == other->metadata_schema_length
              && tsk_memcmp(self->metadata_offset, other->metadata_offset,
                     (self->num_rows + 1) * sizeof(tsk_size_t))
                     == 0
              && tsk_memcmp(self->metadata, other->metadata,
                     self->metadata_length * sizeof(char))
                     == 0
              && tsk_memcmp(self->metadata_schema, other->metadata_schema,
                     self->metadata_schema_length * sizeof(char))
                     == 0;
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
        ret = tsk_trace_error(TSK_ERR_BAD_TABLE_POSITION);
        goto out;
    }
    self->num_rows = num_rows;
    self->ancestral_state_length = self->ancestral_state_offset[num_rows];
    self->metadata_length = self->metadata_offset[num_rows];
out:
    return ret;
}

int
tsk_site_table_extend(tsk_site_table_t *self, const tsk_site_table_t *other,
    tsk_size_t num_rows, const tsk_id_t *row_indexes, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_id_t ret_id;
    tsk_size_t j;
    tsk_site_t site;

    if (self == other) {
        ret = tsk_trace_error(TSK_ERR_CANNOT_EXTEND_FROM_SELF);
        goto out;
    }

    /* We know how much to expand the non-ragged columns, so do it ahead of time */
    ret = tsk_site_table_expand_main_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < num_rows; j++) {
        ret = tsk_site_table_get_row(
            other, row_indexes == NULL ? (tsk_id_t) j : row_indexes[j], &site);
        if (ret != 0) {
            goto out;
        }
        ret_id = tsk_site_table_add_row(self, site.position, site.ancestral_state,
            site.ancestral_state_length, site.metadata, site.metadata_length);
        if (ret_id < 0) {
            ret = (int) ret_id;
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

void
tsk_site_table_print_state(const tsk_site_table_t *self, FILE *out)
{
    int ret;

    fprintf(out, "\n" TABLE_SEP);
    fprintf(out, "site_table: %p:\n", (const void *) self);
    fprintf(out, "num_rows = %lld\t(max= %lld\tincrement = %lld)\n",
        (long long) self->num_rows, (long long) self->max_rows,
        (long long) self->max_rows_increment);
    fprintf(out, "ancestral_state_length = %lld\t(max= %lld\tincrement = %lld)\n",
        (long long) self->ancestral_state_length,
        (long long) self->max_ancestral_state_length,
        (long long) self->max_ancestral_state_length_increment);
    fprintf(out, "metadata_length = %lld(\tmax= %lld\tincrement = %lld)\n",
        (long long) self->metadata_length, (long long) self->max_metadata_length,
        (long long) self->max_metadata_length_increment);
    fprintf(out, TABLE_SEP);
    ret = tsk_site_table_dump_text(self, out);
    tsk_bug_assert(ret == 0);

    tsk_bug_assert(self->ancestral_state_offset[0] == 0);
    tsk_bug_assert(
        self->ancestral_state_length == self->ancestral_state_offset[self->num_rows]);
    tsk_bug_assert(self->metadata_offset[0] == 0);
    tsk_bug_assert(self->metadata_length == self->metadata_offset[self->num_rows]);
}

static inline void
tsk_site_table_get_row_unsafe(
    const tsk_site_table_t *self, tsk_id_t index, tsk_site_t *row)
{
    row->id = (tsk_id_t) index;
    row->position = self->position[index];
    row->ancestral_state_length
        = self->ancestral_state_offset[index + 1] - self->ancestral_state_offset[index];
    row->ancestral_state = self->ancestral_state + self->ancestral_state_offset[index];
    row->metadata_length
        = self->metadata_offset[index + 1] - self->metadata_offset[index];
    row->metadata = self->metadata + self->metadata_offset[index];
    /* This struct has a placeholder for mutations. Probably should be separate
     * structs for this (tsk_site_table_row_t?) */
    row->mutations_length = 0;
    row->mutations = NULL;
}

int
tsk_site_table_get_row(const tsk_site_table_t *self, tsk_id_t index, tsk_site_t *row)
{
    int ret = 0;

    if (index < 0 || index >= (tsk_id_t) self->num_rows) {
        ret = tsk_trace_error(TSK_ERR_SITE_OUT_OF_BOUNDS);
        goto out;
    }
    tsk_site_table_get_row_unsafe(self, index, row);
out:
    return ret;
}

int
tsk_site_table_set_metadata_schema(tsk_site_table_t *self, const char *metadata_schema,
    tsk_size_t metadata_schema_length)
{
    return replace_string(&self->metadata_schema, &self->metadata_schema_length,
        metadata_schema, metadata_schema_length);
}

int
tsk_site_table_dump_text(const tsk_site_table_t *self, FILE *out)
{
    tsk_size_t j;
    int ret = TSK_ERR_IO;
    int err;
    tsk_size_t ancestral_state_len, metadata_len;

    err = write_metadata_schema_header(
        out, self->metadata_schema, self->metadata_schema_length);
    if (err < 0) {
        goto out;
    }
    err = fprintf(out, "id\tposition\tancestral_state\tmetadata\n");
    if (err < 0) {
        goto out;
    }
    for (j = 0; j < self->num_rows; j++) {
        ancestral_state_len
            = self->ancestral_state_offset[j + 1] - self->ancestral_state_offset[j];
        metadata_len = self->metadata_offset[j + 1] - self->metadata_offset[j];
        err = fprintf(out, "%lld\t%f\t%.*s\t%.*s\n", (long long) j, self->position[j],
            (int) ancestral_state_len,
            self->ancestral_state + self->ancestral_state_offset[j], (int) metadata_len,
            self->metadata + self->metadata_offset[j]);
        if (err < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

int
tsk_site_table_keep_rows(tsk_site_table_t *self, const tsk_bool_t *keep,
    tsk_flags_t TSK_UNUSED(options), tsk_id_t *id_map)
{
    int ret = 0;
    tsk_size_t remaining_rows;

    if (id_map != NULL) {
        keep_mask_to_id_map(self->num_rows, keep, id_map);
    }

    remaining_rows = subset_double_column(self->position, self->num_rows, keep);
    self->ancestral_state_length = subset_ragged_char_column(
        self->ancestral_state, self->ancestral_state_offset, self->num_rows, keep);
    if (self->metadata_length > 0) {
        self->metadata_length = subset_ragged_char_column(
            self->metadata, self->metadata_offset, self->num_rows, keep);
    }
    self->num_rows = remaining_rows;
    return ret;
}

static int
tsk_site_table_dump(const tsk_site_table_t *self, kastore_t *store, tsk_flags_t options)
{
    const write_table_col_t cols[] = {
        { "sites/position", (void *) self->position, self->num_rows, KAS_FLOAT64 },
        { "sites/metadata_schema", (void *) self->metadata_schema,
            self->metadata_schema_length, KAS_UINT8 },
        { .name = NULL },
    };
    const write_table_ragged_col_t ragged_cols[] = {
        { "sites/ancestral_state", (void *) self->ancestral_state,
            self->ancestral_state_length, KAS_UINT8, self->ancestral_state_offset,
            self->num_rows },
        { "sites/metadata", (void *) self->metadata, self->metadata_length, KAS_UINT8,
            self->metadata_offset, self->num_rows },
        { .name = NULL },
    };

    return write_table(store, cols, ragged_cols, options);
}

static int
tsk_site_table_load(tsk_site_table_t *self, kastore_t *store)
{
    int ret = 0;
    char *metadata_schema = NULL;
    double *position = NULL;
    char *ancestral_state = NULL;
    tsk_size_t *ancestral_state_offset = NULL;
    char *metadata = NULL;
    tsk_size_t *metadata_offset = NULL;
    tsk_size_t num_rows, ancestral_state_length, metadata_length, metadata_schema_length;

    read_table_col_t cols[] = {
        { "sites/position", (void **) &position, KAS_FLOAT64, 0 },
        { .name = NULL },
    };
    read_table_ragged_col_t ragged_cols[] = {
        { "sites/ancestral_state", (void **) &ancestral_state, &ancestral_state_length,
            KAS_UINT8, &ancestral_state_offset, 0 },
        { "sites/metadata", (void **) &metadata, &metadata_length, KAS_UINT8,
            &metadata_offset, 0 },
        { .name = NULL },
    };
    read_table_property_t properties[] = {
        { "sites/metadata_schema", (void **) &metadata_schema, &metadata_schema_length,
            KAS_UINT8, TSK_COL_OPTIONAL },
        { .name = NULL },
    };

    ret = read_table(store, &num_rows, cols, ragged_cols, properties, 0);
    if (ret != 0) {
        goto out;
    }
    if (metadata_schema != NULL) {
        ret = tsk_site_table_set_metadata_schema(
            self, metadata_schema, metadata_schema_length);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_site_table_takeset_columns(self, num_rows, position, ancestral_state,
        ancestral_state_offset, metadata, metadata_offset);
    if (ret != 0) {
        goto out;
    }
    position = NULL;
    ancestral_state = NULL;
    ancestral_state_offset = NULL;
    metadata = NULL;
    metadata_offset = NULL;

out:
    free_read_table_mem(cols, ragged_cols, properties);
    return ret;
}

/*************************
 * mutation table
 *************************/

static void
tsk_mutation_table_free_columns(tsk_mutation_table_t *self)
{
    tsk_safe_free(self->node);
    tsk_safe_free(self->site);
    tsk_safe_free(self->parent);
    tsk_safe_free(self->time);
    tsk_safe_free(self->derived_state);
    tsk_safe_free(self->derived_state_offset);
    tsk_safe_free(self->metadata);
    tsk_safe_free(self->metadata_offset);
}

int
tsk_mutation_table_free(tsk_mutation_table_t *self)
{
    tsk_mutation_table_free_columns(self);
    tsk_safe_free(self->metadata_schema);
    return 0;
}

static int
tsk_mutation_table_expand_main_columns(
    tsk_mutation_table_t *self, tsk_size_t additional_rows)
{
    int ret = 0;
    tsk_size_t new_max_rows;

    ret = calculate_max_rows(self->num_rows, self->max_rows, self->max_rows_increment,
        additional_rows, &new_max_rows);
    if (ret != 0) {
        goto out;
    }
    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column((void **) &self->site, new_max_rows, sizeof(tsk_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->node, new_max_rows, sizeof(tsk_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->parent, new_max_rows, sizeof(tsk_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->time, new_max_rows, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column(
            (void **) &self->derived_state_offset, new_max_rows + 1, sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column(
            (void **) &self->metadata_offset, new_max_rows + 1, sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        self->max_rows = new_max_rows;
    }
out:
    return ret;
}

static int
tsk_mutation_table_expand_derived_state(
    tsk_mutation_table_t *self, tsk_size_t additional_length)
{
    return expand_ragged_column(self->derived_state_length, additional_length,
        self->max_derived_state_length_increment, &self->max_derived_state_length,
        (void **) &self->derived_state, sizeof(*self->derived_state));
}

static int
tsk_mutation_table_expand_metadata(
    tsk_mutation_table_t *self, tsk_size_t additional_length)
{
    return expand_ragged_column(self->metadata_length, additional_length,
        self->max_metadata_length_increment, &self->max_metadata_length,
        (void **) &self->metadata, sizeof(*self->metadata));
}

int
tsk_mutation_table_set_max_rows_increment(
    tsk_mutation_table_t *self, tsk_size_t max_rows_increment)
{
    self->max_rows_increment = max_rows_increment;
    return 0;
}

int
tsk_mutation_table_set_max_metadata_length_increment(
    tsk_mutation_table_t *self, tsk_size_t max_metadata_length_increment)
{
    self->max_metadata_length_increment = max_metadata_length_increment;
    return 0;
}

int
tsk_mutation_table_set_max_derived_state_length_increment(
    tsk_mutation_table_t *self, tsk_size_t max_derived_state_length_increment)
{
    self->max_derived_state_length_increment = max_derived_state_length_increment;
    return 0;
}

int
tsk_mutation_table_init(tsk_mutation_table_t *self, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;

    tsk_memset(self, 0, sizeof(tsk_mutation_table_t));

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
    self->max_rows_increment = 0;
    self->max_derived_state_length_increment = 0;
    self->max_metadata_length_increment = 0;
    tsk_mutation_table_set_metadata_schema(self, NULL, 0);
out:
    return ret;
}

tsk_id_t
tsk_mutation_table_add_row(tsk_mutation_table_t *self, tsk_id_t site, tsk_id_t node,
    tsk_id_t parent, double time, const char *derived_state,
    tsk_size_t derived_state_length, const char *metadata, tsk_size_t metadata_length)
{
    tsk_id_t ret;
    tsk_size_t derived_state_offset, metadata_offset;

    ret = tsk_mutation_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->site[self->num_rows] = site;
    self->node[self->num_rows] = node;
    self->parent[self->num_rows] = parent;
    self->time[self->num_rows] = time;

    derived_state_offset = self->derived_state_length;
    tsk_bug_assert(self->derived_state_offset[self->num_rows] == derived_state_offset);
    ret = tsk_mutation_table_expand_derived_state(self, derived_state_length);
    if (ret != 0) {
        goto out;
    }
    self->derived_state_length += derived_state_length;
    tsk_memmove(
        self->derived_state + derived_state_offset, derived_state, derived_state_length);
    self->derived_state_offset[self->num_rows + 1] = self->derived_state_length;

    metadata_offset = self->metadata_length;
    tsk_bug_assert(self->metadata_offset[self->num_rows] == metadata_offset);
    ret = tsk_mutation_table_expand_metadata(self, metadata_length);
    if (ret != 0) {
        goto out;
    }
    self->metadata_length += metadata_length;
    tsk_memmove(self->metadata + metadata_offset, metadata, metadata_length);
    self->metadata_offset[self->num_rows + 1] = self->metadata_length;

    ret = (tsk_id_t) self->num_rows;
    self->num_rows++;
out:
    return ret;
}

static int
tsk_mutation_table_update_row_rewrite(tsk_mutation_table_t *self, tsk_id_t index,
    tsk_id_t site, tsk_id_t node, tsk_id_t parent, double time,
    const char *derived_state, tsk_size_t derived_state_length, const char *metadata,
    tsk_size_t metadata_length)
{
    int ret = 0;
    tsk_id_t j, ret_id;
    tsk_mutation_table_t copy;
    tsk_size_t num_rows;
    tsk_id_t *rows = NULL;

    ret = tsk_mutation_table_copy(self, &copy, 0);
    if (ret != 0) {
        goto out;
    }
    rows = tsk_malloc(self->num_rows * sizeof(*rows));
    if (rows == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    ret = tsk_mutation_table_truncate(self, (tsk_size_t) index);
    tsk_bug_assert(ret == 0);
    ret_id = tsk_mutation_table_add_row(self, site, node, parent, time, derived_state,
        derived_state_length, metadata, metadata_length);
    if (ret_id < 0) {
        ret = (int) ret_id;
        goto out;
    }
    num_rows = 0;
    for (j = index + 1; j < (tsk_id_t) copy.num_rows; j++) {
        rows[num_rows] = j;
        num_rows++;
    }
    ret = tsk_mutation_table_extend(self, &copy, num_rows, rows, 0);
    if (ret != 0) {
        goto out;
    }
out:
    tsk_mutation_table_free(&copy);
    tsk_safe_free(rows);
    return ret;
}

int
tsk_mutation_table_update_row(tsk_mutation_table_t *self, tsk_id_t index, tsk_id_t site,
    tsk_id_t node, tsk_id_t parent, double time, const char *derived_state,
    tsk_size_t derived_state_length, const char *metadata, tsk_size_t metadata_length)
{
    int ret = 0;
    tsk_mutation_t current_row;

    ret = tsk_mutation_table_get_row(self, index, &current_row);
    if (ret != 0) {
        goto out;
    }
    if (current_row.metadata_length == metadata_length
        && current_row.derived_state_length == derived_state_length) {
        self->site[index] = site;
        self->node[index] = node;
        self->parent[index] = parent;
        self->time[index] = time;
        /* Note: important to use tsk_memmove here as we may be provided pointers
         * to the column memory as input via get_row */
        tsk_memmove(&self->derived_state[self->derived_state_offset[index]],
            derived_state, derived_state_length * sizeof(*derived_state));
        tsk_memmove(&self->metadata[self->metadata_offset[index]], metadata,
            metadata_length * sizeof(*metadata));
    } else {
        ret = tsk_mutation_table_update_row_rewrite(self, index, site, node, parent,
            time, derived_state, derived_state_length, metadata, metadata_length);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

int
tsk_mutation_table_append_columns(tsk_mutation_table_t *self, tsk_size_t num_rows,
    const tsk_id_t *site, const tsk_id_t *node, const tsk_id_t *parent,
    const double *time, const char *derived_state,
    const tsk_size_t *derived_state_offset, const char *metadata,
    const tsk_size_t *metadata_offset)
{
    int ret = 0;
    tsk_size_t j, derived_state_length, metadata_length;

    if (site == NULL || node == NULL || derived_state == NULL
        || derived_state_offset == NULL) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    if ((metadata == NULL) != (metadata_offset == NULL)) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }

    ret = tsk_mutation_table_expand_main_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    tsk_memcpy(self->site + self->num_rows, site, num_rows * sizeof(tsk_id_t));
    tsk_memcpy(self->node + self->num_rows, node, num_rows * sizeof(tsk_id_t));
    if (parent == NULL) {
        /* If parent is NULL, set all parents to the null mutation */
        tsk_memset(self->parent + self->num_rows, 0xff, num_rows * sizeof(tsk_id_t));
    } else {
        tsk_memcpy(self->parent + self->num_rows, parent, num_rows * sizeof(tsk_id_t));
    }
    if (time == NULL) {
        /* If time is NULL, set all times to TSK_UNKNOWN_TIME which is the
         * default */
        for (j = 0; j < num_rows; j++) {
            self->time[self->num_rows + j] = TSK_UNKNOWN_TIME;
        }
    } else {
        tsk_memcpy(self->time + self->num_rows, time, num_rows * sizeof(double));
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
        tsk_memcpy(self->metadata + self->metadata_length, metadata,
            metadata_length * sizeof(char));
        for (j = 0; j < num_rows; j++) {
            self->metadata_offset[self->num_rows + j]
                = self->metadata_length + metadata_offset[j];
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
    tsk_memcpy(self->derived_state + self->derived_state_length, derived_state,
        derived_state_length * sizeof(char));
    for (j = 0; j < num_rows; j++) {
        self->derived_state_offset[self->num_rows + j]
            = self->derived_state_length + derived_state_offset[j];
    }
    self->derived_state_length += derived_state_length;
    self->derived_state_offset[self->num_rows + num_rows] = self->derived_state_length;

    self->num_rows += num_rows;
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_mutation_table_takeset_columns(tsk_mutation_table_t *self, tsk_size_t num_rows,
    tsk_id_t *site, tsk_id_t *node, tsk_id_t *parent, double *time, char *derived_state,
    tsk_size_t *derived_state_offset, char *metadata, tsk_size_t *metadata_offset)
{
    tsk_size_t j;
    int ret = 0;

    if (site == NULL || node == NULL || derived_state == NULL
        || derived_state_offset == NULL) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    /* We need to check all the inputs before we start freeing or taking memory */
    ret = check_ragged_column(num_rows, derived_state, derived_state_offset);
    if (ret != 0) {
        goto out;
    }
    ret = check_ragged_column(num_rows, metadata, metadata_offset);
    if (ret != 0) {
        goto out;
    }

    tsk_mutation_table_free_columns(self);
    self->num_rows = num_rows;
    self->max_rows = num_rows;
    self->site = site;
    self->node = node;

    ret = takeset_optional_id_column(num_rows, parent, &self->parent);
    if (ret != 0) {
        goto out;
    }
    if (time == NULL) {
        /* Time defaults to unknown time if not specified. */
        self->time = tsk_malloc(num_rows * sizeof(*self->time));
        if (self->time == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
        for (j = 0; j < num_rows; j++) {
            self->time[j] = TSK_UNKNOWN_TIME;
        }

    } else {
        self->time = time;
    }

    ret = takeset_ragged_column(num_rows, derived_state, derived_state_offset,
        (void *) &self->derived_state, &self->derived_state_offset,
        &self->derived_state_length);
    if (ret != 0) {
        goto out;
    }
    ret = takeset_ragged_column(num_rows, metadata, metadata_offset,
        (void *) &self->metadata, &self->metadata_offset, &self->metadata_length);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_mutation_table_copy(
    const tsk_mutation_table_t *self, tsk_mutation_table_t *dest, tsk_flags_t options)
{
    int ret = 0;

    if (!(options & TSK_NO_INIT)) {
        ret = tsk_mutation_table_init(dest, 0);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_mutation_table_set_columns(dest, self->num_rows, self->site, self->node,
        self->parent, self->time, self->derived_state, self->derived_state_offset,
        self->metadata, self->metadata_offset);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_mutation_table_set_metadata_schema(
        dest, self->metadata_schema, self->metadata_schema_length);
out:
    return ret;
}

int
tsk_mutation_table_set_columns(tsk_mutation_table_t *self, tsk_size_t num_rows,
    const tsk_id_t *site, const tsk_id_t *node, const tsk_id_t *parent,
    const double *time, const char *derived_state,
    const tsk_size_t *derived_state_offset, const char *metadata,
    const tsk_size_t *metadata_offset)
{
    int ret = 0;

    ret = tsk_mutation_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_mutation_table_append_columns(self, num_rows, site, node, parent, time,
        derived_state, derived_state_offset, metadata, metadata_offset);
out:
    return ret;
}

bool
tsk_mutation_table_equals(const tsk_mutation_table_t *self,
    const tsk_mutation_table_t *other, tsk_flags_t options)
{
    bool ret
        = self->num_rows == other->num_rows
          && self->derived_state_length == other->derived_state_length
          && tsk_memcmp(self->site, other->site, self->num_rows * sizeof(tsk_id_t)) == 0
          && tsk_memcmp(self->node, other->node, self->num_rows * sizeof(tsk_id_t)) == 0
          && tsk_memcmp(self->parent, other->parent, self->num_rows * sizeof(tsk_id_t))
                 == 0
          && tsk_memcmp(self->time, other->time, self->num_rows * sizeof(double)) == 0
          && tsk_memcmp(self->derived_state_offset, other->derived_state_offset,
                 (self->num_rows + 1) * sizeof(tsk_size_t))
                 == 0
          && tsk_memcmp(self->derived_state, other->derived_state,
                 self->derived_state_length * sizeof(char))
                 == 0;
    if (!(options & TSK_CMP_IGNORE_METADATA)) {
        ret = ret && self->metadata_length == other->metadata_length
              && self->metadata_schema_length == other->metadata_schema_length
              && tsk_memcmp(self->metadata_offset, other->metadata_offset,
                     (self->num_rows + 1) * sizeof(tsk_size_t))
                     == 0
              && tsk_memcmp(self->metadata, other->metadata,
                     self->metadata_length * sizeof(char))
                     == 0
              && tsk_memcmp(self->metadata_schema, other->metadata_schema,
                     self->metadata_schema_length * sizeof(char))
                     == 0
              && tsk_memcmp(self->metadata_schema, other->metadata_schema,
                     self->metadata_schema_length * sizeof(char))
                     == 0;
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
        ret = tsk_trace_error(TSK_ERR_BAD_TABLE_POSITION);
        goto out;
    }
    mutations->num_rows = num_rows;
    mutations->derived_state_length = mutations->derived_state_offset[num_rows];
    mutations->metadata_length = mutations->metadata_offset[num_rows];
out:
    return ret;
}

int
tsk_mutation_table_extend(tsk_mutation_table_t *self, const tsk_mutation_table_t *other,
    tsk_size_t num_rows, const tsk_id_t *row_indexes, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_id_t ret_id;
    tsk_size_t j;
    tsk_mutation_t mutation;

    if (self == other) {
        ret = tsk_trace_error(TSK_ERR_CANNOT_EXTEND_FROM_SELF);
        goto out;
    }

    /* We know how much to expand the non-ragged columns, so do it ahead of time */
    ret = tsk_mutation_table_expand_main_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < num_rows; j++) {
        ret = tsk_mutation_table_get_row(
            other, row_indexes == NULL ? (tsk_id_t) j : row_indexes[j], &mutation);
        if (ret != 0) {
            goto out;
        }
        ret_id = tsk_mutation_table_add_row(self, mutation.site, mutation.node,
            mutation.parent, mutation.time, mutation.derived_state,
            mutation.derived_state_length, mutation.metadata, mutation.metadata_length);
        if (ret_id < 0) {
            ret = (int) ret_id;
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

void
tsk_mutation_table_print_state(const tsk_mutation_table_t *self, FILE *out)
{
    int ret;

    fprintf(out, "\n" TABLE_SEP);
    fprintf(out, "mutation_table: %p:\n", (const void *) self);
    fprintf(out, "num_rows = %lld\tmax= %lld\tincrement = %lld)\n",
        (long long) self->num_rows, (long long) self->max_rows,
        (long long) self->max_rows_increment);
    fprintf(out, "derived_state_length = %lld\tmax= %lld\tincrement = %lld)\n",
        (long long) self->derived_state_length,
        (long long) self->max_derived_state_length,
        (long long) self->max_derived_state_length_increment);
    fprintf(out, "metadata_length = %lld\tmax= %lld\tincrement = %lld)\n",
        (long long) self->metadata_length, (long long) self->max_metadata_length,
        (long long) self->max_metadata_length_increment);
    fprintf(out, TABLE_SEP);
    ret = tsk_mutation_table_dump_text(self, out);
    tsk_bug_assert(ret == 0);
    tsk_bug_assert(self->derived_state_offset[0] == 0);
    tsk_bug_assert(
        self->derived_state_length == self->derived_state_offset[self->num_rows]);
    tsk_bug_assert(self->metadata_offset[0] == 0);
    tsk_bug_assert(self->metadata_length == self->metadata_offset[self->num_rows]);
}

static inline void
tsk_mutation_table_get_row_unsafe(
    const tsk_mutation_table_t *self, tsk_id_t index, tsk_mutation_t *row)
{
    row->id = (tsk_id_t) index;
    row->site = self->site[index];
    row->node = self->node[index];
    row->parent = self->parent[index];
    row->time = self->time[index];
    row->derived_state_length
        = self->derived_state_offset[index + 1] - self->derived_state_offset[index];
    row->derived_state = self->derived_state + self->derived_state_offset[index];
    row->metadata_length
        = self->metadata_offset[index + 1] - self->metadata_offset[index];
    row->metadata = self->metadata + self->metadata_offset[index];
    row->edge = TSK_NULL;
}

int
tsk_mutation_table_get_row(
    const tsk_mutation_table_t *self, tsk_id_t index, tsk_mutation_t *row)
{
    int ret = 0;

    if (index < 0 || index >= (tsk_id_t) self->num_rows) {
        ret = tsk_trace_error(TSK_ERR_MUTATION_OUT_OF_BOUNDS);
        goto out;
    }
    tsk_mutation_table_get_row_unsafe(self, index, row);
out:
    return ret;
}

int
tsk_mutation_table_set_metadata_schema(tsk_mutation_table_t *self,
    const char *metadata_schema, tsk_size_t metadata_schema_length)
{
    return replace_string(&self->metadata_schema, &self->metadata_schema_length,
        metadata_schema, metadata_schema_length);
}

int
tsk_mutation_table_dump_text(const tsk_mutation_table_t *self, FILE *out)
{
    int ret = TSK_ERR_IO;
    int err;
    tsk_size_t j, derived_state_len, metadata_len;

    err = write_metadata_schema_header(
        out, self->metadata_schema, self->metadata_schema_length);
    if (err < 0) {
        goto out;
    }
    err = fprintf(out, "id\tsite\tnode\tparent\ttime\tderived_state\tmetadata\n");
    if (err < 0) {
        goto out;
    }
    for (j = 0; j < self->num_rows; j++) {
        derived_state_len
            = self->derived_state_offset[j + 1] - self->derived_state_offset[j];
        metadata_len = self->metadata_offset[j + 1] - self->metadata_offset[j];
        err = fprintf(out, "%lld\t%lld\t%lld\t%lld\t%f\t%.*s\t%.*s\n", (long long) j,
            (long long) self->site[j], (long long) self->node[j],
            (long long) self->parent[j], self->time[j], (int) derived_state_len,
            self->derived_state + self->derived_state_offset[j], (int) metadata_len,
            self->metadata + self->metadata_offset[j]);
        if (err < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

int
tsk_mutation_table_keep_rows(tsk_mutation_table_t *self, const tsk_bool_t *keep,
    tsk_flags_t TSK_UNUSED(options), tsk_id_t *ret_id_map)
{
    int ret = 0;
    const tsk_size_t current_num_rows = self->num_rows;
    tsk_size_t j, remaining_rows;
    tsk_id_t pj;
    tsk_id_t *id_map = ret_id_map;
    tsk_id_t *restrict parent = self->parent;

    if (ret_id_map == NULL) {
        id_map = tsk_malloc(current_num_rows * sizeof(*id_map));
        if (id_map == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
    }

    keep_mask_to_id_map(current_num_rows, keep, id_map);

    /* Note: we could add some options to avoid these checks if we wanted.
     * MAP_DELETED_TO_NULL is an obvious one, and I guess it might be
     * helpful to also provide NO_REMAP to prevent reference remapping
     * entirely. */
    for (j = 0; j < current_num_rows; j++) {
        if (keep[j]) {
            pj = parent[j];
            if (pj != TSK_NULL) {
                if (pj < 0 || pj >= (tsk_id_t) current_num_rows) {
                    ret = tsk_trace_error(TSK_ERR_MUTATION_OUT_OF_BOUNDS);
                    goto out;
                }
                if (id_map[pj] == TSK_NULL) {
                    ret = tsk_trace_error(TSK_ERR_KEEP_ROWS_MAP_TO_DELETED);
                    goto out;
                }
            }
        }
    }

    remaining_rows = subset_id_column(self->site, current_num_rows, keep);
    subset_id_column(self->node, current_num_rows, keep);
    subset_remap_id_column(parent, current_num_rows, keep, id_map);
    subset_double_column(self->time, current_num_rows, keep);
    self->derived_state_length = subset_ragged_char_column(
        self->derived_state, self->derived_state_offset, current_num_rows, keep);
    if (self->metadata_length > 0) {
        self->metadata_length = subset_ragged_char_column(
            self->metadata, self->metadata_offset, current_num_rows, keep);
    }
    self->num_rows = remaining_rows;
out:
    if (ret_id_map == NULL) {
        tsk_safe_free(id_map);
    }
    return ret;
}

static int
tsk_mutation_table_dump(
    const tsk_mutation_table_t *self, kastore_t *store, tsk_flags_t options)
{
    const write_table_col_t cols[] = {
        { "mutations/site", (void *) self->site, self->num_rows, TSK_ID_STORAGE_TYPE },
        { "mutations/node", (void *) self->node, self->num_rows, TSK_ID_STORAGE_TYPE },
        { "mutations/parent", (void *) self->parent, self->num_rows,
            TSK_ID_STORAGE_TYPE },
        { "mutations/time", (void *) self->time, self->num_rows, KAS_FLOAT64 },
        { "mutations/metadata_schema", (void *) self->metadata_schema,
            self->metadata_schema_length, KAS_UINT8 },
        { .name = NULL },
    };
    const write_table_ragged_col_t ragged_cols[] = {
        { "mutations/derived_state", (void *) self->derived_state,
            self->derived_state_length, KAS_UINT8, self->derived_state_offset,
            self->num_rows },
        { "mutations/metadata", (void *) self->metadata, self->metadata_length,
            KAS_UINT8, self->metadata_offset, self->num_rows },
        { .name = NULL },
    };

    return write_table(store, cols, ragged_cols, options);
}

static int
tsk_mutation_table_load(tsk_mutation_table_t *self, kastore_t *store)
{
    int ret = 0;
    tsk_id_t *node = NULL;
    tsk_id_t *site = NULL;
    tsk_id_t *parent = NULL;
    double *time = NULL;
    char *derived_state = NULL;
    tsk_size_t *derived_state_offset = NULL;
    char *metadata = NULL;
    tsk_size_t *metadata_offset = NULL;
    char *metadata_schema = NULL;
    tsk_size_t num_rows, derived_state_length, metadata_length, metadata_schema_length;

    read_table_col_t cols[] = {
        { "mutations/site", (void **) &site, TSK_ID_STORAGE_TYPE, 0 },
        { "mutations/node", (void **) &node, TSK_ID_STORAGE_TYPE, 0 },
        { "mutations/parent", (void **) &parent, TSK_ID_STORAGE_TYPE, 0 },
        { "mutations/time", (void **) &time, KAS_FLOAT64, TSK_COL_OPTIONAL },
        { .name = NULL },
    };
    read_table_ragged_col_t ragged_cols[] = {
        { "mutations/derived_state", (void **) &derived_state, &derived_state_length,
            KAS_UINT8, &derived_state_offset, 0 },
        { "mutations/metadata", (void **) &metadata, &metadata_length, KAS_UINT8,
            &metadata_offset, 0 },
        { .name = NULL },
    };
    read_table_property_t properties[] = {
        { "mutations/metadata_schema", (void **) &metadata_schema,
            &metadata_schema_length, KAS_UINT8, TSK_COL_OPTIONAL },
        { .name = NULL },
    };

    ret = read_table(store, &num_rows, cols, ragged_cols, properties, 0);
    if (ret != 0) {
        goto out;
    }
    if (metadata_schema != NULL) {
        ret = tsk_mutation_table_set_metadata_schema(
            self, metadata_schema, metadata_schema_length);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_mutation_table_takeset_columns(self, num_rows, site, node, parent, time,
        derived_state, derived_state_offset, metadata, metadata_offset);
    if (ret != 0) {
        goto out;
    }
    site = NULL;
    node = NULL;
    parent = NULL;
    time = NULL;
    derived_state = NULL;
    derived_state_offset = NULL;
    metadata = NULL;
    metadata_offset = NULL;

out:
    free_read_table_mem(cols, ragged_cols, properties);
    return ret;
}

/*************************
 * migration table
 *************************/

static void
tsk_migration_table_free_columns(tsk_migration_table_t *self)
{
    tsk_safe_free(self->left);
    tsk_safe_free(self->right);
    tsk_safe_free(self->node);
    tsk_safe_free(self->source);
    tsk_safe_free(self->dest);
    tsk_safe_free(self->time);
    tsk_safe_free(self->metadata);
    tsk_safe_free(self->metadata_offset);
}

int
tsk_migration_table_free(tsk_migration_table_t *self)
{
    tsk_migration_table_free_columns(self);
    tsk_safe_free(self->metadata_schema);
    return 0;
}

static int
tsk_migration_table_expand_main_columns(
    tsk_migration_table_t *self, tsk_size_t additional_rows)
{
    int ret = 0;
    tsk_size_t new_max_rows;

    ret = calculate_max_rows(self->num_rows, self->max_rows, self->max_rows_increment,
        additional_rows, &new_max_rows);
    if (ret != 0) {
        goto out;
    }
    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column((void **) &self->left, new_max_rows, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->right, new_max_rows, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->node, new_max_rows, sizeof(tsk_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->source, new_max_rows, sizeof(tsk_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->dest, new_max_rows, sizeof(tsk_id_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column((void **) &self->time, new_max_rows, sizeof(double));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column(
            (void **) &self->metadata_offset, new_max_rows + 1, sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }

        self->max_rows = new_max_rows;
    }
out:
    return ret;
}

static int
tsk_migration_table_expand_metadata(
    tsk_migration_table_t *self, tsk_size_t additional_length)
{
    return expand_ragged_column(self->metadata_length, additional_length,
        self->max_metadata_length_increment, &self->max_metadata_length,
        (void **) &self->metadata, sizeof(*self->metadata));
}

int
tsk_migration_table_set_max_rows_increment(
    tsk_migration_table_t *self, tsk_size_t max_rows_increment)
{
    self->max_rows_increment = max_rows_increment;
    return 0;
}

int
tsk_migration_table_set_max_metadata_length_increment(
    tsk_migration_table_t *self, tsk_size_t max_metadata_length_increment)
{
    self->max_metadata_length_increment = max_metadata_length_increment;
    return 0;
}

int
tsk_migration_table_init(tsk_migration_table_t *self, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;

    tsk_memset(self, 0, sizeof(tsk_migration_table_t));

    /* Allocate space for one row initially, ensuring we always have valid pointers
     * even if the table is empty */
    self->max_rows_increment = 1;
    self->max_metadata_length_increment = 1;
    ret = tsk_migration_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_migration_table_expand_metadata(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->metadata_offset[0] = 0;
    self->max_rows_increment = 0;
    self->max_metadata_length_increment = 0;
    tsk_migration_table_set_metadata_schema(self, NULL, 0);
out:
    return ret;
}

int
tsk_migration_table_append_columns(tsk_migration_table_t *self, tsk_size_t num_rows,
    const double *left, const double *right, const tsk_id_t *node,
    const tsk_id_t *source, const tsk_id_t *dest, const double *time,
    const char *metadata, const tsk_size_t *metadata_offset)
{
    int ret;
    tsk_size_t j, metadata_length;

    if (left == NULL || right == NULL || node == NULL || source == NULL || dest == NULL
        || time == NULL) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    if ((metadata == NULL) != (metadata_offset == NULL)) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }

    ret = tsk_migration_table_expand_main_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    tsk_memcpy(self->left + self->num_rows, left, num_rows * sizeof(double));
    tsk_memcpy(self->right + self->num_rows, right, num_rows * sizeof(double));
    tsk_memcpy(self->node + self->num_rows, node, num_rows * sizeof(tsk_id_t));
    tsk_memcpy(self->source + self->num_rows, source, num_rows * sizeof(tsk_id_t));
    tsk_memcpy(self->dest + self->num_rows, dest, num_rows * sizeof(tsk_id_t));
    tsk_memcpy(self->time + self->num_rows, time, num_rows * sizeof(double));
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
            self->metadata_offset[self->num_rows + j]
                = (tsk_size_t) self->metadata_length + metadata_offset[j];
        }
        metadata_length = metadata_offset[num_rows];
        ret = tsk_migration_table_expand_metadata(self, metadata_length);
        if (ret != 0) {
            goto out;
        }
        tsk_memcpy(self->metadata + self->metadata_length, metadata,
            metadata_length * sizeof(char));
        self->metadata_length += metadata_length;
    }

    self->num_rows += num_rows;
    self->metadata_offset[self->num_rows] = self->metadata_length;
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_migration_table_takeset_columns(tsk_migration_table_t *self, tsk_size_t num_rows,
    double *left, double *right, tsk_id_t *node, tsk_id_t *source, tsk_id_t *dest,
    double *time, char *metadata, tsk_size_t *metadata_offset)
{
    int ret = 0;

    if (left == NULL || right == NULL || node == NULL || source == NULL || dest == NULL
        || time == NULL) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }

    /* We need to check all the inputs before we start freeing or taking memory */
    ret = check_ragged_column(num_rows, metadata, metadata_offset);
    if (ret != 0) {
        goto out;
    }

    tsk_migration_table_free_columns(self);
    self->num_rows = num_rows;
    self->max_rows = num_rows;
    self->left = left;
    self->right = right;
    self->node = node;
    self->source = source;
    self->dest = dest;
    self->time = time;

    ret = takeset_ragged_column(num_rows, metadata, metadata_offset,
        (void *) &self->metadata, &self->metadata_offset, &self->metadata_length);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_migration_table_copy(
    const tsk_migration_table_t *self, tsk_migration_table_t *dest, tsk_flags_t options)
{
    int ret = 0;

    if (!(options & TSK_NO_INIT)) {
        ret = tsk_migration_table_init(dest, 0);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_migration_table_set_columns(dest, self->num_rows, self->left, self->right,
        self->node, self->source, self->dest, self->time, self->metadata,
        self->metadata_offset);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_migration_table_set_metadata_schema(
        dest, self->metadata_schema, self->metadata_schema_length);
out:
    return ret;
}

int
tsk_migration_table_set_columns(tsk_migration_table_t *self, tsk_size_t num_rows,
    const double *left, const double *right, const tsk_id_t *node,
    const tsk_id_t *source, const tsk_id_t *dest, const double *time,
    const char *metadata, const tsk_size_t *metadata_offset)
{
    int ret;

    ret = tsk_migration_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_migration_table_append_columns(self, num_rows, left, right, node, source,
        dest, time, metadata, metadata_offset);
out:
    return ret;
}

tsk_id_t
tsk_migration_table_add_row(tsk_migration_table_t *self, double left, double right,
    tsk_id_t node, tsk_id_t source, tsk_id_t dest, double time, const char *metadata,
    tsk_size_t metadata_length)
{
    tsk_id_t ret = 0;

    ret = tsk_migration_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_migration_table_expand_metadata(self, metadata_length);
    if (ret != 0) {
        goto out;
    }

    tsk_bug_assert(self->num_rows < self->max_rows);
    tsk_bug_assert(self->metadata_length + metadata_length <= self->max_metadata_length);
    tsk_memmove(self->metadata + self->metadata_length, metadata, metadata_length);
    self->left[self->num_rows] = left;
    self->right[self->num_rows] = right;
    self->node[self->num_rows] = node;
    self->source[self->num_rows] = source;
    self->dest[self->num_rows] = dest;
    self->time[self->num_rows] = time;
    self->metadata_offset[self->num_rows + 1] = self->metadata_length + metadata_length;
    self->metadata_length += metadata_length;

    ret = (tsk_id_t) self->num_rows;
    self->num_rows++;
out:
    return ret;
}

static int
tsk_migration_table_update_row_rewrite(tsk_migration_table_t *self, tsk_id_t index,
    double left, double right, tsk_id_t node, tsk_id_t source, tsk_id_t dest,
    double time, const char *metadata, tsk_size_t metadata_length)
{
    int ret = 0;
    tsk_id_t j, ret_id;
    tsk_migration_table_t copy;
    tsk_size_t num_rows;
    tsk_id_t *rows = NULL;

    ret = tsk_migration_table_copy(self, &copy, 0);
    if (ret != 0) {
        goto out;
    }
    rows = tsk_malloc(self->num_rows * sizeof(*rows));
    if (rows == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    ret = tsk_migration_table_truncate(self, (tsk_size_t) index);
    tsk_bug_assert(ret == 0);
    ret_id = tsk_migration_table_add_row(
        self, left, right, node, source, dest, time, metadata, metadata_length);
    if (ret_id < 0) {
        ret = (int) ret_id;
        goto out;
    }
    num_rows = 0;
    for (j = index + 1; j < (tsk_id_t) copy.num_rows; j++) {
        rows[num_rows] = j;
        num_rows++;
    }
    ret = tsk_migration_table_extend(self, &copy, num_rows, rows, 0);
    if (ret != 0) {
        goto out;
    }
out:
    tsk_migration_table_free(&copy);
    tsk_safe_free(rows);
    return ret;
}

int
tsk_migration_table_update_row(tsk_migration_table_t *self, tsk_id_t index, double left,
    double right, tsk_id_t node, tsk_id_t source, tsk_id_t dest, double time,
    const char *metadata, tsk_size_t metadata_length)
{
    int ret = 0;
    tsk_migration_t current_row;

    ret = tsk_migration_table_get_row(self, index, &current_row);
    if (ret != 0) {
        goto out;
    }
    if (current_row.metadata_length == metadata_length) {
        self->left[index] = left;
        self->right[index] = right;
        self->node[index] = node;
        self->source[index] = source;
        self->dest[index] = dest;
        self->time[index] = time;
        /* Note: important to use tsk_memmove here as we may be provided pointers
         * to the column memory as input via get_row */
        tsk_memmove(&self->metadata[self->metadata_offset[index]], metadata,
            metadata_length * sizeof(*metadata));
    } else {
        ret = tsk_migration_table_update_row_rewrite(self, index, left, right, node,
            source, dest, time, metadata, metadata_length);
        if (ret != 0) {
            goto out;
        }
    }
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
        ret = tsk_trace_error(TSK_ERR_BAD_TABLE_POSITION);
        goto out;
    }
    self->num_rows = num_rows;
    self->metadata_length = self->metadata_offset[num_rows];
out:
    return ret;
}

int
tsk_migration_table_extend(tsk_migration_table_t *self,
    const tsk_migration_table_t *other, tsk_size_t num_rows, const tsk_id_t *row_indexes,
    tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_id_t ret_id;
    tsk_size_t j;
    tsk_migration_t migration;

    if (self == other) {
        ret = tsk_trace_error(TSK_ERR_CANNOT_EXTEND_FROM_SELF);
        goto out;
    }

    /* We know how much to expand the non-ragged columns, so do it ahead of time */
    ret = tsk_migration_table_expand_main_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < num_rows; j++) {
        ret = tsk_migration_table_get_row(
            other, row_indexes == NULL ? (tsk_id_t) j : row_indexes[j], &migration);
        if (ret != 0) {
            goto out;
        }
        ret_id = tsk_migration_table_add_row(self, migration.left, migration.right,
            migration.node, migration.source, migration.dest, migration.time,
            migration.metadata, migration.metadata_length);
        if (ret_id < 0) {
            ret = (int) ret_id;
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

void
tsk_migration_table_print_state(const tsk_migration_table_t *self, FILE *out)
{
    int ret;

    fprintf(out, "\n" TABLE_SEP);
    fprintf(out, "migration_table: %p:\n", (const void *) self);
    fprintf(out, "num_rows = %lld\tmax= %lld\tincrement = %lld)\n",
        (long long) self->num_rows, (long long) self->max_rows,
        (long long) self->max_rows_increment);
    fprintf(out, "metadata_length = %lld\tmax= %lld\tincrement = %lld)\n",
        (long long) self->metadata_length, (long long) self->max_metadata_length,
        (long long) self->max_metadata_length_increment);
    fprintf(out, TABLE_SEP);
    ret = tsk_migration_table_dump_text(self, out);
    tsk_bug_assert(ret == 0);
}

static inline void
tsk_migration_table_get_row_unsafe(
    const tsk_migration_table_t *self, tsk_id_t index, tsk_migration_t *row)
{
    row->id = (tsk_id_t) index;
    row->left = self->left[index];
    row->right = self->right[index];
    row->node = self->node[index];
    row->source = self->source[index];
    row->dest = self->dest[index];
    row->time = self->time[index];
    row->metadata_length
        = self->metadata_offset[index + 1] - self->metadata_offset[index];
    row->metadata = self->metadata + self->metadata_offset[index];
}

int
tsk_migration_table_get_row(
    const tsk_migration_table_t *self, tsk_id_t index, tsk_migration_t *row)
{
    int ret = 0;

    if (index < 0 || index >= (tsk_id_t) self->num_rows) {
        ret = tsk_trace_error(TSK_ERR_MIGRATION_OUT_OF_BOUNDS);
        goto out;
    }
    tsk_migration_table_get_row_unsafe(self, index, row);
out:
    return ret;
}

int
tsk_migration_table_set_metadata_schema(tsk_migration_table_t *self,
    const char *metadata_schema, tsk_size_t metadata_schema_length)
{
    return replace_string(&self->metadata_schema, &self->metadata_schema_length,
        metadata_schema, metadata_schema_length);
}

int
tsk_migration_table_dump_text(const tsk_migration_table_t *self, FILE *out)
{
    tsk_size_t j;
    int ret = TSK_ERR_IO;
    tsk_size_t metadata_len;
    int err;

    err = write_metadata_schema_header(
        out, self->metadata_schema, self->metadata_schema_length);
    if (err < 0) {
        goto out;
    }
    err = fprintf(out, "left\tright\tnode\tsource\tdest\ttime\tmetadata\n");
    if (err < 0) {
        goto out;
    }
    for (j = 0; j < self->num_rows; j++) {
        metadata_len = self->metadata_offset[j + 1] - self->metadata_offset[j];
        err = fprintf(out, "%.3f\t%.3f\t%lld\t%lld\t%lld\t%f\t%.*s\n", self->left[j],
            self->right[j], (long long) self->node[j], (long long) self->source[j],
            (long long) self->dest[j], self->time[j], (int) metadata_len,
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
tsk_migration_table_equals(const tsk_migration_table_t *self,
    const tsk_migration_table_t *other, tsk_flags_t options)
{
    bool ret
        = self->num_rows == other->num_rows
          && tsk_memcmp(self->left, other->left, self->num_rows * sizeof(double)) == 0
          && tsk_memcmp(self->right, other->right, self->num_rows * sizeof(double)) == 0
          && tsk_memcmp(self->node, other->node, self->num_rows * sizeof(tsk_id_t)) == 0
          && tsk_memcmp(self->source, other->source, self->num_rows * sizeof(tsk_id_t))
                 == 0
          && tsk_memcmp(self->dest, other->dest, self->num_rows * sizeof(tsk_id_t)) == 0
          && tsk_memcmp(self->time, other->time, self->num_rows * sizeof(double)) == 0;
    if (!(options & TSK_CMP_IGNORE_METADATA)) {
        ret = ret && self->metadata_length == other->metadata_length
              && self->metadata_schema_length == other->metadata_schema_length
              && tsk_memcmp(self->metadata_offset, other->metadata_offset,
                     (self->num_rows + 1) * sizeof(tsk_size_t))
                     == 0
              && tsk_memcmp(self->metadata, other->metadata,
                     self->metadata_length * sizeof(char))
                     == 0
              && tsk_memcmp(self->metadata_schema, other->metadata_schema,
                     self->metadata_schema_length * sizeof(char))
                     == 0;
    }
    return ret;
}

int
tsk_migration_table_keep_rows(tsk_migration_table_t *self, const tsk_bool_t *keep,
    tsk_flags_t TSK_UNUSED(options), tsk_id_t *id_map)
{
    int ret = 0;
    tsk_size_t remaining_rows;

    if (id_map != NULL) {
        keep_mask_to_id_map(self->num_rows, keep, id_map);
    }

    remaining_rows = subset_double_column(self->left, self->num_rows, keep);
    subset_double_column(self->right, self->num_rows, keep);
    subset_id_column(self->node, self->num_rows, keep);
    subset_id_column(self->source, self->num_rows, keep);
    subset_id_column(self->dest, self->num_rows, keep);
    subset_double_column(self->time, self->num_rows, keep);
    if (self->metadata_length > 0) {
        self->metadata_length = subset_ragged_char_column(
            self->metadata, self->metadata_offset, self->num_rows, keep);
    }
    self->num_rows = remaining_rows;
    return ret;
}

static int
tsk_migration_table_dump(
    const tsk_migration_table_t *self, kastore_t *store, tsk_flags_t options)
{
    const write_table_col_t cols[] = {
        { "migrations/left", (void *) self->left, self->num_rows, KAS_FLOAT64 },
        { "migrations/right", (void *) self->right, self->num_rows, KAS_FLOAT64 },
        { "migrations/node", (void *) self->node, self->num_rows, TSK_ID_STORAGE_TYPE },
        { "migrations/source", (void *) self->source, self->num_rows,
            TSK_ID_STORAGE_TYPE },
        { "migrations/dest", (void *) self->dest, self->num_rows, TSK_ID_STORAGE_TYPE },
        { "migrations/time", (void *) self->time, self->num_rows, KAS_FLOAT64 },
        { "migrations/metadata_schema", (void *) self->metadata_schema,
            self->metadata_schema_length, KAS_UINT8 },
        { .name = NULL },
    };
    const write_table_ragged_col_t ragged_cols[] = {
        { "migrations/metadata", (void *) self->metadata, self->metadata_length,
            KAS_UINT8, self->metadata_offset, self->num_rows },
        { .name = NULL },
    };

    return write_table(store, cols, ragged_cols, options);
}

static int
tsk_migration_table_load(tsk_migration_table_t *self, kastore_t *store)
{
    int ret = 0;
    tsk_id_t *source = NULL;
    tsk_id_t *dest = NULL;
    tsk_id_t *node = NULL;
    double *left = NULL;
    double *right = NULL;
    double *time = NULL;
    char *metadata = NULL;
    tsk_size_t *metadata_offset = NULL;
    char *metadata_schema = NULL;
    tsk_size_t num_rows, metadata_length, metadata_schema_length;

    read_table_col_t cols[] = {
        { "migrations/left", (void **) &left, KAS_FLOAT64, 0 },
        { "migrations/right", (void **) &right, KAS_FLOAT64, 0 },
        { "migrations/node", (void **) &node, TSK_ID_STORAGE_TYPE, 0 },
        { "migrations/source", (void **) &source, TSK_ID_STORAGE_TYPE, 0 },
        { "migrations/dest", (void **) &dest, TSK_ID_STORAGE_TYPE, 0 },
        { "migrations/time", (void **) &time, KAS_FLOAT64, 0 },
        { .name = NULL },
    };
    read_table_ragged_col_t ragged_cols[] = {
        { "migrations/metadata", (void **) &metadata, &metadata_length, KAS_UINT8,
            &metadata_offset, TSK_COL_OPTIONAL },
        { .name = NULL },
    };
    read_table_property_t properties[] = {
        { "migrations/metadata_schema", (void **) &metadata_schema,
            &metadata_schema_length, KAS_UINT8, TSK_COL_OPTIONAL },
        { .name = NULL },
    };

    ret = read_table(store, &num_rows, cols, ragged_cols, properties, 0);
    if (ret != 0) {
        goto out;
    }
    if (metadata_schema != NULL) {
        ret = tsk_migration_table_set_metadata_schema(
            self, metadata_schema, metadata_schema_length);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_migration_table_takeset_columns(self, num_rows, left, right, node, source,
        dest, time, metadata, metadata_offset);
    if (ret != 0) {
        goto out;
    }
    left = NULL;
    right = NULL;
    node = NULL;
    source = NULL;
    dest = NULL;
    time = NULL;
    metadata = NULL;
    metadata_offset = NULL;

out:
    free_read_table_mem(cols, ragged_cols, properties);
    return ret;
}

/*************************
 * population table
 *************************/

static void
tsk_population_table_free_columns(tsk_population_table_t *self)
{
    tsk_safe_free(self->metadata);
    tsk_safe_free(self->metadata_offset);
}

int
tsk_population_table_free(tsk_population_table_t *self)
{
    tsk_population_table_free_columns(self);
    tsk_safe_free(self->metadata_schema);
    return 0;
}

static int
tsk_population_table_expand_main_columns(
    tsk_population_table_t *self, tsk_size_t additional_rows)
{
    int ret = 0;
    tsk_size_t new_max_rows;

    ret = calculate_max_rows(self->num_rows, self->max_rows, self->max_rows_increment,
        additional_rows, &new_max_rows);
    if (ret != 0) {
        goto out;
    }
    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column(
            (void **) &self->metadata_offset, new_max_rows + 1, sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        self->max_rows = new_max_rows;
    }
out:
    return ret;
}

static int
tsk_population_table_expand_metadata(
    tsk_population_table_t *self, tsk_size_t additional_length)
{
    return expand_ragged_column(self->metadata_length, additional_length,
        self->max_metadata_length_increment, &self->max_metadata_length,
        (void **) &self->metadata, sizeof(*self->metadata));
}

int
tsk_population_table_set_max_rows_increment(
    tsk_population_table_t *self, tsk_size_t max_rows_increment)
{
    self->max_rows_increment = max_rows_increment;
    return 0;
}

int
tsk_population_table_set_max_metadata_length_increment(
    tsk_population_table_t *self, tsk_size_t max_metadata_length_increment)
{
    self->max_metadata_length_increment = max_metadata_length_increment;
    return 0;
}

int
tsk_population_table_init(tsk_population_table_t *self, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;

    tsk_memset(self, 0, sizeof(tsk_population_table_t));
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
    self->max_rows_increment = 0;
    self->max_metadata_length_increment = 0;
    tsk_population_table_set_metadata_schema(self, NULL, 0);
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_population_table_copy(const tsk_population_table_t *self,
    tsk_population_table_t *dest, tsk_flags_t options)
{
    int ret = 0;

    if (!(options & TSK_NO_INIT)) {
        ret = tsk_population_table_init(dest, 0);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_population_table_set_columns(
        dest, self->num_rows, self->metadata, self->metadata_offset);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_population_table_set_metadata_schema(
        dest, self->metadata_schema, self->metadata_schema_length);
out:
    return ret;
}

int
tsk_population_table_set_columns(tsk_population_table_t *self, tsk_size_t num_rows,
    const char *metadata, const tsk_size_t *metadata_offset)
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
    const char *metadata, const tsk_size_t *metadata_offset)
{
    int ret;
    tsk_size_t j, metadata_length;

    if (metadata == NULL || metadata_offset == NULL) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
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
        self->metadata_offset[self->num_rows + j]
            = self->metadata_length + metadata_offset[j];
    }
    metadata_length = metadata_offset[num_rows];
    ret = tsk_population_table_expand_metadata(self, metadata_length);
    if (ret != 0) {
        goto out;
    }
    tsk_memcpy(self->metadata + self->metadata_length, metadata,
        metadata_length * sizeof(char));
    self->metadata_length += metadata_length;

    self->num_rows += num_rows;
    self->metadata_offset[self->num_rows] = self->metadata_length;
out:
    return ret;
}

int
tsk_population_table_takeset_columns(tsk_population_table_t *self, tsk_size_t num_rows,
    char *metadata, tsk_size_t *metadata_offset)
{
    int ret = 0;

    /* We need to check all the inputs before we start freeing or taking memory */
    if (metadata == NULL || metadata_offset == NULL) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    ret = check_ragged_column(num_rows, metadata, metadata_offset);
    if (ret != 0) {
        goto out;
    }

    tsk_population_table_free_columns(self);
    self->num_rows = num_rows;
    self->max_rows = num_rows;

    ret = takeset_ragged_column(num_rows, metadata, metadata_offset,
        (void *) &self->metadata, &self->metadata_offset, &self->metadata_length);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

static tsk_id_t
tsk_population_table_add_row_internal(
    tsk_population_table_t *self, const char *metadata, tsk_size_t metadata_length)
{
    tsk_id_t ret = 0;

    tsk_bug_assert(self->num_rows < self->max_rows);
    tsk_bug_assert(self->metadata_length + metadata_length <= self->max_metadata_length);
    tsk_memmove(self->metadata + self->metadata_length, metadata, metadata_length);
    self->metadata_offset[self->num_rows + 1] = self->metadata_length + metadata_length;
    self->metadata_length += metadata_length;
    ret = (tsk_id_t) self->num_rows;
    self->num_rows++;
    return ret;
}

tsk_id_t
tsk_population_table_add_row(
    tsk_population_table_t *self, const char *metadata, tsk_size_t metadata_length)
{
    tsk_id_t ret = 0;

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

static int
tsk_population_table_update_row_rewrite(tsk_population_table_t *self, tsk_id_t index,
    const char *metadata, tsk_size_t metadata_length)
{
    int ret = 0;
    tsk_id_t j, ret_id;
    tsk_population_table_t copy;
    tsk_size_t num_rows;
    tsk_id_t *rows = NULL;

    ret = tsk_population_table_copy(self, &copy, 0);
    if (ret != 0) {
        goto out;
    }
    rows = tsk_malloc(self->num_rows * sizeof(*rows));
    if (rows == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    ret = tsk_population_table_truncate(self, (tsk_size_t) index);
    tsk_bug_assert(ret == 0);
    ret_id = tsk_population_table_add_row(self, metadata, metadata_length);
    if (ret_id < 0) {
        ret = (int) ret_id;
        goto out;
    }
    num_rows = 0;
    for (j = index + 1; j < (tsk_id_t) copy.num_rows; j++) {
        rows[num_rows] = j;
        num_rows++;
    }
    ret = tsk_population_table_extend(self, &copy, num_rows, rows, 0);
    if (ret != 0) {
        goto out;
    }
out:
    tsk_population_table_free(&copy);
    tsk_safe_free(rows);
    return ret;
}

int
tsk_population_table_update_row(tsk_population_table_t *self, tsk_id_t index,
    const char *metadata, tsk_size_t metadata_length)
{
    int ret = 0;
    tsk_population_t current_row;

    ret = tsk_population_table_get_row(self, index, &current_row);
    if (ret != 0) {
        goto out;
    }
    if (current_row.metadata_length == metadata_length) {
        /* Note: important to use tsk_memmove here as we may be provided pointers
         * to the column memory as input via get_row */
        tsk_memmove(&self->metadata[self->metadata_offset[index]], metadata,
            metadata_length * sizeof(*metadata));
    } else {
        ret = tsk_population_table_update_row_rewrite(
            self, index, metadata, metadata_length);
        if (ret != 0) {
            goto out;
        }
    }
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
        ret = tsk_trace_error(TSK_ERR_BAD_TABLE_POSITION);
        goto out;
    }
    self->num_rows = num_rows;
    self->metadata_length = self->metadata_offset[num_rows];
out:
    return ret;
}

int
tsk_population_table_extend(tsk_population_table_t *self,
    const tsk_population_table_t *other, tsk_size_t num_rows,
    const tsk_id_t *row_indexes, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_id_t ret_id;
    tsk_size_t j;
    tsk_population_t population;

    if (self == other) {
        ret = tsk_trace_error(TSK_ERR_CANNOT_EXTEND_FROM_SELF);
        goto out;
    }

    /* We know how much to expand the non-ragged columns, so do it ahead of time */
    ret = tsk_population_table_expand_main_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < num_rows; j++) {
        ret = tsk_population_table_get_row(
            other, row_indexes == NULL ? (tsk_id_t) j : row_indexes[j], &population);
        if (ret != 0) {
            goto out;
        }
        ret_id = tsk_population_table_add_row(
            self, population.metadata, population.metadata_length);
        if (ret_id < 0) {
            ret = (int) ret_id;
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

void
tsk_population_table_print_state(const tsk_population_table_t *self, FILE *out)
{
    tsk_size_t j, k;

    fprintf(out, "\n" TABLE_SEP);
    fprintf(out, "population_table: %p:\n", (const void *) self);
    fprintf(out, "num_rows          = %lld\tmax= %lld\tincrement = %lld)\n",
        (long long) self->num_rows, (long long) self->max_rows,
        (long long) self->max_rows_increment);
    fprintf(out, "metadata_length  = %lld\tmax= %lld\tincrement = %lld)\n",
        (long long) self->metadata_length, (long long) self->max_metadata_length,
        (long long) self->max_metadata_length_increment);
    fprintf(out, TABLE_SEP);
    write_metadata_schema_header(
        out, self->metadata_schema, self->metadata_schema_length);
    fprintf(out, "index\tmetadata_offset\tmetadata\n");
    for (j = 0; j < self->num_rows; j++) {
        fprintf(
            out, "%lld\t%lld\t", (long long) j, (long long) self->metadata_offset[j]);
        for (k = self->metadata_offset[j]; k < self->metadata_offset[j + 1]; k++) {
            fprintf(out, "%c", self->metadata[k]);
        }
        fprintf(out, "\n");
    }
    tsk_bug_assert(self->metadata_offset[0] == 0);
    tsk_bug_assert(self->metadata_offset[self->num_rows] == self->metadata_length);
}

static inline void
tsk_population_table_get_row_unsafe(
    const tsk_population_table_t *self, tsk_id_t index, tsk_population_t *row)
{
    row->id = (tsk_id_t) index;
    row->metadata_length
        = self->metadata_offset[index + 1] - self->metadata_offset[index];
    row->metadata = self->metadata + self->metadata_offset[index];
}

int
tsk_population_table_get_row(
    const tsk_population_table_t *self, tsk_id_t index, tsk_population_t *row)
{
    int ret = 0;

    if (index < 0 || index >= (tsk_id_t) self->num_rows) {
        ret = tsk_trace_error(TSK_ERR_POPULATION_OUT_OF_BOUNDS);
        goto out;
    }
    tsk_population_table_get_row_unsafe(self, index, row);
out:
    return ret;
}

int
tsk_population_table_set_metadata_schema(tsk_population_table_t *self,
    const char *metadata_schema, tsk_size_t metadata_schema_length)
{
    return replace_string(&self->metadata_schema, &self->metadata_schema_length,
        metadata_schema, metadata_schema_length);
}

int
tsk_population_table_dump_text(const tsk_population_table_t *self, FILE *out)
{
    int ret = TSK_ERR_IO;
    int err;
    tsk_size_t j;
    tsk_size_t metadata_len;

    err = write_metadata_schema_header(
        out, self->metadata_schema, self->metadata_schema_length);
    if (err < 0) {
        goto out;
    }
    err = fprintf(out, "metadata\n");
    if (err < 0) {
        goto out;
    }
    for (j = 0; j < self->num_rows; j++) {
        metadata_len = self->metadata_offset[j + 1] - self->metadata_offset[j];
        err = fprintf(out, "%.*s\n", (int) metadata_len,
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
tsk_population_table_equals(const tsk_population_table_t *self,
    const tsk_population_table_t *other, tsk_flags_t options)
{
    /* Since we only have the metadata column in the table currently, equality
     * reduces to comparing the number of rows if we disable metadata comparison.
     */
    bool ret = self->num_rows == other->num_rows;
    if (!(options & TSK_CMP_IGNORE_METADATA)) {
        ret = ret && self->metadata_length == other->metadata_length
              && self->metadata_schema_length == other->metadata_schema_length
              && tsk_memcmp(self->metadata_offset, other->metadata_offset,
                     (self->num_rows + 1) * sizeof(tsk_size_t))
                     == 0
              && tsk_memcmp(self->metadata, other->metadata,
                     self->metadata_length * sizeof(char))
                     == 0
              && tsk_memcmp(self->metadata_schema, other->metadata_schema,
                     self->metadata_schema_length * sizeof(char))
                     == 0;
    }
    return ret;
}

int
tsk_population_table_keep_rows(tsk_population_table_t *self, const tsk_bool_t *keep,
    tsk_flags_t TSK_UNUSED(options), tsk_id_t *id_map)
{
    int ret = 0;

    if (id_map != NULL) {
        keep_mask_to_id_map(self->num_rows, keep, id_map);
    }

    if (self->metadata_length > 0) {
        self->metadata_length = subset_ragged_char_column(
            self->metadata, self->metadata_offset, self->num_rows, keep);
    }
    self->num_rows = count_true(self->num_rows, keep);
    return ret;
}

static int
tsk_population_table_dump(
    const tsk_population_table_t *self, kastore_t *store, tsk_flags_t options)
{
    const write_table_col_t cols[] = {
        { "populations/metadata_schema", (void *) self->metadata_schema,
            self->metadata_schema_length, KAS_UINT8 },
        { .name = NULL },
    };
    const write_table_ragged_col_t ragged_cols[] = {
        { "populations/metadata", (void *) self->metadata, self->metadata_length,
            KAS_UINT8, self->metadata_offset, self->num_rows },
        { .name = NULL },
    };

    return write_table(store, cols, ragged_cols, options);
}

static int
tsk_population_table_load(tsk_population_table_t *self, kastore_t *store)
{
    int ret = 0;
    char *metadata = NULL;
    tsk_size_t *metadata_offset = NULL;
    char *metadata_schema = NULL;
    tsk_size_t num_rows, metadata_length, metadata_schema_length;

    read_table_ragged_col_t ragged_cols[] = {
        { "populations/metadata", (void **) &metadata, &metadata_length, KAS_UINT8,
            &metadata_offset, 0 },
        { .name = NULL },
    };
    read_table_property_t properties[] = {
        { "populations/metadata_schema", (void **) &metadata_schema,
            &metadata_schema_length, KAS_UINT8, TSK_COL_OPTIONAL },
        { .name = NULL },
    };

    ret = read_table(store, &num_rows, NULL, ragged_cols, properties, 0);
    if (ret != 0) {
        goto out;
    }
    if (metadata_schema != NULL) {
        ret = tsk_population_table_set_metadata_schema(
            self, metadata_schema, metadata_schema_length);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_population_table_takeset_columns(
        self, num_rows, metadata, metadata_offset);
    if (ret != 0) {
        goto out;
    }
    metadata = NULL;
    metadata_offset = NULL;

out:
    free_read_table_mem(NULL, ragged_cols, properties);
    return ret;
}

/*************************
 * provenance table
 *************************/

static void
tsk_provenance_table_free_columns(tsk_provenance_table_t *self)
{
    tsk_safe_free(self->timestamp);
    tsk_safe_free(self->timestamp_offset);
    tsk_safe_free(self->record);
    tsk_safe_free(self->record_offset);
}

int
tsk_provenance_table_free(tsk_provenance_table_t *self)
{
    tsk_provenance_table_free_columns(self);
    return 0;
}

static int
tsk_provenance_table_expand_main_columns(
    tsk_provenance_table_t *self, tsk_size_t additional_rows)
{
    int ret = 0;
    tsk_size_t new_max_rows;

    ret = calculate_max_rows(self->num_rows, self->max_rows, self->max_rows_increment,
        additional_rows, &new_max_rows);
    if (ret != 0) {
        goto out;
    }
    if ((self->num_rows + additional_rows) > self->max_rows) {
        ret = expand_column(
            (void **) &self->timestamp_offset, new_max_rows + 1, sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        ret = expand_column(
            (void **) &self->record_offset, new_max_rows + 1, sizeof(tsk_size_t));
        if (ret != 0) {
            goto out;
        }
        self->max_rows = new_max_rows;
    }
out:
    return ret;
}

static int
tsk_provenance_table_expand_timestamp(
    tsk_provenance_table_t *self, tsk_size_t additional_length)
{
    return expand_ragged_column(self->timestamp_length, additional_length,
        self->max_timestamp_length_increment, &self->max_timestamp_length,
        (void **) &self->timestamp, sizeof(*self->timestamp));
}

static int
tsk_provenance_table_expand_record(
    tsk_provenance_table_t *self, tsk_size_t additional_length)
{
    return expand_ragged_column(self->record_length, additional_length,
        self->max_record_length_increment, &self->max_record_length,
        (void **) &self->record, sizeof(*self->record));
}

int
tsk_provenance_table_set_max_rows_increment(
    tsk_provenance_table_t *self, tsk_size_t max_rows_increment)
{
    self->max_rows_increment = max_rows_increment;
    return 0;
}

int
tsk_provenance_table_set_max_timestamp_length_increment(
    tsk_provenance_table_t *self, tsk_size_t max_timestamp_length_increment)
{
    self->max_timestamp_length_increment = max_timestamp_length_increment;
    return 0;
}

int
tsk_provenance_table_set_max_record_length_increment(
    tsk_provenance_table_t *self, tsk_size_t max_record_length_increment)
{
    self->max_record_length_increment = max_record_length_increment;
    return 0;
}

int
tsk_provenance_table_init(tsk_provenance_table_t *self, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;

    tsk_memset(self, 0, sizeof(tsk_provenance_table_t));
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
    ret = tsk_provenance_table_expand_record(self, 1);
    if (ret != 0) {
        goto out;
    }
    self->record_offset[0] = 0;
    self->max_rows_increment = 0;
    self->max_timestamp_length_increment = 0;
    self->max_record_length_increment = 0;
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_provenance_table_copy(const tsk_provenance_table_t *self,
    tsk_provenance_table_t *dest, tsk_flags_t options)
{
    int ret = 0;

    if (!(options & TSK_NO_INIT)) {
        ret = tsk_provenance_table_init(dest, 0);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_provenance_table_set_columns(dest, self->num_rows, self->timestamp,
        self->timestamp_offset, self->record, self->record_offset);
out:
    return ret;
}

int
tsk_provenance_table_set_columns(tsk_provenance_table_t *self, tsk_size_t num_rows,
    const char *timestamp, const tsk_size_t *timestamp_offset, const char *record,
    const tsk_size_t *record_offset)
{
    int ret;

    ret = tsk_provenance_table_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_provenance_table_append_columns(
        self, num_rows, timestamp, timestamp_offset, record, record_offset);
out:
    return ret;
}

int
tsk_provenance_table_append_columns(tsk_provenance_table_t *self, tsk_size_t num_rows,
    const char *timestamp, const tsk_size_t *timestamp_offset, const char *record,
    const tsk_size_t *record_offset)
{
    int ret;
    tsk_size_t j, timestamp_length, record_length;

    if (timestamp == NULL || timestamp_offset == NULL || record == NULL
        || record_offset == NULL) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
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
        self->timestamp_offset[self->num_rows + j]
            = self->timestamp_length + timestamp_offset[j];
    }
    timestamp_length = timestamp_offset[num_rows];
    ret = tsk_provenance_table_expand_timestamp(self, timestamp_length);
    if (ret != 0) {
        goto out;
    }
    tsk_memcpy(self->timestamp + self->timestamp_length, timestamp,
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
    ret = tsk_provenance_table_expand_record(self, record_length);
    if (ret != 0) {
        goto out;
    }
    tsk_memcpy(self->record + self->record_length, record, record_length * sizeof(char));
    self->record_length += record_length;

    self->num_rows += num_rows;
    self->timestamp_offset[self->num_rows] = self->timestamp_length;
    self->record_offset[self->num_rows] = self->record_length;
out:
    return ret;
}

int
tsk_provenance_table_takeset_columns(tsk_provenance_table_t *self, tsk_size_t num_rows,
    char *timestamp, tsk_size_t *timestamp_offset, char *record,
    tsk_size_t *record_offset)
{
    int ret = 0;

    /* We need to check all the inputs before we start freeing or taking memory */
    if (timestamp == NULL || timestamp_offset == NULL || record == NULL
        || record_offset == NULL) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    ret = check_ragged_column(num_rows, timestamp, timestamp_offset);
    if (ret != 0) {
        goto out;
    }
    ret = check_ragged_column(num_rows, record, record_offset);
    if (ret != 0) {
        goto out;
    }

    tsk_provenance_table_free_columns(self);
    self->num_rows = num_rows;
    self->max_rows = num_rows;

    ret = takeset_ragged_column(num_rows, timestamp, timestamp_offset,
        (void *) &self->timestamp, &self->timestamp_offset, &self->timestamp_length);
    if (ret != 0) {
        goto out;
    }
    ret = takeset_ragged_column(num_rows, record, record_offset, (void *) &self->record,
        &self->record_offset, &self->record_length);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

static tsk_id_t
tsk_provenance_table_add_row_internal(tsk_provenance_table_t *self,
    const char *timestamp, tsk_size_t timestamp_length, const char *record,
    tsk_size_t record_length)
{
    tsk_id_t ret = 0;

    tsk_bug_assert(self->num_rows < self->max_rows);
    tsk_bug_assert(
        self->timestamp_length + timestamp_length <= self->max_timestamp_length);
    tsk_memmove(self->timestamp + self->timestamp_length, timestamp, timestamp_length);
    self->timestamp_offset[self->num_rows + 1]
        = self->timestamp_length + timestamp_length;
    self->timestamp_length += timestamp_length;
    tsk_bug_assert(self->record_length + record_length <= self->max_record_length);
    tsk_memmove(self->record + self->record_length, record, record_length);
    self->record_offset[self->num_rows + 1] = self->record_length + record_length;
    self->record_length += record_length;
    ret = (tsk_id_t) self->num_rows;
    self->num_rows++;
    return ret;
}

tsk_id_t
tsk_provenance_table_add_row(tsk_provenance_table_t *self, const char *timestamp,
    tsk_size_t timestamp_length, const char *record, tsk_size_t record_length)
{
    tsk_id_t ret = 0;

    ret = tsk_provenance_table_expand_main_columns(self, 1);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_provenance_table_expand_timestamp(self, timestamp_length);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_provenance_table_expand_record(self, record_length);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_provenance_table_add_row_internal(
        self, timestamp, timestamp_length, record, record_length);
out:
    return ret;
}

static int
tsk_provenance_table_update_row_rewrite(tsk_provenance_table_t *self, tsk_id_t index,
    const char *timestamp, tsk_size_t timestamp_length, const char *record,
    tsk_size_t record_length)
{
    int ret = 0;
    tsk_id_t j, ret_id;
    tsk_provenance_table_t copy;
    tsk_size_t num_rows;
    tsk_id_t *rows = NULL;

    ret = tsk_provenance_table_copy(self, &copy, 0);
    if (ret != 0) {
        goto out;
    }
    rows = tsk_malloc(self->num_rows * sizeof(*rows));
    if (rows == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    ret = tsk_provenance_table_truncate(self, (tsk_size_t) index);
    tsk_bug_assert(ret == 0);
    ret_id = tsk_provenance_table_add_row(
        self, timestamp, timestamp_length, record, record_length);
    if (ret_id < 0) {
        ret = (int) ret_id;
        goto out;
    }
    num_rows = 0;
    for (j = index + 1; j < (tsk_id_t) copy.num_rows; j++) {
        rows[num_rows] = j;
        num_rows++;
    }
    ret = tsk_provenance_table_extend(self, &copy, num_rows, rows, 0);
    if (ret != 0) {
        goto out;
    }
out:
    tsk_provenance_table_free(&copy);
    tsk_safe_free(rows);
    return ret;
}

int
tsk_provenance_table_update_row(tsk_provenance_table_t *self, tsk_id_t index,
    const char *timestamp, tsk_size_t timestamp_length, const char *record,
    tsk_size_t record_length)
{
    int ret = 0;
    tsk_provenance_t current_row;

    ret = tsk_provenance_table_get_row(self, index, &current_row);
    if (ret != 0) {
        goto out;
    }
    if (current_row.timestamp_length == timestamp_length
        && current_row.record_length == record_length) {
        /* Note: important to use tsk_memmove here as we may be provided pointers
         * to the column memory as input via get_row */
        tsk_memmove(&self->timestamp[self->timestamp_offset[index]], timestamp,
            timestamp_length * sizeof(*timestamp));
        tsk_memmove(&self->record[self->record_offset[index]], record,
            record_length * sizeof(*record));
    } else {
        ret = tsk_provenance_table_update_row_rewrite(
            self, index, timestamp, timestamp_length, record, record_length);
        if (ret != 0) {
            goto out;
        }
    }
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
        ret = tsk_trace_error(TSK_ERR_BAD_TABLE_POSITION);
        goto out;
    }
    self->num_rows = num_rows;
    self->timestamp_length = self->timestamp_offset[num_rows];
    self->record_length = self->record_offset[num_rows];
out:
    return ret;
}

int
tsk_provenance_table_extend(tsk_provenance_table_t *self,
    const tsk_provenance_table_t *other, tsk_size_t num_rows,
    const tsk_id_t *row_indexes, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_id_t ret_id;
    tsk_size_t j;
    tsk_provenance_t provenance;

    if (self == other) {
        ret = tsk_trace_error(TSK_ERR_CANNOT_EXTEND_FROM_SELF);
        goto out;
    }

    /* We know how much to expand the non-ragged columns, so do it ahead of time */
    ret = tsk_provenance_table_expand_main_columns(self, num_rows);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < num_rows; j++) {
        ret = tsk_provenance_table_get_row(
            other, row_indexes == NULL ? (tsk_id_t) j : row_indexes[j], &provenance);
        if (ret != 0) {
            goto out;
        }
        ret_id = tsk_provenance_table_add_row(self, provenance.timestamp,
            provenance.timestamp_length, provenance.record, provenance.record_length);
        if (ret_id < 0) {
            ret = (int) ret_id;
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

void
tsk_provenance_table_print_state(const tsk_provenance_table_t *self, FILE *out)
{
    tsk_size_t j, k;

    fprintf(out, "\n" TABLE_SEP);
    fprintf(out, "provenance_table: %p:\n", (const void *) self);
    fprintf(out, "num_rows          = %lld\tmax= %lld\tincrement = %lld)\n",
        (long long) self->num_rows, (long long) self->max_rows,
        (long long) self->max_rows_increment);
    fprintf(out, "timestamp_length  = %lld\tmax= %lld\tincrement = %lld)\n",
        (long long) self->timestamp_length, (long long) self->max_timestamp_length,
        (long long) self->max_timestamp_length_increment);
    fprintf(out, "record_length = %lld\tmax= %lld\tincrement = %lld)\n",
        (long long) self->record_length, (long long) self->max_record_length,
        (long long) self->max_record_length_increment);
    fprintf(out, TABLE_SEP);
    fprintf(out, "index\ttimestamp_offset\ttimestamp\trecord_offset\tprovenance\n");
    for (j = 0; j < self->num_rows; j++) {
        fprintf(
            out, "%lld\t%lld\t", (long long) j, (long long) self->timestamp_offset[j]);
        for (k = self->timestamp_offset[j]; k < self->timestamp_offset[j + 1]; k++) {
            fprintf(out, "%c", self->timestamp[k]);
        }
        fprintf(out, "\t%lld\t", (long long) self->record_offset[j]);
        for (k = self->record_offset[j]; k < self->record_offset[j + 1]; k++) {
            fprintf(out, "%c", self->record[k]);
        }
        fprintf(out, "\n");
    }
    tsk_bug_assert(self->timestamp_offset[0] == 0);
    tsk_bug_assert(self->timestamp_offset[self->num_rows] == self->timestamp_length);
    tsk_bug_assert(self->record_offset[0] == 0);
    tsk_bug_assert(self->record_offset[self->num_rows] == self->record_length);
}

static inline void
tsk_provenance_table_get_row_unsafe(
    const tsk_provenance_table_t *self, tsk_id_t index, tsk_provenance_t *row)
{
    row->id = (tsk_id_t) index;
    row->timestamp_length
        = self->timestamp_offset[index + 1] - self->timestamp_offset[index];
    row->timestamp = self->timestamp + self->timestamp_offset[index];
    row->record_length = self->record_offset[index + 1] - self->record_offset[index];
    row->record = self->record + self->record_offset[index];
}

int
tsk_provenance_table_get_row(
    const tsk_provenance_table_t *self, tsk_id_t index, tsk_provenance_t *row)
{
    int ret = 0;

    if (index < 0 || index >= (tsk_id_t) self->num_rows) {
        ret = tsk_trace_error(TSK_ERR_PROVENANCE_OUT_OF_BOUNDS);
        goto out;
    }
    tsk_provenance_table_get_row_unsafe(self, index, row);
out:
    return ret;
}

int
tsk_provenance_table_dump_text(const tsk_provenance_table_t *self, FILE *out)
{
    int ret = TSK_ERR_IO;
    int err;
    tsk_size_t j, timestamp_len, record_len;

    err = fprintf(out, "record\ttimestamp\n");
    if (err < 0) {
        goto out;
    }
    for (j = 0; j < self->num_rows; j++) {
        record_len = self->record_offset[j + 1] - self->record_offset[j];
        timestamp_len = self->timestamp_offset[j + 1] - self->timestamp_offset[j];
        err = fprintf(out, "%.*s\t%.*s\n", (int) record_len,
            self->record + self->record_offset[j], (int) timestamp_len,
            self->timestamp + self->timestamp_offset[j]);
        if (err < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

bool
tsk_provenance_table_equals(const tsk_provenance_table_t *self,
    const tsk_provenance_table_t *other, tsk_flags_t options)
{
    bool ret
        = self->num_rows == other->num_rows
          && self->record_length == other->record_length
          && tsk_memcmp(self->record_offset, other->record_offset,
                 (self->num_rows + 1) * sizeof(tsk_size_t))
                 == 0
          && tsk_memcmp(self->record, other->record, self->record_length * sizeof(char))
                 == 0;
    if (!(options & TSK_CMP_IGNORE_TIMESTAMPS)) {
        ret = ret && self->timestamp_length == other->timestamp_length
              && tsk_memcmp(self->timestamp_offset, other->timestamp_offset,
                     (self->num_rows + 1) * sizeof(tsk_size_t))
                     == 0
              && tsk_memcmp(self->timestamp, other->timestamp,
                     self->timestamp_length * sizeof(char))
                     == 0;
    }
    return ret;
}

int
tsk_provenance_table_keep_rows(tsk_provenance_table_t *self, const tsk_bool_t *keep,
    tsk_flags_t TSK_UNUSED(options), tsk_id_t *id_map)
{
    int ret = 0;

    if (id_map != NULL) {
        keep_mask_to_id_map(self->num_rows, keep, id_map);
    }
    self->timestamp_length = subset_ragged_char_column(
        self->timestamp, self->timestamp_offset, self->num_rows, keep);
    self->record_length = subset_ragged_char_column(
        self->record, self->record_offset, self->num_rows, keep);
    self->num_rows = count_true(self->num_rows, keep);

    return ret;
}

static int
tsk_provenance_table_dump(
    const tsk_provenance_table_t *self, kastore_t *store, tsk_flags_t options)
{
    write_table_ragged_col_t ragged_cols[] = {
        { "provenances/timestamp", (void *) self->timestamp, self->timestamp_length,
            KAS_UINT8, self->timestamp_offset, self->num_rows },
        { "provenances/record", (void *) self->record, self->record_length, KAS_UINT8,
            self->record_offset, self->num_rows },
        { .name = NULL },
    };

    return write_table_ragged_cols(store, ragged_cols, options);
}

static int
tsk_provenance_table_load(tsk_provenance_table_t *self, kastore_t *store)
{
    int ret;
    char *timestamp = NULL;
    tsk_size_t *timestamp_offset = NULL;
    char *record = NULL;
    tsk_size_t *record_offset = NULL;
    tsk_size_t num_rows, timestamp_length, record_length;

    read_table_ragged_col_t ragged_cols[] = {
        { "provenances/timestamp", (void **) &timestamp, &timestamp_length, KAS_UINT8,
            &timestamp_offset, 0 },
        { "provenances/record", (void **) &record, &record_length, KAS_UINT8,
            &record_offset, 0 },
        { .name = NULL },
    };

    ret = read_table(store, &num_rows, NULL, ragged_cols, NULL, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_provenance_table_takeset_columns(
        self, num_rows, timestamp, timestamp_offset, record, record_offset);
    if (ret != 0) {
        goto out;
    }
    timestamp = NULL;
    timestamp_offset = NULL;
    record = NULL;
    record_offset = NULL;

out:
    free_read_table_mem(NULL, ragged_cols, NULL);
    return ret;
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
    /* It would be a little bit more convenient to store a pointer to the
     * metadata here in the struct rather than an offset back into the
     * original array. However, this would increase the size of the struct
     * from 40 bytes to 48 and we will allocate very large numbers of these.
     */
    tsk_size_t metadata_offset;
    tsk_size_t metadata_length;
} edge_sort_t;

typedef struct {
    tsk_mutation_t mut;
    int num_descendants;
    double node_time;
} mutation_sort_t;

typedef struct {
    tsk_individual_t ind;
    tsk_id_t first_node;
    tsk_size_t num_descendants;
} individual_canonical_sort_t;

typedef struct {
    double left;
    double right;
    tsk_id_t node;
    tsk_id_t source;
    tsk_id_t dest;
    double time;
    tsk_size_t metadata_offset;
    tsk_size_t metadata_length;
} migration_sort_t;

static int
cmp_site(const void *a, const void *b)
{
    const tsk_site_t *ia = (const tsk_site_t *) a;
    const tsk_site_t *ib = (const tsk_site_t *) b;
    /* Compare sites by position */
    int ret = (ia->position > ib->position) - (ia->position < ib->position);
    if (ret == 0) {
        /* Within a particular position sort by ID.  This ensures that relative
         * ordering of multiple sites at the same position is maintained; the
         * redundant sites will get compacted down by clean_tables(), but in the
         * meantime if the order of the redundant sites changes it will cause the
         * sort order of mutations to be corrupted, as the mutations will follow
         * their sites. */
        ret = (ia->id > ib->id) - (ia->id < ib->id);
    }
    return ret;
}

static int
cmp_mutation(const void *a, const void *b)
{
    const mutation_sort_t *ia = (const mutation_sort_t *) a;
    const mutation_sort_t *ib = (const mutation_sort_t *) b;
    /* Compare mutations by site */
    int ret = (ia->mut.site > ib->mut.site) - (ia->mut.site < ib->mut.site);

    /* Within a particular site sort by time if known */
    if (ret == 0 && !tsk_is_unknown_time(ia->mut.time)
        && !tsk_is_unknown_time(ib->mut.time)) {
        ret = (ia->mut.time < ib->mut.time) - (ia->mut.time > ib->mut.time);
    }
    /* Or node times when mutation times are unknown or equal */
    if (ret == 0) {
        ret = (ia->node_time < ib->node_time) - (ia->node_time > ib->node_time);
    }
    /* If node times are equal, sort by number of descendants */
    if (ret == 0) {
        ret = (ia->num_descendants < ib->num_descendants)
              - (ia->num_descendants > ib->num_descendants);
    }
    /* If number of descendants are equal, sort by node */
    if (ret == 0) {
        ret = (ia->mut.node > ib->mut.node) - (ia->mut.node < ib->mut.node);
    }
    /* Final tiebreaker: ID */
    if (ret == 0) {
        ret = (ia->mut.id > ib->mut.id) - (ia->mut.id < ib->mut.id);
    }
    return ret;
}

static int
cmp_individual_canonical(const void *a, const void *b)
{
    const individual_canonical_sort_t *ia = (const individual_canonical_sort_t *) a;
    const individual_canonical_sort_t *ib = (const individual_canonical_sort_t *) b;
    int ret = (ia->num_descendants < ib->num_descendants)
              - (ia->num_descendants > ib->num_descendants);
    if (ret == 0) {
        ret = (ia->first_node > ib->first_node) - (ia->first_node < ib->first_node);
    }
    if (ret == 0) {
        ret = (ia->ind.id > ib->ind.id) - (ia->ind.id < ib->ind.id);
    }
    return ret;
}

static int
cmp_edge(const void *a, const void *b)
{
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
cmp_migration(const void *a, const void *b)
{
    const migration_sort_t *ca = (const migration_sort_t *) a;
    const migration_sort_t *cb = (const migration_sort_t *) b;

    int ret = (ca->time > cb->time) - (ca->time < cb->time);
    /* If time values are equal, sort by the source population */
    if (ret == 0) {
        ret = (ca->source > cb->source) - (ca->source < cb->source);
        /* If the source populations are equal, sort by the dest */
        if (ret == 0) {
            ret = (ca->dest > cb->dest) - (ca->dest < cb->dest);
            /* If the dest populations are equal, sort by the left coordinate. */
            if (ret == 0) {
                ret = (ca->left > cb->left) - (ca->left < cb->left);
                /* If everything else is equal, compare by node */
                if (ret == 0) {
                    ret = (ca->node > cb->node) - (ca->node < cb->node);
                }
            }
        }
    }
    return ret;
}

static int
tsk_table_sorter_sort_edges(tsk_table_sorter_t *self, tsk_size_t start)
{
    int ret = 0;
    const tsk_edge_table_t *edges = &self->tables->edges;
    const double *restrict node_time = self->tables->nodes.time;
    edge_sort_t *e;
    tsk_size_t j, k, metadata_offset;
    tsk_size_t n = edges->num_rows - start;
    edge_sort_t *sorted_edges = tsk_malloc(n * sizeof(*sorted_edges));
    char *old_metadata = tsk_malloc(edges->metadata_length);
    bool has_metadata = tsk_edge_table_has_metadata(edges);

    if (sorted_edges == NULL || old_metadata == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memcpy(old_metadata, edges->metadata, edges->metadata_length);
    for (j = 0; j < n; j++) {
        e = sorted_edges + j;
        k = start + j;
        e->left = edges->left[k];
        e->right = edges->right[k];
        e->parent = edges->parent[k];
        e->child = edges->child[k];
        e->time = node_time[e->parent];
        if (has_metadata) {
            e->metadata_offset = edges->metadata_offset[k];
            e->metadata_length
                = edges->metadata_offset[k + 1] - edges->metadata_offset[k];
        }
    }
    qsort(sorted_edges, (size_t) n, sizeof(edge_sort_t), cmp_edge);
    /* Copy the edges back into the table. */
    metadata_offset = 0;
    for (j = 0; j < n; j++) {
        e = sorted_edges + j;
        k = start + j;
        edges->left[k] = e->left;
        edges->right[k] = e->right;
        edges->parent[k] = e->parent;
        edges->child[k] = e->child;
        if (has_metadata) {
            tsk_memcpy(edges->metadata + metadata_offset,
                old_metadata + e->metadata_offset, e->metadata_length);
            edges->metadata_offset[k] = metadata_offset;
            metadata_offset += e->metadata_length;
        }
    }
out:
    tsk_safe_free(sorted_edges);
    tsk_safe_free(old_metadata);
    return ret;
}

static int
tsk_table_sorter_sort_migrations(tsk_table_sorter_t *self, tsk_size_t start)
{
    int ret = 0;
    const tsk_migration_table_t *migrations = &self->tables->migrations;
    migration_sort_t *m;
    tsk_size_t j, k, metadata_offset;
    tsk_size_t n = migrations->num_rows - start;
    migration_sort_t *sorted_migrations = tsk_malloc(n * sizeof(*sorted_migrations));
    char *old_metadata = tsk_malloc(migrations->metadata_length);

    if (sorted_migrations == NULL || old_metadata == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memcpy(old_metadata, migrations->metadata, migrations->metadata_length);
    for (j = 0; j < n; j++) {
        m = sorted_migrations + j;
        k = start + j;
        m->left = migrations->left[k];
        m->right = migrations->right[k];
        m->node = migrations->node[k];
        m->source = migrations->source[k];
        m->dest = migrations->dest[k];
        m->time = migrations->time[k];
        m->metadata_offset = migrations->metadata_offset[k];
        m->metadata_length
            = migrations->metadata_offset[k + 1] - migrations->metadata_offset[k];
    }
    qsort(sorted_migrations, (size_t) n, sizeof(migration_sort_t), cmp_migration);
    /* Copy the migrations back into the table. */
    metadata_offset = 0;
    for (j = 0; j < n; j++) {
        m = sorted_migrations + j;
        k = start + j;
        migrations->left[k] = m->left;
        migrations->right[k] = m->right;
        migrations->node[k] = m->node;
        migrations->source[k] = m->source;
        migrations->dest[k] = m->dest;
        migrations->time[k] = m->time;
        tsk_memcpy(migrations->metadata + metadata_offset,
            old_metadata + m->metadata_offset, m->metadata_length);
        migrations->metadata_offset[k] = metadata_offset;
        metadata_offset += m->metadata_length;
    }
out:
    tsk_safe_free(sorted_migrations);
    tsk_safe_free(old_metadata);
    return ret;
}

static int
tsk_table_sorter_sort_sites(tsk_table_sorter_t *self)
{
    int ret = 0;
    tsk_id_t ret_id;
    tsk_site_table_t *sites = &self->tables->sites;
    tsk_site_table_t copy;
    tsk_size_t j;
    tsk_size_t num_sites = sites->num_rows;
    tsk_site_t *sorted_sites = tsk_malloc(num_sites * sizeof(*sorted_sites));

    ret = tsk_site_table_copy(sites, &copy, 0);
    if (ret != 0) {
        goto out;
    }
    if (sorted_sites == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    for (j = 0; j < num_sites; j++) {
        tsk_site_table_get_row_unsafe(&copy, (tsk_id_t) j, sorted_sites + j);
    }

    /* Sort the sites by position */
    qsort(sorted_sites, (size_t) num_sites, sizeof(*sorted_sites), cmp_site);

    /* Build the mapping from old site IDs to new site IDs and copy back into the
     * table
     */
    tsk_site_table_clear(sites);
    for (j = 0; j < num_sites; j++) {
        self->site_id_map[sorted_sites[j].id] = (tsk_id_t) j;
        ret_id = tsk_site_table_add_row(sites, sorted_sites[j].position,
            sorted_sites[j].ancestral_state, sorted_sites[j].ancestral_state_length,
            sorted_sites[j].metadata, sorted_sites[j].metadata_length);
        if (ret_id < 0) {
            ret = (int) ret_id;
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
tsk_table_sorter_sort_mutations(tsk_table_sorter_t *self)
{
    int ret = 0;
    tsk_size_t j;
    tsk_id_t ret_id, parent, mapped_parent, p;
    tsk_mutation_table_t *mutations = &self->tables->mutations;
    tsk_node_table_t *nodes = &self->tables->nodes;
    tsk_size_t num_mutations = mutations->num_rows;
    tsk_mutation_table_t copy;
    mutation_sort_t *sorted_mutations
        = tsk_malloc(num_mutations * sizeof(*sorted_mutations));
    tsk_id_t *mutation_id_map = tsk_malloc(num_mutations * sizeof(*mutation_id_map));

    ret = tsk_mutation_table_copy(mutations, &copy, 0);
    if (ret != 0) {
        goto out;
    }
    if (mutation_id_map == NULL || sorted_mutations == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    /* compute numbers of descendants for each mutation */
    for (j = 0; j < num_mutations; j++) {
        sorted_mutations[j].num_descendants = 0;
    }
    for (j = 0; j < num_mutations; j++) {
        p = mutations->parent[j];
        while (p != TSK_NULL) {
            sorted_mutations[p].num_descendants += 1;
            if (sorted_mutations[p].num_descendants > (int) num_mutations) {
                ret = tsk_trace_error(TSK_ERR_MUTATION_PARENT_INCONSISTENT);
                goto out;
            }
            p = mutations->parent[p];
        }
    }

    for (j = 0; j < num_mutations; j++) {
        tsk_mutation_table_get_row_unsafe(&copy, (tsk_id_t) j, &sorted_mutations[j].mut);
        sorted_mutations[j].mut.site = self->site_id_map[sorted_mutations[j].mut.site];
        sorted_mutations[j].node_time = nodes->time[sorted_mutations[j].mut.node];
    }
    ret = tsk_mutation_table_clear(mutations);
    if (ret != 0) {
        goto out;
    }

    qsort(sorted_mutations, (size_t) num_mutations, sizeof(*sorted_mutations),
        cmp_mutation);

    /* Make a first pass through the sorted mutations to build the ID map. */
    for (j = 0; j < num_mutations; j++) {
        mutation_id_map[sorted_mutations[j].mut.id] = (tsk_id_t) j;
    }

    for (j = 0; j < num_mutations; j++) {
        mapped_parent = TSK_NULL;
        parent = sorted_mutations[j].mut.parent;
        if (parent != TSK_NULL) {
            mapped_parent = mutation_id_map[parent];
        }
        ret_id = tsk_mutation_table_add_row(mutations, sorted_mutations[j].mut.site,
            sorted_mutations[j].mut.node, mapped_parent, sorted_mutations[j].mut.time,
            sorted_mutations[j].mut.derived_state,
            sorted_mutations[j].mut.derived_state_length,
            sorted_mutations[j].mut.metadata, sorted_mutations[j].mut.metadata_length);
        if (ret_id < 0) {
            ret = (int) ret_id;
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
tsk_individual_table_topological_sort(
    tsk_individual_table_t *self, tsk_id_t *traversal_order, tsk_size_t *num_descendants)
{
    int ret = 0;
    tsk_id_t i, j, p;
    tsk_individual_t individual;
    tsk_size_t num_individuals = self->num_rows;
    tsk_size_t current_todo = 0;
    tsk_size_t todo_insertion_point = 0;
    tsk_size_t *incoming_edge_count
        = tsk_malloc(num_individuals * sizeof(*incoming_edge_count));
    bool count_descendants = (num_descendants != NULL);

    if (incoming_edge_count == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    for (i = 0; i < (tsk_id_t) num_individuals; i++) {
        incoming_edge_count[i] = 0;
        traversal_order[i] = TSK_NULL;
        if (count_descendants) {
            num_descendants[i] = 0;
        }
    }

    /* First find the set of individuals that have no children by creating
     * an array of incoming edge counts */
    for (i = 0; i < (tsk_id_t) self->parents_length; i++) {
        if (self->parents[i] != TSK_NULL) {
            incoming_edge_count[self->parents[i]]++;
        }
    }
    /* Use these as the starting points for checking all individuals,
     * doing this in reverse makes the sort stable */
    for (i = (tsk_id_t) num_individuals - 1; i >= 0; i--) {
        if (incoming_edge_count[i] == 0) {
            traversal_order[todo_insertion_point] = i;
            todo_insertion_point++;
        }
    }

    /* Now process individuals from the set that have no children, updating their
     * parents' information as we go, and adding their parents to the list if
     * this was their last child */
    while (current_todo < todo_insertion_point) {
        j = traversal_order[current_todo];
        tsk_individual_table_get_row_unsafe(self, j, &individual);
        for (i = 0; i < (tsk_id_t) individual.parents_length; i++) {
            p = individual.parents[i];
            if (p != TSK_NULL) {
                incoming_edge_count[p]--;
                if (count_descendants) {
                    num_descendants[p] += 1 + num_descendants[j];
                }
                if (incoming_edge_count[p] == 0) {
                    traversal_order[todo_insertion_point] = p;
                    todo_insertion_point++;
                }
            }
        }
        current_todo++;
    }

    /* Any edges left are parts of cycles */
    for (i = 0; i < (tsk_id_t) num_individuals; i++) {
        if (incoming_edge_count[i] > 0) {
            ret = tsk_trace_error(TSK_ERR_INDIVIDUAL_PARENT_CYCLE);
            goto out;
        }
    }

out:
    tsk_safe_free(incoming_edge_count);
    return ret;
}

int
tsk_table_collection_individual_topological_sort(
    tsk_table_collection_t *self, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_id_t i, ret_id;
    tsk_individual_table_t copy;
    tsk_individual_t individual;
    tsk_individual_table_t *individuals = &self->individuals;
    tsk_node_table_t *nodes = &self->nodes;
    tsk_size_t num_individuals = individuals->num_rows;
    tsk_id_t *traversal_order = tsk_malloc(num_individuals * sizeof(*traversal_order));
    tsk_id_t *new_id_map = tsk_malloc(num_individuals * sizeof(*new_id_map));

    if (new_id_map == NULL || traversal_order == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memset(new_id_map, 0xff, num_individuals * sizeof(*new_id_map));

    ret = tsk_individual_table_copy(individuals, &copy, 0);
    if (ret != 0) {
        goto out;
    }

    ret_id = tsk_table_collection_check_integrity(self, 0);
    if (ret_id != 0) {
        ret = (int) ret_id;
        goto out;
    }

    ret = tsk_individual_table_clear(individuals);
    if (ret != 0) {
        goto out;
    }

    ret = tsk_individual_table_topological_sort(&copy, traversal_order, NULL);
    if (ret != 0) {
        goto out;
    }

    /* The sorted individuals are in reverse order */
    for (i = (tsk_id_t) num_individuals - 1; i >= 0; i--) {
        tsk_individual_table_get_row_unsafe(&copy, traversal_order[i], &individual);
        ret_id = tsk_individual_table_add_row(individuals, individual.flags,
            individual.location, individual.location_length, individual.parents,
            individual.parents_length, individual.metadata, individual.metadata_length);
        if (ret_id < 0) {
            ret = (int) ret_id;
            goto out;
        }
        new_id_map[traversal_order[i]] = ret_id;
    }

    /* Rewrite the parent ids */
    for (i = 0; i < (tsk_id_t) individuals->parents_length; i++) {
        if (individuals->parents[i] != TSK_NULL) {
            individuals->parents[i] = new_id_map[individuals->parents[i]];
        }
    }
    /* Rewrite the node individual ids */
    for (i = 0; i < (tsk_id_t) nodes->num_rows; i++) {
        if (nodes->individual[i] != TSK_NULL) {
            nodes->individual[i] = new_id_map[nodes->individual[i]];
        }
    }

    ret = 0;
out:
    tsk_safe_free(traversal_order);
    tsk_safe_free(new_id_map);
    tsk_individual_table_free(&copy);
    return ret;
}

static int
tsk_table_sorter_sort_individuals_canonical(tsk_table_sorter_t *self)
{
    int ret = 0;
    tsk_id_t ret_id, i, j, parent, mapped_parent;
    tsk_individual_table_t *individuals = &self->tables->individuals;
    tsk_node_table_t *nodes = &self->tables->nodes;
    tsk_individual_table_t copy;
    tsk_size_t num_individuals = individuals->num_rows;
    individual_canonical_sort_t *sorted_individuals
        = tsk_malloc(num_individuals * sizeof(*sorted_individuals));
    tsk_id_t *individual_id_map
        = tsk_malloc(num_individuals * sizeof(*individual_id_map));
    tsk_size_t *num_descendants = tsk_malloc(num_individuals * sizeof(*num_descendants));
    tsk_id_t *traversal_order = tsk_malloc(num_individuals * sizeof(*traversal_order));

    if (individual_id_map == NULL || sorted_individuals == NULL
        || traversal_order == NULL || num_descendants == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    ret = tsk_individual_table_copy(individuals, &copy, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_individual_table_clear(individuals);
    if (ret != 0) {
        goto out;
    }

    ret = tsk_individual_table_topological_sort(&copy, traversal_order, num_descendants);
    if (ret != 0) {
        goto out;
    }

    for (i = 0; i < (tsk_id_t) num_individuals; i++) {
        sorted_individuals[i].num_descendants = num_descendants[i];
        sorted_individuals[i].first_node = (tsk_id_t) nodes->num_rows;
    }

    /* find first referring node */
    for (j = 0; j < (tsk_id_t) nodes->num_rows; j++) {
        if (nodes->individual[j] != TSK_NULL) {
            sorted_individuals[nodes->individual[j]].first_node
                = TSK_MIN(j, sorted_individuals[nodes->individual[j]].first_node);
        }
    }

    for (j = 0; j < (tsk_id_t) num_individuals; j++) {
        tsk_individual_table_get_row_unsafe(
            &copy, (tsk_id_t) j, &sorted_individuals[j].ind);
    }

    qsort(sorted_individuals, (size_t) num_individuals, sizeof(*sorted_individuals),
        cmp_individual_canonical);

    /* Make a first pass through the sorted individuals to build the ID map. */
    for (j = 0; j < (tsk_id_t) num_individuals; j++) {
        individual_id_map[sorted_individuals[j].ind.id] = (tsk_id_t) j;
    }

    for (i = 0; i < (tsk_id_t) num_individuals; i++) {
        for (j = 0; j < (tsk_id_t) sorted_individuals[i].ind.parents_length; j++) {
            parent = sorted_individuals[i].ind.parents[j];
            if (parent != TSK_NULL) {
                mapped_parent = individual_id_map[parent];
                sorted_individuals[i].ind.parents[j] = mapped_parent;
            }
        }
        ret_id = tsk_individual_table_add_row(individuals,
            sorted_individuals[i].ind.flags, sorted_individuals[i].ind.location,
            sorted_individuals[i].ind.location_length, sorted_individuals[i].ind.parents,
            sorted_individuals[i].ind.parents_length, sorted_individuals[i].ind.metadata,
            sorted_individuals[i].ind.metadata_length);
        if (ret_id < 0) {
            ret = (int) ret_id;
            goto out;
        }
    }
    ret = 0;

    /* remap individuals in the node table */
    for (i = 0; i < (tsk_id_t) nodes->num_rows; i++) {
        j = nodes->individual[i];
        if (j != TSK_NULL) {
            nodes->individual[i] = individual_id_map[j];
        }
    }

out:
    tsk_safe_free(sorted_individuals);
    tsk_safe_free(individual_id_map);
    tsk_safe_free(traversal_order);
    tsk_safe_free(num_descendants);
    tsk_individual_table_free(&copy);
    return ret;
}

int
tsk_table_sorter_run(tsk_table_sorter_t *self, const tsk_bookmark_t *start)
{
    int ret = 0;
    tsk_size_t edge_start = 0;
    tsk_size_t migration_start = 0;
    bool skip_sites = false;
    bool skip_individuals = false;

    if (start != NULL) {
        if (start->edges > self->tables->edges.num_rows) {
            ret = tsk_trace_error(TSK_ERR_EDGE_OUT_OF_BOUNDS);
            goto out;
        }
        edge_start = start->edges;
        if (start->migrations > self->tables->migrations.num_rows) {
            ret = tsk_trace_error(TSK_ERR_MIGRATION_OUT_OF_BOUNDS);
            goto out;
        }
        migration_start = start->migrations;

        /* We only allow sites and mutations to be specified as a way to
         * skip sorting them entirely. Both sites and mutations must be
         * equal to the number of rows */
        if (start->sites == self->tables->sites.num_rows
            && start->mutations == self->tables->mutations.num_rows) {
            skip_sites = true;
        } else if (start->sites != 0 || start->mutations != 0) {
            ret = tsk_trace_error(TSK_ERR_SORT_OFFSET_NOT_SUPPORTED);
            goto out;
        }
    }
    /* The indexes will be invalidated, so drop them */
    ret = tsk_table_collection_drop_index(self->tables, 0);
    if (ret != 0) {
        goto out;
    }

    if (self->sort_edges != NULL) {
        ret = self->sort_edges(self, edge_start);
        if (ret != 0) {
            goto out;
        }
    }
    /* Avoid calling sort_migrations in the common case when it's a no-op */
    if (self->tables->migrations.num_rows > 0) {
        ret = tsk_table_sorter_sort_migrations(self, migration_start);
        if (ret != 0) {
            goto out;
        }
    }
    if (!skip_sites) {
        ret = tsk_table_sorter_sort_sites(self);
        if (ret != 0) {
            goto out;
        }
        ret = self->sort_mutations(self);
        if (ret != 0) {
            goto out;
        }
    }
    if (!skip_individuals && self->sort_individuals != NULL) {
        ret = self->sort_individuals(self);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

int
tsk_table_sorter_init(
    tsk_table_sorter_t *self, tsk_table_collection_t *tables, tsk_flags_t options)
{
    int ret = 0;
    tsk_id_t ret_id;

    tsk_memset(self, 0, sizeof(tsk_table_sorter_t));
    if (!(options & TSK_NO_CHECK_INTEGRITY)) {
        ret_id = tsk_table_collection_check_integrity(tables, 0);
        if (ret_id != 0) {
            ret = (int) ret_id;
            goto out;
        }
    }
    self->tables = tables;

    self->site_id_map = tsk_malloc(self->tables->sites.num_rows * sizeof(tsk_id_t));
    if (self->site_id_map == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    /* Set the sort_edges and sort_mutations methods to the default. */
    self->sort_edges = tsk_table_sorter_sort_edges;
    self->sort_mutations = tsk_table_sorter_sort_mutations;
    /* Default sort doesn't touch individuals */
    self->sort_individuals = NULL;
out:
    return ret;
}

int
tsk_table_sorter_free(tsk_table_sorter_t *self)
{
    tsk_safe_free(self->site_id_map);
    return 0;
}

/*************************
 * segment overlapper
 *************************/

typedef struct _interval_list_t {
    double left;
    double right;
    struct _interval_list_t *next;
} interval_list_t;

typedef struct _mutation_id_list_t {
    tsk_id_t mutation;
    struct _mutation_id_list_t *next;
} mutation_id_list_t;

typedef struct _tsk_segment_t {
    double left;
    double right;
    struct _tsk_segment_t *next;
    tsk_id_t node;
} tsk_segment_t;

/* segment overlap finding algorithm */
typedef struct {
    /* The input segments. This buffer is sorted by the algorithm and we also
     * assume that there is space for an extra element at the end */
    tsk_segment_t *segments;
    tsk_size_t num_segments;
    tsk_size_t index;
    tsk_size_t num_overlapping;
    double left;
    double right;
    /* Output buffer */
    tsk_size_t max_overlapping;
    tsk_segment_t **overlapping;
} segment_overlapper_t;

typedef struct {
    tsk_size_t num_samples;
    tsk_flags_t options;
    tsk_table_collection_t *tables;
    /* Keep a copy of the input tables */
    tsk_table_collection_t input_tables;
    /* State for topology */
    tsk_segment_t **ancestor_map_head;
    tsk_segment_t **ancestor_map_tail;
    /* Mapping of input node IDs to output node IDs. */
    tsk_id_t *node_id_map;
    bool *is_sample;
    /* Segments for a particular parent that are processed together */
    tsk_segment_t *segment_queue;
    tsk_size_t segment_queue_size;
    tsk_size_t max_segment_queue_size;
    segment_overlapper_t segment_overlapper;
    tsk_blkalloc_t segment_heap;
    /* Buffer for output edges. For each child we keep a linked list of
     * intervals, and also store the actual children that have been buffered. */
    tsk_blkalloc_t interval_list_heap;
    interval_list_t **child_edge_map_head;
    interval_list_t **child_edge_map_tail;
    tsk_id_t *buffered_children;
    tsk_size_t num_buffered_children;
    /* For each mutation, map its output node. */
    tsk_id_t *mutation_node_map;
    /* Map of input nodes to the list of input mutation IDs */
    mutation_id_list_t **node_mutation_list_map_head;
    mutation_id_list_t **node_mutation_list_map_tail;
    mutation_id_list_t *node_mutation_list_mem;
    /* When reducing topology, we need a map positions to their corresponding
     * sites.*/
    double *position_lookup;
    int64_t edge_sort_offset;
} simplifier_t;

static int
cmp_segment(const void *a, const void *b)
{
    const tsk_segment_t *ia = (const tsk_segment_t *) a;
    const tsk_segment_t *ib = (const tsk_segment_t *) b;
    int ret = (ia->left > ib->left) - (ia->left < ib->left);
    /* Break ties using the node */
    if (ret == 0) {
        ret = (ia->node > ib->node) - (ia->node < ib->node);
    }
    return ret;
}

static int TSK_WARN_UNUSED
segment_overlapper_alloc(segment_overlapper_t *self)
{
    int ret = 0;

    tsk_memset(self, 0, sizeof(*self));
    self->max_overlapping = 8; /* Making sure we call tsk_realloc in tests */
    self->overlapping = tsk_malloc(self->max_overlapping * sizeof(*self->overlapping));
    if (self->overlapping == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
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
segment_overlapper_start(
    segment_overlapper_t *self, tsk_segment_t *segments, tsk_size_t num_segments)
{
    int ret = 0;
    tsk_segment_t *sentinel;
    void *p;

    if (self->max_overlapping < num_segments) {
        self->max_overlapping = num_segments;
        p = tsk_realloc(
            self->overlapping, self->max_overlapping * sizeof(*self->overlapping));
        if (p == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
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
    qsort(
        self->segments, (size_t) self->num_segments, sizeof(tsk_segment_t), cmp_segment);
    /* NOTE! We are assuming that there's space for another element on the end
     * here. This is to insert a sentinel which simplifies the logic. */
    sentinel = self->segments + self->num_segments;
    sentinel->left = DBL_MAX;
out:
    return ret;
}

static int TSK_WARN_UNUSED
segment_overlapper_next(segment_overlapper_t *self, double *left, double *right,
    tsk_segment_t ***overlapping, tsk_size_t *num_overlapping)
{
    int ret = 0;
    tsk_size_t j, k;
    tsk_size_t n = self->num_segments;
    tsk_segment_t *S = self->segments;

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
            tsk_bug_assert(self->num_overlapping < self->max_overlapping);
            self->overlapping[self->num_overlapping] = &S[self->index];
            self->num_overlapping++;
            self->index++;
        }
        self->index--;
        self->right = S[self->index + 1].left;
        for (j = 0; j < self->num_overlapping; j++) {
            self->right = TSK_MIN(self->right, self->overlapping[j]->right);
        }
        tsk_bug_assert(self->left < self->right);
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

static int
cmp_node_id(const void *a, const void *b)
{
    const tsk_id_t *ia = (const tsk_id_t *) a;
    const tsk_id_t *ib = (const tsk_id_t *) b;
    return (*ia > *ib) - (*ia < *ib);
}

/*************************
 * Ancestor mapper
 *************************/

/* NOTE: this struct shares a lot with the simplifier_t, mostly in
 * terms of infrastructure for managing the list of intervals, saving
 * edges etc. We should try to abstract the common functionality out
 * into a separate class, which handles this.
 */
typedef struct {
    tsk_id_t *samples;
    tsk_size_t num_samples;
    tsk_id_t *ancestors;
    tsk_size_t num_ancestors;
    tsk_table_collection_t *tables;
    tsk_edge_table_t *result;
    tsk_segment_t **ancestor_map_head;
    tsk_segment_t **ancestor_map_tail;
    bool *is_sample;
    bool *is_ancestor;
    tsk_segment_t *segment_queue;
    tsk_size_t segment_queue_size;
    tsk_size_t max_segment_queue_size;
    segment_overlapper_t segment_overlapper;
    tsk_blkalloc_t segment_heap;
    tsk_blkalloc_t interval_list_heap;
    interval_list_t **child_edge_map_head;
    interval_list_t **child_edge_map_tail;
    tsk_id_t *buffered_children;
    tsk_size_t num_buffered_children;
    double sequence_length;
    double oldest_node_time;
} ancestor_mapper_t;

static tsk_segment_t *TSK_WARN_UNUSED
ancestor_mapper_alloc_segment(
    ancestor_mapper_t *self, double left, double right, tsk_id_t node)
{
    tsk_segment_t *seg = NULL;

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

static interval_list_t *TSK_WARN_UNUSED
ancestor_mapper_alloc_interval_list(ancestor_mapper_t *self, double left, double right)
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

static int
ancestor_mapper_flush_edges(
    ancestor_mapper_t *self, tsk_id_t parent, tsk_size_t *ret_num_edges)
{
    int ret = 0;
    tsk_id_t ret_id;
    tsk_size_t j;
    tsk_id_t child;
    interval_list_t *x;
    tsk_size_t num_edges = 0;

    qsort(self->buffered_children, (size_t) self->num_buffered_children,
        sizeof(tsk_id_t), cmp_node_id);
    for (j = 0; j < self->num_buffered_children; j++) {
        child = self->buffered_children[j];
        for (x = self->child_edge_map_head[child]; x != NULL; x = x->next) {
            ret_id = tsk_edge_table_add_row(
                self->result, x->left, x->right, parent, child, NULL, 0);
            if (ret_id < 0) {
                ret = (int) ret_id;
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

static int
ancestor_mapper_record_edge(
    ancestor_mapper_t *self, double left, double right, tsk_id_t child)
{
    int ret = 0;
    interval_list_t *tail, *x;

    tail = self->child_edge_map_tail[child];
    if (tail == NULL) {
        tsk_bug_assert(self->num_buffered_children < self->tables->nodes.num_rows);
        self->buffered_children[self->num_buffered_children] = child;
        self->num_buffered_children++;
        x = ancestor_mapper_alloc_interval_list(self, left, right);
        if (x == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
        self->child_edge_map_head[child] = x;
        self->child_edge_map_tail[child] = x;
    } else {
        if (tail->right == left) {
            tail->right = right;
        } else {
            x = ancestor_mapper_alloc_interval_list(self, left, right);
            if (x == NULL) {
                ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
                goto out;
            }
            tail->next = x;
            self->child_edge_map_tail[child] = x;
        }
    }
out:
    return ret;
}

static int TSK_WARN_UNUSED
ancestor_mapper_add_ancestry(ancestor_mapper_t *self, tsk_id_t input_id, double left,
    double right, tsk_id_t output_id)
{
    int ret = 0;
    tsk_segment_t *tail = self->ancestor_map_tail[input_id];
    tsk_segment_t *x;

    tsk_bug_assert(left < right);
    if (tail == NULL) {
        x = ancestor_mapper_alloc_segment(self, left, right, output_id);
        if (x == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
        self->ancestor_map_head[input_id] = x;
        self->ancestor_map_tail[input_id] = x;
    } else {
        if (tail->right == left && tail->node == output_id) {
            tail->right = right;
        } else {
            x = ancestor_mapper_alloc_segment(self, left, right, output_id);
            if (x == NULL) {
                ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
                goto out;
            }
            tail->next = x;
            self->ancestor_map_tail[input_id] = x;
        }
    }
out:
    return ret;
}

static void
ancestor_mapper_find_oldest_node(ancestor_mapper_t *self)
{
    const double *node_time = self->tables->nodes.time;
    tsk_size_t j;
    double max_time = -1;

    for (j = 0; j < self->num_ancestors; j++) {
        max_time = TSK_MAX(max_time, node_time[self->ancestors[j]]);
    }
    for (j = 0; j < self->num_samples; j++) {
        max_time = TSK_MAX(max_time, node_time[self->samples[j]]);
    }

    self->oldest_node_time = max_time;
}

static int
ancestor_mapper_init_samples(ancestor_mapper_t *self, tsk_id_t *samples)
{
    int ret = 0;
    tsk_size_t j;

    /* Go through the samples to check for errors. */
    for (j = 0; j < self->num_samples; j++) {
        if (samples[j] < 0 || samples[j] > (tsk_id_t) self->tables->nodes.num_rows) {
            ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
            goto out;
        }
        if (self->is_sample[samples[j]]) {
            ret = tsk_trace_error(TSK_ERR_DUPLICATE_SAMPLE);
            goto out;
        }
        self->is_sample[samples[j]] = true;
        ret = ancestor_mapper_add_ancestry(
            self, samples[j], 0, self->tables->sequence_length, samples[j]);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

static int
ancestor_mapper_init_ancestors(ancestor_mapper_t *self, tsk_id_t *ancestors)
{
    int ret = 0;
    tsk_size_t j;

    /* Go through the samples to check for errors. */
    for (j = 0; j < self->num_ancestors; j++) {
        if (ancestors[j] < 0 || ancestors[j] > (tsk_id_t) self->tables->nodes.num_rows) {
            ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
            goto out;
        }
        if (self->is_ancestor[ancestors[j]]) {
            ret = tsk_trace_error(TSK_ERR_DUPLICATE_SAMPLE);
            goto out;
        }
        self->is_ancestor[ancestors[j]] = true;
    }
out:
    return ret;
}

static int
ancestor_mapper_init(ancestor_mapper_t *self, tsk_id_t *samples, tsk_size_t num_samples,
    tsk_id_t *ancestors, tsk_size_t num_ancestors, tsk_table_collection_t *tables,
    tsk_edge_table_t *result)
{
    int ret = 0;
    tsk_size_t num_nodes;

    tsk_memset(self, 0, sizeof(ancestor_mapper_t));
    self->num_samples = num_samples;
    self->num_ancestors = num_ancestors;
    self->samples = samples;
    self->ancestors = ancestors;
    self->tables = tables;
    self->result = result;
    self->sequence_length = self->tables->sequence_length;

    if (samples == NULL || num_samples == 0 || ancestors == NULL || num_ancestors == 0) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }

    /* Allocate the heaps used for small objects-> Assuming 8K is a good chunk size
     */
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

    num_nodes = tables->nodes.num_rows;
    /* Make the maps and set the intial state */
    self->ancestor_map_head = tsk_calloc(num_nodes, sizeof(tsk_segment_t *));
    self->ancestor_map_tail = tsk_calloc(num_nodes, sizeof(tsk_segment_t *));
    self->child_edge_map_head = tsk_calloc(num_nodes, sizeof(interval_list_t *));
    self->child_edge_map_tail = tsk_calloc(num_nodes, sizeof(interval_list_t *));
    self->buffered_children = tsk_malloc(num_nodes * sizeof(tsk_id_t));
    self->is_sample = tsk_calloc(num_nodes, sizeof(bool));
    self->is_ancestor = tsk_calloc(num_nodes, sizeof(bool));
    self->max_segment_queue_size = 64;
    self->segment_queue
        = tsk_malloc(self->max_segment_queue_size * sizeof(tsk_segment_t));
    if (self->ancestor_map_head == NULL || self->ancestor_map_tail == NULL
        || self->child_edge_map_head == NULL || self->child_edge_map_tail == NULL
        || self->is_sample == NULL || self->is_ancestor == NULL
        || self->segment_queue == NULL || self->buffered_children == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    // Clear memory.
    ret = ancestor_mapper_init_samples(self, samples);
    if (ret != 0) {
        goto out;
    }
    ret = ancestor_mapper_init_ancestors(self, ancestors);
    if (ret != 0) {
        goto out;
    }
    ancestor_mapper_find_oldest_node(self);
    ret = tsk_edge_table_clear(self->result);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

static int
ancestor_mapper_free(ancestor_mapper_t *self)
{
    tsk_blkalloc_free(&self->segment_heap);
    tsk_blkalloc_free(&self->interval_list_heap);
    segment_overlapper_free(&self->segment_overlapper);
    tsk_safe_free(self->ancestor_map_head);
    tsk_safe_free(self->ancestor_map_tail);
    tsk_safe_free(self->child_edge_map_head);
    tsk_safe_free(self->child_edge_map_tail);
    tsk_safe_free(self->segment_queue);
    tsk_safe_free(self->is_sample);
    tsk_safe_free(self->is_ancestor);
    tsk_safe_free(self->buffered_children);
    return 0;
}

static int TSK_WARN_UNUSED
ancestor_mapper_enqueue_segment(
    ancestor_mapper_t *self, double left, double right, tsk_id_t node)
{
    int ret = 0;
    tsk_segment_t *seg;
    void *p;

    tsk_bug_assert(left < right);
    /* Make sure we always have room for one more segment in the queue so we
     * can put a tail sentinel on it */
    if (self->segment_queue_size == self->max_segment_queue_size - 1) {
        self->max_segment_queue_size *= 2;
        p = tsk_realloc(self->segment_queue,
            self->max_segment_queue_size * sizeof(*self->segment_queue));
        if (p == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
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
ancestor_mapper_merge_ancestors(ancestor_mapper_t *self, tsk_id_t input_id)
{
    int ret = 0;
    tsk_segment_t **X, *x;
    tsk_size_t j, num_overlapping, num_flushed_edges;
    double left, right, prev_right;
    bool is_sample = self->is_sample[input_id];
    bool is_ancestor = self->is_ancestor[input_id];

    if (is_sample) {
        /* Free up the existing ancestry mapping. */
        x = self->ancestor_map_tail[input_id];
        tsk_bug_assert(x->left == 0 && x->right == self->sequence_length);
        self->ancestor_map_head[input_id] = NULL;
        self->ancestor_map_tail[input_id] = NULL;
    }
    ret = segment_overlapper_start(
        &self->segment_overlapper, self->segment_queue, self->segment_queue_size);
    if (ret != 0) {
        goto out;
    }

    prev_right = 0;
    while ((ret = segment_overlapper_next(
                &self->segment_overlapper, &left, &right, &X, &num_overlapping))
           == 1) {
        tsk_bug_assert(left < right);
        tsk_bug_assert(num_overlapping > 0);
        if (is_ancestor || is_sample) {
            for (j = 0; j < num_overlapping; j++) {
                ret = ancestor_mapper_record_edge(self, left, right, X[j]->node);
                if (ret != 0) {
                    goto out;
                }
            }
            ret = ancestor_mapper_add_ancestry(self, input_id, left, right, input_id);
            if (ret != 0) {
                goto out;
            }
            if (is_sample && left != prev_right) {
                /* Fill in any gaps in ancestry for the sample */
                ret = ancestor_mapper_add_ancestry(
                    self, input_id, prev_right, left, input_id);
                if (ret != 0) {
                    goto out;
                }
            }
        } else {
            for (j = 0; j < num_overlapping; j++) {
                ret = ancestor_mapper_add_ancestry(
                    self, input_id, left, right, X[j]->node);
                if (ret != 0) {
                    goto out;
                }
            }
        }
        prev_right = right;
    }
    if (is_sample && prev_right != self->tables->sequence_length) {
        /* If a trailing gap exists in the sample ancestry, fill it in. */
        ret = ancestor_mapper_add_ancestry(
            self, input_id, prev_right, self->sequence_length, input_id);
        if (ret != 0) {
            goto out;
        }
    }
    if (input_id != TSK_NULL) {
        ret = ancestor_mapper_flush_edges(self, input_id, &num_flushed_edges);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

static int TSK_WARN_UNUSED
ancestor_mapper_process_parent_edges(
    ancestor_mapper_t *self, tsk_id_t parent, tsk_size_t start, tsk_size_t end)
{
    int ret = 0;
    tsk_size_t j;
    tsk_segment_t *x;
    const tsk_edge_table_t *input_edges = &self->tables->edges;
    tsk_id_t child;
    double left, right;

    /* Go through the edges and queue up ancestry segments for processing. */
    self->segment_queue_size = 0;
    for (j = start; j < end; j++) {
        tsk_bug_assert(parent == input_edges->parent[j]);
        child = input_edges->child[j];
        left = input_edges->left[j];
        right = input_edges->right[j];
        // printf("C: %i, L: %f, R: %f\n", child, left, right);
        for (x = self->ancestor_map_head[child]; x != NULL; x = x->next) {
            if (x->right > left && right > x->left) {
                ret = ancestor_mapper_enqueue_segment(
                    self, TSK_MAX(x->left, left), TSK_MIN(x->right, right), x->node);
                if (ret != 0) {
                    goto out;
                }
            }
        }
    }
    // We can now merge the ancestral segments for the parent
    ret = ancestor_mapper_merge_ancestors(self, parent);
    if (ret != 0) {
        goto out;
    }

out:
    return ret;
}

static int TSK_WARN_UNUSED
ancestor_mapper_run(ancestor_mapper_t *self)
{
    int ret = 0;
    tsk_size_t j, start;
    tsk_id_t parent, current_parent;
    const tsk_edge_table_t *input_edges = &self->tables->edges;
    tsk_size_t num_edges = input_edges->num_rows;
    const double *node_time = self->tables->nodes.time;
    bool early_exit = false;

    if (num_edges > 0) {
        start = 0;
        current_parent = input_edges->parent[0];
        for (j = 0; j < num_edges; j++) {
            parent = input_edges->parent[j];
            if (parent != current_parent) {
                ret = ancestor_mapper_process_parent_edges(
                    self, current_parent, start, j);
                if (ret != 0) {
                    goto out;
                }
                start = j;
                current_parent = parent;
                if (node_time[current_parent] > self->oldest_node_time) {
                    early_exit = true;
                    break;
                }
            }
        }
        if (!early_exit) {
            /* If we didn't break out of the loop early, we need to still process
             * the final parent */
            ret = ancestor_mapper_process_parent_edges(self, current_parent, start, j);
            if (ret != 0) {
                goto out;
            }
        }
    }
out:
    return ret;
}

/*************************
 * IBD Segments
 *************************/

/* This maps two positive integers 0 <= a < b < N into the set
 * {0, ..., N^2}. For us to overflow an int64, N would need to
 * be > sqrt(2^63), ~3 * 10^9. The maximum value for a 32bit int
 * is ~2 * 10^9, so this can't happen here, however it is
 * theoretically possible with 64 bit IDs. It would require
 * a *very* large node table --- assuming 24 bytes per row
 * it would be at least 67GiB. To make sure this eventuality
 * doesn't happen, we have a tsk_bug_assert in the
 * tsk_identity_segments_init.
 */
static inline int64_t
pair_to_integer(tsk_id_t a, tsk_id_t b, tsk_size_t N)
{
    tsk_id_t tmp;
    if (a > b) {
        tmp = a;
        a = b;
        b = tmp;
    }
    return ((int64_t) a) * (int64_t) N + (int64_t) b;
}

static inline void
integer_to_pair(int64_t index, tsk_size_t N, tsk_id_t *a, tsk_id_t *b)
{
    *a = (tsk_id_t)(index / (int64_t) N);
    *b = (tsk_id_t)(index % (int64_t) N);
}

static int64_t
tsk_identity_segments_get_key(
    const tsk_identity_segments_t *self, tsk_id_t a, tsk_id_t b)
{
    int64_t ret;
    tsk_id_t N = (tsk_id_t) self->num_nodes;

    if (a < 0 || b < 0 || a >= N || b >= N) {
        ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
        goto out;
    }
    if (a == b) {
        ret = tsk_trace_error(TSK_ERR_SAME_NODES_IN_PAIR);
        goto out;
    }
    ret = pair_to_integer(a, b, self->num_nodes);
out:
    return ret;
}

static tsk_identity_segment_t *TSK_WARN_UNUSED
tsk_identity_segments_alloc_segment(
    tsk_identity_segments_t *self, double left, double right, tsk_id_t node)
{
    tsk_identity_segment_t *seg = tsk_blkalloc_get(&self->heap, sizeof(*seg));
    if (seg == NULL) {
        goto out;
    }
    tsk_bug_assert(left < right);
    tsk_bug_assert(node >= 0 && node < (tsk_id_t) self->num_nodes);

    seg->next = NULL;
    seg->left = left;
    seg->right = right;
    seg->node = node;
out:
    return seg;
}

static tsk_avl_node_int_t *
tsk_identity_segments_alloc_new_pair(tsk_identity_segments_t *self, int64_t key)
{
    tsk_avl_node_int_t *avl_node = tsk_blkalloc_get(&self->heap, sizeof(*avl_node));
    tsk_identity_segment_list_t *list = tsk_blkalloc_get(&self->heap, sizeof(*list));

    if (avl_node == NULL || list == NULL) {
        return NULL;
    }
    avl_node->key = key;
    avl_node->value = list;
    memset(list, 0, sizeof(*list));
    return avl_node;
}

/* Deliberately not making this a part of the public interface for now,
 * so we don't have to worry about the signature */
static int
tsk_identity_segments_init(
    tsk_identity_segments_t *self, tsk_size_t num_nodes, tsk_flags_t options)
{
    int ret = 0;
    /* Make sure we don't overflow in the ID mapping. See the comments in pair_to_integer
     * for details. */
    double max_num_nodes = sqrt(1ULL << 63);
    tsk_bug_assert((double) num_nodes < max_num_nodes);

    memset(self, 0, sizeof(*self));
    self->num_nodes = num_nodes;
    /* Storing segments implies storing pairs */
    if (options & TSK_IBD_STORE_SEGMENTS) {
        self->store_pairs = true;
        self->store_segments = true;
    } else if (options & TSK_IBD_STORE_PAIRS) {
        self->store_pairs = true;
    }
    ret = tsk_avl_tree_int_init(&self->pair_map);
    if (ret != 0) {
        goto out;
    }
    /* Allocate heap memory in 1MiB blocks */
    ret = tsk_blkalloc_init(&self->heap, 1024 * 1024);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

void
tsk_identity_segments_print_state(tsk_identity_segments_t *self, FILE *out)
{
    tsk_avl_node_int_t **nodes = tsk_malloc(self->pair_map.size * sizeof(*nodes));
    int64_t key;
    tsk_identity_segment_list_t *value;
    tsk_identity_segment_t *seg;
    tsk_size_t j;
    tsk_id_t a, b;

    tsk_bug_assert(nodes != NULL);

    fprintf(out, "===\nIBD Result\n===\n");
    fprintf(out, "total_span     = %f\n", self->total_span);
    fprintf(out, "num_segments   = %lld\n", (unsigned long long) self->num_segments);
    fprintf(out, "store_pairs    = %d\n", self->store_pairs);
    fprintf(out, "store_segments = %d\n", self->store_segments);
    if (self->store_pairs) {
        fprintf(out, "num_keys       = %d\n", (int) self->pair_map.size);
        tsk_avl_tree_int_ordered_nodes(&self->pair_map, nodes);
        for (j = 0; j < self->pair_map.size; j++) {
            key = nodes[j]->key;
            value = (tsk_identity_segment_list_t *) nodes[j]->value;
            integer_to_pair(key, self->num_nodes, &a, &b);
            fprintf(out, "%lld\t(%d,%d) n=%d total_span=%f\t", (long long) key, (int) a,
                (int) b, (int) value->num_segments, value->total_span);
            if (self->store_segments) {
                for (seg = value->head; seg != NULL; seg = seg->next) {
                    fprintf(
                        out, "(%f, %f)->%d, ", seg->left, seg->right, (int) seg->node);
                }
            }
            fprintf(out, "\n");
        }
    }
    fprintf(out, "Segment memory\n");
    tsk_blkalloc_print_state(&self->heap, out);
    tsk_safe_free(nodes);
}

tsk_size_t
tsk_identity_segments_get_num_segments(const tsk_identity_segments_t *self)
{
    return self->num_segments;
}

double
tsk_identity_segments_get_total_span(const tsk_identity_segments_t *self)
{
    return self->total_span;
}

tsk_size_t
tsk_identity_segments_get_num_pairs(const tsk_identity_segments_t *self)
{
    return self->pair_map.size;
}

/* Use an inorder traversal on the AVL tree to get the pairs in order.
 * Recursion is safe here because it's a balanced tree (see the AVL tree
 * code for notes on this).
 */
static int
get_keys_traverse(tsk_avl_node_int_t *node, int index, tsk_size_t N, tsk_id_t *pairs)
{
    tsk_id_t a, b;

    if (node == NULL) {
        return index;
    }
    index = get_keys_traverse(node->llink, index, N, pairs);
    integer_to_pair(node->key, N, &a, &b);
    pairs[2 * index] = a;
    pairs[2 * index + 1] = b;
    return get_keys_traverse(node->rlink, index + 1, N, pairs);
}

int
tsk_identity_segments_get_keys(const tsk_identity_segments_t *self, tsk_id_t *pairs)
{
    if (!self->store_pairs) {
        return TSK_ERR_IBD_PAIRS_NOT_STORED;
    }
    get_keys_traverse(
        tsk_avl_tree_int_get_root(&self->pair_map), 0, self->num_nodes, pairs);
    return 0;
}

static int
get_items_traverse(tsk_avl_node_int_t *node, int index, tsk_size_t N, tsk_id_t *pairs,
    tsk_identity_segment_list_t **lists)
{
    tsk_id_t a, b;

    if (node == NULL) {
        return index;
    }
    index = get_items_traverse(node->llink, index, N, pairs, lists);
    integer_to_pair(node->key, N, &a, &b);
    pairs[2 * index] = a;
    pairs[2 * index + 1] = b;
    lists[index] = node->value;
    return get_items_traverse(node->rlink, index + 1, N, pairs, lists);
}

int
tsk_identity_segments_get_items(const tsk_identity_segments_t *self, tsk_id_t *pairs,
    tsk_identity_segment_list_t **lists)
{
    if (!self->store_pairs) {
        return TSK_ERR_IBD_PAIRS_NOT_STORED;
    }
    get_items_traverse(
        tsk_avl_tree_int_get_root(&self->pair_map), 0, self->num_nodes, pairs, lists);
    return 0;
}

int
tsk_identity_segments_free(tsk_identity_segments_t *self)
{
    tsk_blkalloc_free(&self->heap);
    tsk_avl_tree_int_free(&self->pair_map);
    return 0;
}

static int TSK_WARN_UNUSED
tsk_identity_segments_update_pair(tsk_identity_segments_t *self, tsk_id_t a, tsk_id_t b,
    double left, double right, tsk_id_t node)
{
    int ret = 0;
    tsk_identity_segment_t *x;
    tsk_identity_segment_list_t *list;
    /* skip the error checking here since this an internal API */
    int64_t key = pair_to_integer(a, b, self->num_nodes);
    tsk_avl_node_int_t *avl_node = tsk_avl_tree_int_search(&self->pair_map, key);

    if (avl_node == NULL) {
        /* We haven't seen this pair before */
        avl_node = tsk_identity_segments_alloc_new_pair(self, key);
        if (avl_node == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
        ret = tsk_avl_tree_int_insert(&self->pair_map, avl_node);
        tsk_bug_assert(ret == 0);
    }
    list = (tsk_identity_segment_list_t *) avl_node->value;
    list->num_segments++;
    list->total_span += right - left;
    if (self->store_segments) {
        x = tsk_identity_segments_alloc_segment(self, left, right, node);
        if (x == NULL) {
            goto out;
        }
        if (list->tail == NULL) {
            list->head = x;
            list->tail = x;
        } else {
            list->tail->next = x;
            list->tail = x;
        }
    }
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_identity_segments_add_segment(tsk_identity_segments_t *self, tsk_id_t a, tsk_id_t b,
    double left, double right, tsk_id_t node)
{
    int ret = 0;

    if (self->store_pairs) {
        ret = tsk_identity_segments_update_pair(self, a, b, left, right, node);
        if (ret != 0) {
            goto out;
        }
    }
    self->total_span += right - left;
    self->num_segments++;
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_identity_segments_get(const tsk_identity_segments_t *self, tsk_id_t sample_a,
    tsk_id_t sample_b, tsk_identity_segment_list_t **ret_list)
{
    int ret = 0;
    int64_t key = tsk_identity_segments_get_key(self, sample_a, sample_b);
    tsk_avl_node_int_t *avl_node;

    if (key < 0) {
        ret = (int) key;
        goto out;
    }
    if (!self->store_pairs) {
        ret = tsk_trace_error(TSK_ERR_IBD_PAIRS_NOT_STORED);
        goto out;
    }
    avl_node = tsk_avl_tree_int_search(&self->pair_map, key);
    *ret_list = NULL;
    if (avl_node != NULL) {
        *ret_list = (tsk_identity_segment_list_t *) avl_node->value;
    }
out:
    return ret;
}

/*************************
 * IBD finder
 *************************/

typedef struct {
    tsk_identity_segments_t *result;
    double min_span;
    double max_time;
    const tsk_table_collection_t *tables;
    /* Maps nodes to their sample set IDs. Input samples map to set 0
     * in the "within" case. */
    tsk_id_t *sample_set_id;
    /* True if we're finding IBD between sample sets, false otherwise. */
    bool finding_between;
    tsk_segment_t **ancestor_map_head;
    tsk_segment_t **ancestor_map_tail;
    tsk_segment_t *segment_queue;
    tsk_size_t segment_queue_size;
    tsk_size_t max_segment_queue_size;
    tsk_blkalloc_t segment_heap;
} tsk_ibd_finder_t;

static tsk_segment_t *TSK_WARN_UNUSED
tsk_ibd_finder_alloc_segment(
    tsk_ibd_finder_t *self, double left, double right, tsk_id_t node)
{
    tsk_segment_t *seg = NULL;

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
static int TSK_WARN_UNUSED
tsk_ibd_finder_add_ancestry(tsk_ibd_finder_t *self, tsk_id_t input_id, double left,
    double right, tsk_id_t output_id)
{
    int ret = 0;
    tsk_segment_t *tail = self->ancestor_map_tail[input_id];
    tsk_segment_t *x = NULL;

    tsk_bug_assert(left < right);
    x = tsk_ibd_finder_alloc_segment(self, left, right, output_id);
    if (x == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    if (tail == NULL) {
        self->ancestor_map_head[input_id] = x;
        self->ancestor_map_tail[input_id] = x;
    } else {
        tail->next = x;
        self->ancestor_map_tail[input_id] = x;
    }
out:
    return ret;
}

static int
tsk_ibd_finder_init_samples_from_set(
    tsk_ibd_finder_t *self, const tsk_id_t *samples, tsk_size_t num_samples)
{
    int ret = 0;
    tsk_size_t j;
    tsk_id_t u;

    for (j = 0; j < num_samples; j++) {
        u = samples[j];

        if (u < 0 || u > (tsk_id_t) self->tables->nodes.num_rows) {
            ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
            goto out;
        }
        if (self->sample_set_id[u] != TSK_NULL) {
            ret = tsk_trace_error(TSK_ERR_DUPLICATE_SAMPLE);
            goto out;
        }
        self->sample_set_id[u] = 0;
    }
out:
    return ret;
}

static void
tsk_ibd_finder_init_samples_from_nodes(tsk_ibd_finder_t *self)
{
    tsk_id_t u;
    const tsk_id_t num_nodes = (tsk_id_t) self->tables->nodes.num_rows;
    const tsk_flags_t *restrict flags = self->tables->nodes.flags;

    for (u = 0; u < num_nodes; u++) {
        if (flags[u] & TSK_NODE_IS_SAMPLE) {
            self->sample_set_id[u] = 0;
        }
    }
}

static int
tsk_ibd_finder_add_sample_ancestry(tsk_ibd_finder_t *self)
{

    int ret = 0;
    tsk_id_t u;
    const tsk_id_t num_nodes = (tsk_id_t) self->tables->nodes.num_rows;
    const double L = self->tables->sequence_length;

    for (u = 0; u < num_nodes; u++) {
        if (self->sample_set_id[u] != TSK_NULL) {
            ret = tsk_ibd_finder_add_ancestry(self, u, 0, L, u);
            if (ret != 0) {
                goto out;
            }
        }
    }
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_ibd_finder_init(tsk_ibd_finder_t *self, const tsk_table_collection_t *tables,
    tsk_identity_segments_t *result, double min_span, double max_time)
{
    int ret = 0;
    tsk_size_t num_nodes;

    tsk_memset(self, 0, sizeof(tsk_ibd_finder_t));

    if (min_span < 0) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    if (max_time < 0) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }

    self->tables = tables;
    self->result = result;
    self->max_time = max_time;
    self->min_span = min_span;

    ret = tsk_blkalloc_init(&self->segment_heap, 8192);
    if (ret != 0) {
        goto out;
    }

    num_nodes = tables->nodes.num_rows;
    self->ancestor_map_head = tsk_calloc(num_nodes, sizeof(*self->ancestor_map_head));
    self->ancestor_map_tail = tsk_calloc(num_nodes, sizeof(*self->ancestor_map_tail));
    self->sample_set_id = tsk_malloc(num_nodes * sizeof(*self->sample_set_id));
    self->segment_queue_size = 0;
    self->max_segment_queue_size = 64;
    self->segment_queue
        = tsk_malloc(self->max_segment_queue_size * sizeof(*self->segment_queue));
    if (self->ancestor_map_head == NULL || self->ancestor_map_tail == NULL
        || self->sample_set_id == NULL || self->segment_queue == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memset(self->sample_set_id, TSK_NULL, num_nodes * sizeof(*self->sample_set_id));
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_ibd_finder_enqueue_segment(
    tsk_ibd_finder_t *self, double left, double right, tsk_id_t node)
{
    int ret = 0;
    tsk_segment_t *seg;
    void *p;

    if ((right - left) > self->min_span) {
        /* Make sure we always have room for one more segment in the queue so we
         * can put a tail sentinel on it */
        if (self->segment_queue_size == self->max_segment_queue_size - 1) {
            self->max_segment_queue_size *= 2;
            p = tsk_realloc(self->segment_queue,
                self->max_segment_queue_size * sizeof(*self->segment_queue));
            if (p == NULL) {
                ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
                goto out;
            }
            self->segment_queue = p;
        }
        seg = self->segment_queue + self->segment_queue_size;
        seg->left = left;
        seg->right = right;
        seg->node = node;
        self->segment_queue_size++;
    }
out:
    return ret;
}

static bool
tsk_ibd_finder_passes_filters(
    const tsk_ibd_finder_t *self, tsk_id_t a, tsk_id_t b, double left, double right)
{
    if (a == b) {
        return false;
    }
    if ((right - left) <= self->min_span) {
        return false;
    }
    if (self->finding_between) {
        return self->sample_set_id[a] != self->sample_set_id[b];
    } else {
        return true;
    }
}

static int TSK_WARN_UNUSED
tsk_ibd_finder_record_ibd(tsk_ibd_finder_t *self, tsk_id_t parent)
{
    int ret = 0;
    tsk_size_t j;
    tsk_segment_t *seg0, *seg1;
    double left, right;

    for (seg0 = self->ancestor_map_head[parent]; seg0 != NULL; seg0 = seg0->next) {
        for (j = 0; j < self->segment_queue_size; j++) {
            seg1 = &self->segment_queue[j];
            left = TSK_MAX(seg0->left, seg1->left);
            right = TSK_MIN(seg0->right, seg1->right);
            if (tsk_ibd_finder_passes_filters(
                    self, seg0->node, seg1->node, left, right)) {
                ret = tsk_identity_segments_add_segment(
                    self->result, seg0->node, seg1->node, left, right, parent);
                if (ret != 0) {
                    goto out;
                }
            }
        }
    }
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_ibd_finder_add_queued_ancestry(tsk_ibd_finder_t *self, tsk_id_t parent)
{
    int ret = 0;
    tsk_size_t j;
    tsk_segment_t seg;

    for (j = 0; j < self->segment_queue_size; j++) {
        seg = self->segment_queue[j];
        ret = tsk_ibd_finder_add_ancestry(self, parent, seg.left, seg.right, seg.node);
        if (ret != 0) {
            goto out;
        }
    }
    self->segment_queue_size = 0;
out:
    return ret;
}

static void
tsk_ibd_finder_print_state(tsk_ibd_finder_t *self, FILE *out)
{
    tsk_size_t j;
    tsk_segment_t *u = NULL;

    fprintf(out, "--ibd-finder stats--\n");
    fprintf(out, "max_time = %f\n", self->max_time);
    fprintf(out, "min_span = %f\n", self->min_span);
    fprintf(out, "finding_between = %d\n", self->finding_between);
    fprintf(out, "===\nEdges\n===\n");
    for (j = 0; j < self->tables->edges.num_rows; j++) {
        fprintf(out, "L:%f, R:%f, P:%lld, C:%lld\n", self->tables->edges.left[j],
            self->tables->edges.right[j], (long long) self->tables->edges.parent[j],
            (long long) self->tables->edges.child[j]);
    }
    fprintf(out, "===\nNodes\n===\n");
    for (j = 0; j < self->tables->nodes.num_rows; j++) {
        fprintf(out, "ID:%d, Time:%f, Flag:%lld Sample set:%d\n", (int) j,
            self->tables->nodes.time[j], (long long) self->tables->nodes.flags[j],
            (int) self->sample_set_id[j]);
    }
    fprintf(out, "===\nAncestral map\n===\n");
    for (j = 0; j < self->tables->nodes.num_rows; j++) {
        fprintf(out, "Node %lld: ", (long long) j);
        for (u = self->ancestor_map_head[j]; u != NULL; u = u->next) {
            fprintf(out, "(%f,%f->%lld)", u->left, u->right, (long long) u->node);
        }
        fprintf(out, "\n");
    }
    tsk_identity_segments_print_state(self->result, out);
}

static int TSK_WARN_UNUSED
tsk_ibd_finder_init_within(
    tsk_ibd_finder_t *self, const tsk_id_t *samples, tsk_size_t num_samples)
{
    int ret;

    if (samples == NULL) {
        tsk_ibd_finder_init_samples_from_nodes(self);
    } else {
        ret = tsk_ibd_finder_init_samples_from_set(self, samples, num_samples);
        if (ret != 0) {
            goto out;
        }
    }
    self->finding_between = false;
    ret = tsk_ibd_finder_add_sample_ancestry(self);
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_ibd_finder_init_between(tsk_ibd_finder_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets)
{
    int ret = 0;
    tsk_size_t j, k, index;
    tsk_id_t u;

    index = 0;
    for (j = 0; j < num_sample_sets; j++) {
        for (k = 0; k < sample_set_sizes[j]; k++) {
            u = sample_sets[index];
            if (u < 0 || u > (tsk_id_t) self->tables->nodes.num_rows) {
                ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
                goto out;
            }
            if (self->sample_set_id[u] != TSK_NULL) {
                ret = tsk_trace_error(TSK_ERR_DUPLICATE_SAMPLE);
                goto out;
            }
            self->sample_set_id[u] = (tsk_id_t) j;
            index++;
        }
    }
    self->finding_between = true;
    ret = tsk_ibd_finder_add_sample_ancestry(self);
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_ibd_finder_run(tsk_ibd_finder_t *self)
{
    const tsk_edge_table_t *input_edges = &self->tables->edges;
    const tsk_size_t num_edges = input_edges->num_rows;
    int ret = 0;
    tsk_size_t j;
    tsk_segment_t *s;
    tsk_id_t parent, child;
    double left, right, intvl_l, intvl_r, time;

    for (j = 0; j < num_edges; j++) {
        parent = input_edges->parent[j];
        left = input_edges->left[j];
        right = input_edges->right[j];
        child = input_edges->child[j];
        time = self->tables->nodes.time[parent];
        if (time > self->max_time) {
            break;
        }

        for (s = self->ancestor_map_head[child]; s != NULL; s = s->next) {
            intvl_l = TSK_MAX(left, s->left);
            intvl_r = TSK_MIN(right, s->right);
            ret = tsk_ibd_finder_enqueue_segment(self, intvl_l, intvl_r, s->node);
            if (ret != 0) {
                goto out;
            }
        }
        ret = tsk_ibd_finder_record_ibd(self, parent);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_ibd_finder_add_queued_ancestry(self, parent);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

static int
tsk_ibd_finder_free(tsk_ibd_finder_t *self)
{
    tsk_blkalloc_free(&self->segment_heap);
    tsk_safe_free(self->sample_set_id);
    tsk_safe_free(self->ancestor_map_head);
    tsk_safe_free(self->ancestor_map_tail);
    tsk_safe_free(self->segment_queue);
    return 0;
}

/*************************
 * simplifier
 *************************/

static void
simplifier_check_state(simplifier_t *self)
{
    tsk_size_t j, k;
    tsk_segment_t *u;
    mutation_id_list_t *list_node;
    tsk_id_t site;
    interval_list_t *int_list;
    tsk_id_t child;
    double position, last_position;
    bool found;
    tsk_size_t num_intervals;

    for (j = 0; j < self->input_tables.nodes.num_rows; j++) {
        tsk_bug_assert((self->ancestor_map_head[j] == NULL)
                       == (self->ancestor_map_tail[j] == NULL));
        for (u = self->ancestor_map_head[j]; u != NULL; u = u->next) {
            tsk_bug_assert(u->left < u->right);
            if (u->next != NULL) {
                tsk_bug_assert(u->right <= u->next->left);
                if (u->right == u->next->left) {
                    tsk_bug_assert(u->node != u->next->node);
                }
            } else {
                tsk_bug_assert(u == self->ancestor_map_tail[j]);
            }
        }
    }

    for (j = 0; j < self->segment_queue_size; j++) {
        tsk_bug_assert(self->segment_queue[j].left < self->segment_queue[j].right);
    }

    for (j = 0; j < self->input_tables.nodes.num_rows; j++) {
        last_position = -1;
        for (list_node = self->node_mutation_list_map_head[j]; list_node != NULL;
             list_node = list_node->next) {
            tsk_bug_assert(
                self->input_tables.mutations.node[list_node->mutation] == (tsk_id_t) j);
            site = self->input_tables.mutations.site[list_node->mutation];
            position = self->input_tables.sites.position[site];
            tsk_bug_assert(last_position <= position);
            last_position = position;
        }
    }

    /* check the buffered edges */
    for (j = 0; j < self->input_tables.nodes.num_rows; j++) {
        tsk_bug_assert((self->child_edge_map_head[j] == NULL)
                       == (self->child_edge_map_tail[j] == NULL));
        if (self->child_edge_map_head[j] != NULL) {
            /* Make sure that the child is in our list */
            found = false;
            for (k = 0; k < self->num_buffered_children; k++) {
                if (self->buffered_children[k] == (tsk_id_t) j) {
                    found = true;
                    break;
                }
            }
            tsk_bug_assert(found);
        }
    }
    num_intervals = 0;
    for (j = 0; j < self->num_buffered_children; j++) {
        child = self->buffered_children[j];
        tsk_bug_assert(self->child_edge_map_head[child] != NULL);
        for (int_list = self->child_edge_map_head[child]; int_list != NULL;
             int_list = int_list->next) {
            tsk_bug_assert(int_list->left < int_list->right);
            if (int_list->next != NULL) {
                tsk_bug_assert(int_list->right < int_list->next->left);
            }
            num_intervals++;
        }
    }
    tsk_bug_assert(
        num_intervals
        == self->interval_list_heap.total_allocated / (sizeof(interval_list_t)));
}

static void
print_segment_chain(tsk_segment_t *head, FILE *out)
{
    tsk_segment_t *u;

    for (u = head; u != NULL; u = u->next) {
        fprintf(out, "(%f,%f->%lld)", u->left, u->right, (long long) u->node);
    }
}

static void
simplifier_print_state(simplifier_t *self, FILE *out)
{
    tsk_size_t j;
    tsk_segment_t *u;
    mutation_id_list_t *list_node;
    interval_list_t *int_list;
    tsk_id_t child;

    fprintf(out, "--simplifier state--\n");
    fprintf(out, "options:\n");
    fprintf(out, "\tfilter_unreferenced_sites   : %d\n",
        !!(self->options & TSK_SIMPLIFY_FILTER_SITES));
    fprintf(out, "\tno_filter_nodes   : %d\n",
        !!(self->options & TSK_SIMPLIFY_NO_FILTER_NODES));
    fprintf(out, "\treduce_to_site_topology : %d\n",
        !!(self->options & TSK_SIMPLIFY_REDUCE_TO_SITE_TOPOLOGY));
    fprintf(out, "\tkeep_unary              : %d\n",
        !!(self->options & TSK_SIMPLIFY_KEEP_UNARY));
    fprintf(out, "\tkeep_input_roots        : %d\n",
        !!(self->options & TSK_SIMPLIFY_KEEP_INPUT_ROOTS));
    fprintf(out, "\tkeep_unary_in_individuals : %d\n",
        !!(self->options & TSK_SIMPLIFY_KEEP_UNARY_IN_INDIVIDUALS));

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
        fprintf(out, "%lld:\t", (long long) j);
        print_segment_chain(self->ancestor_map_head[j], out);
        fprintf(out, "\n");
    }
    fprintf(out, "===\nnode_id map (input->output)\n==\n");
    for (j = 0; j < self->input_tables.nodes.num_rows; j++) {
        if (self->node_id_map[j] != TSK_NULL) {
            fprintf(
                out, "%lld->%lld\n", (long long) j, (long long) self->node_id_map[j]);
        }
    }
    fprintf(out, "===\nsegment queue\n==\n");
    for (j = 0; j < self->segment_queue_size; j++) {
        u = &self->segment_queue[j];
        fprintf(out, "(%f,%f->%lld)", u->left, u->right, (long long) u->node);
        fprintf(out, "\n");
    }
    fprintf(out, "===\nbuffered children\n==\n");
    for (j = 0; j < self->num_buffered_children; j++) {
        child = self->buffered_children[j];
        fprintf(out, "%lld -> ", (long long) j);
        for (int_list = self->child_edge_map_head[child]; int_list != NULL;
             int_list = int_list->next) {
            fprintf(out, "(%f, %f), ", int_list->left, int_list->right);
        }
        fprintf(out, "\n");
    }
    fprintf(out, "===\nmutation node map\n==\n");
    for (j = 0; j < self->input_tables.mutations.num_rows; j++) {
        fprintf(out, "%lld\t-> %lld\n", (long long) j,
            (long long) self->mutation_node_map[j]);
    }
    fprintf(out, "===\nnode mutation id list map\n==\n");
    for (j = 0; j < self->input_tables.nodes.num_rows; j++) {
        if (self->node_mutation_list_map_head[j] != NULL) {
            fprintf(out, "%lld\t-> [", (long long) j);
            for (list_node = self->node_mutation_list_map_head[j]; list_node != NULL;
                 list_node = list_node->next) {
                fprintf(out, "%lld,", (long long) list_node->mutation);
            }
            fprintf(out, "]\n");
        }
    }
    if (!!(self->options & TSK_SIMPLIFY_REDUCE_TO_SITE_TOPOLOGY)) {
        fprintf(out, "===\nposition_lookup\n==\n");
        for (j = 0; j < self->input_tables.sites.num_rows + 2; j++) {
            fprintf(out, "%lld\t-> %f\n", (long long) j, self->position_lookup[j]);
        }
    }
    simplifier_check_state(self);
}

static tsk_segment_t *TSK_WARN_UNUSED
simplifier_alloc_segment(simplifier_t *self, double left, double right, tsk_id_t node)
{
    tsk_segment_t *seg = NULL;

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

static interval_list_t *TSK_WARN_UNUSED
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
static tsk_id_t TSK_WARN_UNUSED
simplifier_record_node(simplifier_t *self, tsk_id_t input_id)
{
    tsk_node_t node;
    bool update_flags = !(self->options & TSK_SIMPLIFY_NO_UPDATE_SAMPLE_FLAGS);

    tsk_node_table_get_row_unsafe(&self->input_tables.nodes, (tsk_id_t) input_id, &node);
    if (update_flags) {
        /* Zero out the sample bit */
        node.flags &= (tsk_flags_t) ~TSK_NODE_IS_SAMPLE;
        if (self->is_sample[input_id]) {
            node.flags |= TSK_NODE_IS_SAMPLE;
        }
    }
    self->node_id_map[input_id] = (tsk_id_t) self->tables->nodes.num_rows;
    return tsk_node_table_add_row(&self->tables->nodes, node.flags, node.time,
        node.population, node.individual, node.metadata, node.metadata_length);
}

/* Remove the mapping for the last recorded node. */
static int
simplifier_rewind_node(simplifier_t *self, tsk_id_t input_id, tsk_id_t output_id)
{
    self->node_id_map[input_id] = TSK_NULL;
    return tsk_node_table_truncate(&self->tables->nodes, (tsk_size_t) output_id);
}

static int
simplifier_flush_edges(simplifier_t *self, tsk_id_t parent, tsk_size_t *ret_num_edges)
{
    int ret = 0;
    tsk_id_t ret_id;
    tsk_size_t j;
    tsk_id_t child;
    interval_list_t *x;
    tsk_size_t num_edges = 0;

    qsort(self->buffered_children, (size_t) self->num_buffered_children,
        sizeof(tsk_id_t), cmp_node_id);
    for (j = 0; j < self->num_buffered_children; j++) {
        child = self->buffered_children[j];
        for (x = self->child_edge_map_head[child]; x != NULL; x = x->next) {
            ret_id = tsk_edge_table_add_row(
                &self->tables->edges, x->left, x->right, parent, child, NULL, 0);
            if (ret_id < 0) {
                ret = (int) ret_id;
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
    tsk_size_t num_sites = self->input_tables.sites.num_rows;

    self->position_lookup = tsk_malloc((num_sites + 2) * sizeof(*self->position_lookup));
    if (self->position_lookup == NULL) {
        goto out;
    }
    self->position_lookup[0] = 0;
    self->position_lookup[num_sites + 1] = self->input_tables.sequence_length;
    tsk_memcpy(self->position_lookup + 1, self->input_tables.sites.position,
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
    tsk_size_t N = self->input_tables.sites.num_rows + 2;
    tsk_size_t left_index, right_index;
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

    if (self->options & TSK_SIMPLIFY_REDUCE_TO_SITE_TOPOLOGY) {
        skip = simplifier_map_reduced_coordinates(self, &left, &right);
        /* NOTE: we exit early here when reduce_coordindates has told us to
         * skip this edge, as it is not visible in the reduced tree sequence */
        if (skip) {
            goto out;
        }
    }

    tail = self->child_edge_map_tail[child];
    if (tail == NULL) {
        tsk_bug_assert(self->num_buffered_children < self->input_tables.nodes.num_rows);
        self->buffered_children[self->num_buffered_children] = child;
        self->num_buffered_children++;
        x = simplifier_alloc_interval_list(self, left, right);
        if (x == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
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
                ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
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
    tsk_size_t j;

    self->mutation_node_map
        = tsk_calloc(self->input_tables.mutations.num_rows, sizeof(tsk_id_t));
    self->node_mutation_list_mem
        = tsk_malloc(self->input_tables.mutations.num_rows * sizeof(mutation_id_list_t));
    self->node_mutation_list_map_head
        = tsk_calloc(self->input_tables.nodes.num_rows, sizeof(mutation_id_list_t *));
    self->node_mutation_list_map_tail
        = tsk_calloc(self->input_tables.nodes.num_rows, sizeof(mutation_id_list_t *));
    if (self->mutation_node_map == NULL || self->node_mutation_list_mem == NULL
        || self->node_mutation_list_map_head == NULL
        || self->node_mutation_list_map_tail == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memset(self->mutation_node_map, 0xff,
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

static void
simplifier_map_mutations(
    simplifier_t *self, tsk_id_t input_id, double left, double right, tsk_id_t output_id)
{
    mutation_id_list_t *m_node;
    double position;
    tsk_id_t site;

    m_node = self->node_mutation_list_map_head[input_id];
    while (m_node != NULL) {
        site = self->input_tables.mutations.site[m_node->mutation];
        position = self->input_tables.sites.position[site];
        if (left <= position && position < right) {
            self->mutation_node_map[m_node->mutation] = output_id;
        }
        m_node = m_node->next;
    }
}

static int TSK_WARN_UNUSED
simplifier_add_ancestry(
    simplifier_t *self, tsk_id_t input_id, double left, double right, tsk_id_t output_id)
{
    int ret = 0;
    tsk_segment_t *tail = self->ancestor_map_tail[input_id];
    tsk_segment_t *x;

    tsk_bug_assert(left < right);
    if (tail == NULL) {
        x = simplifier_alloc_segment(self, left, right, output_id);
        if (x == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
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
                ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
                goto out;
            }
            tail->next = x;
            self->ancestor_map_tail[input_id] = x;
        }
    }
    simplifier_map_mutations(self, input_id, left, right, output_id);
out:
    return ret;
}

/* Sets up the internal working copies of the various tables, as needed
 * depending on the specified options. */
static int
simplifier_init_tables(simplifier_t *self)
{
    int ret;
    bool filter_nodes = !(self->options & TSK_SIMPLIFY_NO_FILTER_NODES);
    bool filter_populations = self->options & TSK_SIMPLIFY_FILTER_POPULATIONS;
    bool filter_individuals = self->options & TSK_SIMPLIFY_FILTER_INDIVIDUALS;
    bool filter_sites = self->options & TSK_SIMPLIFY_FILTER_SITES;
    tsk_bookmark_t rows_to_retain;

    /* NOTE: this is a bit inefficient here as we're taking copies of
     * the tables even in the no-filter case where the original tables
     * won't be touched (beyond references to external tables that may
     * need updating). Future versions may do something a bit more
     * complicated like temporarily stealing the pointers to the
     * underlying column memory in these tables, and then being careful
     * not to free the table at the end.
     */
    ret = tsk_table_collection_copy(self->tables, &self->input_tables, 0);
    if (ret != 0) {
        goto out;
    }
    memset(&rows_to_retain, 0, sizeof(rows_to_retain));
    rows_to_retain.provenances = self->tables->provenances.num_rows;
    if (!filter_nodes) {
        rows_to_retain.nodes = self->tables->nodes.num_rows;
    }
    if (!filter_populations) {
        rows_to_retain.populations = self->tables->populations.num_rows;
    }
    if (!filter_individuals) {
        rows_to_retain.individuals = self->tables->individuals.num_rows;
    }
    if (!filter_sites) {
        rows_to_retain.sites = self->tables->sites.num_rows;
    }

    ret = tsk_table_collection_truncate(self->tables, &rows_to_retain);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

static int
simplifier_init_nodes(simplifier_t *self, const tsk_id_t *samples)
{
    int ret = 0;
    tsk_id_t node_id;
    tsk_size_t j;
    const tsk_size_t num_nodes = self->input_tables.nodes.num_rows;
    bool filter_nodes = !(self->options & TSK_SIMPLIFY_NO_FILTER_NODES);
    bool update_flags = !(self->options & TSK_SIMPLIFY_NO_UPDATE_SAMPLE_FLAGS);
    tsk_flags_t *node_flags = self->tables->nodes.flags;
    tsk_id_t *node_id_map = self->node_id_map;

    if (filter_nodes) {
        tsk_bug_assert(self->tables->nodes.num_rows == 0);
        /* The node table has been cleared. Add nodes for the samples. */
        for (j = 0; j < self->num_samples; j++) {
            node_id = simplifier_record_node(self, samples[j]);
            if (node_id < 0) {
                ret = (int) node_id;
                goto out;
            }
        }
    } else {
        tsk_bug_assert(self->tables->nodes.num_rows == num_nodes);
        if (update_flags) {
            for (j = 0; j < num_nodes; j++) {
                /* Reset the sample flags */
                node_flags[j] &= (tsk_flags_t) ~TSK_NODE_IS_SAMPLE;
                if (self->is_sample[j]) {
                    node_flags[j] |= TSK_NODE_IS_SAMPLE;
                }
            }
        }

        for (j = 0; j < num_nodes; j++) {
            node_id_map[j] = (tsk_id_t) j;
        }
    }
    /* Add the initial ancestry */
    for (j = 0; j < self->num_samples; j++) {
        node_id = samples[j];
        ret = simplifier_add_ancestry(self, node_id, 0,
            self->input_tables.sequence_length, self->node_id_map[node_id]);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

static int
simplifier_init(simplifier_t *self, const tsk_id_t *samples, tsk_size_t num_samples,
    tsk_table_collection_t *tables, tsk_flags_t options)
{
    int ret = 0;
    tsk_size_t j;
    tsk_id_t ret_id;
    tsk_size_t num_nodes;

    tsk_memset(self, 0, sizeof(simplifier_t));
    self->num_samples = num_samples;
    self->options = options;
    self->tables = tables;

    /* TODO we can add a flag to skip these checks for when we know they are
     * unnecessary */
    /* TODO Current unit tests require TSK_CHECK_SITE_DUPLICATES but it's
     * debateable whether we need it. If we remove, we definitely need explicit
     * tests to ensure we're doing sensible things with duplicate sites.
     * (Particularly, re TSK_SIMPLIFY_REDUCE_TO_SITE_TOPOLOGY.) */
    ret_id = tsk_table_collection_check_integrity(tables,
        TSK_CHECK_EDGE_ORDERING | TSK_CHECK_SITE_ORDERING | TSK_CHECK_SITE_DUPLICATES);
    if (ret_id != 0) {
        ret = (int) ret_id;
        goto out;
    }

    /* Allocate the heaps used for small objects-> Assuming 8K is a good chunk size
     */
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
    num_nodes = tables->nodes.num_rows;
    /* Make the maps and set the intial state */
    self->ancestor_map_head = tsk_calloc(num_nodes, sizeof(tsk_segment_t *));
    self->ancestor_map_tail = tsk_calloc(num_nodes, sizeof(tsk_segment_t *));
    self->child_edge_map_head = tsk_calloc(num_nodes, sizeof(interval_list_t *));
    self->child_edge_map_tail = tsk_calloc(num_nodes, sizeof(interval_list_t *));
    self->node_id_map = tsk_malloc(num_nodes * sizeof(tsk_id_t));
    self->buffered_children = tsk_malloc(num_nodes * sizeof(tsk_id_t));
    self->is_sample = tsk_calloc(num_nodes, sizeof(bool));
    self->max_segment_queue_size = 64;
    self->segment_queue
        = tsk_malloc(self->max_segment_queue_size * sizeof(tsk_segment_t));
    if (self->ancestor_map_head == NULL || self->ancestor_map_tail == NULL
        || self->child_edge_map_head == NULL || self->child_edge_map_tail == NULL
        || self->node_id_map == NULL || self->is_sample == NULL
        || self->segment_queue == NULL || self->buffered_children == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    /* Go through the samples to check for errors before we clear the tables. */
    for (j = 0; j < self->num_samples; j++) {
        if (samples[j] < 0 || samples[j] >= (tsk_id_t) num_nodes) {
            ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
            goto out;
        }
        if (self->is_sample[samples[j]]) {
            ret = tsk_trace_error(TSK_ERR_DUPLICATE_SAMPLE);
            goto out;
        }
        self->is_sample[samples[j]] = true;
    }
    tsk_memset(self->node_id_map, 0xff, num_nodes * sizeof(tsk_id_t));

    ret = simplifier_init_tables(self);
    if (ret != 0) {
        goto out;
    }
    ret = simplifier_init_sites(self);
    if (ret != 0) {
        goto out;
    }
    ret = simplifier_init_nodes(self, samples);
    if (ret != 0) {
        goto out;
    }
    if (self->options & TSK_SIMPLIFY_REDUCE_TO_SITE_TOPOLOGY) {
        ret = simplifier_init_position_lookup(self);
        if (ret != 0) {
            goto out;
        }
    }

    self->edge_sort_offset = TSK_NULL;
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
    tsk_safe_free(self->ancestor_map_head);
    tsk_safe_free(self->ancestor_map_tail);
    tsk_safe_free(self->child_edge_map_head);
    tsk_safe_free(self->child_edge_map_tail);
    tsk_safe_free(self->node_id_map);
    tsk_safe_free(self->segment_queue);
    tsk_safe_free(self->is_sample);
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
    tsk_segment_t *seg;
    void *p;

    tsk_bug_assert(left < right);
    /* Make sure we always have room for one more segment in the queue so we
     * can put a tail sentinel on it */
    if (self->segment_queue_size == self->max_segment_queue_size - 1) {
        self->max_segment_queue_size *= 2;
        p = tsk_realloc(self->segment_queue,
            self->max_segment_queue_size * sizeof(*self->segment_queue));
        if (p == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
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
    tsk_segment_t **X, *x;
    tsk_size_t j, num_overlapping, num_flushed_edges;
    double left, right, prev_right;
    tsk_id_t ancestry_node;
    tsk_id_t output_id = self->node_id_map[input_id];
    bool is_sample = self->is_sample[input_id];
    bool filter_nodes = !(self->options & TSK_SIMPLIFY_NO_FILTER_NODES);
    bool keep_unary = self->options & TSK_SIMPLIFY_KEEP_UNARY;

    if ((self->options & TSK_SIMPLIFY_KEEP_UNARY_IN_INDIVIDUALS)
        && (self->input_tables.nodes.individual[input_id] != TSK_NULL)) {
        keep_unary = true;
    }

    if (is_sample) {
        /* Free up the existing ancestry mapping. */
        x = self->ancestor_map_tail[input_id];
        tsk_bug_assert(x->left == 0 && x->right == self->tables->sequence_length);
        self->ancestor_map_head[input_id] = NULL;
        self->ancestor_map_tail[input_id] = NULL;
    }

    ret = segment_overlapper_start(
        &self->segment_overlapper, self->segment_queue, self->segment_queue_size);
    if (ret != 0) {
        goto out;
    }
    prev_right = 0;
    while ((ret = segment_overlapper_next(
                &self->segment_overlapper, &left, &right, &X, &num_overlapping))
           == 1) {
        tsk_bug_assert(left < right);
        tsk_bug_assert(num_overlapping > 0);
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
                    output_id = simplifier_record_node(self, input_id);
                }
                ret = simplifier_record_edge(self, left, right, ancestry_node);
                if (ret != 0) {
                    goto out;
                }
            }
        } else {
            if (output_id == TSK_NULL) {
                output_id = simplifier_record_node(self, input_id);
                if (output_id < 0) {
                    ret = (int) output_id;
                    goto out;
                }
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
        ret = simplifier_add_ancestry(
            self, input_id, prev_right, self->tables->sequence_length, output_id);
        if (ret != 0) {
            goto out;
        }
    }
    if (output_id != TSK_NULL) {
        ret = simplifier_flush_edges(self, output_id, &num_flushed_edges);
        if (ret != 0) {
            goto out;
        }
        if (filter_nodes && (num_flushed_edges == 0) && !is_sample) {
            ret = simplifier_rewind_node(self, input_id, output_id);
        }
    }
out:
    return ret;
}

/* Extract the ancestry for the specified input node over the specified
 * interval and queue it up for merging.
 */
static int TSK_WARN_UNUSED
simplifier_extract_ancestry(
    simplifier_t *self, double left, double right, tsk_id_t input_id)
{
    int ret = 0;
    tsk_segment_t *x = self->ancestor_map_head[input_id];
    tsk_segment_t y; /* y is the segment that has been removed */
    tsk_segment_t *x_head, *x_prev, *seg_left, *seg_right;

    x_head = NULL;
    x_prev = NULL;
    while (x != NULL) {
        if (x->right > left && right > x->left) {
            y.left = TSK_MAX(x->left, left);
            y.right = TSK_MIN(x->right, right);
            y.node = x->node;
            ret = simplifier_enqueue_segment(self, y.left, y.right, y.node);
            if (ret != 0) {
                goto out;
            }
            seg_left = NULL;
            seg_right = NULL;
            if (x->left != y.left) {
                seg_left = simplifier_alloc_segment(self, x->left, y.left, x->node);
                if (seg_left == NULL) {
                    ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
                    goto out;
                }
                if (x_prev == NULL) {
                    x_head = seg_left;
                } else {
                    x_prev->next = seg_left;
                }
                x_prev = seg_left;
            }
            if (x->right != y.right) {
                x->left = y.right;
                seg_right = x;
            } else {
                seg_right = x->next;
                // TODO free x
            }
            if (x_prev == NULL) {
                x_head = seg_right;
            } else {
                x_prev->next = seg_right;
            }
            x = seg_right;
        } else {
            if (x_prev == NULL) {
                x_head = x;
            }
            x_prev = x;
            x = x->next;
        }
    }

    self->ancestor_map_head[input_id] = x_head;
    self->ancestor_map_tail[input_id] = x_prev;
out:
    return ret;
}

static int TSK_WARN_UNUSED
simplifier_process_parent_edges(
    simplifier_t *self, tsk_id_t parent, tsk_size_t start, tsk_size_t end)
{
    int ret = 0;
    tsk_size_t j;
    const tsk_edge_table_t *input_edges = &self->input_tables.edges;
    tsk_id_t child;
    double left, right;

    /* Go through the edges and queue up ancestry segments for processing. */
    self->segment_queue_size = 0;
    for (j = start; j < end; j++) {
        tsk_bug_assert(parent == input_edges->parent[j]);
        child = input_edges->child[j];
        left = input_edges->left[j];
        right = input_edges->right[j];
        ret = simplifier_extract_ancestry(self, left, right, child);
        if (ret != 0) {
            goto out;
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
simplifier_finalise_site_references(
    simplifier_t *self, const bool *site_referenced, tsk_id_t *site_id_map)
{
    int ret = 0;
    tsk_id_t ret_id;
    tsk_size_t j;
    tsk_site_t site;
    const tsk_size_t num_sites = self->input_tables.sites.num_rows;

    if (self->options & TSK_SIMPLIFY_FILTER_SITES) {
        for (j = 0; j < num_sites; j++) {
            tsk_site_table_get_row_unsafe(
                &self->input_tables.sites, (tsk_id_t) j, &site);
            site_id_map[j] = TSK_NULL;
            if (site_referenced[j]) {
                ret_id = tsk_site_table_add_row(&self->tables->sites, site.position,
                    site.ancestral_state, site.ancestral_state_length, site.metadata,
                    site.metadata_length);
                if (ret_id < 0) {
                    ret = (int) ret_id;
                    goto out;
                }
                site_id_map[j] = ret_id;
            }
        }
    } else {
        tsk_bug_assert(self->tables->sites.num_rows == num_sites);
        for (j = 0; j < num_sites; j++) {
            site_id_map[j] = (tsk_id_t) j;
        }
    }
out:
    return ret;
}

static int TSK_WARN_UNUSED
simplifier_finalise_population_references(simplifier_t *self)
{
    int ret = 0;
    tsk_size_t j;
    tsk_id_t pop_id, ret_id;
    tsk_population_t pop;
    tsk_id_t *node_population = self->tables->nodes.population;
    const tsk_size_t num_nodes = self->tables->nodes.num_rows;
    const tsk_size_t num_populations = self->input_tables.populations.num_rows;
    bool *population_referenced
        = tsk_calloc(num_populations, sizeof(*population_referenced));
    tsk_id_t *population_id_map
        = tsk_malloc(num_populations * sizeof(*population_id_map));

    tsk_bug_assert(self->options & TSK_SIMPLIFY_FILTER_POPULATIONS);

    if (population_referenced == NULL || population_id_map == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    for (j = 0; j < num_nodes; j++) {
        pop_id = node_population[j];
        if (pop_id != TSK_NULL) {
            population_referenced[pop_id] = true;
        }
    }

    for (j = 0; j < num_populations; j++) {
        tsk_population_table_get_row_unsafe(
            &self->input_tables.populations, (tsk_id_t) j, &pop);
        population_id_map[j] = TSK_NULL;
        if (population_referenced[j]) {
            ret_id = tsk_population_table_add_row(
                &self->tables->populations, pop.metadata, pop.metadata_length);
            if (ret_id < 0) {
                ret = (int) ret_id;
                goto out;
            }
            population_id_map[j] = ret_id;
        }
    }

    /* Remap the IDs in the node table */
    for (j = 0; j < num_nodes; j++) {
        pop_id = node_population[j];
        if (pop_id != TSK_NULL) {
            node_population[j] = population_id_map[pop_id];
        }
    }
out:
    tsk_safe_free(population_id_map);
    tsk_safe_free(population_referenced);
    return ret;
}

static int TSK_WARN_UNUSED
simplifier_finalise_individual_references(simplifier_t *self)
{
    int ret = 0;
    tsk_size_t j;
    tsk_id_t pop_id, ret_id;
    tsk_individual_t ind;
    tsk_id_t *node_individual = self->tables->nodes.individual;
    tsk_id_t *parents;
    const tsk_size_t num_nodes = self->tables->nodes.num_rows;
    const tsk_size_t num_individuals = self->input_tables.individuals.num_rows;
    bool *individual_referenced
        = tsk_calloc(num_individuals, sizeof(*individual_referenced));
    tsk_id_t *individual_id_map
        = tsk_malloc(num_individuals * sizeof(*individual_id_map));

    tsk_bug_assert(self->options & TSK_SIMPLIFY_FILTER_INDIVIDUALS);

    if (individual_referenced == NULL || individual_id_map == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    for (j = 0; j < num_nodes; j++) {
        pop_id = node_individual[j];
        if (pop_id != TSK_NULL) {
            individual_referenced[pop_id] = true;
        }
    }

    for (j = 0; j < num_individuals; j++) {
        tsk_individual_table_get_row_unsafe(
            &self->input_tables.individuals, (tsk_id_t) j, &ind);
        individual_id_map[j] = TSK_NULL;
        if (individual_referenced[j]) {
            /* Can't remap the parents inline here because we have no
             * guarantees about sortedness */
            ret_id = tsk_individual_table_add_row(&self->tables->individuals, ind.flags,
                ind.location, ind.location_length, ind.parents, ind.parents_length,
                ind.metadata, ind.metadata_length);
            if (ret_id < 0) {
                ret = (int) ret_id;
                goto out;
            }
            individual_id_map[j] = ret_id;
        }
    }

    /* Remap the IDs in the node table */
    for (j = 0; j < num_nodes; j++) {
        pop_id = node_individual[j];
        if (pop_id != TSK_NULL) {
            node_individual[j] = individual_id_map[pop_id];
        }
    }

    /* Remap parent IDs. *
     * NOTE! must take the pointer reference here as it can change from
     * the start of the function */
    parents = self->tables->individuals.parents;
    for (j = 0; j < self->tables->individuals.parents_length; j++) {
        if (parents[j] != TSK_NULL) {
            parents[j] = individual_id_map[parents[j]];
        }
    }

out:
    tsk_safe_free(individual_id_map);
    tsk_safe_free(individual_referenced);
    return ret;
}

static int TSK_WARN_UNUSED
simplifier_output_sites(simplifier_t *self)
{
    int ret = 0;
    tsk_id_t ret_id;
    tsk_size_t j;
    tsk_mutation_t mutation;
    const tsk_size_t num_sites = self->input_tables.sites.num_rows;
    const tsk_size_t num_mutations = self->input_tables.mutations.num_rows;
    bool *site_referenced = tsk_calloc(num_sites, sizeof(*site_referenced));
    tsk_id_t *site_id_map = tsk_malloc(num_sites * sizeof(*site_id_map));
    tsk_id_t *mutation_id_map = tsk_malloc(num_mutations * sizeof(*mutation_id_map));
    const tsk_id_t *mutation_node_map = self->mutation_node_map;
    const tsk_id_t *mutation_site = self->input_tables.mutations.site;

    if (site_referenced == NULL || site_id_map == NULL || mutation_id_map == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    for (j = 0; j < num_mutations; j++) {
        if (mutation_node_map[j] != TSK_NULL) {
            site_referenced[mutation_site[j]] = true;
        }
    }
    ret = simplifier_finalise_site_references(self, site_referenced, site_id_map);
    if (ret != 0) {
        goto out;
    }

    for (j = 0; j < num_mutations; j++) {
        mutation_id_map[j] = TSK_NULL;
        if (mutation_node_map[j] != TSK_NULL) {
            tsk_mutation_table_get_row_unsafe(
                &self->input_tables.mutations, (tsk_id_t) j, &mutation);
            mutation.node = mutation_node_map[j];
            mutation.site = site_id_map[mutation.site];
            if (mutation.parent != TSK_NULL) {
                mutation.parent = mutation_id_map[mutation.parent];
            }
            ret_id = tsk_mutation_table_add_row(&self->tables->mutations, mutation.site,
                mutation.node, mutation.parent, mutation.time, mutation.derived_state,
                mutation.derived_state_length, mutation.metadata,
                mutation.metadata_length);
            if (ret_id < 0) {
                ret = (int) ret_id;
                goto out;
            }
            mutation_id_map[j] = ret_id;
        }
    }
out:
    tsk_safe_free(site_referenced);
    tsk_safe_free(site_id_map);
    tsk_safe_free(mutation_id_map);
    return ret;
}

/* Flush the remaining non-edge and node data in the model to the
 * output tables. */
static int TSK_WARN_UNUSED
simplifier_flush_output(simplifier_t *self)
{
    int ret = 0;

    /* TODO Migrations fit reasonably neatly into the pattern that we have here. We
     * can consider references to populations from migration objects in the same way
     * as from nodes, so that we only remove a population if its referenced by
     * neither. Mapping the population IDs in migrations is then easy. In principle
     * nodes are similar, but the semantics are slightly different because we've
     * already allocated all the nodes by their references from edges. We then
     * need to decide whether we remove migrations that reference unmapped nodes
     * or whether to add these nodes back in (probably the former is the correct
     * approach).*/
    if (self->input_tables.migrations.num_rows != 0) {
        ret = tsk_trace_error(TSK_ERR_SIMPLIFY_MIGRATIONS_NOT_SUPPORTED);
        goto out;
    }

    ret = simplifier_output_sites(self);
    if (ret != 0) {
        goto out;
    }

    if (self->options & TSK_SIMPLIFY_FILTER_POPULATIONS) {
        ret = simplifier_finalise_population_references(self);
        if (ret != 0) {
            goto out;
        }
    }
    if (self->options & TSK_SIMPLIFY_FILTER_INDIVIDUALS) {
        ret = simplifier_finalise_individual_references(self);
        if (ret != 0) {
            goto out;
        }
    }

out:
    return ret;
}

static void
simplifier_set_edge_sort_offset(simplifier_t *self, double youngest_root_time)
{
    const tsk_edge_table_t edges = self->tables->edges;
    const double *node_time = self->tables->nodes.time;
    int64_t offset;

    for (offset = 0; offset < (int64_t) edges.num_rows; offset++) {
        if (node_time[edges.parent[offset]] >= youngest_root_time) {
            break;
        }
    }
    self->edge_sort_offset = offset;
}

static int TSK_WARN_UNUSED
simplifier_sort_edges(simplifier_t *self)
{
    /* designated initialisers are guaranteed to set any missing fields to
     * zero, so we don't need to set the rest of them. */
    tsk_bookmark_t bookmark = {
        .edges = (tsk_size_t) self->edge_sort_offset,
        .sites = self->tables->sites.num_rows,
        .mutations = self->tables->mutations.num_rows,
    };
    tsk_bug_assert(self->edge_sort_offset >= 0);
    return tsk_table_collection_sort(self->tables, &bookmark, 0);
}

static int TSK_WARN_UNUSED
simplifier_insert_input_roots(simplifier_t *self)
{
    int ret = 0;
    tsk_id_t input_id, output_id;
    tsk_segment_t *x;
    tsk_size_t num_flushed_edges;
    double youngest_root_time = DBL_MAX;
    const double *node_time = self->tables->nodes.time;

    for (input_id = 0; input_id < (tsk_id_t) self->input_tables.nodes.num_rows;
         input_id++) {
        x = self->ancestor_map_head[input_id];
        if (x != NULL) {
            output_id = self->node_id_map[input_id];
            if (output_id == TSK_NULL) {
                output_id = simplifier_record_node(self, input_id);
                if (output_id < 0) {
                    ret = (int) output_id;
                    goto out;
                }
            }
            youngest_root_time = TSK_MIN(youngest_root_time, node_time[output_id]);
            while (x != NULL) {
                if (x->node != output_id) {
                    ret = simplifier_record_edge(self, x->left, x->right, x->node);
                    if (ret != 0) {
                        goto out;
                    }
                    simplifier_map_mutations(
                        self, input_id, x->left, x->right, output_id);
                }
                x = x->next;
            }
            ret = simplifier_flush_edges(self, output_id, &num_flushed_edges);
            if (ret != 0) {
                goto out;
            }
        }
    }
    if (youngest_root_time != DBL_MAX) {
        simplifier_set_edge_sort_offset(self, youngest_root_time);
    }
out:
    return ret;
}

static int TSK_WARN_UNUSED
simplifier_run(simplifier_t *self, tsk_id_t *node_map)
{
    int ret = 0;
    tsk_size_t j, start;
    tsk_id_t parent, current_parent;
    const tsk_edge_table_t *input_edges = &self->input_tables.edges;
    tsk_size_t num_edges = input_edges->num_rows;

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
    if (self->options & TSK_SIMPLIFY_KEEP_INPUT_ROOTS) {
        ret = simplifier_insert_input_roots(self);
        if (ret != 0) {
            goto out;
        }
    }
    ret = simplifier_flush_output(self);
    if (ret != 0) {
        goto out;
    }
    if (node_map != NULL) {
        /* Finally, output the new IDs for the nodes, if required. */
        tsk_memcpy(node_map, self->node_id_map,
            self->input_tables.nodes.num_rows * sizeof(tsk_id_t));
    }
    if (self->edge_sort_offset != TSK_NULL) {
        tsk_bug_assert(self->options & TSK_SIMPLIFY_KEEP_INPUT_ROOTS);
        ret = simplifier_sort_edges(self);
        if (ret != 0) {
            goto out;
        }
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
cmp_index_sort(const void *a, const void *b)
{
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
tsk_table_collection_check_offsets(const tsk_table_collection_t *self)
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
tsk_table_collection_check_node_integrity(
    const tsk_table_collection_t *self, tsk_flags_t options)
{
    int ret = 0;
    tsk_size_t j;
    double node_time;
    tsk_id_t population, individual;
    tsk_id_t num_populations = (tsk_id_t) self->populations.num_rows;
    tsk_id_t num_individuals = (tsk_id_t) self->individuals.num_rows;
    const bool check_population_refs = !(options & TSK_NO_CHECK_POPULATION_REFS);

    for (j = 0; j < self->nodes.num_rows; j++) {
        node_time = self->nodes.time[j];
        if (!tsk_isfinite(node_time)) {
            ret = tsk_trace_error(TSK_ERR_TIME_NONFINITE);
            goto out;
        }
        if (check_population_refs) {
            population = self->nodes.population[j];
            if (population < TSK_NULL || population >= num_populations) {
                ret = tsk_trace_error(TSK_ERR_POPULATION_OUT_OF_BOUNDS);
                goto out;
            }
        }
        individual = self->nodes.individual[j];
        if (individual < TSK_NULL || individual >= num_individuals) {
            ret = tsk_trace_error(TSK_ERR_INDIVIDUAL_OUT_OF_BOUNDS);
            goto out;
        }
    }
out:
    return ret;
}

static int
tsk_table_collection_check_edge_integrity(
    const tsk_table_collection_t *self, tsk_flags_t options)
{
    int ret = 0;
    tsk_size_t j;
    tsk_id_t parent, last_parent, child, last_child;
    double left, last_left, right;
    const double *time = self->nodes.time;
    const double L = self->sequence_length;
    const tsk_edge_table_t edges = self->edges;
    const tsk_id_t num_nodes = (tsk_id_t) self->nodes.num_rows;
    const bool check_ordering = !!(options & TSK_CHECK_EDGE_ORDERING);
    bool *parent_seen = NULL;

    if (check_ordering) {
        parent_seen = tsk_calloc((tsk_size_t) num_nodes, sizeof(*parent_seen));
        if (parent_seen == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
    }

    /* Just keeping compiler happy; these values don't matter. */
    last_left = 0;
    last_parent = 0;
    last_child = 0;
    for (j = 0; j < edges.num_rows; j++) {
        parent = edges.parent[j];
        child = edges.child[j];
        left = edges.left[j];
        right = edges.right[j];
        /* Node ID integrity */
        if (parent == TSK_NULL) {
            ret = tsk_trace_error(TSK_ERR_NULL_PARENT);
            goto out;
        }
        if (parent < 0 || parent >= num_nodes) {
            ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
            goto out;
        }
        if (child == TSK_NULL) {
            ret = tsk_trace_error(TSK_ERR_NULL_CHILD);
            goto out;
        }
        if (child < 0 || child >= num_nodes) {
            ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
            goto out;
        }
        /* Spatial requirements for edges */
        if (!(tsk_isfinite(left) && tsk_isfinite(right))) {
            ret = tsk_trace_error(TSK_ERR_GENOME_COORDS_NONFINITE);
            goto out;
        }
        if (left < 0) {
            ret = tsk_trace_error(TSK_ERR_LEFT_LESS_ZERO);
            goto out;
        }
        if (right > L) {
            ret = tsk_trace_error(TSK_ERR_RIGHT_GREATER_SEQ_LENGTH);
            goto out;
        }
        if (left >= right) {
            ret = tsk_trace_error(TSK_ERR_BAD_EDGE_INTERVAL);
            goto out;
        }
        /* time[child] must be < time[parent] */
        if (time[child] >= time[parent]) {
            ret = tsk_trace_error(TSK_ERR_BAD_NODE_TIME_ORDERING);
            goto out;
        }

        if (check_ordering) {
            if (parent_seen[parent]) {
                ret = tsk_trace_error(TSK_ERR_EDGES_NONCONTIGUOUS_PARENTS);
                goto out;
            }
            if (j > 0) {
                /* Input data must sorted by (time[parent], parent, child, left). */
                if (time[parent] < time[last_parent]) {
                    ret = tsk_trace_error(TSK_ERR_EDGES_NOT_SORTED_PARENT_TIME);
                    goto out;
                }
                if (time[parent] == time[last_parent]) {
                    if (parent == last_parent) {
                        if (child < last_child) {
                            ret = tsk_trace_error(TSK_ERR_EDGES_NOT_SORTED_CHILD);
                            goto out;
                        }
                        if (child == last_child) {
                            if (left == last_left) {
                                ret = tsk_trace_error(TSK_ERR_DUPLICATE_EDGES);
                                goto out;
                            } else if (left < last_left) {
                                ret = tsk_trace_error(TSK_ERR_EDGES_NOT_SORTED_LEFT);
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
    }
out:
    tsk_safe_free(parent_seen);
    return ret;
}

static int TSK_WARN_UNUSED
tsk_table_collection_check_site_integrity(
    const tsk_table_collection_t *self, tsk_flags_t options)
{
    int ret = 0;
    tsk_size_t j;
    double position;
    const double L = self->sequence_length;
    const tsk_site_table_t sites = self->sites;
    const bool check_site_ordering = !!(options & TSK_CHECK_SITE_ORDERING);
    const bool check_site_duplicates = !!(options & TSK_CHECK_SITE_DUPLICATES);

    for (j = 0; j < sites.num_rows; j++) {
        position = sites.position[j];
        /* Spatial requirements */
        if (!tsk_isfinite(position)) {
            ret = tsk_trace_error(TSK_ERR_BAD_SITE_POSITION);
            goto out;
        }
        if (position < 0 || position >= L) {
            ret = tsk_trace_error(TSK_ERR_BAD_SITE_POSITION);
            goto out;
        }
        if (j > 0) {
            if (check_site_duplicates && sites.position[j - 1] == position) {
                ret = tsk_trace_error(TSK_ERR_DUPLICATE_SITE_POSITION);
                goto out;
            }
            if (check_site_ordering && sites.position[j - 1] > position) {
                ret = tsk_trace_error(TSK_ERR_UNSORTED_SITES);
                goto out;
            }
        }
    }
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_table_collection_check_mutation_integrity(
    const tsk_table_collection_t *self, tsk_flags_t options)
{
    int ret = 0;
    tsk_size_t j;
    tsk_id_t parent_mut;
    double mutation_time;
    double last_known_time = INFINITY;
    const tsk_mutation_table_t mutations = self->mutations;
    const tsk_id_t num_nodes = (tsk_id_t) self->nodes.num_rows;
    const tsk_id_t num_sites = (tsk_id_t) self->sites.num_rows;
    const tsk_id_t num_mutations = (tsk_id_t) self->mutations.num_rows;
    const double *node_time = self->nodes.time;
    const bool check_mutation_ordering = !!(options & TSK_CHECK_MUTATION_ORDERING);
    bool unknown_time;
    int num_known_times = 0;
    int num_unknown_times = 0;

    for (j = 0; j < mutations.num_rows; j++) {
        /* Basic reference integrity */
        if (mutations.site[j] < 0 || mutations.site[j] >= num_sites) {
            ret = tsk_trace_error(TSK_ERR_SITE_OUT_OF_BOUNDS);
            goto out;
        }
        if (mutations.node[j] < 0 || mutations.node[j] >= num_nodes) {
            ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
            goto out;
        }
        /* Integrity check for mutation parent */
        parent_mut = mutations.parent[j];
        if (parent_mut < TSK_NULL || parent_mut >= num_mutations) {
            ret = tsk_trace_error(TSK_ERR_MUTATION_OUT_OF_BOUNDS);
            goto out;
        }
        if (parent_mut == (tsk_id_t) j) {
            ret = tsk_trace_error(TSK_ERR_MUTATION_PARENT_EQUAL);
            goto out;
        }
        /* Check that time is finite and not more recent than node time */
        mutation_time = mutations.time[j];
        unknown_time = tsk_is_unknown_time(mutation_time);
        if (!unknown_time) {
            if (!tsk_isfinite(mutation_time)) {
                ret = tsk_trace_error(TSK_ERR_TIME_NONFINITE);
                goto out;
            }
            if (mutation_time < node_time[mutations.node[j]]) {
                ret = tsk_trace_error(TSK_ERR_MUTATION_TIME_YOUNGER_THAN_NODE);
                goto out;
            }
        }

        /* reset checks when reaching a new site */
        if (j > 0 && mutations.site[j - 1] != mutations.site[j]) {
            last_known_time = INFINITY;
            num_known_times = 0;
            num_unknown_times = 0;
        }

        /* Check known/unknown times are not both present on a site */
        if (unknown_time) {
            num_unknown_times++;
        } else {
            num_known_times++;
        }
        if ((num_unknown_times > 0) && (num_known_times > 0)) {
            ret = tsk_trace_error(TSK_ERR_MUTATION_TIME_HAS_BOTH_KNOWN_AND_UNKNOWN);
            goto out;
        }

        /* check parent site agrees */
        if (parent_mut != TSK_NULL) {
            if (mutations.site[parent_mut] != mutations.site[j]) {
                ret = tsk_trace_error(TSK_ERR_MUTATION_PARENT_DIFFERENT_SITE);
                goto out;
            }
            /* If this mutation time is known, then the parent time
             * must also be, or else the
             * TSK_ERR_MUTATION_TIME_HAS_BOTH_KNOWN_AND_UNKNOWN check
             * above will fail. */
            if (!unknown_time && mutation_time > mutations.time[parent_mut]) {
                ret = tsk_trace_error(TSK_ERR_MUTATION_TIME_OLDER_THAN_PARENT_MUTATION);
                goto out;
            }
        }

        if (check_mutation_ordering) {
            /* Check site ordering */
            if (j > 0 && mutations.site[j - 1] > mutations.site[j]) {
                ret = tsk_trace_error(TSK_ERR_UNSORTED_MUTATIONS);
                goto out;
            }

            /* Check if parents are listed before their children */
            if (parent_mut != TSK_NULL && parent_mut > (tsk_id_t) j) {
                ret = tsk_trace_error(TSK_ERR_MUTATION_PARENT_AFTER_CHILD);
                goto out;
            }

            /* Check time ordering. We do this after the other checks above,
             * so that more specific errors trigger first */
            if (!unknown_time) {
                if (mutation_time > last_known_time) {
                    ret = tsk_trace_error(TSK_ERR_UNSORTED_MUTATIONS);
                    goto out;
                }
                last_known_time = mutation_time;
            }
        }
    }
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_table_collection_check_migration_integrity(
    const tsk_table_collection_t *self, tsk_flags_t options)
{
    int ret = 0;
    tsk_size_t j;
    double left, right, time;
    const double L = self->sequence_length;
    const tsk_migration_table_t migrations = self->migrations;
    const tsk_id_t num_nodes = (tsk_id_t) self->nodes.num_rows;
    const tsk_id_t num_populations = (tsk_id_t) self->populations.num_rows;
    const bool check_population_refs = !(options & TSK_NO_CHECK_POPULATION_REFS);
    const bool check_migration_ordering = !!(options & TSK_CHECK_MIGRATION_ORDERING);

    for (j = 0; j < migrations.num_rows; j++) {
        if (migrations.node[j] < 0 || migrations.node[j] >= num_nodes) {
            ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
            goto out;
        }
        if (check_population_refs) {
            if (migrations.source[j] < 0 || migrations.source[j] >= num_populations) {
                ret = tsk_trace_error(TSK_ERR_POPULATION_OUT_OF_BOUNDS);
                goto out;
            }
            if (migrations.dest[j] < 0 || migrations.dest[j] >= num_populations) {
                ret = tsk_trace_error(TSK_ERR_POPULATION_OUT_OF_BOUNDS);
                goto out;
            }
        }
        time = migrations.time[j];
        if (!tsk_isfinite(time)) {
            ret = tsk_trace_error(TSK_ERR_TIME_NONFINITE);
            goto out;
        }
        if (j > 0) {
            if (check_migration_ordering && migrations.time[j - 1] > time) {
                ret = tsk_trace_error(TSK_ERR_UNSORTED_MIGRATIONS);
                goto out;
            }
        }
        left = migrations.left[j];
        right = migrations.right[j];
        /* Spatial requirements */
        /* TODO it's a bit misleading to use the edge-specific errors here. */
        if (!(tsk_isfinite(left) && tsk_isfinite(right))) {
            ret = tsk_trace_error(TSK_ERR_GENOME_COORDS_NONFINITE);
            goto out;
        }
        if (left < 0) {
            ret = tsk_trace_error(TSK_ERR_LEFT_LESS_ZERO);
            goto out;
        }
        if (right > L) {
            ret = tsk_trace_error(TSK_ERR_RIGHT_GREATER_SEQ_LENGTH);
            goto out;
        }
        if (left >= right) {
            ret = tsk_trace_error(TSK_ERR_BAD_EDGE_INTERVAL);
            goto out;
        }
    }
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_table_collection_check_individual_integrity(
    const tsk_table_collection_t *self, tsk_flags_t options)
{
    int ret = 0;
    tsk_size_t j, k;
    const tsk_individual_table_t individuals = self->individuals;
    const tsk_id_t num_individuals = (tsk_id_t) individuals.num_rows;
    const bool check_individual_ordering = options & TSK_CHECK_INDIVIDUAL_ORDERING;

    for (j = 0; j < (tsk_size_t) num_individuals; j++) {
        for (k = individuals.parents_offset[j]; k < individuals.parents_offset[j + 1];
             k++) {
            /* Check parent references are valid */
            if (individuals.parents[k] != TSK_NULL
                && (individuals.parents[k] < 0
                       || individuals.parents[k] >= num_individuals)) {
                ret = tsk_trace_error(TSK_ERR_INDIVIDUAL_OUT_OF_BOUNDS);
                goto out;
            }
            /* Check no-one is their own parent */
            if (individuals.parents[k] == (tsk_id_t) j) {
                ret = tsk_trace_error(TSK_ERR_INDIVIDUAL_SELF_PARENT);
                goto out;
            }
            /* Check parents are ordered */
            if (check_individual_ordering && individuals.parents[k] != TSK_NULL
                && individuals.parents[k] >= (tsk_id_t) j) {
                ret = tsk_trace_error(TSK_ERR_UNSORTED_INDIVIDUALS);
                goto out;
            }
        }
    }
out:
    return ret;
}

static tsk_id_t TSK_WARN_UNUSED
tsk_table_collection_check_tree_integrity(const tsk_table_collection_t *self)
{
    tsk_id_t ret = 0;
    tsk_size_t j, k;
    tsk_id_t e, u, site, mutation;
    double tree_left, tree_right;
    const double sequence_length = self->sequence_length;
    const tsk_id_t num_sites = (tsk_id_t) self->sites.num_rows;
    const tsk_id_t num_mutations = (tsk_id_t) self->mutations.num_rows;
    const tsk_size_t num_edges = self->edges.num_rows;
    const double *restrict site_position = self->sites.position;
    const tsk_id_t *restrict mutation_site = self->mutations.site;
    const tsk_id_t *restrict mutation_node = self->mutations.node;
    const double *restrict mutation_time = self->mutations.time;
    const double *restrict node_time = self->nodes.time;
    const tsk_id_t *restrict I = self->indexes.edge_insertion_order;
    const tsk_id_t *restrict O = self->indexes.edge_removal_order;
    const double *restrict edge_right = self->edges.right;
    const double *restrict edge_left = self->edges.left;
    const tsk_id_t *restrict edge_child = self->edges.child;
    const tsk_id_t *restrict edge_parent = self->edges.parent;
    tsk_id_t *restrict parent = NULL;
    int8_t *restrict used_edges = NULL;
    tsk_id_t num_trees = 0;

    parent = tsk_malloc(self->nodes.num_rows * sizeof(*parent));
    used_edges = tsk_malloc(num_edges * sizeof(*used_edges));
    if (parent == NULL || used_edges == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memset(parent, 0xff, self->nodes.num_rows * sizeof(*parent));
    tsk_memset(used_edges, 0, num_edges * sizeof(*used_edges));

    tree_left = 0;
    num_trees = 0;
    j = 0;
    k = 0;
    site = 0;
    mutation = 0;
    tsk_bug_assert(I != NULL && O != NULL);
    tsk_bug_assert(self->indexes.num_edges == num_edges);

    while (j < num_edges || tree_left < sequence_length) {
        while (k < num_edges && edge_right[O[k]] == tree_left) {
            e = O[k];
            if (used_edges[e] != 1) {
                ret = tsk_trace_error(TSK_ERR_TABLES_BAD_INDEXES);
                goto out;
            }
            parent[edge_child[e]] = TSK_NULL;
            used_edges[e]++;
            k++;
        }
        while (j < num_edges && edge_left[I[j]] == tree_left) {
            e = I[j];
            if (used_edges[e] != 0) {
                ret = tsk_trace_error(TSK_ERR_TABLES_BAD_INDEXES);
                goto out;
            }
            used_edges[e]++;
            u = edge_child[e];
            if (parent[u] != TSK_NULL) {
                ret = tsk_trace_error(TSK_ERR_BAD_EDGES_CONTRADICTORY_CHILDREN);
                goto out;
            }
            parent[u] = edge_parent[e];
            j++;
        }
        tree_right = sequence_length;
        if (j < num_edges) {
            tree_right = TSK_MIN(tree_right, edge_left[I[j]]);
        }
        if (k < num_edges) {
            tree_right = TSK_MIN(tree_right, edge_right[O[k]]);
        }
        while (site < num_sites && site_position[site] < tree_right) {
            while (mutation < num_mutations && mutation_site[mutation] == site) {
                if (!tsk_is_unknown_time(mutation_time[mutation])
                    && parent[mutation_node[mutation]] != TSK_NULL
                    && node_time[parent[mutation_node[mutation]]]
                           <= mutation_time[mutation]) {
                    ret = tsk_trace_error(TSK_ERR_MUTATION_TIME_OLDER_THAN_PARENT_NODE);
                    goto out;
                }
                mutation++;
            }
            site++;
        }
        if (tree_right <= tree_left) {
            ret = tsk_trace_error(TSK_ERR_TABLES_BAD_INDEXES);
            goto out;
        }
        tree_left = tree_right;
        /* This is technically possible; if we have 2**31 edges each defining
         * a single tree, and there's a gap between each of these edges we
         * would overflow this counter. */
        if (num_trees == TSK_MAX_ID) {
            ret = tsk_trace_error(TSK_ERR_TREE_OVERFLOW);
            goto out;
        }
        num_trees++;
    }
    tsk_bug_assert(j == num_edges);
    while (k < num_edges) {
        /* At this point it must be that used_edges[O[k]] == 1,
         * since otherwise we would have added a different edge twice,
         * and so hit the error above. */
        e = O[k];
        if (edge_right[e] != sequence_length) {
            ret = tsk_trace_error(TSK_ERR_TABLES_BAD_INDEXES);
            goto out;
        }
        used_edges[e]++;
        k++;
    }
    ret = num_trees;
out:
    /* Can't use tsk_safe_free because of restrict*/
    if (parent != NULL) {
        free(parent);
    }
    if (used_edges != NULL) {
        free(used_edges);
    }
    return ret;
}

static int TSK_WARN_UNUSED
tsk_table_collection_check_index_integrity(const tsk_table_collection_t *self)
{
    int ret = 0;
    tsk_id_t j;
    const tsk_id_t num_edges = (tsk_id_t) self->edges.num_rows;
    const tsk_id_t *edge_insertion_order = self->indexes.edge_insertion_order;
    const tsk_id_t *edge_removal_order = self->indexes.edge_removal_order;

    if (!tsk_table_collection_has_index(self, 0)) {
        ret = tsk_trace_error(TSK_ERR_TABLES_NOT_INDEXED);
        goto out;
    }
    for (j = 0; j < num_edges; j++) {
        if (edge_insertion_order[j] < 0 || edge_insertion_order[j] >= num_edges) {
            ret = tsk_trace_error(TSK_ERR_EDGE_OUT_OF_BOUNDS);
            goto out;
        }
        if (edge_removal_order[j] < 0 || edge_removal_order[j] >= num_edges) {
            ret = tsk_trace_error(TSK_ERR_EDGE_OUT_OF_BOUNDS);
            goto out;
        }
    }
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_table_collection_compute_mutation_parents_to_array(
    const tsk_table_collection_t *self, tsk_id_t *mutation_parent)
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
    tsk_size_t j, mutation, first_mutation;

    parent = tsk_malloc(nodes.num_rows * sizeof(*parent));
    bottom_mutation = tsk_malloc(nodes.num_rows * sizeof(*bottom_mutation));
    if (parent == NULL || bottom_mutation == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memset(parent, 0xff, nodes.num_rows * sizeof(*parent));
    tsk_memset(bottom_mutation, 0xff, nodes.num_rows * sizeof(*bottom_mutation));
    tsk_memset(mutation_parent, 0xff, self->mutations.num_rows * sizeof(tsk_id_t));

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
                    mutation_parent[mutation] = bottom_mutation[u];
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
                    if (mutation_parent[j] == TSK_NULL) {
                        u = parent[mutations.node[j]];
                        while (u != TSK_NULL && bottom_mutation[u] == TSK_NULL) {
                            u = parent[u];
                        }
                        if (u != TSK_NULL) {
                            mutation_parent[j] = bottom_mutation[u];
                        }
                    }
                }
            }
            /* Reset the mapping for the next site */
            for (j = first_mutation; j < mutation; j++) {
                u = mutations.node[j];
                bottom_mutation[u] = TSK_NULL;
                /* Check that we haven't violated the sortedness property */
                if (mutation_parent[j] > (tsk_id_t) j) {
                    ret = tsk_trace_error(TSK_ERR_MUTATION_PARENT_AFTER_CHILD);
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

static int TSK_WARN_UNUSED
tsk_table_collection_check_mutation_parents(const tsk_table_collection_t *self)
{
    int ret = 0;
    tsk_mutation_table_t mutations = self->mutations;
    tsk_id_t *new_parents = NULL;
    tsk_size_t j;

    if (mutations.num_rows == 0) {
        return ret;
    }

    new_parents = tsk_malloc(mutations.num_rows * sizeof(*new_parents));
    if (new_parents == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    ret = tsk_table_collection_compute_mutation_parents_to_array(self, new_parents);
    if (ret != 0) {
        goto out;
    }

    for (j = 0; j < mutations.num_rows; j++) {
        if (mutations.parent[j] != new_parents[j]) {
            ret = tsk_trace_error(TSK_ERR_BAD_MUTATION_PARENT);
            goto out;
        }
    }

out:
    tsk_safe_free(new_parents);
    return ret;
}

tsk_id_t TSK_WARN_UNUSED
tsk_table_collection_check_integrity(
    const tsk_table_collection_t *self, tsk_flags_t options)
{
    tsk_id_t ret = 0;
    int mut_ret = 0;

    if (options & TSK_CHECK_MUTATION_PARENTS) {
        /* If we're checking mutation parents, we need to check the trees first */
        options |= TSK_CHECK_TREES;
    }

    if (options & TSK_CHECK_TREES) {
        /* Checking the trees implies these checks */
        options |= TSK_CHECK_EDGE_ORDERING | TSK_CHECK_SITE_ORDERING
                   | TSK_CHECK_SITE_DUPLICATES | TSK_CHECK_MUTATION_ORDERING
                   | TSK_CHECK_MIGRATION_ORDERING | TSK_CHECK_INDEXES;
    }

    if (!tsk_isfinite(self->sequence_length) || self->sequence_length <= 0) {
        ret = tsk_trace_error(TSK_ERR_BAD_SEQUENCE_LENGTH);
        goto out;
    }
    ret = tsk_table_collection_check_offsets(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_check_node_integrity(self, options);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_check_edge_integrity(self, options);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_check_site_integrity(self, options);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_check_mutation_integrity(self, options);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_check_migration_integrity(self, options);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_check_individual_integrity(self, options);
    if (ret != 0) {
        goto out;
    }

    if (options & TSK_CHECK_INDEXES) {
        ret = tsk_table_collection_check_index_integrity(self);
        if (ret != 0) {
            goto out;
        }
    }
    if (options & TSK_CHECK_TREES) {
        ret = tsk_table_collection_check_tree_integrity(self);
        if (ret < 0) {
            goto out;
        }
        /* This check requires tree integrity so do it last */
        if (options & TSK_CHECK_MUTATION_PARENTS) {
            mut_ret = tsk_table_collection_check_mutation_parents(self);
            if (mut_ret != 0) {
                ret = mut_ret;
                goto out;
            }
        }
    }
out:
    return ret;
}

void
tsk_table_collection_print_state(const tsk_table_collection_t *self, FILE *out)
{
    fprintf(out, "Table collection state\n");
    fprintf(out, "sequence_length = %f\n", self->sequence_length);

    write_metadata_schema_header(
        out, self->metadata_schema, self->metadata_schema_length);
    fprintf(out, "#metadata#\n");
    fprintf(out, "%.*s\n", (int) self->metadata_length, self->metadata);
    fprintf(out, "#end#metadata\n");
    fprintf(out, "#time_units#\n");
    fprintf(out, "%.*s\n", (int) self->time_units_length, self->time_units);
    fprintf(out, "#end#time_units\n");
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
    tsk_flags_t edge_options = 0;

    tsk_memset(self, 0, sizeof(*self));
    if (options & TSK_TC_NO_EDGE_METADATA) {
        edge_options |= TSK_TABLE_NO_METADATA;
    }

    /* Set default time_units value */
    ret = tsk_table_collection_set_time_units(
        self, TSK_TIME_UNITS_UNKNOWN, strlen(TSK_TIME_UNITS_UNKNOWN));
    if (ret != 0) {
        goto out;
    }

    ret = tsk_node_table_init(&self->nodes, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_edge_table_init(&self->edges, edge_options);
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
    ret = tsk_reference_sequence_init(&self->reference_sequence, 0);
    if (ret != 0) {
        goto out;
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
    tsk_reference_sequence_free(&self->reference_sequence);
    tsk_safe_free(self->indexes.edge_insertion_order);
    tsk_safe_free(self->indexes.edge_removal_order);
    tsk_safe_free(self->file_uuid);
    tsk_safe_free(self->time_units);
    tsk_safe_free(self->metadata);
    tsk_safe_free(self->metadata_schema);
    return 0;
}

bool
tsk_table_collection_equals(const tsk_table_collection_t *self,
    const tsk_table_collection_t *other, tsk_flags_t options)
{
    bool ret = self->sequence_length == other->sequence_length
               && self->time_units_length == other->time_units_length
               && tsk_memcmp(self->time_units, other->time_units,
                      self->time_units_length * sizeof(char))
                      == 0;
    if (!(options & TSK_CMP_IGNORE_TABLES)) {
        ret = ret
              && tsk_individual_table_equals(
                     &self->individuals, &other->individuals, options)
              && tsk_node_table_equals(&self->nodes, &other->nodes, options)
              && tsk_edge_table_equals(&self->edges, &other->edges, options)
              && tsk_migration_table_equals(
                     &self->migrations, &other->migrations, options)
              && tsk_site_table_equals(&self->sites, &other->sites, options)
              && tsk_mutation_table_equals(&self->mutations, &other->mutations, options)
              && tsk_population_table_equals(
                     &self->populations, &other->populations, options);
        /* TSK_CMP_IGNORE_TABLES implies TSK_CMP_IGNORE_PROVENANCE */
        if (!(options & TSK_CMP_IGNORE_PROVENANCE)) {
            ret = ret
                  && tsk_provenance_table_equals(
                         &self->provenances, &other->provenances, options);
        }
    }
    /* TSK_CMP_IGNORE_TS_METADATA is implied by TSK_CMP_IGNORE_METADATA */
    if (options & TSK_CMP_IGNORE_METADATA) {
        options |= TSK_CMP_IGNORE_TS_METADATA;
    }
    if (!(options & TSK_CMP_IGNORE_TS_METADATA)) {
        ret = ret
              && (self->metadata_length == other->metadata_length
                     && self->metadata_schema_length == other->metadata_schema_length
                     && tsk_memcmp(self->metadata, other->metadata,
                            self->metadata_length * sizeof(char))
                            == 0
                     && tsk_memcmp(self->metadata_schema, other->metadata_schema,
                            self->metadata_schema_length * sizeof(char))
                            == 0);
    }

    if (!(options & TSK_CMP_IGNORE_REFERENCE_SEQUENCE)) {
        ret = ret
              && tsk_reference_sequence_equals(
                     &self->reference_sequence, &other->reference_sequence, options);
    }
    return ret;
}

int
tsk_table_collection_set_time_units(
    tsk_table_collection_t *self, const char *time_units, tsk_size_t time_units_length)
{
    return replace_string(
        &self->time_units, &self->time_units_length, time_units, time_units_length);
}

int
tsk_table_collection_set_metadata(
    tsk_table_collection_t *self, const char *metadata, tsk_size_t metadata_length)
{
    return replace_string(
        &self->metadata, &self->metadata_length, metadata, metadata_length);
}

int
tsk_table_collection_takeset_metadata(
    tsk_table_collection_t *self, char *metadata, tsk_size_t metadata_length)
{
    return takeset_string(
        &self->metadata, &self->metadata_length, metadata, metadata_length);
}

int
tsk_table_collection_set_metadata_schema(tsk_table_collection_t *self,
    const char *metadata_schema, tsk_size_t metadata_schema_length)
{
    return replace_string(&self->metadata_schema, &self->metadata_schema_length,
        metadata_schema, metadata_schema_length);
}

int
tsk_table_collection_set_indexes(tsk_table_collection_t *self,
    tsk_id_t *edge_insertion_order, tsk_id_t *edge_removal_order)
{
    int ret = 0;
    tsk_size_t index_size = self->edges.num_rows * sizeof(tsk_id_t);

    tsk_table_collection_drop_index(self, 0);
    self->indexes.edge_insertion_order = tsk_malloc(index_size);
    self->indexes.edge_removal_order = tsk_malloc(index_size);
    if (self->indexes.edge_insertion_order == NULL
        || self->indexes.edge_removal_order == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memcpy(self->indexes.edge_insertion_order, edge_insertion_order, index_size);
    tsk_memcpy(self->indexes.edge_removal_order, edge_removal_order, index_size);
    self->indexes.num_edges = self->edges.num_rows;
out:
    return ret;
}

int
tsk_table_collection_takeset_indexes(tsk_table_collection_t *self,
    tsk_id_t *edge_insertion_order, tsk_id_t *edge_removal_order)
{
    int ret = 0;

    if (edge_insertion_order == NULL || edge_removal_order == NULL) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    tsk_table_collection_drop_index(self, 0);
    self->indexes.edge_insertion_order = edge_insertion_order;
    self->indexes.edge_removal_order = edge_removal_order;
    self->indexes.num_edges = self->edges.num_rows;
out:
    return ret;
}

bool
tsk_table_collection_has_index(
    const tsk_table_collection_t *self, tsk_flags_t TSK_UNUSED(options))
{
    return self->indexes.edge_insertion_order != NULL
           && self->indexes.edge_removal_order != NULL
           && self->indexes.num_edges == self->edges.num_rows;
}

bool
tsk_table_collection_has_reference_sequence(const tsk_table_collection_t *self)
{
    return !tsk_reference_sequence_is_null(&self->reference_sequence);
}

int
tsk_table_collection_drop_index(
    tsk_table_collection_t *self, tsk_flags_t TSK_UNUSED(options))
{
    tsk_safe_free(self->indexes.edge_insertion_order);
    tsk_safe_free(self->indexes.edge_removal_order);
    self->indexes.edge_insertion_order = NULL;
    self->indexes.edge_removal_order = NULL;
    self->indexes.num_edges = 0;
    return 0;
}

int TSK_WARN_UNUSED
tsk_table_collection_build_index(
    tsk_table_collection_t *self, tsk_flags_t TSK_UNUSED(options))
{
    int ret = TSK_ERR_GENERIC;
    tsk_id_t ret_id;
    tsk_size_t j;
    double *time = self->nodes.time;
    index_sort_t *sort_buff = NULL;
    tsk_id_t parent;

    /* For build indexes to make sense we must have referential integrity and
     * sorted edges */
    ret_id = tsk_table_collection_check_integrity(self, TSK_CHECK_EDGE_ORDERING);
    if (ret_id != 0) {
        ret = (int) ret_id;
        goto out;
    }

    tsk_table_collection_drop_index(self, 0);
    self->indexes.edge_insertion_order
        = tsk_malloc(self->edges.num_rows * sizeof(tsk_id_t));
    self->indexes.edge_removal_order
        = tsk_malloc(self->edges.num_rows * sizeof(tsk_id_t));
    sort_buff = tsk_malloc(self->edges.num_rows * sizeof(index_sort_t));
    if (self->indexes.edge_insertion_order == NULL
        || self->indexes.edge_removal_order == NULL || sort_buff == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    /* sort by left and increasing time to give us the order in which
     * records should be inserted */
    for (j = 0; j < self->edges.num_rows; j++) {
        sort_buff[j].index = (tsk_id_t) j;
        sort_buff[j].first = self->edges.left[j];
        parent = self->edges.parent[j];
        sort_buff[j].second = time[parent];
        sort_buff[j].third = parent;
        sort_buff[j].fourth = self->edges.child[j];
    }
    qsort(
        sort_buff, (size_t) self->edges.num_rows, sizeof(index_sort_t), cmp_index_sort);
    for (j = 0; j < self->edges.num_rows; j++) {
        self->indexes.edge_insertion_order[j] = sort_buff[j].index;
    }
    /* sort by right and decreasing parent time to give us the order in which
     * records should be removed. */
    for (j = 0; j < self->edges.num_rows; j++) {
        sort_buff[j].index = (tsk_id_t) j;
        sort_buff[j].first = self->edges.right[j];
        parent = self->edges.parent[j];
        sort_buff[j].second = -time[parent];
        sort_buff[j].third = -parent;
        sort_buff[j].fourth = -self->edges.child[j];
    }
    qsort(
        sort_buff, (size_t) self->edges.num_rows, sizeof(index_sort_t), cmp_index_sort);
    for (j = 0; j < self->edges.num_rows; j++) {
        self->indexes.edge_removal_order[j] = sort_buff[j].index;
    }
    self->indexes.num_edges = self->edges.num_rows;
    ret = 0;
out:
    tsk_safe_free(sort_buff);
    return ret;
}

static int TSK_WARN_UNUSED
tsk_table_collection_set_file_uuid(tsk_table_collection_t *self, const char *uuid)
{
    int ret = 0;

    tsk_safe_free(self->file_uuid);
    self->file_uuid = NULL;

    if (uuid != NULL) {
        /* Allow space for \0 so we can print it as a string */
        self->file_uuid = tsk_malloc(TSK_UUID_SIZE + 1);
        if (self->file_uuid == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
        tsk_memcpy(self->file_uuid, uuid, TSK_UUID_SIZE);
        self->file_uuid[TSK_UUID_SIZE] = '\0';
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_copy(const tsk_table_collection_t *self,
    tsk_table_collection_t *dest, tsk_flags_t options)
{
    int ret = 0;

    if (!(options & TSK_NO_INIT)) {
        ret = tsk_table_collection_init(dest, options);
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
        ret = tsk_table_collection_set_indexes(
            dest, self->indexes.edge_insertion_order, self->indexes.edge_removal_order);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_table_collection_set_time_units(
        dest, self->time_units, self->time_units_length);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_set_metadata(dest, self->metadata, self->metadata_length);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_set_metadata_schema(
        dest, self->metadata_schema, self->metadata_schema_length);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_reference_sequence_copy(
        &self->reference_sequence, &dest->reference_sequence, options);
    if (ret != 0) {
        goto out;
    }
    if (options & TSK_COPY_FILE_UUID) {
        /* The UUID should only be generated on writing to a file (see the call
         * to generate_uuid in tsk_table_collection_write_format_data) and
         * no other writing access is supported. We only read the value from
         * the file, and raise an error if it's the wrong length there. Thus,
         * finding a UUID value of any other length here is undefined behaviour.
         */
        tsk_bug_assert(
            self->file_uuid == NULL || strlen(self->file_uuid) == TSK_UUID_SIZE);
        ret = tsk_table_collection_set_file_uuid(dest, self->file_uuid);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_table_collection_read_format_data(tsk_table_collection_t *self, kastore_t *store)
{
    int ret = 0;
    size_t len;
    uint32_t *version = NULL;
    int8_t *format_name = NULL;
    int8_t *uuid = NULL;
    double *L = NULL;
    char *time_units = NULL;
    char *metadata = NULL;
    char *metadata_schema = NULL;
    size_t time_units_length, metadata_length, metadata_schema_length;
    /* TODO we could simplify this function quite a bit if we use the
     * read_table_properties infrastructure. We would need to add the
     * ability to have non-optional columns to that though. */

    ret = kastore_gets_int8(store, "format/name", &format_name, &len);
    if (ret != 0) {
        ret = tsk_set_kas_error(ret);
        goto out;
    }
    if (len != TSK_FILE_FORMAT_NAME_LENGTH) {
        ret = tsk_trace_error(TSK_ERR_FILE_FORMAT);
        goto out;
    }
    if (tsk_memcmp(TSK_FILE_FORMAT_NAME, format_name, TSK_FILE_FORMAT_NAME_LENGTH)
        != 0) {
        ret = tsk_trace_error(TSK_ERR_FILE_FORMAT);
        goto out;
    }

    ret = kastore_gets_uint32(store, "format/version", &version, &len);
    if (ret != 0) {
        ret = tsk_set_kas_error(ret);
        goto out;
    }
    if (len != 2) {
        ret = tsk_trace_error(TSK_ERR_FILE_FORMAT);
        goto out;
    }
    if (version[0] < TSK_FILE_FORMAT_VERSION_MAJOR) {
        ret = tsk_trace_error(TSK_ERR_FILE_VERSION_TOO_OLD);
        goto out;
    }
    if (version[0] > TSK_FILE_FORMAT_VERSION_MAJOR) {
        ret = tsk_trace_error(TSK_ERR_FILE_VERSION_TOO_NEW);
        goto out;
    }

    ret = kastore_gets_float64(store, "sequence_length", &L, &len);
    if (ret != 0) {
        ret = tsk_set_kas_error(ret);
        goto out;
    }
    if (len != 1) {
        ret = tsk_trace_error(TSK_ERR_FILE_FORMAT);
        goto out;
    }
    if (L[0] <= 0.0) {
        ret = tsk_trace_error(TSK_ERR_BAD_SEQUENCE_LENGTH);
        goto out;
    }
    self->sequence_length = L[0];

    ret = kastore_gets_int8(store, "uuid", &uuid, &len);
    if (ret != 0) {
        ret = tsk_set_kas_error(ret);
        goto out;
    }
    if (len != TSK_UUID_SIZE) {
        ret = tsk_trace_error(TSK_ERR_FILE_FORMAT);
        goto out;
    }
    ret = tsk_table_collection_set_file_uuid(self, (const char *) uuid);
    if (ret != 0) {
        goto out;
    }

    ret = kastore_containss(store, "time_units");
    if (ret < 0) {
        ret = tsk_set_kas_error(ret);
        goto out;
    }
    if (ret == 1) {
        ret = kastore_gets_int8(
            store, "time_units", (int8_t **) &time_units, &time_units_length);
        if (ret != 0) {
            ret = tsk_set_kas_error(ret);
            goto out;
        }
        ret = tsk_table_collection_set_time_units(
            self, time_units, (tsk_size_t) time_units_length);
        if (ret != 0) {
            goto out;
        }
    }
    ret = kastore_containss(store, "metadata");
    if (ret < 0) {
        ret = tsk_set_kas_error(ret);
        goto out;
    }
    if (ret == 1) {
        ret = kastore_gets_int8(
            store, "metadata", (int8_t **) &metadata, &metadata_length);
        if (ret != 0) {
            ret = tsk_set_kas_error(ret);
            goto out;
        }
        ret = tsk_table_collection_takeset_metadata(
            self, metadata, (tsk_size_t) metadata_length);
        if (ret != 0) {
            goto out;
        }
        metadata = NULL;
    }

    ret = kastore_containss(store, "metadata_schema");
    if (ret < 0) {
        ret = tsk_set_kas_error(ret);
        goto out;
    }
    if (ret == 1) {
        ret = kastore_gets_int8(store, "metadata_schema", (int8_t **) &metadata_schema,
            (size_t *) &metadata_schema_length);
        if (ret != 0) {
            ret = tsk_set_kas_error(ret);
            goto out;
        }
        ret = tsk_table_collection_set_metadata_schema(
            self, metadata_schema, (tsk_size_t) metadata_schema_length);
        if (ret != 0) {
            goto out;
        }
    }

out:
    if ((ret ^ (1 << TSK_KAS_ERR_BIT)) == KAS_ERR_KEY_NOT_FOUND) {
        ret = tsk_trace_error(TSK_ERR_REQUIRED_COL_NOT_FOUND);
    }
    tsk_safe_free(version);
    tsk_safe_free(format_name);
    tsk_safe_free(uuid);
    tsk_safe_free(L);
    tsk_safe_free(time_units);
    tsk_safe_free(metadata_schema);
    tsk_safe_free(metadata);
    return ret;
}

static int TSK_WARN_UNUSED
tsk_table_collection_dump_indexes(const tsk_table_collection_t *self, kastore_t *store,
    tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    write_table_col_t cols[] = {
        { "indexes/edge_insertion_order", NULL, self->indexes.num_edges,
            TSK_ID_STORAGE_TYPE },
        { "indexes/edge_removal_order", NULL, self->indexes.num_edges,
            TSK_ID_STORAGE_TYPE },
        { .name = NULL },
    };

    if (tsk_table_collection_has_index(self, 0)) {
        cols[0].array = self->indexes.edge_insertion_order;
        cols[1].array = self->indexes.edge_removal_order;
        ret = write_table_cols(store, cols, 0);
    }
    return ret;
}

static int TSK_WARN_UNUSED
tsk_table_collection_load_indexes(tsk_table_collection_t *self, kastore_t *store)
{
    int ret = 0;
    tsk_id_t *edge_insertion_order = NULL;
    tsk_id_t *edge_removal_order = NULL;
    tsk_size_t num_rows;

    read_table_col_t cols[] = {
        { "indexes/edge_insertion_order", (void **) &edge_insertion_order,
            TSK_ID_STORAGE_TYPE, TSK_COL_OPTIONAL },
        { "indexes/edge_removal_order", (void **) &edge_removal_order,
            TSK_ID_STORAGE_TYPE, TSK_COL_OPTIONAL },
        { .name = NULL },
    };

    num_rows = TSK_NUM_ROWS_UNSET;
    ret = read_table_cols(store, &num_rows, cols, 0);
    if (ret != 0) {
        goto out;
    }

    if ((edge_insertion_order == NULL) != (edge_removal_order == NULL)) {
        ret = tsk_trace_error(TSK_ERR_BOTH_COLUMNS_REQUIRED);
        goto out;
    }
    if (edge_insertion_order != NULL) {
        if (num_rows != self->edges.num_rows) {
            ret = tsk_trace_error(TSK_ERR_FILE_FORMAT);
            goto out;
        }
        ret = tsk_table_collection_takeset_indexes(
            self, edge_insertion_order, edge_removal_order);
        if (ret != 0) {
            goto out;
        }
    }
    edge_insertion_order = NULL;
    edge_removal_order = NULL;
out:
    tsk_safe_free(edge_insertion_order);
    tsk_safe_free(edge_removal_order);
    return ret;
}

static int
tsk_table_collection_load_reference_sequence(
    tsk_table_collection_t *self, kastore_t *store)
{
    int ret = 0;
    char *data = NULL;
    char *url = NULL;
    char *metadata = NULL;
    char *metadata_schema = NULL;
    tsk_size_t data_length = 0, url_length, metadata_length, metadata_schema_length;

    read_table_property_t properties[] = {
        { "reference_sequence/data", (void **) &data, &data_length, KAS_UINT8,
            TSK_COL_OPTIONAL },
        { "reference_sequence/url", (void **) &url, &url_length, KAS_UINT8,
            TSK_COL_OPTIONAL },
        { "reference_sequence/metadata", (void **) &metadata, &metadata_length,
            KAS_UINT8, TSK_COL_OPTIONAL },
        { "reference_sequence/metadata_schema", (void **) &metadata_schema,
            &metadata_schema_length, KAS_UINT8, TSK_COL_OPTIONAL },
        { .name = NULL },
    };

    ret = read_table_properties(store, properties, 0);
    if (ret != 0) {
        goto out;
    }
    if (data != NULL) {
        ret = tsk_reference_sequence_takeset_data(
            &self->reference_sequence, data, (tsk_size_t) data_length);
        if (ret != 0) {
            goto out;
        }
        data = NULL;
    }
    if (metadata != NULL) {
        ret = tsk_reference_sequence_takeset_metadata(
            &self->reference_sequence, metadata, (tsk_size_t) metadata_length);
        if (ret != 0) {
            goto out;
        }
        metadata = NULL;
    }
    if (metadata_schema != NULL) {
        ret = tsk_reference_sequence_set_metadata_schema(&self->reference_sequence,
            metadata_schema, (tsk_size_t) metadata_schema_length);
        if (ret != 0) {
            goto out;
        }
    }
    if (url != NULL) {
        ret = tsk_reference_sequence_set_url(
            &self->reference_sequence, url, (tsk_size_t) url_length);
        if (ret != 0) {
            goto out;
        }
    }

out:
    free_read_table_mem(NULL, NULL, properties);
    return ret;
}

static int TSK_WARN_UNUSED
tsk_table_collection_loadf_inited(
    tsk_table_collection_t *self, FILE *file, tsk_flags_t options)
{
    int ret = 0;
    kastore_t store;

    int kas_flags = KAS_READ_ALL;
    if ((options & TSK_LOAD_SKIP_TABLES)
        || (options & TSK_LOAD_SKIP_REFERENCE_SEQUENCE)) {
        kas_flags = 0;
    }
    kas_flags = kas_flags | KAS_GET_TAKES_OWNERSHIP;
    ret = kastore_openf(&store, file, "r", kas_flags);

    if (ret != 0) {
        if (ret == KAS_ERR_EOF) {
            /* KAS_ERR_EOF means that we tried to read a store from the stream
             * and we hit EOF immediately without reading any bytes. We signal
             * this back to the client, which allows it to read an indefinite
             * number of stores from a stream */
            ret = tsk_trace_error(TSK_ERR_EOF);
        } else {
            ret = tsk_set_kas_error(ret);
        }
        goto out;
    }
    ret = tsk_table_collection_read_format_data(self, &store);
    if (ret != 0) {
        goto out;
    }
    if (!(options & TSK_LOAD_SKIP_TABLES)) {
        ret = tsk_node_table_load(&self->nodes, &store);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_edge_table_load(&self->edges, &store);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_site_table_load(&self->sites, &store);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_mutation_table_load(&self->mutations, &store);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_migration_table_load(&self->migrations, &store);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_individual_table_load(&self->individuals, &store);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_population_table_load(&self->populations, &store);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_provenance_table_load(&self->provenances, &store);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_table_collection_load_indexes(self, &store);
        if (ret != 0) {
            goto out;
        }
    } else {
        ret = tsk_table_collection_build_index(self, 0);
        if (ret != 0) {
            goto out;
        }
    }
    if (!(options & TSK_LOAD_SKIP_REFERENCE_SEQUENCE)) {
        ret = tsk_table_collection_load_reference_sequence(self, &store);
        if (ret != 0) {
            goto out;
        }
    }
    ret = kastore_close(&store);
    if (ret != 0) {
        goto out;
    }
out:
    /* If we're exiting on an error, we ignore any further errors that might come
     * from kastore. In the nominal case, closing an already-closed store is a
     * safe noop */
    kastore_close(&store);
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_loadf(tsk_table_collection_t *self, FILE *file, tsk_flags_t options)
{
    int ret = 0;

    if (!(options & TSK_NO_INIT)) {
        ret = tsk_table_collection_init(self, options);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_table_collection_loadf_inited(self, file, options);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_load(
    tsk_table_collection_t *self, const char *filename, tsk_flags_t options)
{
    int ret = 0;
    FILE *file = NULL;

    if (!(options & TSK_NO_INIT)) {
        ret = tsk_table_collection_init(self, options);
        if (ret != 0) {
            goto out;
        }
    }
    file = fopen(filename, "rb");
    if (file == NULL) {
        ret = tsk_trace_error(TSK_ERR_IO);
        goto out;
    }
    ret = tsk_table_collection_loadf_inited(self, file, options);
    if (ret != 0) {
        goto out;
    }
    if (fclose(file) != 0) {
        ret = tsk_trace_error(TSK_ERR_IO);
        goto out;
    }
    file = NULL;
out:
    if (file != NULL) {
        /* Ignore any additional errors we might get when closing the file
         * in error conditions */
        fclose(file);
    }
    return ret;
}

static int TSK_WARN_UNUSED
tsk_table_collection_dump_reference_sequence(const tsk_table_collection_t *self,
    kastore_t *store, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    const tsk_reference_sequence_t *ref = &self->reference_sequence;
    write_table_col_t write_cols[] = {
        { "reference_sequence/data", (void *) ref->data, ref->data_length, KAS_UINT8 },
        { "reference_sequence/url", (void *) ref->url, ref->url_length, KAS_UINT8 },
        { "reference_sequence/metadata", (void *) ref->metadata, ref->metadata_length,
            KAS_UINT8 },
        { "reference_sequence/metadata_schema", (void *) ref->metadata_schema,
            ref->metadata_schema_length, KAS_UINT8 },
        { .name = NULL },
    };
    if (tsk_table_collection_has_reference_sequence(self)) {
        ret = write_table_cols(store, write_cols, 0);
    }
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_dump(
    const tsk_table_collection_t *self, const char *filename, tsk_flags_t options)
{
    int ret = 0;
    FILE *file = fopen(filename, "wb");

    if (file == NULL) {
        ret = tsk_trace_error(TSK_ERR_IO);
        goto out;
    }
    ret = tsk_table_collection_dumpf(self, file, options);
    if (ret != 0) {
        goto out;
    }
    if (fclose(file) != 0) {
        ret = tsk_trace_error(TSK_ERR_IO);
        goto out;
    }
    file = NULL;
out:
    if (file != NULL) {
        /* Ignore any additional errors we might get when closing the file
         * in error conditions */
        fclose(file);
        /* If an error occurred make sure that the filename is removed */
        remove(filename);
    }
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_dumpf(
    const tsk_table_collection_t *self, FILE *file, tsk_flags_t options)
{
    int ret = 0;
    kastore_t store;
    char uuid[TSK_UUID_SIZE + 1]; // Must include space for trailing null.
    write_table_col_t format_columns[] = {
        { "format/name", (const void *) &TSK_FILE_FORMAT_NAME,
            TSK_FILE_FORMAT_NAME_LENGTH, KAS_INT8 },
        { "format/version",
            (const void *) &(uint32_t[]){
                TSK_FILE_FORMAT_VERSION_MAJOR, TSK_FILE_FORMAT_VERSION_MINOR },
            2, KAS_UINT32 },
        { "sequence_length", (const void *) &self->sequence_length, 1, KAS_FLOAT64 },
        { "uuid", (void *) uuid, TSK_UUID_SIZE, KAS_INT8 },
        { "time_units", (void *) self->time_units, self->time_units_length, KAS_INT8 },
        { "metadata", (void *) self->metadata, self->metadata_length, KAS_INT8 },
        { "metadata_schema", (void *) self->metadata_schema,
            self->metadata_schema_length, KAS_INT8 },
        { .name = NULL },
    };

    tsk_memset(&store, 0, sizeof(store));

    ret = kastore_openf(&store, file, "w", 0);
    if (ret != 0) {
        ret = tsk_set_kas_error(ret);
        goto out;
    }

    /* Write format data */
    ret = tsk_generate_uuid(uuid, 0);
    if (ret != 0) {
        goto out;
    }

    ret = write_table_cols(&store, format_columns, options);
    if (ret != 0) {
        goto out;
    }

    /* All of these functions will set the kas_error internally, so we don't have
     * to modify the return value. */
    ret = tsk_node_table_dump(&self->nodes, &store, options);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_edge_table_dump(&self->edges, &store, options);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_site_table_dump(&self->sites, &store, options);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_migration_table_dump(&self->migrations, &store, options);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_mutation_table_dump(&self->mutations, &store, options);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_individual_table_dump(&self->individuals, &store, options);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_population_table_dump(&self->populations, &store, options);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_provenance_table_dump(&self->provenances, &store, options);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_dump_indexes(self, &store, options);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_dump_reference_sequence(self, &store, options);
    if (ret != 0) {
        goto out;
    }

    ret = kastore_close(&store);
    if (ret != 0) {
        ret = tsk_set_kas_error(ret);
        goto out;
    }
out:
    /* It's safe to close a kastore twice. */
    if (ret != 0) {
        kastore_close(&store);
    }
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_simplify(tsk_table_collection_t *self, const tsk_id_t *samples,
    tsk_size_t num_samples, tsk_flags_t options, tsk_id_t *node_map)
{
    int ret = 0;
    simplifier_t simplifier;
    tsk_id_t *local_samples = NULL;
    tsk_id_t u;

    /* Avoid calling to simplifier_free with uninit'd memory on error branches */
    tsk_memset(&simplifier, 0, sizeof(simplifier_t));

    if ((options & TSK_SIMPLIFY_KEEP_UNARY)
        && (options & TSK_SIMPLIFY_KEEP_UNARY_IN_INDIVIDUALS)) {
        ret = tsk_trace_error(TSK_ERR_KEEP_UNARY_MUTUALLY_EXCLUSIVE);
        goto out;
    }

    /* For now we don't bother with edge metadata, but it can easily be
     * implemented. */
    if (self->edges.metadata_length > 0) {
        ret = tsk_trace_error(TSK_ERR_CANT_PROCESS_EDGES_WITH_METADATA);
        goto out;
    }

    if (samples == NULL) {
        local_samples = tsk_malloc(self->nodes.num_rows * sizeof(*local_samples));
        if (local_samples == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
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

    ret = simplifier_init(&simplifier, samples, num_samples, self, options);
    if (ret != 0) {
        goto out;
    }
    ret = simplifier_run(&simplifier, node_map);
    if (ret != 0) {
        goto out;
    }
    if (!!(options & TSK_DEBUG)) {
        simplifier_print_state(&simplifier, tsk_get_debug_stream());
    }
    /* The indexes are invalidated now so drop them */
    ret = tsk_table_collection_drop_index(self, 0);
out:
    simplifier_free(&simplifier);
    tsk_safe_free(local_samples);
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_link_ancestors(tsk_table_collection_t *self, tsk_id_t *samples,
    tsk_size_t num_samples, tsk_id_t *ancestors, tsk_size_t num_ancestors,
    tsk_flags_t TSK_UNUSED(options), tsk_edge_table_t *result)
{
    int ret = 0;
    ancestor_mapper_t ancestor_mapper;

    tsk_memset(&ancestor_mapper, 0, sizeof(ancestor_mapper_t));

    if (self->edges.metadata_length > 0) {
        ret = tsk_trace_error(TSK_ERR_CANT_PROCESS_EDGES_WITH_METADATA);
        goto out;
    }

    ret = ancestor_mapper_init(
        &ancestor_mapper, samples, num_samples, ancestors, num_ancestors, self, result);
    if (ret != 0) {
        goto out;
    }
    ret = ancestor_mapper_run(&ancestor_mapper);
    if (ret != 0) {
        goto out;
    }
out:
    ancestor_mapper_free(&ancestor_mapper);
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_ibd_within(const tsk_table_collection_t *self,
    tsk_identity_segments_t *result, const tsk_id_t *samples, tsk_size_t num_samples,
    double min_span, double max_time, tsk_flags_t options)
{
    int ret = 0;
    tsk_ibd_finder_t ibd_finder;

    ret = tsk_identity_segments_init(result, self->nodes.num_rows, options);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_ibd_finder_init(&ibd_finder, self, result, min_span, max_time);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_ibd_finder_init_within(&ibd_finder, samples, num_samples);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_ibd_finder_run(&ibd_finder);
    if (ret != 0) {
        goto out;
    }
    if (!!(options & TSK_DEBUG)) {
        tsk_ibd_finder_print_state(&ibd_finder, tsk_get_debug_stream());
    }
out:
    tsk_ibd_finder_free(&ibd_finder);
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_ibd_between(const tsk_table_collection_t *self,
    tsk_identity_segments_t *result, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, double min_span,
    double max_time, tsk_flags_t options)
{
    int ret = 0;
    tsk_ibd_finder_t ibd_finder;

    ret = tsk_identity_segments_init(result, self->nodes.num_rows, options);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_ibd_finder_init(&ibd_finder, self, result, min_span, max_time);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_ibd_finder_init_between(
        &ibd_finder, num_sample_sets, sample_set_sizes, sample_sets);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_ibd_finder_run(&ibd_finder);
    if (ret != 0) {
        goto out;
    }
    if (!!(options & TSK_DEBUG)) {
        tsk_ibd_finder_print_state(&ibd_finder, tsk_get_debug_stream());
    }
out:
    tsk_ibd_finder_free(&ibd_finder);
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_sort(
    tsk_table_collection_t *self, const tsk_bookmark_t *start, tsk_flags_t options)
{
    int ret = 0;
    tsk_table_sorter_t sorter;

    ret = tsk_table_sorter_init(&sorter, self, options);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_sorter_run(&sorter, start);
    if (ret != 0) {
        goto out;
    }
out:
    tsk_table_sorter_free(&sorter);
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_canonicalise(tsk_table_collection_t *self, tsk_flags_t options)
{
    int ret = 0;
    tsk_id_t k;
    tsk_id_t *nodes = NULL;
    tsk_table_sorter_t sorter;
    tsk_flags_t subset_options = options & TSK_SUBSET_KEEP_UNREFERENCED;

    ret = tsk_table_sorter_init(&sorter, self, 0);
    if (ret != 0) {
        goto out;
    }
    sorter.sort_mutations = tsk_table_sorter_sort_mutations;
    sorter.sort_individuals = tsk_table_sorter_sort_individuals_canonical;

    nodes = tsk_malloc(self->nodes.num_rows * sizeof(*nodes));
    if (nodes == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    for (k = 0; k < (tsk_id_t) self->nodes.num_rows; k++) {
        nodes[k] = k;
    }
    ret = tsk_table_collection_subset(self, nodes, self->nodes.num_rows, subset_options);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_sorter_run(&sorter, NULL);
    if (ret != 0) {
        goto out;
    }
out:
    tsk_safe_free(nodes);
    tsk_table_sorter_free(&sorter);
    return ret;
}

/*
 * Remove any sites with duplicate positions, retaining only the *first*
 * one. Assumes the tables have been sorted, throwing an error if not.
 */
int TSK_WARN_UNUSED
tsk_table_collection_deduplicate_sites(
    tsk_table_collection_t *self, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_id_t ret_id;
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
    ret_id = tsk_table_collection_check_integrity(self, TSK_CHECK_SITE_ORDERING);
    if (ret_id != 0) {
        ret = (int) ret_id;
        goto out;
    }

    site_id_map = tsk_malloc(copy.num_rows * sizeof(*site_id_map));
    if (site_id_map == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    ret = tsk_site_table_clear(&self->sites);
    if (ret != 0) {
        goto out;
    }

    last_row.position = -1;
    site_id_map[0] = 0;
    for (j = 0; j < copy.num_rows; j++) {
        tsk_site_table_get_row_unsafe(&copy, (tsk_id_t) j, &row);
        if (row.position != last_row.position) {
            ret_id
                = tsk_site_table_add_row(&self->sites, row.position, row.ancestral_state,
                    row.ancestral_state_length, row.metadata, row.metadata_length);
            if (ret_id < 0) {
                ret = (int) ret_id;
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
tsk_table_collection_compute_mutation_parents(
    tsk_table_collection_t *self, tsk_flags_t options)
{
    int ret = 0;
    tsk_mutation_table_t *mutations = &self->mutations;
    tsk_id_t *parent_backup = NULL;
    bool restore_parents = false;

    if (!(options & TSK_NO_CHECK_INTEGRITY)) {
        if (mutations->num_rows > 0) {
            /* We need to wipe the parent column before computing, as otherwise invalid
             * parents can cause integrity checks to fail. We take a copy to restore on
             * error */
            parent_backup = tsk_malloc(mutations->num_rows * sizeof(*parent_backup));
            if (parent_backup == NULL) {
                ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
                goto out;
            }
            tsk_memcpy(parent_backup, mutations->parent,
                mutations->num_rows * sizeof(*parent_backup));
            /* Set the parent pointers to TSK_NULL */
            tsk_memset(mutations->parent, 0xff,
                mutations->num_rows * sizeof(*mutations->parent));
            restore_parents = true;
        }
        /* Safe to cast here as we're not counting trees */
        ret = (int) tsk_table_collection_check_integrity(self, TSK_CHECK_TREES);
        if (ret < 0) {
            goto out;
        }
    }

    ret = tsk_table_collection_compute_mutation_parents_to_array(
        self, self->mutations.parent);
    if (ret != 0) {
        goto out;
    }

out:
    if (ret != 0 && restore_parents) {
        tsk_memcpy(mutations->parent, parent_backup,
            mutations->num_rows * sizeof(*parent_backup));
    }
    tsk_safe_free(parent_backup);
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_compute_mutation_times(
    tsk_table_collection_t *self, double *random, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_id_t num_trees;
    const tsk_id_t *restrict I = self->indexes.edge_insertion_order;
    const tsk_id_t *restrict O = self->indexes.edge_removal_order;
    const tsk_edge_table_t edges = self->edges;
    const tsk_node_table_t nodes = self->nodes;
    const tsk_site_table_t sites = self->sites;
    const tsk_mutation_table_t mutations = self->mutations;
    const tsk_id_t M = (tsk_id_t) edges.num_rows;
    tsk_id_t tj, tk;
    tsk_id_t *parent = NULL;
    double *numerator = NULL;
    double *denominator = NULL;
    tsk_id_t u;
    double left, right, parent_time;
    tsk_id_t site;
    /* Using unsigned values here avoids potentially undefined behaviour */
    tsk_size_t j, mutation, first_mutation;
    tsk_bookmark_t skip_edges = { 0, 0, self->edges.num_rows, 0, 0, 0, 0, 0 };

    /* The random param is for future usage */
    if (random != NULL) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }

    /* First set the times to TSK_UNKNOWN_TIME so that check will succeed */
    for (j = 0; j < mutations.num_rows; j++) {
        mutations.time[j] = TSK_UNKNOWN_TIME;
    }
    /* TSK_CHECK_MUTATION_PARENTS isn't needed here as we're not using the parents */
    num_trees = tsk_table_collection_check_integrity(self, TSK_CHECK_TREES);
    if (num_trees < 0) {
        ret = (int) num_trees;
        goto out;
    }
    parent = tsk_malloc(nodes.num_rows * sizeof(*parent));
    numerator = tsk_malloc(nodes.num_rows * sizeof(*numerator));
    denominator = tsk_malloc(nodes.num_rows * sizeof(*denominator));
    if (parent == NULL || numerator == NULL || denominator == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memset(parent, 0xff, nodes.num_rows * sizeof(*parent));
    tsk_memset(numerator, 0, nodes.num_rows * sizeof(*numerator));
    tsk_memset(denominator, 0, nodes.num_rows * sizeof(*denominator));

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
            first_mutation = mutation;
            /* Count how many mutations each edge has to get our
               denominator */
            while (mutation < mutations.num_rows && mutations.site[mutation] == site) {
                denominator[mutations.node[mutation]]++;
                mutation++;
            }
            /* Go over the mutations again assigning times. As the sorting
               requirements guarantee that parents are before children, we assign
               oldest first */
            for (j = first_mutation; j < mutation; j++) {
                u = mutations.node[j];
                numerator[u]++;
                if (parent[u] == TSK_NULL) {
                    /* This mutation is above a root */
                    mutations.time[j] = nodes.time[u];
                } else {
                    parent_time = nodes.time[parent[u]];
                    mutations.time[j] = parent_time
                                        - (parent_time - nodes.time[u]) * numerator[u]
                                              / (denominator[u] + 1);
                }
            }
            /* Reset the book-keeping for the next site */
            for (j = first_mutation; j < mutation; j++) {
                u = mutations.node[j];
                numerator[u] = 0;
                denominator[u] = 0;
            }
            site++;
        }
        /* Move on to the next tree */
        left = right;
    }

    /* Now that mutations have times their sort order may have been invalidated, so
     * re-sort. Safe to cast the result to an int here because we're not counting
     * trees. */
    ret = (int) tsk_table_collection_check_integrity(self, TSK_CHECK_MUTATION_ORDERING);
    if (ret == TSK_ERR_UNSORTED_MUTATIONS) {
        ret = tsk_table_collection_sort(self, &skip_edges, 0);
        if (ret != 0) {
            goto out;
        }
    } else if (ret < 0) {
        goto out;
    }

out:
    tsk_safe_free(parent);
    tsk_safe_free(numerator);
    tsk_safe_free(denominator);
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_delete_older(
    tsk_table_collection_t *self, double time, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_edge_t edge;
    tsk_mutation_t mutation;
    tsk_migration_t migration;
    tsk_edge_table_t edges;
    tsk_mutation_table_t mutations;
    tsk_migration_table_t migrations;
    const double *restrict node_time = self->nodes.time;
    tsk_id_t j, ret_id, parent;
    double mutation_time;
    tsk_id_t *mutation_map = NULL;

    memset(&edges, 0, sizeof(edges));
    memset(&mutations, 0, sizeof(mutations));
    memset(&migrations, 0, sizeof(migrations));

    ret = tsk_edge_table_copy(&self->edges, &edges, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_edge_table_clear(&self->edges);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < (tsk_id_t) edges.num_rows; j++) {
        tsk_edge_table_get_row_unsafe(&edges, j, &edge);
        if (node_time[edge.parent] <= time) {
            ret_id = tsk_edge_table_add_row(&self->edges, edge.left, edge.right,
                edge.parent, edge.child, edge.metadata, edge.metadata_length);
            if (ret_id < 0) {
                ret = (int) ret_id;
                goto out;
            }
        }
    }
    /* Calling x_table_free multiple times is safe, so get rid of the
     * extra edge table memory as soon as we can. */
    tsk_edge_table_free(&edges);

    mutation_map = tsk_malloc(self->mutations.num_rows * sizeof(*mutation_map));
    if (mutation_map == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    ret = tsk_mutation_table_copy(&self->mutations, &mutations, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_mutation_table_clear(&self->mutations);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < (tsk_id_t) mutations.num_rows; j++) {
        tsk_mutation_table_get_row_unsafe(&mutations, j, &mutation);
        mutation_time = tsk_is_unknown_time(mutation.time) ? node_time[mutation.node]
                                                           : mutation.time;
        mutation_map[j] = TSK_NULL;
        if (mutation_time < time) {
            ret_id = tsk_mutation_table_add_row(&self->mutations, mutation.site,
                mutation.node, mutation.parent, mutation.time, mutation.derived_state,
                mutation.derived_state_length, mutation.metadata,
                mutation.metadata_length);
            if (ret_id < 0) {
                ret = (int) ret_id;
                goto out;
            }
            mutation_map[j] = ret_id;
        }
    }
    tsk_mutation_table_free(&mutations);
    for (j = 0; j < (tsk_id_t) self->mutations.num_rows; j++) {
        parent = self->mutations.parent[j];
        if (parent != TSK_NULL) {
            self->mutations.parent[j] = mutation_map[parent];
        }
    }

    ret = tsk_migration_table_copy(&self->migrations, &migrations, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_migration_table_clear(&self->migrations);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < (tsk_id_t) migrations.num_rows; j++) {
        tsk_migration_table_get_row_unsafe(&migrations, j, &migration);
        if (migration.time < time) {
            ret_id = tsk_migration_table_add_row(&self->migrations, migration.left,
                migration.right, migration.node, migration.source, migration.dest,
                migration.time, migration.metadata, migration.metadata_length);
            if (ret_id < 0) {
                ret = (int) ret_id;
                goto out;
            }
        }
    }
    tsk_migration_table_free(&migrations);
out:
    tsk_edge_table_free(&edges);
    tsk_mutation_table_free(&mutations);
    tsk_migration_table_free(&migrations);
    tsk_safe_free(mutation_map);
    return ret;
}

int
tsk_table_collection_record_num_rows(
    const tsk_table_collection_t *self, tsk_bookmark_t *position)
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
tsk_table_collection_truncate(tsk_table_collection_t *tables, tsk_bookmark_t *position)
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
tsk_table_collection_clear(tsk_table_collection_t *self, tsk_flags_t options)
{
    int ret = 0;
    bool clear_provenance = !!(options & TSK_CLEAR_PROVENANCE);
    bool clear_metadata_schemas = !!(options & TSK_CLEAR_METADATA_SCHEMAS);
    bool clear_ts_metadata = !!(options & TSK_CLEAR_TS_METADATA_AND_SCHEMA);
    tsk_bookmark_t rows_to_retain
        = { .provenances = clear_provenance ? 0 : self->provenances.num_rows };

    ret = tsk_table_collection_truncate(self, &rows_to_retain);
    if (ret != 0) {
        goto out;
    }

    if (clear_metadata_schemas) {
        ret = tsk_individual_table_set_metadata_schema(&self->individuals, "", 0);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_node_table_set_metadata_schema(&self->nodes, "", 0);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_edge_table_set_metadata_schema(&self->edges, "", 0);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_migration_table_set_metadata_schema(&self->migrations, "", 0);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_site_table_set_metadata_schema(&self->sites, "", 0);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_mutation_table_set_metadata_schema(&self->mutations, "", 0);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_population_table_set_metadata_schema(&self->populations, "", 0);
        if (ret != 0) {
            goto out;
        }
    }

    if (clear_ts_metadata) {
        ret = tsk_table_collection_set_metadata(self, "", 0);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_table_collection_set_metadata_schema(self, "", 0);
        if (ret != 0) {
            goto out;
        }
    }

out:
    return ret;
}

static int
tsk_table_collection_add_and_remap_node(tsk_table_collection_t *self,
    const tsk_table_collection_t *other, tsk_id_t node_id, tsk_id_t *individual_map,
    tsk_id_t *population_map, tsk_id_t *node_map, bool add_populations)
{
    int ret = 0;
    tsk_id_t ret_id, new_ind, new_pop;
    tsk_node_t node;
    tsk_individual_t ind;
    tsk_population_t pop;

    ret = tsk_node_table_get_row(&other->nodes, node_id, &node);
    if (ret < 0) {
        goto out;
    }
    new_ind = TSK_NULL;
    if (node.individual != TSK_NULL) {
        if (individual_map[node.individual] == TSK_NULL) {
            ret = tsk_individual_table_get_row(
                &other->individuals, node.individual, &ind);
            if (ret < 0) {
                goto out;
            }
            ret_id = tsk_individual_table_add_row(&self->individuals, ind.flags,
                ind.location, ind.location_length, ind.parents, ind.parents_length,
                ind.metadata, ind.metadata_length);
            if (ret < 0) {
                ret = (int) ret_id;
                goto out;
            }
            individual_map[node.individual] = ret_id;
        }
        new_ind = individual_map[node.individual];
    }
    new_pop = TSK_NULL;
    if (node.population != TSK_NULL) {
        // keep same pops if add_populations is False
        if (!add_populations) {
            population_map[node.population] = node.population;
        }
        if (population_map[node.population] == TSK_NULL) {
            ret = tsk_population_table_get_row(
                &other->populations, node.population, &pop);
            if (ret < 0) {
                goto out;
            }
            ret_id = tsk_population_table_add_row(
                &self->populations, pop.metadata, pop.metadata_length);
            if (ret_id < 0) {
                ret = (int) ret_id;
                goto out;
            }
            population_map[node.population] = ret_id;
        }
        new_pop = population_map[node.population];
    }
    ret_id = tsk_node_table_add_row(&self->nodes, node.flags, node.time, new_pop,
        new_ind, node.metadata, node.metadata_length);
    if (ret_id < 0) {
        ret = (int) ret_id;
        goto out;
    }
    node_map[node.id] = ret_id;

out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_subset(tsk_table_collection_t *self, const tsk_id_t *nodes,
    tsk_size_t num_nodes, tsk_flags_t options)
{
    int ret = 0;
    tsk_id_t ret_id, j, k, parent_ind, new_parent, new_child, new_node, site_id;
    tsk_size_t num_parents;
    tsk_individual_t ind;
    tsk_edge_t edge;
    tsk_id_t *node_map = NULL;
    tsk_id_t *individual_map = NULL;
    tsk_id_t *population_map = NULL;
    tsk_id_t *site_map = NULL;
    tsk_id_t *mutation_map = NULL;
    tsk_table_collection_t tables;
    tsk_population_t pop;
    tsk_site_t site;
    tsk_mutation_t mut;
    bool keep_unreferenced = !!(options & TSK_SUBSET_KEEP_UNREFERENCED);
    bool no_change_populations = !!(options & TSK_SUBSET_NO_CHANGE_POPULATIONS);

    ret = tsk_table_collection_copy(self, &tables, 0);
    if (ret != 0) {
        goto out;
    }
    /* Not calling TSK_CHECK_TREES so casting to int is safe */
    ret = (int) tsk_table_collection_check_integrity(self, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_clear(self, 0);
    if (ret != 0) {
        goto out;
    }

    node_map = tsk_malloc(tables.nodes.num_rows * sizeof(*node_map));
    individual_map = tsk_malloc(tables.individuals.num_rows * sizeof(*individual_map));
    population_map = tsk_malloc(tables.populations.num_rows * sizeof(*population_map));
    site_map = tsk_malloc(tables.sites.num_rows * sizeof(*site_map));
    mutation_map = tsk_malloc(tables.mutations.num_rows * sizeof(*mutation_map));
    if (node_map == NULL || individual_map == NULL || population_map == NULL
        || site_map == NULL || mutation_map == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memset(node_map, 0xff, tables.nodes.num_rows * sizeof(*node_map));
    tsk_memset(
        individual_map, 0xff, tables.individuals.num_rows * sizeof(*individual_map));
    tsk_memset(
        population_map, 0xff, tables.populations.num_rows * sizeof(*population_map));
    tsk_memset(site_map, 0xff, tables.sites.num_rows * sizeof(*site_map));
    tsk_memset(mutation_map, 0xff, tables.mutations.num_rows * sizeof(*mutation_map));

    if (no_change_populations) {
        ret = tsk_population_table_copy(
            &tables.populations, &self->populations, TSK_NO_INIT);
        if (ret < 0) {
            goto out;
        }
        for (k = 0; k < (tsk_id_t) tables.populations.num_rows; k++) {
            population_map[k] = k;
        }
    }

    // First do individuals so they stay in the same order.
    // So we can remap individual parents and not rely on sortedness,
    // we first check who to keep; then build the individual map, and
    // finally populate the tables.
    if (keep_unreferenced) {
        for (k = 0; k < (tsk_id_t) tables.individuals.num_rows; k++) {
            // put a non-NULL value here; fill in the actual order next
            individual_map[k] = 0;
        }
    } else {
        for (k = 0; k < (tsk_id_t) num_nodes; k++) {
            if (nodes[k] < 0 || nodes[k] >= (tsk_id_t) tables.nodes.num_rows) {
                ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
                goto out;
            }
            j = tables.nodes.individual[nodes[k]];
            if (j != TSK_NULL) {
                individual_map[j] = 0;
            }
        }
    }
    j = 0;
    for (k = 0; k < (tsk_id_t) tables.individuals.num_rows; k++) {
        if (individual_map[k] != TSK_NULL) {
            individual_map[k] = j;
            j++;
        }
    }
    for (k = 0; k < (tsk_id_t) tables.individuals.num_rows; k++) {
        if (individual_map[k] != TSK_NULL) {
            tsk_individual_table_get_row_unsafe(&tables.individuals, k, &ind);
            num_parents = 0;
            for (j = 0; j < (tsk_id_t) ind.parents_length; j++) {
                parent_ind = ind.parents[j];
                new_parent = parent_ind;
                if (parent_ind != TSK_NULL) {
                    new_parent = individual_map[parent_ind];
                }
                if ((parent_ind == TSK_NULL) || (new_parent != TSK_NULL)) {
                    /* Beware: this modifies the parents column of tables.individuals
                     * in-place! But it's OK as we don't use it again. */
                    ind.parents[num_parents] = new_parent;
                    num_parents++;
                }
            }
            ret_id = tsk_individual_table_add_row(&self->individuals, ind.flags,
                ind.location, ind.location_length, ind.parents, num_parents,
                ind.metadata, ind.metadata_length);
            if (ret_id < 0) {
                ret = (int) ret_id;
                goto out;
            }
            tsk_bug_assert(individual_map[k] == ret_id);
        }
    }

    // Nodes and populations
    for (k = 0; k < (tsk_id_t) num_nodes; k++) {
        ret = tsk_table_collection_add_and_remap_node(
            self, &tables, nodes[k], individual_map, population_map, node_map, true);
        if (ret < 0) {
            goto out;
        }
    }

    /* TODO: Subset the migrations table. We would need to make sure
     * that we don't remove populations that are referenced, so it would
     * need to be done before the next code block. */
    if (tables.migrations.num_rows != 0) {
        ret = tsk_trace_error(TSK_ERR_MIGRATIONS_NOT_SUPPORTED);
        goto out;
    }

    if (keep_unreferenced) {
        // Keep unused populations
        for (k = 0; k < (tsk_id_t) tables.populations.num_rows; k++) {
            if (population_map[k] == TSK_NULL) {
                tsk_population_table_get_row_unsafe(&tables.populations, k, &pop);
                ret_id = tsk_population_table_add_row(
                    &self->populations, pop.metadata, pop.metadata_length);
                if (ret_id < 0) {
                    ret = (int) ret_id;
                    goto out;
                }
            }
        }
    }

    // Edges
    for (k = 0; k < (tsk_id_t) tables.edges.num_rows; k++) {
        tsk_edge_table_get_row_unsafe(&tables.edges, k, &edge);
        new_parent = node_map[edge.parent];
        new_child = node_map[edge.child];
        if ((new_parent != TSK_NULL) && (new_child != TSK_NULL)) {
            ret_id = tsk_edge_table_add_row(&self->edges, edge.left, edge.right,
                new_parent, new_child, edge.metadata, edge.metadata_length);
            if (ret_id < 0) {
                ret = (int) ret_id;
                goto out;
            }
        }
    }

    // Mutations and sites
    // Make a first pass through to build the mutation_map so that
    // mutation parent can be remapped even if the table is not in order.
    j = 0;
    for (k = 0; k < (tsk_id_t) tables.mutations.num_rows; k++) {
        if (node_map[tables.mutations.node[k]] != TSK_NULL) {
            mutation_map[k] = j;
            j++;
            site_id = tables.mutations.site[k];
            if (site_map[site_id] == TSK_NULL) {
                // Insert a temporary non-NULL value
                site_map[site_id] = 1;
            }
        }
    }
    // Keep retained sites in their original order
    j = 0;
    for (k = 0; k < (tsk_id_t) tables.sites.num_rows; k++) {
        if (keep_unreferenced || site_map[k] != TSK_NULL) {
            tsk_site_table_get_row_unsafe(&tables.sites, k, &site);
            ret_id = tsk_site_table_add_row(&self->sites, site.position,
                site.ancestral_state, site.ancestral_state_length, site.metadata,
                site.metadata_length);
            if (ret_id < 0) {
                ret = (int) ret_id;
                goto out;
            }
            site_map[k] = j;
            j++;
        }
    }
    for (k = 0; k < (tsk_id_t) tables.mutations.num_rows; k++) {
        tsk_mutation_table_get_row_unsafe(&tables.mutations, k, &mut);
        new_node = node_map[mut.node];
        if (new_node != TSK_NULL) {
            new_parent = TSK_NULL;
            if (mut.parent != TSK_NULL) {
                new_parent = mutation_map[mut.parent];
            }
            ret_id = tsk_mutation_table_add_row(&self->mutations, site_map[mut.site],
                new_node, new_parent, mut.time, mut.derived_state,
                mut.derived_state_length, mut.metadata, mut.metadata_length);
            if (ret_id < 0) {
                ret = (int) ret_id;
                goto out;
            }
            tsk_bug_assert(mutation_map[mut.id] == ret_id);
        }
        if (ret < 0) {
            goto out;
        }
    }

    ret = 0;
out:
    tsk_safe_free(node_map);
    tsk_safe_free(individual_map);
    tsk_safe_free(population_map);
    tsk_safe_free(site_map);
    tsk_safe_free(mutation_map);
    tsk_table_collection_free(&tables);
    return ret;
}

static int
tsk_check_subset_equality(tsk_table_collection_t *self,
    const tsk_table_collection_t *other, const tsk_id_t *other_node_mapping,
    tsk_size_t num_shared_nodes)
{
    int ret = 0;
    tsk_id_t k, i;
    tsk_id_t *self_nodes = NULL;
    tsk_id_t *other_nodes = NULL;
    tsk_table_collection_t self_copy;
    tsk_table_collection_t other_copy;

    tsk_memset(&self_copy, 0, sizeof(self_copy));
    tsk_memset(&other_copy, 0, sizeof(other_copy));
    self_nodes = tsk_malloc(num_shared_nodes * sizeof(*self_nodes));
    other_nodes = tsk_malloc(num_shared_nodes * sizeof(*other_nodes));
    if (self_nodes == NULL || other_nodes == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    i = 0;
    for (k = 0; k < (tsk_id_t) other->nodes.num_rows; k++) {
        if (other_node_mapping[k] != TSK_NULL) {
            self_nodes[i] = other_node_mapping[k];
            other_nodes[i] = k;
            i++;
        }
    }

    ret = tsk_table_collection_copy(self, &self_copy, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_copy(other, &other_copy, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_subset(&self_copy, self_nodes, num_shared_nodes, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_subset(&other_copy, other_nodes, num_shared_nodes, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_canonicalise(&self_copy, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_canonicalise(&other_copy, 0);
    if (ret != 0) {
        goto out;
    }
    if (!tsk_table_collection_equals(&self_copy, &other_copy,
            TSK_CMP_IGNORE_TS_METADATA | TSK_CMP_IGNORE_PROVENANCE
                | TSK_CMP_IGNORE_REFERENCE_SEQUENCE)) {
        ret = tsk_trace_error(TSK_ERR_UNION_DIFF_HISTORIES);
        goto out;
    }

out:
    tsk_table_collection_free(&self_copy);
    tsk_table_collection_free(&other_copy);
    tsk_safe_free(other_nodes);
    tsk_safe_free(self_nodes);
    return ret;
}

int TSK_WARN_UNUSED
tsk_table_collection_union(tsk_table_collection_t *self,
    const tsk_table_collection_t *other, const tsk_id_t *other_node_mapping,
    tsk_flags_t options)
{
    int ret = 0;
    tsk_id_t ret_id, k, i, new_parent, new_child;
    tsk_size_t num_shared_nodes = 0;
    tsk_size_t num_individuals_self = self->individuals.num_rows;
    tsk_edge_t edge;
    tsk_mutation_t mut;
    tsk_site_t site;
    tsk_id_t *node_map = NULL;
    tsk_id_t *individual_map = NULL;
    tsk_id_t *population_map = NULL;
    tsk_id_t *site_map = NULL;
    bool add_populations = !(options & TSK_UNION_NO_ADD_POP);
    bool check_shared_portion = !(options & TSK_UNION_NO_CHECK_SHARED);
    bool all_edges = !!(options & TSK_UNION_ALL_EDGES);
    bool all_mutations = !!(options & TSK_UNION_ALL_MUTATIONS);

    /* Not calling TSK_CHECK_TREES so casting to int is safe */
    ret = (int) tsk_table_collection_check_integrity(self, 0);
    if (ret != 0) {
        goto out;
    }
    ret = (int) tsk_table_collection_check_integrity(other, 0);
    if (ret != 0) {
        goto out;
    }
    for (k = 0; k < (tsk_id_t) other->nodes.num_rows; k++) {
        if (other_node_mapping[k] >= (tsk_id_t) self->nodes.num_rows
            || other_node_mapping[k] < TSK_NULL) {
            ret = tsk_trace_error(TSK_ERR_UNION_BAD_MAP);
            goto out;
        }
        if (other_node_mapping[k] != TSK_NULL) {
            num_shared_nodes++;
        }
    }

    if (check_shared_portion) {
        ret = tsk_check_subset_equality(
            self, other, other_node_mapping, num_shared_nodes);
        if (ret != 0) {
            goto out;
        }
    }

    // Maps relating the IDs in other to the new IDs in self.
    node_map = tsk_malloc(other->nodes.num_rows * sizeof(*node_map));
    individual_map = tsk_malloc(other->individuals.num_rows * sizeof(*individual_map));
    population_map = tsk_malloc(other->populations.num_rows * sizeof(*population_map));
    site_map = tsk_malloc(other->sites.num_rows * sizeof(*site_map));
    if (node_map == NULL || individual_map == NULL || population_map == NULL
        || site_map == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memset(node_map, 0xff, other->nodes.num_rows * sizeof(*node_map));
    tsk_memset(
        individual_map, 0xff, other->individuals.num_rows * sizeof(*individual_map));
    tsk_memset(
        population_map, 0xff, other->populations.num_rows * sizeof(*population_map));
    tsk_memset(site_map, 0xff, other->sites.num_rows * sizeof(*site_map));

    /* We have to map the individuals who are linked to nodes in the intersection first
       as otherwise an individual linked to one node in the intersection and one in
       `other` would be duplicated. We assume that the individual in `self` takes
       priority.
     */
    for (k = 0; k < (tsk_id_t) other->nodes.num_rows; k++) {
        if (other_node_mapping[k] != TSK_NULL
            && other->nodes.individual[k] != TSK_NULL) {
            individual_map[other->nodes.individual[k]]
                = self->nodes.individual[other_node_mapping[k]];
        }
    }
    // nodes, individuals, populations
    for (k = 0; k < (tsk_id_t) other->nodes.num_rows; k++) {
        if (other_node_mapping[k] != TSK_NULL) {
            node_map[k] = other_node_mapping[k];
        } else {
            ret = tsk_table_collection_add_and_remap_node(self, other, k, individual_map,
                population_map, node_map, add_populations);
            if (ret < 0) {
                goto out;
            }
        }
    }

    /* Now we know the full individual map we can remap the parents of the new
     * individuals*/
    for (k = (tsk_id_t) self->individuals.parents_offset[num_individuals_self];
         k < (tsk_id_t) self->individuals.parents_length; k++) {
        if (self->individuals.parents[k] != TSK_NULL) {
            self->individuals.parents[k] = individual_map[self->individuals.parents[k]];
        }
    }

    // edges
    for (k = 0; k < (tsk_id_t) other->edges.num_rows; k++) {
        tsk_edge_table_get_row_unsafe(&other->edges, k, &edge);
        if (all_edges || (other_node_mapping[edge.parent] == TSK_NULL)
            || (other_node_mapping[edge.child] == TSK_NULL)) {
            new_parent = node_map[edge.parent];
            new_child = node_map[edge.child];
            ret_id = tsk_edge_table_add_row(&self->edges, edge.left, edge.right,
                new_parent, new_child, edge.metadata, edge.metadata_length);
            if (ret_id < 0) {
                ret = (int) ret_id;
                goto out;
            }
        }
    }

    // sites
    // first do the "disjoint" (all_mutations) case, where we just add all sites;
    // otherwise we want to just add sites for new mutations
    if (all_mutations) {
        for (k = 0; k < (tsk_id_t) other->sites.num_rows; k++) {
            tsk_site_table_get_row_unsafe(&other->sites, k, &site);
            ret_id = tsk_site_table_add_row(&self->sites, site.position,
                site.ancestral_state, site.ancestral_state_length, site.metadata,
                site.metadata_length);
            if (ret_id < 0) {
                ret = (int) ret_id;
                goto out;
            }
            site_map[site.id] = ret_id;
        }
    }

    // mutations (and maybe sites)
    i = 0;
    for (k = 0; k < (tsk_id_t) other->sites.num_rows; k++) {
        tsk_site_table_get_row_unsafe(&other->sites, k, &site);
        while ((i < (tsk_id_t) other->mutations.num_rows)
               && (other->mutations.site[i] == site.id)) {
            tsk_mutation_table_get_row_unsafe(&other->mutations, i, &mut);
            if (all_mutations || (other_node_mapping[mut.node] == TSK_NULL)) {
                if (site_map[site.id] == TSK_NULL) {
                    ret_id = tsk_site_table_add_row(&self->sites, site.position,
                        site.ancestral_state, site.ancestral_state_length, site.metadata,
                        site.metadata_length);
                    if (ret_id < 0) {
                        ret = (int) ret_id;
                        goto out;
                    }
                    site_map[site.id] = ret_id;
                }
                // the parents will be recomputed later
                new_parent = TSK_NULL;
                ret_id = tsk_mutation_table_add_row(&self->mutations, site_map[site.id],
                    node_map[mut.node], new_parent, mut.time, mut.derived_state,
                    mut.derived_state_length, mut.metadata, mut.metadata_length);
                if (ret_id < 0) {
                    ret = (int) ret_id;
                    goto out;
                }
            }
            i++;
        }
    }

    /* TODO: Union of the Migrations Table. The only hindrance to performing the
     * union operation on Migrations Tables is that tsk_table_collection_sort
     * does not sort migrations by time, and instead throws an error. */
    if (self->migrations.num_rows != 0 || other->migrations.num_rows != 0) {
        ret = tsk_trace_error(TSK_ERR_MIGRATIONS_NOT_SUPPORTED);
        goto out;
    }

    // sorting, deduplicating, and computing parents
    ret = tsk_table_collection_sort(self, 0, 0);
    if (ret < 0) {
        goto out;
    }

    ret = tsk_table_collection_deduplicate_sites(self, 0);
    if (ret < 0) {
        goto out;
    }

    // need to sort again since after deduplicating sites, mutations
    // may not be sorted by time within sites
    ret = tsk_table_collection_sort(self, 0, 0);
    if (ret < 0) {
        goto out;
    }

    ret = tsk_table_collection_build_index(self, 0);
    if (ret < 0) {
        goto out;
    }

    ret = tsk_table_collection_compute_mutation_parents(self, 0);
    if (ret < 0) {
        goto out;
    }

out:
    tsk_safe_free(node_map);
    tsk_safe_free(individual_map);
    tsk_safe_free(population_map);
    tsk_safe_free(site_map);
    return ret;
}

static int
cmp_edge_cl(const void *a, const void *b)
{
    const tsk_edge_t *ia = (const tsk_edge_t *) a;
    const tsk_edge_t *ib = (const tsk_edge_t *) b;
    int ret = (ia->parent > ib->parent) - (ia->parent < ib->parent);
    if (ret == 0) {
        ret = (ia->child > ib->child) - (ia->child < ib->child);
        if (ret == 0) {
            ret = (ia->left > ib->left) - (ia->left < ib->left);
        }
    }
    return ret;
}

/* Squash the edges in the specified array in place. The output edges will
 * be sorted by (child_id, left).
 */

int TSK_WARN_UNUSED
tsk_squash_edges(tsk_edge_t *edges, tsk_size_t num_edges, tsk_size_t *num_output_edges)
{
    int ret = 0;
    tsk_size_t j, k, l;

    if (num_edges < 2) {
        *num_output_edges = num_edges;
        return ret;
    }

    qsort(edges, (size_t) num_edges, sizeof(tsk_edge_t), cmp_edge_cl);
    j = 0;
    l = 0;
    for (k = 1; k < num_edges; k++) {
        if (edges[k - 1].metadata_length > 0) {
            ret = tsk_trace_error(TSK_ERR_CANT_PROCESS_EDGES_WITH_METADATA);
            goto out;
        }

        /* Check for overlapping edges. */
        if (edges[k - 1].parent == edges[k].parent
            && edges[k - 1].child == edges[k].child
            && edges[k - 1].right > edges[k].left) {
            ret = tsk_trace_error(TSK_ERR_BAD_EDGES_CONTRADICTORY_CHILDREN);
            goto out;
        }

        /* Add squashed edge. */
        if (edges[k - 1].parent != edges[k].parent || edges[k - 1].right != edges[k].left
            || edges[j].child != edges[k].child) {

            edges[l].left = edges[j].left;
            edges[l].right = edges[k - 1].right;
            edges[l].parent = edges[j].parent;
            edges[l].child = edges[j].child;

            j = k;
            l++;
        }
    }
    edges[l].left = edges[j].left;
    edges[l].right = edges[k - 1].right;
    edges[l].parent = edges[j].parent;
    edges[l].child = edges[j].child;

    *num_output_edges = (tsk_size_t) l + 1;

out:
    return ret;
}
