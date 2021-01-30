/*
 * MIT License
 *
 * Copyright (c) 2019 Tskit Developers
 * Copyright (c) 2016-2017 University of Oxford
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
#include <stdlib.h>
#include <math.h>

#include <tskit/stats.h>

static void
tsk_ld_calc_check_state(const tsk_ld_calc_t *self)
{
    uint32_t u;
    uint32_t num_nodes = (uint32_t) tsk_treeseq_get_num_nodes(self->tree_sequence);
    tsk_tree_t *tA = self->outer_tree;
    tsk_tree_t *tB = self->inner_tree;

    tsk_bug_assert(tA->index == tB->index);

    /* The inner tree's mark values should all be zero. */
    for (u = 0; u < num_nodes; u++) {
        tsk_bug_assert(tA->marked[u] == 0);
        tsk_bug_assert(tB->marked[u] == 0);
    }
}

void
tsk_ld_calc_print_state(const tsk_ld_calc_t *self, FILE *out)
{
    fprintf(out, "tree_sequence = %p\n", (const void *) self->tree_sequence);
    fprintf(out, "outer tree index = %d\n", (int) self->outer_tree->index);
    fprintf(out, "outer tree interval = (%f, %f)\n", self->outer_tree->left,
        self->outer_tree->right);
    fprintf(out, "inner tree index = %d\n", (int) self->inner_tree->index);
    fprintf(out, "inner tree interval = (%f, %f)\n", self->inner_tree->left,
        self->inner_tree->right);
    tsk_ld_calc_check_state(self);
}

