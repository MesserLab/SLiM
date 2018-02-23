#ifndef TSK_TABLES_H
#define TSK_TABLES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "util.h"
#include "object_heap.h"
#include "avl.h"

typedef int32_t node_id_t;
typedef int32_t edge_id_t;
typedef int32_t population_id_t;
typedef int32_t site_id_t;
typedef int32_t mutation_id_t;
typedef int32_t migration_id_t;
typedef int32_t provenance_id_t;
typedef uint32_t table_size_t;

typedef struct {
    table_size_t num_rows;
    table_size_t max_rows;
    table_size_t max_rows_increment;
    table_size_t ancestral_state_length;
    table_size_t max_ancestral_state_length;
    table_size_t max_ancestral_state_length_increment;
    table_size_t metadata_length;
    table_size_t max_metadata_length;
    table_size_t max_metadata_length_increment;
    double *position;
    char *ancestral_state;
    table_size_t *ancestral_state_offset;
    char *metadata;
    table_size_t *metadata_offset;
} site_table_t;

typedef struct {
    table_size_t num_rows;
    table_size_t max_rows;
    table_size_t max_rows_increment;
    table_size_t derived_state_length;
    table_size_t max_derived_state_length;
    table_size_t max_derived_state_length_increment;
    table_size_t metadata_length;
    table_size_t max_metadata_length;
    table_size_t max_metadata_length_increment;
    node_id_t *node;
    site_id_t *site;
    mutation_id_t *parent;
    char *derived_state;
    table_size_t *derived_state_offset;
    char *metadata;
    table_size_t *metadata_offset;
} mutation_table_t;

typedef struct {
    table_size_t num_rows;
    table_size_t max_rows;
    table_size_t max_rows_increment;
    table_size_t metadata_length;
    table_size_t max_metadata_length;
    table_size_t max_metadata_length_increment;
    uint32_t *flags;
    double *time;
    population_id_t *population;
    char *metadata;
    table_size_t *metadata_offset;
} node_table_t;

typedef struct {
    size_t num_rows;
    size_t max_rows;
    size_t max_rows_increment;
    double *left;
    double *right;
    node_id_t *parent;
    node_id_t *child;
} edge_table_t;

typedef struct {
    size_t num_rows;
    size_t max_rows;
    size_t max_rows_increment;
    population_id_t *source;
    population_id_t *dest;
    node_id_t *node;
    double *left;
    double *right;
    double *time;
} migration_table_t;

typedef struct {
    table_size_t num_rows;
    table_size_t max_rows;
    table_size_t max_rows_increment;
    table_size_t timestamp_length;
    table_size_t max_timestamp_length;
    table_size_t max_timestamp_length_increment;
    table_size_t record_length;
    table_size_t max_record_length;
    table_size_t max_record_length_increment;
    char *timestamp;
    table_size_t *timestamp_offset;
    char *record;
    table_size_t *record_offset;
} provenance_table_t;

/* Definitions for the basic objects */
typedef struct {
    uint32_t flags;
    double time;
    population_id_t population;
    const char *metadata;
    table_size_t metadata_length;
} node_t;

typedef struct {
    node_id_t parent;
    node_id_t child;
    double left;
    double right;
} edge_t;

typedef struct _mutation_t {
    mutation_id_t id;
    site_id_t site;
    node_id_t node;
    mutation_id_t parent;
    const char *derived_state;
    table_size_t derived_state_length;
    const char *metadata;
    table_size_t metadata_length;
    // TODO remove this and change to ID?
    size_t index;
} mutation_t;

typedef struct {
    site_id_t id;
    double position;
    const char *ancestral_state;
    table_size_t ancestral_state_length;
    const char *metadata;
    table_size_t metadata_length;
    mutation_t *mutations;
    table_size_t mutations_length;
} site_t;

typedef struct {
    population_id_t source;
    population_id_t dest;
    node_id_t node;
    double left;
    double right;
    double time;
} migration_t;

typedef struct {
    table_size_t id;
    const char *timestamp;
    table_size_t timestamp_length;
    const char *record;
    table_size_t record_length;
} provenance_t;


