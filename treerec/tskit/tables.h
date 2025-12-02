/*
 * MIT License
 *
 * Copyright (c) 2019-2024 Tskit Developers
 * Copyright (c) 2017-2018 University of Oxford
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
 * @file tables.h
 * @brief Tskit Tables API.
 */
#ifndef TSK_TABLES_H
#define TSK_TABLES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include <kastore.h>

#include <tskit/core.h>

/****************************************************************************/
/* Definitions for the basic objects */
/****************************************************************************/

/**
@brief A single individual defined by a row in the individual table.

@rst
See the :ref:`data model <sec_data_model_definitions>` section for the definition of
an individual and its properties.
@endrst
*/
typedef struct {
    /** @brief Non-negative ID value corresponding to table row. */
    tsk_id_t id;
    /** @brief Bitwise flags. */
    tsk_flags_t flags;
    /** @brief Spatial location. The number of dimensions is defined by
     * ``location_length``. */
    const double *location;
    /** @brief Number of spatial dimensions. */
    tsk_size_t location_length;
    /** @brief IDs of the parents. The number of parents given by ``parents_length``*/
    tsk_id_t *parents;
    /** @brief Number of parents. */
    tsk_size_t parents_length;
    /** @brief Metadata. */
    const char *metadata;
    /** @brief Size of the metadata in bytes. */
    tsk_size_t metadata_length;
    /** @brief An array of the nodes associated with this individual */
    const tsk_id_t *nodes;
    /** @brief The number of nodes associated with this individual*/
    tsk_size_t nodes_length;
} tsk_individual_t;

/**
@brief A single node defined by a row in the node table.

@rst
See the :ref:`data model <sec_data_model_definitions>` section for the definition of
a node and its properties.
@endrst
*/
typedef struct {
    /** @brief Non-negative ID value corresponding to table row. */
    tsk_id_t id;
    /** @brief Bitwise flags. */
    tsk_flags_t flags;
    /** @brief Time. */
    double time;
    /** @brief Population ID. */
    tsk_id_t population;
    /** @brief Individual ID. */
    tsk_id_t individual;
    /** @brief Metadata. */
    const char *metadata;
    /** @brief Size of the metadata in bytes. */
    tsk_size_t metadata_length;
} tsk_node_t;

/**
@brief A single edge defined by a row in the edge table.

@rst
See the :ref:`data model <sec_data_model_definitions>` section for the definition of
an edge and its properties.
@endrst
*/
typedef struct {
    /** @brief Non-negative ID value corresponding to table row. */
    tsk_id_t id;
    /** @brief Parent node ID. */
    tsk_id_t parent;
    /** @brief Child node ID. */
    tsk_id_t child;
    /** @brief Left coordinate. */
    double left;
    /** @brief Right coordinate. */
    double right;
    /** @brief Metadata. */
    const char *metadata;
    /** @brief Size of the metadata in bytes. */
    tsk_size_t metadata_length;
} tsk_edge_t;

/**
@brief A single mutation defined by a row in the mutation table.

@rst
See the :ref:`data model <sec_data_model_definitions>` section for the definition of
a mutation and its properties.
@endrst
*/
typedef struct {
    /** @brief Non-negative ID value corresponding to table row. */
    tsk_id_t id;
    /** @brief Site ID. */
    tsk_id_t site;
    /** @brief Node ID. */
    tsk_id_t node;
    /** @brief Parent mutation ID. */
    tsk_id_t parent;
    /** @brief Mutation time. */
    double time;
    /** @brief Derived state. */
    const char *derived_state;
    /** @brief Size of the derived state in bytes. */
    tsk_size_t derived_state_length;
    /** @brief Metadata. */
    const char *metadata;
    /** @brief Size of the metadata in bytes. */
    tsk_size_t metadata_length;
    /** @brief The ID of the edge that this mutation lies on, or TSK_NULL
      if there is no corresponding edge.*/
    tsk_id_t edge;
    /** @brief Inherited state. */
    const char *inherited_state;
    /** @brief Size of the inherited state in bytes. */
    tsk_size_t inherited_state_length;
} tsk_mutation_t;

/**
@brief A single site defined by a row in the site table.

@rst
See the :ref:`data model <sec_data_model_definitions>` section for the definition of
a site and its properties.
@endrst
*/
typedef struct {
    /** @brief Non-negative ID value corresponding to table row. */
    tsk_id_t id;
    /** @brief Position coordinate. */
    double position;
    /** @brief Ancestral state. */
    const char *ancestral_state;
    /** @brief Ancestral state length in bytes. */
    tsk_size_t ancestral_state_length;
    /** @brief Metadata. */
    const char *metadata;
    /** @brief Metadata length in bytes. */
    tsk_size_t metadata_length;
    /** @brief An array of this site's mutations */
    const tsk_mutation_t *mutations;
    /** @brief The number of mutations at this site */
    tsk_size_t mutations_length;
} tsk_site_t;

/**
@brief A single migration defined by a row in the migration table.

@rst
See the :ref:`data model <sec_data_model_definitions>` section for the definition of
a migration and its properties.
@endrst
*/
typedef struct {
    /** @brief Non-negative ID value corresponding to table row. */
    tsk_id_t id;
    /** @brief Source population ID. */
    tsk_id_t source;
    /** @brief Destination population ID. */
    tsk_id_t dest;
    /** @brief Node ID. */
    tsk_id_t node;
    /** @brief Left coordinate. */
    double left;
    /** @brief Right coordinate. */
    double right;
    /** @brief Time. */
    double time;
    /** @brief Metadata. */
    const char *metadata;
    /** @brief Size of the metadata in bytes. */
    tsk_size_t metadata_length;

} tsk_migration_t;

/**
@brief A single population defined by a row in the population table.

@rst
See the :ref:`data model <sec_data_model_definitions>` section for the definition of
a population and its properties.
@endrst
*/
typedef struct {
    /** @brief Non-negative ID value corresponding to table row. */
    tsk_id_t id;
    /** @brief Metadata. */
    const char *metadata;
    /** @brief Metadata length in bytes. */
    tsk_size_t metadata_length;
} tsk_population_t;

/**
@brief A single provenance defined by a row in the provenance table.

@rst
See the :ref:`data model <sec_data_model_definitions>` section for the definition of
a provenance object and its properties. See the :ref:`sec_provenance` section
for more information on how provenance records should be structured.
@endrst
*/
typedef struct {
    /** @brief Non-negative ID value corresponding to table row. */
    tsk_id_t id;
    /** @brief The timestamp. */
    const char *timestamp;
    /** @brief The timestamp length in bytes. */
    tsk_size_t timestamp_length;
    /** @brief The record. */
    const char *record;
    /** @brief The record length in bytes. */
    tsk_size_t record_length;
} tsk_provenance_t;

/****************************************************************************/
/* Table definitions */
/****************************************************************************/

/**
@brief The individual table.

@rst
See the individual :ref:`table definition <sec_individual_table_definition>` for
details of the columns in this table.
@endrst
*/
typedef struct {
    /** @brief The number of rows in this table. */
    tsk_size_t num_rows;
    tsk_size_t max_rows;
    tsk_size_t max_rows_increment;
    /** @brief The total length of the location column. */
    tsk_size_t location_length;
    tsk_size_t max_location_length;
    tsk_size_t max_location_length_increment;
    /** @brief The total length of the parent column. */
    tsk_size_t parents_length;
    tsk_size_t max_parents_length;
    tsk_size_t max_parents_length_increment;
    /** @brief The total length of the metadata column. */
    tsk_size_t metadata_length;
    tsk_size_t max_metadata_length;
    tsk_size_t max_metadata_length_increment;
    tsk_size_t metadata_schema_length;
    /** @brief The flags column. */
    tsk_flags_t *flags;
    /** @brief The location column. */
    double *location;
    /** @brief The location_offset column. */
    tsk_size_t *location_offset;
    /** @brief The parents column. */
    tsk_id_t *parents;
    /** @brief The parents_offset column. */
    tsk_size_t *parents_offset;
    /** @brief The metadata column. */
    char *metadata;
    /** @brief The metadata_offset column. */
    tsk_size_t *metadata_offset;
    /** @brief The metadata schema */
    char *metadata_schema;
} tsk_individual_table_t;

/**
@brief The node table.

@rst
See the node :ref:`table definition <sec_node_table_definition>` for
details of the columns in this table.
@endrst
*/
typedef struct {
    /** @brief The number of rows in this table. */
    tsk_size_t num_rows;
    tsk_size_t max_rows;
    tsk_size_t max_rows_increment;
    /** @brief The total length of the metadata column. */
    tsk_size_t metadata_length;
    tsk_size_t max_metadata_length;
    tsk_size_t max_metadata_length_increment;
    tsk_size_t metadata_schema_length;
    /** @brief The flags column. */
    tsk_flags_t *flags;
    /** @brief The time column. */
    double *time;
    /** @brief The population column. */
    tsk_id_t *population;
    /** @brief The individual column. */
    tsk_id_t *individual;
    /** @brief The metadata column. */
    char *metadata;
    /** @brief The metadata_offset column. */
    tsk_size_t *metadata_offset;
    /** @brief The metadata schema */
    char *metadata_schema;
} tsk_node_table_t;

/**
@brief The edge table.

@rst
See the edge :ref:`table definition <sec_edge_table_definition>` for
details of the columns in this table.
@endrst
*/
typedef struct {
    /** @brief The number of rows in this table. */
    tsk_size_t num_rows;
    tsk_size_t max_rows;
    tsk_size_t max_rows_increment;
    /** @brief The total length of the metadata column. */
    tsk_size_t metadata_length;
    tsk_size_t max_metadata_length;
    tsk_size_t max_metadata_length_increment;
    tsk_size_t metadata_schema_length;
    /** @brief The left column. */
    double *left;
    /** @brief The right column. */
    double *right;
    /** @brief The parent column. */
    tsk_id_t *parent;
    /** @brief The child column. */
    tsk_id_t *child;
    /** @brief The metadata column. */
    char *metadata;
    /** @brief The metadata_offset column. */
    tsk_size_t *metadata_offset;
    /** @brief The metadata schema */
    char *metadata_schema;
    /** @brief Flags for this table */
    tsk_flags_t options;
} tsk_edge_table_t;

/**
@brief The migration table.

@rst
See the migration :ref:`table definition <sec_migration_table_definition>` for
details of the columns in this table.
@endrst
*/
typedef struct {
    /** @brief The number of rows in this table. */
    tsk_size_t num_rows;
    tsk_size_t max_rows;
    tsk_size_t max_rows_increment;
    /** @brief The total length of the metadata column. */
    tsk_size_t metadata_length;
    tsk_size_t max_metadata_length;
    tsk_size_t max_metadata_length_increment;
    tsk_size_t metadata_schema_length;
    /** @brief The source column. */
    tsk_id_t *source;
    /** @brief The dest column. */
    tsk_id_t *dest;
    /** @brief The node column. */
    tsk_id_t *node;
    /** @brief The left column. */
    double *left;
    /** @brief The right column. */
    double *right;
    /** @brief The time column. */
    double *time;
    /** @brief The metadata column. */
    char *metadata;
    /** @brief The metadata_offset column. */
    tsk_size_t *metadata_offset;
    /** @brief The metadata schema */
    char *metadata_schema;
} tsk_migration_table_t;

/**
@brief The site table.

@rst
See the site :ref:`table definition <sec_site_table_definition>` for
details of the columns in this table.
@endrst
*/
typedef struct {
    /** @brief The number of rows in this table. */
    tsk_size_t num_rows;
    tsk_size_t max_rows;
    tsk_size_t max_rows_increment;
    tsk_size_t ancestral_state_length;
    tsk_size_t max_ancestral_state_length;
    tsk_size_t max_ancestral_state_length_increment;
    /** @brief The total length of the metadata column. */
    tsk_size_t metadata_length;
    tsk_size_t max_metadata_length;
    tsk_size_t max_metadata_length_increment;
    tsk_size_t metadata_schema_length;
    /** @brief The position column. */
    double *position;
    /** @brief The ancestral_state column. */
    char *ancestral_state;
    /** @brief The ancestral_state_offset column. */
    tsk_size_t *ancestral_state_offset;
    /** @brief The metadata column. */
    char *metadata;
    /** @brief The metadata_offset column. */
    tsk_size_t *metadata_offset;
    /** @brief The metadata schema */
    char *metadata_schema;
} tsk_site_table_t;

/**
@brief The mutation table.

@rst
See the mutation :ref:`table definition <sec_mutation_table_definition>` for
details of the columns in this table.
@endrst
*/
typedef struct {
    /** @brief The number of rows in this table. */
    tsk_size_t num_rows;
    tsk_size_t max_rows;
    tsk_size_t max_rows_increment;
    tsk_size_t derived_state_length;
    tsk_size_t max_derived_state_length;
    tsk_size_t max_derived_state_length_increment;
    /** @brief The total length of the metadata column. */
    tsk_size_t metadata_length;
    tsk_size_t max_metadata_length;
    tsk_size_t max_metadata_length_increment;
    tsk_size_t metadata_schema_length;
    /** @brief The node column. */
    tsk_id_t *node;
    /** @brief The site column. */
    tsk_id_t *site;
    /** @brief The parent column. */
    tsk_id_t *parent;
    /** @brief The time column. */
    double *time;
    /** @brief The derived_state column. */
    char *derived_state;
    /** @brief The derived_state_offset column. */
    tsk_size_t *derived_state_offset;
    /** @brief The metadata column. */
    char *metadata;
    /** @brief The metadata_offset column. */
    tsk_size_t *metadata_offset;
    /** @brief The metadata schema */
    char *metadata_schema;
} tsk_mutation_table_t;

/**
@brief The population table.

@rst
See the population :ref:`table definition <sec_population_table_definition>` for
details of the columns in this table.
@endrst
*/
typedef struct {
    /** @brief The number of rows in this table. */
    tsk_size_t num_rows;
    tsk_size_t max_rows;
    tsk_size_t max_rows_increment;
    /** @brief The total length of the metadata column. */
    tsk_size_t metadata_length;
    tsk_size_t max_metadata_length;
    tsk_size_t max_metadata_length_increment;
    tsk_size_t metadata_schema_length;
    /** @brief The metadata column. */
    char *metadata;
    /** @brief The metadata_offset column. */
    tsk_size_t *metadata_offset;
    /** @brief The metadata schema */
    char *metadata_schema;
} tsk_population_table_t;

/**
@brief The provenance table.

@rst
See the provenance :ref:`table definition <sec_provenance_table_definition>` for
details of the columns in this table.
@endrst
*/
typedef struct {
    /** @brief The number of rows in this table. */
    tsk_size_t num_rows;
    tsk_size_t max_rows;
    tsk_size_t max_rows_increment;
    /** @brief The total length of the timestamp column. */
    tsk_size_t timestamp_length;
    tsk_size_t max_timestamp_length;
    tsk_size_t max_timestamp_length_increment;
    /** @brief The total length of the record column. */
    tsk_size_t record_length;
    tsk_size_t max_record_length;
    tsk_size_t max_record_length_increment;
    /** @brief The timestamp column. */
    char *timestamp;
    /** @brief The timestamp_offset column. */
    tsk_size_t *timestamp_offset;
    /** @brief The record column. */
    char *record;
    /** @brief The record_offset column. */
    tsk_size_t *record_offset;
} tsk_provenance_table_t;

typedef struct {
    char *data;
    tsk_size_t data_length;
    char *url;
    tsk_size_t url_length;
    char *metadata;
    tsk_size_t metadata_length;
    char *metadata_schema;
    tsk_size_t metadata_schema_length;
} tsk_reference_sequence_t;

/**
@brief A collection of tables defining the data for a tree sequence.
*/
typedef struct {
    /** @brief The sequence length defining the tree sequence's coordinate space */
    double sequence_length;
    char *file_uuid;
    /** @brief The units of the time dimension */
    char *time_units;
    tsk_size_t time_units_length;
    /** @brief The tree-sequence metadata */
    char *metadata;
    tsk_size_t metadata_length;
    /** @brief The metadata schema */
    char *metadata_schema;
    tsk_size_t metadata_schema_length;
    tsk_reference_sequence_t reference_sequence;
    /** @brief The individual table */
    tsk_individual_table_t individuals;
    /** @brief The node table */
    tsk_node_table_t nodes;
    /** @brief The edge table */
    tsk_edge_table_t edges;
    /** @brief The migration table */
    tsk_migration_table_t migrations;
    /** @brief The site table */
    tsk_site_table_t sites;
    /** @brief The mutation table */
    tsk_mutation_table_t mutations;
    /** @brief The population table */
    tsk_population_table_t populations;
    /** @brief The provenance table */
    tsk_provenance_table_t provenances;
    struct {
        tsk_id_t *edge_insertion_order;
        tsk_id_t *edge_removal_order;
        tsk_size_t num_edges;
    } indexes;
} tsk_table_collection_t;

/**
@brief A bookmark recording the position of all the tables in a table collection.
*/
typedef struct {
    /** @brief The position in the individual table. */
    tsk_size_t individuals;
    /** @brief The position in the node table. */
    tsk_size_t nodes;
    /** @brief The position in the edge table. */
    tsk_size_t edges;
    /** @brief The position in the migration table. */
    tsk_size_t migrations;
    /** @brief The position in the site table. */
    tsk_size_t sites;
    /** @brief The position in the mutation table. */
    tsk_size_t mutations;
    /** @brief The position in the population table. */
    tsk_size_t populations;
    /** @brief The position in the provenance table. */
    tsk_size_t provenances;
} tsk_bookmark_t;

/**
@brief Low-level table sorting method.
*/
typedef struct _tsk_table_sorter_t {
    /** @brief The input tables that are being sorted. */
    tsk_table_collection_t *tables;
    /** @brief The edge sorting function. If set to NULL, edges are not sorted. */
    int (*sort_edges)(struct _tsk_table_sorter_t *self, tsk_size_t start);
    /** @brief The mutation sorting function. */
    int (*sort_mutations)(struct _tsk_table_sorter_t *self);
    /** @brief The individual sorting function. */
    int (*sort_individuals)(struct _tsk_table_sorter_t *self);
    /** @brief An opaque pointer for use by client code */
    void *user_data;
    /** @brief Mapping from input site IDs to output site IDs */
    tsk_id_t *site_id_map;
} tsk_table_sorter_t;

/* Structs for IBD finding.
 * TODO: document properly
 * */

/* Note for tskit developers: it's perhaps a bit confusing/pointless to
 * have the tsk_identity_segment_t struct as well as the internal tsk_segment_t
 * struct (which is identical). However, we may want to implement either
 * segment type differently in future, and since the tsk_identity_segment_t
 * is part of the public API we want to allow the freedom for the different
 * structures to evolve over time */
typedef struct _tsk_identity_segment_t {
    double left;
    double right;
    struct _tsk_identity_segment_t *next;
    tsk_id_t node;
} tsk_identity_segment_t;

typedef struct {
    tsk_size_t num_segments;
    double total_span;
    tsk_identity_segment_t *head;
    tsk_identity_segment_t *tail;
} tsk_identity_segment_list_t;

typedef struct {
    tsk_size_t num_nodes;
    tsk_avl_tree_int_t pair_map;
    tsk_size_t num_segments;
    double total_span;
    tsk_blkalloc_t heap;
    bool store_segments;
    bool store_pairs;
} tsk_identity_segments_t;

/* Diff iterator. */
typedef struct _tsk_edge_list_node_t {
    tsk_edge_t edge;
    struct _tsk_edge_list_node_t *next;
    struct _tsk_edge_list_node_t *prev;
} tsk_edge_list_node_t;

typedef struct {
    tsk_edge_list_node_t *head;
    tsk_edge_list_node_t *tail;
} tsk_edge_list_t;

/****************************************************************************/
/* Common function options */
/****************************************************************************/

