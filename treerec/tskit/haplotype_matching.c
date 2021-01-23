/*
 * MIT License
 *
 * Copyright (c) 2019-2020 Tskit Developers
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
#include <float.h>
#include <assert.h>

#include <tskit/haplotype_matching.h>

const char *_zero_one_alleles[] = { "0", "1", NULL };
const char *_acgt_alleles[] = { "A", "C", "G", "T", NULL };

static int
cmp_double(const void *a, const void *b)
{
    const double *ia = (const double *) a;
    const double *ib = (const double *) b;
    return (*ia > *ib) - (*ia < *ib);
}

static int
cmp_argsort(const void *a, const void *b)
{
    const tsk_argsort_t *ia = (const tsk_argsort_t *) a;
    const tsk_argsort_t *ib = (const tsk_argsort_t *) b;
    int ret = (ia->value > ib->value) - (ia->value < ib->value);
    /* Break any ties using the index to ensure consistency */
    if (ret == 0) {
        ret = (ia->index > ib->index) - (ia->index < ib->index);
    }
    return ret;
}

static void
tsk_ls_hmm_check_state(tsk_ls_hmm_t *self)
{
    tsk_id_t *T_index = self->transition_index;
    tsk_value_transition_t *T = self->transitions;
    tsk_id_t j;

    for (j = 0; j < (tsk_id_t) self->num_transitions; j++) {
        if (T[j].tree_node != TSK_NULL) {
            tsk_bug_assert(T_index[T[j].tree_node] == j);
        }
    }
    /* tsk_bug_assert(self->num_transitions <= self->num_samples); */

    if (self->num_transitions > 0) {
        for (j = 0; j < (tsk_id_t) self->num_nodes; j++) {
            if (T_index[j] != TSK_NULL) {
                tsk_bug_assert(T[T_index[j]].tree_node == j);
            }
            tsk_bug_assert(self->tree.parent[j] == self->parent[j]);
        }
    }
}

void
tsk_ls_hmm_print_state(tsk_ls_hmm_t *self, FILE *out)
{
    tsk_size_t j, l;

    fprintf(out, "tree_sequence   = %p\n", (void *) self->tree_sequence);
    fprintf(out, "num_sites       = %d\n", self->num_sites);
    fprintf(out, "num_samples     = %d\n", self->num_samples);
    fprintf(out, "num_values      = %d\n", (int) self->num_values);
    fprintf(out, "max_values      = %d\n", (int) self->max_values);
    fprintf(out, "num_optimal_value_set_words = %d\n",
        (int) self->num_optimal_value_set_words);

    fprintf(out, "sites::\n");
    for (l = 0; l < self->num_sites; l++) {
        fprintf(out, "%d\t%d\t[", l, self->num_alleles[l]);
        for (j = 0; j < self->num_alleles[l]; j++) {
            fprintf(out, "%s,", self->alleles[l][j]);
        }
        fprintf(out, "]\n");
    }
    fprintf(out, "transitions::%d\n", (int) self->num_transitions);
    for (j = 0; j < self->num_transitions; j++) {
        fprintf(out, "tree_node=%d\tvalue=%.14f\tvalue_index=%d\n",
            self->transitions[j].tree_node, self->transitions[j].value,
            self->transitions[j].value_index);
    }
    if (self->num_transitions > 0) {
        fprintf(out, "tree::%d\n", (int) self->num_nodes);
        for (j = 0; j < self->num_nodes; j++) {
            fprintf(out, "%d\tparent=%d\ttransition=%d\n", j, self->parent[j],
                self->transition_index[j]);
        }
    }
    tsk_ls_hmm_check_state(self);
}

