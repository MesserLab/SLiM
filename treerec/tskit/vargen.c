/*
** Copyright (C) 2016-2018 University of Oxford
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

void
vargen_print_state(vargen_t *self, FILE *out)
{
    fprintf(out, "vargen state\n");
    fprintf(out, "tree_site_index = %d\n", (int) self->tree_site_index);
}

static int
vargen_next_tree(vargen_t *self)
{
    int ret = 0;

    ret = sparse_tree_next(&self->tree);
    if (ret == 0) {
        self->finished = 1;
    } else if (ret < 0) {
        goto out;
    }
    self->tree_site_index = 0;
out:
    return ret;
}

int
vargen_alloc(vargen_t *self, tree_sequence_t *tree_sequence,
        node_id_t *samples, size_t num_samples, int flags)
{
    int ret = MSP_ERR_NO_MEMORY;
    int tree_flags;
    size_t j, num_nodes, num_samples_alloc;
    table_size_t max_alleles = 4;

    assert(tree_sequence != NULL);
    memset(self, 0, sizeof(vargen_t));

    if (samples == NULL) {
        self->num_samples = tree_sequence_get_num_samples(tree_sequence);
        num_samples_alloc = self->num_samples;
    } else {
        /* Take a copy of the samples for simplicity */
        num_nodes = tree_sequence_get_num_nodes(tree_sequence);
        /* We can have num_samples = 0 here, so guard against malloc(0) */
        num_samples_alloc = num_samples + 1;
        self->samples = malloc(num_samples_alloc * sizeof(*self->samples));
        self->sample_index_map = malloc(num_nodes * sizeof(*self->sample_index_map));
        if (self->samples == NULL || self->sample_index_map == NULL) {
            ret = MSP_ERR_NO_MEMORY;
            goto out;
        }
        memcpy(self->samples, samples, num_samples * sizeof(*self->samples));
        memset(self->sample_index_map, 0xff, num_nodes * sizeof(*self->sample_index_map));
        /* Create the reverse mapping */
        for (j = 0; j < num_samples; j++) {
            if (samples[j] < 0 || samples[j] >= (node_id_t) num_nodes) {
                ret = MSP_ERR_OUT_OF_BOUNDS;
                goto out;
            }
            if (self->sample_index_map[samples[j]] != MSP_NULL_NODE) {
                ret = MSP_ERR_DUPLICATE_SAMPLE;
                goto out;
            }
            self->sample_index_map[samples[j]] = (node_id_t) j;
        }
        self->num_samples = num_samples;
    }
    self->num_sites = tree_sequence_get_num_sites(tree_sequence);
    self->tree_sequence = tree_sequence;
    self->flags = flags;
    if (self->flags & MSP_16_BIT_GENOTYPES) {
        self->variant.genotypes.u16 = malloc(
            num_samples_alloc * sizeof(*self->variant.genotypes.u16));
    } else {
        self->variant.genotypes.u8 = malloc(
            num_samples_alloc * sizeof(*self->variant.genotypes.u8));
    }
    self->variant.max_alleles = max_alleles;
    self->variant.alleles = malloc(max_alleles * sizeof(*self->variant.alleles));
    self->variant.allele_lengths = malloc(max_alleles
            * sizeof(*self->variant.allele_lengths));
    /* Because genotypes is a union we can check the pointer */
    if (self->variant.genotypes.u8 == NULL || self->variant.alleles == NULL
            || self->variant.allele_lengths == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    /* When a list of samples is given, we use the traversal based algorithm
     * and turn off the sample list tracking in the tree */
    tree_flags = 0;
    if (self->samples == NULL) {
        tree_flags = MSP_SAMPLE_LISTS;
    }
    ret = sparse_tree_alloc(&self->tree, tree_sequence, tree_flags);
    if (ret != 0) {
        goto out;
    }
    self->finished = 0;
    self->tree_site_index = 0;
    ret = sparse_tree_first(&self->tree);
    if (ret < 0) {
        goto out;
    }
    ret = 0;
out:
    return ret;
}

int
vargen_free(vargen_t *self)
{
    sparse_tree_free(&self->tree);
    msp_safe_free(self->variant.genotypes.u8);
    msp_safe_free(self->variant.alleles);
    msp_safe_free(self->variant.allele_lengths);
    msp_safe_free(self->samples);
    msp_safe_free(self->sample_index_map);
    return 0;
}