/**
@defgroup API_FLAGS_SIMPLIFY_GROUP :c:func:`tsk_table_collection_simplify` and
:c:func:`tsk_treeseq_simplify` specific flags.
@{
*/
/** Remove sites from the output if there are no mutations that reference them.*/
#define TSK_SIMPLIFY_FILTER_SITES (1 << 0)
/** Remove populations from the output if there are no nodes or migrations that
reference them. */
#define TSK_SIMPLIFY_FILTER_POPULATIONS (1 << 1)
/** Remove individuals from the output if there are no nodes that reference them.*/
#define TSK_SIMPLIFY_FILTER_INDIVIDUALS (1 << 2)
/** Do not remove nodes from the output if there are no edges that reference
them and do not reorder nodes so that the samples are nodes 0 to num_samples - 1.
Note that this flag is negated compared to other filtering options because
the default behaviour is to filter unreferenced nodes and reorder to put samples
first.
*/
#define TSK_SIMPLIFY_NO_FILTER_NODES (1 << 7)
/**
Do not update the sample status of nodes as a result of simplification.
*/
#define TSK_SIMPLIFY_NO_UPDATE_SAMPLE_FLAGS (1 << 8)
/**
Reduce the topological information in the tables to the minimum necessary to
represent the trees that contain sites. If there are zero sites this will
result in an zero output edges. When the number of sites is greater than zero,
every tree in the output tree sequence will contain at least one site.
For a given site, the topology of the tree containing that site will be
identical (up to node ID remapping) to the topology of the corresponding tree
in the input.
*/
#define TSK_SIMPLIFY_REDUCE_TO_SITE_TOPOLOGY (1 << 3)
/**
By default simplify removes unary nodes (i.e., nodes with exactly one child)
along the path from samples to root. If this option is specified such unary
nodes will be preserved in the output.
*/
#define TSK_SIMPLIFY_KEEP_UNARY (1 << 4)
/**
By default simplify removes all topology ancestral the MRCAs of the samples.
This option inserts edges from these MRCAs back to the roots of the input
trees.
*/
#define TSK_SIMPLIFY_KEEP_INPUT_ROOTS (1 << 5)
/**
@rst
This acts like :c:macro:`TSK_SIMPLIFY_KEEP_UNARY` (and is mutually exclusive with that
flag). It keeps unary nodes, but only if the unary node is referenced from an individual.
@endrst
*/
#define TSK_SIMPLIFY_KEEP_UNARY_IN_INDIVIDUALS (1 << 6)
/** @} */

/**
@defgroup API_FLAGS_SUBSET_GROUP :c:func:`tsk_table_collection_subset` specific flags.
@{
*/
/**If this flag is provided, the population table will not be changed in any way.*/
#define TSK_SUBSET_NO_CHANGE_POPULATIONS (1 << 0)
/**
@rst
If this flag is provided, then unreferenced sites, individuals, and populations
will not be removed. If so, the site and individual tables will not be changed,
and (unless :c:macro:`TSK_SUBSET_NO_CHANGE_POPULATIONS` is also provided) unreferenced
populations will be placed last, in their original order.
@endrst
*/
#define TSK_SUBSET_KEEP_UNREFERENCED (1 << 1)
/** @} */

/**
@defgroup API_FLAGS_CHECK_INTEGRITY_GROUP :c:func:`tsk_table_collection_check_integrity`
specific flags.
@{
*/
/** Check edge ordering constraints for a tree sequence. */
#define TSK_CHECK_EDGE_ORDERING (1 << 0)
/** Check that sites are in non-decreasing position order. */
#define TSK_CHECK_SITE_ORDERING (1 << 1)
/**Check for any duplicate site positions. */
#define TSK_CHECK_SITE_DUPLICATES (1 << 2)
/**
Check constraints on the ordering of mutations. Any non-null
mutation parents and known times are checked for ordering
constraints.
*/
#define TSK_CHECK_MUTATION_ORDERING (1 << 3)
/**Check individual parents are before children, where specified. */
#define TSK_CHECK_INDIVIDUAL_ORDERING (1 << 4)
/**Check migrations are ordered by time. */
#define TSK_CHECK_MIGRATION_ORDERING (1 << 5)
/**Check that the table indexes exist, and contain valid edge references. */
#define TSK_CHECK_INDEXES (1 << 6)
/**
All checks needed to define a valid tree sequence. Note that
this implies all of the above checks.
*/
#define TSK_CHECK_TREES (1 << 7)
/**
Check mutation parents are consistent with topology.
Implies TSK_CHECK_TREES.
*/
#define TSK_CHECK_MUTATION_PARENTS (1 << 8)

/* Leave room for more positive check flags */
/**
Do not check integrity of references to populations. This
can be safely combined with the other checks.
*/
#define TSK_NO_CHECK_POPULATION_REFS (1 << 12)
/** @} */

/**
@defgroup API_FLAGS_LOAD_INIT_GROUP Flags used by load and init methods.
@{
*/
/* These flags are for table collection load or init, or used as
   flags on table collection or individual tables.
 * As flags are passed though from load to init they share a namespace */
/** Skip reading tables, and only load top-level information. */
#define TSK_LOAD_SKIP_TABLES (1 << 0)
/** Do not load reference sequence. */
#define TSK_LOAD_SKIP_REFERENCE_SEQUENCE (1 << 1)
/**
@rst
Do not allocate space to store metadata in this table. Operations
attempting to add non-empty metadata to the table will fail
with error TSK_ERR_METADATA_DISABLED.
@endrst
*/
#define TSK_TABLE_NO_METADATA (1 << 2)
/**
@rst
Do not allocate space to store metadata in the edge table. Operations
attempting to add non-empty metadata to the edge table will fail
with error TSK_ERR_METADATA_DISABLED.
@endrst
*/
#define TSK_TC_NO_EDGE_METADATA (1 << 3)
/** @} */

/* Flags for dump tables */
/* We may not want to document this flag, but it's useful for testing
 * so we put it high up in the bit space, below the common options */
#define TSK_DUMP_FORCE_OFFSET_64 (1 << 27)

/**
@defgroup API_FLAGS_COPY_GROUP Flags used by :c:func:`tsk_table_collection_copy`.
@{
*/
/** Copy the file uuid, by default this is not copied. */
#define TSK_COPY_FILE_UUID (1 << 0)
/** @} */

/**
@defgroup API_FLAGS_UNION_GROUP Flags used by :c:func:`tsk_table_collection_union`.
@{
*/
/**
By default, union checks that the portion of shared history between
``self`` and ``other``, as implied by ``other_node_mapping``, are indeed
equivalent. It does so by subsetting both ``self`` and ``other`` on the
equivalent nodes specified in ``other_node_mapping``, and then checking for
equality of the subsets.
*/
#define TSK_UNION_NO_CHECK_SHARED (1 << 0)
/**
By default, all nodes new to ``self`` are assigned new populations. If this
option is specified, nodes that are added to ``self`` will retain the
population IDs they have in ``other``.
 */
#define TSK_UNION_NO_ADD_POP (1 << 1)
/**
By default, union only adds edges adjacent to a newly added node;
this option adds all edges.
 */
#define TSK_UNION_ALL_EDGES (1 << 2)
/**
By default, union only adds only mutations on newly added edges, and
sites for those mutations; this option adds all mutations and all sites.
 */
#define TSK_UNION_ALL_MUTATIONS (1 << 3)
/** @} */

/**
@defgroup API_FLAGS_CMP_GROUP Flags used by :c:func:`tsk_table_collection_equals`.
@{
*/
/**
Do not include the top-level tree sequence metadata and metadata schemas
in the comparison.
*/
#define TSK_CMP_IGNORE_TS_METADATA (1 << 0)
/** Do not include the provenance table in comparison. */
#define TSK_CMP_IGNORE_PROVENANCE (1 << 1)
/**
@rst
Do not include metadata when comparing the table collections.
This includes both the top-level tree sequence metadata as well as the
metadata for each of the tables (i.e, :c:macro:`TSK_CMP_IGNORE_TS_METADATA` is implied).
All metadata schemas are also ignored.
@endrst
*/
#define TSK_CMP_IGNORE_METADATA (1 << 2)
/**
@rst
Do not include the timestamp information when comparing the provenance
tables. This has no effect if :c:macro:`TSK_CMP_IGNORE_PROVENANCE` is specified.
@endrst
*/
#define TSK_CMP_IGNORE_TIMESTAMPS (1 << 3)
/**
Do not include any tables in the comparison, thus comparing only the
top-level information of the table collections being compared.
*/
#define TSK_CMP_IGNORE_TABLES (1 << 4)
/** Do not include the reference sequence in the comparison. */
#define TSK_CMP_IGNORE_REFERENCE_SEQUENCE (1 << 5)
/** @} */

/**
@defgroup API_FLAGS_CLEAR_GROUP Flags used by :c:func:`tsk_table_collection_clear`.
@{
*/
/** Additionally clear the table metadata schemas*/
#define TSK_CLEAR_METADATA_SCHEMAS (1 << 0)
/** Additionally clear the tree-sequence metadata and schema*/
#define TSK_CLEAR_TS_METADATA_AND_SCHEMA (1 << 1)
/** Additionally clear the provenance table*/
#define TSK_CLEAR_PROVENANCE (1 << 2)
/** @} */

/* For the edge diff iterator */
#define TSK_INCLUDE_TERMINAL (1 << 0)

/** @brief Value returned by seeking methods when they have successfully
    seeked to a non-null tree.

    @ingroup TREE_API_SEEKING_GROUP
*/
#define TSK_TREE_OK 1

/****************************************************************************/
/* Function signatures */
/****************************************************************************/

/**
@defgroup INDIVIDUAL_TABLE_API_GROUP Individual table API.
@{
*/

/**
@brief Initialises the table by allocating the internal memory.

@rst
This must be called before any operations are performed on the table.
See the :ref:`sec_c_api_overview_structure` for details on how objects
are initialised and freed.
@endrst

@param self A pointer to an uninitialised tsk_individual_table_t object.
@param options Allocation time options. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_individual_table_init(tsk_individual_table_t *self, tsk_flags_t options);

/**
@brief Free the internal memory for the specified table.

@param self A pointer to an initialised tsk_individual_table_t object.
@return Always returns 0.
*/
int tsk_individual_table_free(tsk_individual_table_t *self);

/**
@brief Adds a row to this individual table.

@rst
Add a new individual with the specified ``flags``, ``location``, ``parents`` and
``metadata`` to the table. Copies of the ``location``, ``parents`` and ``metadata``
parameters are taken immediately. See the :ref:`table definition
<sec_individual_table_definition>` for details of the columns in this table.
@endrst

@param self A pointer to a tsk_individual_table_t object.
@param flags The bitwise flags for the new individual.
@param location A pointer to a double array representing the spatial location
    of the new individual. Can be ``NULL`` if ``location_length`` is 0.
@param location_length The number of dimensions in the locations position.
    Note this the number of elements in the corresponding double array
    not the number of bytes.
@param parents A pointer to a ``tsk_id`` array representing the parents
    of the new individual. Can be ``NULL`` if ``parents_length`` is 0.
@param parents_length The number of parents.
    Note this the number of elements in the corresponding ``tsk_id`` array
    not the number of bytes.
@param metadata The metadata to be associated with the new individual. This
    is a pointer to arbitrary memory. Can be ``NULL`` if ``metadata_length`` is 0.
@param metadata_length The size of the metadata array in bytes.
@return Return the ID of the newly added individual on success,
    or a negative value on failure.
*/
tsk_id_t tsk_individual_table_add_row(tsk_individual_table_t *self, tsk_flags_t flags,
    const double *location, tsk_size_t location_length, const tsk_id_t *parents,
    tsk_size_t parents_length, const char *metadata, tsk_size_t metadata_length);

/**
@brief Updates the row at the specified index.

@rst
Rewrite the row at the specified index in this table to use the specified
values. Copies of the ``location``, ``parents`` and ``metadata``
parameters are taken immediately. See the :ref:`table definition
<sec_individual_table_definition>` for details of the columns in this table.

.. warning::
    Because of the way that ragged columns are encoded, this method requires a
    full rewrite of the internal column memory in worst case, and would
    therefore be inefficient for bulk updates for such columns. However, if the
    sizes of all ragged column values are unchanged in the updated row, this
    method is guaranteed to only update the memory for the row in question.
@endrst

@param self A pointer to a tsk_individual_table_t object.
@param index The row to update.
@param flags The bitwise flags for the individual.
@param location A pointer to a double array representing the spatial location
    of the new individual. Can be ``NULL`` if ``location_length`` is 0.
@param location_length The number of dimensions in the locations position.
    Note this the number of elements in the corresponding double array
    not the number of bytes.
@param parents A pointer to a ``tsk_id`` array representing the parents
    of the new individual. Can be ``NULL`` if ``parents_length`` is 0.
@param parents_length The number of parents.
    Note this the number of elements in the corresponding ``tsk_id`` array
    not the number of bytes.
@param metadata The metadata to be associated with the new individual. This
    is a pointer to arbitrary memory. Can be ``NULL`` if ``metadata_length`` is 0.
@param metadata_length The size of the metadata array in bytes.
@return Return 0 on success or a negative value on failure.
*/
int tsk_individual_table_update_row(tsk_individual_table_t *self, tsk_id_t index,
    tsk_flags_t flags, const double *location, tsk_size_t location_length,
    const tsk_id_t *parents, tsk_size_t parents_length, const char *metadata,
    tsk_size_t metadata_length);

/**
@brief Clears this table, setting the number of rows to zero.

@rst
No memory is freed as a result of this operation; please use
:c:func:`tsk_individual_table_free` to free the table's internal resources. Note that the
metadata schema is not cleared.
@endrst

@param self A pointer to a tsk_individual_table_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_individual_table_clear(tsk_individual_table_t *self);

/**
@brief Truncates this table so that only the first num_rows are retained.

@param self A pointer to a tsk_individual_table_t object.
@param num_rows The number of rows to retain in the table.
@return Return 0 on success or a negative value on failure.
*/
int tsk_individual_table_truncate(tsk_individual_table_t *self, tsk_size_t num_rows);

/**
@brief Extends this table by appending rows copied from another table.

@rst
Appends the rows at the specified indexes from the table ``other`` to the end of this
table. Row indexes can be repeated and in any order. If ``row_indexes`` is NULL, append
the first ``num_rows`` from ``other`` to this table. Note that metadata is copied as-is
and is not checked for compatibility with any existing schema on this table.
@endrst

@param self A pointer to a tsk_individual_table_t object where rows are to be added.
@param other A pointer to a tsk_individual_table_t object where rows are copied from.
@param num_rows The number of rows from ``other`` to append to this table.
@param row_indexes Array of row indexes in ``other``. If ``NULL`` is passed then the
    first ``num_rows`` of ``other`` are used.
@param options Bitwise option flags. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_individual_table_extend(tsk_individual_table_t *self,
    const tsk_individual_table_t *other, tsk_size_t num_rows,
    const tsk_id_t *row_indexes, tsk_flags_t options);

/**
@brief Subset this table by keeping rows according to a boolean mask.

@rst
Deletes rows from this table and optionally return the mapping from IDs in
the current table to the updated table. Rows are kept or deleted according to
the specified boolean array ``keep`` such that for each row ``j`` if
``keep[j]`` is false (zero) the row is deleted, and otherwise the row is
retained. Thus, ``keep`` must be an array of at least ``num_rows``
:c:type:`bool` values.

If the ``id_map`` argument is non-null, this array will be updated to represent
the mapping between IDs before and after row deletion. For row ``j``,
``id_map[j]`` will contain the new ID for row ``j`` if it is retained, or
:c:macro:`TSK_NULL` if the row has been removed. Thus, ``id_map`` must be an
array of at least ``num_rows`` :c:type:`tsk_id_t` values.

The values in the ``parents`` column are updated according to this map, so that
reference integrity within the table is maintained. As a consequence of this,
the values in the ``parents`` column for kept rows are bounds-checked and an
error raised if they are not valid. Rows that are deleted are not checked for
parent ID integrity.

If an attempt is made to delete rows that are referred to by the ``parents``
column of rows that are retained, an error is raised.

These error conditions are checked before any alterations to the table are
made.

.. warning::
    C++ users need to be careful to specify the correct type when
    passing in values for the ``keep`` array,
    using ``std::vector<tsk_bool_t>`` and not ``std::vector<bool>``,
    as the latter may not be correct size.

@endrst

@param self A pointer to a tsk_individual_table_t object.
@param keep Array of boolean flags describing whether a particular
    row should be kept or not. Must be at least ``num_rows`` long.
@param options Bitwise option flags. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@param id_map An array in which to store the mapping between new
    and old IDs. If NULL, this will be ignored.
@return Return 0 on success or a negative value on failure.
*/
int tsk_individual_table_keep_rows(tsk_individual_table_t *self, const tsk_bool_t *keep,
    tsk_flags_t options, tsk_id_t *id_map);

/**
@brief Returns true if the data in the specified table is identical to the data
       in this table.

@rst

**Options**

Options to control the comparison can be specified by providing one or
more of the following bitwise flags. By default (options=0) tables are
considered equal if they are byte-wise identical in all columns,
and their metadata schemas are byte-wise identical.

- :c:macro:`TSK_CMP_IGNORE_METADATA`
@endrst

@param self A pointer to a tsk_individual_table_t object.
@param other A pointer to a tsk_individual_table_t object.
@param options Bitwise comparison options.
@return Return true if the specified table is equal to this table.
*/
bool tsk_individual_table_equals(const tsk_individual_table_t *self,
    const tsk_individual_table_t *other, tsk_flags_t options);

/**
@brief Copies the state of this table into the specified destination.

@rst
By default the method initialises the specified destination table. If the
destination is already initialised, the :c:macro:`TSK_NO_INIT` option should
be supplied to avoid leaking memory.

Indexes that are present are also copied to the destination table.
@endrst

@param self A pointer to a tsk_individual_table_t object.
@param dest A pointer to a tsk_individual_table_t object. If the TSK_NO_INIT
option is specified, this must be an initialised individual table. If not, it must be an
uninitialised individual table.
@param options Bitwise option flags.
@return Return 0 on success or a negative value on failure.
*/
int tsk_individual_table_copy(const tsk_individual_table_t *self,
    tsk_individual_table_t *dest, tsk_flags_t options);

/**
@brief Get the row at the specified index.

@rst
Updates the specified individual struct to reflect the values in the specified row.
Pointers to memory within this struct are handled by the table and should **not**
be freed by client code. These pointers are guaranteed to be valid until the
next operation that modifies the table (e.g., by adding a new row), but not afterwards.
@endrst

@param self A pointer to a tsk_individual_table_t object.
@param index The requested table row.
@param row A pointer to a tsk_individual_t struct that is updated to reflect the
    values in the specified row.
@return Return 0 on success or a negative value on failure.
*/
int tsk_individual_table_get_row(
    const tsk_individual_table_t *self, tsk_id_t index, tsk_individual_t *row);

/**
@brief Set the metadata schema

@rst
Copies the metadata schema string to this table, replacing any existing.
@endrst

@param self A pointer to a tsk_individual_table_t object.
@param metadata_schema A pointer to a char array.
@param metadata_schema_length The size of the metadata schema in bytes.
@return Return 0 on success or a negative value on failure.
*/
int tsk_individual_table_set_metadata_schema(tsk_individual_table_t *self,
    const char *metadata_schema, tsk_size_t metadata_schema_length);

/**
@brief Print out the state of this table to the specified stream.

This method is intended for debugging purposes and should not be used
in production code. The format of the output should **not** be depended
on and may change arbitrarily between versions.

@param self A pointer to a tsk_individual_table_t object.
@param out The stream to write the summary to.
*/
void tsk_individual_table_print_state(const tsk_individual_table_t *self, FILE *out);

/**
@brief Replace this table's data by copying from a set of column arrays

@rst
Clears the data columns of this table and then copies column data from the specified
set of arrays. The supplied arrays should all contain data on the same number of rows.
The metadata schema is not affected.
@endrst

@param self A pointer to a tsk_individual_table_t object.
@param num_rows The number of rows to copy from the specifed arrays.
@param flags The array of tsk_flag_t flag values to be copied.
@param location The array of double location values to be copied.
@param location_offset The array of tsk_size_t location offset values to be copied.
@param parents The array of tsk_id_t parent values to be copied.
@param parents_offset The array of tsk_size_t parent offset values to be copied.
@param metadata The array of char metadata values to be copied.
@param metadata_offset The array of tsk_size_t metadata offset values to be copied.
@return Return 0 on success or a negative value on failure.
*/
int tsk_individual_table_set_columns(tsk_individual_table_t *self, tsk_size_t num_rows,
    const tsk_flags_t *flags, const double *location, const tsk_size_t *location_offset,
    const tsk_id_t *parents, const tsk_size_t *parents_offset, const char *metadata,
    const tsk_size_t *metadata_offset);

