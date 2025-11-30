/*
 * MIT License
 *
 * Copyright (c) 2019-2025 Tskit Developers
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
#include <math.h>

#include <tskit/genotypes.h>

/* ======================================================== *
 * Variant generator
 * ======================================================== */

void
tsk_variant_print_state(const tsk_variant_t *self, FILE *out)
{
    tsk_size_t j;

    fprintf(out, "tsk_variant state\n");
    fprintf(out, "user_alleles = %lld\n", (long long) self->user_alleles);
    fprintf(out, "num_alleles = %lld\n", (long long) self->num_alleles);
    for (j = 0; j < self->num_alleles; j++) {
        fprintf(out, "\tlen = %lld, '%.*s'\n", (long long) self->allele_lengths[j],
            (int) self->allele_lengths[j], self->alleles[j]);
    }
    fprintf(out, "num_samples = %lld\n", (long long) self->num_samples);
}

void
tsk_vargen_print_state(const tsk_vargen_t *self, FILE *out)
{
    tsk_variant_print_state(&self->variant, out);
}

/* Copy the fixed allele mapping specified by the user into local
 * memory. */
static int
tsk_variant_copy_alleles(tsk_variant_t *self, const char **alleles)
{
    int ret = 0;
    tsk_size_t j;
    size_t total_len, allele_len, offset;

    self->num_alleles = self->max_alleles;

    total_len = 0;
    for (j = 0; j < self->num_alleles; j++) {
        allele_len = strlen(alleles[j]);
        self->allele_lengths[j] = (tsk_size_t) allele_len;
        total_len += allele_len;
    }
    self->user_alleles_mem = tsk_malloc(total_len * sizeof(char *));
    if (self->user_alleles_mem == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    offset = 0;
    for (j = 0; j < self->num_alleles; j++) {
        strcpy(self->user_alleles_mem + offset, alleles[j]);
        self->alleles[j] = self->user_alleles_mem + offset;
        offset += (size_t) self->allele_lengths[j];
    }
out:
    return ret;
}

static int
variant_init_samples_and_index_map(tsk_variant_t *self,
    const tsk_treeseq_t *tree_sequence, const tsk_id_t *samples, tsk_size_t num_samples,
    size_t num_samples_alloc, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_size_t j, num_nodes;
    tsk_id_t u;

    num_nodes = tsk_treeseq_get_num_nodes(tree_sequence);
    self->alt_samples = tsk_malloc(num_samples_alloc * sizeof(*samples));
    self->alt_sample_index_map
        = tsk_malloc(num_nodes * sizeof(*self->alt_sample_index_map));
    if (self->alt_samples == NULL || self->alt_sample_index_map == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memcpy(self->alt_samples, samples, num_samples * sizeof(*samples));
    tsk_memset(self->alt_sample_index_map, 0xff,
        num_nodes * sizeof(*self->alt_sample_index_map));
    /* Create the reverse mapping */
    for (j = 0; j < num_samples; j++) {
        u = samples[j];
        if (u < 0 || u >= (tsk_id_t) num_nodes) {
            ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
            goto out;
        }
        if (self->alt_sample_index_map[u] != TSK_NULL) {
            ret = tsk_trace_error(TSK_ERR_DUPLICATE_SAMPLE);
            goto out;
        }
        self->alt_sample_index_map[samples[j]] = (tsk_id_t) j;
    }
out:
    return ret;
}

int
tsk_variant_init(tsk_variant_t *self, const tsk_treeseq_t *tree_sequence,
    const tsk_id_t *samples, tsk_size_t num_samples, const char **alleles,
    tsk_flags_t options)
{
    int ret = 0;
    tsk_size_t max_alleles_limit, max_alleles;
    tsk_size_t num_samples_alloc;

    tsk_memset(self, 0, sizeof(tsk_variant_t));

    /* Set site id to NULL to indicate the variant is not decoded */
    self->site.id = TSK_NULL;

    self->tree_sequence = tree_sequence;
    ret = tsk_tree_init(
        &self->tree, tree_sequence, samples == NULL ? TSK_SAMPLE_LISTS : 0);
    if (ret != 0) {
        goto out;
    }

    if (samples != NULL) {
        /* Take a copy of the samples so we don't have to manage the lifecycle*/
        self->samples = tsk_malloc(num_samples * sizeof(*samples));
        if (self->samples == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
        tsk_memcpy(self->samples, samples, num_samples * sizeof(*samples));
        self->num_samples = num_samples;
    }

    self->options = options;

    max_alleles_limit = INT32_MAX;

    if (alleles == NULL) {
        self->user_alleles = false;
        max_alleles = 4; /* Arbitrary --- we'll rarely have more than this */
    } else {
        self->user_alleles = true;
        /* Count the input alleles. The end is designated by the NULL sentinel. */
        for (max_alleles = 0; alleles[max_alleles] != NULL; max_alleles++)
            ;
        if (max_alleles > max_alleles_limit) {
            ret = tsk_trace_error(TSK_ERR_TOO_MANY_ALLELES);
            goto out;
        }
        if (max_alleles == 0) {
            ret = tsk_trace_error(TSK_ERR_ZERO_ALLELES);
            goto out;
        }
    }
    self->max_alleles = max_alleles;
    self->alleles = tsk_calloc(max_alleles, sizeof(*self->alleles));
    self->allele_lengths = tsk_malloc(max_alleles * sizeof(*self->allele_lengths));
    if (self->alleles == NULL || self->allele_lengths == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    if (self->user_alleles) {
        ret = tsk_variant_copy_alleles(self, alleles);
        if (ret != 0) {
            goto out;
        }
    }
    if (self->samples == NULL) {
        self->num_samples = tsk_treeseq_get_num_samples(tree_sequence);
        self->samples = tsk_malloc(self->num_samples * sizeof(*self->samples));
        if (self->samples == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
        tsk_memcpy(self->samples, tsk_treeseq_get_samples(tree_sequence),
            self->num_samples * sizeof(*self->samples));

        self->sample_index_map = tsk_treeseq_get_sample_index_map(tree_sequence);
        num_samples_alloc = self->num_samples;
    } else {
        num_samples_alloc = self->num_samples;
        ret = variant_init_samples_and_index_map(self, tree_sequence, self->samples,
            self->num_samples, (size_t) num_samples_alloc, self->options);
        if (ret != 0) {
            goto out;
        }
        self->sample_index_map = self->alt_sample_index_map;
    }
    /* When a list of samples is given, we use the traversal based algorithm
     * which doesn't use sample list tracking in the tree */
    if (self->alt_samples != NULL) {
        self->traversal_stack = tsk_malloc(
            tsk_treeseq_get_num_nodes(tree_sequence) * sizeof(*self->traversal_stack));
        if (self->traversal_stack == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
    }

    self->genotypes = tsk_malloc(num_samples_alloc * sizeof(*self->genotypes));
    if (self->genotypes == NULL || self->alleles == NULL
        || self->allele_lengths == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

out:
    return ret;
}

int
tsk_vargen_init(tsk_vargen_t *self, const tsk_treeseq_t *tree_sequence,
    const tsk_id_t *samples, tsk_size_t num_samples, const char **alleles,
    tsk_flags_t options)
{
    int ret = 0;

    tsk_bug_assert(tree_sequence != NULL);
    tsk_memset(self, 0, sizeof(tsk_vargen_t));

    self->tree_sequence = tree_sequence;
    ret = tsk_variant_init(
        &self->variant, tree_sequence, samples, num_samples, alleles, options);
    if (ret != 0) {
        goto out;
    }
    ret = 0;
out:
    return ret;
}

int
tsk_variant_free(tsk_variant_t *self)
{
    if (self->tree_sequence != NULL) {
        tsk_tree_free(&self->tree);
    }
    tsk_safe_free(self->genotypes);
    tsk_safe_free(self->alleles);
    tsk_safe_free(self->allele_lengths);
    tsk_safe_free(self->user_alleles_mem);
    tsk_safe_free(self->samples);
    tsk_safe_free(self->alt_samples);
    tsk_safe_free(self->alt_sample_index_map);
    tsk_safe_free(self->traversal_stack);
    return 0;
}

int
tsk_vargen_free(tsk_vargen_t *self)
{
    tsk_variant_free(&self->variant);
    return 0;
}

static int
tsk_variant_expand_alleles(tsk_variant_t *self)
{
    int ret = 0;
    void *p;
    tsk_size_t hard_limit = INT32_MAX;

    if (self->max_alleles == hard_limit) {
        ret = tsk_trace_error(TSK_ERR_TOO_MANY_ALLELES);
        goto out;
    }
    self->max_alleles = TSK_MIN(hard_limit, self->max_alleles * 2);
    p = tsk_realloc(self->alleles, self->max_alleles * sizeof(*self->alleles));
    if (p == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    self->alleles = p;
    p = tsk_realloc(
        self->allele_lengths, self->max_alleles * sizeof(*self->allele_lengths));
    if (p == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    self->allele_lengths = p;
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
tsk_variant_update_genotypes_sample_list(
    tsk_variant_t *self, tsk_id_t node, tsk_id_t derived)
{
    int32_t *restrict genotypes = self->genotypes;
    const tsk_id_t *restrict list_left = self->tree.left_sample;
    const tsk_id_t *restrict list_right = self->tree.right_sample;
    const tsk_id_t *restrict list_next = self->tree.next_sample;
    tsk_id_t index, stop;
    int ret = 0;

    tsk_bug_assert(derived < INT32_MAX);

    index = list_left[node];
    if (index != TSK_NULL) {
        stop = list_right[node];
        while (true) {

            ret += genotypes[index] == TSK_MISSING_DATA;
            genotypes[index] = (int32_t) derived;
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

typedef int (*visit_func_t)(tsk_variant_t *, tsk_id_t, tsk_id_t);

static int TSK_WARN_UNUSED
tsk_variant_traverse(
    tsk_variant_t *self, tsk_id_t node, tsk_id_t derived, visit_func_t visit)
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
tsk_variant_visit(tsk_variant_t *self, tsk_id_t sample_index, tsk_id_t derived)
{
    int ret = 0;
    int32_t *restrict genotypes = self->genotypes;

    tsk_bug_assert(derived < INT32_MAX);
    tsk_bug_assert(sample_index != -1);

    ret = genotypes[sample_index] == TSK_MISSING_DATA;
    genotypes[sample_index] = (int32_t) derived;

    return ret;
}

static int TSK_WARN_UNUSED
tsk_variant_update_genotypes_traversal(
    tsk_variant_t *self, tsk_id_t node, tsk_id_t derived)
{
    return tsk_variant_traverse(self, node, derived, tsk_variant_visit);
}

static tsk_size_t
tsk_variant_mark_missing(tsk_variant_t *self)
{
    tsk_size_t num_missing = 0;
    const tsk_id_t *restrict left_child = self->tree.left_child;
    const tsk_id_t *restrict right_sib = self->tree.right_sib;
    const tsk_id_t *restrict sample_index_map = self->sample_index_map;
    const tsk_id_t N = self->tree.virtual_root;
    int32_t *restrict genotypes = self->genotypes;
    tsk_id_t root, sample_index;

    for (root = left_child[N]; root != TSK_NULL; root = right_sib[root]) {
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

/* Mark missing for any requested node (sample or non-sample) that is isolated
 * in the current tree, i.e., has no parent and no children at this position. */
static tsk_size_t
tsk_variant_mark_missing_any(tsk_variant_t *self)
{
    tsk_size_t num_missing = 0;
    int32_t *restrict genotypes = self->genotypes;
    const tsk_id_t *restrict parent = self->tree.parent;
    const tsk_id_t *restrict left_child = self->tree.left_child;
    tsk_size_t j;

    for (j = 0; j < self->num_samples; j++) {
        tsk_id_t u = self->samples[j];
        if (parent[u] == TSK_NULL && left_child[u] == TSK_NULL) {
            genotypes[j] = TSK_MISSING_DATA;
            num_missing++;
        }
    }
    return num_missing;
}

static tsk_id_t
tsk_variant_get_allele_index(tsk_variant_t *self, const char *allele, tsk_size_t length)
{
    tsk_id_t ret = -1;
    tsk_size_t j;

    for (j = 0; j < self->num_alleles; j++) {
        if (length == self->allele_lengths[j]
            && tsk_memcmp(allele, self->alleles[j], length) == 0) {
            ret = (tsk_id_t) j;
            break;
        }
    }
    return ret;
}

int
tsk_variant_decode(
    tsk_variant_t *self, tsk_id_t site_id, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_id_t allele_index;
    tsk_size_t j, num_missing;
    int no_longer_missing;
    tsk_mutation_t mutation;
    bool impute_missing = !!(self->options & TSK_ISOLATED_NOT_MISSING);
    bool by_traversal = self->alt_samples != NULL;
    int (*update_genotypes)(tsk_variant_t *, tsk_id_t, tsk_id_t);
    tsk_size_t (*mark_missing)(tsk_variant_t *);

    if (self->tree_sequence == NULL) {
        ret = tsk_trace_error(TSK_ERR_VARIANT_CANT_DECODE_COPY);
        goto out;
    }

    ret = tsk_treeseq_get_site(self->tree_sequence, site_id, &self->site);
    if (ret != 0) {
        goto out;
    }

    ret = tsk_tree_seek(&self->tree, self->site.position, 0);
    if (ret != 0) {
        goto out;
    }

    /* When we have no specified samples we need sample lists to be active
     * on the tree, as indicated by the presence of left_sample */
    if (!by_traversal && self->tree.left_sample == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_SAMPLE_LISTS);
        goto out;
    }

    /* For now we use a traversal method to find genotypes when we have a
     * specified set of samples, but we should provide the option to do it
     * via tracked_samples in the tree also. There will be a tradeoff: if
     * we only have a small number of samples, it's probably better to
     * do it by traversal. For large sets of samples though, it may be
     * better to use the sample list infrastructure. */

    mark_missing = tsk_variant_mark_missing;
    update_genotypes = tsk_variant_update_genotypes_sample_list;
    if (by_traversal) {
        update_genotypes = tsk_variant_update_genotypes_traversal;
        /* When decoding a user-provided list of nodes (which may include
         * non-samples), mark isolated nodes as missing directly by checking
         * isolation status for each requested node. */
        mark_missing = tsk_variant_mark_missing_any;
    }

    if (self->user_alleles) {
        allele_index = tsk_variant_get_allele_index(
            self, self->site.ancestral_state, self->site.ancestral_state_length);
        if (allele_index == -1) {
            ret = tsk_trace_error(TSK_ERR_ALLELE_NOT_FOUND);
            goto out;
        }
    } else {
        /* Ancestral state is always allele 0 */
        self->alleles[0] = self->site.ancestral_state;
        self->allele_lengths[0] = self->site.ancestral_state_length;
        self->num_alleles = 1;
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
    for (j = 0; j < self->num_samples; j++) {
        self->genotypes[j] = (int32_t) allele_index;
    }

    /* We mark missing data *before* updating the genotypes because
     * mutations directly over samples should not be missing */
    num_missing = 0;
    if (!impute_missing) {
        num_missing = mark_missing(self);
    }
    for (j = 0; j < self->site.mutations_length; j++) {
        mutation = self->site.mutations[j];
        /* Compute the allele index for this derived state value. */
        allele_index = tsk_variant_get_allele_index(
            self, mutation.derived_state, mutation.derived_state_length);
        if (allele_index == -1) {
            if (self->user_alleles) {
                ret = tsk_trace_error(TSK_ERR_ALLELE_NOT_FOUND);
                goto out;
            }
            if (self->num_alleles == self->max_alleles) {
                ret = tsk_variant_expand_alleles(self);
                if (ret != 0) {
                    goto out;
                }
            }
            allele_index = (tsk_id_t) self->num_alleles;
            self->alleles[allele_index] = mutation.derived_state;
            self->allele_lengths[allele_index] = mutation.derived_state_length;
            self->num_alleles++;
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
    self->has_missing_data = num_missing > 0;
out:
    return ret;
}

int
tsk_variant_restricted_copy(const tsk_variant_t *self, tsk_variant_t *other)
{
    int ret = 0;
    tsk_size_t total_len, offset, j;

    /* Copy everything */
    tsk_memcpy(other, self, sizeof(*other));
    /* Tree sequence left as NULL and zero'd tree is a way of indicating this variant is
     * fixed and cannot be further decoded. */
    other->tree_sequence = NULL;
    tsk_memset(&other->tree, sizeof(other->tree), 0);
    other->traversal_stack = NULL;
    other->samples = NULL;
    other->sample_index_map = NULL;
    other->alt_samples = NULL;
    other->alt_sample_index_map = NULL;
    other->user_alleles_mem = NULL;

    total_len = 0;
    for (j = 0; j < self->num_alleles; j++) {
        total_len += self->allele_lengths[j];
    }
    other->samples = tsk_malloc(other->num_samples * sizeof(*other->samples));
    other->genotypes = tsk_malloc(other->num_samples * sizeof(*other->genotypes));
    other->user_alleles_mem = tsk_malloc(total_len * sizeof(*other->user_alleles_mem));
    other->allele_lengths
        = tsk_malloc(other->num_alleles * sizeof(*other->allele_lengths));
    other->alleles = tsk_malloc(other->num_alleles * sizeof(*other->alleles));
    if (other->samples == NULL || other->genotypes == NULL
        || other->user_alleles_mem == NULL || other->allele_lengths == NULL
        || other->alleles == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memcpy(
        other->samples, self->samples, other->num_samples * sizeof(*other->samples));
    tsk_memcpy(other->genotypes, self->genotypes,
        other->num_samples * sizeof(*other->genotypes));
    tsk_memcpy(other->allele_lengths, self->allele_lengths,
        other->num_alleles * sizeof(*other->allele_lengths));
    offset = 0;
    for (j = 0; j < other->num_alleles; j++) {
        tsk_memcpy(other->user_alleles_mem + offset, self->alleles[j],
            other->allele_lengths[j] * sizeof(*other->user_alleles_mem));
        other->alleles[j] = other->user_alleles_mem + offset;
        offset += other->allele_lengths[j];
    }

out:
    return ret;
}

int
tsk_vargen_next(tsk_vargen_t *self, tsk_variant_t **variant)
{
    int ret = 0;

    if ((tsk_size_t) self->site_index < tsk_treeseq_get_num_sites(self->tree_sequence)) {
        ret = tsk_variant_decode(&self->variant, self->site_index, 0);
        if (ret != 0) {
            goto out;
        }
        self->site_index++;
        *variant = &self->variant;
        ret = 1;
    }
out:
    return ret;
}

static int
tsk_treeseq_decode_alignments_overlay_missing(const tsk_treeseq_t *self,
    const tsk_id_t *nodes, tsk_size_t num_nodes, double left, double right,
    char missing_data_character, tsk_size_t L, char *alignments_out)
{
    int ret = 0;
    tsk_tree_t tree;
    tsk_size_t i, seg_left, seg_right;
    char *row = NULL;
    tsk_id_t u;

    tsk_memset(&tree, 0, sizeof(tree));

    ret = tsk_tree_init(&tree, self, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_tree_seek(&tree, left, 0);
    if (ret != 0) {
        goto out;
    }
    while (tree.index != -1 && tree.interval.left < right) {
        seg_left = TSK_MAX((tsk_size_t) tree.interval.left, (tsk_size_t) left);
        seg_right = TSK_MIN((tsk_size_t) tree.interval.right, (tsk_size_t) right);
        if (seg_right > seg_left) {
            for (i = 0; i < num_nodes; i++) {
                u = nodes[i];
                if (tree.parent[u] == TSK_NULL && tree.left_child[u] == TSK_NULL) {
                    row = alignments_out + i * L;
                    /* memset takes an `int`, `missing_data_character` is a `char` which
                     * can be signed or unsigned depending on the platform, so we need to
                     * cast. Some tools/compilers will warn if we just cast
                     * to `unsigned char` and leave the cast to `int` as implicit, hence
                     * the double cast. */
                    tsk_memset(row + (seg_left - (tsk_size_t) left),
                        (int) (unsigned char) missing_data_character,
                        seg_right - seg_left);
                }
            }
        }
        ret = tsk_tree_next(&tree);
        if (ret < 0) {
            goto out;
        }
    }

    /* On success we should return 0, not TSK_TREE_OK from the last tsk_tree_next */
    ret = 0;
out:
    tsk_tree_free(&tree);
    return ret;
}

static int
tsk_treeseq_decode_alignments_overlay_sites(const tsk_treeseq_t *self,
    const tsk_id_t *nodes, tsk_size_t num_nodes, double left, double right,
    char missing_data_character, tsk_size_t L, char *alignments_out, tsk_flags_t options)
{
    int ret = 0;
    tsk_variant_t var;
    tsk_id_t site_id;
    tsk_site_t site;
    char *allele_byte = NULL;
    tsk_size_t allele_cap = 0;
    tsk_size_t i, j;
    char *row = NULL;
    int32_t g;
    char c;
    char *tmp = NULL;

    tsk_memset(&var, 0, sizeof(var));

    ret = tsk_variant_init(&var, self, nodes, num_nodes, NULL, options);
    if (ret != 0) {
        goto out;
    }
    for (site_id = 0; site_id < (tsk_id_t) tsk_treeseq_get_num_sites(self); site_id++) {
        ret = tsk_treeseq_get_site(self, site_id, &site);
        if (ret != 0) {
            goto out;
        }
        if (site.position < left) {
            continue;
        }
        if (site.position >= right) {
            break;
        }
        ret = tsk_variant_decode(&var, site_id, 0);
        if (ret != 0) {
            goto out;
        }
        if (var.num_alleles > 0) {
            if (var.num_alleles > allele_cap) {
                tmp = tsk_realloc(allele_byte, var.num_alleles * sizeof(*allele_byte));
                if (tmp == NULL) {
                    ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
                    goto out;
                }
                allele_byte = tmp;
                allele_cap = var.num_alleles;
            }
            for (j = 0; j < var.num_alleles; j++) {
                if (var.allele_lengths[j] != 1) {
                    ret = tsk_trace_error(TSK_ERR_BAD_ALLELE_LENGTH);
                    goto out;
                }
                allele_byte[j] = var.alleles[j][0];
                if (allele_byte[j] == missing_data_character) {
                    ret = tsk_trace_error(TSK_ERR_MISSING_CHAR_COLLISION);
                    goto out;
                }
            }
            for (i = 0; i < num_nodes; i++) {
                row = alignments_out + i * L;
                g = var.genotypes[i];
                c = missing_data_character;
                if (g != TSK_MISSING_DATA) {
                    tsk_bug_assert(g >= 0);
                    tsk_bug_assert((tsk_size_t) g < var.num_alleles);
                    c = allele_byte[g];
                }
                row[((tsk_size_t) site.position) - (tsk_size_t) left] = (char) c;
            }
        }
    }

out:
    tsk_safe_free(allele_byte);
    tsk_variant_free(&var);
    return ret;
}

/* NOTE: We usually keep functions with a tsk_treeseq_t signature in trees.c.
 * tsk_treeseq_decode_alignments is implemented here instead because it
 * depends directly on tsk_variant_t and the genotype/allele machinery in
 * this file (and thus on genotypes.h). This slightly breaks that layering
 * convention but keeps the implementation close to the variant code. */
int
tsk_treeseq_decode_alignments(const tsk_treeseq_t *self, const char *ref_seq,
    tsk_size_t ref_seq_length, const tsk_id_t *nodes, tsk_size_t num_nodes, double left,
    double right, char missing_data_character, char *alignments_out, tsk_flags_t options)
{
    int ret = 0;
    tsk_size_t i, L;
    char *row = NULL;

    if (!tsk_treeseq_get_discrete_genome(self)) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    if (ref_seq == NULL) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    if (ref_seq_length != (tsk_size_t) tsk_treeseq_get_sequence_length(self)) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    if (trunc(left) != left || trunc(right) != right) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    if (left < 0 || right > tsk_treeseq_get_sequence_length(self)
        || (tsk_size_t) left >= (tsk_size_t) right) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    L = (tsk_size_t) right - (tsk_size_t) left;
    if (num_nodes == 0) {
        return 0;
    }
    if (nodes == NULL || alignments_out == NULL) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    for (i = 0; i < num_nodes; i++) {
        if (nodes[i] < 0 || nodes[i] >= (tsk_id_t) tsk_treeseq_get_num_nodes(self)) {
            ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
            goto out;
        }
    }

    /* Fill rows with the reference slice */
    for (i = 0; i < num_nodes; i++) {
        row = alignments_out + i * L;
        tsk_memcpy(row, ref_seq + (tsk_size_t) left, L);
    }
    if (!(options & TSK_ISOLATED_NOT_MISSING)) {
        ret = tsk_treeseq_decode_alignments_overlay_missing(self, nodes, num_nodes, left,
            right, missing_data_character, L, alignments_out);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_treeseq_decode_alignments_overlay_sites(self, nodes, num_nodes, left,
        right, missing_data_character, L, alignments_out, options);
    if (ret != 0) {
        goto out;
    }

out:
    return ret;
}
