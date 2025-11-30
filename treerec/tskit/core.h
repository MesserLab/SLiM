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

/**
@brief Tskit Object IDs.

@rst
All objects in tskit are referred to by integer IDs corresponding to the
row they occupy in the relevant table. The ``tsk_id_t`` type should be used
when manipulating these ID values. The reserved value :c:macro:`TSK_NULL` (-1) defines
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

/**
@brief Boolean type.

@rst
Fixed-size (1 byte) boolean values.
@endrst
*/
typedef uint8_t tsk_bool_t;

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
#define TSK_VERSION_MAJOR   1
/**
The library minor version. Incremented when non-breaking backward-compatible changes
to the API or ABI are introduced, i.e., the addition of a new function.
*/
#define TSK_VERSION_MINOR   3
/**
The library patch version. Incremented when any changes not relevant to the
to the API or ABI are introduced, i.e., internal refactors of bugfixes.
*/
#define TSK_VERSION_PATCH   0
/** @} */

/*
We define a specific NAN value for default mutation time which indicates
the time is unknown. We use a specific value so that if mutation time is set to
a NAN from a computation we can reject it. This specific value is a non-signalling
NAN with the last six fraction bytes set to the ascii of "tskit!"
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

/**
@defgroup GENERIC_CONSTANTS General options flags used in some functions.
@{
*/
/**
Used in node flags to indicate that a node is a sample node.
*/
#define TSK_NODE_IS_SAMPLE 1u

/**
Null value used for cases such as absent id references.
*/
#define TSK_NULL ((tsk_id_t) -1)

/**
Value used for missing data in genotype arrays.
*/
#define TSK_MISSING_DATA    (-1)

/**
Value to indicate that a time is unknown. Note that this value is a non-signalling NAN
whose representation differs from the NAN generated by computations such as divide by zeros.
*/
#define TSK_UNKNOWN_TIME __tsk_nan_f()

/** @} */

#define TSK_TIME_UNITS_UNKNOWN "unknown"
#define TSK_TIME_UNITS_UNCALIBRATED "uncalibrated"


#define TSK_FILE_FORMAT_NAME          "tskit.trees"
#define TSK_FILE_FORMAT_NAME_LENGTH   11
#define TSK_FILE_FORMAT_VERSION_MAJOR 12
#define TSK_FILE_FORMAT_VERSION_MINOR 7

/**
@defgroup GENERIC_FUNCTION_OPTIONS General options flags used in some functions.
@{
*/

/* Place the common options at the top of the space; this way we can start
options for individual functions at the bottom without worrying about
clashing with the common options
*/

/** Turn on debugging output. Not supported by all functions. */
#define TSK_DEBUG (1u << 31)

/** Do not initialise the parameter object. */
#define TSK_NO_INIT (1u << 30)

/**
Do not run integrity checks before performing an operation.
This performance optimisation should not be used unless the calling code can
guarantee reference integrity within the table collection. References
to rows not in the table or bad offsets will result in undefined
behaviour.
*/
#define TSK_NO_CHECK_INTEGRITY (1u << 29)

/**
Instead of taking a copy of input objects, the function should take ownership
of them and manage their lifecycle. The caller specifying this flag should no
longer modify or free the object or objects passed. See individual functions
using this flag for what object it applies to.
*/
#define TSK_TAKE_OWNERSHIP (1u << 28)

/** @} */


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
An IO error occurred.
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
using the ``tskit upgrade`` command line utility from tskit version<0.6.2.
*/
#define TSK_ERR_FILE_VERSION_TOO_OLD                                -101
/**
The file is in tskit format, but the version is too new for the
library to read. To read the file you must upgrade the version
of tskit.
*/
#define TSK_ERR_FILE_VERSION_TOO_NEW                                -102

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
/** @} */