/**
@brief Extends this table by copying from a set of column arrays

@rst
Copies column data from the specified set of arrays to create new rows at the end of the
table. The supplied arrays should all contain data on the same number of rows. The
metadata schema is not affected.
@endrst

@param self A pointer to a tsk_individual_table_t object.
@param num_rows The number of rows to copy from the specifed arrays
@param flags The array of tsk_flag_t flag values to be copied.
@param location The array of double location values to be copied.
@param location_offset The array of tsk_size_t location offset values to be copied.
@param parents The array of tsk_id_t parent values to be copied.
@param parents_offset The array of tsk_size_t parent offset values to be copied.
@param metadata The array of char metadata values to be copied.
@param metadata_offset The array of tsk_size_t metadata offset values to be copied.
@return Return 0 on success or a negative value on failure.
*/
int tsk_individual_table_append_columns(tsk_individual_table_t *self,
    tsk_size_t num_rows, const tsk_flags_t *flags, const double *location,
    const tsk_size_t *location_offset, const tsk_id_t *parents,
    const tsk_size_t *parents_offset, const char *metadata,
    const tsk_size_t *metadata_offset);

/**
@brief Controls the pre-allocation strategy for this table

@rst
Set a fixed pre-allocation size, or use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_individual_table_t object.
@param max_rows_increment The number of rows to pre-allocate, or zero for the default
    doubling strategy.
@return Return 0 on success or a negative value on failure.
*/
int tsk_individual_table_set_max_rows_increment(
    tsk_individual_table_t *self, tsk_size_t max_rows_increment);

/**
@brief Controls the pre-allocation strategy for the metadata column

@rst
Set a fixed pre-allocation size, or use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_individual_table_t object.
@param max_metadata_length_increment The number of bytes to pre-allocate, or zero for
the default doubling strategy.
@return Return 0 on success or a negative value on failure.
*/
int tsk_individual_table_set_max_metadata_length_increment(
    tsk_individual_table_t *self, tsk_size_t max_metadata_length_increment);

/**
@brief Controls the pre-allocation strategy for the location column

@rst
Set a fixed pre-allocation size, or use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_individual_table_t object.
@param max_location_length_increment The number of bytes to pre-allocate, or zero for
the default doubling strategy.
@return Return 0 on success or a negative value on failure.
*/
int tsk_individual_table_set_max_location_length_increment(
    tsk_individual_table_t *self, tsk_size_t max_location_length_increment);

/**
@brief Controls the pre-allocation strategy for the parents column

@rst
Set a fixed pre-allocation size, or use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_individual_table_t object.
@param max_parents_length_increment The number of bytes to pre-allocate, or zero for
the default doubling strategy.
@return Return 0 on success or a negative value on failure.
*/
int tsk_individual_table_set_max_parents_length_increment(
    tsk_individual_table_t *self, tsk_size_t max_parents_length_increment);

/** @} */

/* Undocumented methods */

int tsk_individual_table_dump_text(const tsk_individual_table_t *self, FILE *out);
/**
@defgroup NODE_TABLE_API_GROUP Node table API.
@{
*/

/**
@brief Initialises the table by allocating the internal memory.

@rst
This must be called before any operations are performed on the table.
See the :ref:`sec_c_api_overview_structure` for details on how objects
are initialised and freed.
@endrst

@param self A pointer to an uninitialised tsk_node_table_t object.
@param options Allocation time options. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_node_table_init(tsk_node_table_t *self, tsk_flags_t options);

/**
@brief Free the internal memory for the specified table.

@param self A pointer to an initialised tsk_node_table_t object.
@return Always returns 0.
*/
int tsk_node_table_free(tsk_node_table_t *self);

/**
@brief Adds a row to this node table.

@rst
Add a new node with the specified ``flags``, ``time``, ``population``,
``individual`` and ``metadata`` to the table. A copy of the ``metadata`` parameter
is taken immediately. See the :ref:`table definition <sec_node_table_definition>`
for details of the columns in this table.
@endrst

@param self A pointer to a tsk_node_table_t object.
@param flags The bitwise flags for the new node.
@param time The time for the new node.
@param population The population for the new node. Set to TSK_NULL if not
known.
@param individual The individual for the new node. Set to TSK_NULL if not
known.
@param metadata The metadata to be associated with the new node. This
    is a pointer to arbitrary memory. Can be ``NULL`` if ``metadata_length`` is 0.
@param metadata_length The size of the metadata array in bytes.
@return Return the ID of the newly added node on success,
    or a negative value on failure.
*/
tsk_id_t tsk_node_table_add_row(tsk_node_table_t *self, tsk_flags_t flags, double time,
    tsk_id_t population, tsk_id_t individual, const char *metadata,
    tsk_size_t metadata_length);

/**
@brief Updates the row at the specified index.

@rst
Rewrite the row at the specified index in this table to use the specified
values. A copy of the ``metadata`` parameter is taken immediately. See the
:ref:`table definition <sec_node_table_definition>` for details of the columns
in this table.

.. warning::
    Because of the way that ragged columns are encoded, this method requires a
    full rewrite of the internal column memory in worst case, and would
    therefore be inefficient for bulk updates for such columns. However, if the
    sizes of all ragged column values are unchanged in the updated row, this
    method is guaranteed to only update the memory for the row in question.
@endrst

@param self A pointer to a tsk_node_table_t object.
@param index The row to update.
@param flags The bitwise flags for the node.
@param time The time for the node.
@param population The population for the node. Set to TSK_NULL if not known.
@param individual The individual for the node. Set to TSK_NULL if not known.
@param metadata The metadata to be associated with the node. This
    is a pointer to arbitrary memory. Can be ``NULL`` if ``metadata_length`` is 0.
@param metadata_length The size of the metadata array in bytes.
@return Return 0 on success or a negative value on failure.
*/
int tsk_node_table_update_row(tsk_node_table_t *self, tsk_id_t index, tsk_flags_t flags,
    double time, tsk_id_t population, tsk_id_t individual, const char *metadata,
    tsk_size_t metadata_length);

/**
@brief Clears this table, setting the number of rows to zero.

@rst
No memory is freed as a result of this operation; please use
:c:func:`tsk_node_table_free` to free the table's internal resources. Note that the
metadata schema is not cleared.
@endrst

@param self A pointer to a tsk_node_table_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_node_table_clear(tsk_node_table_t *self);

/**
@brief Truncates this table so that only the first num_rows are retained.

@param self A pointer to a tsk_node_table_t object.
@param num_rows The number of rows to retain in the table.
@return Return 0 on success or a negative value on failure.
*/
int tsk_node_table_truncate(tsk_node_table_t *self, tsk_size_t num_rows);

/**
@brief Extends this table by appending rows copied from another table.

@rst
Appends the rows at the specified indexes from the table ``other`` to the end of this
table. Row indexes can be repeated and in any order. If ``row_indexes`` is NULL, append
the first ``num_rows`` from ``other`` to this table. Note that metadata is copied as-is
and is not checked for compatibility with any existing schema on this table.
@endrst

@param self A pointer to a tsk_node_table_t object where rows are to be added.
@param other A pointer to a tsk_node_table_t object where rows are copied from.
@param num_rows The number of rows from ``other`` to append to this table.
@param row_indexes Array of row indexes in ``other``. If ``NULL`` is passed then the
    first ``num_rows`` of ``other`` are used.
@param options Bitwise option flags. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_node_table_extend(tsk_node_table_t *self, const tsk_node_table_t *other,
    tsk_size_t num_rows, const tsk_id_t *row_indexes, tsk_flags_t options);

/**
@brief Subset this table by keeping rows according to a boolean mask.

@rst
Deletes rows from this table and optionally return the mapping from IDs in
the current table to the updated table. Rows are kept or deleted according to
the specified boolean array ``keep`` such that for each row ``j`` if
``keep[j]`` is false (zero) the row is deleted, and otherwise the row is
retained. Thus, ``keep`` must be an array of at least ``num_rows``
:c:type:`bool` values.

If the ``id_map`` argument is non-null, this array will be updated to represent
the mapping between IDs before and after row deletion. For row ``j``,
``id_map[j]`` will contain the new ID for row ``j`` if it is retained, or
:c:macro:`TSK_NULL` if the row has been removed. Thus, ``id_map`` must be an
array of at least ``num_rows`` :c:type:`tsk_id_t` values.

.. warning::
    C++ users need to be careful to specify the correct type when
    passing in values for the ``keep`` array,
    using ``std::vector<tsk_bool_t>`` and not ``std::vector<bool>``,
    as the latter may not be correct size.

@endrst

@param self A pointer to a tsk_node_table_t object.
@param keep Array of boolean flags describing whether a particular
    row should be kept or not. Must be at least ``num_rows`` long.
@param options Bitwise option flags. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@param id_map An array in which to store the mapping between new
    and old IDs. If NULL, this will be ignored.
@return Return 0 on success or a negative value on failure.
*/
int tsk_node_table_keep_rows(tsk_node_table_t *self, const tsk_bool_t *keep,
    tsk_flags_t options, tsk_id_t *id_map);

/**
@brief Returns true if the data in the specified table is identical to the data
       in this table.

@rst

**Options**

Options to control the comparison can be specified by providing one or
more of the following bitwise flags. By default (options=0) tables are
considered equal if they are byte-wise identical in all columns,
and their metadata schemas are byte-wise identical.

- :c:macro:`TSK_CMP_IGNORE_METADATA`
@endrst

@param self A pointer to a tsk_node_table_t object.
@param other A pointer to a tsk_node_table_t object.
@param options Bitwise comparison options.
@return Return true if the specified table is equal to this table.
*/
bool tsk_node_table_equals(
    const tsk_node_table_t *self, const tsk_node_table_t *other, tsk_flags_t options);

/**
@brief Copies the state of this table into the specified destination.

@rst
By default the method initialises the specified destination table. If the
destination is already initialised, the TSK_NO_INIT option should
be supplied to avoid leaking memory.
@endrst

@param self A pointer to a tsk_node_table_t object.
@param dest A pointer to a tsk_node_table_t object. If the TSK_NO_INIT option
    is specified, this must be an initialised node table. If not, it must
    be an uninitialised node table.
@param options Bitwise option flags.
@return Return 0 on success or a negative value on failure.
*/
int tsk_node_table_copy(
    const tsk_node_table_t *self, tsk_node_table_t *dest, tsk_flags_t options);

/**
@brief Get the row at the specified index.

@rst
Updates the specified node struct to reflect the values in the specified row.
Pointers to memory within this struct are handled by the table and should **not**
be freed by client code. These pointers are guaranteed to be valid until the
next operation that modifies the table (e.g., by adding a new row), but not afterwards.
@endrst

@param self A pointer to a tsk_node_table_t object.
@param index The requested table row.
@param row A pointer to a tsk_node_t struct that is updated to reflect the
    values in the specified row.
@return Return 0 on success or a negative value on failure.
*/
int tsk_node_table_get_row(
    const tsk_node_table_t *self, tsk_id_t index, tsk_node_t *row);

/**
@brief Set the metadata schema
@rst
Copies the metadata schema string to this table, replacing any existing.
@endrst
@param self A pointer to a tsk_node_table_t object.
@param metadata_schema A pointer to a char array.
@param metadata_schema_length The size of the metadata schema in bytes.
@return Return 0 on success or a negative value on failure.
*/
int tsk_node_table_set_metadata_schema(tsk_node_table_t *self,
    const char *metadata_schema, tsk_size_t metadata_schema_length);

/**
@brief Print out the state of this table to the specified stream.

This method is intended for debugging purposes and should not be used
in production code. The format of the output should **not** be depended
on and may change arbitrarily between versions.

@param self A pointer to a tsk_node_table_t object.
@param out The stream to write the summary to.
*/
void tsk_node_table_print_state(const tsk_node_table_t *self, FILE *out);

/**
@brief Replace this table's data by copying from a set of column arrays

@rst
Clears the data columns of this table and then copies column data from the specified
set of arrays. The supplied arrays should all contain data on the same number of rows.
The metadata schema is not affected.
@endrst

@param self A pointer to a tsk_node_table_t object.
@param num_rows The number of rows to copy from the specifed arrays.
@param flags The array of tsk_flag_t values to be copied.
@param time The array of double time values to be copied.
@param population The array of tsk_id_t population values to be copied.
@param individual The array of tsk_id_t individual values to be copied.
@param metadata The array of char metadata values to be copied.
@param metadata_offset The array of tsk_size_t metadata offset values to be copied.
@return Return 0 on success or a negative value on failure.
*/
int tsk_node_table_set_columns(tsk_node_table_t *self, tsk_size_t num_rows,
    const tsk_flags_t *flags, const double *time, const tsk_id_t *population,
    const tsk_id_t *individual, const char *metadata, const tsk_size_t *metadata_offset);

/**
@brief Extends this table by copying from a set of column arrays

@rst
Copies column data from the specified set of arrays to create new rows at the end of the
table. The supplied arrays should all contain data on the same number of rows. The
metadata schema is not affected.
@endrst

@param self A pointer to a tsk_node_table_t object.
@param num_rows The number of rows to copy from the specifed arrays
@param flags The array of tsk_flag_t values to be copied.
@param time The array of double time values to be copied.
@param population The array of tsk_id_t population values to be copied.
@param individual The array of tsk_id_t individual values to be copied.
@param metadata The array of char metadata values to be copied.
@param metadata_offset The array of tsk_size_t metadata offset values to be copied.
@return Return 0 on success or a negative value on failure.
*/
int tsk_node_table_append_columns(tsk_node_table_t *self, tsk_size_t num_rows,
    const tsk_flags_t *flags, const double *time, const tsk_id_t *population,
    const tsk_id_t *individual, const char *metadata, const tsk_size_t *metadata_offset);

/**
@brief Controls the pre-allocation strategy for this table

@rst
Set a fixed pre-allocation size, or use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_node_table_t object.
@param max_rows_increment The number of rows to pre-allocate, or zero for the default
    doubling strategy.
@return Return 0 on success or a negative value on failure.
*/

int tsk_node_table_set_max_rows_increment(
    tsk_node_table_t *self, tsk_size_t max_rows_increment);

/**
@brief Controls the pre-allocation strategy for the metadata column

@rst
Set a fixed pre-allocation size, or use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_node_table_t object.
@param max_metadata_length_increment The number of bytes to pre-allocate, or zero for
the default doubling strategy.
@return Return 0 on success or a negative value on failure.
*/
int tsk_node_table_set_max_metadata_length_increment(
    tsk_node_table_t *self, tsk_size_t max_metadata_length_increment);

/** @} */

/* Undocumented methods */

int tsk_node_table_dump_text(const tsk_node_table_t *self, FILE *out);

/**
@defgroup EDGE_TABLE_API_GROUP Edge table API.
@{
*/

/**
@brief Initialises the table by allocating the internal memory.

@rst
This must be called before any operations are performed on the table.
See the :ref:`sec_c_api_overview_structure` for details on how objects
are initialised and freed.

**Options**

Options can be specified by providing one or more of the following bitwise
flags:

- :c:macro:`TSK_TABLE_NO_METADATA`
@endrst

@param self A pointer to an uninitialised tsk_edge_table_t object.
@param options Allocation time options.
@return Return 0 on success or a negative value on failure.
*/
int tsk_edge_table_init(tsk_edge_table_t *self, tsk_flags_t options);

/**
@brief Free the internal memory for the specified table.

@param self A pointer to an initialised tsk_edge_table_t object.
@return Always returns 0.
*/
int tsk_edge_table_free(tsk_edge_table_t *self);

/**
@brief Adds a row to this edge table.

@rst
Add a new edge with the specified ``left``, ``right``, ``parent``, ``child`` and
``metadata`` to the table. See the :ref:`table definition <sec_edge_table_definition>`
for details of the columns in this table.
@endrst

@param self A pointer to a tsk_edge_table_t object.
@param left The left coordinate for the new edge.
@param right The right coordinate for the new edge.
@param parent The parent node for the new edge.
@param child The child node for the new edge.
@param metadata The metadata to be associated with the new edge. This
    is a pointer to arbitrary memory. Can be ``NULL`` if ``metadata_length`` is 0.
@param metadata_length The size of the metadata array in bytes.

@return Return the ID of the newly added edge on success,
    or a negative value on failure.
*/
tsk_id_t tsk_edge_table_add_row(tsk_edge_table_t *self, double left, double right,
    tsk_id_t parent, tsk_id_t child, const char *metadata, tsk_size_t metadata_length);

/**
@brief Updates the row at the specified index.

@rst
Rewrite the row at the specified index in this table to use the specified
values. A copy of the ``metadata`` parameter is taken immediately. See the
:ref:`table definition <sec_edge_table_definition>` for details of the columns
in this table.

.. warning::
    Because of the way that ragged columns are encoded, this method requires a
    full rewrite of the internal column memory in worst case, and would
    therefore be inefficient for bulk updates for such columns. However, if the
    sizes of all ragged column values are unchanged in the updated row, this
    method is guaranteed to only update the memory for the row in question.
@endrst

@param self A pointer to a tsk_edge_table_t object.
@param index The row to update.
@param left The left coordinate for the edge.
@param right The right coordinate for the edge.
@param parent The parent node for the edge.
@param child The child node for the edge.
@param metadata The metadata to be associated with the edge. This
    is a pointer to arbitrary memory. Can be ``NULL`` if ``metadata_length`` is 0.
@param metadata_length The size of the metadata array in bytes.
@return Return 0 on success or a negative value on failure.
*/
int tsk_edge_table_update_row(tsk_edge_table_t *self, tsk_id_t index, double left,
    double right, tsk_id_t parent, tsk_id_t child, const char *metadata,
    tsk_size_t metadata_length);

/**
@brief Clears this table, setting the number of rows to zero.

@rst
No memory is freed as a result of this operation; please use
:c:func:`tsk_edge_table_free` to free the table's internal resources. Note that the
metadata schema is not cleared.
@endrst

@param self A pointer to a tsk_edge_table_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_edge_table_clear(tsk_edge_table_t *self);

/**
@brief Truncates this table so that only the first num_rows are retained.

@param self A pointer to a tsk_edge_table_t object.
@param num_rows The number of rows to retain in the table.
@return Return 0 on success or a negative value on failure.
*/
int tsk_edge_table_truncate(tsk_edge_table_t *self, tsk_size_t num_rows);

/**
@brief Extends this table by appending rows copied from another table.

@rst
Appends the rows at the specified indexes from the table ``other`` to the end of this
table. Row indexes can be repeated and in any order. If ``row_indexes`` is ``NULL``,
append the first ``num_rows`` from ``other`` to this table. Note that metadata is copied
as-is and is not checked for compatibility with any existing schema on this table.
@endrst

@param self A pointer to a tsk_edge_table_t object where rows are to be added.
@param other A pointer to a tsk_edge_table_t object where rows are copied from.
@param num_rows The number of rows from ``other`` to append to this table.
@param row_indexes Array of row indexes in ``other``. If ``NULL`` is passed then the
    first ``num_rows`` of ``other`` are used.
@param options Bitwise option flags. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_edge_table_extend(tsk_edge_table_t *self, const tsk_edge_table_t *other,
    tsk_size_t num_rows, const tsk_id_t *row_indexes, tsk_flags_t options);

/**
@brief Subset this table by keeping rows according to a boolean mask.

@rst
Deletes rows from this table and optionally return the mapping from IDs in
the current table to the updated table. Rows are kept or deleted according to
the specified boolean array ``keep`` such that for each row ``j`` if
``keep[j]`` is false (zero) the row is deleted, and otherwise the row is
retained. Thus, ``keep`` must be an array of at least ``num_rows``
:c:type:`bool` values.

If the ``id_map`` argument is non-null, this array will be updated to represent
the mapping between IDs before and after row deletion. For row ``j``,
``id_map[j]`` will contain the new ID for row ``j`` if it is retained, or
:c:macro:`TSK_NULL` if the row has been removed. Thus, ``id_map`` must be an
array of at least ``num_rows`` :c:type:`tsk_id_t` values.

.. warning::
    C++ users need to be careful to specify the correct type when
    passing in values for the ``keep`` array,
    using ``std::vector<tsk_bool_t>`` and not ``std::vector<bool>``,
    as the latter may not be correct size.

@endrst

@param self A pointer to a tsk_edge_table_t object.
@param keep Array of boolean flags describing whether a particular
    row should be kept or not. Must be at least ``num_rows`` long.
@param options Bitwise option flags. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@param id_map An array in which to store the mapping between new
    and old IDs. If NULL, this will be ignored.
@return Return 0 on success or a negative value on failure.
*/
int tsk_edge_table_keep_rows(tsk_edge_table_t *self, const tsk_bool_t *keep,
    tsk_flags_t options, tsk_id_t *id_map);

