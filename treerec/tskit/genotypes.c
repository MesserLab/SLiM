/*
 * MIT License
 *
 * Copyright (c) 2019 Tskit Developers
 * Copyright (c) 2016-2018 University of Oxford
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
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include <tskit/genotypes.h>


/* ======================================================== *
 * Haplotype generator
 * ======================================================== */

/* Ensure the tree is in a consistent state */
static void
tsk_hapgen_check_state(tsk_hapgen_t *TSK_UNUSED(self))
{
    /* TODO some checks! */
}

void
tsk_hapgen_print_state(tsk_hapgen_t *self, FILE *out)
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
    tsk_hapgen_check_state(self);
}


static inline int TSK_WARN_UNUSED
tsk_hapgen_update_sample(tsk_hapgen_t * self, size_t sample_index, tsk_id_t site,
        const char *derived_state)
{
    int ret = 0;
    size_t index = sample_index * (self->num_sites + 1) + (size_t) site;

    if (self->haplotype_matrix[index] == derived_state[0]) {
        ret = TSK_ERR_INCONSISTENT_MUTATIONS;
        goto out;
    }
    self->haplotype_matrix[index] = derived_state[0];
out:
    return ret;
}

static int
tsk_hapgen_apply_tree_site(tsk_hapgen_t *self, tsk_site_t *site)
{
    int ret = 0;
    const tsk_id_t *restrict list_left = self->tree.left_sample;
    const tsk_id_t *restrict list_right = self->tree.right_sample;
    const tsk_id_t *restrict list_next = self->tree.next_sample;
    tsk_id_t node, index, stop;
    tsk_size_t j;
    const char *derived_state;

    for (j = 0; j < site->mutations_length; j++) {
        if (site->mutations[j].derived_state_length != 1) {
            ret = TSK_ERR_NON_SINGLE_CHAR_MUTATION;
            goto out;
        }
        derived_state = site->mutations[j].derived_state;
        node = site->mutations[j].node;
        index = list_left[node];
        if (index != TSK_NULL) {
            stop = list_right[node];
            while (true) {
                ret = tsk_hapgen_update_sample(self, (size_t) index, site->id, derived_state);
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
tsk_hapgen_generate_all_haplotypes(tsk_hapgen_t *self)
{
    int ret = 0;
    tsk_size_t j;
    tsk_size_t num_sites = 0;
    tsk_site_t *sites = NULL;
    tsk_tree_t *t = &self->tree;

    for (ret = tsk_tree_first(t); ret == 1; ret = tsk_tree_next(t)) {
        ret = tsk_tree_get_sites(t, &sites, &num_sites);
        if (ret != 0) {
            goto out;
        }
        for (j = 0; j < num_sites; j++) {
            ret = tsk_hapgen_apply_tree_site(self, &sites[j]);
            if (ret != 0) {
                goto out;
            }
        }
    }
out:
    return ret;
}

int
tsk_hapgen_init(tsk_hapgen_t *self, tsk_treeseq_t *tree_sequence)
{
    int ret = 0;
    size_t j, k;
    tsk_site_t site;

    assert(tree_sequence != NULL);
    memset(self, 0, sizeof(tsk_hapgen_t));
    self->num_samples = tsk_treeseq_get_num_samples(tree_sequence);
    self->sequence_length = tsk_treeseq_get_sequence_length(tree_sequence);
    self->num_sites = tsk_treeseq_get_num_sites(tree_sequence);
    self->tree_sequence = tree_sequence;

    ret = tsk_treeseq_get_sample_index_map(tree_sequence, &self->sample_index_map);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_tree_init(&self->tree, tree_sequence, TSK_SAMPLE_LISTS);
    if (ret != 0) {
        goto out;
    }
    self->haplotype_matrix = malloc(
            self->num_samples * (self->num_sites + 1) * sizeof(char));
    if (self->haplotype_matrix == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    /* Set the NULL string ends. */
    for (j = 0; j < self->num_samples; j++) {
        self->haplotype_matrix[
            (j + 1) * (self->num_sites + 1) - 1] = '\0';
    }
    /* For each site set the ancestral type */
    for (k = 0; k < self->num_sites; k++) {
        ret = tsk_treeseq_get_site(self->tree_sequence, (tsk_id_t) k, &site);
        if (ret != 0) {
            goto out;
        }
        if (site.ancestral_state_length != 1) {
            ret = TSK_ERR_NON_SINGLE_CHAR_MUTATION;
            goto out;
        }
        for (j = 0; j < self->num_samples; j++) {
            self->haplotype_matrix[j * (self->num_sites + 1) + k] =
                site.ancestral_state[0];
        }
    }
    ret = tsk_hapgen_generate_all_haplotypes(self);
out:
    return ret;
}

int
tsk_hapgen_free(tsk_hapgen_t *self)
{
    tsk_safe_free(self->output_haplotype);
    tsk_safe_free(self->haplotype_matrix);
    tsk_tree_free(&self->tree);
    return 0;
}

int
tsk_hapgen_get_haplotype(tsk_hapgen_t *self, tsk_id_t sample_index, char **haplotype)
{
    int ret = 0;

    if (sample_index < 0 || sample_index >= (tsk_id_t) self->num_samples) {
        ret = TSK_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    *haplotype = self->haplotype_matrix + ((size_t) sample_index) * (self->num_sites + 1);
out:
    return ret;
}

/* ======================================================== *
 * Variant generator
 * ======================================================== */

void
tsk_vargen_print_state(tsk_vargen_t *self, FILE *out)
{
    fprintf(out, "tsk_vargen state\n");
    fprintf(out, "tree_site_index = %d\n", (int) self->tree_site_index);
}

static int
tsk_vargen_next_tree(tsk_vargen_t *self)
{
    int ret = 0;

    ret = tsk_tree_next(&self->tree);
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
tsk_vargen_init(tsk_vargen_t *self, tsk_treeseq_t *tree_sequence,
        tsk_id_t *samples, size_t num_samples, tsk_flags_t options)
{
    int ret = TSK_ERR_NO_MEMORY;
    tsk_flags_t tree_options;
    size_t j, num_nodes, num_samples_alloc;
    tsk_size_t max_alleles = 4;

    assert(tree_sequence != NULL);
    memset(self, 0, sizeof(tsk_vargen_t));

    if (samples == NULL) {
        self->num_samples = tsk_treeseq_get_num_samples(tree_sequence);
        num_samples_alloc = self->num_samples;
    } else {
        /* Take a copy of the samples for simplicity */
        num_nodes = tsk_treeseq_get_num_nodes(tree_sequence);
        /* We can have num_samples = 0 here, so guard against malloc(0) */
        num_samples_alloc = num_samples + 1;
        self->samples = malloc(num_samples_alloc * sizeof(*self->samples));
        self->sample_index_map = malloc(num_nodes * sizeof(*self->sample_index_map));
        if (self->samples == NULL || self->sample_index_map == NULL) {
            ret = TSK_ERR_NO_MEMORY;
            goto out;
        }
        memcpy(self->samples, samples, num_samples * sizeof(*self->samples));
        memset(self->sample_index_map, 0xff, num_nodes * sizeof(*self->sample_index_map));
        /* Create the reverse mapping */
        for (j = 0; j < num_samples; j++) {
            if (samples[j] < 0 || samples[j] >= (tsk_id_t) num_nodes) {
                ret = TSK_ERR_OUT_OF_BOUNDS;
                goto out;
            }
            if (self->sample_index_map[samples[j]] != TSK_NULL) {
                ret = TSK_ERR_DUPLICATE_SAMPLE;
                goto out;
            }
            self->sample_index_map[samples[j]] = (tsk_id_t) j;
        }
        self->num_samples = num_samples;
    }
    self->num_sites = tsk_treeseq_get_num_sites(tree_sequence);
    self->tree_sequence = tree_sequence;
    self->options = options;
    if (self->options & TSK_16_BIT_GENOTYPES) {
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
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    /* When a list of samples is given, we use the traversal based algorithm
     * and turn off the sample list tracking in the tree */
    tree_options = 0;
    if (self->samples == NULL) {
        tree_options = TSK_SAMPLE_LISTS;
    }
    ret = tsk_tree_init(&self->tree, tree_sequence, tree_options);
    if (ret != 0) {
        goto out;
    }
    self->finished = 0;
    self->tree_site_index = 0;
    ret = tsk_tree_first(&self->tree);
    if (ret < 0) {
        goto out;
    }
    ret = 0;
out:
    return ret;
}

int
tsk_vargen_free(tsk_vargen_t *self)
{
    tsk_tree_free(&self->tree);
    tsk_safe_free(self->variant.genotypes.u8);
    tsk_safe_free(self->variant.alleles);
    tsk_safe_free(self->variant.allele_lengths);
    tsk_safe_free(self->samples);
    tsk_safe_free(self->sample_index_map);
    return 0;
}

static int
tsk_vargen_expand_alleles(tsk_vargen_t *self)
{
    int ret = 0;
    tsk_variant_t *var = &self->variant;
    void *p;
    tsk_size_t hard_limit = UINT8_MAX;

    if (self->options & TSK_16_BIT_GENOTYPES) {
        hard_limit = UINT16_MAX;
    }
    if (var->max_alleles == hard_limit) {
        ret = TSK_ERR_TOO_MANY_ALLELES;
        goto out;
    }
    var->max_alleles = TSK_MIN(hard_limit, var->max_alleles * 2);
    p = realloc(var->alleles, var->max_alleles * sizeof(*var->alleles));
    if (p == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    var->alleles = p;
    p = realloc(var->allele_lengths, var->max_alleles * sizeof(*var->allele_lengths));
    if (p == NULL) {
        ret = TSK_ERR_NO_MEMORY;
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
static int TSK_WARN_UNUSED
tsk_vargen_update_genotypes_u8_sample_list(tsk_vargen_t *self, tsk_id_t node, tsk_size_t derived)
{
    uint8_t *restrict genotypes = self->variant.genotypes.u8;
    const tsk_id_t *restrict list_left = self->tree.left_sample;
    const tsk_id_t *restrict list_right = self->tree.right_sample;
    const tsk_id_t *restrict list_next = self->tree.next_sample;
    tsk_id_t index, stop;
    int ret = 0;

    assert(derived < UINT8_MAX);

    index = list_left[node];
    if (index != TSK_NULL) {
        stop = list_right[node];
        while (true) {
            if (genotypes[index] == derived) {
                ret = TSK_ERR_INCONSISTENT_MUTATIONS;
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

static int TSK_WARN_UNUSED
tsk_vargen_update_genotypes_u16_sample_list(tsk_vargen_t *self, tsk_id_t node, tsk_size_t derived)
{
    uint16_t *restrict genotypes = self->variant.genotypes.u16;
    const tsk_id_t *restrict list_left = self->tree.left_sample;
    const tsk_id_t *restrict list_right = self->tree.right_sample;
    const tsk_id_t *restrict list_next = self->tree.next_sample;
    tsk_id_t index, stop;
    int ret = 0;

    assert(derived < UINT16_MAX);

    index = list_left[node];
    if (index != TSK_NULL) {
        stop = list_right[node];
        while (true) {
            if (genotypes[index] == derived) {
                ret = TSK_ERR_INCONSISTENT_MUTATIONS;
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

typedef int (*visit_func_t)(tsk_vargen_t *, tsk_id_t, tsk_size_t);

static int TSK_WARN_UNUSED
tsk_vargen_traverse(tsk_vargen_t *self, tsk_id_t node, tsk_size_t derived, visit_func_t visit)
{
    int ret = 0;
    tsk_id_t * restrict stack = self->tree.stack1;
    const tsk_id_t * restrict left_child = self->tree.left_child;
    const tsk_id_t * restrict right_sib = self->tree.right_sib;
    const tsk_id_t *restrict sample_index_map = self->sample_index_map;
    tsk_id_t u, v, sample_index;
    int stack_top;

    stack_top = 0;
    stack[0] = node;
    while (stack_top >= 0) {
        u = stack[stack_top];
        sample_index = sample_index_map[u];
        if (sample_index != TSK_NULL) {
            ret = visit(self, sample_index, derived);
            if (ret != 0) {
                goto out;
            }
        }
        stack_top--;
        for (v = left_child[u]; v != TSK_NULL; v = right_sib[v]) {
            stack_top++;
            stack[stack_top] = v;
        }
    }
out:
    return ret;
}

static int
tsk_vargen_visit_u8(tsk_vargen_t *self, tsk_id_t sample_index, tsk_size_t derived)
{
    int ret = 0;
    uint8_t *restrict genotypes = self->variant.genotypes.u8;

    assert(derived < UINT8_MAX);
    assert(sample_index != -1);
    if (genotypes[sample_index] == derived) {
        ret = TSK_ERR_INCONSISTENT_MUTATIONS;
        goto out;
    }
    genotypes[sample_index] = (uint8_t) derived;
out:
    return ret;
}

static int
tsk_vargen_visit_u16(tsk_vargen_t *self, tsk_id_t sample_index, tsk_size_t derived)
{
    int ret = 0;
    uint16_t *restrict genotypes = self->variant.genotypes.u16;

    assert(derived < UINT16_MAX);
    assert(sample_index != -1);
    if (genotypes[sample_index] == derived) {
        ret = TSK_ERR_INCONSISTENT_MUTATIONS;
        goto out;
    }
    genotypes[sample_index] = (uint16_t) derived;
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_vargen_update_genotypes_u8_traversal(tsk_vargen_t *self, tsk_id_t node, tsk_size_t derived)
{
    return tsk_vargen_traverse(self, node, derived, tsk_vargen_visit_u8);
}

static int TSK_WARN_UNUSED
tsk_vargen_update_genotypes_u16_traversal(tsk_vargen_t *self, tsk_id_t node, tsk_size_t derived)
{
    return tsk_vargen_traverse(self, node, derived, tsk_vargen_visit_u16);
}

static int
tsk_vargen_update_site(tsk_vargen_t *self)
{
    int ret = 0;
    tsk_size_t j, derived;
    tsk_variant_t *var = &self->variant;
    tsk_site_t *site = var->site;
    tsk_mutation_t mutation;
    bool genotypes16 = !!(self->options & TSK_16_BIT_GENOTYPES);
    bool by_traversal = self->samples != NULL;
    int (*update_genotypes)(tsk_vargen_t *, tsk_id_t, tsk_size_t);

    /* For now we use a traversal method to find genotypes when we have a
     * specified set of samples, but we should provide the option to do it
     * via tracked_samples in the tree also. There will be a tradeoff: if
     * we only have a small number of samples, it's probably better to
     * do it by traversal. For large sets of samples though, it'll be
     * definitely better to use the sample list infrastructure. */
    if (genotypes16) {
        update_genotypes = tsk_vargen_update_genotypes_u16_sample_list;
        if (by_traversal) {
            update_genotypes = tsk_vargen_update_genotypes_u16_traversal;
        }
    } else {
        update_genotypes = tsk_vargen_update_genotypes_u8_sample_list;
        if (by_traversal) {
            update_genotypes = tsk_vargen_update_genotypes_u8_traversal;
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
                ret = tsk_vargen_expand_alleles(self);
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
tsk_vargen_next(tsk_vargen_t *self, tsk_variant_t **variant)
{
    int ret = 0;

    bool not_done = true;

    if (!self->finished) {
        while (not_done && self->tree_site_index == self->tree.sites_length) {
            ret = tsk_vargen_next_tree(self);
            if (ret < 0) {
                goto out;
            }
            not_done = ret == 1;
        }
        if (not_done) {
            self->variant.site = &self->tree.sites[self->tree_site_index];
            ret = tsk_vargen_update_site(self);
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
