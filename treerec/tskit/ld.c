/*
** Copyright (C) 2016-2017 University of Oxford
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

#include <gsl/gsl_math.h>

#include "util.h"
#include "object_heap.h"
#include "msprime.h"

static void
ld_calc_check_state(ld_calc_t *self)
{
    uint32_t u;
    uint32_t num_nodes = (uint32_t) tree_sequence_get_num_nodes(self->tree_sequence);
    sparse_tree_t *tA = self->outer_tree;
    sparse_tree_t *tB = self->inner_tree;

    assert(tA->index == tB->index);

    /* The inner tree's mark values should all be zero. */
    for (u = 0; u < num_nodes; u++) {
        assert(tA->marked[u] == 0);
        assert(tB->marked[u] == 0);
    }
}

void
ld_calc_print_state(ld_calc_t *self, FILE *out)
{
    fprintf(out, "tree_sequence = %p\n", (void *) self->tree_sequence);
    fprintf(out, "outer tree index = %d\n", (int) self->outer_tree->index);
    fprintf(out, "outer tree interval = (%f, %f)\n",
            self->outer_tree->left, self->outer_tree->right);
    fprintf(out, "inner tree index = %d\n", (int) self->inner_tree->index);
    fprintf(out, "inner tree interval = (%f, %f)\n",
            self->inner_tree->left, self->inner_tree->right);
    ld_calc_check_state(self);
}