/**
@brief Returns true if the data in the specified table is identical to the data
       in this table.

@rst

**Options**

Options to control the comparison can be specified by providing one or
more of the following bitwise flags. By default (options=0) tables are
considered equal if they are byte-wise identical in all columns,
and their metadata schemas are byte-wise identical.

- :c:macro:`TSK_CMP_IGNORE_METADATA`
@endrst

@param self A pointer to a tsk_edge_table_t object.
@param other A pointer to a tsk_edge_table_t object.
@param options Bitwise comparison options.
@return Return true if the specified table is equal to this table.
*/
bool tsk_edge_table_equals(
    const tsk_edge_table_t *self, const tsk_edge_table_t *other, tsk_flags_t options);

/**
@brief Copies the state of this table into the specified destination.

@rst
By default the method initialises the specified destination table. If the
destination is already initialised, the :c:macro:`TSK_NO_INIT` option should
be supplied to avoid leaking memory.
@endrst

@param self A pointer to a tsk_edge_table_t object.
@param dest A pointer to a tsk_edge_table_t object. If the TSK_NO_INIT option
    is specified, this must be an initialised edge table. If not, it must
    be an uninitialised edge table.
@param options Bitwise option flags.
@return Return 0 on success or a negative value on failure.
*/
int tsk_edge_table_copy(
    const tsk_edge_table_t *self, tsk_edge_table_t *dest, tsk_flags_t options);

/**
@brief Get the row at the specified index.

@rst
Updates the specified edge struct to reflect the values in the specified row.
Pointers to memory within this struct are handled by the table and should **not**
be freed by client code. These pointers are guaranteed to be valid until the
next operation that modifies the table (e.g., by adding a new row), but not afterwards.
@endrst

@param self A pointer to a tsk_edge_table_t object.
@param index The requested table row.
@param row A pointer to a tsk_edge_t struct that is updated to reflect the
    values in the specified row.
@return Return 0 on success or a negative value on failure.
*/
int tsk_edge_table_get_row(
    const tsk_edge_table_t *self, tsk_id_t index, tsk_edge_t *row);

/**
@brief Set the metadata schema
@rst
Copies the metadata schema string to this table, replacing any existing.
@endrst
@param self A pointer to a tsk_edge_table_t object.
@param metadata_schema A pointer to a char array
@param metadata_schema_length The size of the metadata schema in bytes.
@return Return 0 on success or a negative value on failure.
*/
int tsk_edge_table_set_metadata_schema(tsk_edge_table_t *self,
    const char *metadata_schema, tsk_size_t metadata_schema_length);

/**
@brief Print out the state of this table to the specified stream.

This method is intended for debugging purposes and should not be used
in production code. The format of the output should **not** be depended
on and may change arbitrarily between versions.

@param self A pointer to a tsk_edge_table_t object.
@param out The stream to write the summary to.
*/
void tsk_edge_table_print_state(const tsk_edge_table_t *self, FILE *out);

/**
@brief Replace this table's data by copying from a set of column arrays

@rst
Clears the data columns of this table and then copies column data from the specified
set of arrays. The supplied arrays should all contain data on the same number of rows.
The metadata schema is not affected.
@endrst

@param self A pointer to a tsk_edge_table_t object.
@param num_rows The number of rows to copy from the specifed arrays.
@param left The array of double left values to be copied.
@param right The array of double right values to be copied.
@param parent The array of tsk_id_t parent values to be copied.
@param child The array of tsk_id_t child values to be copied.
@param metadata The array of char metadata values to be copied.
@param metadata_offset The array of tsk_size_t metadata offset values to be copied.
@return Return 0 on success or a negative value on failure.
*/
int tsk_edge_table_set_columns(tsk_edge_table_t *self, tsk_size_t num_rows,
    const double *left, const double *right, const tsk_id_t *parent,
    const tsk_id_t *child, const char *metadata, const tsk_size_t *metadata_offset);

/**
@brief Extends this table by copying from a set of column arrays

@rst
Copies column data from the specified set of arrays to create new rows at the end of the
table. The supplied arrays should all contain data on the same number of rows. The
metadata schema is not affected.
@endrst

@param self A pointer to a tsk_edge_table_t object.
@param num_rows The number of rows to copy from the specifed arrays.
@param left The array of double left values to be copied.
@param right The array of double right values to be copied.
@param parent The array of tsk_id_t parent values to be copied.
@param child The array of tsk_id_t child values to be copied.
@param metadata The array of char metadata values to be copied.
@param metadata_offset The array of tsk_size_t metadata offset values to be copied.
*/
int tsk_edge_table_append_columns(tsk_edge_table_t *self, tsk_size_t num_rows,
    const double *left, const double *right, const tsk_id_t *parent,
    const tsk_id_t *child, const char *metadata, const tsk_size_t *metadata_offset);

/**
@brief Controls the pre-allocation strategy for this table

@rst
Set a fixed pre-allocation size, or use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_edge_table_t object.
@param max_rows_increment The number of rows to pre-allocate, or zero for the default
    doubling strategy.
@return Return 0 on success or a negative value on failure.
*/
int tsk_edge_table_set_max_rows_increment(
    tsk_edge_table_t *self, tsk_size_t max_rows_increment);

/**
@brief Controls the pre-allocation strategy for the metadata column

@rst
Set a fixed pre-allocation size, or use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_edge_table_t object.
@param max_metadata_length_increment The number of bytes to pre-allocate, or zero for
the default doubling strategy.
@return Return 0 on success or a negative value on failure.
*/
int tsk_edge_table_set_max_metadata_length_increment(
    tsk_edge_table_t *self, tsk_size_t max_metadata_length_increment);

/**
@brief Squash adjacent edges in-place

@rst
Sorts, then condenses the table into the smallest possible number of rows by
combining any adjacent edges. A pair of edges is said to be `adjacent` if
they have the same parent and child nodes, and if the left coordinate of
one of the edges is equal to the right coordinate of the other edge.
This process is performed in-place so that any set of adjacent edges is
replaced by a single edge. The new edge will have the same parent and child
node, a left coordinate equal to the smallest left coordinate in the set,
and a right coordinate equal to the largest right coordinate in the set.
The new edge table will be sorted in the canonical order (P, C, L, R).

.. note::
    Note that this method will fail if any edges have non-empty metadata.

@endrst

@param self A pointer to a tsk_edge_table_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_edge_table_squash(tsk_edge_table_t *self);

/** @} */

/* Undocumented methods */

int tsk_edge_table_dump_text(const tsk_edge_table_t *self, FILE *out);

/**
@defgroup MIGRATION_TABLE_API_GROUP Migration table API.
@{
*/

/**
@brief Initialises the table by allocating the internal memory.

@rst
This must be called before any operations are performed on the table.
See the :ref:`sec_c_api_overview_structure` for details on how objects
are initialised and freed.
@endrst

@param self A pointer to an uninitialised tsk_migration_table_t object.
@param options Allocation time options. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_migration_table_init(tsk_migration_table_t *self, tsk_flags_t options);

/**
@brief Free the internal memory for the specified table.

@param self A pointer to an initialised tsk_migration_table_t object.
@return Always returns 0.
*/
int tsk_migration_table_free(tsk_migration_table_t *self);

/**
@brief Adds a row to this migration table.

@rst
Add a new migration with the specified ``left``, ``right``, ``node``,
``source``, ``dest``, ``time`` and ``metadata`` to the table.
See the :ref:`table definition <sec_migration_table_definition>`
for details of the columns in this table.
@endrst

@param self A pointer to a tsk_migration_table_t object.
@param left The left coordinate for the new migration.
@param right The right coordinate for the new migration.
@param node The node ID for the new migration.
@param source The source population ID for the new migration.
@param dest The destination population ID for the new migration.
@param time The time for the new migration.
@param metadata The metadata to be associated with the new migration. This
    is a pointer to arbitrary memory. Can be ``NULL`` if ``metadata_length`` is 0.
@param metadata_length The size of the metadata array in bytes.

@return Return the ID of the newly added migration on success,
    or a negative value on failure.
*/
tsk_id_t tsk_migration_table_add_row(tsk_migration_table_t *self, double left,
    double right, tsk_id_t node, tsk_id_t source, tsk_id_t dest, double time,
    const char *metadata, tsk_size_t metadata_length);

/**
@brief Updates the row at the specified index.

@rst
Rewrite the row at the specified index in this table to use the specified
values. A copy of the ``metadata`` parameter is taken immediately. See the
:ref:`table definition <sec_migration_table_definition>` for details of the columns
in this table.

.. warning::
    Because of the way that ragged columns are encoded, this method requires a
    full rewrite of the internal column memory in worst case, and would
    therefore be inefficient for bulk updates for such columns. However, if the
    sizes of all ragged column values are unchanged in the updated row, this
    method is guaranteed to only update the memory for the row in question.
@endrst

@param self A pointer to a tsk_migration_table_t object.
@param index The row to update.
@param left The left coordinate for the migration.
@param right The right coordinate for the migration.
@param node The node ID for the migration.
@param source The source population ID for the migration.
@param dest The destination population ID for the migration.
@param time The time for the migration.
@param metadata The metadata to be associated with the migration. This
    is a pointer to arbitrary memory. Can be ``NULL`` if ``metadata_length`` is 0.
@param metadata_length The size of the metadata array in bytes.
@return Return 0 on success or a negative value on failure.
*/
int tsk_migration_table_update_row(tsk_migration_table_t *self, tsk_id_t index,
    double left, double right, tsk_id_t node, tsk_id_t source, tsk_id_t dest,
    double time, const char *metadata, tsk_size_t metadata_length);

/**
@brief Clears this table, setting the number of rows to zero.

@rst
No memory is freed as a result of this operation; please use
:c:func:`tsk_migration_table_free` to free the table's internal resources. Note that the
metadata schema is not cleared.
@endrst

@param self A pointer to a tsk_migration_table_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_migration_table_clear(tsk_migration_table_t *self);

/**
@brief Truncates this table so that only the first num_rows are retained.

@param self A pointer to a tsk_migration_table_t object.
@param num_rows The number of rows to retain in the table.
@return Return 0 on success or a negative value on failure.
*/
int tsk_migration_table_truncate(tsk_migration_table_t *self, tsk_size_t num_rows);

/**
@brief Extends this table by appending rows copied from another table.

@rst
Appends the rows at the specified indexes from the table ``other`` to the end of this
table. Row indexes can be repeated and in any order. If ``row_indexes`` is NULL, append
the first ``num_rows`` from ``other`` to this table. Note that metadata is copied as-is
and is not checked for compatibility with any existing schema on this table.
@endrst

@param self A pointer to a tsk_migration_table_t object where rows are to be added.
@param other A pointer to a tsk_migration_table_t object where rows are copied from.
@param num_rows The number of rows from ``other`` to append to this table.
@param row_indexes Array of row indexes in ``other``. If ``NULL`` is passed then the
    first ``num_rows`` of ``other`` are used.
@param options Bitwise option flags. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/

int tsk_migration_table_extend(tsk_migration_table_t *self,
    const tsk_migration_table_t *other, tsk_size_t num_rows, const tsk_id_t *row_indexes,
    tsk_flags_t options);

/**
@brief Subset this table by keeping rows according to a boolean mask.

@rst
Deletes rows from this table and optionally return the mapping from IDs in
the current table to the updated table. Rows are kept or deleted according to
the specified boolean array ``keep`` such that for each row ``j`` if
``keep[j]`` is false (zero) the row is deleted, and otherwise the row is
retained. Thus, ``keep`` must be an array of at least ``num_rows``
:c:type:`bool` values.

If the ``id_map`` argument is non-null, this array will be updated to represent
the mapping between IDs before and after row deletion. For row ``j``,
``id_map[j]`` will contain the new ID for row ``j`` if it is retained, or
:c:macro:`TSK_NULL` if the row has been removed. Thus, ``id_map`` must be an
array of at least ``num_rows`` :c:type:`tsk_id_t` values.

.. warning::
    C++ users need to be careful to specify the correct type when
    passing in values for the ``keep`` array,
    using ``std::vector<tsk_bool_t>`` and not ``std::vector<bool>``,
    as the latter may not be correct size.

@endrst

@param self A pointer to a tsk_migration_table_t object.
@param keep Array of boolean flags describing whether a particular
    row should be kept or not. Must be at least ``num_rows`` long.
@param options Bitwise option flags. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@param id_map An array in which to store the mapping between new
    and old IDs. If NULL, this will be ignored.
@return Return 0 on success or a negative value on failure.
*/
int tsk_migration_table_keep_rows(tsk_migration_table_t *self, const tsk_bool_t *keep,
    tsk_flags_t options, tsk_id_t *id_map);

/**
@brief Returns true if the data in the specified table is identical to the data
       in this table.

@rst

**Options**

Options to control the comparison can be specified by providing one or
more of the following bitwise flags. By default (options=0) tables are
considered equal if they are byte-wise identical in all columns,
and their metadata schemas are byte-wise identical.

- :c:macro:`TSK_CMP_IGNORE_METADATA`
@endrst

@param self A pointer to a tsk_migration_table_t object.
@param other A pointer to a tsk_migration_table_t object.
@param options Bitwise comparison options.
@return Return true if the specified table is equal to this table.
*/
bool tsk_migration_table_equals(const tsk_migration_table_t *self,
    const tsk_migration_table_t *other, tsk_flags_t options);

/**
@brief Copies the state of this table into the specified destination.

@rst
By default the method initialises the specified destination table. If the
destination is already initialised, the :c:macro:`TSK_NO_INIT` option should
be supplied to avoid leaking memory.
@endrst

@param self A pointer to a tsk_migration_table_t object.
@param dest A pointer to a tsk_migration_table_t object. If the TSK_NO_INIT
option is specified, this must be an initialised migration table. If not, it must be an
uninitialised migration table.
@param options Bitwise option flags.
@return Return 0 on success or a negative value on failure.
*/
int tsk_migration_table_copy(
    const tsk_migration_table_t *self, tsk_migration_table_t *dest, tsk_flags_t options);

/**
@brief Get the row at the specified index.

@rst
Updates the specified migration struct to reflect the values in the specified row.
Pointers to memory within this struct are handled by the table and should **not**
be freed by client code. These pointers are guaranteed to be valid until the
next operation that modifies the table (e.g., by adding a new row), but not afterwards.
@endrst

@param self A pointer to a tsk_migration_table_t object.
@param index The requested table row.
@param row A pointer to a tsk_migration_t struct that is updated to reflect the
    values in the specified row.
@return Return 0 on success or a negative value on failure.
*/
int tsk_migration_table_get_row(
    const tsk_migration_table_t *self, tsk_id_t index, tsk_migration_t *row);

/**
@brief Set the metadata schema
@rst
Copies the metadata schema string to this table, replacing any existing.
@endrst
@param self A pointer to a tsk_migration_table_t object.
@param metadata_schema A pointer to a char array.
@param metadata_schema_length The size of the metadata schema in bytes.
@return Return 0 on success or a negative value on failure.
*/
int tsk_migration_table_set_metadata_schema(tsk_migration_table_t *self,
    const char *metadata_schema, tsk_size_t metadata_schema_length);

/**
@brief Print out the state of this table to the specified stream.

This method is intended for debugging purposes and should not be used
in production code. The format of the output should **not** be depended
on and may change arbitrarily between versions.

@param self A pointer to a tsk_migration_table_t object.
@param out The stream to write the summary to.
*/
void tsk_migration_table_print_state(const tsk_migration_table_t *self, FILE *out);

/**
@brief Replace this table's data by copying from a set of column arrays

@rst
Clears the data columns of this table and then copies column data from the specified
set of arrays. The supplied arrays should all contain data on the same number of rows.
The metadata schema is not affected.
@endrst

@param self A pointer to a tsk_migration_table_t object.
@param num_rows The number of rows to copy from the specifed arrays.
@param left The array of double left values to be copied.
@param right The array of double right values to be copied.
@param node The array of tsk_id_t node values to be copied.
@param source The array of tsk_id_t source values to be copied.
@param dest The array of tsk_id_t dest values to be copied.
@param time The array of double time values to be copied.
@param metadata The array of char metadata values to be copied.
@param metadata_offset The array of tsk_size_t metadata offset values to be copied.
@return Return 0 on success or a negative value on failure.
*/
int tsk_migration_table_set_columns(tsk_migration_table_t *self, tsk_size_t num_rows,
    const double *left, const double *right, const tsk_id_t *node,
    const tsk_id_t *source, const tsk_id_t *dest, const double *time,
    const char *metadata, const tsk_size_t *metadata_offset);

/**
@brief Extends this table by copying from a set of column arrays

@rst
Copies column data from the specified set of arrays to create new rows at the end of the
table. The supplied arrays should all contain data on the same number of rows. The
metadata schema is not affected.
@endrst

@param self A pointer to a tsk_migration_table_t object.
@param num_rows The number of rows to copy from the specifed arrays
@param left The array of double left values to be copied.
@param right The array of double right values to be copied.
@param node The array of tsk_id_t node values to be copied.
@param source The array of tsk_id_t source values to be copied.
@param dest The array of tsk_id_t dest values to be copied.
@param time The array of double time values to be copied.
@param metadata The array of char metadata values to be copied.
@param metadata_offset The array of tsk_size_t metadata offset values to be copied.
@return Return 0 on success or a negative value on failure.
*/
int tsk_migration_table_append_columns(tsk_migration_table_t *self, tsk_size_t num_rows,
    const double *left, const double *right, const tsk_id_t *node,
    const tsk_id_t *source, const tsk_id_t *dest, const double *time,
    const char *metadata, const tsk_size_t *metadata_offset);

/**
@brief Controls the pre-allocation strategy for this table

@rst
Set a fixed pre-allocation size, or use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_migration_table_t object.
@param max_rows_increment The number of rows to pre-allocate, or zero for the default
    doubling strategy.
@return Return 0 on success or a negative value on failure.
*/
int tsk_migration_table_set_max_rows_increment(
    tsk_migration_table_t *self, tsk_size_t max_rows_increment);

/**
@brief Controls the pre-allocation strategy for the metadata column

@rst
Set a fixed pre-allocation size, or use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_migration_table_t object.
@param max_metadata_length_increment The number of bytes to pre-allocate, or zero for
the default doubling strategy.
@return Return 0 on success or a negative value on failure.
*/
int tsk_migration_table_set_max_metadata_length_increment(
    tsk_migration_table_t *self, tsk_size_t max_metadata_length_increment);

/** @} */

/* Undocumented methods */

int tsk_migration_table_dump_text(const tsk_migration_table_t *self, FILE *out);

/**
@defgroup SITE_TABLE_API_GROUP Site table API.
@{
*/

/**
@brief Initialises the table by allocating the internal memory.

@rst
This must be called before any operations are performed on the table.
See the :ref:`sec_c_api_overview_structure` for details on how objects
are initialised and freed.
@endrst

@param self A pointer to an uninitialised tsk_site_table_t object.
@param options Allocation time options. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_site_table_init(tsk_site_table_t *self, tsk_flags_t options);

/**
@brief Free the internal memory for the specified table.

@param self A pointer to an initialised tsk_site_table_t object.
@return Always returns 0.
*/
int tsk_site_table_free(tsk_site_table_t *self);

/**
@brief Adds a row to this site table.

@rst
Add a new site with the specified ``position``, ``ancestral_state``
and ``metadata`` to the table. Copies of ``ancestral_state`` and ``metadata``
are immediately taken. See the :ref:`table definition <sec_site_table_definition>`
for details of the columns in this table.
@endrst

@param self A pointer to a tsk_site_table_t object.
@param position The position coordinate for the new site.
@param ancestral_state The ancestral_state for the new site.
@param ancestral_state_length The length of the ancestral_state in bytes.
@param metadata The metadata to be associated with the new site. This
    is a pointer to arbitrary memory. Can be ``NULL`` if ``metadata_length`` is 0.
@param metadata_length The size of the metadata array in bytes.
@return Return the ID of the newly added site on success,
    or a negative value on failure.
*/
tsk_id_t tsk_site_table_add_row(tsk_site_table_t *self, double position,
    const char *ancestral_state, tsk_size_t ancestral_state_length, const char *metadata,
    tsk_size_t metadata_length);

