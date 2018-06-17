#ifndef TSK_TREES_H
#define TSK_TREES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tables.h"

#ifndef MSP_LIBRARY_VERSION_STR
#define MSP_LIBRARY_VERSION_STR "undefined"
#endif

#define MSP_SAMPLE_COUNTS  1
#define MSP_SAMPLE_LISTS   2

#define MSP_16_BIT_GENOTYPES    1

#define MSP_DIR_FORWARD 1
#define MSP_DIR_REVERSE -1

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
        uint32_t *flags;
        double *location;
        table_size_t *location_offset;
        char *metadata;
        table_size_t *metadata_offset;
        node_id_t *individual_nodes_mem;
        node_id_t **individual_nodes;
        table_size_t *individual_nodes_length;
    } individuals;

    struct {
        size_t num_records;
        double *time;
        uint32_t *flags;
        population_id_t *population;
        individual_id_t *individual;
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

    struct {
        size_t num_records;
        size_t metadata_length;
        char *metadata;
        table_size_t *metadata_offset;
    } populations;

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
    union {
        uint8_t *u8;
        uint16_t *u16;
    } genotypes;
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
size_t tree_sequence_get_num_populations(tree_sequence_t *self);
size_t tree_sequence_get_num_individuals(tree_sequence_t *self);
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
int tree_sequence_get_population(tree_sequence_t *self, size_t index,
        tmp_population_t *population);
int tree_sequence_get_individual(tree_sequence_t *self, size_t index,
        individual_t *individual);
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
int sparse_tree_get_newick(sparse_tree_t *self, node_id_t root,
        size_t precision, int flags, size_t buffer_size, char *newick_buffer);

void sparse_tree_print_state(sparse_tree_t *self, FILE *out);
/* Method for positioning the tree in the sequence. */
int sparse_tree_first(sparse_tree_t *self);
int sparse_tree_last(sparse_tree_t *self);
int sparse_tree_next(sparse_tree_t *self);
int sparse_tree_prev(sparse_tree_t *self);

/* TODO remove this from the public API. */
int newick_converter_alloc(newick_converter_t *self,
        sparse_tree_t *tree, size_t precision, int flags);
int newick_converter_run(newick_converter_t *self, node_id_t root,
        size_t buffer_size, char *buffer);
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



#ifdef __cplusplus
}
#endif

#endif /* TSK_TREES_H */
