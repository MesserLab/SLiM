/*
** Copyright (C) 2015-2017 University of Oxford
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

#ifndef __MSPRIME_H__
#define __MSPRIME_H__

#ifndef MSP_LIBRARY_VERSION_STR
#define MSP_LIBRARY_VERSION_STR "undefined"
#endif

#include <stdio.h>
#include <stdbool.h>

#include <gsl/gsl_rng.h>

#include "util.h"
#include "avl.h"
#include "fenwick.h"
#include "object_heap.h"
#include "tables.h"

#define MSP_SAMPLE_COUNTS  1
#define MSP_SAMPLE_LISTS   2

#define MSP_DIR_FORWARD 1
#define MSP_DIR_REVERSE -1

#define MSP_MODEL_HUDSON 0
#define MSP_MODEL_SMC 1
#define MSP_MODEL_SMC_PRIME 2
#define MSP_MODEL_BETA 3
#define MSP_MODEL_DIRAC 4
#define MSP_MODEL_DTWF 5


#define MSP_INITIALISED_MAGIC 0x1234567

typedef struct segment_t_t {
    population_id_t population_id;
    /* During simulation we use genetic coordinates */
    uint32_t left;
    uint32_t right;
    node_id_t value;
    size_t id;
    struct segment_t_t *prev;
    struct segment_t_t *next;
} segment_t;


typedef struct {
    uint32_t left; /* TODO CHANGE THIS - not a good name! */
    uint32_t value;
} node_mapping_t;

typedef struct {
    population_id_t population_id;
    double time;
} sample_t;

typedef struct {
    double initial_size;
    double growth_rate;
    double start_time;
    avl_tree_t ancestors;
} population_t;

typedef struct {
    double time;
    node_id_t sample;
    population_id_t population_id;
} sampling_event_t;

/* Simulation models */

typedef struct {
    double alpha;
    double truncation_point;
} beta_coalescent_t;

typedef struct {
    double psi;
    double c; // constant
} dirac_coalescent_t;

typedef struct _simulation_model_t {
    int type;
    double population_size;
    union {
        beta_coalescent_t beta_coalescent;
        dirac_coalescent_t dirac_coalescent;
    } params;
    /* Time and rate conversions */
    double (*model_time_to_generations)(struct _simulation_model_t *model, double t);
    double (*generations_to_model_time)(struct _simulation_model_t *model, double g);
    double (*generation_rate_to_model_rate)(struct _simulation_model_t *model, double rg);
    double (*model_rate_to_generation_rate)(struct _simulation_model_t *model, double rm);
} simulation_model_t;

typedef struct _msp_t {
    gsl_rng *rng;
    /* input parameters */
    simulation_model_t model;
    bool store_migrations;
    uint32_t num_samples;
    uint32_t num_loci;
    double recombination_rate;
    uint32_t num_populations;
    sample_t *samples;
    simulation_model_t initial_model;
    double *initial_migration_matrix;
    population_t *initial_populations;
    /* allocation block sizes */
    size_t avl_node_block_size;
    size_t node_mapping_block_size;
    size_t segment_block_size;
    size_t max_memory;
    /* Counters for statistics */
    size_t num_re_events;
    size_t num_ca_events;
    size_t num_rejected_ca_events;
    size_t *num_migration_events;
    size_t num_trapped_re_events;
    size_t num_multiple_re_events;
    /* sampling events */
    sampling_event_t *sampling_events;
    size_t num_sampling_events;
    size_t next_sampling_event;
    /* Demographic events */
    struct demographic_event_t_t *demographic_events_head;
    struct demographic_event_t_t *demographic_events_tail;
    struct demographic_event_t_t *next_demographic_event;
    /* algorithm state */
    int state;
    size_t used_memory;
    double time;
    double *migration_matrix;
    population_t *populations;
    avl_tree_t breakpoints;
    avl_tree_t overlap_counts;
    fenwick_t links;
    /* memory management */
    object_heap_t avl_node_heap;
    object_heap_t segment_heap;
    object_heap_t node_mapping_heap;
    /* nodes are stored in a flat array */
    node_t *nodes;
    size_t num_nodes;
    size_t max_nodes;
    size_t node_block_size;
    size_t num_node_blocks;
    /* edges are stored in a flat array */
    edge_t *edges;
    size_t num_edges;
    size_t max_edges;
    size_t edge_block_size;
    size_t num_edge_blocks;
    size_t edge_buffer_start;
    /* migration records are stored in a flat array */
    migration_t *migrations;
    size_t num_migrations;
    size_t max_migrations;
    size_t migration_block_size;
    size_t num_migration_blocks;
    /* Methods for getting the waiting time until the next common ancestor
     * event and the event are defined by the simulation model */
    double (*get_common_ancestor_waiting_time)(struct _msp_t *self, population_id_t pop);
    int (*common_ancestor_event)(struct _msp_t *selt, population_id_t pop);
} msp_t;

