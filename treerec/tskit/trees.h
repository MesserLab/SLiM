/*
 * MIT License
 *
 * Copyright (c) 2019 Tskit Developers
 * Copyright (c) 2015-2018 University of Oxford
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

/**
 * @file trees.h
 * @brief Tskit core tree sequence operations.
 */
#ifndef TSK_TREES_H
#define TSK_TREES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tskit/tables.h>

/* The TSK_SAMPLE_COUNTS was removed in version 0.99.3, where
 * the default is now to always count samples except when
 * TSK_NO_SAMPLE_COUNTS is specified. This macro can be undefined
 * at some point in the future and the option reused for something
 * else. */
// clang-format off
#define TSK_SAMPLE_COUNTS           (1 << 0)
#define TSK_SAMPLE_LISTS            (1 << 1)
#define TSK_NO_SAMPLE_COUNTS        (1 << 2)

#define TSK_STAT_SITE               (1 << 0)
#define TSK_STAT_BRANCH             (1 << 1)
#define TSK_STAT_NODE               (1 << 2)

/* Leave room for other stat types */
#define TSK_STAT_POLARISED          (1 << 10)
#define TSK_STAT_SPAN_NORMALISE     (1 << 11)

#define TSK_DIR_FORWARD 1
#define TSK_DIR_REVERSE -1
// clang-format on

/**
@brief The tree sequence object.
*/
typedef struct {
    tsk_size_t num_trees;
    tsk_size_t num_samples;
    tsk_id_t *samples;
    /* Breakpoints along the sequence, including 0 and L. */
    double *breakpoints;
    /* If a node is a sample, map to its index in the samples list */
    tsk_id_t *sample_index_map;
    /* Map individuals to the list of nodes that reference them */
    tsk_id_t *individual_nodes_mem;
    tsk_id_t **individual_nodes;
    tsk_size_t *individual_nodes_length;
    /* For each tree, a list of sites on that tree */
    tsk_site_t *tree_sites_mem;
    tsk_site_t **tree_sites;
    tsk_size_t *tree_sites_length;
    /* For each site, a list of mutations at that site */
    tsk_mutation_t *site_mutations_mem;
    tsk_mutation_t **site_mutations;
    tsk_size_t *site_mutations_length;
    /* The underlying tables */
    tsk_table_collection_t *tables;
} tsk_treeseq_t;

/**
@brief A single tree in a tree sequence.

@rst
A ``tsk_tree_t`` object has two basic functions:

1. Represent the state of a single tree in a tree sequence;
2. Provide methods to transform this state into different trees in the sequence.

The state of a single tree in the tree sequence is represented using the
quintuply linked encoding: please see the
:ref:`data model <sec_data_model_tree_structure>` section for details on
how this works. The left-to-right ordering of nodes in this encoding
is arbitrary, and may change depending on the order in which trees are
accessed within the sequence. Please see the
:ref:`sec_c_api_examples_tree_traversals` examples for recommended
usage.

On initialisation, a tree is in a "null" state: each sample is a
root and there are no edges. We must call one of the 'seeking' methods
to make the state of the tree object correspond to a particular tree
in the sequence. Please see the
:ref:`sec_c_api_examples_tree_iteration` examples for recommended
usage.

@endrst
 */
typedef struct {
    /**
     * @brief The parent tree sequence.
     */
    tsk_treeseq_t *tree_sequence;
    /**
     * @brief The leftmost root in the tree. Roots are siblings, and
     * other roots can be found using right_sib.
     */
    tsk_id_t left_root;
    /**
     @brief The parent of node u is parent[u]. Equal to TSK_NULL if node u is a
     root or is not a node in the current tree.
     */
    tsk_id_t *parent;
    /**
     @brief The leftmost child of node u is left_child[u]. Equal to TSK_NULL
     if node u is a leaf or is not a node in the current tree.
     */
    tsk_id_t *left_child;
    /**
     @brief The rightmost child of node u is right_child[u]. Equal to TSK_NULL
     if node u is a leaf or is not a node in the current tree.
     */
    tsk_id_t *right_child;
    /**
     @brief The sibling to the left of node u is left_sib[u]. Equal to TSK_NULL
     if node u has no siblings to its left.
     */
    tsk_id_t *left_sib;
    /**
     @brief The sibling to the right of node u is right_sib[u]. Equal to TSK_NULL
     if node u has no siblings to its right.
     */
    tsk_id_t *right_sib;

    tsk_size_t num_nodes;
    tsk_flags_t options;
    tsk_size_t root_threshold;
    tsk_id_t *samples;
    /* TODO before documenting this should be change to interval. */
    /* Left and right physical coordinates of the tree */
    double left;
    double right;
    tsk_id_t index;
    /* These are involved in the optional sample tracking; num_samples counts
     * all samples below a give node, and num_tracked_samples counts those
     * from a specific subset. By default sample counts are tracked and roots
     * maintained. If TSK_NO_SAMPLE_COUNTS is specified, then neither sample
     * counts or roots are available. */
    tsk_id_t *num_samples;
    tsk_id_t *num_tracked_samples;
    /* TODO the only place this feature seems to be used is in the ld_calculator.
     * when this is being replaced we should come up with a better way of doing
     * whatever this is being used for. */
    /* All nodes that are marked during a particular transition are marked
     * with a given value. */
    uint8_t *marked;
    uint8_t mark;
    /* These are for the optional sample list tracking. */
    tsk_id_t *left_sample;
    tsk_id_t *right_sample;
    tsk_id_t *next_sample;
    /* The sites on this tree */
    tsk_site_t *sites;
    tsk_size_t sites_length;
    /* Counters needed for next() and prev() transformations. */
    int direction;
    tsk_id_t left_index;
    tsk_id_t right_index;
} tsk_tree_t;