/**
@defgroup OOB_ERROR_GROUP Out of bounds errors.
@{
*/
/**
A bad value was provided for a ragged column offset, values should
start at zero and be monotonically increasing.
*/
#define TSK_ERR_BAD_OFFSET                                          -200
/**
A position to seek to was less than zero or greater than the length
of the genome
*/
#define TSK_ERR_SEEK_OUT_OF_BOUNDS                                  -201
/**
A node id was less than zero or greater than the final index
*/
#define TSK_ERR_NODE_OUT_OF_BOUNDS                                  -202
/**
A edge id was less than zero or greater than the final index
*/
#define TSK_ERR_EDGE_OUT_OF_BOUNDS                                  -203
/**
A population id was less than zero or greater than the final index
*/
#define TSK_ERR_POPULATION_OUT_OF_BOUNDS                            -204
/**
A site id was less than zero or greater than the final index
*/
#define TSK_ERR_SITE_OUT_OF_BOUNDS                                  -205
/**
A mutation id was less than zero or greater than the final index
*/
#define TSK_ERR_MUTATION_OUT_OF_BOUNDS                              -206
/**
An individual id was less than zero or greater than the final index
*/
#define TSK_ERR_INDIVIDUAL_OUT_OF_BOUNDS                            -207
/**
A migration id was less than zero or greater than the final index
*/
#define TSK_ERR_MIGRATION_OUT_OF_BOUNDS                             -208
/**
A provenance id was less than zero or greater than the final index
*/
#define TSK_ERR_PROVENANCE_OUT_OF_BOUNDS                            -209
/**
A time value was non-finite (NaN counts as finite)
*/
#define TSK_ERR_TIME_NONFINITE                                      -210
/**
A genomic position was non-finite
*/
#define TSK_ERR_GENOME_COORDS_NONFINITE                             -211
/**
One of the rows in the retained table refers to a row that has been
deleted.
*/
#define TSK_ERR_KEEP_ROWS_MAP_TO_DELETED                            -212
/**
A genomic position was less than zero or greater equal to the sequence
length
*/
#define TSK_ERR_POSITION_OUT_OF_BOUNDS                              -213

/** @} */

/**
@defgroup EDGE_ERROR_GROUP Edge errors.
@{
*/
/**
A parent node of an edge was TSK_NULL.
*/
#define TSK_ERR_NULL_PARENT                                         -300
/**
A child node of an edge was TSK_NULL.
*/
#define TSK_ERR_NULL_CHILD                                          -301
/**
The edge table was not sorted by the time of each edge's parent
nodes. Sort order is (time[parent], child, left).
*/
#define TSK_ERR_EDGES_NOT_SORTED_PARENT_TIME                        -302
/**
A parent node had edges that were non-contigious.
*/
#define TSK_ERR_EDGES_NONCONTIGUOUS_PARENTS                         -303
/**
The edge table was not sorted by the id of the child node of each edge.
Sort order is (time[parent], child, left).
*/
#define TSK_ERR_EDGES_NOT_SORTED_CHILD                              -304
/**
The edge table was not sorted by the left coordinate each edge.
Sort order is (time[parent], child, left).
*/
#define TSK_ERR_EDGES_NOT_SORTED_LEFT                               -305
/**
An edge had child node that was older than the parent. Parent times must
be greater than the child time.
*/
#define TSK_ERR_BAD_NODE_TIME_ORDERING                              -306
/**
An edge had a genomic interval where right was greater or equal to left.
*/
#define TSK_ERR_BAD_EDGE_INTERVAL                                   -307
/**
An edge was duplicated.
*/
#define TSK_ERR_DUPLICATE_EDGES                                     -308
/**
An edge had a right coord greater than the genomic length.
*/
#define TSK_ERR_RIGHT_GREATER_SEQ_LENGTH                            -309
/**
An edge had a left coord less than zero.
*/
#define TSK_ERR_LEFT_LESS_ZERO                                      -310
/**
A parent node had edges that were contradictory over an interval.
*/
#define TSK_ERR_BAD_EDGES_CONTRADICTORY_CHILDREN                    -311
/**
A method that doesn't support edge metadata was attempted on an edge
table containing metadata.
*/
#define TSK_ERR_CANT_PROCESS_EDGES_WITH_METADATA                    -312
/** @} */