/**
@brief Updates the row at the specified index.

@rst
Rewrite the row at the specified index in this table to use the specified
values. Copies of the ``ancestral_state`` and ``metadata`` parameters are taken
immediately. See the :ref:`table definition <sec_site_table_definition>` for
details of the columns in this table.

.. warning::
    Because of the way that ragged columns are encoded, this method requires a
    full rewrite of the internal column memory in worst case, and would
    therefore be inefficient for bulk updates for such columns. However, if the
    sizes of all ragged column values are unchanged in the updated row, this
    method is guaranteed to only update the memory for the row in question.
@endrst

@param self A pointer to a tsk_site_table_t object.
@param index The row to update.
@param position The position coordinate for the site.
@param ancestral_state The ancestral_state for the site.
@param ancestral_state_length The length of the ancestral_state in bytes.
@param metadata The metadata to be associated with the site. This
    is a pointer to arbitrary memory. Can be ``NULL`` if ``metadata_length`` is 0.
@param metadata_length The size of the metadata array in bytes.
@return Return 0 on success or a negative value on failure.
*/
int tsk_site_table_update_row(tsk_site_table_t *self, tsk_id_t index, double position,
    const char *ancestral_state, tsk_size_t ancestral_state_length, const char *metadata,
    tsk_size_t metadata_length);

/**
@brief Clears this table, setting the number of rows to zero.

@rst
No memory is freed as a result of this operation; please use
:c:func:`tsk_site_table_free` to free the table's internal resources. Note that the
metadata schema is not cleared.
@endrst

@param self A pointer to a tsk_site_table_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_site_table_clear(tsk_site_table_t *self);

/**
@brief Truncates this table so that only the first num_rows are retained.

@param self A pointer to a tsk_site_table_t object.
@param num_rows The number of rows to retain in the table.
@return Return 0 on success or a negative value on failure.
*/
int tsk_site_table_truncate(tsk_site_table_t *self, tsk_size_t num_rows);

/**
@brief Extends this table by appending rows copied from another table.

@rst
Appends the rows at the specified indexes from the table ``other`` to the end of this
table. Row indexes can be repeated and in any order. If ``row_indexes`` is NULL, append
the first ``num_rows`` from ``other`` to this table. Note that metadata is copied as-is
and is not checked for compatibility with any existing schema on this table.
@endrst

@param self A pointer to a tsk_site_table_t object where rows are to be added.
@param other A pointer to a tsk_site_table_t object where rows are copied from.
@param num_rows The number of rows from ``other`` to append to this table.
@param row_indexes Array of row indexes in ``other``. If ``NULL`` is passed then the
    first ``num_rows`` of ``other`` are used.
@param options Bitwise option flags. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_site_table_extend(tsk_site_table_t *self, const tsk_site_table_t *other,
    tsk_size_t num_rows, const tsk_id_t *row_indexes, tsk_flags_t options);

/**
@brief Subset this table by keeping rows according to a boolean mask.

@rst
Deletes rows from this table and optionally return the mapping from IDs in
the current table to the updated table. Rows are kept or deleted according to
the specified boolean array ``keep`` such that for each row ``j`` if
``keep[j]`` is false (zero) the row is deleted, and otherwise the row is
retained. Thus, ``keep`` must be an array of at least ``num_rows``
:c:type:`bool` values.

If the ``id_map`` argument is non-null, this array will be updated to represent
the mapping between IDs before and after row deletion. For row ``j``,
``id_map[j]`` will contain the new ID for row ``j`` if it is retained, or
:c:macro:`TSK_NULL` if the row has been removed. Thus, ``id_map`` must be an
array of at least ``num_rows`` :c:type:`tsk_id_t` values.

.. warning::
    C++ users need to be careful to specify the correct type when
    passing in values for the ``keep`` array,
    using ``std::vector<tsk_bool_t>`` and not ``std::vector<bool>``,
    as the latter may not be correct size.

@endrst

@param self A pointer to a tsk_site_table_t object.
@param keep Array of boolean flags describing whether a particular
    row should be kept or not. Must be at least ``num_rows`` long.
@param options Bitwise option flags. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@param id_map An array in which to store the mapping between new
    and old IDs. If NULL, this will be ignored.
@return Return 0 on success or a negative value on failure.
*/
int tsk_site_table_keep_rows(tsk_site_table_t *self, const tsk_bool_t *keep,
    tsk_flags_t options, tsk_id_t *id_map);

/**
@brief Returns true if the data in the specified table is identical to the data
       in this table.

@rst

**Options**

Options to control the comparison can be specified by providing one or
more of the following bitwise flags. By default (options=0) tables are
considered equal if they are byte-wise identical in all columns,
and their metadata schemas are byte-wise identical.

- :c:macro:`TSK_CMP_IGNORE_METADATA`
@endrst

@param self A pointer to a tsk_site_table_t object.
@param other A pointer to a tsk_site_table_t object.
@param options Bitwise comparison options.
@return Return true if the specified table is equal to this table.
*/
bool tsk_site_table_equals(
    const tsk_site_table_t *self, const tsk_site_table_t *other, tsk_flags_t options);

/**
@brief Copies the state of this table into the specified destination.

@rst
By default the method initialises the specified destination table. If the
destination is already initialised, the :c:macro:`TSK_NO_INIT` option should
be supplied to avoid leaking memory.
@endrst

@param self A pointer to a tsk_site_table_t object.
@param dest A pointer to a tsk_site_table_t object. If the TSK_NO_INIT option
    is specified, this must be an initialised site table. If not, it must
    be an uninitialised site table.
@param options Bitwise option flags.
@return Return 0 on success or a negative value on failure.
*/
int tsk_site_table_copy(
    const tsk_site_table_t *self, tsk_site_table_t *dest, tsk_flags_t options);

/**
@brief Get the row at the specified index.

@rst
Updates the specified site struct to reflect the values in the specified row.

This function always sets the ``mutations`` and ``mutations_length``
fields in the parameter :c:struct:`tsk_site_t` to ``NULL`` and ``0`` respectively.
To get access to the mutations for a particular site, please use the
tree sequence method, :c:func:`tsk_treeseq_get_site`.

Pointers to memory within this struct are handled by the table and should **not**
be freed by client code. These pointers are guaranteed to be valid until the
next operation that modifies the table (e.g., by adding a new row), but not afterwards.
@endrst

@param self A pointer to a tsk_site_table_t object.
@param index The requested table row.
@param row A pointer to a tsk_site_t struct that is updated to reflect the
    values in the specified row.
@return Return 0 on success or a negative value on failure.
*/
int tsk_site_table_get_row(
    const tsk_site_table_t *self, tsk_id_t index, tsk_site_t *row);

/**
@brief Set the metadata schema
@rst
Copies the metadata schema string to this table, replacing any existing.
@endrst
@param self A pointer to a tsk_site_table_t object.
@param metadata_schema A pointer to a char array.
@param metadata_schema_length The size of the metadata schema in bytes.
@return Return 0 on success or a negative value on failure.
*/
int tsk_site_table_set_metadata_schema(tsk_site_table_t *self,
    const char *metadata_schema, tsk_size_t metadata_schema_length);

/**
@brief Print out the state of this table to the specified stream.

This method is intended for debugging purposes and should not be used
in production code. The format of the output should **not** be depended
on and may change arbitrarily between versions.

@param self A pointer to a tsk_site_table_t object.
@param out The stream to write the summary to.
*/
void tsk_site_table_print_state(const tsk_site_table_t *self, FILE *out);

/**
@brief Replace this table's data by copying from a set of column arrays

@rst
Clears the data columns of this table and then copies column data from the specified
set of arrays. The supplied arrays should all contain data on the same number of rows.
The metadata schema is not affected.
@endrst

@param self A pointer to a tsk_site_table_t object.
@param num_rows The number of rows to copy from the specifed arrays.
@param position The array of double position values to be copied.
@param ancestral_state The array of char ancestral state values to be copied.
@param ancestral_state_offset The array of tsk_size_t ancestral state offset values to be
        copied.
@param metadata The array of char metadata values to be copied.
@param metadata_offset The array of tsk_size_t metadata offset values to be copied.
@return Return 0 on success or a negative value on failure.
*/
int tsk_site_table_set_columns(tsk_site_table_t *self, tsk_size_t num_rows,
    const double *position, const char *ancestral_state,
    const tsk_size_t *ancestral_state_offset, const char *metadata,
    const tsk_size_t *metadata_offset);

/**
@brief Extends this table by copying from a set of column arrays

@rst
Copies column data from the specified set of arrays to create new rows at the end of the
table. The supplied arrays should all contain data on the same number of rows. The
metadata schema is not affected.
@endrst

@param self A pointer to a tsk_site_table_t object.
@param num_rows The number of rows to copy from the specifed arrays.
@param position The array of double position values to be copied.
@param ancestral_state The array of char ancestral state values to be copied.
@param ancestral_state_offset The array of tsk_size_t ancestral state offset values to be
    copied.
@param metadata The array of char metadata values to be copied.
@param metadata_offset The array of tsk_size_t metadata offset values to be copied.
@return Return 0 on success or a negative value on failure.
*/
int tsk_site_table_append_columns(tsk_site_table_t *self, tsk_size_t num_rows,
    const double *position, const char *ancestral_state,
    const tsk_size_t *ancestral_state_offset, const char *metadata,
    const tsk_size_t *metadata_offset);

/**
@brief Controls the pre-allocation strategy for this table

@rst
Set a fixed pre-allocation size, or use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_site_table_t object.
@param max_rows_increment The number of rows to pre-allocate, or zero for the default
    doubling strategy.
@return Return 0 on success or a negative value on failure.
*/
int tsk_site_table_set_max_rows_increment(
    tsk_site_table_t *self, tsk_size_t max_rows_increment);

/**
@brief Controls the pre-allocation strategy for the metadata column

@rst
Set a fixed pre-allocation size, or use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_site_table_t object.
@param max_metadata_length_increment The number of bytes to pre-allocate, or zero for
the default doubling strategy.
@return Return 0 on success or a negative value on failure.
*/

int tsk_site_table_set_max_metadata_length_increment(
    tsk_site_table_t *self, tsk_size_t max_metadata_length_increment);

/**
@brief Controls the pre-allocation strategy for the ancestral_state column

@rst
Set a fixed pre-allocation size, or use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_site_table_t object.
@param max_ancestral_state_length_increment The number of bytes to pre-allocate, or zero
for the default doubling strategy.
@return Return 0 on success or a negative value on failure.
*/
int tsk_site_table_set_max_ancestral_state_length_increment(
    tsk_site_table_t *self, tsk_size_t max_ancestral_state_length_increment);

/** @} */

/* Undocumented methods */

int tsk_site_table_dump_text(const tsk_site_table_t *self, FILE *out);

/**
@defgroup MUTATION_TABLE_API_GROUP Mutation table API.
@{
*/

/**
@brief Initialises the table by allocating the internal memory.

@rst
This must be called before any operations are performed on the table.
See the :ref:`sec_c_api_overview_structure` for details on how objects
are initialised and freed.
@endrst

@param self A pointer to an uninitialised tsk_mutation_table_t object.
@param options Allocation time options. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_mutation_table_init(tsk_mutation_table_t *self, tsk_flags_t options);

/**
@brief Free the internal memory for the specified table.

@param self A pointer to an initialised tsk_mutation_table_t object.
@return Always returns 0.
*/
int tsk_mutation_table_free(tsk_mutation_table_t *self);

/**
@brief Adds a row to this mutation table.

@rst
Add a new mutation with the specified ``site``, ``parent``, ``derived_state``
and ``metadata`` to the table. Copies of ``derived_state`` and ``metadata``
are immediately taken. See the :ref:`table definition <sec_mutation_table_definition>`
for details of the columns in this table.
@endrst

@param self A pointer to a tsk_mutation_table_t object.
@param site The site ID for the new mutation.
@param node The ID of the node this mutation occurs over.
@param parent The ID of the parent mutation.
@param time The time of the mutation.
@param derived_state The derived_state for the new mutation.
@param derived_state_length The length of the derived_state in bytes.
@param metadata The metadata to be associated with the new mutation. This
    is a pointer to arbitrary memory. Can be ``NULL`` if ``metadata_length`` is 0.
@param metadata_length The size of the metadata array in bytes.
@return Return the ID of the newly added mutation on success,
    or a negative value on failure.
*/
tsk_id_t tsk_mutation_table_add_row(tsk_mutation_table_t *self, tsk_id_t site,
    tsk_id_t node, tsk_id_t parent, double time, const char *derived_state,
    tsk_size_t derived_state_length, const char *metadata, tsk_size_t metadata_length);

/**
@brief Updates the row at the specified index.

@rst
Rewrite the row at the specified index in this table to use the specified
values. Copies of the ``derived_state`` and ``metadata`` parameters are taken
immediately. See the :ref:`table definition <sec_mutation_table_definition>` for
details of the columns in this table.

.. warning::
    Because of the way that ragged columns are encoded, this method requires a
    full rewrite of the internal column memory in worst case, and would
    therefore be inefficient for bulk updates for such columns. However, if the
    sizes of all ragged column values are unchanged in the updated row, this
    method is guaranteed to only update the memory for the row in question.
@endrst

@param self A pointer to a tsk_mutation_table_t object.
@param index The row to update.
@param site The site ID for the mutation.
@param node The ID of the node this mutation occurs over.
@param parent The ID of the parent mutation.
@param time The time of the mutation.
@param derived_state The derived_state for the mutation.
@param derived_state_length The length of the derived_state in bytes.
@param metadata The metadata to be associated with the mutation. This
    is a pointer to arbitrary memory. Can be ``NULL`` if ``metadata_length`` is 0.
@param metadata_length The size of the metadata array in bytes.
@return Return 0 on success or a negative value on failure.
*/
int tsk_mutation_table_update_row(tsk_mutation_table_t *self, tsk_id_t index,
    tsk_id_t site, tsk_id_t node, tsk_id_t parent, double time,
    const char *derived_state, tsk_size_t derived_state_length, const char *metadata,
    tsk_size_t metadata_length);

/**
@brief Clears this table, setting the number of rows to zero.

@rst
No memory is freed as a result of this operation; please use
:c:func:`tsk_mutation_table_free` to free the table's internal resources. Note that the
metadata schema is not cleared.
@endrst

@param self A pointer to a tsk_mutation_table_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_mutation_table_clear(tsk_mutation_table_t *self);

/**
@brief Truncates this table so that only the first num_rows are retained.

@param self A pointer to a tsk_mutation_table_t object.
@param num_rows The number of rows to retain in the table.
@return Return 0 on success or a negative value on failure.
*/
int tsk_mutation_table_truncate(tsk_mutation_table_t *self, tsk_size_t num_rows);

/**
@brief Extends this table by appending rows copied from another table.

@rst
Appends the rows at the specified indexes from the table ``other`` to the end of this
table. Row indexes can be repeated and in any order. If ``row_indexes`` is NULL, append
the first ``num_rows`` from ``other`` to this table. Note that metadata is copied as-is
and is not checked for compatibility with any existing schema on this table.
@endrst

@param self A pointer to a tsk_mutation_table_t object where rows are to be added.
@param other A pointer to a tsk_mutation_table_t object where rows are copied from.
@param num_rows The number of rows from ``other`` to append to this table.
@param row_indexes Array of row indexes in ``other``. If ``NULL`` is passed then the
    first ``num_rows`` of ``other`` are used.
@param options Bitwise option flags. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_mutation_table_extend(tsk_mutation_table_t *self,
    const tsk_mutation_table_t *other, tsk_size_t num_rows, const tsk_id_t *row_indexes,
    tsk_flags_t options);

/**
@brief Subset this table by keeping rows according to a boolean mask.

@rst
Deletes rows from this table and optionally return the mapping from IDs in
the current table to the updated table. Rows are kept or deleted according to
the specified boolean array ``keep`` such that for each row ``j`` if
``keep[j]`` is false (zero) the row is deleted, and otherwise the row is
retained. Thus, ``keep`` must be an array of at least ``num_rows``
:c:type:`bool` values.

If the ``id_map`` argument is non-null, this array will be updated to represent
the mapping between IDs before and after row deletion. For row ``j``,
``id_map[j]`` will contain the new ID for row ``j`` if it is retained, or
:c:macro:`TSK_NULL` if the row has been removed. Thus, ``id_map`` must be an
array of at least ``num_rows`` :c:type:`tsk_id_t` values.

The values in the ``parent`` column are updated according to this map, so that
reference integrity within the table is maintained. As a consequence of this,
the values in the ``parent`` column for kept rows are bounds-checked and an
error raised if they are not valid. Rows that are deleted are not checked for
parent ID integrity.

If an attempt is made to delete rows that are referred to by the ``parent``
column of rows that are retained, an error is raised.

These error conditions are checked before any alterations to the table are
made.

.. warning::
    C++ users need to be careful to specify the correct type when
    passing in values for the ``keep`` array,
    using ``std::vector<tsk_bool_t>`` and not ``std::vector<bool>``,
    as the latter may not be correct size.

@endrst

@param self A pointer to a tsk_mutation_table_t object.
@param keep Array of boolean flags describing whether a particular
    row should be kept or not. Must be at least ``num_rows`` long.
@param options Bitwise option flags. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@param id_map An array in which to store the mapping between new
    and old IDs. If NULL, this will be ignored.
@return Return 0 on success or a negative value on failure.
*/
int tsk_mutation_table_keep_rows(tsk_mutation_table_t *self, const tsk_bool_t *keep,
    tsk_flags_t options, tsk_id_t *id_map);

/**
@brief Returns true if the data in the specified table is identical to the data
       in this table.

@rst

**Options**

Options to control the comparison can be specified by providing one or
more of the following bitwise flags. By default (options=0) tables are
considered equal if they are byte-wise identical in all columns,
and their metadata schemas are byte-wise identical.

- :c:macro:`TSK_CMP_IGNORE_METADATA`
@endrst

@param self A pointer to a tsk_mutation_table_t object.
@param other A pointer to a tsk_mutation_table_t object.
@param options Bitwise comparison options.
@return Return true if the specified table is equal to this table.
*/
bool tsk_mutation_table_equals(const tsk_mutation_table_t *self,
    const tsk_mutation_table_t *other, tsk_flags_t options);

/**
@brief Copies the state of this table into the specified destination.

@rst
By default the method initialises the specified destination table. If the
destination is already initialised, the :c:macro:`TSK_NO_INIT` option should
be supplied to avoid leaking memory.
@endrst

@param self A pointer to a tsk_mutation_table_t object.
@param dest A pointer to a tsk_mutation_table_t object. If the TSK_NO_INIT
option is specified, this must be an initialised mutation table. If not, it must be an
uninitialised mutation table.
@param options Bitwise option flags.
@return Return 0 on success or a negative value on failure.
*/
int tsk_mutation_table_copy(
    const tsk_mutation_table_t *self, tsk_mutation_table_t *dest, tsk_flags_t options);

/**
@brief Get the row at the specified index.

@rst
Updates the specified mutation struct to reflect the values in the specified row.

This function always sets the ``edge`` field in parameter
:c:struct:`tsk_mutation_t` to ``TSK_NULL``. To determine the ID of
the edge associated with a particular mutation, please use the
tree sequence method, :c:func:`tsk_treeseq_get_mutation`.

Pointers to memory within this struct are handled by the table and should **not**
be freed by client code. These pointers are guaranteed to be valid until the
next operation that modifies the table (e.g., by adding a new row), but not afterwards.
@endrst

@param self A pointer to a tsk_mutation_table_t object.
@param index The requested table row.
@param row A pointer to a tsk_mutation_t struct that is updated to reflect the
    values in the specified row.
@return Return 0 on success or a negative value on failure.
*/
int tsk_mutation_table_get_row(
    const tsk_mutation_table_t *self, tsk_id_t index, tsk_mutation_t *row);

/**
@brief Set the metadata schema
@rst
Copies the metadata schema string to this table, replacing any existing.
@endrst
@param self A pointer to a tsk_mutation_table_t object.
@param metadata_schema A pointer to a char array.
@param metadata_schema_length The size of the metadata schema in bytes.
@return Return 0 on success or a negative value on failure.
*/
int tsk_mutation_table_set_metadata_schema(tsk_mutation_table_t *self,
    const char *metadata_schema, tsk_size_t metadata_schema_length);

/**
@brief Print out the state of this table to the specified stream.

This method is intended for debugging purposes and should not be used
in production code. The format of the output should **not** be depended
on and may change arbitrarily between versions.

@param self A pointer to a tsk_mutation_table_t object.
@param out The stream to write the summary to.
*/
void tsk_mutation_table_print_state(const tsk_mutation_table_t *self, FILE *out);