/* Demographic events */
typedef struct {
    population_id_t population_id;
    double initial_size;
    double growth_rate;
} population_parameters_change_t;

typedef struct {
    int matrix_index;
    double migration_rate;
} migration_rate_change_t;

typedef struct {
    population_id_t source;
    population_id_t destination;
    double proportion;
} mass_migration_t;

typedef struct {
    population_id_t population_id;
    double proportion;
} simple_bottleneck_t;

typedef struct {
    population_id_t population_id;
    double strength;
} instantaneous_bottleneck_t;

typedef struct demographic_event_t_t {
    double time;
    int (*change_state)(msp_t *, struct demographic_event_t_t *);
    void (*print_state)(msp_t *, struct demographic_event_t_t *, FILE *out);
    union {
        simple_bottleneck_t simple_bottleneck;
        instantaneous_bottleneck_t instantaneous_bottleneck;
        mass_migration_t mass_migration;
        migration_rate_change_t migration_rate_change;
        population_parameters_change_t population_parameters_change;
    } params;
    struct demographic_event_t_t *next;
} demographic_event_t;

/* Recombination map */

typedef struct {
    uint32_t num_loci;      /* size of the genetic coordinate space  */
    double sequence_length; /* size of the physical coordinate space */
    double total_recombination_rate;
    size_t size;            /* the total number of values in the map */
    double *positions;
    double *rates;
} recomb_map_t;

/* Tree sequences */
/* TODO This struct has some redundancy now, where we mirror the structure
 * of the table_collection_t within internal structs and keep pointers
 * to its memory (and also a copy of sequence_length). We need to refactor
 * this somehow to remove this redundancy. Probably downstream code should
 * be forced to use some functions rather than accessing these structs
 * to obtain pointers to indexes, edges data and so on.
 */
typedef struct {
    size_t num_trees;
    double sequence_length;
    size_t num_samples;
    node_id_t *samples;

    struct {
        size_t num_records;
        double *time;
        uint32_t *flags;
        population_id_t *population;
        char *metadata;
        table_size_t *metadata_offset;
        node_id_t *sample_index_map;
    } nodes;

    struct {
        size_t num_records;
        double *left;
        double *right;
        node_id_t *parent;
        node_id_t *child;
        struct {
            node_id_t *insertion_order;
            node_id_t *removal_order;
        } indexes;
    } edges;

    struct {
        size_t num_records;
        size_t ancestral_state_length;
        char *ancestral_state;
        table_size_t *ancestral_state_offset;
        size_t metadata_length;
        char *metadata;
        table_size_t *metadata_offset;
        double *position;
        site_t *tree_sites_mem;
        site_t **tree_sites;
        table_size_t *tree_sites_length;
        mutation_t *site_mutations_mem;
        mutation_t **site_mutations;
        table_size_t *site_mutations_length;
    } sites;

    struct {
        size_t num_records;
        size_t derived_state_length;
        char *derived_state;
        table_size_t *derived_state_offset;
        size_t metadata_length;
        char *metadata;
        table_size_t *metadata_offset;
        node_id_t *node;
        site_id_t *site;
        mutation_id_t *parent;
    } mutations;

    struct {
        size_t num_records;
        node_id_t *node;
        population_id_t *source;
        population_id_t *dest;
        double *left;
        double *right;
        double *time;
    } migrations;

    struct {
        size_t num_records;
        size_t timestamp_length;
        size_t record_length;
        char *timestamp;
        table_size_t *timestamp_offset;
        char *record;
        table_size_t *record_offset;
    } provenances;

    table_collection_t *tables;

} tree_sequence_t;

typedef struct _edge_list_t {
    edge_t edge;
    struct _edge_list_t *next;
} edge_list_t;