/* Diff iterator. TODO Not sure if we want to keep this, as it's not used
 * very much in the C code. */
typedef struct _tsk_edge_list_node_t {
    tsk_edge_t edge;
    struct _tsk_edge_list_node_t *next;
    struct _tsk_edge_list_node_t *prev;
} tsk_edge_list_node_t;

typedef struct {
    tsk_edge_list_node_t *head;
    tsk_edge_list_node_t *tail;
} tsk_edge_list_t;

typedef struct {
    size_t num_nodes;
    size_t num_edges;
    double tree_left;
    tsk_treeseq_t *tree_sequence;
    size_t insertion_index;
    size_t removal_index;
    size_t tree_index;
    tsk_edge_list_node_t *edge_list_nodes;
} tsk_diff_iter_t;

/****************************************************************************/
/* Tree sequence.*/
/****************************************************************************/

/**
@defgroup TREESEQ_API_GROUP Tree sequence API
@{
*/
int tsk_treeseq_init(
    tsk_treeseq_t *self, tsk_table_collection_t *tables, tsk_flags_t options);

int tsk_treeseq_load(tsk_treeseq_t *self, const char *filename, tsk_flags_t options);
int tsk_treeseq_loadf(tsk_treeseq_t *self, FILE *file, tsk_flags_t options);

int tsk_treeseq_dump(tsk_treeseq_t *self, const char *filename, tsk_flags_t options);
int tsk_treeseq_dumpf(tsk_treeseq_t *self, FILE *file, tsk_flags_t options);
int tsk_treeseq_copy_tables(
    tsk_treeseq_t *self, tsk_table_collection_t *tables, tsk_flags_t options);
int tsk_treeseq_free(tsk_treeseq_t *self);
void tsk_treeseq_print_state(tsk_treeseq_t *self, FILE *out);

/** @} */

tsk_size_t tsk_treeseq_get_num_nodes(tsk_treeseq_t *self);
tsk_size_t tsk_treeseq_get_num_edges(tsk_treeseq_t *self);
tsk_size_t tsk_treeseq_get_num_migrations(tsk_treeseq_t *self);
tsk_size_t tsk_treeseq_get_num_sites(tsk_treeseq_t *self);
tsk_size_t tsk_treeseq_get_num_mutations(tsk_treeseq_t *self);
tsk_size_t tsk_treeseq_get_num_provenances(tsk_treeseq_t *self);
tsk_size_t tsk_treeseq_get_num_populations(tsk_treeseq_t *self);
tsk_size_t tsk_treeseq_get_num_individuals(tsk_treeseq_t *self);
tsk_size_t tsk_treeseq_get_num_trees(tsk_treeseq_t *self);
tsk_size_t tsk_treeseq_get_num_samples(tsk_treeseq_t *self);
char *tsk_treeseq_get_metadata(tsk_treeseq_t *self);
tsk_size_t tsk_treeseq_get_metadata_length(tsk_treeseq_t *self);
char *tsk_treeseq_get_metadata_schema(tsk_treeseq_t *self);
tsk_size_t tsk_treeseq_get_metadata_schema_length(tsk_treeseq_t *self);
char *tsk_treeseq_get_file_uuid(tsk_treeseq_t *self);
double tsk_treeseq_get_sequence_length(tsk_treeseq_t *self);
double *tsk_treeseq_get_breakpoints(tsk_treeseq_t *self);
tsk_id_t *tsk_treeseq_get_samples(tsk_treeseq_t *self);
tsk_id_t *tsk_treeseq_get_sample_index_map(tsk_treeseq_t *self);
bool tsk_treeseq_is_sample(tsk_treeseq_t *self, tsk_id_t u);

