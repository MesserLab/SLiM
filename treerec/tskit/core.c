/*
 * MIT License
 *
 * Copyright (c) 2019 Tskit Developers
 * Copyright (c) 2015-2018 University of Oxford
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <math.h>

#include <kastore.h>
#include <tskit/core.h>

#define UUID_NUM_BYTES 16

#if defined(_WIN32)

#include <windows.h>
#include <wincrypt.h>

static int TSK_WARN_UNUSED
get_random_bytes(uint8_t *buf)
{
    /* Based on CPython's code in bootstrap_hash.c */
    int ret = TSK_ERR_GENERATE_UUID;
    HCRYPTPROV hCryptProv = NULL;

    if (!CryptAcquireContext(
            &hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        goto out;
    }
    if (!CryptGenRandom(hCryptProv, (DWORD) UUID_NUM_BYTES, buf)) {
        goto out;
    }
    if (!CryptReleaseContext(hCryptProv, 0)) {
        hCryptProv = NULL;
        goto out;
    }
    hCryptProv = NULL;
    ret = 0;
out:
    if (hCryptProv != NULL) {
        CryptReleaseContext(hCryptProv, 0);
    }
    return ret;
}

#else

/* Assuming the existance of /dev/urandom on Unix platforms */
static int TSK_WARN_UNUSED
get_random_bytes(uint8_t *buf)
{
    int ret = TSK_ERR_GENERATE_UUID;
    FILE *f = fopen("/dev/urandom", "r");

    if (f == NULL) {
        goto out;
    }
    if (fread(buf, UUID_NUM_BYTES, 1, f) != 1) {
        goto out;
    }
    if (fclose(f) != 0) {
        goto out;
    }
    ret = 0;
out:
    return ret;
}

#endif

/* Generate a new UUID4 using a system-generated source of randomness.
 * Note that this function writes a NULL terminator to the end of this
 * string, so that the total length of the buffer must be 37 bytes.
 */
int
tsk_generate_uuid(char *dest, int TSK_UNUSED(flags))
{
    int ret = 0;
    uint8_t buf[UUID_NUM_BYTES];
    const char *pattern
        = "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x";

    ret = get_random_bytes(buf);
    if (ret != 0) {
        goto out;
    }
    if (snprintf(dest, TSK_UUID_SIZE + 1, pattern, buf[0], buf[1], buf[2], buf[3],
            buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11], buf[12],
            buf[13], buf[14], buf[15])
        < 0) {
        ret = TSK_ERR_GENERATE_UUID;
        goto out;
    }
