/*
** Copyright (C) 2015-2017 University of Oxford
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
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "trees.h"

/* This infrastructure is left-over from an earlier more complex version
 * of this algorithm that worked over a tree sequence and cached the newick
 * subtrees, updating according to diffs. It's unclear whether this complexity
 * was of any real-world use, since newick output for large trees is pretty
 * pointless. */

int
newick_converter_run(newick_converter_t *self, node_id_t root,
        size_t buffer_size, char *buffer)
{
    int ret = MSP_ERR_GENERIC;
    sparse_tree_t *tree = self->tree;
    node_id_t *stack = self->tree->stack1;
    const double *time = self->tree->tree_sequence->tables->nodes->time;
    int stack_top = 0;
    int label;
    size_t s = 0;
    int r;
    node_id_t u, v, w, root_parent;
    double branch_length;

    if (root < 0 || root >= (node_id_t) self->tree->num_nodes) {
        ret = MSP_ERR_NODE_OUT_OF_BOUNDS;
        goto out;
    }
    if (buffer == NULL) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    root_parent = tree->parent[root];
    stack[0] = root;
    u = root_parent;
    while (stack_top >= 0) {
        v = stack[stack_top];
        if (tree->left_child[v] != MSP_NULL_NODE && v != u) {
            if (s >= buffer_size) {
                ret = MSP_ERR_BUFFER_OVERFLOW;
                goto out;
            }
            buffer[s] = '(';
            s++;
            for (w = tree->right_child[v]; w != MSP_NULL_NODE; w = tree->left_sib[w]) {
                stack_top++;
                stack[stack_top] = w;
            }
        } else {
            u = tree->parent[v];
            stack_top--;
            if (tree->left_child[v] == MSP_NULL_NODE) {
                if (s >= buffer_size) {
                    ret = MSP_ERR_BUFFER_OVERFLOW;
                    goto out;
                }
                /* We do this for ms-compatability. This should be a configurable option
                 * via the flags attribute */
                label = v + 1;
                r = snprintf(buffer + s, buffer_size - s, "%d", label);
                if (r < 0) {
                    ret = MSP_ERR_IO;
                    goto out;
                }
                s += (size_t) r;
                if (s >= buffer_size) {
                    ret = MSP_ERR_BUFFER_OVERFLOW;
                    goto out;
                }
            }
            if (u != root_parent) {
                branch_length = (time[u] - time[v]);
                r = snprintf(buffer + s, buffer_size - s, ":%.*f", (int) self->precision,
                        branch_length);
                if (r < 0) {
                    ret = MSP_ERR_IO;
                    goto out;
                }
                s += (size_t) r;
                if (s >= buffer_size) {
                    ret = MSP_ERR_BUFFER_OVERFLOW;
                    goto out;
                }
                if (v == tree->right_child[u]) {
                    buffer[s] = ')';
                } else {
                    buffer[s] = ',';
                }
                s++;
            }
        }
    }
    if ((s + 1) >= buffer_size) {
        ret = MSP_ERR_BUFFER_OVERFLOW;
        goto out;
    }
    buffer[s] = ';';
    buffer[s + 1] = '\0';
    ret = 0;
out:
    return ret;
}

int
newick_converter_alloc(newick_converter_t *self, sparse_tree_t *tree,
        size_t precision, int flags)
{
    int ret = 0;

    memset(self, 0, sizeof(newick_converter_t));
    self->precision = precision;
    self->flags = flags;
    self->tree = tree;
    return ret;
}

int
newick_converter_free(newick_converter_t *MSP_UNUSED(self))
{
    return 0;
}