int TSK_WARN_UNUSED
tsk_ls_hmm_init(tsk_ls_hmm_t *self, tsk_treeseq_t *tree_sequence,
    double *recombination_rate, double *mutation_rate, tsk_flags_t options)
{
    int ret = TSK_ERR_GENERIC;
    tsk_size_t l;

    memset(self, 0, sizeof(tsk_ls_hmm_t));
    self->tree_sequence = tree_sequence;
    self->precision = 6; /* Seems like a safe value, but probably not ideal for perf */
    self->num_sites = tsk_treeseq_get_num_sites(tree_sequence);
    self->num_samples = tsk_treeseq_get_num_samples(tree_sequence);
    self->num_alleles = malloc(self->num_sites * sizeof(*self->num_alleles));
    self->num_nodes = tsk_treeseq_get_num_nodes(tree_sequence);
    self->parent = malloc(self->num_nodes * sizeof(*self->parent));
    self->allelic_state = malloc(self->num_nodes * sizeof(*self->allelic_state));
    self->transition_index = malloc(self->num_nodes * sizeof(*self->transition_index));
    self->transition_stack = malloc(self->num_nodes * sizeof(*self->transition_stack));
    /* We can't have more than 2 * num_samples transitions, so we use this as the
     * upper bound. Because of the implementation, we'll also have to worry about
     * the extra mutations at the first site, which in worst case involves all
     * mutations. We can definitely save some memory here if we want to.*/
    self->max_transitions
        = 2 * self->num_samples + tsk_treeseq_get_num_mutations(tree_sequence);
    /* FIXME Arbitrarily doubling this after hitting problems */
    self->max_transitions *= 2;
    self->transitions = malloc(self->max_transitions * sizeof(*self->transitions));
    self->transitions_copy = malloc(self->max_transitions * sizeof(*self->transitions));
    self->num_transition_samples
        = malloc(self->max_transitions * sizeof(*self->num_transition_samples));
    self->transition_parent
        = malloc(self->max_transitions * sizeof(*self->transition_parent));
    self->transition_time_order
        = malloc(self->max_transitions * sizeof(*self->transition_time_order));
    self->values = malloc(self->max_transitions * sizeof(*self->values));
    self->recombination_rate
        = malloc(self->num_sites * sizeof(*self->recombination_rate));
    self->mutation_rate = malloc(self->num_sites * sizeof(*self->mutation_rate));
    self->alleles = calloc(self->num_sites, sizeof(*self->alleles));
    if (self->num_alleles == NULL || self->parent == NULL || self->allelic_state == NULL
        || self->transition_index == NULL || self->transition_stack == NULL
        || self->transitions == NULL || self->transitions_copy == NULL
        || self->num_transition_samples == NULL || self->transition_parent == NULL
        || self->transition_time_order == NULL || self->values == NULL
        || self->recombination_rate == NULL || self->mutation_rate == NULL
        || self->alleles == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    for (l = 0; l < self->num_sites; l++) {
        /* TODO check these inputs */
        self->recombination_rate[l] = recombination_rate[l];
        self->mutation_rate[l] = mutation_rate[l];
        if (options & TSK_ALLELES_ACGT) {
            self->num_alleles[l] = 4;
            self->alleles[l] = _acgt_alleles;
        } else {
            /* Default to the 0/1 alleles */
            self->num_alleles[l] = 2;
            self->alleles[l] = _zero_one_alleles;
        }
    }
    ret = tsk_tree_init(&self->tree, self->tree_sequence, 0);
    if (ret != 0) {
        goto out;
    }
    self->num_values = 0;
    self->max_values = 0;
    ret = 0;
out:
    return ret;
}

int
tsk_ls_hmm_set_precision(tsk_ls_hmm_t *self, unsigned int precision)
{
    self->precision = precision;
    return 0;
}

int
tsk_ls_hmm_free(tsk_ls_hmm_t *self)
{
    tsk_tree_free(&self->tree);
    tsk_diff_iter_free(&self->diffs);
    tsk_safe_free(self->recombination_rate);
    tsk_safe_free(self->mutation_rate);
    tsk_safe_free(self->recombination_rate);
    tsk_safe_free(self->alleles);
    tsk_safe_free(self->num_alleles);
    tsk_safe_free(self->parent);
    tsk_safe_free(self->allelic_state);
    tsk_safe_free(self->transition_index);
    tsk_safe_free(self->transition_stack);
    tsk_safe_free(self->transitions);
    tsk_safe_free(self->transitions_copy);
    tsk_safe_free(self->transition_time_order);
    tsk_safe_free(self->values);
    tsk_safe_free(self->num_transition_samples);
    tsk_safe_free(self->transition_parent);
    tsk_safe_free(self->optimal_value_sets);
    return 0;
}

static int
tsk_ls_hmm_reset(tsk_ls_hmm_t *self)
{
    int ret = 0;
    double n = self->num_samples;
    tsk_size_t j;
    tsk_id_t u;
    const tsk_id_t *samples;
    tsk_size_t N = self->num_nodes;

    memset(self->parent, 0xff, N * sizeof(*self->parent));
    memset(self->transition_index, 0xff, N * sizeof(*self->transition_index));
    memset(self->allelic_state, 0xff, N * sizeof(*self->allelic_state));
    memset(self->transitions, 0, self->max_transitions * sizeof(*self->transitions));
    memset(self->num_transition_samples, 0,
        self->max_transitions * sizeof(*self->num_transition_samples));
    memset(self->transition_parent, 0xff,
        self->max_transitions * sizeof(*self->transition_parent));

    /* This is safe because we've already zero'd out the memory. */
    tsk_diff_iter_free(&self->diffs);
    ret = tsk_diff_iter_init(&self->diffs, self->tree_sequence, false);
    if (ret != 0) {
        goto out;
    }
    samples = tsk_treeseq_get_samples(self->tree_sequence);
    for (j = 0; j < self->num_samples; j++) {
        u = samples[j];
        self->transitions[j].tree_node = u;
        self->transitions[j].value = 1.0 / n;
        self->transition_index[u] = (tsk_id_t) j;
    }
    self->num_transitions = self->num_samples;
out:
    return ret;
}

/* After we have moved on to a new tree we can have transitions still associated
 * with the old roots, which are now disconnected. Remove. */
static int
tsk_ls_hmm_remove_dead_roots(tsk_ls_hmm_t *self)
{
    tsk_id_t *restrict T_index = self->transition_index;
    tsk_value_transition_t *restrict T = self->transitions;
    const tsk_id_t *restrict right_sib = self->tree.right_sib;
    const tsk_id_t left_root = self->tree.left_root;
    const tsk_id_t *restrict parent = self->parent;
    tsk_id_t root, u;
    size_t j;
    const tsk_id_t root_marker = -2;

    for (root = left_root; root != TSK_NULL; root = right_sib[root]) {
        if (T_index[root] != TSK_NULL) {
            /* Use the value_index slot as a marker. We don't use this between
             * iterations, so it's safe to appropriate here */
            T[T_index[root]].value_index = root_marker;
        }
    }
    for (j = 0; j < self->num_transitions; j++) {
        u = T[j].tree_node;
        if (u != TSK_NULL) {
            if (parent[u] == TSK_NULL && T[j].value_index != root_marker) {
                T_index[u] = TSK_NULL;
                T[j].tree_node = TSK_NULL;
            }
            T[j].value_index = -1;
        }
    }
    return 0;
}

static int
tsk_ls_hmm_update_tree(tsk_ls_hmm_t *self)
{
    int ret = 0;
    tsk_id_t *restrict parent = self->parent;
    tsk_id_t *restrict T_index = self->transition_index;
    tsk_value_transition_t *restrict T = self->transitions;
    tsk_edge_list_node_t *record;
    tsk_edge_list_t records_out, records_in;
    tsk_edge_t edge;
    double left, right;
    tsk_id_t u;
    tsk_value_transition_t *vt;

    ret = tsk_diff_iter_next(&self->diffs, &left, &right, &records_out, &records_in);
    if (ret < 0) {
        goto out;
    }

    for (record = records_out.head; record != NULL; record = record->next) {
        u = record->edge.child;
        if (T_index[u] == TSK_NULL) {
            /* Ensure the subtree we're detaching has a transition at the root */
            while (T_index[u] == TSK_NULL) {
                u = parent[u];
                tsk_bug_assert(u != TSK_NULL);
            }
            tsk_bug_assert(self->num_transitions < self->max_transitions);
            T_index[record->edge.child] = (tsk_id_t) self->num_transitions;
            T[self->num_transitions].tree_node = record->edge.child;
            T[self->num_transitions].value = T[T_index[u]].value;
            self->num_transitions++;
        }
        parent[record->edge.child] = TSK_NULL;
    }

    for (record = records_in.head; record != NULL; record = record->next) {
        edge = record->edge;
        parent[edge.child] = edge.parent;
        u = edge.parent;
        if (parent[edge.parent] == TSK_NULL) {
            /* Grafting onto a new root. */
            if (T_index[record->edge.parent] == TSK_NULL) {
                T_index[edge.parent] = (tsk_id_t) self->num_transitions;
                tsk_bug_assert(self->num_transitions < self->max_transitions);
                T[self->num_transitions].tree_node = edge.parent;
                T[self->num_transitions].value = T[T_index[edge.child]].value;
                self->num_transitions++;
            }
        } else {
            /* Grafting into an existing subtree. */
            while (T_index[u] == TSK_NULL) {
                u = parent[u];
            }
            tsk_bug_assert(u != TSK_NULL);
        }
        tsk_bug_assert(T_index[u] != -1 && T_index[edge.child] != -1);
        if (T[T_index[u]].value == T[T_index[edge.child]].value) {
            vt = &T[T_index[edge.child]];
            /* Mark the value transition as unusued */
            vt->value = -1;
            vt->tree_node = TSK_NULL;
            T_index[edge.child] = TSK_NULL;
        }
    }

    ret = tsk_ls_hmm_remove_dead_roots(self);
out:
    return ret;
}

static int
tsk_ls_hmm_get_allele_index(tsk_ls_hmm_t *self, tsk_id_t site, const char *allele_state,
    const size_t allele_length)
{
    int ret = TSK_ERR_ALLELE_NOT_FOUND;
    const char **alleles = self->alleles[site];
    const tsk_id_t num_alleles = self->num_alleles[site];

    tsk_id_t j;

    for (j = 0; j < num_alleles; j++) {
        if (strlen(alleles[j]) != allele_length) {
            break;
        }
        if (strncmp(alleles[j], allele_state, allele_length) == 0) {
            ret = j;
            break;
        }
    }
    return ret;
}

static int
tsk_ls_hmm_update_probabilities(
    tsk_ls_hmm_t *self, const tsk_site_t *site, int8_t haplotype_state)
{
    int ret = 0;
    tsk_id_t root;
    tsk_tree_t *tree = &self->tree;
    tsk_id_t *restrict parent = self->parent;
    tsk_id_t *restrict T_index = self->transition_index;
    tsk_value_transition_t *restrict T = self->transitions;
    int8_t *restrict allelic_state = self->allelic_state;
    tsk_mutation_t mut;
    tsk_id_t j, u, v;
    double x;
    bool match;

    /* Set the allelic states */
    ret = tsk_ls_hmm_get_allele_index(
        self, site->id, site->ancestral_state, site->ancestral_state_length);
    if (ret < 0) {
        goto out;
    }
    for (root = tree->left_root; root != TSK_NULL; root = tree->right_sib[root]) {
        allelic_state[root] = (int8_t) ret;
    }

    for (j = 0; j < (tsk_id_t) site->mutations_length; j++) {
        mut = site->mutations[j];
        ret = tsk_ls_hmm_get_allele_index(
            self, site->id, mut.derived_state, mut.derived_state_length);
        if (ret < 0) {
            goto out;
        }
        u = mut.node;
        allelic_state[u] = (int8_t) ret;
        if (T_index[u] == TSK_NULL) {
            while (T_index[u] == TSK_NULL) {
                u = parent[u];
            }
            tsk_bug_assert(self->num_transitions < self->max_transitions);
            T_index[mut.node] = (tsk_id_t) self->num_transitions;
            T[self->num_transitions].tree_node = mut.node;
            T[self->num_transitions].value = T[T_index[u]].value;
            self->num_transitions++;
        }
    }

    for (j = 0; j < (tsk_id_t) self->num_transitions; j++) {
        u = T[j].tree_node;
        if (u != TSK_NULL) {
            /* Get the allelic_state at u. */
            v = u;
            while (allelic_state[v] == TSK_MISSING_DATA) {
                v = parent[v];
                tsk_bug_assert(v != -1);
            }
            match = haplotype_state == TSK_MISSING_DATA
                    || haplotype_state == allelic_state[v];
            ret = self->next_probability(self, site->id, T[j].value, match, u, &x);
            if (ret != 0) {
                goto out;
            }
            T[j].value = x;
        }
    }

    /* Unset the allelic states */
    for (root = tree->left_root; root != TSK_NULL; root = tree->right_sib[root]) {
        allelic_state[root] = TSK_MISSING_DATA;
    }
    for (j = 0; j < (tsk_id_t) site->mutations_length; j++) {
        mut = site->mutations[j];
        allelic_state[mut.node] = TSK_MISSING_DATA;
    }
    ret = 0;
out:
    return ret;
}

static int
tsk_ls_hmm_discretise_values(tsk_ls_hmm_t *self)
{
    int ret = 0;
    tsk_value_transition_t *T = self->transitions;
    double *values = self->values;
    size_t j, k, num_values;

    num_values = 0;
    for (j = 0; j < self->num_transitions; j++) {
        if (T[j].tree_node != TSK_NULL) {
            values[num_values] = T[j].value;
            num_values++;
        }
    }
    tsk_bug_assert(num_values > 0);

    qsort(values, num_values, sizeof(double), cmp_double);

    k = 0;
    for (j = 1; j < num_values; j++) {
        if (values[j] != values[k]) {
            k++;
            values[k] = values[j];
        }
    }
    num_values = k + 1;
    self->num_values = num_values;
    for (j = 0; j < self->num_transitions; j++) {
        if (T[j].tree_node != TSK_NULL) {
            T[j].value_index
                = (tsk_id_t) tsk_search_sorted(values, num_values, T[j].value);
            tsk_bug_assert(T[j].value == self->values[T[j].value_index]);
        }
    }
    return ret;
}

/*
 * TODO We also have these function in tree.c where they're used in the
 * parsimony calculations (which are slightly different). It would be good to bring
 * these together, or at least avoid having the same function in two
 * files. Keeping it as it is for now so that it can be inlined, since
 * it's perf-sensitive. */

static inline tsk_id_t
get_smallest_set_bit(uint64_t v)
{
    /* This is an inefficient implementation, there are several better
     * approaches. On GCC we can use
     * return (uint8_t) (__builtin_ffsll((long long) v) - 1);
     */
    uint64_t t = 1;
    tsk_id_t r = 0;
    assert(v != 0);

    while ((v & t) == 0) {
        t <<= 1;
        r++;
    }
    return r;
}

static inline uint64_t
set_bit(uint64_t value, uint8_t bit)
{
    return value | (1ULL << bit);
}

static inline bool
bit_is_set(uint64_t value, uint8_t bit)
{
    return (value & (1ULL << bit)) != 0;
}

static inline tsk_id_t
get_smallest_element(const uint64_t *restrict A, size_t u, size_t num_words)
{
    size_t base = (size_t) u * num_words;
    const uint64_t *restrict a = A + base;
    tsk_id_t j = 0;

    while (a[j] == 0) {
        j++;
        tsk_bug_assert(j < (tsk_id_t) num_words);
    }
    return j * 64 + get_smallest_set_bit(a[j]);
}

#define MAX_PARSIMONY_WORDS 256
/* static variables are zero-initialised by default. */
static const uint64_t zero_block[MAX_PARSIMONY_WORDS];

static inline bool
all_zero(const uint64_t *restrict A, size_t u, size_t num_words)
{
    if (num_words == 1) {
        return A[u] == 0;
    } else {
        return memcmp(zero_block, A + u * num_words, num_words * sizeof(*A)) == 0;
    }
}

static inline bool
element_in(const uint64_t *restrict A, size_t u, const tsk_id_t state, size_t num_words)
{
    size_t index = ((size_t) u) * num_words + (size_t)(state / 64);
    return (A[index] & (1ULL << (state % 64))) != 0;
}

static inline void
set_optimal_value(
    uint64_t *restrict A, const tsk_id_t u, const size_t num_words, tsk_id_t state)
{
    size_t index = ((size_t) u) * num_words + (size_t)(state / 64);
    tsk_bug_assert(((size_t) state) / 64 < num_words);
    A[index] |= 1ULL << (state % 64);
}

/* TODO the implementation here isn't particularly optimal and the way things
 * were organised was really driven by the old Fitch parsimony algorithm
 * (which only worked on binary trees. In particular, we should be working
 * word-by-word where possible rather than iterating by values like we do here.
 * Needs to be reworked when we're documenting/writing up this algorithm.
 */

static void
compute_optimal_value_1(uint64_t *restrict A, const tsk_id_t *restrict left_child,
    const tsk_id_t *restrict right_sib, const tsk_id_t u, const tsk_id_t parent_state,
    const size_t num_values)
{
    tsk_id_t v;
    uint64_t child;
    tsk_size_t value_count[64], max_value_count;
    uint8_t j;

    assert(num_values < 64);

    memset(value_count, 0, num_values * sizeof(*value_count));
    for (v = left_child[u]; v != TSK_NULL; v = right_sib[v]) {
        child = A[v];
        /* If the set for a given child is empty, then we know it inherits
         * directly from the parent state and must be a singleton set. */
        if (child == 0) {
            child = 1ULL << parent_state;
        }
        for (j = 0; j < num_values; j++) {
            value_count[j] += bit_is_set(child, j);
        }
    }
    max_value_count = 0;
    for (j = 0; j < num_values; j++) {
        max_value_count = TSK_MAX(max_value_count, value_count[j]);
    }
    A[u] = 0;
    for (j = 0; j < num_values; j++) {
        if (value_count[j] == max_value_count) {
            A[u] = set_bit(A[u], j);
        }
    }
}

static void
compute_optimal_value_general(uint64_t *restrict A, const tsk_id_t *restrict left_child,
    const tsk_id_t *restrict right_sib, const tsk_id_t u, const tsk_id_t parent_state,
    const size_t num_values, const size_t num_words)
{
    tsk_id_t v;
    uint64_t child[MAX_PARSIMONY_WORDS];
    uint64_t *Au;
    size_t base, word, bit;
    bool child_all_zero;
    const tsk_id_t state_index = parent_state / 64;
    const uint64_t state_word = 1ULL << (parent_state % 64);
    tsk_size_t value_count[64 * MAX_PARSIMONY_WORDS], max_value_count;
    size_t j;

    tsk_bug_assert(num_values < 64 * MAX_PARSIMONY_WORDS);
    tsk_bug_assert(num_words <= MAX_PARSIMONY_WORDS);
    for (j = 0; j < num_values; j++) {
        value_count[j] = 0;
    }

    for (v = left_child[u]; v != TSK_NULL; v = right_sib[v]) {
        child_all_zero = true;
        base = ((size_t) v) * num_words;
        for (word = 0; word < num_words; word++) {
            child[word] = A[base + word];
            child_all_zero = child_all_zero && (child[word] == 0);
        }
        /* If the set for a given child is empty, then we know it inherits
         * directly from the parent state and must be a singleton set. */
        if (child_all_zero) {
            child[state_index] = state_word;
        }
        for (j = 0; j < num_values; j++) {
            word = j / 64;
            bit = j % 64;
            assert(word < num_words);
            value_count[j] += bit_is_set(child[word], (uint8_t) bit);
        }
    }
    max_value_count = 0;
    for (j = 0; j < num_values; j++) {
        max_value_count = TSK_MAX(max_value_count, value_count[j]);
    }

    Au = A + ((size_t) u * num_words);
    for (word = 0; word < num_words; word++) {
        Au[word] = 0;
    }
    for (j = 0; j < num_values; j++) {
        if (value_count[j] == max_value_count) {
            word = j / 64;
            bit = j % 64;
            Au[word] = set_bit(Au[word], (uint8_t) bit);
        }
    }
}

static void
compute_optimal_value(uint64_t *restrict A, const tsk_id_t *restrict left_child,
    const tsk_id_t *restrict right_sib, const tsk_id_t u, const tsk_id_t parent_state,
    const size_t num_values, const size_t num_words)
{
    if (num_words == 1) {
        compute_optimal_value_1(A, left_child, right_sib, u, parent_state, num_values);
    } else {
        compute_optimal_value_general(
            A, left_child, right_sib, u, parent_state, num_values, num_words);
    }
}

static int
tsk_ls_hmm_setup_optimal_value_sets(tsk_ls_hmm_t *self)
{
    int ret = 0;

    /* We expect that most of the time there will be one word per optimal_value set,
     * but there will be times when we need more than one word. This approach
     * lets us expand the memory if we need to, but when the number of
     * values goes back below 64 we revert to using one word per set. We
     * could in principle release back the memory as well, but it doesn't seem
     * worth the bother. */
    self->num_optimal_value_set_words = (self->num_values / 64) + 1;
    if (self->num_optimal_value_set_words > MAX_PARSIMONY_WORDS) {
        ret = TSK_ERR_TOO_MANY_VALUES;
        goto out;
    }
    if (self->num_values >= self->max_values) {
        self->max_values = self->num_optimal_value_set_words * 64;
        tsk_safe_free(self->optimal_value_sets);
        self->optimal_value_sets
            = calloc(self->num_nodes * self->num_optimal_value_set_words,
                sizeof(*self->optimal_value_sets));
        if (self->optimal_value_sets == NULL) {
            ret = TSK_ERR_NO_MEMORY;
            goto out;
        }
    }
out:
    return ret;
}

static int
tsk_ls_hmm_build_optimal_value_sets(tsk_ls_hmm_t *self)
{
    int ret = 0;
    const double *restrict node_time = self->tree_sequence->tables->nodes.time;
    const tsk_id_t *restrict left_child = self->tree.left_child;
    const tsk_id_t *restrict right_sib = self->tree.right_sib;
    const tsk_id_t *restrict parent = self->parent;
    const tsk_value_transition_t *restrict T = self->transitions;
    const tsk_id_t *restrict T_index = self->transition_index;
    tsk_argsort_t *restrict order = self->transition_time_order;
    const size_t num_optimal_value_set_words = self->num_optimal_value_set_words;
    uint64_t *restrict A = self->optimal_value_sets;
    size_t j;
    tsk_id_t u, v, state, parent_state;

    /* argsort the transitions by node time so we can visit them in the
     * correct order */
    for (j = 0; j < self->num_transitions; j++) {
        order[j].index = j;
        order[j].value = DBL_MAX;
        if (T[j].tree_node != TSK_NULL) {
            order[j].value = node_time[T[j].tree_node];
        }
    }
    qsort(order, self->num_transitions, sizeof(*order), cmp_argsort);

    for (j = 0; j < self->num_transitions; j++) {
        u = T[order[j].index].tree_node;
        if (u != TSK_NULL) {
            state = T[order[j].index].value_index;
            if (left_child[u] == TSK_NULL) {
                /* leaf node */
                set_optimal_value(A, u, num_optimal_value_set_words, state);
            } else {
                compute_optimal_value(A, left_child, right_sib, u, state,
                    self->num_values, num_optimal_value_set_words);
            }
            v = parent[u];
            if (v != TSK_NULL) {
                while (T_index[v] == TSK_NULL) {
                    v = parent[v];
                    tsk_bug_assert(v != TSK_NULL);
                }
                parent_state = T[T_index[v]].value_index;
                v = parent[u];
                while (T_index[v] == TSK_NULL) {
                    compute_optimal_value(A, left_child, right_sib, v, parent_state,
                        self->num_values, num_optimal_value_set_words);
                    v = parent[v];
                    tsk_bug_assert(v != TSK_NULL);
                }
            }
        }
    }
    return ret;
}

static int
tsk_ls_hmm_redistribute_transitions(tsk_ls_hmm_t *self)
{
    int ret = 0;
    const tsk_id_t *restrict left_child = self->tree.left_child;
    const tsk_id_t *restrict right_sib = self->tree.right_sib;
    const tsk_id_t *restrict parent = self->parent;
    tsk_id_t *restrict T_index = self->transition_index;
    tsk_id_t *restrict T_parent = self->transition_parent;
    tsk_value_transition_t *restrict T = self->transitions;
    tsk_value_transition_t *restrict T_old = self->transitions_copy;
    tsk_transition_stack_t *stack = self->transition_stack;
    uint64_t *restrict A = self->optimal_value_sets;
    const size_t num_optimal_value_set_words = self->num_optimal_value_set_words;
    tsk_transition_stack_t s, child_s;
    tsk_id_t root, u, v;
    int stack_top = 0;
    size_t j, old_num_transitions;

    memcpy(T_old, T, self->num_transitions * sizeof(*T));
    old_num_transitions = self->num_transitions;
    self->num_transitions = 0;

    for (root = self->tree.left_root; root != TSK_NULL; root = right_sib[root]) {
        stack[0].tree_node = root;
        stack[0].old_state = T_old[T_index[root]].value_index;
        stack[0].new_state
            = get_smallest_element(A, (size_t) root, num_optimal_value_set_words);
        stack[0].transition_parent = 0;
        stack_top = 0;

        tsk_bug_assert(self->num_transitions < self->max_transitions);
        T_parent[self->num_transitions] = TSK_NULL;
        T[self->num_transitions].tree_node = stack[0].tree_node;
        T[self->num_transitions].value_index = stack[0].new_state;
        self->num_transitions++;

        while (stack_top >= 0) {
            s = stack[stack_top];
            stack_top--;
            for (v = left_child[s.tree_node]; v != TSK_NULL; v = right_sib[v]) {
                child_s = s;
                child_s.tree_node = v;
                if (T_index[v] != TSK_NULL) {
                    child_s.old_state = T_old[T_index[v]].value_index;
                }
                if (!all_zero(A, (size_t) v, num_optimal_value_set_words)) {
                    if (!element_in(
                            A, (size_t) v, s.new_state, num_optimal_value_set_words)) {
                        child_s.new_state = get_smallest_element(
                            A, (size_t) v, num_optimal_value_set_words);
                        child_s.transition_parent = (tsk_id_t) self->num_transitions;
                        /* Add a new transition */
                        tsk_bug_assert(self->num_transitions < self->max_transitions);
                        T_parent[self->num_transitions] = s.transition_parent;
                        T[self->num_transitions].tree_node = v;
                        T[self->num_transitions].value_index = child_s.new_state;
                        self->num_transitions++;
                    }
                    stack_top++;
                    stack[stack_top] = child_s;
                } else {
                    /* Node that we didn't visit when moving up the tree */
                    if (s.old_state != s.new_state) {
                        tsk_bug_assert(self->num_transitions < self->max_transitions);
                        T_parent[self->num_transitions] = s.transition_parent;
                        T[self->num_transitions].tree_node = v;
                        T[self->num_transitions].value_index = s.old_state;
                        self->num_transitions++;
                    }
                }
            }
        }
    }

    /* Unset the old T_index pointers and optimal_value sets. */
    for (j = 0; j < old_num_transitions; j++) {
        u = T_old[j].tree_node;
        if (u != TSK_NULL) {
            T_index[u] = TSK_NULL;
            while (
                u != TSK_NULL && !all_zero(A, (size_t) u, num_optimal_value_set_words)) {
                memset(A + ((size_t) u) * num_optimal_value_set_words, 0,
                    num_optimal_value_set_words * sizeof(uint64_t));
                u = parent[u];
            }
        }
    }
    /* Set the new pointers for transition nodes and the values.*/
    for (j = 0; j < self->num_transitions; j++) {
        T_index[T[j].tree_node] = (tsk_id_t) j;
        T[j].value = self->values[T[j].value_index];
    }
    return ret;
}

static int
tsk_ls_hmm_compress(tsk_ls_hmm_t *self)
{
    int ret = 0;

    ret = tsk_ls_hmm_discretise_values(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_ls_hmm_setup_optimal_value_sets(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_ls_hmm_build_optimal_value_sets(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_ls_hmm_redistribute_transitions(self);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

static int
tsk_ls_hmm_process_site(
    tsk_ls_hmm_t *self, const tsk_site_t *site, int8_t haplotype_state)
{
    int ret = 0;
    double x, normalisation_factor;
    tsk_compressed_matrix_t *output = (tsk_compressed_matrix_t *) self->output;
    tsk_value_transition_t *restrict T = self->transitions;
    const unsigned int precision = (unsigned int) self->precision;
    tsk_size_t j;

    ret = tsk_ls_hmm_update_probabilities(self, site, haplotype_state);
    if (ret != 0) {
        goto out;
    }
    /* See notes in the Python implementation on why we don't want to compress
     * here, but rather should be doing it after rounding. */
    ret = tsk_ls_hmm_compress(self);
    if (ret != 0) {
        goto out;
    }
    tsk_bug_assert(self->num_transitions <= self->num_samples);
    normalisation_factor = self->compute_normalisation_factor(self);

    if (normalisation_factor == 0) {
        ret = TSK_ERR_MATCH_IMPOSSIBLE;
        goto out;
    }
    for (j = 0; j < self->num_transitions; j++) {
        tsk_bug_assert(T[j].tree_node != TSK_NULL);
        x = T[j].value / normalisation_factor;
        T[j].value = tsk_round(x, precision);
    }

    ret = tsk_compressed_matrix_store_site(
        output, site->id, normalisation_factor, (tsk_size_t) self->num_transitions, T);
out:
    return ret;
}

int
tsk_ls_hmm_run(tsk_ls_hmm_t *self, int8_t *haplotype,
    int (*next_probability)(tsk_ls_hmm_t *, tsk_id_t, double, bool, tsk_id_t, double *),
    double (*compute_normalisation_factor)(struct _tsk_ls_hmm_t *), void *output)
{
    int ret = 0;
    int t_ret;
    const tsk_site_t *sites;
    tsk_size_t j, num_sites;

    self->next_probability = next_probability;
    self->compute_normalisation_factor = compute_normalisation_factor;
    self->output = output;

    ret = tsk_ls_hmm_reset(self);
    if (ret != 0) {
        goto out;
    }

    for (t_ret = tsk_tree_first(&self->tree); t_ret == 1;
         t_ret = tsk_tree_next(&self->tree)) {
        ret = tsk_ls_hmm_update_tree(self);
        if (ret != 0) {
            goto out;
        }
        /* tsk_ls_hmm_check_state(self); */
        ret = tsk_tree_get_sites(&self->tree, &sites, &num_sites);
        if (ret != 0) {
            goto out;
        }
        for (j = 0; j < num_sites; j++) {
            ret = tsk_ls_hmm_process_site(self, &sites[j], haplotype[sites[j].id]);
            if (ret != 0) {
                goto out;
            }
        }
    }
    /* Set to zero so we can print and check the state OK. */
    self->num_transitions = 0;
    if (t_ret != 0) {
        ret = t_ret;
        goto out;
    }
out:
    return ret;
}

/****************************************************************
 * Forward Algorithm
 ****************************************************************/

static double
tsk_ls_hmm_compute_normalisation_factor_forward(tsk_ls_hmm_t *self)
{
    tsk_id_t *restrict N = self->num_transition_samples;
    tsk_value_transition_t *restrict T = self->transitions;
    const tsk_id_t *restrict T_parent = self->transition_parent;
    const tsk_id_t *restrict num_samples = self->tree.num_samples;
    const tsk_id_t num_transitions = (tsk_id_t) self->num_transitions;
    double normalisation_factor;
    tsk_id_t j;

    /* Compute the number of samples directly inheriting from each transition */
    for (j = 0; j < num_transitions; j++) {
        tsk_bug_assert(T[j].tree_node != TSK_NULL);
        N[j] = num_samples[T[j].tree_node];
    }
    for (j = 0; j < num_transitions; j++) {
        if (T_parent[j] != TSK_NULL) {
            N[T_parent[j]] -= N[j];
        }
    }

    /* Compute the normalising constant used to avoid underflow */
    normalisation_factor = 0;
    for (j = 0; j < num_transitions; j++) {
        normalisation_factor += N[j] * T[j].value;
    }
    return normalisation_factor;
}

static int
tsk_ls_hmm_next_probability_forward(tsk_ls_hmm_t *self, tsk_id_t site_id, double p_last,
    bool is_match, tsk_id_t TSK_UNUSED(node), double *result)
{
    const double rho = self->recombination_rate[site_id];
    const double mu = self->mutation_rate[site_id];
    const double n = self->num_samples;
    const double num_alleles = self->num_alleles[site_id];
    double p_t, p_e;

    p_t = p_last * (1 - rho) + rho / n;
    p_e = mu;
    if (is_match) {
        p_e = 1 - (num_alleles - 1) * mu;
    }
    *result = p_t * p_e;
    return 0;
}

int
tsk_ls_hmm_forward(tsk_ls_hmm_t *self, int8_t *haplotype,
    tsk_compressed_matrix_t *output, tsk_flags_t options)
{
    int ret = 0;

    if (!(options & TSK_NO_INIT)) {
        ret = tsk_compressed_matrix_init(output, self->tree_sequence, 0, 0);
        if (ret != 0) {
            goto out;
        }
    } else {
        if (output->tree_sequence != self->tree_sequence) {
            ret = TSK_ERR_BAD_PARAM_VALUE;
            goto out;
        }
        ret = tsk_compressed_matrix_clear(output);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_ls_hmm_run(self, haplotype, tsk_ls_hmm_next_probability_forward,
        tsk_ls_hmm_compute_normalisation_factor_forward, output);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

/****************************************************************
 * Viterbi Algorithm
 ****************************************************************/

static double
tsk_ls_hmm_compute_normalisation_factor_viterbi(tsk_ls_hmm_t *self)
{
    tsk_value_transition_t *restrict T = self->transitions;
    const tsk_id_t num_transitions = (tsk_id_t) self->num_transitions;
    tsk_value_transition_t max_vt;
    tsk_id_t j;

    max_vt.value = -1;
    max_vt.tree_node = 0; /* keep compiler happy */
    tsk_bug_assert(num_transitions > 0);
    for (j = 0; j < num_transitions; j++) {
        tsk_bug_assert(T[j].tree_node != TSK_NULL);
        if (T[j].value > max_vt.value) {
            max_vt = T[j];
        }
    }
    return max_vt.value;
}

static int
tsk_ls_hmm_next_probability_viterbi(tsk_ls_hmm_t *self, tsk_id_t site, double p_last,
    bool is_match, tsk_id_t node, double *result)
{
    const double rho = self->recombination_rate[site];
    const double mu = self->mutation_rate[site];
    const double num_alleles = self->num_alleles[site];
    const double n = self->num_samples;
    double p_recomb, p_no_recomb, p_t, p_e;
    bool recombination_required = false;

    p_no_recomb = p_last * (1 - rho + rho / n);
    p_recomb = rho / n;
    if (p_no_recomb > p_recomb) {
        p_t = p_no_recomb;
    } else {
        p_t = p_recomb;
        recombination_required = true;
    }
    p_e = mu;
    if (is_match) {
        p_e = 1 - (num_alleles - 1) * mu;
    }
    *result = p_t * p_e;
    return tsk_viterbi_matrix_add_recombination_required(
        self->output, site, node, recombination_required);
}

int
tsk_ls_hmm_viterbi(tsk_ls_hmm_t *self, int8_t *haplotype, tsk_viterbi_matrix_t *output,
    tsk_flags_t options)
{
    int ret = 0;

    if (!(options & TSK_NO_INIT)) {
        ret = tsk_viterbi_matrix_init(output, self->tree_sequence, 0, 0);
        if (ret != 0) {
            goto out;
        }
    } else {
        if (output->matrix.tree_sequence != self->tree_sequence) {
            ret = TSK_ERR_BAD_PARAM_VALUE;
            goto out;
        }
        ret = tsk_viterbi_matrix_clear(output);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_ls_hmm_run(self, haplotype, tsk_ls_hmm_next_probability_viterbi,
        tsk_ls_hmm_compute_normalisation_factor_viterbi, output);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

/****************************************************************
 * Compressed matrix
 ****************************************************************/

int
tsk_compressed_matrix_init(tsk_compressed_matrix_t *self, tsk_treeseq_t *tree_sequence,
    size_t block_size, tsk_flags_t options)
{
    int ret = 0;
    size_t num_sites;

    memset(self, 0, sizeof(*self));
    self->tree_sequence = tree_sequence;
    self->options = options;
    self->num_sites = tsk_treeseq_get_num_sites(tree_sequence);
    self->num_samples = tsk_treeseq_get_num_samples(tree_sequence);
    /* Avoid malloc(0) */
    num_sites = TSK_MAX(self->num_sites, 1);
    self->num_transitions = malloc(num_sites * sizeof(*self->num_transitions));
    self->normalisation_factor = malloc(num_sites * sizeof(*self->normalisation_factor));
    self->values = malloc(num_sites * sizeof(*self->values));
    self->nodes = malloc(num_sites * sizeof(*self->nodes));
    if (self->num_transitions == NULL || self->values == NULL || self->nodes == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    if (block_size == 0) {
        block_size = 1 << 20;
    }
    ret = tsk_blkalloc_init(&self->memory, block_size);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_compressed_matrix_clear(self);
out:
    return ret;
}

int
tsk_compressed_matrix_free(tsk_compressed_matrix_t *self)
{
    tsk_blkalloc_free(&self->memory);
    tsk_safe_free(self->num_transitions);
    tsk_safe_free(self->normalisation_factor);
    tsk_safe_free(self->values);
    tsk_safe_free(self->nodes);
    return 0;
}

int
tsk_compressed_matrix_clear(tsk_compressed_matrix_t *self)
{
    tsk_blkalloc_reset(&self->memory);
    memset(self->num_transitions, 0, self->num_sites * sizeof(*self->num_transitions));
    memset(self->normalisation_factor, 0,
        self->num_sites * sizeof(*self->normalisation_factor));
    return 0;
}

void
tsk_compressed_matrix_print_state(tsk_compressed_matrix_t *self, FILE *out)
{
    tsk_size_t l, j;

    fprintf(out, "Compressed matrix for %p\n", (void *) self->tree_sequence);
    fprintf(out, "num_sites = %d\n", self->num_sites);
    fprintf(out, "num_samples = %d\n", self->num_samples);
    for (l = 0; l < self->num_sites; l++) {
        fprintf(out, "%d\ts=%f\tv=%d [", l, self->normalisation_factor[l],
            self->num_transitions[l]);
        for (j = 0; j < self->num_transitions[l]; j++) {
            fprintf(out, "(%d, %f)", self->nodes[l][j], self->values[l][j]);
            if (j < self->num_transitions[l] - 1) {
                fprintf(out, ",");
            } else {
                fprintf(out, "]\n");
            }
        }
    }
    fprintf(out, "Memory:\n");
    tsk_blkalloc_print_state(&self->memory, out);
}

int
tsk_compressed_matrix_store_site(tsk_compressed_matrix_t *self, tsk_id_t site,
    double normalisation_factor, tsk_size_t num_transitions,
    const tsk_value_transition_t *transitions)
{
    int ret = 0;
    tsk_size_t j;

    if (site < 0 || site >= (tsk_id_t) self->num_sites) {
        ret = TSK_ERR_OUT_OF_BOUNDS;
        goto out;
    }

    self->num_transitions[site] = num_transitions;
    self->normalisation_factor[site] = normalisation_factor;
    self->nodes[site]
        = tsk_blkalloc_get(&self->memory, num_transitions * sizeof(tsk_id_t));
    self->values[site]
        = tsk_blkalloc_get(&self->memory, num_transitions * sizeof(double));
    if (self->nodes[site] == NULL || self->values[site] == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }

    for (j = 0; j < num_transitions; j++) {
        self->values[site][j] = transitions[j].value;
        self->nodes[site][j] = transitions[j].tree_node;
    }
out:
    return ret;
}

static int
tsk_compressed_matrix_decode_site(tsk_compressed_matrix_t *self, const tsk_tree_t *tree,
    const tsk_id_t site, double *values)
{
    int ret = 0;
    const tsk_id_t *restrict list_left = tree->left_sample;
    const tsk_id_t *restrict list_right = tree->right_sample;
    const tsk_id_t *restrict list_next = tree->next_sample;
    const tsk_id_t num_nodes = (tsk_id_t) tsk_treeseq_get_num_nodes(self->tree_sequence);
    tsk_size_t j;
    tsk_id_t node, index, stop;
    double value;

    for (j = 0; j < self->num_transitions[site]; j++) {
        node = self->nodes[site][j];
        if (node < 0 || node >= num_nodes) {
            ret = TSK_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        value = self->values[site][j];
        index = list_left[node];
        if (index == TSK_NULL) {
            /* It's an error if there are nodes that don't subtend any samples */
            ret = TSK_ERR_BAD_COMPRESSED_MATRIX_NODE;
            goto out;
        }
        stop = list_right[node];
        while (true) {
            values[index] = value;
            if (index == stop) {
                break;
            }
            index = list_next[index];
        }
    }
out:
    return ret;
}

int
tsk_compressed_matrix_decode(tsk_compressed_matrix_t *self, double *values)
{
    int ret = 0;
    int t_ret;
    tsk_tree_t tree;
    tsk_size_t j, num_tree_sites;
    const tsk_site_t *sites = NULL;
    tsk_id_t site_id;
    double *site_array;

    ret = tsk_tree_init(&tree, self->tree_sequence, TSK_SAMPLE_LISTS);
    if (ret != 0) {
        goto out;
    }

    for (t_ret = tsk_tree_first(&tree); t_ret == 1; t_ret = tsk_tree_next(&tree)) {
        ret = tsk_tree_get_sites(&tree, &sites, &num_tree_sites);
        if (ret != 0) {
            goto out;
        }
        for (j = 0; j < num_tree_sites; j++) {
            site_id = sites[j].id;
            site_array = values + ((size_t) site_id) * self->num_samples;
            if (self->num_transitions[site_id] == 0) {
                memset(site_array, 0, self->num_samples * sizeof(site_array));
            } else {
                ret = tsk_compressed_matrix_decode_site(
                    self, &tree, site_id, site_array);
                if (ret != 0) {
                    goto out;
                }
            }
        }
    }
    if (t_ret < 0) {
        ret = t_ret;
        goto out;
    }
out:
    tsk_tree_free(&tree);
    return ret;
}

/****************************************************************
 * Viterbi matrix
 ****************************************************************/

static int
tsk_viterbi_matrix_expand_recomb_records(tsk_viterbi_matrix_t *self)
{
    int ret = 0;
    tsk_recomb_required_record *tmp
        = realloc(self->recombination_required, self->max_recomb_records * sizeof(*tmp));

    if (tmp == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    self->recombination_required = tmp;
out:
    return ret;
}

int
tsk_viterbi_matrix_init(tsk_viterbi_matrix_t *self, tsk_treeseq_t *tree_sequence,
    size_t block_size, tsk_flags_t options)
{
    int ret = 0;

    memset(self, 0, sizeof(*self));
    if (block_size == 0) {
        block_size = 1 << 20; /* 1MiB */
    }
    ret = tsk_compressed_matrix_init(&self->matrix, tree_sequence, block_size, options);
    if (ret != 0) {
        goto out;
    }

    self->max_recomb_records
        = TSK_MAX(1, block_size / sizeof(tsk_recomb_required_record));
    ret = tsk_viterbi_matrix_expand_recomb_records(self);
    if (ret != 0) {
        goto out;
    }
    /* Add the sentinel at the start to simplify traceback */
    self->recombination_required[0].site = -1;

    ret = tsk_viterbi_matrix_clear(self);
out:
    return ret;
}

int
tsk_viterbi_matrix_free(tsk_viterbi_matrix_t *self)
{
    tsk_compressed_matrix_free(&self->matrix);
    tsk_safe_free(self->recombination_required);
    return 0;
}

int
tsk_viterbi_matrix_clear(tsk_viterbi_matrix_t *self)
{
    self->num_recomb_records = 1;
    tsk_compressed_matrix_clear(&self->matrix);
    return 0;
}

void
tsk_viterbi_matrix_print_state(tsk_viterbi_matrix_t *self, FILE *out)
{
    tsk_id_t l, j;

    fprintf(out, "viterbi_matrix\n");
    fprintf(out, "num_recomb_records = %d\n", (int) self->num_recomb_records);
    fprintf(out, "max_recomb_records = %d\n", (int) self->max_recomb_records);

    j = 1;
    for (l = 0; l < (tsk_id_t) self->matrix.num_sites; l++) {
        fprintf(out, "%d\t[", l);
        while (j < (tsk_id_t) self->num_recomb_records
               && self->recombination_required[j].site == l) {
            fprintf(out, "(%d, %d) ", self->recombination_required[j].node,
                self->recombination_required[j].required);
            j++;
        }
        fprintf(out, "]\n");
    }
    tsk_compressed_matrix_print_state(&self->matrix, out);
}

TSK_WARN_UNUSED int
tsk_viterbi_matrix_add_recombination_required(
    tsk_viterbi_matrix_t *self, tsk_id_t site, tsk_id_t node, bool required)
{
    int ret = 0;
    tsk_recomb_required_record *record;

    if (self->num_recomb_records == self->max_recomb_records) {
        self->max_recomb_records *= 2;
        ret = tsk_viterbi_matrix_expand_recomb_records(self);
        if (ret != 0) {
            goto out;
        }
    }
    record = self->recombination_required + self->num_recomb_records;
    record->site = site;
    record->node = node;
    record->required = required;
    self->num_recomb_records++;
out:
    return ret;
}

static tsk_id_t
tsk_viterbi_matrix_choose_sample(
    tsk_viterbi_matrix_t *self, tsk_id_t site, tsk_tree_t *tree)
{
    tsk_id_t ret;
    tsk_id_t u = TSK_NULL;
    const tsk_flags_t *node_flags = self->matrix.tree_sequence->tables->nodes.flags;
    const tsk_size_t num_transitions = self->matrix.num_transitions[site];
    const tsk_id_t *transition_nodes = self->matrix.nodes[site];
    const double *transition_values = self->matrix.values[site];
    double max_value = -1;
    tsk_size_t j;
    tsk_id_t v;
    bool found;

    if (num_transitions == 0) {
        ret = TSK_ERR_NULL_VITERBI_MATRIX;
        goto out;
    }
    for (j = 0; j < num_transitions; j++) {
        if (max_value < transition_values[j]) {
            u = transition_nodes[j];
            max_value = transition_values[j];
        }
    }
    tsk_bug_assert(u != TSK_NULL);

    while (!(node_flags[u] & TSK_NODE_IS_SAMPLE)) {
        found = false;
        for (v = tree->left_child[u]; v != TSK_NULL; v = tree->right_sib[v]) {
            /* Choose the first child that is not in the list of transition nodes */
            for (j = 0; j < num_transitions; j++) {
                if (transition_nodes[j] == v) {
                    break;
                }
            }
            if (j == num_transitions) {
                u = v;
                found = true;
                break;
            }
        }
        /* TODO: should remove this once we're sure this is robust */
        tsk_bug_assert(found);
    }
    ret = u;
out:
    return ret;
}

int
tsk_viterbi_matrix_traceback(
    tsk_viterbi_matrix_t *self, tsk_id_t *path, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_site_t site;
    tsk_id_t u, site_id, current_node;
    tsk_recomb_required_record *rr_record, *rr_record_tmp;
    const tsk_id_t num_sites = (tsk_id_t) self->matrix.num_sites;
    const tsk_id_t num_nodes
        = (tsk_id_t) tsk_treeseq_get_num_nodes(self->matrix.tree_sequence);
    tsk_tree_t tree;
    tsk_id_t *recombination_tree
        = malloc((size_t) num_nodes * sizeof(*recombination_tree));

    ret = tsk_tree_init(&tree, self->matrix.tree_sequence, 0);
    if (ret != 0) {
        goto out;
    }
    if (recombination_tree == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    /* Initialise the path an recombination_tree to contain TSK_NULL */
    memset(path, 0xff, ((size_t) num_sites) * sizeof(*path));
    memset(recombination_tree, 0xff, ((size_t) num_nodes) * sizeof(*path));

    current_node = TSK_NULL;
    rr_record = &self->recombination_required[self->num_recomb_records - 1];
    ret = tsk_tree_last(&tree);
    if (ret < 0) {
        goto out;
    }

    for (site_id = num_sites - 1; site_id >= 0; site_id--) {
        ret = tsk_treeseq_get_site(self->matrix.tree_sequence, site_id, &site);
        if (ret != 0) {
            goto out;
        }
        while (tree.left > site.position) {
            ret = tsk_tree_prev(&tree);
            if (ret < 0) {
                goto out;
            }
        }
        tsk_bug_assert(tree.left <= site.position);
        tsk_bug_assert(site.position < tree.right);

        /* Fill in the recombination tree */
        rr_record_tmp = rr_record;
        while (rr_record->site == site.id) {
            recombination_tree[rr_record->node] = rr_record->required;
            rr_record--;
        }
        if (current_node == TSK_NULL) {
            current_node = tsk_viterbi_matrix_choose_sample(self, site.id, &tree);
            if (current_node < 0) {
                ret = current_node;
                goto out;
            }
        }
        path[site.id] = current_node;
        /* Now traverse up the tree from the current node. The
         * first marked node tells us whether we need to recombine */
        u = current_node;
        while (u != TSK_NULL && recombination_tree[u] == TSK_NULL) {
            u = tree.parent[u];
        }
        tsk_bug_assert(u != TSK_NULL);
        if (recombination_tree[u] == 1) {
            /* Switch at the next site */
            current_node = TSK_NULL;
        }

        /* Reset in the recombination tree */
        rr_record = rr_record_tmp;
        while (rr_record->site == site.id) {
            recombination_tree[rr_record->node] = TSK_NULL;
            rr_record--;
        }
    }
    ret = 0;
out:
    tsk_tree_free(&tree);
    tsk_safe_free(recombination_tree);
    return ret;
}
