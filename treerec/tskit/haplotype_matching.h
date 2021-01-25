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

#ifndef TSK_HAPLOTYPE_MATCHING_H
#define TSK_HAPLOTYPE_MATCHING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tskit/trees.h>

/* Seems like we might use this somewhere else as well, so putting it into the middle
 * of the flags space */
#define TSK_ALLELES_ACGT (1 << 16)

typedef struct {
    tsk_id_t tree_node;
    tsk_id_t value_index;
    double value;
} tsk_value_transition_t;

typedef struct {
    size_t index;
    double value;
} tsk_argsort_t;

typedef struct {
    tsk_id_t tree_node;
    tsk_id_t old_state;
    tsk_id_t new_state;
    tsk_id_t transition_parent;
} tsk_transition_stack_t;

typedef struct {
    double normalisation_factor;
    double *value;
    tsk_id_t *node;
    tsk_size_t num_values;
} tsk_site_probability_t;

typedef struct {
    tsk_treeseq_t *tree_sequence;
    tsk_flags_t options;
    tsk_size_t num_sites;
    tsk_size_t num_samples;
    double *normalisation_factor;
    tsk_size_t *num_transitions;
    double **values;
    tsk_id_t **nodes;
    tsk_blkalloc_t memory;
} tsk_compressed_matrix_t;

typedef struct {
    tsk_id_t site;
    tsk_id_t node;
    bool required;
} tsk_recomb_required_record;

typedef struct {
    tsk_compressed_matrix_t matrix;
    tsk_recomb_required_record *recombination_required;
    size_t num_recomb_records;
    size_t max_recomb_records;
} tsk_viterbi_matrix_t;

typedef struct _tsk_ls_hmm_t {
    /* input */
    tsk_treeseq_t *tree_sequence;
    double *recombination_rate;
    double *mutation_rate;
    const char ***alleles;
    unsigned int precision;
    uint8_t *num_alleles;
    tsk_size_t num_samples;
    tsk_size_t num_sites;
    tsk_size_t num_nodes;
    /* state */
    tsk_tree_t tree;
    tsk_diff_iter_t diffs;
    tsk_id_t *parent;
    /* The probability value transitions on the tree */
    tsk_value_transition_t *transitions;
    tsk_value_transition_t *transitions_copy;
    /* Stack used when distributing transitions on the tree */
    tsk_transition_stack_t *transition_stack;
    /* Map of node_id to index in the transitions list */
    tsk_id_t *transition_index;
    /* Buffer used to argsort the transitions by node time */
    tsk_argsort_t *transition_time_order;
    size_t num_transitions;
    size_t max_transitions;
    /* The distinct values in the transitions */
    double *values;
    size_t num_values;
    size_t max_values;
    /* Number of machine words per node optimal value set. */
    size_t num_optimal_value_set_words;
    uint64_t *optimal_value_sets;
    /* The parent transition; used during compression */
    tsk_id_t *transition_parent;
    /* The number of samples directly subtended by a transition */
    tsk_id_t *num_transition_samples;
    int8_t *allelic_state;
    /* Algorithms set these values before they are run */
    int (*next_probability)(
        struct _tsk_ls_hmm_t *, tsk_id_t, double, bool, tsk_id_t, double *);
    double (*compute_normalisation_factor)(struct _tsk_ls_hmm_t *);
    void *output;
} tsk_ls_hmm_t;

int tsk_ls_hmm_init(tsk_ls_hmm_t *self, tsk_treeseq_t *tree_sequence,
    double *recombination_rate, double *mutation_rate, tsk_flags_t options);
int tsk_ls_hmm_set_precision(tsk_ls_hmm_t *self, unsigned int precision);
int tsk_ls_hmm_free(tsk_ls_hmm_t *self);
void tsk_ls_hmm_print_state(tsk_ls_hmm_t *self, FILE *out);
int tsk_ls_hmm_forward(tsk_ls_hmm_t *self, int8_t *haplotype,
    tsk_compressed_matrix_t *output, tsk_flags_t options);
int tsk_ls_hmm_viterbi(tsk_ls_hmm_t *self, int8_t *haplotype,
    tsk_viterbi_matrix_t *output, tsk_flags_t options);
int tsk_ls_hmm_run(tsk_ls_hmm_t *self, int8_t *haplotype,
    int (*next_probability)(tsk_ls_hmm_t *, tsk_id_t, double, bool, tsk_id_t, double *),
    double (*compute_normalisation_factor)(struct _tsk_ls_hmm_t *), void *output);

int tsk_compressed_matrix_init(tsk_compressed_matrix_t *self,
    tsk_treeseq_t *tree_sequence, size_t block_size, tsk_flags_t options);
int tsk_compressed_matrix_free(tsk_compressed_matrix_t *self);
int tsk_compressed_matrix_clear(tsk_compressed_matrix_t *self);
void tsk_compressed_matrix_print_state(tsk_compressed_matrix_t *self, FILE *out);
int tsk_compressed_matrix_store_site(tsk_compressed_matrix_t *self, tsk_id_t site,
    double normalisation_factor, tsk_size_t num_transitions,
    const tsk_value_transition_t *transitions);
int tsk_compressed_matrix_decode(tsk_compressed_matrix_t *self, double *values);

int tsk_viterbi_matrix_init(tsk_viterbi_matrix_t *self, tsk_treeseq_t *tree_sequence,
    size_t block_size, tsk_flags_t options);
int tsk_viterbi_matrix_free(tsk_viterbi_matrix_t *self);
int tsk_viterbi_matrix_clear(tsk_viterbi_matrix_t *self);
void tsk_viterbi_matrix_print_state(tsk_viterbi_matrix_t *self, FILE *out);
int tsk_viterbi_matrix_add_recombination_required(
    tsk_viterbi_matrix_t *self, tsk_id_t site, tsk_id_t node, bool required);
int tsk_viterbi_matrix_traceback(
    tsk_viterbi_matrix_t *self, tsk_id_t *path, tsk_flags_t options);

#ifdef __cplusplus
}
#endif
#endif