/* For the simplify algorithm, we need specialised forms of ancestral
 * segments, sites and mutations */
typedef struct _simplify_segment_t {
    double left;
    double right;
    struct _simplify_segment_t *next;
    node_id_t node;
} simplify_segment_t;

typedef struct _mutation_node_list_t {
    mutation_id_t mutation_id;
    struct _mutation_node_list_t *next;
} mutation_node_list_t;

typedef struct {
    double position;
    mutation_node_list_t *head;
} mutation_position_map_t;

typedef struct {
    node_id_t *samples;
    size_t num_samples;
    int flags;
    double sequence_length;
    /* Keep a copy of the input nodes simplify mapping */
    node_table_t input_nodes;
    /* TODO remove this field when name_offset has been added to node_table. */
    size_t *node_name_offset;
    /* Also keep a copy of the input edges and a buffer to store unsorted edges */
    edge_table_t input_edges;
    edge_t *edge_buffer;
    size_t num_buffered_edges;
    size_t max_buffered_edges;
    /* Input copy of the sites and mutations */
    site_table_t input_sites;
    mutation_table_t input_mutations;
    /* Input/output tables. */
    node_table_t *nodes;
    edge_table_t *edges;
    site_table_t *sites;
    mutation_table_t *mutations;
    /* State for topology */
    simplify_segment_t **ancestor_map;
    node_id_t *node_id_map;
    bool *is_sample;
    avl_tree_t merge_queue;
    object_heap_t segment_heap;
    object_heap_t avl_node_heap;
    size_t segment_buffer_size;
    simplify_segment_t **segment_buffer;
    /* For each mutation, map its output node. */
    node_id_t *mutation_node_map;
    /* Map of input mutation IDs to output mutation IDs. */
    mutation_id_t *mutation_id_map;
    /* For each input node, map position -> list of mutation IDs */
    avl_tree_t *mutation_position_map;
    mutation_node_list_t *mutation_node_list_mem;
    mutation_position_map_t *mutation_position_map_mem;
} simplifier_t;

int sort_tables(node_table_t *nodes, edge_table_t *edges, migration_table_t *migrations,
        site_table_t *sites, mutation_table_t *mutations, size_t edge_start);

int node_table_alloc(node_table_t *self, size_t max_rows_increment,
        size_t max_name_length_increment);
node_id_t node_table_add_row(node_table_t *self, uint32_t flags, double time,
        population_id_t population, const char *name, size_t name_length);
int node_table_set_columns(node_table_t *self, size_t num_rows, uint32_t *flags, double *time,
        population_id_t *population, char *name, table_size_t *name_length);
int node_table_append_columns(node_table_t *self, size_t num_rows, uint32_t *flags, double *time,
        population_id_t *population, char *name, table_size_t *name_length);
int node_table_clear(node_table_t *self);
int node_table_free(node_table_t *self);
void node_table_print_state(node_table_t *self, FILE *out);
bool node_table_equal(node_table_t *self, node_table_t *other);

int edge_table_alloc(edge_table_t *self, size_t max_rows_increment);
edge_id_t edge_table_add_row(edge_table_t *self, double left, double right, node_id_t parent,
        node_id_t child);
int edge_table_set_columns(edge_table_t *self, size_t num_rows, double *left,
        double *right, node_id_t *parent, node_id_t *child);
int edge_table_append_columns(edge_table_t *self, size_t num_rows, double *left,
        double *right, node_id_t *parent, node_id_t *child);
int edge_table_clear(edge_table_t *self);
int edge_table_free(edge_table_t *self);
void edge_table_print_state(edge_table_t *self, FILE *out);
bool edge_table_equal(edge_table_t *self, edge_table_t *other);

int site_table_alloc(site_table_t *self, size_t max_rows_increment,
        size_t max_ancestral_state_length_increment,
        size_t max_metadata_length_increment);
site_id_t site_table_add_row(site_table_t *self, double position,
        const char *ancestral_state, table_size_t ancestral_state_length,
        const char *metadata, table_size_t metadata_length);

