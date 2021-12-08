/*
 * MIT License
 *
 * Copyright (c) 2019-2020 Tskit Developers
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

/**
 * @file core.h
 * @brief Core utilities used in all of tskit.
 */
#ifndef __TSK_CORE_H__
#define __TSK_CORE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>

#ifdef __GNUC__
#define TSK_WARN_UNUSED __attribute__((warn_unused_result))
#define TSK_UNUSED(x) TSK_UNUSED_##x __attribute__((__unused__))
#else
#define TSK_WARN_UNUSED
#define TSK_UNUSED(x) TSK_UNUSED_##x
/* Don't bother with restrict for MSVC */
#define restrict
#endif

/* We assume CHAR_BIT == 8 when loading strings from 8-bit byte arrays */
#if CHAR_BIT != 8
#error CHAR_BIT MUST EQUAL 8
#endif

/* This sets up TSK_DBL_DECIMAL_DIG, which can then be used as a
 * precision specifier when writing out doubles, if you want sufficient
 * decimal digits to be written to guarantee a lossless round-trip
 * after being read back in.  Usage:
 *
 *     printf("%.*g", TSK_DBL_DECIMAL_DIG, foo);
 *
 * See https://stackoverflow.com/a/19897395/2752221
 */
#ifdef DBL_DECIMAL_DIG
#define TSK_DBL_DECIMAL_DIG (DBL_DECIMAL_DIG)
#else
#define TSK_DBL_DECIMAL_DIG (DBL_DIG + 3)
#endif

/* We define a specific NAN value for default mutation time which indicates
 * the time is unknown. We use a specific value so that if mutation time is set to
 * a NAN from a computation we can reject it. This specific value is a non-signalling
 * NAN with the last six fraction bytes set to the ascii of "tskit!"
 */
#define TSK_UNKNOWN_TIME_HEX 0x7FF874736B697421ULL
static inline double
__tsk_nan_f(void)
{
    const union {
        uint64_t i;
        double f;
    } nan_union = { .i = TSK_UNKNOWN_TIME_HEX };
    return nan_union.f;
}
#define TSK_UNKNOWN_TIME __tsk_nan_f()

#define TSK_TIME_UNITS_UNKNOWN "unknown"
#define TSK_TIME_UNITS_UNCALIBRATED "uncalibrated"

/**
@brief Tskit Object IDs.

@rst
All objects in tskit are referred to by integer IDs corresponding to the
row they occupy in the relevant table. The ``tsk_id_t`` type should be used
when manipulating these ID values. The reserved value ``TSK_NULL`` (-1) defines
missing data.
@endrst
*/
#ifdef _TSK_BIG_TABLES
/* Allow tables to have more than 2^31 rows. This is an EXPERIMENTAL feature
 * and is not supported in any way. This typedef is only included for
 * future-proofing purposes, so that we can be sure that we don't make any
 * design decisions that are incompatible with big tables by building the
 * library in 64 bit mode in CI. See the discussion here for more background:

 * https://github.com/tskit-dev/tskit/issues/343
 *
 * If you need big tables, please open an issue on GitHub to discuss, or comment
 * on the thread above.
 */
typedef int64_t tsk_id_t;
#define TSK_MAX_ID INT64_MAX - 1
#define TSK_ID_STORAGE_TYPE KAS_INT64
#else
typedef int32_t tsk_id_t;
#define TSK_MAX_ID INT32_MAX - 1
#define TSK_ID_STORAGE_TYPE KAS_INT32
#endif

/**
@brief Tskit sizes.

@rst
The ``tsk_size_t`` type is an unsigned integer used for any size or count value.
@endrst
*/
typedef uint64_t tsk_size_t;
#define TSK_MAX_SIZE UINT64_MAX
#define TSK_SIZE_STORAGE_TYPE KAS_UINT64

/**
@brief Container for bitwise flags.

@rst
Bitwise flags are used in tskit as a column type and also as a way to
specify options to API functions.
@endrst
*/
typedef uint32_t tsk_flags_t;
#define TSK_FLAGS_STORAGE_TYPE KAS_UINT32

