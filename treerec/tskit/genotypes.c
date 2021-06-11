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
#include <stdbool.h>
#include <stdlib.h>

#include <tskit/genotypes.h>

/* ======================================================== *
 * Variant generator
 * ======================================================== */

void
tsk_vargen_print_state(const tsk_vargen_t *self, FILE *out)
{
    tsk_size_t j;

    fprintf(out, "tsk_vargen state\n");
    fprintf(out, "tree_index = %d\n", self->tree.index);
    fprintf(out, "tree_site_index = %d\n", (int) self->tree_site_index);
    fprintf(out, "user_alleles = %d\n", self->user_alleles);
    fprintf(out, "num_alleles = %d\n", self->variant.num_alleles);
    for (j = 0; j < self->variant.num_alleles; j++) {
        fprintf(out, "\tlen = %d, '%.*s'\n", self->variant.allele_lengths[j],
            self->variant.allele_lengths[j], self->variant.alleles[j]);
    }
    fprintf(out, "num_samples = %d\n", (int) self->num_samples);
    for (j = 0; j < tsk_treeseq_get_num_nodes(self->tree_sequence); j++) {
        fprintf(out, "\t%d -> %d\n", (int) j, (int) self->sample_index_map[j]);
    }
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

/* Copy the fixed allele mapping specified by the user into local
 * memory. */
static int
tsk_vargen_copy_alleles(tsk_vargen_t *self, const char **alleles)
{
    int ret = 0;
    tsk_size_t j;
    size_t total_len, allele_len, offset;

    self->variant.num_alleles = self->variant.max_alleles;

    total_len = 0;
    for (j = 0; j < self->variant.num_alleles; j++) {
        allele_len = strlen(alleles[j]);
        self->variant.allele_lengths[j] = (tsk_size_t) allele_len;
        total_len += allele_len;
    }
    self->user_alleles_mem = malloc(total_len * sizeof(char *));
    if (self->user_alleles_mem == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    offset = 0;
    for (j = 0; j < self->variant.num_alleles; j++) {
        strcpy(self->user_alleles_mem + offset, alleles[j]);
        self->variant.alleles[j] = self->user_alleles_mem + offset;
        offset += self->variant.allele_lengths[j];
    }
out:
    return ret;
}

static int
vargen_init_samples_and_index_map(tsk_vargen_t *self, const tsk_treeseq_t *tree_sequence,
    const tsk_id_t *samples, size_t num_samples, size_t num_samples_alloc,
    tsk_flags_t options)
{
    int ret = 0;
    const tsk_flags_t *flags = tree_sequence->tables->nodes.flags;
    size_t j, num_nodes;
    bool impute_missing = !!(options & TSK_ISOLATED_NOT_MISSING);
    tsk_id_t u;

    num_nodes = tsk_treeseq_get_num_nodes(tree_sequence);
    self->alt_samples = malloc(num_samples_alloc * sizeof(*samples));
    self->alt_sample_index_map = malloc(num_nodes * sizeof(*self->alt_sample_index_map));
    if (self->alt_samples == NULL || self->alt_sample_index_map == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    memcpy(self->alt_samples, samples, num_samples * sizeof(*samples));
    memset(self->alt_sample_index_map, 0xff,
        num_nodes * sizeof(*self->alt_sample_index_map));
    /* Create the reverse mapping */
    for (j = 0; j < num_samples; j++) {
        u = samples[j];
        if (u < 0 || u >= (tsk_id_t) num_nodes) {
            ret = TSK_ERR_OUT_OF_BOUNDS;
            goto out;
        }
        if (self->alt_sample_index_map[u] != TSK_NULL) {
            ret = TSK_ERR_DUPLICATE_SAMPLE;
            goto out;
        }
        /* We can only detect missing data for samples */
        if (!impute_missing && !(flags[u] & TSK_NODE_IS_SAMPLE)) {
            ret = TSK_ERR_MUST_IMPUTE_NON_SAMPLES;
            goto out;
        }
        self->alt_sample_index_map[samples[j]] = (tsk_id_t) j;
    }
out:
    return ret;
}

int
tsk_vargen_init(tsk_vargen_t *self, const tsk_treeseq_t *tree_sequence,
    const tsk_id_t *samples, size_t num_samples, const char **alleles,
    tsk_flags_t options)
{
    int ret = TSK_ERR_NO_MEMORY;
    tsk_flags_t tree_options;
    size_t num_samples_alloc, max_alleles_limit;
    tsk_size_t max_alleles;

    tsk_bug_assert(tree_sequence != NULL);
    memset(self, 0, sizeof(tsk_vargen_t));

    if (samples == NULL) {
        self->samples = tsk_treeseq_get_samples(tree_sequence);
        self->num_samples = tsk_treeseq_get_num_samples(tree_sequence);
        self->sample_index_map = tsk_treeseq_get_sample_index_map(tree_sequence);
        num_samples_alloc = self->num_samples;
    } else {
        /* Take a copy of the samples for simplicity */
        /* We can have num_samples = 0 here, so guard against malloc(0) */
        num_samples_alloc = num_samples + 1;
        ret = vargen_init_samples_and_index_map(
            self, tree_sequence, samples, num_samples, num_samples_alloc, options);
        if (ret != 0) {
            goto out;
        }
        self->samples = samples;
        self->num_samples = num_samples;
        self->sample_index_map = self->alt_sample_index_map;
    }
    self->num_sites = tsk_treeseq_get_num_sites(tree_sequence);
    self->tree_sequence = tree_sequence;
    self->options = options;
    if (self->options & TSK_16_BIT_GENOTYPES) {
        self->variant.genotypes.i16
            = malloc(num_samples_alloc * sizeof(*self->variant.genotypes.i16));
        max_alleles_limit = INT16_MAX;
    } else {
        self->variant.genotypes.i8
            = malloc(num_samples_alloc * sizeof(*self->variant.genotypes.i8));
        max_alleles_limit = INT8_MAX;
    }

    if (alleles == NULL) {
        self->user_alleles = false;
        max_alleles = 4; /* Arbitrary --- we'll rarely have more than this */
    } else {
        self->user_alleles = true;
        /* Count the input alleles. The end is designated by the NULL sentinel. */
        for (max_alleles = 0; alleles[max_alleles] != NULL; max_alleles++)
            ;
        if (max_alleles > max_alleles_limit) {
            ret = TSK_ERR_TOO_MANY_ALLELES;
            goto out;
        }
        if (max_alleles == 0) {
            ret = TSK_ERR_ZERO_ALLELES;
            goto out;
        }
    }
    self->variant.max_alleles = max_alleles;
    self->variant.alleles = calloc(max_alleles, sizeof(*self->variant.alleles));
    self->variant.allele_lengths
        = malloc(max_alleles * sizeof(*self->variant.allele_lengths));
    /* Because genotypes is a union we can check the pointer */
    if (self->variant.genotypes.i8 == NULL || self->variant.alleles == NULL
        || self->variant.allele_lengths == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    if (self->user_alleles) {
        ret = tsk_vargen_copy_alleles(self, alleles);
        if (ret != 0) {
            goto out;
        }
    }

    /* When a list of samples is given, we use the traversal based algorithm
     * and turn off the sample list tracking in the tree */
    tree_options = 0;
    if (self->alt_samples == NULL) {
        tree_options = TSK_SAMPLE_LISTS;
    } else {
        self->traversal_stack = malloc(
            tsk_treeseq_get_num_nodes(tree_sequence) * sizeof(*self->traversal_stack));
        if (self->traversal_stack == NULL) {
            ret = TSK_ERR_NO_MEMORY;
            goto out;
        }
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
    tsk_safe_free(self->variant.genotypes.i8);
    tsk_safe_free(self->variant.alleles);
    tsk_safe_free(self->variant.allele_lengths);
    tsk_safe_free(self->user_alleles_mem);
    tsk_safe_free(self->alt_samples);
    tsk_safe_free(self->alt_sample_index_map);
    tsk_safe_free(self->traversal_stack);
    return 0;
}

static int
tsk_vargen_expand_alleles(tsk_vargen_t *self)
{
    int ret = 0;
    tsk_variant_t *var = &self->variant;
    void *p;
    tsk_size_t hard_limit = INT8_MAX;

    if (self->options & TSK_16_BIT_GENOTYPES) {
        hard_limit = INT16_MAX;
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
tsk_vargen_update_genotypes_i8_sample_list(
    tsk_vargen_t *self, tsk_id_t node, tsk_id_t derived)
{
    int8_t *restrict genotypes = self->variant.genotypes.i8;
    const tsk_id_t *restrict list_left = self->tree.left_sample;
    const tsk_id_t *restrict list_right = self->tree.right_sample;
    const tsk_id_t *restrict list_next = self->tree.next_sample;
    tsk_id_t index, stop;
    int ret = 0;

    tsk_bug_assert(derived < INT8_MAX);

    index = list_left[node];
    if (index != TSK_NULL) {
        stop = list_right[node];
        while (true) {

            ret += genotypes[index] == TSK_MISSING_DATA;
            genotypes[index] = (int8_t) derived;
            if (index == stop) {
                break;
            }
            index = list_next[index];
        }
    }

    return ret;
}

static int TSK_WARN_UNUSED
tsk_vargen_update_genotypes_i16_sample_list(
    tsk_vargen_t *self, tsk_id_t node, tsk_id_t derived)
{
    int16_t *restrict genotypes = self->variant.genotypes.i16;
    const tsk_id_t *restrict list_left = self->tree.left_sample;
    const tsk_id_t *restrict list_right = self->tree.right_sample;
    const tsk_id_t *restrict list_next = self->tree.next_sample;
    tsk_id_t index, stop;
    int ret = 0;

    tsk_bug_assert(derived < INT16_MAX);

    index = list_left[node];
    if (index != TSK_NULL) {
        stop = list_right[node];
        while (true) {

            ret += genotypes[index] == TSK_MISSING_DATA;
            genotypes[index] = (int16_t) derived;
            if (index == stop) {
                break;
            }
            index = list_next[index];
        }
    }

    return ret;
}

/* The following functions implement the genotype setting by traversing
 * down the tree to the samples. We're not so worried about performance here
 * because this should only be used when we have a very small number of samples,
 * and so we use a visit function to avoid duplicating code.
 */

typedef int (*visit_func_t)(tsk_vargen_t *, tsk_id_t, tsk_id_t);

static int TSK_WARN_UNUSED
tsk_vargen_traverse(
    tsk_vargen_t *self, tsk_id_t node, tsk_id_t derived, visit_func_t visit)
{
    int ret = 0;
    tsk_id_t *restrict stack = self->traversal_stack;
    const tsk_id_t *restrict left_child = self->tree.left_child;
    const tsk_id_t *restrict right_sib = self->tree.right_sib;
    const tsk_id_t *restrict sample_index_map = self->sample_index_map;
    tsk_id_t u, v, sample_index;
    int stack_top;
    int no_longer_missing = 0;

    stack_top = 0;
    stack[0] = node;
    while (stack_top >= 0) {
        u = stack[stack_top];
        sample_index = sample_index_map[u];
        if (sample_index != TSK_NULL) {
            ret = visit(self, sample_index, derived);
            if (ret < 0) {
                goto out;
            }
            no_longer_missing += ret;
        }
        stack_top--;
        for (v = left_child[u]; v != TSK_NULL; v = right_sib[v]) {
            stack_top++;
            stack[stack_top] = v;
        }
    }
    ret = no_longer_missing;
out:
    return ret;
}

static int
tsk_vargen_visit_i8(tsk_vargen_t *self, tsk_id_t sample_index, tsk_id_t derived)
{
    int ret = 0;
    int8_t *restrict genotypes = self->variant.genotypes.i8;

    tsk_bug_assert(derived < INT8_MAX);
    tsk_bug_assert(sample_index != -1);

    ret = genotypes[sample_index] == TSK_MISSING_DATA;
    genotypes[sample_index] = (int8_t) derived;

    return ret;
}

static int
tsk_vargen_visit_i16(tsk_vargen_t *self, tsk_id_t sample_index, tsk_id_t derived)
{
    int ret = 0;
    int16_t *restrict genotypes = self->variant.genotypes.i16;

    tsk_bug_assert(derived < INT16_MAX);
    tsk_bug_assert(sample_index != -1);

    ret = genotypes[sample_index] == TSK_MISSING_DATA;
    genotypes[sample_index] = (int16_t) derived;

    return ret;
}

static int TSK_WARN_UNUSED
tsk_vargen_update_genotypes_i8_traversal(
    tsk_vargen_t *self, tsk_id_t node, tsk_id_t derived)
{
    return tsk_vargen_traverse(self, node, derived, tsk_vargen_visit_i8);
}

static int TSK_WARN_UNUSED
tsk_vargen_update_genotypes_i16_traversal(
    tsk_vargen_t *self, tsk_id_t node, tsk_id_t derived)
{
    return tsk_vargen_traverse(self, node, derived, tsk_vargen_visit_i16);
}

static tsk_size_t
tsk_vargen_mark_missing_i16(tsk_vargen_t *self)
{
    tsk_size_t num_missing = 0;
    const tsk_id_t *restrict left_child = self->tree.left_child;
    const tsk_id_t *restrict right_sib = self->tree.right_sib;
    const tsk_id_t *restrict sample_index_map = self->sample_index_map;
    int16_t *restrict genotypes = self->variant.genotypes.i16;
    tsk_id_t root, sample_index;

    for (root = self->tree.left_root; root != TSK_NULL; root = right_sib[root]) {
        if (left_child[root] == TSK_NULL) {
            sample_index = sample_index_map[root];
            if (sample_index != TSK_NULL) {
                genotypes[sample_index] = TSK_MISSING_DATA;
                num_missing++;
            }
        }
    }
    return num_missing;
}

static tsk_size_t
tsk_vargen_mark_missing_i8(tsk_vargen_t *self)
{
    tsk_size_t num_missing = 0;
    const tsk_id_t *restrict left_child = self->tree.left_child;
    const tsk_id_t *restrict right_sib = self->tree.right_sib;
    const tsk_id_t *restrict sample_index_map = self->sample_index_map;
    int8_t *restrict genotypes = self->variant.genotypes.i8;
    tsk_id_t root, sample_index;

    for (root = self->tree.left_root; root != TSK_NULL; root = right_sib[root]) {
        if (left_child[root] == TSK_NULL) {
            sample_index = sample_index_map[root];
            if (sample_index != TSK_NULL) {
                genotypes[sample_index] = TSK_MISSING_DATA;
                num_missing++;
            }
        }
    }
    return num_missing;
}

static tsk_id_t
tsk_vargen_get_allele_index(tsk_vargen_t *self, const char *allele, tsk_size_t length)
{
    tsk_id_t ret = -1;
    tsk_size_t j;
    const tsk_variant_t *var = &self->variant;

    for (j = 0; j < var->num_alleles; j++) {
        if (length == var->allele_lengths[j]
            && memcmp(allele, var->alleles[j], length) == 0) {
            ret = (tsk_id_t) j;
            break;
        }
    }
    return ret;
}

static int
tsk_vargen_update_site(tsk_vargen_t *self)
{
    int ret = 0;
    tsk_id_t allele_index;
    tsk_size_t j, num_missing;
    int no_longer_missing;
    tsk_variant_t *var = &self->variant;
    const tsk_site_t *site = var->site;
    tsk_mutation_t mutation;
    bool genotypes16 = !!(self->options & TSK_16_BIT_GENOTYPES);
    bool impute_missing = !!(self->options & TSK_ISOLATED_NOT_MISSING);
    bool by_traversal = self->alt_samples != NULL;
    int (*update_genotypes)(tsk_vargen_t *, tsk_id_t, tsk_id_t);
    tsk_size_t (*mark_missing)(tsk_vargen_t *);

    /* For now we use a traversal method to find genotypes when we have a
     * specified set of samples, but we should provide the option to do it
     * via tracked_samples in the tree also. There will be a tradeoff: if
     * we only have a small number of samples, it's probably better to
     * do it by traversal. For large sets of samples though, it may be
     * better to use the sample list infrastructure. */
    if (genotypes16) {
        mark_missing = tsk_vargen_mark_missing_i16;
        update_genotypes = tsk_vargen_update_genotypes_i16_sample_list;
        if (by_traversal) {
            update_genotypes = tsk_vargen_update_genotypes_i16_traversal;
        }
    } else {
        mark_missing = tsk_vargen_mark_missing_i8;
        update_genotypes = tsk_vargen_update_genotypes_i8_sample_list;
        if (by_traversal) {
            update_genotypes = tsk_vargen_update_genotypes_i8_traversal;
        }
    }
    if (self->user_alleles) {
        allele_index = tsk_vargen_get_allele_index(
            self, site->ancestral_state, site->ancestral_state_length);
        if (allele_index == -1) {
            ret = TSK_ERR_ALLELE_NOT_FOUND;
            goto out;
        }
    } else {
        /* Ancestral state is always allele 0 */
        var->alleles[0] = site->ancestral_state;
        var->allele_lengths[0] = site->ancestral_state_length;
        var->num_alleles = 1;
        allele_index = 0;
    }

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
        for (j = 0; j < self->num_samples; j++) {
            self->variant.genotypes.i16[j] = (int16_t) allele_index;
        }
    } else {
        for (j = 0; j < self->num_samples; j++) {
            self->variant.genotypes.i8[j] = (int8_t) allele_index;
        }
    }
    /* We mark missing data *before* updating the genotypes because
     * mutations directly over samples should not be missing */
    num_missing = 0;
    if (!impute_missing) {
        num_missing = mark_missing(self);
    }
    for (j = 0; j < site->mutations_length; j++) {
        mutation = site->mutations[j];
        /* Compute the allele index for this derived state value. */
        allele_index = tsk_vargen_get_allele_index(
            self, mutation.derived_state, mutation.derived_state_length);
        if (allele_index == -1) {
            if (self->user_alleles) {
                ret = TSK_ERR_ALLELE_NOT_FOUND;
                goto out;
            }
            if (var->num_alleles == var->max_alleles) {
                ret = tsk_vargen_expand_alleles(self);
                if (ret != 0) {
                    goto out;
                }
            }
            allele_index = (tsk_id_t) var->num_alleles;
            var->alleles[allele_index] = mutation.derived_state;
            var->allele_lengths[allele_index] = mutation.derived_state_length;
            var->num_alleles++;
        }

        no_longer_missing = update_genotypes(self, mutation.node, allele_index);
        if (no_longer_missing < 0) {
            ret = no_longer_missing;
            goto out;
        }
        /* Update genotypes returns the number of missing values marked
         * not-missing */
        num_missing -= (tsk_size_t) no_longer_missing;
    }
    var->has_missing_data = num_missing > 0;
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