/**
@brief Replace this table's data by copying from a set of column arrays

@rst
Clears the data columns of this table and then copies column data from the specified
set of arrays. The supplied arrays should all contain data on the same number of rows.
The metadata schema is not affected.
@endrst

@param self A pointer to a tsk_mutation_table_t object.
@param num_rows The number of rows to copy from the specifed arrays.
@param site The array of tsk_id_t site values to be copied.
@param node The array of tsk_id_t node values to be copied.
@param parent The array of tsk_id_t parent values to be copied.
@param time The array of double time values to be copied.
@param derived_state The array of char derived_state values to be copied.
@param derived_state_offset The array of tsk_size_t derived state offset values to be
copied.
@param metadata The array of char metadata values to be copied.
@param metadata_offset The array of tsk_size_t metadata offset values to be copied.
@return Return 0 on success or a negative value on failure.
*/
int tsk_mutation_table_set_columns(tsk_mutation_table_t *self, tsk_size_t num_rows,
    const tsk_id_t *site, const tsk_id_t *node, const tsk_id_t *parent,
    const double *time, const char *derived_state,
    const tsk_size_t *derived_state_offset, const char *metadata,
    const tsk_size_t *metadata_offset);

/**
@brief Extends this table by copying from a set of column arrays

@rst
Copies column data from the specified set of arrays to create new rows at the end of the
table. The supplied arrays should all contain data on the same number of rows. The
metadata schema is not affected.
@endrst

@param self A pointer to a tsk_mutation_table_t object.
@param num_rows The number of rows to copy from the specifed arrays.
@param site The array of tsk_id_t site values to be copied.
@param node The array of tsk_id_t node values to be copied.
@param parent The array of tsk_id_t parent values to be copied.
@param time The array of double time values to be copied.
@param derived_state The array of char derived_state values to be copied.
@param derived_state_offset The array of tsk_size_t derived state offset values to be
    copied.
@param metadata The array of char metadata values to be copied.
@param metadata_offset The array of tsk_size_t metadata offset values to be copied.
@return Return 0 on success or a negative value on failure.
*/
int tsk_mutation_table_append_columns(tsk_mutation_table_t *self, tsk_size_t num_rows,
    const tsk_id_t *site, const tsk_id_t *node, const tsk_id_t *parent,
    const double *time, const char *derived_state,
    const tsk_size_t *derived_state_offset, const char *metadata,
    const tsk_size_t *metadata_offset);

/**
@brief Controls the pre-allocation strategy for this table

@rst
Set a fixed pre-allocation size, or use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_mutation_table_t object.
@param max_rows_increment The number of rows to pre-allocate, or zero for the default
    doubling strategy.
@return Return 0 on success or a negative value on failure.
*/
int tsk_mutation_table_set_max_rows_increment(
    tsk_mutation_table_t *self, tsk_size_t max_rows_increment);

/**
@brief Controls the pre-allocation strategy for the metadata column

@rst
Set a fixed pre-allocation size, or use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_mutation_table_t object.
@param max_metadata_length_increment The number of bytes to pre-allocate, or zero for
the default doubling strategy.
@return Return 0 on success or a negative value on failure.
*/
int tsk_mutation_table_set_max_metadata_length_increment(
    tsk_mutation_table_t *self, tsk_size_t max_metadata_length_increment);

/**
@brief Controls the pre-allocation strategy for the derived_state column

@rst
Set a fixed pre-allocation size, or use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_mutation_table_t object.
@param max_derived_state_length_increment The number of bytes to pre-allocate, or zero
for the default doubling strategy.
@return Return 0 on success or a negative value on failure.
*/
int tsk_mutation_table_set_max_derived_state_length_increment(
    tsk_mutation_table_t *self, tsk_size_t max_derived_state_length_increment);

/** @} */

/* Undocumented methods */

int tsk_mutation_table_dump_text(const tsk_mutation_table_t *self, FILE *out);

/**
@defgroup POPULATION_TABLE_API_GROUP Population table API.
@{
*/

/**
@brief Initialises the table by allocating the internal memory.

@rst
This must be called before any operations are performed on the table.
See the :ref:`sec_c_api_overview_structure` for details on how objects
are initialised and freed.
@endrst

@param self A pointer to an uninitialised tsk_population_table_t object.
@param options Allocation time options. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_population_table_init(tsk_population_table_t *self, tsk_flags_t options);

/**
@brief Free the internal memory for the specified table.

@param self A pointer to an initialised tsk_population_table_t object.
@return Always returns 0.
*/
int tsk_population_table_free(tsk_population_table_t *self);

/**
@brief Adds a row to this population table.

@rst
Add a new population with the specified ``metadata`` to the table. A copy of the
``metadata`` is immediately taken. See the :ref:`table definition
<sec_population_table_definition>` for details of the columns in this table.
@endrst

@param self A pointer to a tsk_population_table_t object.
@param metadata The metadata to be associated with the new population. This
    is a pointer to arbitrary memory. Can be ``NULL`` if ``metadata_length`` is 0.
@param metadata_length The size of the metadata array in bytes.
@return Return the ID of the newly added population on success,
    or a negative value on failure.
*/
tsk_id_t tsk_population_table_add_row(
    tsk_population_table_t *self, const char *metadata, tsk_size_t metadata_length);

/**
@brief Updates the row at the specified index.

@rst
Rewrite the row at the specified index in this table to use the specified
values. A copy of the ``metadata`` parameter is taken immediately. See the
:ref:`table definition <sec_population_table_definition>` for details of the
columns in this table.

.. warning::
    Because of the way that ragged columns are encoded, this method requires a
    full rewrite of the internal column memory in worst case, and would
    therefore be inefficient for bulk updates for such columns. However, if the
    sizes of all ragged column values are unchanged in the updated row, this
    method is guaranteed to only update the memory for the row in question.
@endrst

@param self A pointer to a tsk_population_table_t object.
@param index The row to update.
@param metadata The metadata to be associated with the population. This
    is a pointer to arbitrary memory. Can be ``NULL`` if ``metadata_length`` is 0.
@param metadata_length The size of the metadata array in bytes.
@return Return 0 on success or a negative value on failure.
*/
int tsk_population_table_update_row(tsk_population_table_t *self, tsk_id_t index,
    const char *metadata, tsk_size_t metadata_length);

/**
@brief Clears this table, setting the number of rows to zero.

@rst
No memory is freed as a result of this operation; please use
:c:func:`tsk_population_table_free` to free the table's internal resources. Note that the
metadata schema is not cleared.
@endrst

@param self A pointer to a tsk_population_table_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_population_table_clear(tsk_population_table_t *self);

/**
@brief Truncates this table so that only the first num_rows are retained.

@param self A pointer to a tsk_population_table_t object.
@param num_rows The number of rows to retain in the table.
@return Return 0 on success or a negative value on failure.
*/
int tsk_population_table_truncate(tsk_population_table_t *self, tsk_size_t num_rows);

/**
@brief Extends this table by appending rows copied from another table.

@rst
Appends the rows at the specified indexes from the table ``other`` to the end of this
table. Row indexes can be repeated and in any order. If ``row_indexes`` is NULL, append
the first ``num_rows`` from ``other`` to this table. Note that metadata is copied as-is
and is not checked for compatibility with any existing schema on this table.
@endrst

@param self A pointer to a tsk_population_table_t object where rows are to be added.
@param other A pointer to a tsk_population_table_t object where rows are copied from.
@param num_rows The number of rows from ``other`` to append to this table.
@param row_indexes Array of row indexes in ``other``. If ``NULL`` is passed then the
    first ``num_rows`` of ``other`` are used.
@param options Bitwise option flags. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_population_table_extend(tsk_population_table_t *self,
    const tsk_population_table_t *other, tsk_size_t num_rows,
    const tsk_id_t *row_indexes, tsk_flags_t options);

/**
@brief Subset this table by keeping rows according to a boolean mask.

@rst
Deletes rows from this table and optionally return the mapping from IDs in
the current table to the updated table. Rows are kept or deleted according to
the specified boolean array ``keep`` such that for each row ``j`` if
``keep[j]`` is false (zero) the row is deleted, and otherwise the row is
retained. Thus, ``keep`` must be an array of at least ``num_rows``
:c:type:`bool` values.

If the ``id_map`` argument is non-null, this array will be updated to represent
the mapping between IDs before and after row deletion. For row ``j``,
``id_map[j]`` will contain the new ID for row ``j`` if it is retained, or
:c:macro:`TSK_NULL` if the row has been removed. Thus, ``id_map`` must be an
array of at least ``num_rows`` :c:type:`tsk_id_t` values.

.. warning::
    C++ users need to be careful to specify the correct type when
    passing in values for the ``keep`` array,
    using ``std::vector<tsk_bool_t>`` and not ``std::vector<bool>``,
    as the latter may not be correct size.

@endrst

@param self A pointer to a tsk_population_table_t object.
@param keep Array of boolean flags describing whether a particular
    row should be kept or not. Must be at least ``num_rows`` long.
@param options Bitwise option flags. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@param id_map An array in which to store the mapping between new
    and old IDs. If NULL, this will be ignored.
@return Return 0 on success or a negative value on failure.
*/
int tsk_population_table_keep_rows(tsk_population_table_t *self, const tsk_bool_t *keep,
    tsk_flags_t options, tsk_id_t *id_map);

/**
@brief Returns true if the data in the specified table is identical to the data
       in this table.

@rst

**Options**

Options to control the comparison can be specified by providing one or
more of the following bitwise flags. By default (options=0) tables are
considered equal if they are byte-wise identical in all columns,
and their metadata schemas are byte-wise identical.

- :c:macro:`TSK_CMP_IGNORE_METADATA`
    Do not include metadata in the comparison. Note that as metadata is the
    only column in the population table, two population tables are considered
    equal if they have the same number of rows if this flag is specified.
@endrst

@param self A pointer to a tsk_population_table_t object.
@param other A pointer to a tsk_population_table_t object.
@param options Bitwise comparison options.
@return Return true if the specified table is equal to this table.
*/
bool tsk_population_table_equals(const tsk_population_table_t *self,
    const tsk_population_table_t *other, tsk_flags_t options);

/**
@brief Copies the state of this table into the specified destination.

@rst
By default the method initialises the specified destination table. If the
destination is already initialised, the :c:macro:`TSK_NO_INIT` option should
be supplied to avoid leaking memory.
@endrst

@param self A pointer to a tsk_population_table_t object.
@param dest A pointer to a tsk_population_table_t object. If the TSK_NO_INIT
option is specified, this must be an initialised population table. If not, it must be an
uninitialised population table.
@param options Bitwise option flags.
@return Return 0 on success or a negative value on failure.
*/
int tsk_population_table_copy(const tsk_population_table_t *self,
    tsk_population_table_t *dest, tsk_flags_t options);

/**
@brief Get the row at the specified index.

@rst
Updates the specified population struct to reflect the values in the specified row.
Pointers to memory within this struct are handled by the table and should **not**
be freed by client code. These pointers are guaranteed to be valid until the
next operation that modifies the table (e.g., by adding a new row), but not afterwards.
@endrst

@param self A pointer to a tsk_population_table_t object.
@param index The requested table row.
@param row A pointer to a tsk_population_t struct that is updated to reflect the
    values in the specified row.
@return Return 0 on success or a negative value on failure.
*/
int tsk_population_table_get_row(
    const tsk_population_table_t *self, tsk_id_t index, tsk_population_t *row);

/**
@brief Set the metadata schema
@rst
Copies the metadata schema string to this table, replacing any existing.
@endrst
@param self A pointer to a tsk_population_table_t object.
@param metadata_schema A pointer to a char array.
@param metadata_schema_length The size of the metadata schema in bytes.
@return Return 0 on success or a negative value on failure.
*/
int tsk_population_table_set_metadata_schema(tsk_population_table_t *self,
    const char *metadata_schema, tsk_size_t metadata_schema_length);

/**
@brief Print out the state of this table to the specified stream.

This method is intended for debugging purposes and should not be used
in production code. The format of the output should **not** be depended
on and may change arbitrarily between versions.

@param self A pointer to a tsk_population_table_t object.
@param out The stream to write the summary to.
*/
void tsk_population_table_print_state(const tsk_population_table_t *self, FILE *out);

/**
@brief Replace this table's data by copying from a set of column arrays

@rst
Clears the data columns of this table and then copies column data from the specified
set of arrays. The supplied arrays should all contain data on the same number of rows.
The metadata schema is not affected.
@endrst

@param self A pointer to a tsk_population_table_t object.
@param num_rows The number of rows to copy from the specifed arrays.
@param metadata The array of char metadata values to be copied.
@param metadata_offset The array of tsk_size_t metadata offset values to be copied.
@return Return 0 on success or a negative value on failure.
*/
int tsk_population_table_set_columns(tsk_population_table_t *self, tsk_size_t num_rows,
    const char *metadata, const tsk_size_t *metadata_offset);

/**
@brief Extends this table by copying from a set of column arrays

@rst
Copies column data from the specified set of arrays to create new rows at the end of the
table. The supplied arrays should all contain data on the same number of rows. The
metadata schema is not affected.
@endrst

@param self A pointer to a tsk_population_table_t object.
@param num_rows The number of rows to copy from the specifed arrays.
@param metadata The array of char metadata values to be copied.
@param metadata_offset The array of tsk_size_t metadata offset values to be copied.
@return Return 0 on success or a negative value on failure.
*/
int tsk_population_table_append_columns(tsk_population_table_t *self,
    tsk_size_t num_rows, const char *metadata, const tsk_size_t *metadata_offset);

/**
@brief Controls the pre-allocation strategy for this table

@rst
Set a fixed pre-allocation size, or use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_population_table_t object.
@param max_rows_increment The number of rows to pre-allocate, or zero for the default
    doubling strategy.
@return Return 0 on success or a negative value on failure.
*/
int tsk_population_table_set_max_rows_increment(
    tsk_population_table_t *self, tsk_size_t max_rows_increment);

/**
@brief Controls the pre-allocation strategy for the metadata column

@rst
Set a fixed pre-allocation size, or use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_population_table_t object.
@param max_metadata_length_increment The number of bytes to pre-allocate, or zero for
the default doubling strategy.
@return Return 0 on success or a negative value on failure.
*/
int tsk_population_table_set_max_metadata_length_increment(
    tsk_population_table_t *self, tsk_size_t max_metadata_length_increment);

/** @} */

/* Undocumented methods */

int tsk_population_table_dump_text(const tsk_population_table_t *self, FILE *out);

/**
@defgroup PROVENANCE_TABLE_API_GROUP Provenance table API.
@{
*/

/**
@brief Initialises the table by allocating the internal memory.

@rst
This must be called before any operations are performed on the table.
See the :ref:`sec_c_api_overview_structure` for details on how objects
are initialised and freed.
@endrst

@param self A pointer to an uninitialised tsk_provenance_table_t object.
@param options Allocation time options. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_provenance_table_init(tsk_provenance_table_t *self, tsk_flags_t options);

/**
@brief Free the internal memory for the specified table.

@param self A pointer to an initialised tsk_provenance_table_t object.
@return Always returns 0.
*/
int tsk_provenance_table_free(tsk_provenance_table_t *self);

/**
@brief Adds a row to this provenance table.

@rst
Add a new provenance with the specified ``timestamp`` and ``record`` to the table.
Copies of the ``timestamp`` and ``record`` are immediately taken.
See the :ref:`table definition <sec_provenance_table_definition>`
for details of the columns in this table.
@endrst

@param self A pointer to a tsk_provenance_table_t object.
@param timestamp The timestamp to be associated with the new provenance. This
    is a pointer to arbitrary memory. Can be ``NULL`` if ``timestamp_length`` is 0.
@param timestamp_length The size of the timestamp array in bytes.
@param record The record to be associated with the new provenance. This
    is a pointer to arbitrary memory. Can be ``NULL`` if ``record_length`` is 0.
@param record_length The size of the record array in bytes.
@return Return the ID of the newly added provenance on success,
    or a negative value on failure.
*/
tsk_id_t tsk_provenance_table_add_row(tsk_provenance_table_t *self,
    const char *timestamp, tsk_size_t timestamp_length, const char *record,
    tsk_size_t record_length);

/**
@brief Updates the row at the specified index.

@rst
Rewrite the row at the specified index in this table to use the specified
values. Copies of the ``timestamp`` and ``record`` parameters are taken
immediately. See the :ref:`table definition <sec_provenance_table_definition>`
for details of the columns in this table.

.. warning::
    Because of the way that ragged columns are encoded, this method requires a
    full rewrite of the internal column memory in worst case, and would
    therefore be inefficient for bulk updates for such columns. However, if the
    sizes of all ragged column values are unchanged in the updated row, this
    method is guaranteed to only update the memory for the row in question.
@endrst

@param self A pointer to a tsk_provenance_table_t object.
@param index The row to update.
@param timestamp The timestamp to be associated with new provenance. This
    is a pointer to arbitrary memory. Can be ``NULL`` if ``timestamp_length`` is 0.
@param timestamp_length The size of the timestamp array in bytes.
@param record The record to be associated with the provenance. This
    is a pointer to arbitrary memory. Can be ``NULL`` if ``record_length`` is 0.
@param record_length The size of the record array in bytes.
@return Return 0 on success or a negative value on failure.
*/
int tsk_provenance_table_update_row(tsk_provenance_table_t *self, tsk_id_t index,
    const char *timestamp, tsk_size_t timestamp_length, const char *record,
    tsk_size_t record_length);

/**
@brief Clears this table, setting the number of rows to zero.

@rst
No memory is freed as a result of this operation; please use
:c:func:`tsk_provenance_table_free` to free the table's internal resources.
@endrst

@param self A pointer to a tsk_provenance_table_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_provenance_table_clear(tsk_provenance_table_t *self);

/**
@brief Truncates this table so that only the first num_rows are retained.

@param self A pointer to a tsk_provenance_table_t object.
@param num_rows The number of rows to retain in the table.
@return Return 0 on success or a negative value on failure.
*/
int tsk_provenance_table_truncate(tsk_provenance_table_t *self, tsk_size_t num_rows);

/**
@brief Extends this table by appending rows copied from another table.

@rst
Appends the rows at the specified indexes from the table ``other`` to the end of this
table. Row indexes can be repeated and in any order. If ``row_indexes`` is NULL, append
the first ``num_rows`` from ``other`` to this table.
@endrst

@param self A pointer to a tsk_provenance_table_t object where rows are to be added.
@param other A pointer to a tsk_provenance_table_t object where rows are copied from.
@param num_rows The number of rows from ``other`` to append to this table.
@param row_indexes Array of row indexes in ``other``. If ``NULL`` is passed then the
    first ``num_rows`` of ``other`` are used.
@param options Bitwise option flags. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_provenance_table_extend(tsk_provenance_table_t *self,
    const tsk_provenance_table_t *other, tsk_size_t num_rows,
    const tsk_id_t *row_indexes, tsk_flags_t options);

/**
@brief Subset this table by keeping rows according to a boolean mask.

@rst
Deletes rows from this table and optionally return the mapping from IDs in
the current table to the updated table. Rows are kept or deleted according to
the specified boolean array ``keep`` such that for each row ``j`` if
``keep[j]`` is false (zero) the row is deleted, and otherwise the row is
retained. Thus, ``keep`` must be an array of at least ``num_rows``
:c:type:`bool` values.

If the ``id_map`` argument is non-null, this array will be updated to represent
the mapping between IDs before and after row deletion. For row ``j``,
``id_map[j]`` will contain the new ID for row ``j`` if it is retained, or
:c:macro:`TSK_NULL` if the row has been removed. Thus, ``id_map`` must be an
array of at least ``num_rows`` :c:type:`tsk_id_t` values.

.. warning::
    C++ users need to be careful to specify the correct type when
    passing in values for the ``keep`` array,
    using ``std::vector<tsk_bool_t>`` and not ``std::vector<bool>``,
    as the latter may not be correct size.

@endrst

@param self A pointer to a tsk_provenance_table_t object.
@param keep Array of boolean flags describing whether a particular
    row should be kept or not. Must be at least ``num_rows`` long.
@param options Bitwise option flags. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@param id_map An array in which to store the mapping between new
    and old IDs. If NULL, this will be ignored.
@return Return 0 on success or a negative value on failure.
*/
int tsk_provenance_table_keep_rows(tsk_provenance_table_t *self, const tsk_bool_t *keep,
    tsk_flags_t options, tsk_id_t *id_map);

