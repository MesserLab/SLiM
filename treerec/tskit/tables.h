#ifndef TSK_TABLES_H
#define TSK_TABLES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "util.h"
#include "kastore.h"

typedef int32_t node_id_t;
typedef int32_t edge_id_t;
typedef int32_t population_id_t;
typedef int32_t site_id_t;
typedef int32_t mutation_id_t;
typedef int32_t migration_id_t;
typedef int32_t individual_id_t;
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
    table_size_t location_length;
    table_size_t max_location_length;
    table_size_t max_location_length_increment;
    table_size_t metadata_length;
    table_size_t max_metadata_length;
    table_size_t max_metadata_length_increment;
    uint32_t *flags;
    double *location;
    table_size_t *location_offset;
    char *metadata;
    table_size_t *metadata_offset;
} individual_table_t;

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
    individual_id_t *individual;
    char *metadata;
    table_size_t *metadata_offset;
} node_table_t;

typedef struct {
    table_size_t num_rows;
    table_size_t max_rows;
    table_size_t max_rows_increment;
    double *left;
    double *right;
    node_id_t *parent;
    node_id_t *child;
} edge_table_t;

typedef struct {
    table_size_t num_rows;
    table_size_t max_rows;
    table_size_t max_rows_increment;
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
    table_size_t metadata_length;
    table_size_t max_metadata_length;
    table_size_t max_metadata_length_increment;
    char *metadata;
    table_size_t *metadata_offset;
} population_table_t;

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

typedef struct {
    double sequence_length;
    char *file_uuid;
    individual_table_t *individuals;
    node_table_t *nodes;
    edge_table_t *edges;
    migration_table_t *migrations;
    site_table_t *sites;
    mutation_table_t *mutations;
    population_table_t *populations;
    provenance_table_t *provenances;
    struct {
        edge_id_t *edge_insertion_order;
        edge_id_t *edge_removal_order;
        bool malloced_locally;
    } indexes;
    kastore_t *store;
    bool external_tables;
    /* TODO Add in reserved space for future tables. */
} table_collection_t;

typedef struct {
    table_size_t individuals;
    table_size_t nodes;
    table_size_t edges;
    table_size_t migrations;
    table_size_t sites;
    table_size_t mutations;
    table_size_t populations;
    table_size_t provenances;
    /* TODO add reserved space for future tables. */
} table_collection_position_t;

/* Definitions for the basic objects */

typedef struct {
    individual_id_t id;
    uint32_t flags;
    double *location;
    table_size_t location_length;
    const char *metadata;
    table_size_t metadata_length;
    node_id_t *nodes;
    table_size_t nodes_length;
} individual_t;

typedef struct {
    node_id_t id;
    uint32_t flags;
    double time;
    population_id_t population;
    individual_id_t individual;
    const char *metadata;
    table_size_t metadata_length;
} node_t;

typedef struct {
    edge_id_t id;
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
    migration_id_t id;
    population_id_t source;
    population_id_t dest;
    node_id_t node;
    double left;
    double right;
    double time;
} migration_t;

/* FIXME calling this tmp_population_t for now because we already have
 * a population_t in msprime, which is a useful object. Once we have
 * moved this code into tskit we can rename it, probably
 * tsk_population_t */
typedef struct {
    population_id_t id;
    const char *metadata;
    table_size_t metadata_length;
} tmp_population_t;

typedef struct {
    provenance_id_t id;
    const char *timestamp;
    table_size_t timestamp_length;
    const char *record;
    table_size_t record_length;
} provenance_t;

/* TODO Move the simplifier structs into the tables.c file. We don't need to
 * expose this API externally, and can use table_collection_simplify
 * instead */

/* TODO: This should be renamed to tsk_segment_t when we move to tskit.
 * msprime can then #include this and also use it. msprime needs a different
 * segment definition, which it can continue to call 'segment_t' as it
 * doesn't export a C API. */
typedef struct _simplify_segment_t {
    double left;
    double right;
    struct _simplify_segment_t *next;
    node_id_t node;
} simplify_segment_t;

typedef struct _interval_list_t {
    double left;
    double right;
    struct _interval_list_t *next;
} interval_list_t;

typedef struct _mutation_id_list_t {
    mutation_id_t mutation;
    struct _mutation_id_list_t *next;
} mutation_id_list_t;

/* segment overlap finding algorithm */
typedef struct {
    /* The input segments. This buffer is sorted by the algorithm and we also
     * assume that there is space for an extra element at the end */
    simplify_segment_t *segments;
    size_t num_segments;
    size_t index;
    size_t num_overlapping;
    double left;
    double right;
    /* Output buffer */
    size_t max_overlapping;
    simplify_segment_t **overlapping;
} segment_overlapper_t;