typedef struct _node_list {
    node_id_t node;
    struct _node_list *next;
} node_list_t;

typedef struct {
    size_t num_nodes;
    size_t num_edges;
    double tree_left;
    tree_sequence_t *tree_sequence;
    size_t insertion_index;
    size_t removal_index;
    size_t tree_index;
    edge_list_t *edge_list_nodes;
} tree_diff_iterator_t;

typedef struct {
    tree_sequence_t *tree_sequence;
    size_t num_nodes;
    int flags;
    node_id_t *samples;
    /* The left-most root in the forest. Roots are sibs and all roots are found
     * via left_sib and right_sib */
    node_id_t left_root;
    /* Left and right physical coordinates of the tree */
    double left;
    double right;
    node_id_t *parent;          /* parent of node u */
    node_id_t *left_child;      /* leftmost child of node u */
    node_id_t *right_child;     /* rightmost child of node u */
    node_id_t *left_sib;        /* sibling to right of node u */
    node_id_t *right_sib;       /* sibling to the left of node u */
    bool *above_sample;
    size_t index;
    /* These are involved in the optional sample tracking; num_samples counts
     * all samples below a give node, and num_tracked_samples counts those
     * from a specific subset. */
    node_id_t *num_samples;
    node_id_t *num_tracked_samples;
    /* All nodes that are marked during a particular transition are marked
     * with a given value. */
    uint8_t *marked;
    uint8_t mark;
    /* These are for the optional sample list tracking. */
    node_list_t **sample_list_head;
    node_list_t **sample_list_tail;
    node_list_t *sample_list_node_mem;
    /* traversal stacks */
    node_id_t *stack1;
    node_id_t *stack2;
    /* The sites on this tree */
    site_t *sites;
    table_size_t sites_length;
    /* Counters needed for next() and prev() transformations. */
    int direction;
    node_id_t left_index;
    node_id_t right_index;
} sparse_tree_t;

typedef struct {
    size_t precision;
    double time_scale;
    int flags;
    char *newick;
    sparse_tree_t *tree;
} newick_converter_t;

typedef struct {
    size_t num_samples;
    double sequence_length;
    size_t num_sites;
    tree_sequence_t *tree_sequence;
    node_id_t *sample_index_map;
    char *output_haplotype;
    char *haplotype_matrix;
    sparse_tree_t tree;
} hapgen_t;

typedef struct {
    site_t *site;
    const char **alleles;
    table_size_t *allele_lengths;
    table_size_t num_alleles;
    table_size_t max_alleles;
    uint8_t *genotypes;
} variant_t;

typedef struct {
    size_t num_samples;
    size_t num_sites;
    tree_sequence_t *tree_sequence;
    node_id_t *sample_index_map;
    size_t tree_site_index;
    int finished;
    sparse_tree_t tree;
    int flags;
    variant_t variant;
} vargen_t;

typedef struct {
    size_t num_samples;
    size_t num_vcf_samples;
    unsigned int ploidy;
    char *genotypes;
    char *header;
    char *record;
    char *vcf_genotypes;
    size_t vcf_genotypes_size;
    size_t contig_id_size;
    size_t record_size;
    size_t num_sites;
    unsigned long contig_length;
    unsigned long *positions;
    vargen_t *vargen;
} vcf_converter_t;

typedef struct {
    sparse_tree_t *outer_tree;
    sparse_tree_t *inner_tree;
    size_t num_sites;
    int tree_changed;
    tree_sequence_t *tree_sequence;
} ld_calc_t;

typedef struct {
    double position;
    node_id_t node;
    const char *ancestral_state;
    const char *derived_state;
} infinite_sites_mutation_t;

typedef struct {
    int alphabet;
    gsl_rng *rng;
    double mutation_rate;
    size_t num_mutations;
    size_t max_num_mutations;
    size_t mutation_block_size;
    site_table_t *sites;
    infinite_sites_mutation_t *mutations;
    object_heap_t avl_node_heap;
} mutgen_t;

int msp_alloc(msp_t *self, size_t num_samples, sample_t *samples, gsl_rng *rng);
int msp_set_simulation_model_hudson(msp_t *self, double population_size);
int msp_set_simulation_model_smc(msp_t *self, double population_size);
int msp_set_simulation_model_smc_prime(msp_t *self, double population_size);
int msp_set_simulation_model_dtwf(msp_t *self, double population_size);
int msp_set_simulation_model_dirac(msp_t *self, double population_size, double psi,
    double c);
