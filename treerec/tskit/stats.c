/*
 * MIT License
 *
 * Copyright (c) 2018-2025 Tskit Developers
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

void
tsk_ld_calc_print_state(const tsk_ld_calc_t *self, FILE *out)
{
    fprintf(out, "tree = %p\n", (const void *) &self->tree);
    fprintf(out, "max_sites = %d\n", (int) self->max_sites);
    fprintf(out, "max_distance = %f\n", self->max_distance);
}

int TSK_WARN_UNUSED
tsk_ld_calc_init(tsk_ld_calc_t *self, const tsk_treeseq_t *tree_sequence)
{
    int ret = 0;
    tsk_memset(self, 0, sizeof(*self));

    ret = tsk_tree_init(&self->tree, tree_sequence, 0);
    if (ret != 0) {
        goto out;
    }
    self->tree_sequence = tree_sequence;
    self->total_samples = tsk_treeseq_get_num_samples(self->tree_sequence);

    self->sample_buffer = tsk_malloc(self->total_samples * sizeof(*self->sample_buffer));
    if (self->sample_buffer == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
out:
    return ret;
}

int
tsk_ld_calc_free(tsk_ld_calc_t *self)
{
    tsk_tree_free(&self->tree);
    tsk_safe_free(self->sample_buffer);
    return 0;
}

static int
tsk_ld_calc_check_site(tsk_ld_calc_t *TSK_UNUSED(self), const tsk_site_t *site)
{
    int ret = 0;

    /* These are both limitations in the current implementation, there's no
     * fundamental reason why we can't support them */
    if (site->mutations_length != 1) {
        ret = tsk_trace_error(TSK_ERR_ONLY_INFINITE_SITES);
        goto out;
    }
    if (site->ancestral_state_length == site->mutations[0].derived_state_length
        && tsk_memcmp(site->ancestral_state, site->mutations[0].derived_state,
               site->ancestral_state_length)
               == 0) {
        ret = tsk_trace_error(TSK_ERR_SILENT_MUTATIONS_NOT_SUPPORTED);
        goto out;
    }
out:
    return ret;
}

static int
tsk_ld_calc_set_focal_samples(tsk_ld_calc_t *self)
{
    int ret = 0;
    tsk_id_t focal_node = self->focal_site.mutations[0].node;

    ret = tsk_tree_track_descendant_samples(&self->tree, focal_node);
    if (ret != 0) {
        goto out;
    }
    self->focal_samples = self->tree.num_tracked_samples[focal_node];
out:
    return ret;
}

static int
tsk_ld_calc_initialise(tsk_ld_calc_t *self, tsk_id_t a)
{
    int ret = 0;

    ret = tsk_treeseq_get_site(self->tree_sequence, a, &self->focal_site);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_ld_calc_check_site(self, &self->focal_site);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_tree_seek(&self->tree, self->focal_site.position, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_ld_calc_set_focal_samples(self);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

static int
tsk_ld_calc_compute_r2(tsk_ld_calc_t *self, const tsk_site_t *target_site, double *r2)
{
    const double n = (double) self->total_samples;
    double f_a, f_b, f_ab, D, denom;
    tsk_id_t node;
    int ret = tsk_ld_calc_check_site(self, target_site);

    if (ret != 0) {
        goto out;
    }
    node = target_site->mutations[0].node;
    f_a = ((double) self->focal_samples) / n;
    f_b = ((double) self->tree.num_samples[node]) / n;
    f_ab = ((double) self->tree.num_tracked_samples[node]) / n;
    D = f_ab - f_a * f_b;
    denom = f_a * f_b * (1 - f_a) * (1 - f_b);
    *r2 = (D * D) / denom;
out:
    return ret;
}

static int
tsk_ld_calc_compute_and_append(
    tsk_ld_calc_t *self, const tsk_site_t *target_site, bool *ret_done)
{
    int ret = 0;
    double r2;
    double distance = fabs(self->focal_site.position - target_site->position);
    bool done = true;

    if (distance <= self->max_distance && self->result_length < self->max_sites) {
        ret = tsk_ld_calc_compute_r2(self, target_site, &r2);
        if (ret != 0) {
            goto out;
        }
        self->result[self->result_length] = r2;
        self->result_length++;
        done = false;
    }
    *ret_done = done;
out:
    return ret;
}

static int
tsk_ld_calc_run_forward(tsk_ld_calc_t *self)
{
    int ret = 0;
    tsk_size_t j;
    bool done = false;

    for (j = 0; j < self->tree.sites_length; j++) {
        if (self->tree.sites[j].id > self->focal_site.id) {
            ret = tsk_ld_calc_compute_and_append(self, &self->tree.sites[j], &done);
            if (ret != 0) {
                goto out;
            }
            if (done) {
                break;
            }
        }
    }
    while (((ret = tsk_tree_next(&self->tree)) == TSK_TREE_OK) && !done) {
        for (j = 0; j < self->tree.sites_length; j++) {
            ret = tsk_ld_calc_compute_and_append(self, &self->tree.sites[j], &done);
            if (ret != 0) {
                goto out;
            }
            if (done) {
                break;
            }
        }
    }
    if (ret < 0) {
        goto out;
    }
    ret = 0;
out:
    return ret;
}

static int
tsk_ld_calc_run_reverse(tsk_ld_calc_t *self)
{
    int ret = 0;
    tsk_id_t j;
    bool done = false;

    for (j = (tsk_id_t) self->tree.sites_length - 1; j >= 0; j--) {
        if (self->tree.sites[j].id < self->focal_site.id) {
            ret = tsk_ld_calc_compute_and_append(self, &self->tree.sites[j], &done);
            if (ret != 0) {
                goto out;
            }
            if (done) {
                break;
            }
        }
    }
    while (((ret = tsk_tree_prev(&self->tree)) == TSK_TREE_OK) && !done) {
        for (j = (tsk_id_t) self->tree.sites_length - 1; j >= 0; j--) {
            ret = tsk_ld_calc_compute_and_append(self, &self->tree.sites[j], &done);
            if (ret != 0) {
                goto out;
            }
            if (done) {
                break;
            }
        }
    }
    if (ret < 0) {
        goto out;
    }
    ret = 0;
out:
    return ret;
}

int
tsk_ld_calc_get_r2(tsk_ld_calc_t *self, tsk_id_t a, tsk_id_t b, double *r2)
{
    int ret = 0;
    tsk_site_t target_site;

    ret = tsk_ld_calc_initialise(self, a);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_get_site(self->tree_sequence, b, &target_site);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_tree_seek(&self->tree, target_site.position, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_ld_calc_compute_r2(self, &target_site, r2);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

int
tsk_ld_calc_get_r2_array(tsk_ld_calc_t *self, tsk_id_t a, int direction,
    tsk_size_t max_sites, double max_distance, double *r2, tsk_size_t *num_r2_values)
{
    int ret = tsk_ld_calc_initialise(self, a);

    if (ret != 0) {
        goto out;
    }

    self->max_sites = max_sites;
    self->max_distance = max_distance;
    self->result_length = 0;
    self->result = r2;

    if (direction == TSK_DIR_FORWARD) {
        ret = tsk_ld_calc_run_forward(self);
    } else if (direction == TSK_DIR_REVERSE) {
        ret = tsk_ld_calc_run_reverse(self);
    } else {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
    }
    if (ret != 0) {
        goto out;
    }
    *num_r2_values = self->result_length;
out:
    return ret;
}