typedef struct {
    node_id_t *samples;
    size_t num_samples;
    int flags;
    table_collection_t *tables;
    /* Keep a copy of the input tables */
    table_collection_t input_tables;
    /* State for topology */
    simplify_segment_t **ancestor_map_head;
    simplify_segment_t **ancestor_map_tail;
    node_id_t *node_id_map;
    bool *is_sample;
    /* Segments for a particular parent that are processed together */
    simplify_segment_t *segment_queue;
    size_t segment_queue_size;
    size_t max_segment_queue_size;
    segment_overlapper_t segment_overlapper;
    block_allocator_t segment_heap;
    /* Buffer for output edges. For each child we keep a linked list of
     * intervals, and also store the actual children that have been buffered. */
    block_allocator_t interval_list_heap;
    interval_list_t **child_edge_map_head;
    interval_list_t **child_edge_map_tail;
    node_id_t *buffered_children;
    size_t num_buffered_children;
    /* For each mutation, map its output node. */
    node_id_t *mutation_node_map;
    /* Map of input mutation IDs to output mutation IDs. */
    mutation_id_t *mutation_id_map;
    /* Map of input nodes to the list of input mutation IDs */
    mutation_id_list_t **node_mutation_list_map_head;
    mutation_id_list_t **node_mutation_list_map_tail;
    mutation_id_list_t *node_mutation_list_mem;
    /* When reducing topology, we need a map positions to their corresponding
     * sites.*/
    double *position_lookup;
} simplifier_t;

int node_table_alloc(node_table_t *self, size_t max_rows_increment,
        size_t max_metadata_length_increment);
node_id_t node_table_add_row(node_table_t *self, uint32_t flags, double time,
        population_id_t population, individual_id_t individual,
        const char *metadata, size_t metadata_length);
int node_table_set_columns(node_table_t *self, size_t num_rows, uint32_t *flags, double *time,
        population_id_t *population, individual_id_t *individual,
        const char *metadata, table_size_t *metadata_length);
int node_table_append_columns(node_table_t *self, size_t num_rows, uint32_t *flags, double *time,
        population_id_t *population, individual_id_t *individual,
        const char *metadata, table_size_t *metadata_length);
int node_table_clear(node_table_t *self);
int node_table_truncate(node_table_t *self, size_t num_rows);
int node_table_free(node_table_t *self);
int node_table_dump_text(node_table_t *self, FILE *out);
int node_table_copy(node_table_t *self, node_table_t *dest);
void node_table_print_state(node_table_t *self, FILE *out);
bool node_table_equals(node_table_t *self, node_table_t *other);
int node_table_get_row(node_table_t *self, size_t index, node_t *row);

int edge_table_alloc(edge_table_t *self, size_t max_rows_increment);
edge_id_t edge_table_add_row(edge_table_t *self, double left, double right, node_id_t parent,
        node_id_t child);
int edge_table_set_columns(edge_table_t *self, size_t num_rows, double *left,
        double *right, node_id_t *parent, node_id_t *child);
int edge_table_append_columns(edge_table_t *self, size_t num_rows, double *left,
        double *right, node_id_t *parent, node_id_t *child);
int edge_table_clear(edge_table_t *self);
int edge_table_truncate(edge_table_t *self, size_t num_rows);
int edge_table_free(edge_table_t *self);
int edge_table_dump_text(edge_table_t *self, FILE *out);
int edge_table_copy(edge_table_t *self, edge_table_t *dest);
void edge_table_print_state(edge_table_t *self, FILE *out);
bool edge_table_equals(edge_table_t *self, edge_table_t *other);
int edge_table_get_row(edge_table_t *self, size_t index, edge_t *row);

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
bool site_table_equals(site_table_t *self, site_table_t *other);
int site_table_clear(site_table_t *self);
int site_table_truncate(site_table_t *self, size_t num_rows);
int site_table_copy(site_table_t *self, site_table_t *dest);
int site_table_free(site_table_t *self);
int site_table_dump_text(site_table_t *self, FILE *out);
void site_table_print_state(site_table_t *self, FILE *out);
int site_table_get_row(site_table_t *self, size_t index, site_t *row);

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
bool mutation_table_equals(mutation_table_t *self, mutation_table_t *other);
int mutation_table_clear(mutation_table_t *self);
int mutation_table_truncate(mutation_table_t *self, size_t num_rows);
int mutation_table_copy(mutation_table_t *self, mutation_table_t *dest);
int mutation_table_free(mutation_table_t *self);
int mutation_table_dump_text(mutation_table_t *self, FILE *out);
void mutation_table_print_state(mutation_table_t *self, FILE *out);
int mutation_table_get_row(mutation_table_t *self, size_t index, mutation_t *row);

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
int migration_table_truncate(migration_table_t *self, size_t num_rows);
int migration_table_free(migration_table_t *self);
int migration_table_copy(migration_table_t *self, migration_table_t *dest);
int migration_table_dump_text(migration_table_t *self, FILE *out);
void migration_table_print_state(migration_table_t *self, FILE *out);
bool migration_table_equals(migration_table_t *self, migration_table_t *other);
int migration_table_get_row(migration_table_t *self, size_t index, migration_t *row);

int individual_table_alloc(individual_table_t *self, size_t max_rows_increment,
        size_t max_location_length_increment, size_t max_metadata_length_increment);