int msp_set_simulation_model_beta(msp_t *self, double population_size, double alpha,
        double truncation_point);
int msp_set_num_loci(msp_t *self, size_t num_loci);
int msp_set_store_migrations(msp_t *self, bool store_migrations);
int msp_set_num_populations(msp_t *self, size_t num_populations);
int msp_set_recombination_rate(msp_t *self, double recombination_rate);
int msp_set_max_memory(msp_t *self, size_t max_memory);
int msp_set_node_mapping_block_size(msp_t *self, size_t block_size);
int msp_set_segment_block_size(msp_t *self, size_t block_size);
int msp_set_avl_node_block_size(msp_t *self, size_t block_size);
int msp_set_node_block_size(msp_t *self, size_t block_size);
int msp_set_edge_block_size(msp_t *self, size_t block_size);
int msp_set_migration_block_size(msp_t *self, size_t block_size);
int msp_set_sample_configuration(msp_t *self, size_t num_populations,
        size_t *sample_configuration);
int msp_set_migration_matrix(msp_t *self, size_t size,
        double *migration_matrix);
int msp_set_population_configuration(msp_t *self, int population_id,
        double initial_size, double growth_rate);

int msp_add_population_parameters_change(msp_t *self, double time,
        int population_id, double size, double growth_rate);
int msp_add_migration_rate_change(msp_t *self, double time, int matrix_index,
        double migration_rate);
int msp_add_mass_migration(msp_t *self, double time, int source, int dest,
        double proportion);
int msp_add_simple_bottleneck(msp_t *self, double time, int population_id,
        double intensity);
int msp_add_instantaneous_bottleneck(msp_t *self, double time, int population_id,
        double strength);

int msp_initialise(msp_t *self);
int msp_run(msp_t *self, double max_time, unsigned long max_events);
int msp_debug_demography(msp_t *self, double *end_time);
int msp_populate_tables(msp_t *self, recomb_map_t *recomb_map,
        node_table_t *node_table, edge_table_t *edge_table,
        migration_table_t *migration_table);
int msp_reset(msp_t *self);
int msp_print_state(msp_t *self, FILE *out);
int msp_free(msp_t *self);
void msp_verify(msp_t *self);

int msp_get_ancestors(msp_t *self, segment_t **ancestors);
int msp_get_breakpoints(msp_t *self, size_t *breakpoints);
int msp_get_migration_matrix(msp_t *self, double *migration_matrix);
int msp_get_num_migration_events(msp_t *self, size_t *num_migration_events);
int msp_get_nodes(msp_t *self, node_t **nodes);
int msp_get_edges(msp_t *self, edge_t **edges);
int msp_get_migrations(msp_t *self, migration_t **migrations);
int msp_get_samples(msp_t *self, sample_t **samples);
int msp_get_population_configuration(msp_t *self, size_t population_id,
        double *initial_size, double *growth_rate);
int msp_compute_population_size(msp_t *self, size_t population_id,
        double time, double *pop_size);
int msp_is_completed(msp_t *self);

simulation_model_t * msp_get_model(msp_t *self);
const char * msp_get_model_name(msp_t *self);
bool msp_get_store_migrations(msp_t *self);
double msp_get_recombination_rate(msp_t *self);
double msp_get_time(msp_t *self);
size_t msp_get_num_samples(msp_t *self);
size_t msp_get_num_loci(msp_t *self);
size_t msp_get_num_populations(msp_t *self);
size_t msp_get_num_ancestors(msp_t *self);
size_t msp_get_num_breakpoints(msp_t *self);
size_t msp_get_num_nodes(msp_t *self);
size_t msp_get_num_edges(msp_t *self);
size_t msp_get_num_migrations(msp_t *self);
size_t msp_get_num_avl_node_blocks(msp_t *self);
size_t msp_get_num_node_mapping_blocks(msp_t *self);
size_t msp_get_num_segment_blocks(msp_t *self);
size_t msp_get_num_node_blocks(msp_t *self);
size_t msp_get_num_edge_blocks(msp_t *self);
size_t msp_get_num_migration_blocks(msp_t *self);
size_t msp_get_used_memory(msp_t *self);
size_t msp_get_num_common_ancestor_events(msp_t *self);
size_t msp_get_num_rejected_common_ancestor_events(msp_t *self);
size_t msp_get_num_recombination_events(msp_t *self);