// clang-format off
/**
@defgroup API_VERSION_GROUP API version macros.
@{
*/
/**
The library major version. Incremented when breaking changes to the API or ABI are
introduced. This includes any changes to the signatures of functions and the
sizes and types of externally visible structs.
*/
#define TSK_VERSION_MAJOR   0
/**
The library major version. Incremented when non-breaking backward-compatible changes
to the API or ABI are introduced, i.e., the addition of a new function.
*/
#define TSK_VERSION_MINOR   99
/**
The library patch version. Incremented when any changes not relevant to the
to the API or ABI are introduced, i.e., internal refactors of bugfixes.
*/
#define TSK_VERSION_PATCH   15
/** @} */

/* Node flags */
#define TSK_NODE_IS_SAMPLE 1u

/* The null ID */
#define TSK_NULL ((tsk_id_t) -1)

/* Missing data in an array of genotypes */
#define TSK_MISSING_DATA    (-1)

#define TSK_FILE_FORMAT_NAME          "tskit.trees"
#define TSK_FILE_FORMAT_NAME_LENGTH   11
#define TSK_FILE_FORMAT_VERSION_MAJOR 12
#define TSK_FILE_FORMAT_VERSION_MINOR 7

/**
@defgroup GENERAL_ERROR_GROUP General errors.
@{
*/

/**
Generic error thrown when no other message can be generated.
*/
#define TSK_ERR_GENERIC                                             -1
/**
Memory could not be allocated.
*/
#define TSK_ERR_NO_MEMORY                                           -2
/**
An IO error occured.
*/
#define TSK_ERR_IO                                                  -3
#define TSK_ERR_BAD_PARAM_VALUE                                     -4
#define TSK_ERR_BUFFER_OVERFLOW                                     -5
#define TSK_ERR_UNSUPPORTED_OPERATION                               -6
#define TSK_ERR_GENERATE_UUID                                       -7
/**
The file stream ended after reading zero bytes.
*/
#define TSK_ERR_EOF                                                 -8
/** @} */

/**
@defgroup FILE_FORMAT_ERROR_GROUP File format errors.
@{
*/

/**
A file could not be read because it is in the wrong format
*/
#define TSK_ERR_FILE_FORMAT                                         -100
/**
The file is in tskit format, but the version is too old for the
library to read. The file should be upgraded to the latest version
using the ``tskit upgrade`` command line utility.
*/
#define TSK_ERR_FILE_VERSION_TOO_OLD                                -101
/**
The file is in tskit format, but the version is too new for the
library to read. To read the file you must upgrade the version
of tskit.
*/
#define TSK_ERR_FILE_VERSION_TOO_NEW                                -102
/** @} */

/**
A column that is a required member of a table was not found in
the file.
*/
#define TSK_ERR_REQUIRED_COL_NOT_FOUND                              -103

/**
One of a pair of columns that must be specified together was
not found in the file.
*/
#define TSK_ERR_BOTH_COLUMNS_REQUIRED                               -104

/**
An unsupported type was provided for a column in the file.
*/
#define TSK_ERR_BAD_COLUMN_TYPE                                     -105


/* Out of bounds errors */
#define TSK_ERR_BAD_OFFSET                                          -200
#define TSK_ERR_OUT_OF_BOUNDS                                       -201
#define TSK_ERR_NODE_OUT_OF_BOUNDS                                  -202
#define TSK_ERR_EDGE_OUT_OF_BOUNDS                                  -203
#define TSK_ERR_POPULATION_OUT_OF_BOUNDS                            -204
#define TSK_ERR_SITE_OUT_OF_BOUNDS                                  -205
#define TSK_ERR_MUTATION_OUT_OF_BOUNDS                              -206
#define TSK_ERR_INDIVIDUAL_OUT_OF_BOUNDS                            -207
#define TSK_ERR_MIGRATION_OUT_OF_BOUNDS                             -208
#define TSK_ERR_PROVENANCE_OUT_OF_BOUNDS                            -209
#define TSK_ERR_TIME_NONFINITE                                      -210
#define TSK_ERR_GENOME_COORDS_NONFINITE                             -211
#define TSK_ERR_SEEK_OUT_OF_BOUNDS                                  -212