individual_id_t individual_table_add_row(individual_table_t *self, uint32_t flags,
        double *location, size_t location_length,
        const char *metadata, size_t metadata_length);
int individual_table_set_columns(individual_table_t *self, size_t num_rows, uint32_t *flags,
        double *location, table_size_t *location_length,
        const char *metadata, table_size_t *metadata_length);
int individual_table_append_columns(individual_table_t *self, size_t num_rows, uint32_t *flags,
        double *location, table_size_t *location_length,
        const char *metadata, table_size_t *metadata_length);
int individual_table_clear(individual_table_t *self);
int individual_table_truncate(individual_table_t *self, size_t num_rows);
int individual_table_free(individual_table_t *self);
int individual_table_dump_text(individual_table_t *self, FILE *out);
int individual_table_copy(individual_table_t *self, individual_table_t *dest);
void individual_table_print_state(individual_table_t *self, FILE *out);
bool individual_table_equals(individual_table_t *self, individual_table_t *other);
int individual_table_get_row(individual_table_t *self, size_t index, individual_t *row);

int population_table_alloc(population_table_t *self, size_t max_rows_increment,
        size_t max_metadata_length_increment);
population_id_t population_table_add_row(population_table_t *self,
        const char *metadata, size_t metadata_length);
int population_table_set_columns(population_table_t *self, size_t num_rows,
        const char *metadata, table_size_t *metadata_offset);
int population_table_append_columns(population_table_t *self, size_t num_rows,
        const char *metadata, table_size_t *metadata_offset);
int population_table_clear(population_table_t *self);
int population_table_truncate(population_table_t *self, size_t num_rows);
int population_table_copy(population_table_t *self, population_table_t *dest);
int population_table_free(population_table_t *self);
void population_table_print_state(population_table_t *self, FILE *out);
int population_table_dump_text(population_table_t *self, FILE *out);
bool population_table_equals(population_table_t *self, population_table_t *other);
int population_table_get_row(population_table_t *self, size_t index, tmp_population_t *row);

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
int provenance_table_truncate(provenance_table_t *self, size_t num_rows);
int provenance_table_copy(provenance_table_t *self, provenance_table_t *dest);
int provenance_table_free(provenance_table_t *self);
int provenance_table_dump_text(provenance_table_t *self, FILE *out);
void provenance_table_print_state(provenance_table_t *self, FILE *out);
bool provenance_table_equals(provenance_table_t *self, provenance_table_t *other);
int provenance_table_get_row(provenance_table_t *self, size_t index, provenance_t *row);

int table_collection_alloc(table_collection_t *self, int flags);
int table_collection_set_tables(table_collection_t *self,
        individual_table_t *individuals, node_table_t *nodes, edge_table_t *edges,
        migration_table_t *migrations, site_table_t *sites,
        mutation_table_t *mutations, population_table_t *populations,
        provenance_table_t *provenances);
int table_collection_print_state(table_collection_t *self, FILE *out);
bool table_collection_is_indexed(table_collection_t *self);
int table_collection_drop_indexes(table_collection_t *self);
int table_collection_build_indexes(table_collection_t *self, int flags);
int table_collection_load(table_collection_t *self, const char *filename, int flags);
int table_collection_dump(table_collection_t *tables, const char *filename, int flags);
int table_collection_copy(table_collection_t *self, table_collection_t *dest);
int table_collection_free(table_collection_t *self);
int table_collection_simplify(table_collection_t *self,
        node_id_t *samples, size_t num_samples, int flags, node_id_t *node_map);
int table_collection_sort(table_collection_t *self, size_t edge_start, int flags);
int table_collection_deduplicate_sites(table_collection_t *tables, int flags);
int table_collection_compute_mutation_parents(table_collection_t *self, int flags);
bool table_collection_equals(table_collection_t *self, table_collection_t *other);
int table_collection_record_position(table_collection_t *self,
        table_collection_position_t *position);
int table_collection_reset_position(table_collection_t *self,
        table_collection_position_t *position);
int table_collection_clear(table_collection_t *self);
int table_collection_check_integrity(table_collection_t *self, int flags);

int segment_overlapper_alloc(segment_overlapper_t *self);
int segment_overlapper_free(segment_overlapper_t *self);
int segment_overlapper_init(segment_overlapper_t *self, simplify_segment_t *segments,
        size_t num_segments);
int segment_overlapper_next(segment_overlapper_t *self,
        double *left, double *right, simplify_segment_t ***overlapping,
        size_t *num_overlapping);

int simplifier_alloc(simplifier_t *self, node_id_t *samples, size_t num_samples,
        table_collection_t *tables, int flags);
int simplifier_free(simplifier_t *self);
int simplifier_run(simplifier_t *self, node_id_t *node_map);
void simplifier_print_state(simplifier_t *self, FILE *out);

int squash_edges(edge_t *edges, size_t num_edges, size_t *num_output_edges);

#ifdef __cplusplus
}
#endif

#endif /* TSK_TABLES_H */