/**
@brief Returns true if the data in the specified table is identical to the data
       in this table.

@rst

**Options**

Options to control the comparison can be specified by providing one or
more of the following bitwise flags. By default (options=0) tables are
considered equal if they are byte-wise identical in all columns.

- :c:macro:`TSK_CMP_IGNORE_TIMESTAMPS`
@endrst

@param self A pointer to a tsk_provenance_table_t object.
@param other A pointer to a tsk_provenance_table_t object.
@param options Bitwise comparison options.
@return Return true if the specified table is equal to this table.
*/
bool tsk_provenance_table_equals(const tsk_provenance_table_t *self,
    const tsk_provenance_table_t *other, tsk_flags_t options);

/**
@brief Copies the state of this table into the specified destination.

@rst
By default the method initialises the specified destination table. If the
destination is already initialised, the :c:macro:`TSK_NO_INIT` option should
be supplied to avoid leaking memory.
@endrst

@param self A pointer to a tsk_provenance_table_t object.
@param dest A pointer to a tsk_provenance_table_t object. If the TSK_NO_INIT
option is specified, this must be an initialised provenance table. If not, it must be an
uninitialised provenance table.
@param options Bitwise option flags.
@return Return 0 on success or a negative value on failure.
*/
int tsk_provenance_table_copy(const tsk_provenance_table_t *self,
    tsk_provenance_table_t *dest, tsk_flags_t options);

/**
@brief Get the row at the specified index.

@rst
Updates the specified provenance struct to reflect the values in the specified row.
Pointers to memory within this struct are handled by the table and should **not**
be freed by client code. These pointers are guaranteed to be valid until the
next operation that modifies the table (e.g., by adding a new row), but not afterwards.
@endrst

@param self A pointer to a tsk_provenance_table_t object.
@param index The requested table row.
@param row A pointer to a tsk_provenance_t struct that is updated to reflect the
    values in the specified row.
@return Return 0 on success or a negative value on failure.
*/
int tsk_provenance_table_get_row(
    const tsk_provenance_table_t *self, tsk_id_t index, tsk_provenance_t *row);

/**
@brief Print out the state of this table to the specified stream.

This method is intended for debugging purposes and should not be used
in production code. The format of the output should **not** be depended
on and may change arbitrarily between versions.

@param self A pointer to a tsk_provenance_table_t object.
@param out The stream to write the summary to.
*/
void tsk_provenance_table_print_state(const tsk_provenance_table_t *self, FILE *out);

/**
@brief Replace this table's data by copying from a set of column arrays

@rst
Clears the data columns of this table and then copies column data from the specified
set of arrays. The supplied arrays should all contain data on the same number of rows.
The metadata schema is not affected.
@endrst

@param self A pointer to a tsk_provenance_table_t object.
@param num_rows The number of rows to copy from the specifed arrays.
@param timestamp The array of char timestamp values to be copied.
@param timestamp_offset The array of tsk_size_t timestamp offset values to be copied.
@param record The array of char record values to be copied.
@param record_offset The array of tsk_size_t record offset values to be copied.
@return Return 0 on success or a negative value on failure.
*/
int tsk_provenance_table_set_columns(tsk_provenance_table_t *self, tsk_size_t num_rows,
    const char *timestamp, const tsk_size_t *timestamp_offset, const char *record,
    const tsk_size_t *record_offset);

/**
@brief Extends this table by copying from a set of column arrays

@rst
Copies column data from the specified set of arrays to create new rows at the end of the
table. The supplied arrays should all contain data on the same number of rows. The
metadata schema is not affected.
@endrst

@param self A pointer to a tsk_provenance_table_t object.
@param num_rows The number of rows to copy from the specifed arrays.
@param timestamp The array of char timestamp values to be copied.
@param timestamp_offset The array of tsk_size_t timestamp offset values to be copied.
@param record The array of char record values to be copied.
@param record_offset The array of tsk_size_t record offset values to be copied.
@return Return 0 on success or a negative value on failure.
*/
int tsk_provenance_table_append_columns(tsk_provenance_table_t *self,
    tsk_size_t num_rows, const char *timestamp, const tsk_size_t *timestamp_offset,
    const char *record, const tsk_size_t *record_offset);

/**
@brief Controls the pre-allocation strategy for this table

@rst
Set a fixed pre-allocation size, or use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_provenance_table_t object.
@param max_rows_increment The number of rows to pre-allocate, or zero for the default
    doubling strategy.
@return Return 0 on success or a negative value on failure.
*/
int tsk_provenance_table_set_max_rows_increment(
    tsk_provenance_table_t *self, tsk_size_t max_rows_increment);

/**
@brief Controls the pre-allocation strategy for the timestamp column

@rst
Set a fixed pre-allocation size, or use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_provenance_table_t object.
@param max_timestamp_length_increment The number of bytes to pre-allocate, or zero for
the default doubling strategy.
@return Return 0 on success or a negative value on failure.
*/
int tsk_provenance_table_set_max_timestamp_length_increment(
    tsk_provenance_table_t *self, tsk_size_t max_timestamp_length_increment);

/**
@brief Controls the pre-allocation strategy for the record column

@rst
Set a fixed pre-allocation size, use the default doubling strategy.
See :ref:`sec_c_api_memory_allocation_strategy` for details on the default
pre-allocation strategy,
@endrst

@param self A pointer to a tsk_provenance_table_t object.
@param max_record_length_increment The number of bytes to pre-allocate, or zero for the
default doubling strategy.
@return Return 0 on success or a negative value on failure.
*/
int tsk_provenance_table_set_max_record_length_increment(
    tsk_provenance_table_t *self, tsk_size_t max_record_length_increment);

/** @} */

/* Undocumented methods */
int tsk_provenance_table_dump_text(const tsk_provenance_table_t *self, FILE *out);

/****************************************************************************/
/* Table collection .*/
/****************************************************************************/

/**
@defgroup TABLE_COLLECTION_API_GROUP Table collection API.
@{
*/

/**
@brief Initialises the table collection by allocating the internal memory
       and initialising all the constituent tables.

@rst
This must be called before any operations are performed on the table
collection. See the :ref:`sec_c_api_overview_structure` for details on how objects
are initialised and freed.

**Options**

Options can be specified by providing bitwise flags:

- :c:macro:`TSK_TC_NO_EDGE_METADATA`
@endrst

@param self A pointer to an uninitialised tsk_table_collection_t object.
@param options Allocation time options as above.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_init(tsk_table_collection_t *self, tsk_flags_t options);

/**
@brief Free the internal memory for the specified table collection.

@param self A pointer to an initialised tsk_table_collection_t object.
@return Always returns 0.
*/
int tsk_table_collection_free(tsk_table_collection_t *self);

/**
@brief Clears data tables (and optionally provenances and metadata) in
this table collection.

@rst
By default this operation clears all tables except the provenance table, retaining
table metadata schemas and the tree-sequence level metadata and schema.

No memory is freed as a result of this operation; please use
:c:func:`tsk_table_collection_free` to free internal resources.

**Options**

Options can be specified by providing one or more of the following bitwise
flags:

- :c:macro:`TSK_CLEAR_PROVENANCE`
- :c:macro:`TSK_CLEAR_METADATA_SCHEMAS`
- :c:macro:`TSK_CLEAR_TS_METADATA_AND_SCHEMA`
@endrst

@param self A pointer to a tsk_table_collection_t object.
@param options Bitwise clearing options.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_clear(tsk_table_collection_t *self, tsk_flags_t options);

/**
@brief Returns true if the data in the specified table collection is equal
    to the data in this table collection.

@rst

Returns true if the two table collections are equal. The indexes are
not considered as these are derived from the tables. We also do not
consider the ``file_uuid``, since it is a property of the file that set
of tables is stored in.

**Options**

Options to control the comparison can be specified by providing one or
more of the following bitwise flags. By default (options=0) two table
collections are considered equal if all of the tables are byte-wise
identical, and the sequence lengths, metadata and metadata schemas
of the two table collections are identical.

- :c:macro:`TSK_CMP_IGNORE_PROVENANCE`
- :c:macro:`TSK_CMP_IGNORE_METADATA`
- :c:macro:`TSK_CMP_IGNORE_TS_METADATA`
- :c:macro:`TSK_CMP_IGNORE_TIMESTAMPS`
- :c:macro:`TSK_CMP_IGNORE_TABLES`
- :c:macro:`TSK_CMP_IGNORE_REFERENCE_SEQUENCE`
@endrst

@param self A pointer to a tsk_table_collection_t object.
@param other A pointer to a tsk_table_collection_t object.
@param options Bitwise comparison options.
@return Return true if the specified table collection is equal to this table.
*/
bool tsk_table_collection_equals(const tsk_table_collection_t *self,
    const tsk_table_collection_t *other, tsk_flags_t options);

/**
@brief Copies the state of this table collection into the specified destination.

@rst
By default the method initialises the specified destination table collection. If the
destination is already initialised, the :c:macro:`TSK_NO_INIT` option should
be supplied to avoid leaking memory.

**Options**

Options can be specified by providing bitwise flags:

:c:macro:`TSK_COPY_FILE_UUID`
@endrst

@param self A pointer to a tsk_table_collection_t object.
@param dest A pointer to a tsk_table_collection_t object. If the TSK_NO_INIT
option is specified, this must be an initialised table collection. If not, it must be an
uninitialised table collection.
@param options Bitwise option flags.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_copy(const tsk_table_collection_t *self,
    tsk_table_collection_t *dest, tsk_flags_t options);

/**
@brief Print out the state of this table collection to the specified stream.

This method is intended for debugging purposes and should not be used
in production code. The format of the output should **not** be depended
on and may change arbitrarily between versions.

@param self A pointer to a tsk_table_collection_t object.
@param out The stream to write the summary to.
*/
void tsk_table_collection_print_state(const tsk_table_collection_t *self, FILE *out);

/**
@brief Load a table collection from a file path.

@rst
Loads the data from the specified file into this table collection.
By default, the table collection is also initialised.
The resources allocated must be freed using
:c:func:`tsk_table_collection_free` even in error conditions.

If the :c:macro:`TSK_NO_INIT` option is set, the table collection is
not initialised, allowing an already initialised table collection to
be overwritten with the data from a file.

If the file contains multiple table collections, this function will load
the first. Please see the :c:func:`tsk_table_collection_loadf` for details
on how to sequentially load table collections from a stream.

If the :c:macro:`TSK_LOAD_SKIP_TABLES` option is set, only the non-table information from
the table collection will be read, leaving all tables with zero rows and no
metadata or schema.
If the :c:macro:`TSK_LOAD_SKIP_REFERENCE_SEQUENCE` option is set, the table collection is
read without loading the reference sequence.

**Options**

Options can be specified by providing one or more of the following bitwise
flags:

- :c:macro:`TSK_NO_INIT`
- :c:macro:`TSK_LOAD_SKIP_TABLES`
- :c:macro:`TSK_LOAD_SKIP_REFERENCE_SEQUENCE`

**Examples**

.. code-block:: c

    int ret;
    tsk_table_collection_t tables;
    ret = tsk_table_collection_load(&tables, "data.trees", 0);
    if (ret != 0) {
        fprintf(stderr, "Load error:%s\n", tsk_strerror(ret));
        exit(EXIT_FAILURE);
    }

@endrst

@param self A pointer to an uninitialised tsk_table_collection_t object
    if the TSK_NO_INIT option is not set (default), or an initialised
    tsk_table_collection_t otherwise.
@param filename A NULL terminated string containing the filename.
@param options Bitwise options. See above for details.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_load(
    tsk_table_collection_t *self, const char *filename, tsk_flags_t options);

/**
@brief Load a table collection from a stream.

@rst
Loads a tables definition from the specified file stream to this table
collection. By default, the table collection is also initialised.
The resources allocated must be freed using
:c:func:`tsk_table_collection_free` even in error conditions.

If the :c:macro:`TSK_NO_INIT` option is set, the table collection is
not initialised, allowing an already initialised table collection to
be overwritten with the data from a file.

The stream can be an arbitrary file descriptor, for example a network socket.
No seek operations are performed.

If the stream contains multiple table collection definitions, this function
will load the next table collection from the stream. If the stream contains no
more table collection definitions the error value :c:macro:`TSK_ERR_EOF` will
be returned. Note that EOF is only returned in the case where zero bytes are
read from the stream --- malformed files or other errors will result in
different error conditions. Please see the
:ref:`sec_c_api_examples_file_streaming` section for an example of how to
sequentially load tree sequences from a stream.

Please note that this streaming behaviour is not supported if the
:c:macro:`TSK_LOAD_SKIP_TABLES` or :c:macro:`TSK_LOAD_SKIP_REFERENCE_SEQUENCE` option is
set. If the :c:macro:`TSK_LOAD_SKIP_TABLES` option is set, only the non-table information
from the table collection will be read, leaving all tables with zero rows and no metadata
or schema. If the :c:macro:`TSK_LOAD_SKIP_REFERENCE_SEQUENCE` option is set, the table
collection is read without loading the reference sequence. When attempting to read from a
stream with multiple table collection definitions and either of these two options set,
the requested information from the first table collection will be read on the first call
to :c:func:`tsk_table_collection_loadf`, with subsequent calls leading to errors.

**Options**

Options can be specified by providing one or more of the following bitwise
flags:

- :c:macro:`TSK_NO_INIT`
- :c:macro:`TSK_LOAD_SKIP_TABLES`
- :c:macro:`TSK_LOAD_SKIP_REFERENCE_SEQUENCE`
@endrst

@param self A pointer to an uninitialised tsk_table_collection_t object
    if the TSK_NO_INIT option is not set (default), or an initialised
    tsk_table_collection_t otherwise.
@param file A FILE stream opened in an appropriate mode for reading (e.g.
    "r", "r+" or "w+") positioned at the beginning of a table collection
    definition.
@param options Bitwise options. See above for details.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_loadf(
    tsk_table_collection_t *self, FILE *file, tsk_flags_t options);

/**
@brief Write a table collection to file.

@rst
Writes the data from this table collection to the specified file.

If an error occurs the file path is deleted, ensuring that only complete
and well formed files will be written.

**Examples**

.. code-block:: c

    int ret;
    tsk_table_collection_t tables;

    ret = tsk_table_collection_init(&tables, 0);
    error_check(ret);
    tables.sequence_length = 1.0;
    // Write out the empty tree sequence
    ret = tsk_table_collection_dump(&tables, "empty.trees", 0);
    error_check(ret);

@endrst

@param self A pointer to an initialised tsk_table_collection_t object.
@param filename A NULL terminated string containing the filename.
@param options Bitwise options. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_dump(
    const tsk_table_collection_t *self, const char *filename, tsk_flags_t options);

/**
@brief Write a table collection to a stream.

@rst
Writes the data from this table collection to the specified FILE stream.
Semantics are identical to :c:func:`tsk_table_collection_dump`.

Please see the :ref:`sec_c_api_examples_file_streaming` section for an example
of how to sequentially dump and load tree sequences from a stream.

@endrst

@param self A pointer to an initialised tsk_table_collection_t object.
@param file A FILE stream opened in an appropriate mode for writing (e.g.
    "w", "a", "r+" or "w+").
@param options Bitwise options. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_dumpf(
    const tsk_table_collection_t *self, FILE *file, tsk_flags_t options);

/**
@brief Record the number of rows in each table in the specified tsk_bookmark_t object.

@param self A pointer to an initialised tsk_table_collection_t object.
@param bookmark A pointer to a tsk_bookmark_t which is updated to contain the number of
    rows in all tables.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_record_num_rows(
    const tsk_table_collection_t *self, tsk_bookmark_t *bookmark);

/**
@brief Truncates the tables in this table collection according to the specified bookmark.

@rst
Truncate the tables in this collection so that each one has the number
of rows specified in the parameter :c:type:`tsk_bookmark_t`. Use the
:c:func:`tsk_table_collection_record_num_rows` function to record the
number rows for each table in a table collection at a particular time.
@endrst

@param self A pointer to a tsk_individual_table_t object.
@param bookmark The number of rows to retain in each table.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_truncate(
    tsk_table_collection_t *self, tsk_bookmark_t *bookmark);

/**
@brief Sorts the tables in this collection.

@rst
Some of the tables in a table collection must satisfy specific sortedness requirements
in order to define a :ref:`valid tree sequence <sec_valid_tree_sequence_requirements>`.
This method sorts the ``edge``, ``site``, ``mutation`` and ``individual`` tables such
that these requirements are guaranteed to be fulfilled. The ``node``, ``population``
and ``provenance`` tables do not have any sortedness requirements, and are therefore
ignored by this method.

.. note:: The current implementation **may** sort in such a way that exceeds
    these requirements, but this behaviour should not be relied upon and later
    versions may weaken the level of sortedness. However, the method does **guarantee**
    that the resulting tables describes a valid tree sequence.

.. warning:: Sorting migrations is currently not supported and an error will be raised
    if a table collection containing a non-empty migration table is specified.

The specified :c:type:`tsk_bookmark_t` allows us to specify a start position
for sorting in each of the tables; rows before this value are assumed to already be
in sorted order and this information is used to make sorting more efficient.
Positions in tables that are not sorted (``node``, ``population``
and ``provenance``) are ignored and can be set to arbitrary values.

.. warning:: The current implementation only supports specifying a start
    position for the ``edge`` table and in a limited form for the
    ``site``, ``mutation`` and ``individual`` tables. Specifying a non-zero
    ``migration``, start position results in an error. The start positions for the
    ``site``, ``mutation`` and ``individual`` tables can either be 0 or the length of the
    respective tables, allowing these tables to either be fully sorted, or not sorted at
    all.

The table collection will always be unindexed after sort successfully completes.

For more control over the sorting process, see the :ref:`sec_c_api_low_level_sorting`
section.

**Options**

Options can be specified by providing one or more of the following bitwise
flags:

:c:macro:`TSK_NO_CHECK_INTEGRITY`
    Do not run integrity checks using
    :c:func:`tsk_table_collection_check_integrity` before sorting,
    potentially leading to a small reduction in execution time. This
    performance optimisation should not be used unless the calling code can
    guarantee reference integrity within the table collection. References
    to rows not in the table or bad offsets will result in undefined
    behaviour.
@endrst

@param self A pointer to a tsk_table_collection_t object.
@param start The position to begin sorting in each table; all rows less than this
    position must fulfill the tree sequence sortedness requirements. If this is
    NULL, sort all rows.
@param options Sort options.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_sort(
    tsk_table_collection_t *self, const tsk_bookmark_t *start, tsk_flags_t options);

/**
@brief Sorts the individual table in this collection.

@rst
Sorts the individual table in place, so that parents come before children,
and the parent column is remapped as required. Node references to individuals
are also updated.
@endrst

@param self A pointer to a tsk_table_collection_t object.
@param options Sort options. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_individual_topological_sort(
    tsk_table_collection_t *self, tsk_flags_t options);

/**
@brief Puts the tables into canonical form.

@rst
Put tables into canonical form such that randomly reshuffled tables
are guaranteed to always be sorted in the same order, and redundant
information is removed. The canonical sorting exceeds the usual
tree sequence sortedness requirements.

**Options**:

Options can be specified by providing one or more of the following bitwise
flags:

- :c:macro:`TSK_SUBSET_KEEP_UNREFERENCED`

@endrst

@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_canonicalise(tsk_table_collection_t *self, tsk_flags_t options);

/**
@brief Simplify the tables to remove redundant information.

@rst
Simplification transforms the tables to remove redundancy and canonicalise
tree sequence data. See the :ref:`simplification <sec_simplification>` tutorial for
more details.

A mapping from the node IDs in the table before simplification to their equivalent
values after simplification can be obtained via the ``node_map`` argument. If this
is non NULL, ``node_map[u]`` will contain the new ID for node ``u`` after simplification,
or :c:macro:`TSK_NULL` if the node has been removed. Thus, ``node_map`` must be an array
of at least ``self->nodes.num_rows`` :c:type:`tsk_id_t` values.

If the `TSK_SIMPLIFY_NO_FILTER_NODES` option is specified, the node table will be
unaltered except for changing the sample status of nodes (but see the
`TSK_SIMPLIFY_NO_UPDATE_SAMPLE_FLAGS` option below) and to update references
to other tables that may have changed as a result of filtering (see below).
The ``node_map`` (if specified) will always be the identity mapping, such that
``node_map[u] == u`` for all nodes. Note also that the order of the list of
samples is not important in this case.

When a table is not filtered (i.e., if the `TSK_SIMPLIFY_NO_FILTER_NODES`
option is provided or the `TSK_SIMPLIFY_FILTER_SITES`,
`TSK_SIMPLIFY_FILTER_POPULATIONS` or `TSK_SIMPLIFY_FILTER_INDIVIDUALS`
options are *not* provided) the corresponding table is modified as
little as possible, and all pointers are guaranteed to remain valid
after simplification. The only changes made to an unfiltered table are
to update any references to tables that may have changed (for example,
remapping population IDs in the node table if
`TSK_SIMPLIFY_FILTER_POPULATIONS` was specified) or altering the
sample status flag of nodes.

.. note:: It is possible for populations and individuals to be filtered
   even if `TSK_SIMPLIFY_NO_FILTER_NODES` is specified because there
   may be entirely unreferenced entities in the input tables, which
   are not affected by whether we filter nodes or not.

By default, the node sample flags are updated by unsetting the
:c:macro:`TSK_NODE_IS_SAMPLE` flag for all nodes and subsequently setting it
for the nodes provided as input to this function. The
`TSK_SIMPLIFY_NO_UPDATE_SAMPLE_FLAGS` option will prevent this from occuring,
making it the responsibility of calling code to keep track of the ultimate
sample status of nodes. Using this option in conjunction with
`TSK_SIMPLIFY_NO_FILTER_NODES` (and without the
`TSK_SIMPLIFY_FILTER_POPULATIONS` and `TSK_SIMPLIFY_FILTER_INDIVIDUALS`
options) guarantees that the node table will not be written to during the
lifetime of this function.

The table collection will always be unindexed after simplify successfully completes.

.. note:: Migrations are currently not supported by simplify, and an error will
    be raised if we attempt call simplify on a table collection with greater
    than zero migrations. See `<https://github.com/tskit-dev/tskit/issues/20>`_

**Options**:

Options can be specified by providing one or more of the following bitwise
flags:

- :c:macro:`TSK_SIMPLIFY_FILTER_SITES`
- :c:macro:`TSK_SIMPLIFY_FILTER_POPULATIONS`
- :c:macro:`TSK_SIMPLIFY_FILTER_INDIVIDUALS`
- :c:macro:`TSK_SIMPLIFY_NO_FILTER_NODES`
- :c:macro:`TSK_SIMPLIFY_NO_UPDATE_SAMPLE_FLAGS`
- :c:macro:`TSK_SIMPLIFY_REDUCE_TO_SITE_TOPOLOGY`
- :c:macro:`TSK_SIMPLIFY_KEEP_UNARY`
- :c:macro:`TSK_SIMPLIFY_KEEP_INPUT_ROOTS`
- :c:macro:`TSK_SIMPLIFY_KEEP_UNARY_IN_INDIVIDUALS`
@endrst

@param self A pointer to a tsk_table_collection_t object.
@param samples Either NULL or an array of num_samples distinct and valid node IDs.
    If non-null the nodes in this array will be marked as samples in the output.
    If NULL, the num_samples parameter is ignored and the samples in the output
    will be the same as the samples in the input. This is equivalent to populating
    the samples array with all of the sample nodes in the input in increasing
    order of ID.
@param num_samples The number of node IDs in the input samples array. Ignored
    if the samples array is NULL.
@param options Simplify options; see above for the available bitwise flags.
    For the default behaviour, a value of 0 should be provided.
@param node_map If not NULL, this array will be filled to define the mapping
    between nodes IDs in the table collection before and after simplification.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_simplify(tsk_table_collection_t *self, const tsk_id_t *samples,
    tsk_size_t num_samples, tsk_flags_t options, tsk_id_t *node_map);

/**
@brief Subsets and reorders a table collection according to an array of nodes.

@rst
Reduces the table collection to contain only the entries referring to
the provided list of nodes, with nodes reordered according to the order
they appear in the ``nodes`` argument. Specifically, this subsets and reorders
each of the tables as follows (but see options, below):

1. Nodes: if in the list of nodes, and in the order provided.
2. Individuals: if referred to by a retained node.
3. Populations: if referred to by a retained node, and in the order first seen
   when traversing the list of retained nodes.
4. Edges: if both parent and child are retained nodes.
5. Mutations: if the mutation's node is a retained node.
6. Sites: if any mutations remain at the site after removing mutations.

Retained individuals, edges, mutations, and sites appear in the same
order as in the original tables. Note that only the information *directly*
associated with the provided nodes is retained - for instance,
subsetting to nodes=[A, B] does not retain nodes ancestral to A and B,
and only retains the individuals A and B are in, and not their parents.

This function does *not* require the tables to be sorted.

.. note:: Migrations are currently not supported by subset, and an error will
    be raised if we attempt call subset on a table collection with greater
    than zero migrations.

**Options**:

Options can be specified by providing one or more of the following bitwise
flags:

- :c:macro:`TSK_SUBSET_NO_CHANGE_POPULATIONS`
- :c:macro:`TSK_SUBSET_KEEP_UNREFERENCED`
@endrst

@param self A pointer to a tsk_table_collection_t object.
@param nodes An array of num_nodes valid node IDs.
@param num_nodes The number of node IDs in the input nodes array.
@param options Bitwise option flags.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_subset(tsk_table_collection_t *self, const tsk_id_t *nodes,
    tsk_size_t num_nodes, tsk_flags_t options);

/**
@brief Forms the node-wise union of two table collections.

@rst
Expands this table collection by adding the non-shared portions of another table
collection to itself. The ``other_node_mapping`` encodes which nodes in ``other`` are
equivalent to a node in ``self``. The positions in the ``other_node_mapping`` array
correspond to node ids in ``other``, and the elements encode the equivalent
node id in ``self`` or :c:macro:`TSK_NULL` if the node is exclusive to ``other``. Nodes
that are exclusive ``other`` are added to ``self``, along with:

1. Individuals which are new to ``self``.
2. Edges whose parent or child are new to ``self``.
3. Sites which were not present in ``self``.
4. Mutations whose nodes are new to ``self``.

By default, populations of newly added nodes are assumed to be new populations,
and added to the population table as well.

The behavior can be changed by the flags ``TSK_UNION_ALL_EDGES`` and
``TSK_UNION_ALL_MUTATIONS``, which will (respectively) add *all* edges
or *all* sites and mutations instead.

This operation will also sort the resulting tables, so the tables may change
even if nothing new is added, if the original tables were not sorted.

.. note:: Migrations are currently not supported by union, and an error will
    be raised if we attempt call union on a table collection with migrations.

**Options**:

Options can be specified by providing one or more of the following bitwise
flags:

- :c:macro:`TSK_UNION_NO_CHECK_SHARED`
- :c:macro:`TSK_UNION_NO_ADD_POP`
@endrst

@param self A pointer to a tsk_table_collection_t object.
@param other A pointer to a tsk_table_collection_t object.
@param other_node_mapping An array of node IDs that relate nodes in other to nodes in
self: the k-th element of other_node_mapping should be the index of the equivalent
node in self, or TSK_NULL if the node is not present in self (in which case it
will be added to self).
@param options Union options; see above for the available bitwise flags.
    For the default behaviour, a value of 0 should be provided.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_union(tsk_table_collection_t *self,
    const tsk_table_collection_t *other, const tsk_id_t *other_node_mapping,
    tsk_flags_t options);

/**
@brief Set the time_units
@rst
Copies the time_units string to this table collection, replacing any existing.
@endrst
@param self A pointer to a tsk_table_collection_t object.
@param time_units A pointer to a char array.
@param time_units_length The size of the time units string in bytes.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_set_time_units(
    tsk_table_collection_t *self, const char *time_units, tsk_size_t time_units_length);

/**
@brief Set the metadata
@rst
Copies the metadata string to this table collection, replacing any existing.
@endrst
@param self A pointer to a tsk_table_collection_t object.
@param metadata A pointer to a char array.
@param metadata_length The size of the metadata in bytes.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_set_metadata(
    tsk_table_collection_t *self, const char *metadata, tsk_size_t metadata_length);

/**
@brief Set the metadata schema
@rst
Copies the metadata schema string to this table collection, replacing any existing.
@endrst
@param self A pointer to a tsk_table_collection_t object.
@param metadata_schema A pointer to a char array.
@param metadata_schema_length The size of the metadata schema in bytes.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_set_metadata_schema(tsk_table_collection_t *self,
    const char *metadata_schema, tsk_size_t metadata_schema_length);

/**
@brief Returns true if this table collection is indexed.

@rst
This method returns true if the table collection has an index
for the edge table. It guarantees that the index exists, and that
it is for the same number of edges that are in the edge table. It
does *not* guarantee that the index is valid (i.e., if the rows
in the edge have been permuted in some way since the index was built).

See the :ref:`sec_c_api_table_indexes` section for details on the index
life-cycle.
@endrst

@param self A pointer to a tsk_table_collection_t object.
@param options Bitwise options. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return true if there is an index present for this table collection.
*/
bool tsk_table_collection_has_index(
    const tsk_table_collection_t *self, tsk_flags_t options);

