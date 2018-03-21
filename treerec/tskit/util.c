/*
** Copyright (C) 2015-2018 University of Oxford
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

/* Basic utilities needed in all files */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <hdf5.h>

#include "util.h"

#define MSP_HDF5_ERR_MSG_SIZE 1024

static char _hdf5_error[MSP_HDF5_ERR_MSG_SIZE];

static herr_t
hdf5_error_walker(unsigned n, const H5E_error2_t *err_desc, void *client_data)
{
    /* We only copy the message from the first element in the error stack */
    if (_hdf5_error[0] == '\0') {
        snprintf(_hdf5_error, MSP_HDF5_ERR_MSG_SIZE,
                "HDF5 Error: %d: %d:'%s'",
                (int) err_desc->maj_num, (int) err_desc->min_num,
                err_desc->desc);
    }
    return 0;
}

const char *
msp_strerror(int err)
{
    const char *ret = "Unknown error";

    switch (err) {
        case 0:
            ret = "Normal exit condition. This is not an error!";
            goto out;
        case MSP_ERR_NO_MEMORY:
            ret = "Out of memory.";
            break;
        case MSP_ERR_GENERIC:
            ret = "Generic error; please file a bug report";
            break;
        case MSP_ERR_FILE_FORMAT:
            ret = "File format error";
            break;
        case MSP_ERR_BAD_STATE:
            ret = "Bad simulator state. Initialise or reset must be called.";
            break;
        case MSP_ERR_BUFFER_OVERFLOW:
            ret = "Supplied buffer if too small";
            break;
        case MSP_ERR_UNSORTED_DEMOGRAPHIC_EVENTS:
            ret = "Demographic events must be time sorted.";
            break;
        case MSP_ERR_POPULATION_OVERFLOW:
            ret = "Population Overflow occurred.";
            break;
        case MSP_ERR_LINKS_OVERFLOW:
            ret = "Links Overflow occurred.";
            break;
        case MSP_ERR_OUT_OF_BOUNDS:
            ret = "Array index out of bounds";
            break;
        case MSP_ERR_BAD_ORDERING:
            ret = "Bad record ordering requested";
            break;
        case MSP_ERR_BAD_MUTATION:
            ret = "Bad mutation provided";
            break;
        case MSP_ERR_BAD_PARAM_VALUE:
            ret = "Bad parameter value provided";
            break;
        case MSP_ERR_UNSUPPORTED_OPERATION:
            ret = "Operation cannot be performed in current configuration";
            break;
        case MSP_ERR_BAD_POPULATION_CONFIGURATION:
            ret = "Bad population configuration provided.";
            break;
        case MSP_ERR_BAD_POPULATION_SIZE:
            ret = "Bad population size provided. Must be > 0.";
            break;
        case MSP_ERR_BAD_POPULATION_ID:
            ret = "Bad population id provided.";
            break;
        case MSP_ERR_BAD_MIGRATION_MATRIX:
            ret = "Bad migration matrix provided.";
            break;
        case MSP_ERR_BAD_MIGRATION_MATRIX_INDEX:
            ret = "Bad migration matrix index provided.";
            break;
        case MSP_ERR_DIAGONAL_MIGRATION_MATRIX_INDEX:
            ret = "Cannot set diagonal migration matrix elements.";
            break;
        case MSP_ERR_INFINITE_WAITING_TIME:
            ret = "Infinite waiting time until next simulation event.";
            break;
        case MSP_ERR_ASSERTION_FAILED:
            ret = "Internal error; please file a bug report.";
            break;
        case MSP_ERR_SOURCE_DEST_EQUAL:
            ret = "Source and destination populations equal.";
            break;
        case MSP_ERR_BAD_RECOMBINATION_MAP:
            ret = "Bad recombination map provided.";
            break;
        case MSP_ERR_INSUFFICIENT_SAMPLES:
            ret = "At least two samples needed.";
            break;
        case MSP_ERR_ZERO_RECORDS:
            ret = "At least one record must be supplied";
            break;
        case MSP_ERR_EDGES_NOT_SORTED_PARENT_TIME:
            ret = "Edges must be listed in (time[parent], child, left) order;"
                " time[parent] order violated";
            break;
        case MSP_ERR_EDGES_NONCONTIGUOUS_PARENTS:
            ret = "All edges for a given parent must be contiguous";
            break;
        case MSP_ERR_EDGES_NOT_SORTED_CHILD:
            ret = "Edges must be listed in (time[parent], child, left) order;"
                " child order violated";
            break;
        case MSP_ERR_EDGES_NOT_SORTED_LEFT:
            ret = "Edges must be listed in (time[parent], child, left) order;"
                " left order violated";
            break;
        case MSP_ERR_NULL_PARENT:
            ret = "Edge in parent is null.";
            break;
        case MSP_ERR_NULL_CHILD:
            ret = "Edge in parent is null.";
            break;
        case MSP_ERR_BAD_NODE_TIME_ORDERING:
            ret = "time[parent] must be greater than time[child]";
            break;
        case MSP_ERR_BAD_EDGE_INTERVAL:
            ret = "Bad edge interval where right <= left";
            break;
        case MSP_ERR_DUPLICATE_EDGES:
            ret = "Duplicate edges provided.";
            break;
        case MSP_ERR_CANNOT_SIMPLIFY:
            ret = "Cannot simplify the tree sequence; no output records.";
            break;
        case MSP_ERR_BAD_SAMPLES:
            ret = "Bad sample configuration provided.";
            break;
        case MSP_ERR_FILE_VERSION_TOO_OLD:
            ret = "HDF5 file version too old. Please upgrade using the "
                "'msp upgrade' command";
            break;
        case MSP_ERR_FILE_VERSION_TOO_NEW:
            ret = "HDF5 file version is too new for this version of msprime. "
                "Please upgrade msprime to the latest version.";
            break;
        case MSP_ERR_DUPLICATE_SAMPLE:
            ret = "Duplicate value provided in tracked leaf list.";
            break;
        case MSP_ERR_REFCOUNT_NONZERO:
            ret = "Cannot change the state of the tree sequence when "
                "other objects reference it. Make sure all trees are freed first.";
            break;
        case MSP_ERR_BAD_MODEL:
            ret = "Model error. Either a bad model, or the requested operation "
                "is not supported for the current model";
            break;
        case MSP_ERR_NOT_INITIALISED:
            ret = "object not initialised. Please file a bug report.";
            break;
        case MSP_ERR_DUPLICATE_MUTATION_NODES:
            ret = "Cannot have more than one mutation at a node for a given site.";
            break;
        case MSP_ERR_NONBINARY_MUTATIONS_UNSUPPORTED:
            ret = "Only binary mutations are supported for this operation.";
            break;
        case MSP_ERR_INCONSISTENT_MUTATIONS:
            ret = "Inconsistent mutations: state already equal to derived state.";
            break;
        case MSP_ERR_COORDINATE_NOT_FOUND:
            ret = "Coordinate not found.";
            break;
        case MSP_ERR_BAD_NODES_ARRAY:
            ret = "Malformed nodes array.";
            break;
        case MSP_ERR_BAD_CHILDREN_ARRAY:
            ret = "Malformed array of children.";
            break;
        case MSP_ERR_SITE_OUT_OF_BOUNDS:
            ret = "Site out of bounds";
            break;
        case MSP_ERR_NODE_OUT_OF_BOUNDS:
            ret = "Node out of bounds";
            break;
        case MSP_ERR_LENGTH_MISMATCH:
            ret = "Mismatch in stored total column length and sum of row lengths";
            break;
        case MSP_ERR_NON_SINGLE_CHAR_MUTATION:
            ret = "Only single char mutations supported.";
            break;
        case MSP_ERR_UNSORTED_SITES:
            ret = "Sites must be provided in strictly increasing position order.";
            break;
        case MSP_ERR_BAD_SITE_POSITION:
            ret = "Sites positions must be between 0 and sequence_length";
            break;
        case MSP_ERR_UNSORTED_MUTATIONS:
            ret = "Mutations must be provided in non-decreasing site order";
            break;
        case MSP_ERR_EDGESETS_FOR_PARENT_NOT_ADJACENT:
            ret = "All edges for a given parent must be adjacent.";
            break;
        case MSP_ERR_BAD_EDGESET_CONTRADICTORY_CHILDREN:
            ret = "Bad edges: contradictory children for a given parent over "
                "an interval.";
            break;
        case MSP_ERR_BAD_EDGESET_OVERLAPPING_PARENT:
            ret = "Bad edges: multiple definitions of a given parent over an interval";
            break;
        case MSP_ERR_MULTIROOT_NEWICK:
            ret = "Newick output not supported for trees with > 1 roots.";
            break;
        case MSP_ERR_BAD_SEQUENCE_LENGTH:
            ret = "Sequence length must be > 0.";
            break;
        case MSP_ERR_RIGHT_GREATER_SEQ_LENGTH:
            ret = "Right coordinate > sequence length.";
            break;
        case MSP_ERR_MUTATION_OUT_OF_BOUNDS:
            ret = "mutation ID out of bounds";
            break;
        case MSP_ERR_MUTATION_PARENT_DIFFERENT_SITE:
            ret = "Specified parent mutation is at a different site.";
            break;
        case MSP_ERR_MUTATION_PARENT_EQUAL:
            ret = "Parent mutation refers to itself.";
            break;
        case MSP_ERR_MUTATION_PARENT_AFTER_CHILD:
            ret = "Parent mutation ID must be < current ID.";
            break;
        case MSP_ERR_BAD_OFFSET:
            ret = "Bad offset provided in input array.";
            break;
        case MSP_ERR_TOO_MANY_ALLELES:
            ret = "Cannot have more than 255 alleles.";
            break;
        case MSP_ERR_IO:
            if (errno != 0) {
                ret = strerror(errno);
            } else {
                ret = "Unspecified IO error";
            }
            break;
/*
        case MSP_ERR_HDF5:
            _hdf5_error[0] = '\0';
            if (H5Ewalk2(H5E_DEFAULT, H5E_WALK_UPWARD, hdf5_error_walker, NULL)
                    != 0) {
                ret = "Eek! Error handling HDF5 error.";
                goto out;
            }
            ret = _hdf5_error;
            break;
*/
        default:
            ret = "Error occurred generating error string. Please file a bug "
                "report!";
            break;
    }
out:
    return ret;
}

void
__msp_safe_free(void **ptr) {
    if (ptr != NULL) {
        if (*ptr != NULL) {
            free(*ptr);
            *ptr = NULL;
        }
    }
}