static int
vargen_expand_alleles(vargen_t *self)
{
    int ret = 0;
    variant_t *var = &self->variant;
    void *p;
    table_size_t hard_limit = UINT8_MAX;

    if (self->flags & MSP_16_BIT_GENOTYPES) {
        hard_limit = UINT16_MAX;
    }
    if (var->max_alleles == hard_limit) {
        ret = MSP_ERR_TOO_MANY_ALLELES;
        goto out;
    }
    var->max_alleles = MSP_MIN(hard_limit, var->max_alleles * 2);
    p = realloc(var->alleles, var->max_alleles * sizeof(*var->alleles));
    if (p == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    var->alleles = p;
    p = realloc(var->allele_lengths, var->max_alleles * sizeof(*var->allele_lengths));
    if (p == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    var->allele_lengths = p;
out:
    return ret;
}

/* The following pair of functions are identical except one handles 8 bit
 * genotypes and the other handles 16 bit genotypes. This is done for performance
 * reasons as this is a key function and for common alleles can entail
 * iterating over millions of samples. The compiler hints are included for the
 * same reason.
 */
static int WARN_UNUSED
vargen_update_genotypes_u8_sample_list(vargen_t *self, node_id_t node, table_size_t derived)
{
    uint8_t *restrict genotypes = self->variant.genotypes.u8;
    const node_id_t *restrict list_left = self->tree.left_sample;
    const node_id_t *restrict list_right = self->tree.right_sample;
    const node_id_t *restrict list_next = self->tree.next_sample;
    node_id_t index, stop;
    int ret = 0;

    assert(derived < UINT8_MAX);

    index = list_left[node];
    if (index != MSP_NULL_NODE) {
        stop = list_right[node];
        while (true) {
            if (genotypes[index] == derived) {
                ret = MSP_ERR_INCONSISTENT_MUTATIONS;
                goto out;
            }
            genotypes[index] = (uint8_t) derived;
            if (index == stop) {
                break;
            }
            index = list_next[index];
        }
    }
out:
    return ret;
}

static int WARN_UNUSED
vargen_update_genotypes_u16_sample_list(vargen_t *self, node_id_t node, table_size_t derived)
{
    uint16_t *restrict genotypes = self->variant.genotypes.u16;
    const node_id_t *restrict list_left = self->tree.left_sample;
    const node_id_t *restrict list_right = self->tree.right_sample;
    const node_id_t *restrict list_next = self->tree.next_sample;
    node_id_t index, stop;
    int ret = 0;

    assert(derived < UINT16_MAX);

    index = list_left[node];
    if (index != MSP_NULL_NODE) {
        stop = list_right[node];
        while (true) {
            if (genotypes[index] == derived) {
                ret = MSP_ERR_INCONSISTENT_MUTATIONS;
                goto out;
            }
            genotypes[index] = (uint16_t) derived;
            if (index == stop) {
                break;
            }
            index = list_next[index];
        }
    }
out:
    return ret;
}

/* The following functions implement the genotype setting by traversing
 * down the tree to the samples. We're not so worried about performance here
 * because this should only be used when we have a very small number of samples,
 * and so we use a visit function to avoid duplicating code.
 */

typedef int (*visit_func_t)(vargen_t *, node_id_t, table_size_t);

static int WARN_UNUSED
vargen_traverse(vargen_t *self, node_id_t node, table_size_t derived, visit_func_t visit)
{
    int ret = 0;
    node_id_t * restrict stack = self->tree.stack1;
    const node_id_t * restrict left_child = self->tree.left_child;
    const node_id_t * restrict right_sib = self->tree.right_sib;
    const node_id_t *restrict sample_index_map = self->sample_index_map;
    node_id_t u, v, sample_index;
    int stack_top;

    stack_top = 0;
    stack[0] = node;
    while (stack_top >= 0) {
        u = stack[stack_top];
        sample_index = sample_index_map[u];
        if (sample_index != MSP_NULL_NODE) {
            ret = visit(self, sample_index, derived);
            if (ret != 0) {
                goto out;
            }
        }
        stack_top--;
        for (v = left_child[u]; v != MSP_NULL_NODE; v = right_sib[v]) {
            stack_top++;
            stack[stack_top] = v;
        }
    }
out:
    return ret;
}

static int
vargen_visit_u8(vargen_t *self, node_id_t sample_index, table_size_t derived)
{
    int ret = 0;
    uint8_t *restrict genotypes = self->variant.genotypes.u8;

    assert(derived < UINT8_MAX);
    assert(sample_index != -1);
    if (genotypes[sample_index] == derived) {
        ret = MSP_ERR_INCONSISTENT_MUTATIONS;
        goto out;
    }
    genotypes[sample_index] = (uint8_t) derived;
out:
    return ret;
}

static int
vargen_visit_u16(vargen_t *self, node_id_t sample_index, table_size_t derived)
{
    int ret = 0;
    uint16_t *restrict genotypes = self->variant.genotypes.u16;

    assert(derived < UINT16_MAX);
    assert(sample_index != -1);
    if (genotypes[sample_index] == derived) {
        ret = MSP_ERR_INCONSISTENT_MUTATIONS;
        goto out;
    }
    genotypes[sample_index] = (uint16_t) derived;
out:
    return ret;
}

static int WARN_UNUSED
vargen_update_genotypes_u8_traversal(vargen_t *self, node_id_t node, table_size_t derived)
{
    return vargen_traverse(self, node, derived, vargen_visit_u8);
}

static int WARN_UNUSED
vargen_update_genotypes_u16_traversal(vargen_t *self, node_id_t node, table_size_t derived)
{
    return vargen_traverse(self, node, derived, vargen_visit_u16);
}

static int
vargen_update_site(vargen_t *self)
{
    int ret = 0;
    table_size_t j, derived;
    variant_t *var = &self->variant;
    site_t *site = var->site;
    mutation_t mutation;
    bool genotypes16 = !!(self->flags & MSP_16_BIT_GENOTYPES);
    bool by_traversal = self->samples != NULL;
    int (*update_genotypes)(vargen_t *, node_id_t, table_size_t);

    /* For now we use a traversal method to find genotypes when we have a
     * specified set of samples, but we should provide the option to do it
     * via tracked_samples in the tree also. There will be a tradeoff: if
     * we only have a small number of samples, it's probably better to
     * do it by traversal. For large sets of samples though, it'll be
     * definitely better to use the sample list infrastructure. */
    if (genotypes16) {
        update_genotypes = vargen_update_genotypes_u16_sample_list;
        if (by_traversal) {
            update_genotypes = vargen_update_genotypes_u16_traversal;
        }
    } else {
        update_genotypes = vargen_update_genotypes_u8_sample_list;
        if (by_traversal) {
            update_genotypes = vargen_update_genotypes_u8_traversal;
        }
    }

    /* Ancestral state is always allele 0 */
    var->alleles[0] = site->ancestral_state;
    var->allele_lengths[0] = site->ancestral_state_length;
    var->num_alleles = 1;

    /* The algorithm for generating the allelic state of every sample works by
     * examining each mutation in order, and setting the state for all the
     * samples under the mutation's node. For complex sites where there is
     * more than one mutation, we depend on the ordering of mutations being
     * correct. Specifically, any mutation that is above another mutation in
     * the tree must be visited first. This is enforced using the mutation.parent
     * field, where we require that a mutation's parent must appear before it
     * in the list of mutations. This guarantees the correctness of this algorithm.
     */
    if (genotypes16) {
        memset(self->variant.genotypes.u16, 0, 2 * self->num_samples);
    } else {
        memset(self->variant.genotypes.u8, 0, self->num_samples);
    }
    for (j = 0; j < site->mutations_length; j++) {
        mutation = site->mutations[j];
        /* Compute the allele index for this derived state value. */
        derived = 0;
        while (derived < var->num_alleles) {
            if (mutation.derived_state_length == var->allele_lengths[derived]
                    && memcmp(mutation.derived_state, var->alleles[derived],
                        var->allele_lengths[derived]) == 0) {
                break;
            }
            derived++;
        }
        if (derived == var->num_alleles) {
            if (var->num_alleles == var->max_alleles) {
                ret = vargen_expand_alleles(self);
                if (ret != 0) {
                    goto out;
                }
            }
            var->alleles[derived] = mutation.derived_state;
            var->allele_lengths[derived] = mutation.derived_state_length;
            var->num_alleles++;
        }
        ret = update_genotypes(self, mutation.node, derived);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

int
vargen_next(vargen_t *self, variant_t **variant)
{
    int ret = 0;

    bool not_done = true;

    if (!self->finished) {
        while (not_done && self->tree_site_index == self->tree.sites_length) {
            ret = vargen_next_tree(self);
            if (ret < 0) {
                goto out;
            }
            not_done = ret == 1;
        }
        if (not_done) {
            self->variant.site = &self->tree.sites[self->tree_site_index];
            ret = vargen_update_site(self);
            if (ret != 0) {
                goto out;
            }
            self->tree_site_index++;
            *variant = &self->variant;
            ret = 1;
        }
    }
out:
    return ret;
}