/**
@defgroup SITE_ERROR_GROUP Site errors.
@{
*/
/**
The site table was not in order of increasing genomic position.
*/
#define TSK_ERR_UNSORTED_SITES                                      -400
/**
The site table had more than one site at a single genomic position.
*/
#define TSK_ERR_DUPLICATE_SITE_POSITION                             -401
/**
A site had a position that was less than zero or greater than the sequence
length.
*/
#define TSK_ERR_BAD_SITE_POSITION                                   -402
/** @} */

/**
@defgroup MUTATION_ERROR_GROUP Mutation errors.
@{
*/
/**
A mutation had a parent mutation that was at a different site.
*/
#define TSK_ERR_MUTATION_PARENT_DIFFERENT_SITE                      -500
/**
A mutation had a parent mutation that was itself.
*/
#define TSK_ERR_MUTATION_PARENT_EQUAL                               -501
/**
A mutation had a parent mutation that had a greater id.
*/
#define TSK_ERR_MUTATION_PARENT_AFTER_CHILD                         -502
/**
Two or more mutation parent references formed a loop
*/
#define TSK_ERR_MUTATION_PARENT_INCONSISTENT                        -503
/**
The mutation table was not in the order of non-decreasing site id and
non-increasing time within each site.
*/
#define TSK_ERR_UNSORTED_MUTATIONS                                  -504
/* 505 was the now unused TSK_ERR_NON_SINGLE_CHAR_MUTATION */
/**
A mutation's time was younger (not >=) the time of its node
and wasn't TSK_UNKNOWN_TIME.
*/
#define TSK_ERR_MUTATION_TIME_YOUNGER_THAN_NODE                     -506
/**
A mutation's time was older (not <=) than the time of its parent
mutation, and wasn't TSK_UNKNOWN_TIME.
*/
#define TSK_ERR_MUTATION_TIME_OLDER_THAN_PARENT_MUTATION            -507
/**
A mutation's time was older (not <) than the time of the parent node of
the edge on which it occurs, and wasn't TSK_UNKNOWN_TIME.
*/
#define TSK_ERR_MUTATION_TIME_OLDER_THAN_PARENT_NODE                -508
/**
A single site had a mixture of known mutation times and TSK_UNKNOWN_TIME
*/
#define TSK_ERR_MUTATION_TIME_HAS_BOTH_KNOWN_AND_UNKNOWN            -509
/**
Some mutations have TSK_UNKNOWN_TIME in an algorithm where that's
disallowed (use compute_mutation_times?).
*/
#define TSK_ERR_DISALLOWED_UNKNOWN_MUTATION_TIME                    -510

/** 
A mutation's parent was not consistent with the topology of the tree.
 */
#define TSK_ERR_BAD_MUTATION_PARENT                                 -511

/** @} */

/**
@defgroup MIGRATION_ERROR_GROUP Migration errors.
@{
*/
/**
The migration table was not sorted by time.
*/
#define TSK_ERR_UNSORTED_MIGRATIONS                                 -550
/** @} */

/**
@defgroup SAMPLE_ERROR_GROUP Sample errors.
@{
*/
/**
A duplicate sample was specified.
*/
#define TSK_ERR_DUPLICATE_SAMPLE                                    -600
/**
A sample id that was not valid was specified.
*/
#define TSK_ERR_BAD_SAMPLES                                         -601
/** @} */