out:
    return ret;
}
static const char *
tsk_strerror_internal(int err)
{
    const char *ret = "Unknown error";

    switch (err) {
        case 0:
            ret = "Normal exit condition. This is not an error!";
            break;

        /* General errors */
        case TSK_ERR_GENERIC:
            ret = "Generic error; please file a bug report";
            break;
        case TSK_ERR_NO_MEMORY:
            ret = "Out of memory";
            break;
        case TSK_ERR_IO:
            if (errno != 0) {
                ret = strerror(errno);
            } else {
                ret = "Unspecified IO error";
            }
            break;
        case TSK_ERR_BAD_PARAM_VALUE:
            ret = "Bad parameter value provided";
            break;
        case TSK_ERR_BUFFER_OVERFLOW:
            ret = "Supplied buffer is too small";
            break;
        case TSK_ERR_UNSUPPORTED_OPERATION:
            ret = "Operation cannot be performed in current configuration";
            break;
        case TSK_ERR_GENERATE_UUID:
            ret = "Error generating UUID";
            break;
        case TSK_ERR_EOF:
            ret = "End of file";
            break;

        /* File format errors */
        case TSK_ERR_FILE_FORMAT:
            ret = "File format error";
            break;
        case TSK_ERR_FILE_VERSION_TOO_OLD:
            ret = "tskit file version too old. Please upgrade using the "
                  "'tskit upgrade' command";
            break;
        case TSK_ERR_FILE_VERSION_TOO_NEW:
            ret = "tskit file version is too new for this instance. "
                  "Please upgrade tskit to the latest version";
            break;
        case TSK_ERR_REQUIRED_COL_NOT_FOUND:
            ret = "A required column was not found in the file.";
            break;
        case TSK_ERR_BOTH_COLUMNS_REQUIRED:
            ret = "Both columns in a related pair must be provided";
            break;

        /* Out of bounds errors */
        case TSK_ERR_BAD_OFFSET:
            ret = "Bad offset provided in input array";
            break;
        case TSK_ERR_OUT_OF_BOUNDS:
            ret = "Object reference out of bounds";
            break;
        case TSK_ERR_NODE_OUT_OF_BOUNDS:
            ret = "Node out of bounds";
            break;
        case TSK_ERR_EDGE_OUT_OF_BOUNDS:
            ret = "Edge out of bounds";
            break;
        case TSK_ERR_POPULATION_OUT_OF_BOUNDS:
            ret = "Population out of bounds";
            break;
        case TSK_ERR_SITE_OUT_OF_BOUNDS:
            ret = "Site out of bounds";
            break;
        case TSK_ERR_MUTATION_OUT_OF_BOUNDS:
            ret = "Mutation out of bounds";
            break;
        case TSK_ERR_MIGRATION_OUT_OF_BOUNDS:
            ret = "Migration out of bounds";
            break;
        case TSK_ERR_INDIVIDUAL_OUT_OF_BOUNDS:
            ret = "Individual out of bounds";
            break;
        case TSK_ERR_PROVENANCE_OUT_OF_BOUNDS:
            ret = "Provenance out of bounds";
            break;
        case TSK_ERR_TIME_NONFINITE:
            ret = "Times must be finite";
            break;
        case TSK_ERR_GENOME_COORDS_NONFINITE:
            ret = "Genome coordinates must be finite numbers";
            break;

        /* Edge errors */
        case TSK_ERR_NULL_PARENT:
            ret = "Edge in parent is null";
            break;
        case TSK_ERR_NULL_CHILD:
            ret = "Edge in parent is null";
            break;
        case TSK_ERR_EDGES_NOT_SORTED_PARENT_TIME:
            ret = "Edges must be listed in (time[parent], child, left) order;"
                  " time[parent] order violated";
            break;
        case TSK_ERR_EDGES_NONCONTIGUOUS_PARENTS:
            ret = "All edges for a given parent must be contiguous";
            break;
        case TSK_ERR_EDGES_NOT_SORTED_CHILD:
            ret = "Edges must be listed in (time[parent], child, left) order;"
                  " child order violated";
            break;
        case TSK_ERR_EDGES_NOT_SORTED_LEFT:
            ret = "Edges must be listed in (time[parent], child, left) order;"
                  " left order violated";
            break;
        case TSK_ERR_BAD_NODE_TIME_ORDERING:
            ret = "time[parent] must be greater than time[child]";
            break;
        case TSK_ERR_BAD_EDGE_INTERVAL:
            ret = "Bad edge interval where right <= left";
            break;
        case TSK_ERR_DUPLICATE_EDGES:
            ret = "Duplicate edges provided";
            break;
        case TSK_ERR_RIGHT_GREATER_SEQ_LENGTH:
            ret = "Right coordinate > sequence length";
            break;
        case TSK_ERR_LEFT_LESS_ZERO:
            ret = "Left coordinate must be >= 0";
            break;
        case TSK_ERR_BAD_EDGES_CONTRADICTORY_CHILDREN:
            ret = "Bad edges: contradictory children for a given parent over "
                  "an interval";
            break;
        case TSK_ERR_CANT_PROCESS_EDGES_WITH_METADATA:
            ret = "Can't squash, flush, simplify or link ancestors with edges that have "
                  "non-empty metadata";
            break;

        /* Site errors */
        case TSK_ERR_UNSORTED_SITES:
            ret = "Sites must be provided in strictly increasing position order";
            break;
        case TSK_ERR_DUPLICATE_SITE_POSITION:
            ret = "Duplicate site positions";
            break;
        case TSK_ERR_BAD_SITE_POSITION:
            ret = "Site positions must be between 0 and sequence_length";
            break;

        /* Mutation errors */
        case TSK_ERR_MUTATION_PARENT_DIFFERENT_SITE:
            ret = "Specified parent mutation is at a different site";
            break;
        case TSK_ERR_MUTATION_PARENT_EQUAL:
            ret = "Parent mutation refers to itself";
            break;
        case TSK_ERR_MUTATION_PARENT_AFTER_CHILD:
            ret = "Parent mutation ID must be < current ID";
            break;
        case TSK_ERR_INCONSISTENT_MUTATIONS:
            ret = "Inconsistent mutations: state already equal to derived state";
            break;
        case TSK_ERR_UNSORTED_MUTATIONS:
            ret = "Mutations must be provided in non-decreasing site order and "
                  "non-increasing time order within each site";
            break;
        case TSK_ERR_MUTATION_TIME_YOUNGER_THAN_NODE:
            ret = "A mutation's time must be >= the node time, or be marked as "
                  "'unknown'";
            break;
        case TSK_ERR_MUTATION_TIME_OLDER_THAN_PARENT_MUTATION:
            ret = "A mutation's time must be <= the parent mutation time (if known), or "
                  "be marked as 'unknown'";
            break;
        case TSK_ERR_MUTATION_TIME_OLDER_THAN_PARENT_NODE:
            ret = "A mutation's time must be < the parent node of the edge on which it "
                  "occurs, or be marked as 'unknown'";
            break;
        case TSK_ERR_MUTATION_TIME_HAS_BOTH_KNOWN_AND_UNKNOWN:
            ret = "Mutation times must either be all marked 'unknown', or all be known "
                  "values for any single site.";
            break;

        /* Sample errors */
        case TSK_ERR_DUPLICATE_SAMPLE:
            ret = "Duplicate sample value";
            break;
        case TSK_ERR_BAD_SAMPLES:
            ret = "Bad sample configuration provided";
            break;

        /* Table errors */
        case TSK_ERR_BAD_TABLE_POSITION:
            ret = "Bad table position provided to truncate/reset";
            break;
        case TSK_ERR_BAD_SEQUENCE_LENGTH:
            ret = "Sequence length must be > 0";
            break;
        case TSK_ERR_TABLES_NOT_INDEXED:
            ret = "Table collection must be indexed";
            break;
        case TSK_ERR_TABLE_OVERFLOW:
            ret = "Table too large; cannot allocate more than 2**31 rows.";
            break;
        case TSK_ERR_COLUMN_OVERFLOW:
            ret = "Table column too large; cannot be more than 2**32 bytes.";
            break;
        case TSK_ERR_TREE_OVERFLOW:
            ret = "Too many trees; cannot be more than 2**31.";
            break;
        case TSK_ERR_METADATA_DISABLED:
            ret = "Metadata is disabled for this table, so cannot be set";
            break;

        /* Limitations */
        case TSK_ERR_ONLY_INFINITE_SITES:
            ret = "Only infinite sites mutations are supported for this operation";
            break;
        case TSK_ERR_SIMPLIFY_MIGRATIONS_NOT_SUPPORTED:
            ret = "Migrations not currently supported by simplify";
            break;
        case TSK_ERR_SORT_MIGRATIONS_NOT_SUPPORTED:
            ret = "Migrations not currently supported by sort";
            break;
        case TSK_ERR_MIGRATIONS_NOT_SUPPORTED:
            ret = "Migrations not currently supported by this operation";
            break;
        case TSK_ERR_SORT_OFFSET_NOT_SUPPORTED:
            ret = "Sort offsets for sites and mutations must be either 0 "
                  "or the length of the respective tables. Intermediate values "
                  "are not supported";
            break;
        case TSK_ERR_NONBINARY_MUTATIONS_UNSUPPORTED:
            ret = "Only binary mutations are supported for this operation";
            break;
        case TSK_ERR_UNION_NOT_SUPPORTED:
            ret = "Union is not supported for cases where there is non-shared"
                  "history older than the shared history of the two Table Collections";
            break;

        /* Stats errors */
        case TSK_ERR_BAD_NUM_WINDOWS:
            ret = "Must have at least one window, [0, L]";
            break;
        case TSK_ERR_BAD_WINDOWS:
            ret = "Windows must be increasing list [0, ..., L]";
            break;
        case TSK_ERR_MULTIPLE_STAT_MODES:
            ret = "Cannot specify more than one stats mode.";
            break;
        case TSK_ERR_BAD_STATE_DIMS:
            ret = "Must have state dimension >= 1";
            break;
        case TSK_ERR_BAD_RESULT_DIMS:
            ret = "Must have result dimension >= 1";
            break;
        case TSK_ERR_INSUFFICIENT_SAMPLE_SETS:
            ret = "Insufficient sample sets provided.";
            break;
        case TSK_ERR_INSUFFICIENT_INDEX_TUPLES:
            ret = "Insufficient sample set index tuples provided.";
            break;
        case TSK_ERR_BAD_SAMPLE_SET_INDEX:
            ret = "Sample set index out of bounds";
            break;
        case TSK_ERR_EMPTY_SAMPLE_SET:
            ret = "Samples cannot be empty";
            break;
        case TSK_ERR_UNSUPPORTED_STAT_MODE:
            ret = "Requested statistics mode not supported for this method.";
            break;

        /* Mutation mapping errors */
        case TSK_ERR_GENOTYPES_ALL_MISSING:
            ret = "Must provide at least one non-missing genotype.";
            break;
        case TSK_ERR_BAD_GENOTYPE:
            ret = "Bad genotype value provided";
            break;

        /* Genotype decoding errors */
        case TSK_ERR_TOO_MANY_ALLELES:
            ret = "Cannot have more than 127 alleles";
            break;
        case TSK_ERR_ZERO_ALLELES:
            ret = "Must have at least one allele when specifying an allele map";
            break;
        case TSK_ERR_MUST_IMPUTE_NON_SAMPLES:
            ret = "Cannot generate genotypes for non-samples when isolated nodes are "
                  "considered as missing";
            break;
        case TSK_ERR_ALLELE_NOT_FOUND:
            ret = "An allele was not found in the user-specified allele map";
            break;

        /* Distance metric errors */
        case TSK_ERR_SAMPLE_SIZE_MISMATCH:
            ret = "Cannot compare trees with different numbers of samples.";
            break;
        case TSK_ERR_SAMPLES_NOT_EQUAL:
            ret = "Samples must be identical in trees to compare.";
            break;
        case TSK_ERR_MULTIPLE_ROOTS:
            ret = "Trees with multiple roots not supported.";
            break;
        case TSK_ERR_UNARY_NODES:
            ret = "Unsimplified trees with unary nodes are not supported.";
            break;
        case TSK_ERR_SEQUENCE_LENGTH_MISMATCH:
            ret = "Sequence lengths must be identical to compare.";
            break;
        case TSK_ERR_NO_SAMPLE_LISTS:
            ret = "The sample_lists option must be enabled to perform this operation.";
            break;

        /* Haplotype matching errors */
        case TSK_ERR_NULL_VITERBI_MATRIX:
            ret = "Viterbi matrix has not filled.";
            break;
        case TSK_ERR_MATCH_IMPOSSIBLE:
            ret = "No matching haplotype exists with current parameters";
            break;
        case TSK_ERR_BAD_COMPRESSED_MATRIX_NODE:
            ret = "The compressed matrix contains a node that subtends no samples";
            break;
        case TSK_ERR_TOO_MANY_VALUES:
            ret = "Too many values to compress";
            break;

        /* Union errors */
        case TSK_ERR_UNION_BAD_MAP:
            ret = "Node map contains an entry of a node not present in this table "
                  "collection.";
            break;
        case TSK_ERR_UNION_DIFF_HISTORIES:
            // histories could be equivalent, because subset does not reorder
            // edges (if not sorted) or mutations.
            ret = "Shared portions of the tree sequences are not equal.";
    }
    return ret;
}

