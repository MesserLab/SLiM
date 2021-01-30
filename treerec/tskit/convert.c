/*
 * MIT License
 *
 * Copyright (c) 2019 Tskit Developers
 * Copyright (c) 2015-2017 University of Oxford
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
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

#include <tskit/convert.h>

/* ======================================================== *
 * Newick output.
 * ======================================================== */

/* This infrastructure is left-over from an earlier more complex version
 * of this algorithm that worked over a tree sequence and cached the newick
 * subtrees, updating according to diffs. It's unclear whether this complexity
 * was of any real-world use, since newick output for large trees is pretty
 * pointless. */

typedef struct {
    size_t precision;
    tsk_flags_t options;
    char *newick;
    tsk_id_t *traversal_stack;
    const tsk_tree_t *tree;
} tsk_newick_converter_t;

static int
tsk_newick_converter_run(
    tsk_newick_converter_t *self, tsk_id_t root, size_t buffer_size, char *buffer)
{
    int ret = TSK_ERR_GENERIC;
    const tsk_tree_t *tree = self->tree;
    tsk_id_t *stack = self->traversal_stack;
    const double *time = self->tree->tree_sequence->tables->nodes.time;
    int stack_top = 0;
    int label;
    size_t s = 0;
    int r;
    tsk_id_t u, v, w, root_parent;
    double branch_length;

    if (root < 0 || root >= (tsk_id_t) self->tree->num_nodes) {
        ret = TSK_ERR_NODE_OUT_OF_BOUNDS;
        goto out;
    }
    if (buffer == NULL) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    root_parent = tree->parent[root];
    stack[0] = root;
    u = root_parent;
    while (stack_top >= 0) {
        v = stack[stack_top];
        if (tree->left_child[v] != TSK_NULL && v != u) {
            if (s >= buffer_size) {
                ret = TSK_ERR_BUFFER_OVERFLOW;
                goto out;
            }
            buffer[s] = '(';
            s++;
            for (w = tree->right_child[v]; w != TSK_NULL; w = tree->left_sib[w]) {
                stack_top++;
                stack[stack_top] = w;
            }
        } else {
            u = tree->parent[v];
            stack_top--;
            if (tree->left_child[v] == TSK_NULL) {
                if (s >= buffer_size) {
                    ret = TSK_ERR_BUFFER_OVERFLOW;
                    goto out;
                }
                /* We do this for ms-compatability. This should be a configurable option
                 * via the flags attribute */
                label = v + 1;
                r = snprintf(buffer + s, buffer_size - s, "%d", label);
                if (r < 0) {
                    ret = TSK_ERR_IO;
                    goto out;
                }
                s += (size_t) r;
                if (s >= buffer_size) {
                    ret = TSK_ERR_BUFFER_OVERFLOW;
                    goto out;
                }
            }
            if (u != root_parent) {
                branch_length = (time[u] - time[v]);
                r = snprintf(buffer + s, buffer_size - s, ":%.*f", (int) self->precision,
                    branch_length);
                if (r < 0) {
                    ret = TSK_ERR_IO;
                    goto out;
                }
                s += (size_t) r;
                if (s >= buffer_size) {
                    ret = TSK_ERR_BUFFER_OVERFLOW;
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
        ret = TSK_ERR_BUFFER_OVERFLOW;
        goto out;
    }
    buffer[s] = ';';
    buffer[s + 1] = '\0';
    ret = 0;
out:
    return ret;
}

static int
tsk_newick_converter_init(tsk_newick_converter_t *self, const tsk_tree_t *tree,
    size_t precision, tsk_flags_t options)
{
    int ret = 0;

    memset(self, 0, sizeof(tsk_newick_converter_t));
    self->precision = precision;
    self->options = options;
    self->tree = tree;
    self->traversal_stack = malloc(tsk_treeseq_get_num_nodes(self->tree->tree_sequence)
                                   * sizeof(*self->traversal_stack));
    if (self->traversal_stack == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
out:
    return ret;
}

static int
tsk_newick_converter_free(tsk_newick_converter_t *self)
{
    tsk_safe_free(self->traversal_stack);
    return 0;
}

int
tsk_convert_newick(const tsk_tree_t *tree, tsk_id_t root, size_t precision,
    tsk_flags_t options, size_t buffer_size, char *buffer)
{
    int ret = 0;
    tsk_newick_converter_t nc;

    ret = tsk_newick_converter_init(&nc, tree, precision, options);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_newick_converter_run(&nc, root, buffer_size, buffer);
out:
    tsk_newick_converter_free(&nc);
    return ret;
}