/**
@defgroup TABLE_ERROR_GROUP Table errors.
@{
*/
/**
An invalid table position was specifed.
*/
#define TSK_ERR_BAD_TABLE_POSITION                                  -700
/**
A sequence length equal to or less than zero was specified.
*/
#define TSK_ERR_BAD_SEQUENCE_LENGTH                                 -701
/**
The table collection was not indexed.
*/
#define TSK_ERR_TABLES_NOT_INDEXED                                  -702
/**
Tables cannot be larger than 2**31 rows.
*/
#define TSK_ERR_TABLE_OVERFLOW                                      -703
/**
Ragged array columns cannot be larger than 2**64 bytes.
*/
#define TSK_ERR_COLUMN_OVERFLOW                                     -704
/**
The table collection contains more than 2**31 trees.
*/
#define TSK_ERR_TREE_OVERFLOW                                       -705
/**
Metadata was attempted to be set on a table where it is disabled.
*/
#define TSK_ERR_METADATA_DISABLED                                   -706
/**
There was an error with the table's indexes.
*/
#define TSK_ERR_TABLES_BAD_INDEXES                                  -707
/** @} */

/**
@defgroup LIMITATION_ERROR_GROUP Limitation errors.
@{
*/
/**
An operation was attempted that only supports infinite sites, i.e.
at most a single mutation per site.
*/
#define TSK_ERR_ONLY_INFINITE_SITES                                 -800
/**
Simplification was attempted with migrations present, which are not
supported.
*/
#define TSK_ERR_SIMPLIFY_MIGRATIONS_NOT_SUPPORTED                   -801
/**
Sorting was attempted on migrations, which is not supported.
*/
#define TSK_ERR_SORT_MIGRATIONS_NOT_SUPPORTED                       -802
/**
An invalid sort offset was specified, for sites and mutations this must
be either 0 or the table length.
*/
#define TSK_ERR_SORT_OFFSET_NOT_SUPPORTED                           -803
/**
An operation was attempted that only supports binary mutations.
*/
#define TSK_ERR_NONBINARY_MUTATIONS_UNSUPPORTED                     -804
/**
An operation was attempted that doesn't support migrations, with a
non-empty migration table.
*/
#define TSK_ERR_MIGRATIONS_NOT_SUPPORTED                            -805
/**
A table attempted to extend from itself.
*/
#define TSK_ERR_CANNOT_EXTEND_FROM_SELF                             -806
/**
An operation was attempted that doesn't support silent mutations, i.e.
a mutation that doesn't change the allelic state.
*/
#define TSK_ERR_SILENT_MUTATIONS_NOT_SUPPORTED                      -807
/**
A copy of a variant cannot be decoded.
*/
#define TSK_ERR_VARIANT_CANT_DECODE_COPY                            -808
/**
A tree sequence cannot take ownership of a table collection where
TSK_NO_EDGE_METADATA.
*/
#define TSK_ERR_CANT_TAKE_OWNERSHIP_NO_EDGE_METADATA                -809
/**
Operation is undefined for nonbinary trees
*/
#define TSK_ERR_UNDEFINED_NONBINARY                                 -810
/**
Operation is undefined for trees with multiple roots.
*/
#define TSK_ERR_UNDEFINED_MULTIROOT                                 -811

/** @} */