int
tsk_set_kas_error(int err)
{
    if (err == KAS_ERR_IO) {
        /* If we've detected an IO error, report it as TSK_ERR_IO so that we have
         * a consistent error code covering these situtations */
        return TSK_ERR_IO;
    } else {
        /* Flip this bit. As the error is negative, this sets the bit to 0 */
        return err ^ (1 << TSK_KAS_ERR_BIT);
    }
}

bool
tsk_is_kas_error(int err)
{
    return !(err & (1 << TSK_KAS_ERR_BIT));
}

const char *
tsk_strerror(int err)
{
    if (err != 0 && tsk_is_kas_error(err)) {
        err ^= (1 << TSK_KAS_ERR_BIT);
        return kas_strerror(err);
    } else {
        return tsk_strerror_internal(err);
    }
}

void
__tsk_safe_free(void **ptr)
{
    if (ptr != NULL) {
        if (*ptr != NULL) {
            free(*ptr);
            *ptr = NULL;
        }
    }
}

/* Block allocator. Simple allocator when we lots of chunks of memory
 * and don't need to free them individually.
 */

void
tsk_blkalloc_print_state(tsk_blkalloc_t *self, FILE *out)
{
    fprintf(out, "Block allocator%p::\n", (void *) self);
    fprintf(out, "\ttop = %d\n", (int) self->top);
    fprintf(out, "\tchunk_size = %d\n", (int) self->chunk_size);
    fprintf(out, "\tnum_chunks = %d\n", (int) self->num_chunks);
    fprintf(out, "\ttotal_allocated = %d\n", (int) self->total_allocated);
    fprintf(out, "\ttotal_size = %d\n", (int) self->total_size);
}