/**
@brief Deletes the indexes for this table collection.

@rst
Unconditionally drop the indexes that may be present for this table collection. It
is not an error to call this method on an unindexed table collection.
See the :ref:`sec_c_api_table_indexes` section for details on the index
life-cycle.
@endrst

@param self A pointer to a tsk_table_collection_t object.
@param options Bitwise options. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Always returns 0.
*/
int tsk_table_collection_drop_index(tsk_table_collection_t *self, tsk_flags_t options);

/**
@brief Builds indexes for this table collection.

@rst
Builds the tree traversal :ref:`indexes <sec_table_indexes>` for this table
collection. Any existing index is first dropped using
:c:func:`tsk_table_collection_drop_index`. See the
:ref:`sec_c_api_table_indexes` section for details on the index life-cycle.
@endrst

@param self A pointer to a tsk_table_collection_t object.
@param options Bitwise options. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_build_index(tsk_table_collection_t *self, tsk_flags_t options);

/**
@brief Runs integrity checks on this table collection.

@rst

Checks the integrity of this table collection. The default checks (i.e., with
options = 0) guarantee the integrity of memory and entity references within the
table collection. All positions along the genome are checked
to see if they are finite values and within the required bounds. Time values
are checked to see if they are finite or marked as unknown.
Consistency of the direction of inheritance is also checked: whether
parents are more recent than children, mutations are not more recent
than their nodes or their mutation parents, etcetera.

To check if a set of tables fulfills the :ref:`requirements
<sec_valid_tree_sequence_requirements>` needed for a valid tree sequence, use
the :c:macro:`TSK_CHECK_TREES` option. When this method is called with
:c:macro:`TSK_CHECK_TREES`, the number of trees in the tree sequence is returned. Thus,
to check for errors client code should verify that the return value is less than zero.
All other options will return zero on success and a negative value on failure.

More fine-grained checks can be achieved using bitwise combinations of the
other options.

**Options**:

Options can be specified by providing one or more of the following bitwise
flags:

- :c:macro:`TSK_CHECK_EDGE_ORDERING`
- :c:macro:`TSK_CHECK_SITE_ORDERING`
- :c:macro:`TSK_CHECK_SITE_DUPLICATES`
- :c:macro:`TSK_CHECK_MUTATION_ORDERING`
- :c:macro:`TSK_CHECK_INDIVIDUAL_ORDERING`
- :c:macro:`TSK_CHECK_MIGRATION_ORDERING`
- :c:macro:`TSK_CHECK_INDEXES`
- :c:macro:`TSK_CHECK_TREES`
- :c:macro:`TSK_NO_CHECK_POPULATION_REFS`
@endrst

@param self A pointer to a tsk_table_collection_t object.
@param options Bitwise options.
@return Return a negative error value on if any problems are detected
   in the tree sequence. If the TSK_CHECK_TREES option is provided,
   the number of trees in the tree sequence will be returned, on
   success.
*/
tsk_id_t tsk_table_collection_check_integrity(
    const tsk_table_collection_t *self, tsk_flags_t options);

/** @} */

/* Undocumented methods */

/* Flags for ibd_segments */
#define TSK_IBD_STORE_PAIRS (1 << 0)
#define TSK_IBD_STORE_SEGMENTS (1 << 1)

/* TODO be systematic about where "result" should be in the params
 * list, different here and in link_ancestors. */
/* FIXME the order of num_samples and samples needs to be reversed in within.
 * This should be done as part of documenting, I guess. */
int tsk_table_collection_ibd_within(const tsk_table_collection_t *self,
    tsk_identity_segments_t *result, const tsk_id_t *samples, tsk_size_t num_samples,
    double min_span, double max_time, tsk_flags_t options);

int tsk_table_collection_ibd_between(const tsk_table_collection_t *self,
    tsk_identity_segments_t *result, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, double min_span,
    double max_time, tsk_flags_t options);

int tsk_table_collection_link_ancestors(tsk_table_collection_t *self, tsk_id_t *samples,
    tsk_size_t num_samples, tsk_id_t *ancestors, tsk_size_t num_ancestors,
    tsk_flags_t options, tsk_edge_table_t *result);
int tsk_table_collection_deduplicate_sites(
    tsk_table_collection_t *tables, tsk_flags_t options);
int tsk_table_collection_compute_mutation_parents(
    tsk_table_collection_t *self, tsk_flags_t options);
int tsk_table_collection_compute_mutation_times(
    tsk_table_collection_t *self, double *random, tsk_flags_t options);
int tsk_table_collection_delete_older(
    tsk_table_collection_t *self, double time, tsk_flags_t options);

int tsk_table_collection_set_indexes(tsk_table_collection_t *self,
    tsk_id_t *edge_insertion_order, tsk_id_t *edge_removal_order);

int tsk_table_collection_takeset_metadata(
    tsk_table_collection_t *self, char *metadata, tsk_size_t metadata_length);
int tsk_table_collection_takeset_indexes(tsk_table_collection_t *self,
    tsk_id_t *edge_insertion_order, tsk_id_t *edge_removal_order);
int tsk_individual_table_takeset_columns(tsk_individual_table_t *self,
    tsk_size_t num_rows, tsk_flags_t *flags, double *location,
    tsk_size_t *location_offset, tsk_id_t *parents, tsk_size_t *parents_offset,
    char *metadata, tsk_size_t *metadata_offset);
int tsk_node_table_takeset_columns(tsk_node_table_t *self, tsk_size_t num_rows,
    tsk_flags_t *flags, double *time, tsk_id_t *population, tsk_id_t *individual,
    char *metadata, tsk_size_t *metadata_offset);
int tsk_edge_table_takeset_columns(tsk_edge_table_t *self, tsk_size_t num_rows,
    double *left, double *right, tsk_id_t *parent, tsk_id_t *child, char *metadata,
    tsk_size_t *metadata_offset);
int tsk_migration_table_takeset_columns(tsk_migration_table_t *self, tsk_size_t num_rows,
    double *left, double *right, tsk_id_t *node, tsk_id_t *source, tsk_id_t *dest,
    double *time, char *metadata, tsk_size_t *metadata_offset);
int tsk_site_table_takeset_columns(tsk_site_table_t *self, tsk_size_t num_rows,
    double *position, char *ancestral_state, tsk_size_t *ancestral_state_offset,
    char *metadata, tsk_size_t *metadata_offset);
int tsk_mutation_table_takeset_columns(tsk_mutation_table_t *self, tsk_size_t num_rows,
    tsk_id_t *site, tsk_id_t *node, tsk_id_t *parent, double *time, char *derived_state,
    tsk_size_t *derived_state_offset, char *metadata, tsk_size_t *metadata_offset);
int tsk_population_table_takeset_columns(tsk_population_table_t *self,
    tsk_size_t num_rows, char *metadata, tsk_size_t *metadata_offset);
int tsk_provenance_table_takeset_columns(tsk_provenance_table_t *self,
    tsk_size_t num_rows, char *timestamp, tsk_size_t *timestamp_offset, char *record,
    tsk_size_t *record_offset);

bool tsk_table_collection_has_reference_sequence(const tsk_table_collection_t *self);

int tsk_reference_sequence_init(tsk_reference_sequence_t *self, tsk_flags_t options);
int tsk_reference_sequence_free(tsk_reference_sequence_t *self);
bool tsk_reference_sequence_is_null(const tsk_reference_sequence_t *self);
bool tsk_reference_sequence_equals(const tsk_reference_sequence_t *self,
    const tsk_reference_sequence_t *other, tsk_flags_t options);
int tsk_reference_sequence_copy(const tsk_reference_sequence_t *self,
    tsk_reference_sequence_t *dest, tsk_flags_t options);
int tsk_reference_sequence_set_data(
    tsk_reference_sequence_t *self, const char *data, tsk_size_t data_length);
int tsk_reference_sequence_set_url(
    tsk_reference_sequence_t *self, const char *url, tsk_size_t url_length);
int tsk_reference_sequence_set_metadata(
    tsk_reference_sequence_t *self, const char *metadata, tsk_size_t metadata_length);
int tsk_reference_sequence_set_metadata_schema(tsk_reference_sequence_t *self,
    const char *metadata_schema, tsk_size_t metadata_schema_length);
int tsk_reference_sequence_takeset_data(
    tsk_reference_sequence_t *self, char *data, tsk_size_t data_length);
int tsk_reference_sequence_takeset_metadata(
    tsk_reference_sequence_t *self, char *metadata, tsk_size_t metadata_length);

/**
@defgroup TABLE_SORTER_API_GROUP Low-level table sorter API.
@{
*/

/* NOTE: We use the "struct _tsk_table_sorter_t" form here
 * rather then the usual tsk_table_sorter_t alias because
 * of problems with Doxygen. This was the only way I could
 * get it to work - ideally, we'd use the usual typedefs
 * to avoid confusing people.
 */

/**
@brief Initialises the memory for the sorter object.

@rst
This must be called before any operations are performed on the
table sorter and initialises all fields. The ``edge_sort`` function
is set to the default method using qsort. The ``user_data``
field is set to NULL.
This method supports the same options as
:c:func:`tsk_table_collection_sort`.

@endrst

@param self A pointer to an uninitialised tsk_table_sorter_t object.
@param tables The table collection to sort.
@param options Sorting options.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_sorter_init(struct _tsk_table_sorter_t *self,
    tsk_table_collection_t *tables, tsk_flags_t options);

/**
@brief Runs the sort using the configured functions.

@rst
Runs the sorting process:

1. Drop the table indexes.
2. If the ``sort_edges`` function pointer is not NULL, run it. The
   first parameter to the called function will be a pointer to this
   table_sorter_t object. The second parameter will be the value
   ``start.edges``. This specifies the offset at which sorting should
   start in the edge table. This offset is guaranteed to be within the
   bounds of the edge table.
3. Sort the site table, building the mapping between site IDs in the
   current and sorted tables.
4. Sort the mutation table, using the ``sort_mutations`` pointer.

If an error occurs during the execution of a user-supplied
sorting function a non-zero value must be returned. This value
will then be returned by ``tsk_table_sorter_run``. The error
return value should be chosen to avoid conflicts with tskit error
codes.

See :c:func:`tsk_table_collection_sort` for details on the ``start`` parameter.

@endrst

@param self A pointer to a tsk_table_sorter_t object.
@param start The position in the tables at which sorting starts.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_sorter_run(struct _tsk_table_sorter_t *self, const tsk_bookmark_t *start);

/**
@brief Free the internal memory for the specified table sorter.

@param self A pointer to an initialised tsk_table_sorter_t object.
@return Always returns 0.
*/
int tsk_table_sorter_free(struct _tsk_table_sorter_t *self);

/** @} */

int tsk_squash_edges(
    tsk_edge_t *edges, tsk_size_t num_edges, tsk_size_t *num_output_edges);

/* IBD segments API. This is experimental and the interface may change. */

tsk_size_t tsk_identity_segments_get_num_segments(const tsk_identity_segments_t *self);
double tsk_identity_segments_get_total_span(const tsk_identity_segments_t *self);
tsk_size_t tsk_identity_segments_get_num_pairs(const tsk_identity_segments_t *self);
int tsk_identity_segments_get_keys(
    const tsk_identity_segments_t *result, tsk_id_t *pairs);
int tsk_identity_segments_get_items(const tsk_identity_segments_t *self, tsk_id_t *pairs,
    tsk_identity_segment_list_t **lists);
int tsk_identity_segments_get(const tsk_identity_segments_t *self, tsk_id_t a,
    tsk_id_t b, tsk_identity_segment_list_t **ret_list);
void tsk_identity_segments_print_state(tsk_identity_segments_t *self, FILE *out);
int tsk_identity_segments_free(tsk_identity_segments_t *self);

#ifdef __cplusplus
}
#endif
#endif