/**
@defgroup STATS_ERROR_GROUP Stats errors.
@{
*/
/**
Zero windows were specified, at least one window must be specified.
*/
#define TSK_ERR_BAD_NUM_WINDOWS                                     -900
/**
The window specification was not an increasing list of positions between
0 and the sequence length.
*/
#define TSK_ERR_BAD_WINDOWS                                         -901
/**
More than one stat mode was specified.
*/
#define TSK_ERR_MULTIPLE_STAT_MODES                                 -902
/**
The state dimension was not >=1.
*/
#define TSK_ERR_BAD_STATE_DIMS                                      -903
/**
The result dimension was not >=1.
*/
#define TSK_ERR_BAD_RESULT_DIMS                                     -904
/**
Insufficient sample sets were provided.
*/
#define TSK_ERR_INSUFFICIENT_SAMPLE_SETS                            -905
/**
Insufficient sample set index tuples were provided.
*/
#define TSK_ERR_INSUFFICIENT_INDEX_TUPLES                           -906
/**
The sample set index was out of bounds.
*/
#define TSK_ERR_BAD_SAMPLE_SET_INDEX                                -907
/**
The sample set index was empty.
*/
#define TSK_ERR_EMPTY_SAMPLE_SET                                    -908
/**
A stat mode was attempted that is not supported by the operation.
*/
#define TSK_ERR_UNSUPPORTED_STAT_MODE                               -909
/**
Statistics based on branch lengths were attempted when the ``time_units``
were ``uncalibrated``.
*/
#define TSK_ERR_TIME_UNCALIBRATED                                   -910
/**
The TSK_STAT_POLARISED option was passed to a statistic that does
not support it.
*/
#define TSK_ERR_STAT_POLARISED_UNSUPPORTED                          -911
/**
The TSK_STAT_SPAN_NORMALISE option was passed to a statistic that does
not support it.
*/
#define TSK_ERR_STAT_SPAN_NORMALISE_UNSUPPORTED                     -912
/**
Insufficient weights were provided.
*/
#define TSK_ERR_INSUFFICIENT_WEIGHTS                                -913
/**
The node bin map contains a value less than TSK_NULL.
*/
#define TSK_ERR_BAD_NODE_BIN_MAP                                    -914
/**
Maximum index in node bin map is greater than output dimension.
*/
#define TSK_ERR_BAD_NODE_BIN_MAP_DIM                                -915
/**
The vector of quantiles is out of bounds or in nonascending order.
*/
#define TSK_ERR_BAD_QUANTILES                                       -916
/**
Times are not in ascending order
*/
#define TSK_ERR_UNSORTED_TIMES                                      -917
/*
The provided positions are not provided in strictly increasing order
*/
#define TSK_ERR_STAT_UNSORTED_POSITIONS                             -918
/**
The provided positions are not unique
*/
#define TSK_ERR_STAT_DUPLICATE_POSITIONS                            -919
/**
The provided sites are not provided in strictly increasing position order
*/
#define TSK_ERR_STAT_UNSORTED_SITES                                 -920
/**
The provided sites are not unique
*/
#define TSK_ERR_STAT_DUPLICATE_SITES                                -921
/**
The number of time windows is zero
*/
#define TSK_ERR_BAD_TIME_WINDOWS_DIM                                -922
/**
Sample times do not all equal the start of first time window
*/
#define TSK_ERR_BAD_SAMPLE_PAIR_TIMES                               -923
/**
Time windows are not strictly increasing
*/
#define TSK_ERR_BAD_TIME_WINDOWS                                    -924
/**
Time windows do not end at infinity
*/
#define TSK_ERR_BAD_TIME_WINDOWS_END                                -925
/**
Node time does not fall within assigned time window
*/
#define TSK_ERR_BAD_NODE_TIME_WINDOW                                -926
/** @} */

/**
@defgroup MAPPING_ERROR_GROUP Mutation mapping errors.
@{
*/
/**
Only missing genotypes were specified, at least one non-missing is
required.
*/
#define TSK_ERR_GENOTYPES_ALL_MISSING                              -1000
/**
A genotype value was greater than the maximum allowed (64) or less
than TSK_MISSING_DATA (-1).
*/
#define TSK_ERR_BAD_GENOTYPE                                       -1001
/**
A ancestral genotype value was greater than the maximum allowed (64) or less
than 0.
*/
#define TSK_ERR_BAD_ANCESTRAL_STATE                                -1002
/** @} */