int site_table_set_columns(site_table_t *self, size_t num_rows, double *position,
        const char *ancestral_state, table_size_t *ancestral_state_length,
        const char *metadata, table_size_t *metadata_length);
int site_table_append_columns(site_table_t *self, size_t num_rows, double *position,
        const char *ancestral_state, table_size_t *ancestral_state_length,
        const char *metadata, table_size_t *metadata_length);
bool site_table_equal(site_table_t *self, site_table_t *other);
int site_table_clear(site_table_t *self);
int site_table_free(site_table_t *self);
void site_table_print_state(site_table_t *self, FILE *out);

void mutation_table_print_state(mutation_table_t *self, FILE *out);
int mutation_table_alloc(mutation_table_t *self, size_t max_rows_increment,
        size_t max_total_derived_state_length_increment,
        size_t max_total_metadata_length_increment);
mutation_id_t mutation_table_add_row(mutation_table_t *self, site_id_t site,
        node_id_t node, mutation_id_t parent,
        const char *derived_state, table_size_t derived_state_length,
        const char *metadata, table_size_t metadata_length);
int mutation_table_set_columns(mutation_table_t *self, size_t num_rows,
        site_id_t *site, node_id_t *node, mutation_id_t *parent,
        const char *derived_state, table_size_t *derived_state_length,
        const char *metadata, table_size_t *metadata_length);
int mutation_table_append_columns(mutation_table_t *self, size_t num_rows,
        site_id_t *site, node_id_t *node, mutation_id_t *parent,
        const char *derived_state, table_size_t *derived_state_length,
        const char *metadata, table_size_t *metadata_length);
bool mutation_table_equal(mutation_table_t *self, mutation_table_t *other);
int mutation_table_clear(mutation_table_t *self);
int mutation_table_free(mutation_table_t *self);
void mutation_table_print_state(mutation_table_t *self, FILE *out);

int migration_table_alloc(migration_table_t *self, size_t max_rows_increment);
migration_id_t migration_table_add_row(migration_table_t *self, double left,
        double right, node_id_t node, population_id_t source,
        population_id_t dest, double time);
int migration_table_set_columns(migration_table_t *self, size_t num_rows,
        double *left, double *right, node_id_t *node, population_id_t *source,
        population_id_t *dest, double *time);
int migration_table_append_columns(migration_table_t *self, size_t num_rows,
        double *left, double *right, node_id_t *node, population_id_t *source,
        population_id_t *dest, double *time);
int migration_table_clear(migration_table_t *self);
int migration_table_free(migration_table_t *self);
void migration_table_print_state(migration_table_t *self, FILE *out);

int simplifier_alloc(simplifier_t *self, double sequence_length,
        node_id_t *samples, size_t num_samples,
        node_table_t *nodes, edge_table_t *edges, migration_table_t *migrations,
        site_table_t *sites, mutation_table_t *mutations,
        size_t max_buffered_edges, int flags);
int simplifier_free(simplifier_t *self);
int simplifier_run(simplifier_t *self, node_id_t *node_map);
void simplifier_print_state(simplifier_t *self, FILE *out);

int provenance_table_alloc(provenance_table_t *self, size_t max_rows_increment,
        size_t max_timestamp_length_increment,
        size_t max_provenance_length_increment);
provenance_id_t provenance_table_add_row(provenance_table_t *self,
        const char *timestamp, size_t timestamp_length,
        const char *record, size_t record_length);
int provenance_table_set_columns(provenance_table_t *self, size_t num_rows,
       char *timestamp, table_size_t *timestamp_offset,
       char *record, table_size_t *record_offset);
int provenance_table_append_columns(provenance_table_t *self, size_t num_rows,
        char *timestamp, table_size_t *timestamp_offset,
        char *record, table_size_t *record_offset);
int provenance_table_clear(provenance_table_t *self);
int provenance_table_free(provenance_table_t *self);
void provenance_table_print_state(provenance_table_t *self, FILE *out);
bool provenance_table_equal(provenance_table_t *self, provenance_table_t *other);

int squash_edges(edge_t *edges, size_t num_edges, size_t *num_output_edges);

#ifdef __cplusplus
}
#endif

#endif /* TSK_TABLES_H */