int tsk_treeseq_get_node(tsk_treeseq_t *self, tsk_id_t index, tsk_node_t *node);
int tsk_treeseq_get_edge(tsk_treeseq_t *self, tsk_id_t index, tsk_edge_t *edge);
int tsk_treeseq_get_migration(
    tsk_treeseq_t *self, tsk_id_t index, tsk_migration_t *migration);
int tsk_treeseq_get_site(tsk_treeseq_t *self, tsk_id_t index, tsk_site_t *site);
int tsk_treeseq_get_mutation(
    tsk_treeseq_t *self, tsk_id_t index, tsk_mutation_t *mutation);
int tsk_treeseq_get_provenance(
    tsk_treeseq_t *self, tsk_id_t index, tsk_provenance_t *provenance);
int tsk_treeseq_get_population(
    tsk_treeseq_t *self, tsk_id_t index, tsk_population_t *population);
int tsk_treeseq_get_individual(
    tsk_treeseq_t *self, tsk_id_t index, tsk_individual_t *individual);

int tsk_treeseq_simplify(tsk_treeseq_t *self, tsk_id_t *samples, tsk_size_t num_samples,
    tsk_flags_t options, tsk_treeseq_t *output, tsk_id_t *node_map);

int tsk_treeseq_kc_distance(
    tsk_treeseq_t *self, tsk_treeseq_t *other, double lambda_, double *result);

/* TODO do these belong in trees or stats? They should probably be in stats.
 * Keep them here for now until we figure out the correct interface.
 */
int tsk_treeseq_genealogical_nearest_neighbours(tsk_treeseq_t *self, tsk_id_t *focal,
    size_t num_focal, tsk_id_t **reference_sets, size_t *reference_set_size,
    size_t num_reference_sets, tsk_flags_t options, double *ret_array);
int tsk_treeseq_mean_descendants(tsk_treeseq_t *self, tsk_id_t **reference_sets,
    size_t *reference_set_size, size_t num_reference_sets, tsk_flags_t options,
    double *ret_array);

/* TODO change all these size_t's to tsk_size_t */

typedef int general_stat_func_t(size_t K, double *X, size_t M, double *Y, void *params);

int tsk_treeseq_general_stat(tsk_treeseq_t *self, size_t K, double *W, size_t M,
    general_stat_func_t *f, void *f_params, size_t num_windows, double *windows,
    double *sigma, tsk_flags_t options);

/* One way weighted stats */
int tsk_treeseq_trait_covariance(tsk_treeseq_t *self, tsk_size_t num_weights,
    double *weights, tsk_size_t num_windows, double *windows, double *result,
    tsk_flags_t options);
int tsk_treeseq_trait_correlation(tsk_treeseq_t *self, tsk_size_t num_weights,
    double *weights, tsk_size_t num_windows, double *windows, double *result,
    tsk_flags_t options);
/* One way weighted stats with covariates */
int tsk_treeseq_trait_regression(tsk_treeseq_t *self, tsk_size_t num_weights,
    double *weights, tsk_size_t num_covariates, double *covariates,
    tsk_size_t num_windows, double *windows, double *result, tsk_flags_t options);

/* One way sample set stats */
int tsk_treeseq_diversity(tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    tsk_size_t *sample_set_sizes, tsk_id_t *sample_sets, tsk_size_t num_windows,
    double *windows, double *result, tsk_flags_t options);
int tsk_treeseq_segregating_sites(tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    tsk_size_t *sample_set_sizes, tsk_id_t *sample_sets, tsk_size_t num_windows,
    double *windows, double *result, tsk_flags_t options);
int tsk_treeseq_Y1(tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    tsk_size_t *sample_set_sizes, tsk_id_t *sample_sets, tsk_size_t num_windows,
    double *windows, double *result, tsk_flags_t options);
int tsk_treeseq_allele_frequency_spectrum(tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, tsk_size_t *sample_set_sizes, tsk_id_t *sample_sets,
    tsk_size_t num_windows, double *windows, double *result, tsk_flags_t options);

/* Two way sample set stats */
int tsk_treeseq_divergence(tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    tsk_size_t *sample_set_sizes, tsk_id_t *sample_sets, tsk_size_t num_index_tuples,
    tsk_id_t *index_tuples, tsk_size_t num_windows, double *windows, double *result,
    tsk_flags_t options);
int tsk_treeseq_Y2(tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    tsk_size_t *sample_set_sizes, tsk_id_t *sample_sets, tsk_size_t num_index_tuples,
    tsk_id_t *index_tuples, tsk_size_t num_windows, double *windows, double *result,
    tsk_flags_t options);
int tsk_treeseq_f2(tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    tsk_size_t *sample_set_sizes, tsk_id_t *sample_sets, tsk_size_t num_index_tuples,
    tsk_id_t *index_tuples, tsk_size_t num_windows, double *windows, double *result,
    tsk_flags_t options);