/**
@defgroup GENOTYPE_ERROR_GROUP Genotype decoding errors.
@{
*/
/**
Genotypes were requested for non-samples at the same time
as asking that isolated nodes be marked as missing. This is not
supported.
*/
#define TSK_ERR_MUST_IMPUTE_NON_SAMPLES                            -1100
/**
A user-specified allele map was used, but didn't contain an allele
found in the tree sequence.
*/
#define TSK_ERR_ALLELE_NOT_FOUND                                   -1101
/**
More than 2147483647 alleles were specified.
*/
#define TSK_ERR_TOO_MANY_ALLELES                                   -1102
/**
A user-specified allele map was used, but it contained zero alleles.
*/
#define TSK_ERR_ZERO_ALLELES                                       -1103
/**
An allele used when decoding alignments had length other than one.
*/
#define TSK_ERR_BAD_ALLELE_LENGTH                                  -1104
/**
An allele used when decoding alignments matched the missing data character.
*/
#define TSK_ERR_MISSING_CHAR_COLLISION                             -1105
/** @} */

/**
@defgroup DISTANCE_ERROR_GROUP Distance metric errors.
@{
*/
/**
Trees with different numbers of samples were specified.
*/
#define TSK_ERR_SAMPLE_SIZE_MISMATCH                               -1200
/**
Trees with nonidentical samples were specified.
*/
#define TSK_ERR_SAMPLES_NOT_EQUAL                                  -1201
/**
A tree with multiple roots was specified.
*/
#define TSK_ERR_MULTIPLE_ROOTS                                     -1202
/**
A tree with unary nodes was specified.
*/
#define TSK_ERR_UNARY_NODES                                        -1203
/**
Trees were specifed that had unequal sequence lengths.
*/
#define TSK_ERR_SEQUENCE_LENGTH_MISMATCH                           -1204
/**
A tree was specifed that did not have the sample lists option
enabled (TSK_SAMPLE_LISTS).
*/
#define TSK_ERR_NO_SAMPLE_LISTS                                    -1205
/** @} */

/**
@defgroup HAPLOTYPE_ERROR_GROUP Haplotype matching errors.
@{
*/
/**
The Viterbi matrix has not filled (it has zero transitions).
*/
#define TSK_ERR_NULL_VITERBI_MATRIX                                -1300
/**
There was no matching haplotype.
*/
#define TSK_ERR_MATCH_IMPOSSIBLE                                   -1301
/**
The compressed matrix has a node that has no samples in it's descendants.
*/
#define TSK_ERR_BAD_COMPRESSED_MATRIX_NODE                         -1302
/**
There are too many values to compress.
*/
#define TSK_ERR_TOO_MANY_VALUES                                    -1303
/** @} */

/**
@defgroup UNION_ERROR_GROUP Union errors.
@{
*/
/**
A node map was specified that contained a node not present in the
specified table collection.
*/
#define TSK_ERR_UNION_BAD_MAP                                      -1400
/**
The shared portions of the specified tree sequences are not equal.
Note that this may be the case if the table collections were not
fully sorted before union was called.
*/
#define TSK_ERR_UNION_DIFF_HISTORIES                               -1401
/** @} */

/**
@defgroup IBD_ERROR_GROUP IBD errors.
@{
*/
/**
Both nodes in a sample pair are the same node.
*/
#define TSK_ERR_SAME_NODES_IN_PAIR                                 -1500
/**
Per-pair statistics were requested without TSK_IBD_STORE_PAIRS being
specified.
*/
#define TSK_ERR_IBD_PAIRS_NOT_STORED                               -1501
/**
Segments were requested without TSK_IBD_STORE_SEGMENTS being specified.
*/
#define TSK_ERR_IBD_SEGMENTS_NOT_STORED                            -1502
/** @} */

/**
@defgroup SIMPLIFY_ERROR_GROUP Simplify errors.
@{
*/
/**
Both TSK_SIMPLIFY_KEEP_UNARY and TSK_SIMPLIFY_KEEP_UNARY_IN_INDIVIDUALS
were specified. Only one can be used.
*/
#define TSK_ERR_KEEP_UNARY_MUTUALLY_EXCLUSIVE                      -1600
/** @} */

