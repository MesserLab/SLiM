/*
 * MIT License
 *
 * Copyright (c) 2019-2025 Tskit Developers
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
    int ret = 0;
    HCRYPTPROV hCryptProv = (HCRYPTPROV) NULL;

    if (!CryptAcquireContext(
            &hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        ret = tsk_trace_error(TSK_ERR_GENERATE_UUID);
        goto out;
    }
    if (!CryptGenRandom(hCryptProv, (DWORD) UUID_NUM_BYTES, buf)) {
        ret = tsk_trace_error(TSK_ERR_GENERATE_UUID);
        goto out;
    }
    if (!CryptReleaseContext(hCryptProv, 0)) {
        hCryptProv = (HCRYPTPROV) NULL;
        ret = tsk_trace_error(TSK_ERR_GENERATE_UUID);
        goto out;
    }
    hCryptProv = (HCRYPTPROV) NULL;
out:
    if (hCryptProv != (HCRYPTPROV) NULL) {
        CryptReleaseContext(hCryptProv, 0);
    }
    return ret;
}

#else

/* Assuming the existance of /dev/urandom on Unix platforms */
static int TSK_WARN_UNUSED
get_random_bytes(uint8_t *buf)
{
    int ret = 0;
    FILE *f = fopen("/dev/urandom", "r");

    if (f == NULL) {
        ret = tsk_trace_error(TSK_ERR_GENERATE_UUID);
        goto out;
    }
    if (fread(buf, UUID_NUM_BYTES, 1, f) != 1) {
        ret = tsk_trace_error(TSK_ERR_GENERATE_UUID);
        goto out;
    }
    if (fclose(f) != 0) {
        ret = tsk_trace_error(TSK_ERR_GENERATE_UUID);
        goto out;
    }
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
        ret = tsk_trace_error(TSK_ERR_GENERATE_UUID);
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
            ret = "Generic error; please file a bug report. (TSK_ERR_GENERIC)";
            break;
        case TSK_ERR_NO_MEMORY:
            ret = "Out of memory. (TSK_ERR_NO_MEMORY)";
            break;
        case TSK_ERR_IO:
            if (errno != 0) {
                ret = strerror(errno);
            } else {
                ret = "Unspecified IO error";
            }
            break;
        case TSK_ERR_BAD_PARAM_VALUE:
            ret = "Bad parameter value provided. (TSK_ERR_BAD_PARAM_VALUE)";
            break;
        case TSK_ERR_BUFFER_OVERFLOW:
            ret = "Supplied buffer is too small. (TSK_ERR_BUFFER_OVERFLOW)";
            break;
        case TSK_ERR_UNSUPPORTED_OPERATION:
            ret = "Operation cannot be performed in current configuration. "
                  "(TSK_ERR_UNSUPPORTED_OPERATION)";
            break;
        case TSK_ERR_GENERATE_UUID:
            ret = "Error generating UUID. (TSK_ERR_GENERATE_UUID)";
            break;
        case TSK_ERR_EOF:
            ret = "End of file. (TSK_ERR_EOF)";
            break;

        /* File format errors */
        case TSK_ERR_FILE_FORMAT:
            ret = "File format error. (TSK_ERR_FILE_FORMAT)";
            break;
        case TSK_ERR_FILE_VERSION_TOO_OLD:
            ret = "tskit file version too old. Please upgrade using the "
                  "'tskit upgrade' command from tskit version<0.6.2. "
                  "(TSK_ERR_FILE_VERSION_TOO_OLD)";
            break;
        case TSK_ERR_FILE_VERSION_TOO_NEW:
            ret = "tskit file version is too new for this instance. "
                  "Please upgrade tskit to the latest version. "
                  "(TSK_ERR_FILE_VERSION_TOO_NEW)";
            break;
        case TSK_ERR_REQUIRED_COL_NOT_FOUND:
            ret = "A required column was not found in the file. "
                  "(TSK_ERR_REQUIRED_COL_NOT_FOUND)";
            break;
        case TSK_ERR_BOTH_COLUMNS_REQUIRED:
            ret = "Both columns in a related pair must be provided. "
                  "(TSK_ERR_BOTH_COLUMNS_REQUIRED)";
            break;
        case TSK_ERR_BAD_COLUMN_TYPE:
            ret = "An incompatible type for a column was found in the file. "
                  "(TSK_ERR_BAD_COLUMN_TYPE)";
            break;

        /* Out of bounds errors */
        case TSK_ERR_BAD_OFFSET:
            ret = "Bad offset provided in input array. (TSK_ERR_BAD_OFFSET)";
            break;
        case TSK_ERR_NODE_OUT_OF_BOUNDS:
            ret = "Node out of bounds. (TSK_ERR_NODE_OUT_OF_BOUNDS)";
            break;
        case TSK_ERR_EDGE_OUT_OF_BOUNDS:
            ret = "Edge out of bounds. (TSK_ERR_EDGE_OUT_OF_BOUNDS)";
            break;
        case TSK_ERR_POPULATION_OUT_OF_BOUNDS:
            ret = "Population out of bounds. (TSK_ERR_POPULATION_OUT_OF_BOUNDS)";
            break;
        case TSK_ERR_SITE_OUT_OF_BOUNDS:
            ret = "Site out of bounds. (TSK_ERR_SITE_OUT_OF_BOUNDS)";
            break;
        case TSK_ERR_MUTATION_OUT_OF_BOUNDS:
            ret = "Mutation out of bounds. (TSK_ERR_MUTATION_OUT_OF_BOUNDS)";
            break;
        case TSK_ERR_MIGRATION_OUT_OF_BOUNDS:
            ret = "Migration out of bounds. (TSK_ERR_MIGRATION_OUT_OF_BOUNDS)";
            break;
        case TSK_ERR_INDIVIDUAL_OUT_OF_BOUNDS:
            ret = "Individual out of bounds. (TSK_ERR_INDIVIDUAL_OUT_OF_BOUNDS)";
            break;
        case TSK_ERR_PROVENANCE_OUT_OF_BOUNDS:
            ret = "Provenance out of bounds. (TSK_ERR_PROVENANCE_OUT_OF_BOUNDS)";
            break;
        case TSK_ERR_TIME_NONFINITE:
            ret = "Times must be finite. (TSK_ERR_TIME_NONFINITE)";
            break;
        case TSK_ERR_GENOME_COORDS_NONFINITE:
            ret = "Genome coordinates must be finite numbers. "
                  "(TSK_ERR_GENOME_COORDS_NONFINITE)";
            break;
        case TSK_ERR_SEEK_OUT_OF_BOUNDS:
            ret = "Tree seek position out of bounds. (TSK_ERR_SEEK_OUT_OF_BOUNDS)";
            break;
        case TSK_ERR_KEEP_ROWS_MAP_TO_DELETED:
            ret = "One of the kept rows in the table refers to a deleted row. "
                  "(TSK_ERR_KEEP_ROWS_MAP_TO_DELETED)";
            break;
        case TSK_ERR_POSITION_OUT_OF_BOUNDS:
            ret = "Position out of bounds. (TSK_ERR_POSITION_OUT_OF_BOUNDS)";
            break;

        /* Edge errors */
        case TSK_ERR_NULL_PARENT:
            ret = "Edge parent is null. (TSK_ERR_NULL_PARENT)";
            break;
        case TSK_ERR_NULL_CHILD:
            ret = "Edge child is null. (TSK_ERR_NULL_CHILD)";
            break;
        case TSK_ERR_EDGES_NOT_SORTED_PARENT_TIME:
            ret = "Edges must be listed in (time[parent], child, left) order;"
                  " time[parent] order violated. (TSK_ERR_EDGES_NOT_SORTED_PARENT_TIME)";
            break;
        case TSK_ERR_EDGES_NONCONTIGUOUS_PARENTS:
            ret = "All edges for a given parent must be contiguous. "
                  "(TSK_ERR_EDGES_NONCONTIGUOUS_PARENTS)";
            break;
        case TSK_ERR_EDGES_NOT_SORTED_CHILD:
            ret = "Edges must be listed in (time[parent], child, left) order;"
                  " child order violated. (TSK_ERR_EDGES_NOT_SORTED_CHILD)";
            break;
        case TSK_ERR_EDGES_NOT_SORTED_LEFT:
            ret = "Edges must be listed in (time[parent], child, left) order;"
                  " left order violated. (TSK_ERR_EDGES_NOT_SORTED_LEFT)";
            break;
        case TSK_ERR_BAD_NODE_TIME_ORDERING:
            ret = "time[parent] must be greater than time[child]. "
                  "(TSK_ERR_BAD_NODE_TIME_ORDERING)";
            break;
        case TSK_ERR_BAD_EDGE_INTERVAL:
            ret = "Bad edge interval where right <= left. (TSK_ERR_BAD_EDGE_INTERVAL)";
            break;
        case TSK_ERR_DUPLICATE_EDGES:
            ret = "Duplicate edges provided. (TSK_ERR_DUPLICATE_EDGES)";
            break;
        case TSK_ERR_RIGHT_GREATER_SEQ_LENGTH:
            ret = "Right coordinate > sequence length. "
                  "(TSK_ERR_RIGHT_GREATER_SEQ_LENGTH)";
            break;
        case TSK_ERR_LEFT_LESS_ZERO:
            ret = "Left coordinate must be >= 0. (TSK_ERR_LEFT_LESS_ZERO)";
            break;
        case TSK_ERR_BAD_EDGES_CONTRADICTORY_CHILDREN:
            ret = "Bad edges: contradictory children for a given parent over "
                  "an interval, or indexes need to be rebuilt. "
                  "(TSK_ERR_BAD_EDGES_CONTRADICTORY_CHILDREN)";
            break;
        case TSK_ERR_CANT_PROCESS_EDGES_WITH_METADATA:
            ret = "Can't squash, flush, simplify or link ancestors with edges that have "
                  "non-empty metadata. Removing the metadata from the edges will allow "
                  "these operations to proceed. For example using "
                  "tables.edges.drop_metadata() in the tskit Python API. "
                  "(TSK_ERR_CANT_PROCESS_EDGES_WITH_METADATA)";
            break;

        /* Site errors */
        case TSK_ERR_UNSORTED_SITES:
            ret = "Sites must be provided in strictly increasing position order. "
                  "(TSK_ERR_UNSORTED_SITES)";
            break;
        case TSK_ERR_DUPLICATE_SITE_POSITION:
            ret = "Duplicate site positions. (TSK_ERR_DUPLICATE_SITE_POSITION)";
            break;
        case TSK_ERR_BAD_SITE_POSITION:
            ret = "Site positions must be between 0 and sequence_length. "
                  "(TSK_ERR_BAD_SITE_POSITION)";
            break;

        /* Mutation errors */
        case TSK_ERR_MUTATION_PARENT_DIFFERENT_SITE:
            ret = "Specified parent mutation is at a different site. "
                  "(TSK_ERR_MUTATION_PARENT_DIFFERENT_SITE)";
            break;
        case TSK_ERR_MUTATION_PARENT_EQUAL:
            ret = "Parent mutation refers to itself. (TSK_ERR_MUTATION_PARENT_EQUAL)";
            break;
        case TSK_ERR_MUTATION_PARENT_AFTER_CHILD:
            ret = "Parent mutation ID must be < current ID. "
                  "(TSK_ERR_MUTATION_PARENT_AFTER_CHILD)";
            break;
        case TSK_ERR_MUTATION_PARENT_INCONSISTENT:
            ret = "Mutation parent references form a loop. "
                  "(TSK_ERR_MUTATION_PARENT_INCONSISTENT)";
            break;
        case TSK_ERR_UNSORTED_MUTATIONS:
            ret = "Mutations must be provided in non-decreasing site order and "
                  "non-increasing time order within each site. "
                  "(TSK_ERR_UNSORTED_MUTATIONS)";
            break;
        case TSK_ERR_MUTATION_TIME_YOUNGER_THAN_NODE:
            ret = "A mutation's time must be >= the node time, or be marked as "
                  "'unknown'. (TSK_ERR_MUTATION_TIME_YOUNGER_THAN_NODE)";
            break;
        case TSK_ERR_MUTATION_TIME_OLDER_THAN_PARENT_MUTATION:
            ret = "A mutation's time must be <= the parent mutation time (if known), or "
                  "be marked as 'unknown'. "
                  "(TSK_ERR_MUTATION_TIME_OLDER_THAN_PARENT_MUTATION)";
            break;
        case TSK_ERR_MUTATION_TIME_OLDER_THAN_PARENT_NODE:
            ret = "A mutation's time must be < the parent node of the edge on which it "
                  "occurs, or be marked as 'unknown'. "
                  "(TSK_ERR_MUTATION_TIME_OLDER_THAN_PARENT_NODE)";
            break;
        case TSK_ERR_MUTATION_TIME_HAS_BOTH_KNOWN_AND_UNKNOWN:
            ret = "Mutation times must either be all marked 'unknown', or all be known "
                  "values for any single site. "
                  "(TSK_ERR_MUTATION_TIME_HAS_BOTH_KNOWN_AND_UNKNOWN)";
            break;
        case TSK_ERR_DISALLOWED_UNKNOWN_MUTATION_TIME:
            ret = "Some mutation times are marked 'unknown' for a method that requires "
                  "no unknown times. (Use compute_mutation_times to add times?) "
                  "(TSK_ERR_DISALLOWED_UNKNOWN_MUTATION_TIME)";
            break;

        case TSK_ERR_BAD_MUTATION_PARENT:
            ret = "A mutation's parent is not consistent with the topology of the tree. "
                  "Use compute_mutation_parents to set the parents correctly."
                  "(TSK_ERR_BAD_MUTATION_PARENT)";
            break;

        /* Migration errors */
        case TSK_ERR_UNSORTED_MIGRATIONS:
            ret = "Migrations must be sorted by time. (TSK_ERR_UNSORTED_MIGRATIONS)";
            break;

        /* Sample errors */
        case TSK_ERR_DUPLICATE_SAMPLE:
            ret = "Duplicate sample value. (TSK_ERR_DUPLICATE_SAMPLE)";
            break;
        case TSK_ERR_BAD_SAMPLES:
            ret = "The nodes provided are not samples. (TSK_ERR_BAD_SAMPLES)";
            break;

        /* Table errors */
        case TSK_ERR_BAD_TABLE_POSITION:
            ret = "Bad table position provided to truncate/reset. "
                  "(TSK_ERR_BAD_TABLE_POSITION)";
            break;
        case TSK_ERR_BAD_SEQUENCE_LENGTH:
            ret = "Sequence length must be > 0. (TSK_ERR_BAD_SEQUENCE_LENGTH)";
            break;
        case TSK_ERR_TABLES_NOT_INDEXED:
            ret = "Table collection must be indexed. (TSK_ERR_TABLES_NOT_INDEXED)";
            break;
        case TSK_ERR_TABLES_BAD_INDEXES:
            ret = "Table collection indexes inconsistent: do they need to be rebuilt? "
                  "(TSK_ERR_TABLES_BAD_INDEXES)";
            break;
        case TSK_ERR_TABLE_OVERFLOW:
            ret = "Table too large; cannot allocate more than 2**31 rows. This error "
                  "is often caused by a lack of simplification when simulating. "
                  "(TSK_ERR_TABLE_OVERFLOW)";
            break;
        case TSK_ERR_COLUMN_OVERFLOW:
            ret = "Table column too large; cannot be more than 2**64 bytes. "
                  "(TSK_ERR_COLUMN_OVERFLOW)";
            break;
        case TSK_ERR_TREE_OVERFLOW:
            ret = "Too many trees; cannot be more than 2**31. (TSK_ERR_TREE_OVERFLOW)";
            break;
        case TSK_ERR_METADATA_DISABLED:
            ret = "Metadata is disabled for this table, so cannot be set. "
                  "(TSK_ERR_METADATA_DISABLED)";
            break;

        /* Limitations */
        case TSK_ERR_ONLY_INFINITE_SITES:
            ret = "Only infinite sites mutations are supported for this operation, "
                  "i.e. at most a single mutation per site. "
                  "(TSK_ERR_ONLY_INFINITE_SITES)";
            break;
        case TSK_ERR_SIMPLIFY_MIGRATIONS_NOT_SUPPORTED:
            ret = "Migrations not currently supported by simplify. "
                  "(TSK_ERR_SIMPLIFY_MIGRATIONS_NOT_SUPPORTED)";
            break;
        case TSK_ERR_SORT_MIGRATIONS_NOT_SUPPORTED:
            ret = "Migrations not currently supported by sort. "
                  "(TSK_ERR_SORT_MIGRATIONS_NOT_SUPPORTED)";
            break;
        case TSK_ERR_SORT_OFFSET_NOT_SUPPORTED:
            ret = "Sort offsets for sites and mutations must be either 0 "
                  "or the length of the respective tables. Intermediate values "
                  "are not supported. (TSK_ERR_SORT_OFFSET_NOT_SUPPORTED)";
            break;
        case TSK_ERR_NONBINARY_MUTATIONS_UNSUPPORTED:
            ret = "Only binary mutations are supported for this operation. "
                  "(TSK_ERR_NONBINARY_MUTATIONS_UNSUPPORTED)";
            break;
        case TSK_ERR_MIGRATIONS_NOT_SUPPORTED:
            ret = "Migrations not currently supported by this operation. "
                  "(TSK_ERR_MIGRATIONS_NOT_SUPPORTED)";
            break;
        case TSK_ERR_CANNOT_EXTEND_FROM_SELF:
            ret = "Tables can only be extended using rows from a different table. "
                  "(TSK_ERR_CANNOT_EXTEND_FROM_SELF)";
            break;
        case TSK_ERR_SILENT_MUTATIONS_NOT_SUPPORTED:
            ret = "Silent mutations not supported by this operation. "
                  "(TSK_ERR_SILENT_MUTATIONS_NOT_SUPPORTED)";
            break;
        case TSK_ERR_VARIANT_CANT_DECODE_COPY:
            ret = "Can't decode a copy of a variant. (TSK_ERR_VARIANT_CANT_DECODE_COPY)";
            break;
        case TSK_ERR_CANT_TAKE_OWNERSHIP_NO_EDGE_METADATA:
            ret = "A tree sequence can't take ownership of tables with "
                  "TSK_NO_EDGE_METADATA. (TSK_ERR_CANT_TAKE_OWNERSHIP_NO_EDGE_METADATA)";
            break;
        case TSK_ERR_UNDEFINED_NONBINARY:
            ret = "Operation undefined for nonbinary trees. "
                  "(TSK_ERR_UNDEFINED_NONBINARY)";
            break;
        case TSK_ERR_UNDEFINED_MULTIROOT:
            ret = "Operation undefined for trees that are not singly-rooted. "
                  "(TSK_ERR_UNDEFINED_MULTIROOT)";
            break;

        /* Stats errors */
        case TSK_ERR_BAD_NUM_WINDOWS:
            ret = "Must have at least one window, [0, L]. (TSK_ERR_BAD_NUM_WINDOWS)";
            break;
        case TSK_ERR_BAD_WINDOWS:
            ret = "Windows must be increasing list [0, ..., L]. (TSK_ERR_BAD_WINDOWS)";
            break;
        case TSK_ERR_MULTIPLE_STAT_MODES:
            ret = "Cannot specify more than one stats mode. "
                  "(TSK_ERR_MULTIPLE_STAT_MODES)";
            break;
        case TSK_ERR_BAD_STATE_DIMS:
            ret = "Must have state dimension >= 1. (TSK_ERR_BAD_STATE_DIMS)";
            break;
        case TSK_ERR_BAD_RESULT_DIMS:
            ret = "Must have result dimension >= 1. (TSK_ERR_BAD_RESULT_DIMS)";
            break;
        case TSK_ERR_INSUFFICIENT_SAMPLE_SETS:
            ret = "Insufficient sample sets provided. "
                  "(TSK_ERR_INSUFFICIENT_SAMPLE_SETS)";
            break;
        case TSK_ERR_INSUFFICIENT_INDEX_TUPLES:
            ret = "Insufficient sample set index tuples provided. "
                  "(TSK_ERR_INSUFFICIENT_INDEX_TUPLES)";
            break;
        case TSK_ERR_BAD_SAMPLE_SET_INDEX:
            ret = "Sample set index out of bounds. (TSK_ERR_BAD_SAMPLE_SET_INDEX)";
            break;
        case TSK_ERR_EMPTY_SAMPLE_SET:
            ret = "Samples cannot be empty. (TSK_ERR_EMPTY_SAMPLE_SET)";
            break;
        case TSK_ERR_UNSUPPORTED_STAT_MODE:
            ret = "Requested statistics mode not supported for this method. "
                  "(TSK_ERR_UNSUPPORTED_STAT_MODE)";
            break;
        case TSK_ERR_TIME_UNCALIBRATED:
            ret = "Statistics using branch lengths cannot be calculated when time_units "
                  "is 'uncalibrated'. (TSK_ERR_TIME_UNCALIBRATED)";
            break;
        case TSK_ERR_STAT_POLARISED_UNSUPPORTED:
            ret = "The TSK_STAT_POLARISED option is not supported by this statistic. "
                  "(TSK_ERR_STAT_POLARISED_UNSUPPORTED)";
            break;
        case TSK_ERR_STAT_SPAN_NORMALISE_UNSUPPORTED:
            ret = "The TSK_STAT_SPAN_NORMALISE option is not supported by this "
                  "statistic. "
                  "(TSK_ERR_STAT_SPAN_NORMALISE_UNSUPPORTED)";
            break;
        case TSK_ERR_INSUFFICIENT_WEIGHTS:
            ret = "Insufficient weights provided (at least 1 required). "
                  "(TSK_ERR_INSUFFICIENT_WEIGHTS)";
            break;

        /* Pair coalescence errors */
        case TSK_ERR_BAD_NODE_BIN_MAP:
            ret = "Node-to-bin map contains values less than TSK_NULL. "
                  "(TSK_ERR_BAD_NODE_BIN_MAP)";
            break;
        case TSK_ERR_BAD_NODE_BIN_MAP_DIM:
            ret = "Maximum index in node-to-bin map is greater than the "
                  "output dimension. (TSK_ERR_BAD_NODE_BIN_MAP_DIM)";
            break;
        case TSK_ERR_BAD_QUANTILES:
            ret = "Quantiles must be between 0 and 1 (inclusive) "
                  "and strictly increasing. (TSK_ERR_BAD_QUANTILES)";
            break;
        case TSK_ERR_UNSORTED_TIMES:
            ret = "Times must be strictly increasing. (TSK_ERR_UNSORTED_TIMES)";
            break;
        case TSK_ERR_BAD_TIME_WINDOWS_DIM:
            ret = "Must have at least one time window. (TSK_ERR_BAD_TIME_WINDOWS_DIM)";
            break;
        case TSK_ERR_BAD_SAMPLE_PAIR_TIMES:
            ret = "All sample times must be equal to the start of first time window. "
                  "(TSK_ERR_BAD_SAMPLE_PAIR_TIMES)";
            break;
        case TSK_ERR_BAD_TIME_WINDOWS:
            ret = "Time windows must start at zero and be strictly increasing. "
                  "(TSK_ERR_BAD_TIME_WINDOWS)";
            break;
        case TSK_ERR_BAD_TIME_WINDOWS_END:
            ret = "Time windows must end at infinity for this method. "
                  "(TSK_ERR_BAD_TIME_WINDOWS_END)";
            break;
        case TSK_ERR_BAD_NODE_TIME_WINDOW:
            ret = "Node time does not fall within assigned time window. "
                  "(TSK_ERR_BAD_NODE_TIME_WINDOW)";
            break;

        /* Two locus errors */
        case TSK_ERR_STAT_UNSORTED_POSITIONS:
            ret = "The provided positions are not sorted in strictly increasing "
                  "order. (TSK_ERR_STAT_UNSORTED_POSITIONS)";
            break;
        case TSK_ERR_STAT_DUPLICATE_POSITIONS:
            ret = "The provided positions contain duplicates. "
                  "(TSK_ERR_STAT_DUPLICATE_POSITIONS)";
            break;
        case TSK_ERR_STAT_UNSORTED_SITES:
            ret = "The provided sites are not sorted in strictly increasing position "
                  "order. (TSK_ERR_STAT_UNSORTED_SITES)";
            break;
        case TSK_ERR_STAT_DUPLICATE_SITES:
            ret = "The provided sites contain duplicated entries. "
                  "(TSK_ERR_STAT_DUPLICATE_SITES)";
            break;

        /* Mutation mapping errors */
        case TSK_ERR_GENOTYPES_ALL_MISSING:
            ret = "Must provide at least one non-missing genotype. "
                  "(TSK_ERR_GENOTYPES_ALL_MISSING)";
            break;
        case TSK_ERR_BAD_GENOTYPE:
            ret = "Bad genotype value provided. (TSK_ERR_BAD_GENOTYPE)";
            break;
        case TSK_ERR_BAD_ANCESTRAL_STATE:
            ret = "Bad ancestral state specified. (TSK_ERR_BAD_ANCESTRAL_STATE)";
            break;

        /* Genotype decoding errors */
        case TSK_ERR_MUST_IMPUTE_NON_SAMPLES:
            ret = "Cannot generate genotypes for non-samples when isolated nodes are "
                  "considered as missing. (TSK_ERR_MUST_IMPUTE_NON_SAMPLES)";
            break;
        case TSK_ERR_ALLELE_NOT_FOUND:
            ret = "An allele was not found in the user-specified allele map. "
                  "(TSK_ERR_ALLELE_NOT_FOUND)";
            break;
        case TSK_ERR_TOO_MANY_ALLELES:
            ret = "Cannot have more than 2147483647 alleles (TSK_ERR_TOO_MANY_ALLELES)";
            break;
        case TSK_ERR_ZERO_ALLELES:
            ret = "Must have at least one allele when specifying an allele map. "
                  "(TSK_ERR_ZERO_ALLELES)";
            break;
        case TSK_ERR_BAD_ALLELE_LENGTH:
            ret = "Alleles used when decoding alignments must have length one. "
                  "(TSK_ERR_BAD_ALLELE_LENGTH)";
            break;
        case TSK_ERR_MISSING_CHAR_COLLISION:
            ret = "Alleles used when decoding alignments must not match the missing "
                  "data character. (TSK_ERR_MISSING_CHAR_COLLISION)";
            break;

        /* Distance metric errors */
        case TSK_ERR_SAMPLE_SIZE_MISMATCH:
            ret = "Cannot compare trees with different numbers of samples. "
                  "(TSK_ERR_SAMPLE_SIZE_MISMATCH)";
            break;
        case TSK_ERR_SAMPLES_NOT_EQUAL:
            ret = "Samples must be identical in trees to compare. "
                  "(TSK_ERR_SAMPLES_NOT_EQUAL)";
            break;
        case TSK_ERR_MULTIPLE_ROOTS:
            ret = "Trees with multiple roots not supported. (TSK_ERR_MULTIPLE_ROOTS)";
            break;
        case TSK_ERR_UNARY_NODES:
            ret = "Unsimplified trees with unary nodes are not supported. "
                  "(TSK_ERR_UNARY_NODES)";
            break;
        case TSK_ERR_SEQUENCE_LENGTH_MISMATCH:
            ret = "Sequence lengths must be identical to compare. "
                  "(TSK_ERR_SEQUENCE_LENGTH_MISMATCH)";
            break;
        case TSK_ERR_NO_SAMPLE_LISTS:
            ret = "The sample_lists option must be enabled on the tree to perform this "
                  "operation. Pass the option to the constructor or method that created "
                  "the tree. (TSK_ERR_NO_SAMPLE_LISTS)";
            break;

        /* Haplotype matching errors */
        case TSK_ERR_NULL_VITERBI_MATRIX:
            ret = "Viterbi matrix has not filled. (TSK_ERR_NULL_VITERBI_MATRIX)";
            break;
        case TSK_ERR_MATCH_IMPOSSIBLE:
            ret = "No matching haplotype exists with current parameters. "
                  "(TSK_ERR_MATCH_IMPOSSIBLE)";
            break;
        case TSK_ERR_BAD_COMPRESSED_MATRIX_NODE:
            ret = "The compressed matrix contains a node that subtends no samples. "
                  "(TSK_ERR_BAD_COMPRESSED_MATRIX_NODE)";
            break;
        case TSK_ERR_TOO_MANY_VALUES:
            ret = "Too many values to compress. (TSK_ERR_TOO_MANY_VALUES)";
            break;

        /* Union errors */
        case TSK_ERR_UNION_BAD_MAP:
            ret = "Node map contains an entry of a node not present in this table "
                  "collection. (TSK_ERR_UNION_BAD_MAP)";
            break;
        case TSK_ERR_UNION_DIFF_HISTORIES:
            // histories could be equivalent, because subset does not reorder
            // edges (if not sorted) or mutations.
            ret = "Shared portions of the tree sequences are not equal. "
                  "(TSK_ERR_UNION_DIFF_HISTORIES)";
            break;

        /* IBD errors */
        case TSK_ERR_SAME_NODES_IN_PAIR:
            ret = "Both nodes in the sample pair are the same. "
                  "(TSK_ERR_SAME_NODES_IN_PAIR)";
            break;

        case TSK_ERR_IBD_PAIRS_NOT_STORED:
            ret = "The sample pairs are not stored by default in ibd_segments. Please "
                  "add the TSK_IBD_STORE_PAIRS option flag if per-pair statistics are "
                  "required. (TSK_ERR_IBD_PAIRS_NOT_STORED)";
            break;

        case TSK_ERR_IBD_SEGMENTS_NOT_STORED:
            ret = "All segments are not stored by default in ibd_segments. Please "
                  "add the TSK_IBD_STORE_SEGMENTS option flag if they are required. "
                  "(TSK_ERR_IBD_SEGMENTS_NOT_STORED)";
            break;

        /* Simplify errors */
        case TSK_ERR_KEEP_UNARY_MUTUALLY_EXCLUSIVE:
            ret = "You cannot specify both TSK_SIMPLIFY_KEEP_UNARY and "
                  "TSK_SIMPLIFY_KEEP_UNARY_IN_INDIVDUALS. "
                  "(TSK_ERR_KEEP_UNARY_MUTUALLY_EXCLUSIVE)";
            break;

        /* Individual errors */
        case TSK_ERR_UNSORTED_INDIVIDUALS:
            ret = "Individuals must be provided in an order where children are after "
                  "their parent individuals (TSK_ERR_UNSORTED_INDIVIDUALS)";
            break;

        case TSK_ERR_INDIVIDUAL_SELF_PARENT:
            ret = "Individuals cannot be their own parents. "
                  "(TSK_ERR_INDIVIDUAL_SELF_PARENT)";
            break;

        case TSK_ERR_INDIVIDUAL_PARENT_CYCLE:
            ret = "Individuals cannot be their own ancestor. "
                  "(TSK_ERR_INDIVIDUAL_PARENT_CYCLE)";
            break;

        case TSK_ERR_INDIVIDUAL_POPULATION_MISMATCH:
            ret = "Individual populations cannot be returned "
                  "if an individual has nodes from more than one population. "
                  "(TSK_ERR_INDIVIDUAL_POPULATION_MISMATCH)";
            break;

        case TSK_ERR_INDIVIDUAL_TIME_MISMATCH:
            ret = "Individual times cannot be returned "
                  "if an individual has nodes from more than one time. "
                  "(TSK_ERR_INDIVIDUAL_TIME_MISMATCH)";
            break;

        case TSK_ERR_EXTEND_EDGES_BAD_MAXITER:
            ret = "Maximum number of iterations must be positive. "
                  "(TSK_ERR_EXTEND_EDGES_BAD_MAXITER)";
            break;
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

int
tsk_get_kas_error(int err)
{
    return err ^ (1 << TSK_KAS_ERR_BIT);
}

const char *
tsk_strerror(int err)
{
    if (err != 0 && tsk_is_kas_error(err)) {
        return kas_strerror(tsk_get_kas_error(err));
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
    fprintf(out, "\ttop = %lld\n", (long long) self->top);
    fprintf(out, "\tchunk_size = %lld\n", (long long) self->chunk_size);
    fprintf(out, "\tnum_chunks = %lld\n", (long long) self->num_chunks);
    fprintf(out, "\ttotal_allocated = %lld\n", (long long) self->total_allocated);
    fprintf(out, "\ttotal_size = %lld\n", (long long) self->total_size);
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

    tsk_memset(self, 0, sizeof(tsk_blkalloc_t));
    if (chunk_size < 1) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
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
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    self->mem_chunks[0] = malloc(chunk_size);
    if (self->mem_chunks[0] == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
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
tsk_size_t
tsk_search_sorted(const double *restrict array, tsk_size_t size, double value)
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
    return (tsk_size_t)(lower + offset);
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

/* Work around a bug which seems to show up on various mixtures of
 * compiler and libc versions, where isfinite and isnan result in
 * spurious warnings about casting down to float. The original issue
 * is here:
 * https://github.com/tskit-dev/tskit/issues/721
 *
 * The simplest approach seems to be to use the builtins where they
 * are available (clang and gcc), and to use the library macro
 * otherwise. There would be no disadvantage to using the builtin
 * version, so there's no real harm in this approach.
 */

bool
tsk_isnan(double val)
{
#if defined(__GNUC__)
    return __builtin_isnan(val);
#else
    return isnan(val);
#endif
}

bool
tsk_isfinite(double val)
{
#if defined(__GNUC__)
    return __builtin_isfinite(val);
#else
    return isfinite(val);
#endif
}

void *
tsk_malloc(tsk_size_t size)
{
    /* Avoid malloc(0) as it's not portable */
    if (size == 0) {
        size = 1;
    }
#if TSK_MAX_SIZE > SIZE_MAX
    if (size > SIZE_MAX) {
        return NULL;
    }
#endif
    return malloc((size_t) size);
}

void *
tsk_realloc(void *ptr, tsk_size_t size)
{
    /* We shouldn't ever realloc to a zero size in tskit */
    tsk_bug_assert(size > 0);
    return realloc(ptr, (size_t) size);
}

/* We keep the size argument here as a size_t because we'd have to
 * cast the outputs of sizeof() otherwise, which would lead to
 * less readable code. We need to be careful to use calloc within
 * the library accordingly, so that size can't overflow on 32 bit.
 */
void *
tsk_calloc(tsk_size_t n, size_t size)
{
    /* Avoid calloc(0) as it's not portable */
    if (n == 0) {
        n = 1;
    }
#if TSK_MAX_SIZE > SIZE_MAX
    if (n > SIZE_MAX) {
        return NULL;
    }
#endif
    return calloc((size_t) n, size);
}

void *
tsk_memset(void *ptr, int fill, tsk_size_t size)
{
    return memset(ptr, fill, (size_t) size);
}

void *
tsk_memcpy(void *dest, const void *src, tsk_size_t size)
{
    return memcpy(dest, src, (size_t) size);
}

void *
tsk_memmove(void *dest, const void *src, tsk_size_t size)
{
    return memmove(dest, src, (size_t) size);
}

int
tsk_memcmp(const void *s1, const void *s2, tsk_size_t size)
{
    return memcmp(s1, s2, (size_t) size);
}

/* We can't initialise the stream to its real default value because
 * of limitations on static initialisers. To work around this, we initialise
 * it to NULL and then set the value to the required standard stream
 * when called. */

FILE *_tsk_debug_stream = NULL;

void
tsk_set_debug_stream(FILE *f)
{
    _tsk_debug_stream = f;
}

FILE *
tsk_get_debug_stream(void)
{
    if (_tsk_debug_stream == NULL) {
        _tsk_debug_stream = stdout;
    }
    return _tsk_debug_stream;
}

/* AVL Tree implementation. This is based directly on Knuth's implementation
 * in TAOCP. See the python/tests/test_avl_tree.py for more information,
 * and equivalent code annotated with the original algorithm listing.
 */

static void
tsk_avl_tree_int_print_node(tsk_avl_node_int_t *node, int depth, FILE *out)
{
    int d;

    if (node == NULL) {
        return;
    }
    for (d = 0; d < depth; d++) {
        fprintf(out, "  ");
    }
    fprintf(out, "key=%d balance=%d\n", (int) node->key, node->balance);
    tsk_avl_tree_int_print_node(node->llink, depth + 1, out);
    tsk_avl_tree_int_print_node(node->rlink, depth + 1, out);
}
void
tsk_avl_tree_int_print_state(tsk_avl_tree_int_t *self, FILE *out)
{
    fprintf(out, "AVL tree: size=%d height=%d\n", (int) self->size, (int) self->height);
    tsk_avl_tree_int_print_node(self->head.rlink, 0, out);
}

int
tsk_avl_tree_int_init(tsk_avl_tree_int_t *self)
{
    memset(self, 0, sizeof(*self));
    return 0;
}

int
tsk_avl_tree_int_free(tsk_avl_tree_int_t *TSK_UNUSED(self))
{
    return 0;
}

tsk_avl_node_int_t *
tsk_avl_tree_int_get_root(const tsk_avl_tree_int_t *self)
{
    return self->head.rlink;
}

tsk_avl_node_int_t *
tsk_avl_tree_int_search(const tsk_avl_tree_int_t *self, int64_t key)
{
    tsk_avl_node_int_t *P = self->head.rlink;

    while (P != NULL) {
        if (key == P->key) {
            break;
        } else if (key < P->key) {
            P = P->llink;
        } else {
            P = P->rlink;
        }
    }
    return P;
}

static int
tsk_avl_tree_int_insert_empty(tsk_avl_tree_int_t *self, tsk_avl_node_int_t *node)
{
    self->head.rlink = node;
    self->size = 1;
    self->height = 1;
    node->llink = NULL;
    node->rlink = NULL;
    node->balance = 0;
    return 0;
}

#define get_link(a, P) ((a) == -1 ? (P)->llink : (P)->rlink)
#define set_link(a, P, val)                                                             \
    do {                                                                                \
        if ((a) == -1) {                                                                \
            (P)->llink = val;                                                           \
        } else {                                                                        \
            (P)->rlink = val;                                                           \
        }                                                                               \
    } while (0);

static int
tsk_avl_tree_int_insert_non_empty(tsk_avl_tree_int_t *self, tsk_avl_node_int_t *node)
{
    const int64_t K = node->key;
    tsk_avl_node_int_t *T = &self->head;
    tsk_avl_node_int_t *S = T->rlink;
    tsk_avl_node_int_t *P = T->rlink;
    tsk_avl_node_int_t *Q, *R;
    int a;

    while (true) {
        if (K == P->key) {
            /* TODO figure out what the most useful semantics are here. Just
             * returning 1 as a non-zero value for now. */
            return 1;
        } else if (K < P->key) {
            Q = P->llink;
            if (Q == NULL) {
                Q = node;
                P->llink = Q;
                break;
            }
        } else {
            Q = P->rlink;
            if (Q == NULL) {
                Q = node;
                P->rlink = Q;
                break;
            }
        }
        if (Q->balance != 0) {
            T = P;
            S = Q;
        }
        P = Q;
    }

    self->size++;
    Q->llink = NULL;
    Q->rlink = NULL;
    Q->balance = 0;

    if (K < S->key) {
        a = -1;
    } else {
        a = 1;
    }
    P = get_link(a, S);
    R = P;
    while (P != Q) {
        if (K < P->key) {
            P->balance = -1;
            P = P->llink;
        } else if (K > P->key) {
            P->balance = 1;
            P = P->rlink;
        }
    }

    if (S->balance == 0) {
        S->balance = a;
        self->height++;
    } else if (S->balance == -a) {
        S->balance = 0;
    } else {
        if (R->balance == a) {
            P = R;
            set_link(a, S, get_link(-a, R));
            set_link(-a, R, S);
            S->balance = 0;
            R->balance = 0;
        } else if (R->balance == -a) {
            P = get_link(-a, R);
            set_link(-a, R, get_link(a, P));
            set_link(a, P, R);
            set_link(a, S, get_link(-a, P));
            set_link(-a, P, S);
            if (P->balance == a) {
                S->balance = -a;
                R->balance = 0;
            } else if (P->balance == 0) {
                S->balance = 0;
                R->balance = 0;
            } else {
                S->balance = 0;
                R->balance = a;
            }
            P->balance = 0;
        }
        if (S == T->rlink) {
            T->rlink = P;
        } else {
            T->llink = P;
        }
    }
    return 0;
}

int
tsk_avl_tree_int_insert(tsk_avl_tree_int_t *self, tsk_avl_node_int_t *node)
{
    int ret = 0;

    if (self->size == 0) {
        ret = tsk_avl_tree_int_insert_empty(self, node);
    } else {
        ret = tsk_avl_tree_int_insert_non_empty(self, node);
    }
    return ret;
}

/* An inorder traversal of the nodes in an AVL tree (or any binary search tree)
 * yields the keys in sorted order. The recursive implementation is safe here
 * because this is an AVL tree and it is strictly balanced, the depth is very
 * limited. Using GCC's __builtin_frame_address it looks like the size of a stack
 * frame for this function is 48 bytes. Assuming a stack size of 1MiB, this
 * would give us a maximum tree depth of 21845 - so, we're pretty safe.
 */
static int
ordered_nodes_traverse(tsk_avl_node_int_t *node, int index, tsk_avl_node_int_t **out)
{
    if (node == NULL) {
        return index;
    }
    index = ordered_nodes_traverse(node->llink, index, out);
    out[index] = node;
    return ordered_nodes_traverse(node->rlink, index + 1, out);
}

int
tsk_avl_tree_int_ordered_nodes(const tsk_avl_tree_int_t *self, tsk_avl_node_int_t **out)
{
    ordered_nodes_traverse(self->head.rlink, 0, out);
    return 0;
}

// Bit Array implementation. Allows us to store unsigned integers in a compact manner.
// Currently implemented as an array of 32-bit unsigned integers.

int
tsk_bitset_init(tsk_bitset_t *self, tsk_size_t num_bits, tsk_size_t length)
{
    int ret = 0;

    self->row_len = (num_bits / TSK_BITSET_BITS) + (num_bits % TSK_BITSET_BITS ? 1 : 0);
    self->len = length;
    self->data = tsk_calloc(self->row_len * length, sizeof(*self->data));
    if (self->data == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
out:
    return ret;
}

#define BITSET_DATA_ROW(bs, row) ((bs)->data + (row) * (bs)->row_len)

void
tsk_bitset_intersect(const tsk_bitset_t *self, tsk_size_t self_row,
    const tsk_bitset_t *other, tsk_size_t other_row, tsk_bitset_t *out)
{
    const tsk_bitset_val_t *restrict self_d = BITSET_DATA_ROW(self, self_row);
    const tsk_bitset_val_t *restrict other_d = BITSET_DATA_ROW(other, other_row);
    tsk_bitset_val_t *restrict out_d = out->data;
    for (tsk_size_t i = 0; i < self->row_len; i++) {
        out_d[i] = self_d[i] & other_d[i];
    }
}

void
tsk_bitset_subtract(tsk_bitset_t *self, tsk_size_t self_row, const tsk_bitset_t *other,
    tsk_size_t other_row)
{
    tsk_bitset_val_t *restrict self_d = BITSET_DATA_ROW(self, self_row);
    const tsk_bitset_val_t *restrict other_d = BITSET_DATA_ROW(other, other_row);
    for (tsk_size_t i = 0; i < self->row_len; i++) {
        self_d[i] &= ~(other_d[i]);
    }
}

void
tsk_bitset_union(tsk_bitset_t *self, tsk_size_t self_row, const tsk_bitset_t *other,
    tsk_size_t other_row)
{
    tsk_bitset_val_t *restrict self_d = BITSET_DATA_ROW(self, self_row);
    const tsk_bitset_val_t *restrict other_d = BITSET_DATA_ROW(other, other_row);
    for (tsk_size_t i = 0; i < self->row_len; i++) {
        self_d[i] |= other_d[i];
    }
}

void
tsk_bitset_set_bit(tsk_bitset_t *self, tsk_size_t row, const tsk_bitset_val_t bit)
{
    tsk_bitset_val_t i = (bit / TSK_BITSET_BITS);
    *(BITSET_DATA_ROW(self, row) + i) |= (tsk_bitset_val_t) 1
                                         << (bit - (TSK_BITSET_BITS * i));
}

bool
tsk_bitset_contains(const tsk_bitset_t *self, tsk_size_t row, const tsk_bitset_val_t bit)
{
    tsk_bitset_val_t i = (bit / TSK_BITSET_BITS);
    return *(BITSET_DATA_ROW(self, row) + i)
           & ((tsk_bitset_val_t) 1 << (bit - (TSK_BITSET_BITS * i)));
}

static inline uint32_t
popcount(tsk_bitset_val_t v)
{
    // Utilizes 12 operations per chunk. NB this only works on 32 bit integers.
    // Taken from:
    //   https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
    // There's a nice breakdown of this algorithm here:
    //   https://stackoverflow.com/a/109025
    //
    // The gcc/clang compiler flag will -mpopcnt will convert this code to a
    // popcnt instruction (most if not all modern CPUs will support this). The
    // popcnt instruction will yield some speed improvements, which depend on
    // the tree sequence.
    //
    // NB: 32bit counting is typically faster than 64bit counting for this task.
    //     (at least on x86-64)

    v = v - ((v >> 1) & 0x55555555);
    v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
    return (((v + (v >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
}

tsk_size_t
tsk_bitset_count(const tsk_bitset_t *self, tsk_size_t row)
{
    tsk_size_t i = 0;
    tsk_size_t count = 0;
    const tsk_bitset_val_t *restrict self_d = BITSET_DATA_ROW(self, row);

    for (i = 0; i < self->row_len; i++) {
        count += popcount(self_d[i]);
    }
    return count;
}

void
tsk_bitset_get_items(
    const tsk_bitset_t *self, tsk_size_t row, tsk_id_t *items, tsk_size_t *n_items)
{
    // Get the items stored in the row of a bitset.
    // Uses a de Bruijn sequence lookup table to determine the lowest bit set.
    // See the wikipedia article for more info: https://w.wiki/BYiF

    tsk_size_t i, n, off;
    tsk_bitset_val_t v, lsb; // least significant bit
    static const tsk_id_t lookup[32] = { 0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25,
        17, 4, 8, 31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9 };
    const tsk_bitset_val_t *restrict self_d = BITSET_DATA_ROW(self, row);

    n = 0;
    for (i = 0; i < self->row_len; i++) {
        v = self_d[i];
        off = i * TSK_BITSET_BITS;
        if (v == 0) {
            continue;
        }
        while ((lsb = v & -v)) {
            items[n] = lookup[(lsb * 0x077cb531U) >> 27] + (tsk_id_t) off;
            n++;
            v ^= lsb;
        }
    }
    *n_items = n;
}

void
tsk_bitset_free(tsk_bitset_t *self)
{
    tsk_safe_free(self->data);
}