int TSK_WARN_UNUSED
tsk_blkalloc_reset(tsk_blkalloc_t *self)
{
    int ret = 0;

    self->top = 0;
    self->current_chunk = 0;
    self->total_allocated = 0;
    return ret;
}

int TSK_WARN_UNUSED
tsk_blkalloc_init(tsk_blkalloc_t *self, size_t chunk_size)
{
    int ret = 0;

    memset(self, 0, sizeof(tsk_blkalloc_t));
    if (chunk_size < 1) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    self->chunk_size = chunk_size;
    self->top = 0;
    self->current_chunk = 0;
    self->total_allocated = 0;
    self->total_size = 0;
    self->num_chunks = 0;
    self->mem_chunks = malloc(sizeof(char *));
    if (self->mem_chunks == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    self->mem_chunks[0] = malloc(chunk_size);
    if (self->mem_chunks[0] == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    self->num_chunks = 1;
    self->total_size = chunk_size + sizeof(void *);
out:
    return ret;
}

void *TSK_WARN_UNUSED
tsk_blkalloc_get(tsk_blkalloc_t *self, size_t size)
{
    void *ret = NULL;
    void *p;

    if (size > self->chunk_size) {
        goto out;
    }
    if ((self->top + size) > self->chunk_size) {
        if (self->current_chunk == (self->num_chunks - 1)) {
            p = realloc(self->mem_chunks, (self->num_chunks + 1) * sizeof(void *));
            if (p == NULL) {
                goto out;
            }
            self->mem_chunks = p;
            p = malloc(self->chunk_size);
            if (p == NULL) {
                goto out;
            }
            self->mem_chunks[self->num_chunks] = p;
            self->num_chunks++;
            self->total_size += self->chunk_size + sizeof(void *);
        }
        self->current_chunk++;
        self->top = 0;
    }
    ret = self->mem_chunks[self->current_chunk] + self->top;
    self->top += size;
    self->total_allocated += size;
out:
    return ret;
}

void
tsk_blkalloc_free(tsk_blkalloc_t *self)
{
    size_t j;

    for (j = 0; j < self->num_chunks; j++) {
        if (self->mem_chunks[j] != NULL) {
            free(self->mem_chunks[j]);
        }
    }
    if (self->mem_chunks != NULL) {
        free(self->mem_chunks);
    }
}

/* Mirrors the semantics of numpy's searchsorted function. Uses binary
 * search to find the index of the closest value in the array. */
size_t
tsk_search_sorted(const double *restrict array, size_t size, double value)
{
    int64_t upper = (int64_t) size;
    int64_t lower = 0;
    int64_t offset = 0;
    int64_t mid;

    if (upper == 0) {
        return 0;
    }

    while (upper - lower > 1) {
        mid = (upper + lower) / 2;
        if (value >= array[mid]) {
            lower = mid;
        } else {
            upper = mid;
        }
    }
    offset = (int64_t)(array[lower] < value);
    return (size_t)(lower + offset);
}

/* Rounds the specified double to the closest multiple of 10**-num_digits. If
 * num_digits > 22, return value without changes. This is intended for use with
 * small positive numbers; behaviour with large inputs has not been considered.
 *
 * Based on double_round from the Python standard library
 * https://github.com/python/cpython/blob/master/Objects/floatobject.c#L985
 */
double
tsk_round(double x, unsigned int ndigits)
{
    double pow1, y, z;

    z = x;
    if (ndigits < 22) {
        pow1 = pow(10.0, (double) ndigits);
        y = x * pow1;
        z = round(y);
        if (fabs(y - z) == 0.5) {
            /* halfway between two integers; use round-half-even */
            z = 2.0 * round(y / 2.0);
        }
        z = z / pow1;
    }
    return z;
}

/* As NANs are not equal, use this function to check for equality to TSK_UNKNOWN_TIME */
bool
tsk_is_unknown_time(double val)
{
    union {
        uint64_t i;
        double f;
    } nan_union;
    nan_union.f = val;
    return nan_union.i == TSK_UNKNOWN_TIME_HEX;
}