/**
@defgroup INDIVIDUAL_ERROR_GROUP Individual errors.
@{
*/
/**
Individuals were provided in an order where parents were after their
children.
*/
#define TSK_ERR_UNSORTED_INDIVIDUALS                               -1700
/**
An individual was its own parent.
*/
#define TSK_ERR_INDIVIDUAL_SELF_PARENT                             -1701
/**
An individual was its own ancestor in a cycle of references.
*/
#define TSK_ERR_INDIVIDUAL_PARENT_CYCLE                            -1702
/**
An individual had nodes from more than one population
(and only one was requested).
*/
#define TSK_ERR_INDIVIDUAL_POPULATION_MISMATCH                     -1703
/**
An individual had nodes from more than one time
(and only one was requested).
*/
#define TSK_ERR_INDIVIDUAL_TIME_MISMATCH                           -1704
/** @} */

/**
@defgroup EXTEND_EDGES_ERROR_GROUP Extend edges errors.
@{
*/
/**
Maximum iteration number (max_iter) must be positive.
*/
#define TSK_ERR_EXTEND_EDGES_BAD_MAXITER                          -1800
/** @} */
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

#ifdef TSK_TRACE_ERRORS

static inline int
_tsk_trace_error(int err, int line, const char *file)
{
    fprintf(stderr, "tskit-trace-error: %d='%s' at line %d in %s\n", err,
        tsk_strerror(err), line, file);
    return err;
}

#define tsk_trace_error(err) (_tsk_trace_error(err, __LINE__, __FILE__))
#else
#define tsk_trace_error(err) (err)
#endif

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
@brief Check if a number is ``TSK_UNKNOWN_TIME``

@rst
Unknown time values in tskit are represented by a particular NaN value. Since NaN values
are not equal to each other by definition, a simple comparison like
``mutation.time == TSK_UNKNOWN_TIME`` will fail even if the mutation's time is
TSK_UNKNOWN_TIME. This function compares the underlying bit representation of a double
value and returns true iff it is equal to the specific NaN value
:c:macro:`TSK_UNKNOWN_TIME`.
@endrst

@param val The number to check
@return true if the number is ``TSK_UNKNOWN_TIME`` else false
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

/* Bit Array functionality */

// define a 32-bit chunk size for our bitsets.
// this means we'll be able to hold 32 distinct items in each 32 bit uint
#define TSK_BITSET_BITS ((tsk_size_t) 32)
typedef uint32_t tsk_bitset_val_t;

typedef struct {
    tsk_size_t row_len; // Number of size TSK_BITSET_BITS chunks per row
    tsk_size_t len;     // Number of rows
    tsk_bitset_val_t *data;
} tsk_bitset_t;

int tsk_bitset_init(tsk_bitset_t *self, tsk_size_t num_bits, tsk_size_t length);
void tsk_bitset_free(tsk_bitset_t *self);
void tsk_bitset_intersect(const tsk_bitset_t *self, tsk_size_t self_row,
    const tsk_bitset_t *other, tsk_size_t other_row, tsk_bitset_t *out);
void tsk_bitset_subtract(tsk_bitset_t *self, tsk_size_t self_row,
    const tsk_bitset_t *other, tsk_size_t other_row);
void tsk_bitset_union(tsk_bitset_t *self, tsk_size_t self_row, const tsk_bitset_t *other,
    tsk_size_t other_row);
void tsk_bitset_set_bit(tsk_bitset_t *self, tsk_size_t row, const tsk_bitset_val_t bit);
bool tsk_bitset_contains(
    const tsk_bitset_t *self, tsk_size_t row, const tsk_bitset_val_t bit);
tsk_size_t tsk_bitset_count(const tsk_bitset_t *self, tsk_size_t row);
void tsk_bitset_get_items(
    const tsk_bitset_t *self, tsk_size_t row, tsk_id_t *items, tsk_size_t *n_items);

#ifdef __cplusplus
}
#endif

#endif