/* Edge errors */
#define TSK_ERR_NULL_PARENT                                         -300
#define TSK_ERR_NULL_CHILD                                          -301
#define TSK_ERR_EDGES_NOT_SORTED_PARENT_TIME                        -302
#define TSK_ERR_EDGES_NONCONTIGUOUS_PARENTS                         -303
#define TSK_ERR_EDGES_NOT_SORTED_CHILD                              -304
#define TSK_ERR_EDGES_NOT_SORTED_LEFT                               -305
#define TSK_ERR_BAD_NODE_TIME_ORDERING                              -306
#define TSK_ERR_BAD_EDGE_INTERVAL                                   -307
#define TSK_ERR_DUPLICATE_EDGES                                     -308
#define TSK_ERR_RIGHT_GREATER_SEQ_LENGTH                            -309
#define TSK_ERR_LEFT_LESS_ZERO                                      -310
#define TSK_ERR_BAD_EDGES_CONTRADICTORY_CHILDREN                    -311
#define TSK_ERR_CANT_PROCESS_EDGES_WITH_METADATA                    -312

/* Site errors */
#define TSK_ERR_UNSORTED_SITES                                      -400
#define TSK_ERR_DUPLICATE_SITE_POSITION                             -401
#define TSK_ERR_BAD_SITE_POSITION                                   -402

/* Mutation errors */
#define TSK_ERR_MUTATION_PARENT_DIFFERENT_SITE                      -500
#define TSK_ERR_MUTATION_PARENT_EQUAL                               -501
#define TSK_ERR_MUTATION_PARENT_AFTER_CHILD                         -502
#define TSK_ERR_MUTATION_PARENT_INCONSISTENT                        -503
#define TSK_ERR_UNSORTED_MUTATIONS                                  -504
#define TSK_ERR_NON_SINGLE_CHAR_MUTATION                            -505
#define TSK_ERR_MUTATION_TIME_YOUNGER_THAN_NODE                     -506
#define TSK_ERR_MUTATION_TIME_OLDER_THAN_PARENT_MUTATION            -507
#define TSK_ERR_MUTATION_TIME_OLDER_THAN_PARENT_NODE                -508
#define TSK_ERR_MUTATION_TIME_HAS_BOTH_KNOWN_AND_UNKNOWN            -509

/* Migration errors */
#define TSK_ERR_UNSORTED_MIGRATIONS                                 -550

/* Sample errors */
#define TSK_ERR_DUPLICATE_SAMPLE                                    -600
#define TSK_ERR_BAD_SAMPLES                                         -601

/* Table errors */
#define TSK_ERR_BAD_TABLE_POSITION                                  -700
#define TSK_ERR_BAD_SEQUENCE_LENGTH                                 -701
#define TSK_ERR_TABLES_NOT_INDEXED                                  -702
#define TSK_ERR_TABLE_OVERFLOW                                      -703
#define TSK_ERR_COLUMN_OVERFLOW                                     -704
#define TSK_ERR_TREE_OVERFLOW                                       -705
#define TSK_ERR_METADATA_DISABLED                                   -706

/* Limitations */
#define TSK_ERR_ONLY_INFINITE_SITES                                 -800
#define TSK_ERR_SIMPLIFY_MIGRATIONS_NOT_SUPPORTED                   -801
#define TSK_ERR_SORT_MIGRATIONS_NOT_SUPPORTED                       -802
#define TSK_ERR_SORT_OFFSET_NOT_SUPPORTED                           -803
#define TSK_ERR_NONBINARY_MUTATIONS_UNSUPPORTED                     -804
#define TSK_ERR_MIGRATIONS_NOT_SUPPORTED                            -805
#define TSK_ERR_CANNOT_EXTEND_FROM_SELF                             -806
#define TSK_ERR_SILENT_MUTATIONS_NOT_SUPPORTED                      -807

/* Stats errors */
#define TSK_ERR_BAD_NUM_WINDOWS                                     -900
#define TSK_ERR_BAD_WINDOWS                                         -901
#define TSK_ERR_MULTIPLE_STAT_MODES                                 -902
#define TSK_ERR_BAD_STATE_DIMS                                      -903
#define TSK_ERR_BAD_RESULT_DIMS                                     -904
#define TSK_ERR_INSUFFICIENT_SAMPLE_SETS                            -905
#define TSK_ERR_INSUFFICIENT_INDEX_TUPLES                           -906
#define TSK_ERR_BAD_SAMPLE_SET_INDEX                                -907
#define TSK_ERR_EMPTY_SAMPLE_SET                                    -908
#define TSK_ERR_UNSUPPORTED_STAT_MODE                               -909
#define TSK_ERR_TIME_UNCALIBRATED                                   -910