void tree_sequence_print_state(tree_sequence_t *self, FILE *out);
int tree_sequence_load_tables(tree_sequence_t *self, table_collection_t *tables,
        int flags);
int tree_sequence_dump_tables(tree_sequence_t *self, table_collection_t *tables,
        int flags);
int tree_sequence_load(tree_sequence_t *self, const char *filename, int flags);
int tree_sequence_dump(tree_sequence_t *self, const char *filename, int flags);
int tree_sequence_free(tree_sequence_t *self);

size_t tree_sequence_get_num_nodes(tree_sequence_t *self);
size_t tree_sequence_get_num_edges(tree_sequence_t *self);
size_t tree_sequence_get_num_migrations(tree_sequence_t *self);
size_t tree_sequence_get_num_sites(tree_sequence_t *self);
size_t tree_sequence_get_num_mutations(tree_sequence_t *self);
size_t tree_sequence_get_num_provenances(tree_sequence_t *self);
size_t tree_sequence_get_num_trees(tree_sequence_t *self);
size_t tree_sequence_get_num_samples(tree_sequence_t *self);
double tree_sequence_get_sequence_length(tree_sequence_t *self);
bool tree_sequence_is_sample(tree_sequence_t *self, node_id_t u);

int tree_sequence_get_node(tree_sequence_t *self, node_id_t index, node_t *node);
int tree_sequence_get_edge(tree_sequence_t *self, size_t index, edge_t *edge);
int tree_sequence_get_migration(tree_sequence_t *self, size_t index,
        migration_t *migration);
int tree_sequence_get_site(tree_sequence_t *self, site_id_t id, site_t *site);
int tree_sequence_get_mutation(tree_sequence_t *self, mutation_id_t id,
        mutation_t *mutation);
int tree_sequence_get_provenance(tree_sequence_t *self, size_t index,
        provenance_t *provenance);
int tree_sequence_get_samples(tree_sequence_t *self, node_id_t **samples);
int tree_sequence_get_sample_index_map(tree_sequence_t *self,
        node_id_t **sample_index_map);

int tree_sequence_simplify(tree_sequence_t *self, node_id_t *samples,
        size_t num_samples, int flags, tree_sequence_t *output,
        node_id_t *node_map);
int tree_sequence_get_pairwise_diversity(tree_sequence_t *self,
    node_id_t *samples, size_t num_samples, double *pi);

int tree_diff_iterator_alloc(tree_diff_iterator_t *self,
        tree_sequence_t *tree_sequence);
int tree_diff_iterator_free(tree_diff_iterator_t *self);
int tree_diff_iterator_next(tree_diff_iterator_t *self,
        double *left, double *right,
        edge_list_t **edges_out, edge_list_t **edges_in);
void tree_diff_iterator_print_state(tree_diff_iterator_t *self, FILE *out);

int sparse_tree_alloc(sparse_tree_t *self, tree_sequence_t *tree_sequence,
        int flags);
int sparse_tree_free(sparse_tree_t *self);
int sparse_tree_copy(sparse_tree_t *self, sparse_tree_t *source);
int sparse_tree_equal(sparse_tree_t *self, sparse_tree_t *other);
int sparse_tree_set_tracked_samples(sparse_tree_t *self,
        size_t num_tracked_samples, node_id_t *tracked_samples);
int sparse_tree_set_tracked_samples_from_sample_list(sparse_tree_t *self,
        node_list_t *head, node_list_t *tail);
int sparse_tree_get_root(sparse_tree_t *self, node_id_t *root);
bool sparse_tree_is_sample(sparse_tree_t *self, node_id_t u);
size_t sparse_tree_get_num_roots(sparse_tree_t *self);
int sparse_tree_get_parent(sparse_tree_t *self, node_id_t u, node_id_t *parent);
int sparse_tree_get_time(sparse_tree_t *self, node_id_t u, double *t);
int sparse_tree_get_mrca(sparse_tree_t *self, node_id_t u, node_id_t v, node_id_t *mrca);
int sparse_tree_get_num_samples(sparse_tree_t *self, node_id_t u, size_t *num_samples);
int sparse_tree_get_num_tracked_samples(sparse_tree_t *self, node_id_t u,
        size_t *num_tracked_samples);
