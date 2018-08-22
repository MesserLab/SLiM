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
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "trees.h"


/* Ensure the tree is in a consistent state */
static void
hapgen_check_state(hapgen_t *MSP_UNUSED(self))
{
    /* TODO some checks! */
}

void
hapgen_print_state(hapgen_t *self, FILE *out)
{
    size_t j;

    fprintf(out, "Hapgen state\n");
    fprintf(out, "num_samples = %d\n", (int) self->num_samples);
    fprintf(out, "num_sites = %d\n", (int) self->num_sites);
    fprintf(out, "haplotype matrix\n");
    for (j = 0; j < self->num_samples; j++) {
        fprintf(out, "%s\n",
            self->haplotype_matrix + (j * (self->num_sites + 1)));
    }
    hapgen_check_state(self);
}


static inline int WARN_UNUSED
hapgen_update_sample(hapgen_t * self, size_t sample_index, site_id_t site,
        const char *derived_state)
{
    int ret = 0;
    size_t index = sample_index * (self->num_sites + 1) + (size_t) site;

    if (self->haplotype_matrix[index] == derived_state[0]) {
        ret = MSP_ERR_INCONSISTENT_MUTATIONS;
        goto out;
    }
    self->haplotype_matrix[index] = derived_state[0];
out:
    return ret;
}

static int
hapgen_apply_tree_site(hapgen_t *self, site_t *site)
{
    int ret = 0;
    const node_id_t *restrict list_left = self->tree.left_sample;
    const node_id_t *restrict list_right = self->tree.right_sample;
    const node_id_t *restrict list_next = self->tree.next_sample;
    node_id_t node, index, stop;
    table_size_t j;
    const char *derived_state;

    for (j = 0; j < site->mutations_length; j++) {
        if (site->mutations[j].derived_state_length != 1) {
            ret = MSP_ERR_NON_SINGLE_CHAR_MUTATION;
            goto out;
        }
        derived_state = site->mutations[j].derived_state;
        node = site->mutations[j].node;
        index = list_left[node];
        if (index != MSP_NULL_NODE) {
            stop = list_right[node];
            while (true) {
                ret = hapgen_update_sample(self, (size_t) index, site->id, derived_state);
                if (ret != 0) {
                    goto out;
                }
                if (index == stop) {
                    break;
                }
                index = list_next[index];
            }
        }
    }
out:
    return ret;
}

static int
hapgen_generate_all_haplotypes(hapgen_t *self)
{
    int ret = 0;
    table_size_t j;
    table_size_t num_sites = 0;
    site_t *sites = NULL;
    sparse_tree_t *t = &self->tree;

    for (ret = sparse_tree_first(t); ret == 1; ret = sparse_tree_next(t)) {
        ret = sparse_tree_get_sites(t, &sites, &num_sites);
        if (ret != 0) {
            goto out;
        }
        for (j = 0; j < num_sites; j++) {
            ret = hapgen_apply_tree_site(self, &sites[j]);
            if (ret != 0) {
                goto out;
            }
        }
    }
out:
    return ret;
}

int
hapgen_alloc(hapgen_t *self, tree_sequence_t *tree_sequence)
{
    int ret = 0;
    size_t j, k;
    site_t site;

    assert(tree_sequence != NULL);
    memset(self, 0, sizeof(hapgen_t));
    self->num_samples = tree_sequence_get_num_samples(tree_sequence);
    self->sequence_length = tree_sequence_get_sequence_length(tree_sequence);
    self->num_sites = tree_sequence_get_num_sites(tree_sequence);
    self->tree_sequence = tree_sequence;

    ret = tree_sequence_get_sample_index_map(tree_sequence, &self->sample_index_map);
    if (ret != 0) {
        goto out;
    }
    ret = sparse_tree_alloc(&self->tree, tree_sequence, MSP_SAMPLE_LISTS);
    if (ret != 0) {
        goto out;
    }
    self->haplotype_matrix = malloc(
            self->num_samples * (self->num_sites + 1) * sizeof(char));
    if (self->haplotype_matrix == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    /* Set the NULL string ends. */
    for (j = 0; j < self->num_samples; j++) {
        self->haplotype_matrix[
            (j + 1) * (self->num_sites + 1) - 1] = '\0';
    }
    /* For each site set the ancestral type */
    for (k = 0; k < self->num_sites; k++) {
        ret = tree_sequence_get_site(self->tree_sequence, k, &site);
        if (ret != 0) {
            goto out;
        }
        if (site.ancestral_state_length != 1) {
            ret = MSP_ERR_NON_SINGLE_CHAR_MUTATION;
            goto out;
        }
        for (j = 0; j < self->num_samples; j++) {
            self->haplotype_matrix[j * (self->num_sites + 1) + k] =
                site.ancestral_state[0];
        }
    }
    ret = hapgen_generate_all_haplotypes(self);
out:
    return ret;
}

int
hapgen_free(hapgen_t *self)
{
    msp_safe_free(self->output_haplotype);
    msp_safe_free(self->haplotype_matrix);
    sparse_tree_free(&self->tree);
    return 0;
}

int
hapgen_get_haplotype(hapgen_t *self, node_id_t sample_index, char **haplotype)
{
    int ret = 0;

    if (sample_index >= (node_id_t) self->num_samples) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    *haplotype = self->haplotype_matrix + ((size_t) sample_index) * (self->num_sites + 1);
out:
    return ret;
}
