/*
** Copyright (C) 2015-2016 University of Oxford
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
#ifndef __UTIL_H__
#define __UTIL_H__

/*
 * raise a compiler warning if a potentially error raising function's return
 * value is not used.
 */
#ifdef __GNUC__
    #define WARN_UNUSED __attribute__ ((warn_unused_result))
#else
    #define WARN_UNUSED
#endif

#define MSP_NODE_IS_SAMPLE 1

/* The root node indicator */
#define MSP_NULL_NODE (-1)
/* Indicates the that the population ID has not been set. */
#define MSP_NULL_POPULATION_ID (-1)
/* There is no parent for a given mutation */
#define MSP_NULL_MUTATION (-1)

/* Flags for simplify() */
#define MSP_FILTER_ZERO_MUTATION_SITES 1

/* Flags for dump tables */
#define MSP_ALLOC_TABLES 1

/* Error codes */
#define MSP_ERR_GENERIC                                             -1
#define MSP_ERR_NO_MEMORY                                           -2
#define MSP_ERR_IO                                                  -3
#define MSP_ERR_FILE_FORMAT                                         -4
#define MSP_ERR_REFCOUNT_NONZERO                                    -5
#define MSP_ERR_BAD_STATE                                           -6
#define MSP_ERR_BAD_PARAM_VALUE                                     -7
#define MSP_ERR_OUT_OF_BOUNDS                                       -8
#define MSP_ERR_BUFFER_OVERFLOW                                     -9
#define MSP_ERR_UNSORTED_DEMOGRAPHIC_EVENTS                         -10
#define MSP_ERR_POPULATION_OVERFLOW                                 -11
#define MSP_ERR_LINKS_OVERFLOW                                      -12
#define MSP_ERR_HDF5                                                -13
#define MSP_ERR_BAD_POPULATION_ID                                   -14
#define MSP_ERR_DUPLICATE_SAMPLE                                    -15
#define MSP_ERR_BAD_ORDERING                                        -16
#define MSP_ERR_BAD_MUTATION                                        -17
#define MSP_ERR_UNSUPPORTED_OPERATION                               -18
#define MSP_ERR_BAD_POPULATION_CONFIGURATION                        -19
#define MSP_ERR_BAD_MIGRATION_MATRIX                                -20
#define MSP_ERR_BAD_MIGRATION_MATRIX_INDEX                          -21
#define MSP_ERR_DIAGONAL_MIGRATION_MATRIX_INDEX                     -22
#define MSP_ERR_INFINITE_WAITING_TIME                               -23
#define MSP_ERR_ASSERTION_FAILED                                    -24
#define MSP_ERR_SOURCE_DEST_EQUAL                                   -25
#define MSP_ERR_BAD_RECOMBINATION_MAP                               -26
#define MSP_ERR_BAD_POPULATION_SIZE                                 -27
#define MSP_ERR_BAD_SAMPLES                                         -28
#define MSP_ERR_MULTIROOT_NEWICK                                    -29
#define MSP_ERR_FILE_VERSION_TOO_OLD                                -30
#define MSP_ERR_FILE_VERSION_TOO_NEW                                -31
#define MSP_ERR_CANNOT_SIMPLIFY                                     -32
#define MSP_ERR_BAD_MODEL                                           -33
#define MSP_ERR_NULL_PARENT                                         -34
#define MSP_ERR_NULL_CHILD                                          -35
#define MSP_ERR_EDGES_NOT_SORTED_PARENT_TIME                        -36
#define MSP_ERR_EDGES_NONCONTIGUOUS_PARENTS                         -37
#define MSP_ERR_EDGES_NOT_SORTED_CHILD                              -38
#define MSP_ERR_EDGES_NOT_SORTED_LEFT                               -39
#define MSP_ERR_BAD_NODE_TIME_ORDERING                              -40
#define MSP_ERR_BAD_EDGE_INTERVAL                                   -41
#define MSP_ERR_DUPLICATE_EDGES                                     -42
#define MSP_ERR_NOT_INITIALISED                                     -43
#define MSP_ERR_BAD_OFFSET                                          -44
#define MSP_ERR_TOO_MANY_ALLELES                                    -45
#define MSP_ERR_DUPLICATE_MUTATION_NODES                            -46
#define MSP_ERR_NONBINARY_MUTATIONS_UNSUPPORTED                     -47
#define MSP_ERR_INCONSISTENT_MUTATIONS                              -48
#define MSP_ERR_INSUFFICIENT_SAMPLES                                -49
#define MSP_ERR_ZERO_RECORDS                                        -50
#define MSP_ERR_COORDINATE_NOT_FOUND                                -51
#define MSP_ERR_BAD_NODES_ARRAY                                     -52
#define MSP_ERR_BAD_CHILDREN_ARRAY                                  -53
#define MSP_ERR_SITE_OUT_OF_BOUNDS                                  -54
#define MSP_ERR_NODE_OUT_OF_BOUNDS                                  -55
#define MSP_ERR_LENGTH_MISMATCH                                     -56

#define MSP_ERR_NON_SINGLE_CHAR_MUTATION                            -58
#define MSP_ERR_UNSORTED_SITES                                      -59
#define MSP_ERR_BAD_SITE_POSITION                                   -60
#define MSP_ERR_UNSORTED_MUTATIONS                                  -61
#define MSP_ERR_UNDEFINED_MULTIPLE_MERGER_COALESCENT                -62
#define MSP_ERR_EDGESETS_FOR_PARENT_NOT_ADJACENT                    -63
#define MSP_ERR_BAD_EDGESET_CONTRADICTORY_CHILDREN                  -64
#define MSP_ERR_BAD_EDGESET_OVERLAPPING_PARENT                      -65
#define MSP_ERR_BAD_SEQUENCE_LENGTH                                 -66
#define MSP_ERR_RIGHT_GREATER_SEQ_LENGTH                            -67
#define MSP_ERR_MUTATION_OUT_OF_BOUNDS                              -68
#define MSP_ERR_MUTATION_PARENT_DIFFERENT_SITE                      -69
#define MSP_ERR_MUTATION_PARENT_EQUAL                               -70
#define MSP_ERR_MUTATION_PARENT_AFTER_CHILD                         -71

const char * msp_strerror(int err);
void __msp_safe_free(void **ptr);
#define msp_safe_free(pointer) __msp_safe_free((void **) &(pointer))

#endif /*__UTIL_H__*/