/* Mutation mapping errors */
#define TSK_ERR_GENOTYPES_ALL_MISSING                              -1000
#define TSK_ERR_BAD_GENOTYPE                                       -1001
#define TSK_ERR_BAD_ANCESTRAL_STATE                                -1002

/* Genotype decoding errors */
#define TSK_ERR_MUST_IMPUTE_NON_SAMPLES                            -1100
#define TSK_ERR_ALLELE_NOT_FOUND                                   -1101
#define TSK_ERR_TOO_MANY_ALLELES                                   -1102
#define TSK_ERR_ZERO_ALLELES                                       -1103

/* Distance metric errors */
#define TSK_ERR_SAMPLE_SIZE_MISMATCH                               -1200
#define TSK_ERR_SAMPLES_NOT_EQUAL                                  -1201
#define TSK_ERR_MULTIPLE_ROOTS                                     -1202
#define TSK_ERR_UNARY_NODES                                        -1203
#define TSK_ERR_SEQUENCE_LENGTH_MISMATCH                           -1204
#define TSK_ERR_NO_SAMPLE_LISTS                                    -1205

/* Haplotype matching errors */
#define TSK_ERR_NULL_VITERBI_MATRIX                                -1300
#define TSK_ERR_MATCH_IMPOSSIBLE                                   -1301
#define TSK_ERR_BAD_COMPRESSED_MATRIX_NODE                         -1302
#define TSK_ERR_TOO_MANY_VALUES                                    -1303

/* Union errors */
#define TSK_ERR_UNION_BAD_MAP                                      -1400
#define TSK_ERR_UNION_DIFF_HISTORIES                               -1401

/* IBD errors */
#define TSK_ERR_SAME_NODES_IN_PAIR                                 -1500
#define TSK_ERR_IBD_PAIRS_NOT_STORED                               -1501
#define TSK_ERR_IBD_SEGMENTS_NOT_STORED                            -1502

/* Simplify errors */
#define TSK_ERR_KEEP_UNARY_MUTUALLY_EXCLUSIVE                      -1600

/* Individual errors */
#define TSK_ERR_UNSORTED_INDIVIDUALS                               -1700
#define TSK_ERR_INDIVIDUAL_SELF_PARENT                             -1701
#define TSK_ERR_INDIVIDUAL_PARENT_CYCLE                            -1702

// clang-format on

/* This bit is 0 for any errors originating from kastore */
#define TSK_KAS_ERR_BIT 14

int tsk_set_kas_error(int err);
bool tsk_is_kas_error(int err);
int tsk_get_kas_error(int err);

/**
@brief Return a description of the specified error.

The memory for the returned string is handled by the library and should
not be freed by client code.

@param err A tskit error code.
@return A description of the error.
*/
const char *tsk_strerror(int err);

#ifndef TSK_BUG_ASSERT_MESSAGE
#define TSK_BUG_ASSERT_MESSAGE                                                          \
    "If you are using tskit directly please open an issue on"                           \
    " GitHub, ideally with a reproducible example."                                     \
    " (https://github.com/tskit-dev/tskit/issues) If you are"                           \
    " using software that uses tskit, please report an issue"                           \
    " to that software's issue tracker, at least initially."
#endif

/**
We often wish to assert a condition that is unexpected, but using the normal `assert`
means compiling without NDEBUG. This macro still asserts when NDEBUG is defined.
If you are using this macro in your own software then please set TSK_BUG_ASSERT_MESSAGE
to point users to your issue tracker.
*/
#define tsk_bug_assert(condition)                                                       \
    do {                                                                                \
        if (!(condition)) {                                                             \
            fprintf(stderr, "Bug detected in %s at line %d. %s\n", __FILE__, __LINE__,  \
                TSK_BUG_ASSERT_MESSAGE);                                                \
            abort();                                                                    \
        }                                                                               \
    } while (0)

void __tsk_safe_free(void **ptr);
#define tsk_safe_free(pointer) __tsk_safe_free((void **) &(pointer))

#define TSK_MAX(a, b) ((a) > (b) ? (a) : (b))
#define TSK_MIN(a, b) ((a) < (b) ? (a) : (b))