/* Three way sample set stats */
int tsk_treeseq_Y3(tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    tsk_size_t *sample_set_sizes, tsk_id_t *sample_sets, tsk_size_t num_index_tuples,
    tsk_id_t *index_tuples, tsk_size_t num_windows, double *windows, double *result,
    tsk_flags_t options);
int tsk_treeseq_f3(tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    tsk_size_t *sample_set_sizes, tsk_id_t *sample_sets, tsk_size_t num_index_tuples,
    tsk_id_t *index_tuples, tsk_size_t num_windows, double *windows, double *result,
    tsk_flags_t options);

/* Four way sample set stats */
int tsk_treeseq_f4(tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    tsk_size_t *sample_set_sizes, tsk_id_t *sample_sets, tsk_size_t num_index_tuples,
    tsk_id_t *index_tuples, tsk_size_t num_windows, double *windows, double *result,
    tsk_flags_t options);

/****************************************************************************/
/* Tree */
/****************************************************************************/

/**
@defgroup TREE_API_GROUP Tree sequence API
@{
*/

int tsk_tree_init(tsk_tree_t *self, tsk_treeseq_t *tree_sequence, tsk_flags_t options);
int tsk_tree_free(tsk_tree_t *self);

tsk_id_t tsk_tree_get_index(tsk_tree_t *self);
tsk_size_t tsk_tree_get_num_roots(tsk_tree_t *self);

int tsk_tree_first(tsk_tree_t *self);
int tsk_tree_last(tsk_tree_t *self);
int tsk_tree_next(tsk_tree_t *self);
int tsk_tree_prev(tsk_tree_t *self);
int tsk_tree_clear(tsk_tree_t *self);

void tsk_tree_print_state(tsk_tree_t *self, FILE *out);
/** @} */

int tsk_tree_set_root_threshold(tsk_tree_t *self, tsk_size_t root_threshold);
tsk_size_t tsk_tree_get_root_threshold(tsk_tree_t *self);

bool tsk_tree_has_sample_lists(tsk_tree_t *self);
bool tsk_tree_has_sample_counts(tsk_tree_t *self);
bool tsk_tree_equals(tsk_tree_t *self, tsk_tree_t *other);
/* Returns true if u is a descendant of v; false otherwise */
bool tsk_tree_is_descendant(tsk_tree_t *self, tsk_id_t u, tsk_id_t v);
bool tsk_tree_is_sample(tsk_tree_t *self, tsk_id_t u);

int tsk_tree_copy(tsk_tree_t *self, tsk_tree_t *dest, tsk_flags_t options);
int tsk_tree_set_tracked_samples(
    tsk_tree_t *self, size_t num_tracked_samples, tsk_id_t *tracked_samples);
int tsk_tree_set_tracked_samples_from_sample_list(
    tsk_tree_t *self, tsk_tree_t *other, tsk_id_t node);

int tsk_tree_get_parent(tsk_tree_t *self, tsk_id_t u, tsk_id_t *parent);
int tsk_tree_get_time(tsk_tree_t *self, tsk_id_t u, double *t);
int tsk_tree_get_mrca(tsk_tree_t *self, tsk_id_t u, tsk_id_t v, tsk_id_t *mrca);
int tsk_tree_get_num_samples(tsk_tree_t *self, tsk_id_t u, size_t *num_samples);
int tsk_tree_get_num_tracked_samples(
    tsk_tree_t *self, tsk_id_t u, size_t *num_tracked_samples);
int tsk_tree_get_sites(tsk_tree_t *self, tsk_site_t **sites, tsk_size_t *sites_length);
int tsk_tree_depth(tsk_tree_t *self, tsk_id_t u, tsk_size_t *depth);

typedef struct {
    tsk_id_t node;
    tsk_id_t parent;
    int8_t state;
} tsk_state_transition_t;

int tsk_tree_map_mutations(tsk_tree_t *self, int8_t *genotypes, double *cost_matrix,
    tsk_flags_t options, int8_t *ancestral_state, tsk_size_t *num_transitions,
    tsk_state_transition_t **transitions);

int tsk_tree_kc_distance(
    tsk_tree_t *self, tsk_tree_t *other, double lambda, double *result);

/****************************************************************************/
/* Diff iterator */
/****************************************************************************/

int tsk_diff_iter_init(tsk_diff_iter_t *self, tsk_treeseq_t *tree_sequence);
int tsk_diff_iter_free(tsk_diff_iter_t *self);
int tsk_diff_iter_next(tsk_diff_iter_t *self, double *left, double *right,
    tsk_edge_list_t *edges_out, tsk_edge_list_t *edges_in);
void tsk_diff_iter_print_state(tsk_diff_iter_t *self, FILE *out);

#ifdef __cplusplus
}
#endif
#endif