int WARN_UNUSED
ld_calc_alloc(ld_calc_t *self, tree_sequence_t *tree_sequence)
{
    int ret = MSP_ERR_GENERIC;

    memset(self, 0, sizeof(ld_calc_t));
    self->tree_sequence = tree_sequence;
    self->num_sites = tree_sequence_get_num_sites(tree_sequence);
    self->outer_tree = malloc(sizeof(sparse_tree_t));
    self->inner_tree = malloc(sizeof(sparse_tree_t));
    if (self->outer_tree == NULL || self->inner_tree == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    ret = sparse_tree_alloc(self->outer_tree, self->tree_sequence,
            MSP_SAMPLE_COUNTS|MSP_SAMPLE_LISTS);
    if (ret != 0) {
        goto out;
    }
    ret = sparse_tree_alloc(self->inner_tree, self->tree_sequence,
            MSP_SAMPLE_COUNTS);
    if (ret != 0) {
        goto out;
    }
    ret = sparse_tree_first(self->outer_tree);
    if (ret < 0) {
        goto out;
    }
    ret = sparse_tree_first(self->inner_tree);
    if (ret < 0) {
        goto out;
    }
    ret = 0;
out:
    return ret;
}

int
ld_calc_free(ld_calc_t *self)
{
    if (self->inner_tree != NULL) {
        sparse_tree_free(self->inner_tree);
        free(self->inner_tree);
    }
    if (self->outer_tree != NULL) {
        sparse_tree_free(self->outer_tree);
        free(self->outer_tree);
    }
    return 0;
}

/* Position the two trees so that the specified site is within their
 * interval.
 */
static int WARN_UNUSED
ld_calc_position_trees(ld_calc_t *self, size_t site_index)
{
    int ret = MSP_ERR_GENERIC;
    site_t mut;
    double x;
    sparse_tree_t *tA = self->outer_tree;
    sparse_tree_t *tB = self->inner_tree;

    ret = tree_sequence_get_site(self->tree_sequence, (site_id_t) site_index,
            &mut);
    if (ret != 0) {
        goto out;
    }
    x = mut.position;
    assert(tA->index == tB->index);
    while (x >= tA->right) {
        ret = sparse_tree_next(tA);
        if (ret < 0) {
            goto out;
        }
        assert(ret == 1);
        ret = sparse_tree_next(tB);
        if (ret < 0) {
            goto out;
        }
        assert(ret == 1);
    }
    while (x < tA->left) {
        ret = sparse_tree_prev(tA);
        if (ret < 0) {
            goto out;
        }
        assert(ret == 1);
        ret = sparse_tree_prev(tB);
        if (ret < 0) {
            goto out;
        }
        assert(ret == 1);
    }
    ret = 0;
    assert(x >= tA->left && x < tB->right);
    assert(tA->index == tB->index);
out:
    return ret;
}

static double
ld_calc_overlap_within_tree(ld_calc_t *self, site_t sA, site_t sB)
{
    sparse_tree_t *t = self->inner_tree;
    tree_sequence_t *s = self->tree_sequence;
    node_id_t u, v, nAB;

    assert(sA.mutations_length == 1);
    assert(sB.mutations_length == 1);
    u = sA.mutations[0].node;
    v = sB.mutations[0].node;
    if (s->nodes.time[u] > s->nodes.time[v]) {
        v = sA.mutations[0].node;
        u = sB.mutations[0].node;
    }
    while (u != v && u != MSP_NULL_NODE) {
        u = t->parent[u];
    }
    nAB = 0;
    if (u == v) {
        nAB = GSL_MIN(t->num_samples[sA.mutations[0].node], t->num_samples[sB.mutations[0].node]);
    }
    return (double) nAB;
}

static inline int WARN_UNUSED
ld_calc_set_tracked_samples(ld_calc_t *self, site_t sA)
{
    int ret = 0;
    node_list_t *head, *tail;

    assert(sA.mutations_length == 1);
    ret = sparse_tree_get_sample_list(self->outer_tree, sA.mutations[0].node, &head, &tail);
    if (ret != 0) {
        goto out;
    }
    ret = sparse_tree_set_tracked_samples_from_sample_list(self->inner_tree,
            head, tail);
out:
    return ret;
}

static int WARN_UNUSED
ld_calc_get_r2_array_forward(ld_calc_t *self, size_t source_index,
        size_t max_sites, double max_distance, double *r2,
        size_t *num_r2_values)
{
    int ret = MSP_ERR_GENERIC;
    site_t sA, sB;
    double fA, fB, fAB, D;
    int tracked_samples_set = 0;
    sparse_tree_t *tA, *tB;
    double n = (double) tree_sequence_get_num_samples(self->tree_sequence);
    size_t j;
    double nAB;

    tA = self->outer_tree;
    tB = self->inner_tree;
    ret = tree_sequence_get_site(self->tree_sequence, (site_id_t) source_index, &sA);
    if (ret != 0) {
        goto out;
    }
    if (sA.mutations_length > 1) {
        ret = MSP_ERR_UNSUPPORTED_OPERATION;
        goto out;
    }
    fA = ((double) tA->num_samples[sA.mutations[0].node]) / n;
    assert(fA > 0);
    tB->mark = 1;
    for (j = 0; j < max_sites; j++) {
        if (source_index + j + 1 >= self->num_sites) {
            break;
        }
        ret = tree_sequence_get_site(self->tree_sequence,
                (site_id_t) (source_index + j + 1), &sB);
        if (ret != 0) {
            goto out;
        }
        if (sB.mutations_length > 1) {
            ret = MSP_ERR_UNSUPPORTED_OPERATION;
            goto out;
        }
        if (sB.position - sA.position > max_distance) {
            break;
        }
        while (sB.position >= tB->right) {
            ret = sparse_tree_next(tB);
            if (ret < 0) {
                goto out;
            }
            assert(ret == 1);
        }
        fB = ((double) tB->num_samples[sB.mutations[0].node]) / n;
        assert(fB > 0);
        if (sB.position < tA->right) {
            nAB = ld_calc_overlap_within_tree(self, sA, sB);
        } else {
            if (!tracked_samples_set && tB->marked[sA.mutations[0].node] == 1) {
                tracked_samples_set = 1;
                ret = ld_calc_set_tracked_samples(self, sA);
                if (ret != 0) {
                    goto out;
                }
            }
            if (tracked_samples_set) {
                nAB = (double)tB->num_tracked_samples[sB.mutations[0].node];
            } else {
                nAB = ld_calc_overlap_within_tree(self, sA, sB);
            }
        }
        fAB = nAB / n;
        D = fAB - fA * fB;
        r2[j] = D * D / (fA * fB * (1 - fA) * (1 - fB));
    }

    /* Now rewind back the inner iterator and unmark all nodes that
     * were set to 1 as we moved forward. */
    tB->mark = 0;
    while (tB->index > tA->index) {
        ret = sparse_tree_prev(tB);
        if (ret < 0) {
            goto out;
        }
        assert(ret == 1);
    }
    *num_r2_values = j;
    ret = 0;
out:
    return ret;
}

static int WARN_UNUSED
ld_calc_get_r2_array_reverse(ld_calc_t *self, size_t source_index,
        size_t max_sites, double max_distance, double *r2,
        size_t *num_r2_values)
{
    int ret = MSP_ERR_GENERIC;
    site_t sA, sB;
    double fA, fB, fAB, D;
    int tracked_samples_set = 0;
    sparse_tree_t *tA, *tB;
    double n = (double) tree_sequence_get_num_samples(self->tree_sequence);
    size_t j;
    double nAB;
    int64_t site_index;

    tA = self->outer_tree;
    tB = self->inner_tree;
    ret = tree_sequence_get_site(self->tree_sequence, (site_id_t) source_index, &sA);
    if (ret != 0) {
        goto out;
    }
    if (sA.mutations_length > 1) {
        ret = MSP_ERR_UNSUPPORTED_OPERATION;
        goto out;
    }
    fA = ((double) tA->num_samples[sA.mutations[0].node]) / n;
    assert(fA > 0);
    tB->mark = 1;
    for (j = 0; j < max_sites; j++) {
        site_index = ((int64_t) source_index) - ((int64_t) j) - 1;
        if (site_index < 0) {
            break;
        }
        ret = tree_sequence_get_site(self->tree_sequence, (site_id_t) site_index, &sB);
        if (ret != 0) {
            goto out;
        }
        if (sB.mutations_length > 1) {
            ret = MSP_ERR_UNSUPPORTED_OPERATION;
            goto out;
        }
        if (sA.position - sB.position > max_distance) {
            break;
        }
        while (sB.position < tB->left) {
            ret = sparse_tree_prev(tB);
            if (ret < 0) {
                goto out;
            }
            assert(ret == 1);
        }
        fB = ((double) tB->num_samples[sB.mutations[0].node]) / n;
        assert(fB > 0);
        if (sB.position >= tA->left) {
            nAB = ld_calc_overlap_within_tree(self, sA, sB);
        } else {
            if (!tracked_samples_set && tB->marked[sA.mutations[0].node] == 1) {
                tracked_samples_set = 1;
                ret = ld_calc_set_tracked_samples(self, sA);
                if (ret != 0) {
                    goto out;
                }
            }
            if (tracked_samples_set) {
                nAB = (double) tB->num_tracked_samples[sB.mutations[0].node];
            } else {
                nAB = ld_calc_overlap_within_tree(self, sA, sB);
            }
        }
        fAB = nAB / n;
        D = fAB - fA * fB;
        r2[j] = D * D / (fA * fB * (1 - fA) * (1 - fB));
    }

    /* Now fast forward the inner iterator and unmark all nodes that
     * were set to 1 as we moved back. */
    tB->mark = 0;
    while (tB->index < tA->index) {
        ret = sparse_tree_next(tB);
        if (ret < 0) {
            goto out;
        }
        assert(ret == 1);
    }
    *num_r2_values = j;
    ret = 0;
out:
    return ret;
}

int WARN_UNUSED
ld_calc_get_r2_array(ld_calc_t *self, size_t a, int direction,
        size_t max_sites, double max_distance, double *r2,
        size_t *num_r2_values)
{
    int ret = MSP_ERR_GENERIC;

    if (a >= self->num_sites) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    ret = ld_calc_position_trees(self, a);
    if (ret != 0) {
        goto out;
    }
    if (direction == MSP_DIR_FORWARD) {
        ret = ld_calc_get_r2_array_forward(self, a, max_sites, max_distance,
                r2, num_r2_values);
    } else if (direction == MSP_DIR_REVERSE) {
        ret = ld_calc_get_r2_array_reverse(self, a, max_sites, max_distance,
                r2, num_r2_values);
    } else {
        ret = MSP_ERR_BAD_PARAM_VALUE;
    }
out:
    return ret;
}

int WARN_UNUSED
ld_calc_get_r2(ld_calc_t *self, size_t a, size_t b, double *r2)
{
    int ret = MSP_ERR_GENERIC;
    site_t sA, sB;
    double fA, fB, fAB, D;
    sparse_tree_t *tA, *tB;
    double n = (double) tree_sequence_get_num_samples(self->tree_sequence);
    double nAB;
    size_t tmp;

    if (a >= self->num_sites || b >= self->num_sites) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    if (a > b) {
        tmp = a;
        a = b;
        b = tmp;
    }
    ret = ld_calc_position_trees(self, a);
    if (ret != 0) {
        goto out;
    }
    /* We can probably do a lot better than this implementation... */
    tA = self->outer_tree;
    tB = self->inner_tree;
    ret = tree_sequence_get_site(self->tree_sequence, (site_id_t) a, &sA);
    if (ret != 0) {
        goto out;
    }
    ret = tree_sequence_get_site(self->tree_sequence, (site_id_t) b, &sB);
    if (ret != 0) {
        goto out;
    }
    if (sA.mutations_length > 1 || sB.mutations_length > 1) {
        ret = MSP_ERR_UNSUPPORTED_OPERATION;
        goto out;
    }
    assert(sA.mutations_length == 1);
    /* assert(tA->parent[sA.mutations[0].node] != MSP_NULL_NODE); */
    fA = ((double) tA->num_samples[sA.mutations[0].node]) / n;
    assert(fA > 0);
    ret = ld_calc_set_tracked_samples(self, sA);
    if (ret != 0) {
        goto out;
    }

    while (sB.position >= tB->right) {
        ret = sparse_tree_next(tB);
        if (ret < 0) {
            goto out;
        }
        assert(ret == 1);
    }
    /* assert(tB->parent[sB.mutations[0].node] != MSP_NULL_NODE); */
    fB = ((double) tB->num_samples[sB.mutations[0].node]) / n;
    assert(fB > 0);
    nAB = (double) tB->num_tracked_samples[sB.mutations[0].node];
    fAB = nAB / n;
    D = fAB - fA * fB;
    *r2 = D * D / (fA * fB * (1 - fA) * (1 - fB));

    /* Now rewind the inner iterator back. */
    while (tB->index > tA->index) {
        ret = sparse_tree_prev(tB);
        if (ret < 0) {
            goto out;
        }
        assert(ret == 1);
    }
    ret = 0;
out:
    return ret;
}