int sparse_tree_get_sample_list(sparse_tree_t *self, node_id_t u,
        node_list_t **head, node_list_t **tail);
int sparse_tree_get_sites(sparse_tree_t *self, site_t **sites, table_size_t *sites_length);
int sparse_tree_get_newick(sparse_tree_t *self, size_t precision, double time_scale,
        int flags, size_t buffer_size, char *newick_buffer);
void sparse_tree_print_state(sparse_tree_t *self, FILE *out);
/* Method for positioning the tree in the sequence. */
int sparse_tree_first(sparse_tree_t *self);
int sparse_tree_last(sparse_tree_t *self);
int sparse_tree_next(sparse_tree_t *self);
int sparse_tree_prev(sparse_tree_t *self);

/* TODO remove this from the public API. */
int newick_converter_alloc(newick_converter_t *self,
        sparse_tree_t *tree, size_t precision, double time_scale, int flags);
int newick_converter_run(newick_converter_t *self, size_t buffer_size, char *buffer);
int newick_converter_free(newick_converter_t *self);

int vcf_converter_alloc(vcf_converter_t *self,
        tree_sequence_t *tree_sequence, unsigned int ploidy, const char *chrom);
int vcf_converter_get_header(vcf_converter_t *self, char **header);
int vcf_converter_next(vcf_converter_t *self, char **record);
int vcf_converter_free(vcf_converter_t *self);
void vcf_converter_print_state(vcf_converter_t *self, FILE *out);

int ld_calc_alloc(ld_calc_t *self, tree_sequence_t *tree_sequence);
int ld_calc_free(ld_calc_t *self);
void ld_calc_print_state(ld_calc_t *self, FILE *out);
int ld_calc_get_r2(ld_calc_t *self, size_t a, size_t b, double *r2);
int ld_calc_get_r2_array(ld_calc_t *self, size_t a, int direction,
        size_t max_mutations, double max_distance,
        double *r2, size_t *num_r2_values);

int hapgen_alloc(hapgen_t *self, tree_sequence_t *tree_sequence);
int hapgen_get_haplotype(hapgen_t *self, node_id_t j, char **haplotype);
int hapgen_free(hapgen_t *self);
void hapgen_print_state(hapgen_t *self, FILE *out);

int vargen_alloc(vargen_t *self, tree_sequence_t *tree_sequence, int flags);
int vargen_next(vargen_t *self, variant_t **variant);
int vargen_free(vargen_t *self);
void vargen_print_state(vargen_t *self, FILE *out);

int recomb_map_alloc(recomb_map_t *self, uint32_t num_loci,
        double sequence_length, double *positions, double *rates,
        size_t size);
int recomb_map_free(recomb_map_t *self);
uint32_t recomb_map_get_num_loci(recomb_map_t *self);
double recomb_map_get_sequence_length(recomb_map_t *self);
double recomb_map_get_per_locus_recombination_rate(recomb_map_t *self);
double recomb_map_get_total_recombination_rate(recomb_map_t *self);
double recomb_map_genetic_to_phys(recomb_map_t *self, double genetic_x);
double recomb_map_phys_to_genetic(recomb_map_t *self, double phys_x);
int recomb_map_genetic_to_phys_bulk(recomb_map_t *self, double *genetic_x,
        size_t n);
size_t recomb_map_get_size(recomb_map_t *self);
int recomb_map_get_positions(recomb_map_t *self, double *positions);
int recomb_map_get_rates(recomb_map_t *self, double *rates);

void recomb_map_print_state(recomb_map_t *self, FILE *out);

int mutgen_alloc(mutgen_t *self, double mutation_rate, gsl_rng *rng,
        int alphabet, size_t mutation_block_size);
int mutgen_free(mutgen_t *self);
/* TODO finalise this interface */
int mutgen_generate_tables_tmp(mutgen_t *self, node_table_t *nodes,
        edge_table_t *edges);
int mutgen_populate_tables(mutgen_t *self, site_table_t *sites,
        mutation_table_t *mutations);
void mutgen_print_state(mutgen_t *self, FILE *out);

double compute_falling_factorial_log(unsigned int  m);


#endif /*__MSPRIME_H__*/