int TSK_WARN_UNUSED
tsk_ld_calc_init(tsk_ld_calc_t *self, const tsk_treeseq_t *tree_sequence)
{
    int ret = TSK_ERR_GENERIC;

    memset(self, 0, sizeof(tsk_ld_calc_t));
    self->tree_sequence = tree_sequence;
    self->num_sites = tsk_treeseq_get_num_sites(tree_sequence);
    self->outer_tree = malloc(sizeof(tsk_tree_t));
    self->inner_tree = malloc(sizeof(tsk_tree_t));
    if (self->outer_tree == NULL || self->inner_tree == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    ret = tsk_tree_init(self->outer_tree, self->tree_sequence, TSK_SAMPLE_LISTS);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_tree_init(self->inner_tree, self->tree_sequence, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_tree_first(self->outer_tree);
    if (ret < 0) {
        goto out;
    }
    ret = tsk_tree_first(self->inner_tree);
    if (ret < 0) {
        goto out;
    }
    ret = 0;
out:
    return ret;
}

int
tsk_ld_calc_free(tsk_ld_calc_t *self)
{
    if (self->inner_tree != NULL) {
        tsk_tree_free(self->inner_tree);
        free(self->inner_tree);
    }
    if (self->outer_tree != NULL) {
        tsk_tree_free(self->outer_tree);
        free(self->outer_tree);
    }
    return 0;
}

/* Position the two trees so that the specified site is within their
 * interval.
 */
static int TSK_WARN_UNUSED
tsk_ld_calc_position_trees(tsk_ld_calc_t *self, tsk_id_t site_index)
{
    int ret = TSK_ERR_GENERIC;
    tsk_site_t mut;
    double x;
    tsk_tree_t *tA = self->outer_tree;
    tsk_tree_t *tB = self->inner_tree;

    ret = tsk_treeseq_get_site(self->tree_sequence, site_index, &mut);
    if (ret != 0) {
        goto out;
    }
    x = mut.position;
    tsk_bug_assert(tA->index == tB->index);
    while (x >= tA->right) {
        ret = tsk_tree_next(tA);
        if (ret < 0) {
            goto out;
        }
        tsk_bug_assert(ret == 1);
        ret = tsk_tree_next(tB);
        if (ret < 0) {
            goto out;
        }
        tsk_bug_assert(ret == 1);
    }
    while (x < tA->left) {
        ret = tsk_tree_prev(tA);
        if (ret < 0) {
            goto out;
        }
        tsk_bug_assert(ret == 1);
        ret = tsk_tree_prev(tB);
        if (ret < 0) {
            goto out;
        }
        tsk_bug_assert(ret == 1);
    }
    ret = 0;
    tsk_bug_assert(x >= tA->left && x < tB->right);
    tsk_bug_assert(tA->index == tB->index);
out:
    return ret;
}

static double
tsk_ld_calc_overlap_within_tree(tsk_ld_calc_t *self, tsk_site_t sA, tsk_site_t sB)
{
    const tsk_tree_t *t = self->inner_tree;
    const tsk_node_table_t *nodes = &self->tree_sequence->tables->nodes;
    tsk_id_t u, v, nAB;

    tsk_bug_assert(sA.mutations_length == 1);
    tsk_bug_assert(sB.mutations_length == 1);
    u = sA.mutations[0].node;
    v = sB.mutations[0].node;
    if (nodes->time[u] > nodes->time[v]) {
        v = sA.mutations[0].node;
        u = sB.mutations[0].node;
    }
    while (u != v && u != TSK_NULL) {
        u = t->parent[u];
    }
    nAB = 0;
    if (u == v) {
        nAB = TSK_MIN(
            t->num_samples[sA.mutations[0].node], t->num_samples[sB.mutations[0].node]);
    }
    return (double) nAB;
}

static inline int TSK_WARN_UNUSED
tsk_ld_calc_set_tracked_samples(tsk_ld_calc_t *self, tsk_site_t sA)
{
    int ret = 0;

    tsk_bug_assert(sA.mutations_length == 1);
    ret = tsk_tree_set_tracked_samples_from_sample_list(
        self->inner_tree, self->outer_tree, sA.mutations[0].node);
    return ret;
}

static int TSK_WARN_UNUSED
tsk_ld_calc_get_r2_array_forward(tsk_ld_calc_t *self, tsk_id_t source_index,
    tsk_size_t max_sites, double max_distance, double *r2, tsk_size_t *num_r2_values)
{
    int ret = TSK_ERR_GENERIC;
    tsk_site_t sA, sB;
    double fA, fB, fAB, D;
    int tracked_samples_set = 0;
    tsk_tree_t *tA, *tB;
    double n = (double) tsk_treeseq_get_num_samples(self->tree_sequence);
    tsk_id_t j;
    double nAB;

    tA = self->outer_tree;
    tB = self->inner_tree;
    ret = tsk_treeseq_get_site(self->tree_sequence, source_index, &sA);
    if (ret != 0) {
        goto out;
    }
    if (sA.mutations_length > 1) {
        ret = TSK_ERR_ONLY_INFINITE_SITES;
        goto out;
    }
    fA = ((double) tA->num_samples[sA.mutations[0].node]) / n;
    tsk_bug_assert(fA > 0);
    tB->mark = 1;
    for (j = 0; j < (tsk_id_t) max_sites; j++) {
        if (source_index + j + 1 >= (tsk_id_t) self->num_sites) {
            break;
        }
        ret = tsk_treeseq_get_site(self->tree_sequence, (source_index + j + 1), &sB);
        if (ret != 0) {
            goto out;
        }
        if (sB.mutations_length > 1) {
            ret = TSK_ERR_ONLY_INFINITE_SITES;
            goto out;
        }
        if (sB.position - sA.position > max_distance) {
            break;
        }
        while (sB.position >= tB->right) {
            ret = tsk_tree_next(tB);
            if (ret < 0) {
                goto out;
            }
            tsk_bug_assert(ret == 1);
        }
        fB = ((double) tB->num_samples[sB.mutations[0].node]) / n;
        tsk_bug_assert(fB > 0);
        if (sB.position < tA->right) {
            nAB = tsk_ld_calc_overlap_within_tree(self, sA, sB);
        } else {
            if (!tracked_samples_set && tB->marked[sA.mutations[0].node] == 1) {
                tracked_samples_set = 1;
                ret = tsk_ld_calc_set_tracked_samples(self, sA);
                if (ret != 0) {
                    goto out;
                }
            }
            if (tracked_samples_set) {
                nAB = (double) tB->num_tracked_samples[sB.mutations[0].node];
            } else {
                nAB = tsk_ld_calc_overlap_within_tree(self, sA, sB);
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
        ret = tsk_tree_prev(tB);
        if (ret < 0) {
            goto out;
        }
        tsk_bug_assert(ret == 1);
    }
    *num_r2_values = (tsk_size_t) j;
    ret = 0;
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_ld_calc_get_r2_array_reverse(tsk_ld_calc_t *self, tsk_id_t source_index,
    tsk_size_t max_sites, double max_distance, double *r2, tsk_size_t *num_r2_values)
{
    int ret = TSK_ERR_GENERIC;
    tsk_site_t sA, sB;
    double fA, fB, fAB, D;
    int tracked_samples_set = 0;
    tsk_tree_t *tA, *tB;
    double n = (double) tsk_treeseq_get_num_samples(self->tree_sequence);
    double nAB;
    tsk_id_t j, site_index;

    tA = self->outer_tree;
    tB = self->inner_tree;
    ret = tsk_treeseq_get_site(self->tree_sequence, source_index, &sA);
    if (ret != 0) {
        goto out;
    }
    if (sA.mutations_length > 1) {
        ret = TSK_ERR_ONLY_INFINITE_SITES;
        goto out;
    }
    fA = ((double) tA->num_samples[sA.mutations[0].node]) / n;
    tsk_bug_assert(fA > 0);
    tB->mark = 1;
    for (j = 0; j < (tsk_id_t) max_sites; j++) {
        site_index = source_index - j - 1;
        if (site_index < 0) {
            break;
        }
        ret = tsk_treeseq_get_site(self->tree_sequence, site_index, &sB);
        if (ret != 0) {
            goto out;
        }
        if (sB.mutations_length > 1) {
            ret = TSK_ERR_ONLY_INFINITE_SITES;
            goto out;
        }
        if (sA.position - sB.position > max_distance) {
            break;
        }
        while (sB.position < tB->left) {
            ret = tsk_tree_prev(tB);
            if (ret < 0) {
                goto out;
            }
            tsk_bug_assert(ret == 1);
        }
        fB = ((double) tB->num_samples[sB.mutations[0].node]) / n;
        tsk_bug_assert(fB > 0);
        if (sB.position >= tA->left) {
            nAB = tsk_ld_calc_overlap_within_tree(self, sA, sB);
        } else {
            if (!tracked_samples_set && tB->marked[sA.mutations[0].node] == 1) {
                tracked_samples_set = 1;
                ret = tsk_ld_calc_set_tracked_samples(self, sA);
                if (ret != 0) {
                    goto out;
                }
            }
            if (tracked_samples_set) {
                nAB = (double) tB->num_tracked_samples[sB.mutations[0].node];
            } else {
                nAB = tsk_ld_calc_overlap_within_tree(self, sA, sB);
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
        ret = tsk_tree_next(tB);
        if (ret < 0) {
            goto out;
        }
        tsk_bug_assert(ret == 1);
    }
    *num_r2_values = (tsk_size_t) j;
    ret = 0;
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_ld_calc_get_r2_array(tsk_ld_calc_t *self, tsk_id_t a, int direction,
    tsk_size_t max_sites, double max_distance, double *r2, tsk_size_t *num_r2_values)
{
    int ret = TSK_ERR_GENERIC;

    if (a < 0 || a >= (tsk_id_t) self->num_sites) {
        ret = TSK_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    ret = tsk_ld_calc_position_trees(self, a);
    if (ret != 0) {
        goto out;
    }
    if (direction == TSK_DIR_FORWARD) {
        ret = tsk_ld_calc_get_r2_array_forward(
            self, a, max_sites, max_distance, r2, num_r2_values);
    } else if (direction == TSK_DIR_REVERSE) {
        ret = tsk_ld_calc_get_r2_array_reverse(
            self, a, max_sites, max_distance, r2, num_r2_values);
    } else {
        ret = TSK_ERR_BAD_PARAM_VALUE;
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_ld_calc_get_r2(tsk_ld_calc_t *self, tsk_id_t a, tsk_id_t b, double *r2)
{
    int ret = TSK_ERR_GENERIC;
    tsk_site_t sA, sB;
    double fA, fB, fAB, D;
    tsk_tree_t *tA, *tB;
    double n = (double) tsk_treeseq_get_num_samples(self->tree_sequence);
    double nAB;
    tsk_id_t tmp;

    if (a < 0 || b < 0 || a >= (tsk_id_t) self->num_sites
        || b >= (tsk_id_t) self->num_sites) {
        ret = TSK_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    if (a > b) {
        tmp = a;
        a = b;
        b = tmp;
    }
    ret = tsk_ld_calc_position_trees(self, a);
    if (ret != 0) {
        goto out;
    }
    /* We can probably do a lot better than this implementation... */
    tA = self->outer_tree;
    tB = self->inner_tree;
    ret = tsk_treeseq_get_site(self->tree_sequence, a, &sA);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_get_site(self->tree_sequence, b, &sB);
    if (ret != 0) {
        goto out;
    }
    if (sA.mutations_length > 1 || sB.mutations_length > 1) {
        ret = TSK_ERR_ONLY_INFINITE_SITES;
        goto out;
    }
    tsk_bug_assert(sA.mutations_length == 1);
    /* tsk_bug_assert(tA->parent[sA.mutations[0].node] != TSK_NULL); */
    fA = ((double) tA->num_samples[sA.mutations[0].node]) / n;
    tsk_bug_assert(fA > 0);
    ret = tsk_ld_calc_set_tracked_samples(self, sA);
    if (ret != 0) {
        goto out;
    }

    while (sB.position >= tB->right) {
        ret = tsk_tree_next(tB);
        if (ret < 0) {
            goto out;
        }
        tsk_bug_assert(ret == 1);
    }
    /* tsk_bug_assert(tB->parent[sB.mutations[0].node] != TSK_NULL); */
    fB = ((double) tB->num_samples[sB.mutations[0].node]) / n;
    tsk_bug_assert(fB > 0);
    nAB = (double) tB->num_tracked_samples[sB.mutations[0].node];
    fAB = nAB / n;
    D = fAB - fA * fB;
    *r2 = D * D / (fA * fB * (1 - fA) * (1 - fB));

    /* Now rewind the inner iterator back. */
    while (tB->index > tA->index) {
        ret = tsk_tree_prev(tB);
        if (ret < 0) {
            goto out;
        }
        tsk_bug_assert(ret == 1);
    }
    ret = 0;
out:
    return ret;
}