/* This is a simple allocator that is optimised to efficiently allocate a
 * large number of small objects without large numbers of calls to malloc.
 * The allocator mallocs memory in chunks of a configurable size. When
 * responding to calls to get(), it will return a chunk of this memory.
 * This memory cannot be subsequently handed back to the allocator. However,
 * all memory allocated by the allocator can be returned at once by calling
 * reset.
 */

typedef struct {
    size_t chunk_size; /* number of bytes per chunk */
    size_t top;        /* the offset of the next available byte in the current chunk */
    size_t current_chunk;   /* the index of the chunk currently being used */
    size_t total_size;      /* the total number of bytes allocated + overhead. */
    size_t total_allocated; /* the total number of bytes allocated. */
    size_t num_chunks;      /* the number of memory chunks. */
    char **mem_chunks;      /* the memory chunks */
} tsk_blkalloc_t;

extern void tsk_blkalloc_print_state(tsk_blkalloc_t *self, FILE *out);
extern int tsk_blkalloc_reset(tsk_blkalloc_t *self);
extern int tsk_blkalloc_init(tsk_blkalloc_t *self, size_t chunk_size);
extern void *tsk_blkalloc_get(tsk_blkalloc_t *self, size_t size);
extern void tsk_blkalloc_free(tsk_blkalloc_t *self);

typedef struct _tsk_avl_node_int_t {
    int64_t key;
    void *value;
    struct _tsk_avl_node_int_t *llink;
    struct _tsk_avl_node_int_t *rlink;
    /* This can only contain -1, 0, 1. We could set it to a smaller type,
     * but there's no point because of struct padding and alignment so
     * it's simplest to keep it as a plain int. */
    int balance;
} tsk_avl_node_int_t;

typedef struct {
    tsk_avl_node_int_t head;
    tsk_size_t size;
    tsk_size_t height;
} tsk_avl_tree_int_t;

int tsk_avl_tree_int_init(tsk_avl_tree_int_t *self);
int tsk_avl_tree_int_free(tsk_avl_tree_int_t *self);
void tsk_avl_tree_int_print_state(tsk_avl_tree_int_t *self, FILE *out);
int tsk_avl_tree_int_insert(tsk_avl_tree_int_t *self, tsk_avl_node_int_t *node);
tsk_avl_node_int_t *tsk_avl_tree_int_search(const tsk_avl_tree_int_t *self, int64_t key);
int tsk_avl_tree_int_ordered_nodes(
    const tsk_avl_tree_int_t *self, tsk_avl_node_int_t **out);
tsk_avl_node_int_t *tsk_avl_tree_int_get_root(const tsk_avl_tree_int_t *self);

tsk_size_t tsk_search_sorted(const double *array, tsk_size_t size, double value);

double tsk_round(double x, unsigned int ndigits);

/**
@brief Check if a number is TSK_UNKNOWN_TIME

@rst
Unknown time values in tskit are represented by a particular NaN value. Since NaN values
are not equal to each other by definition, a simple comparison like
``mutation.time == TSK_UNKNOWN_TIME`` will fail even if the mutation's time is
TSK_UNKNOWN_TIME. This function compares the underlying bit representation of a double
value and returns true iff it is equal to the specific NaN value TSK_UNKNOWN_TIME.
@endrst

@param val The number to check
@return true if the number is TSK_UNKNOWN_TIME else false
*/
bool tsk_is_unknown_time(double val);

/* We define local versions of isnan and isfinite to workaround some portability
 * issues. */
bool tsk_isnan(double val);
bool tsk_isfinite(double val);

#define TSK_UUID_SIZE 36
int tsk_generate_uuid(char *dest, int flags);

/* TODO most of these can probably be macros so they compile out as no-ops.
 * Lets do the 64 bit tsk_size_t switch first though. */
void *tsk_malloc(tsk_size_t size);
void *tsk_realloc(void *ptr, tsk_size_t size);
void *tsk_calloc(tsk_size_t n, size_t size);
void *tsk_memset(void *ptr, int fill, tsk_size_t size);
void *tsk_memcpy(void *dest, const void *src, tsk_size_t size);
void *tsk_memmove(void *dest, const void *src, tsk_size_t size);
int tsk_memcmp(const void *s1, const void *s2, tsk_size_t size);

/* Developer debug utilities. These are **not** threadsafe */
void tsk_set_debug_stream(FILE *f);
FILE *tsk_get_debug_stream(void);

#ifdef __cplusplus
}
#endif

#endif
