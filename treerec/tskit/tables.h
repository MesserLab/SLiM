/*
 * MIT License
 *
 * Copyright (c) 2019 Tskit Developers
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

/**
@brief Tskit Object IDs.

@rst
All objects in tskit are referred to by integer IDs corresponding to the
row they occupy in the relevant table. The ``tsk_id_t`` type should be used
when manipulating these ID values. The reserved value ``TSK_NULL`` (-1) defines
missing data.
@endrst
*/
typedef int32_t tsk_id_t;

/**
@brief Tskit sizes.

@rst
Sizes in tskit are defined by the ``tsk_size_t`` type.
@endrst
*/
typedef uint32_t tsk_size_t;

/**
@brief Container for bitwise flags.

@rst
Bitwise flags are used in tskit as a column type and also as a way to
specify options to API functions.
@endrst
*/
typedef uint32_t tsk_flags_t;

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
    double *location;
    /** @brief Number of spatial dimensions. */
    tsk_size_t location_length;
    /** @brief Metadata. */
    const char *metadata;
    /** @brief Size of the metadata in bytes. */
    tsk_size_t metadata_length;
    tsk_id_t *nodes;
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
    tsk_mutation_t *mutations;
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

/**
@brief A collection of tables defining the data for a tree sequence.
*/
typedef struct {
    /** @brief The sequence length defining the tree sequence's coordinate space */
    double sequence_length;
    char *file_uuid;
    /** @brief The tree-sequence metadata */
    char *metadata;
    tsk_size_t metadata_length;
    /** @brief The metadata schema */
    char *metadata_schema;
    tsk_size_t metadata_schema_length;
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
    /** @brief An opaque pointer for use by client code */
    void *user_data;
    /** @brief Mapping from input site IDs to output site IDs */
    tsk_id_t *site_id_map;
} tsk_table_sorter_t;

/****************************************************************************/
/* Common function options */
/****************************************************************************/

/**
@defgroup TABLES_API_FUNCTION_OPTIONS Common function options in tables API
@{
*/

/* Start the commmon options at the top of the space; this way we can start
 * options for individual functions at the bottom without worrying about
 * clashing with the common options */

/** @brief Turn on debugging output. Not supported by all functions. */
#define TSK_DEBUG (1u << 31)

/** @brief Do not initialise the parameter object. */
#define TSK_NO_INIT (1u << 30)

/** @brief Do not run integrity checks before performing an operation. */
#define TSK_NO_CHECK_INTEGRITY (1u << 29)

/**@} */

/* Flags for simplify() */
#define TSK_FILTER_SITES (1 << 0)
#define TSK_FILTER_POPULATIONS (1 << 1)
#define TSK_FILTER_INDIVIDUALS (1 << 2)
#define TSK_REDUCE_TO_SITE_TOPOLOGY (1 << 3)
#define TSK_KEEP_UNARY (1 << 4)
#define TSK_KEEP_INPUT_ROOTS (1 << 5)

/* Flags for check_integrity */
#define TSK_CHECK_EDGE_ORDERING (1 << 0)
#define TSK_CHECK_SITE_ORDERING (1 << 1)
#define TSK_CHECK_SITE_DUPLICATES (1 << 2)
#define TSK_CHECK_MUTATION_ORDERING (1 << 3)
#define TSK_CHECK_INDEXES (1 << 4)
#define TSK_CHECK_TREES (1 << 5)

/* Leave room for more positive check flags */
#define TSK_NO_CHECK_POPULATION_REFS (1 << 10)

/* Flags for dump tables */
#define TSK_NO_BUILD_INDEXES (1 << 0)

/* Flags for load tables */
#define TSK_BUILD_INDEXES (1 << 0)

/* Flags for table collection init */
#define TSK_NO_EDGE_METADATA (1 << 0)

/* Flags for table init. */
#define TSK_NO_METADATA (1 << 0)

/* Flags for union() */
#define TSK_UNION_NO_CHECK_SHARED (1 << 0)
#define TSK_UNION_NO_ADD_POP (1 << 1)

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
Add a new individual with the specified ``flags``, ``location`` and ``metadata``
to the table. Copies of the ``location`` and ``metadata`` parameters are taken
immediately.
See the :ref:`table definition <sec_individual_table_definition>` for details
of the columns in this table.
@endrst

@param self A pointer to a tsk_individual_table_t object.
@param flags The bitwise flags for the new individual.
@param location A pointer to a double array representing the spatial location
    of the new individual. Can be ``NULL`` if ``location_length`` is 0.
@param location_length The number of dimensions in the locations position.
    Note this the number of elements in the corresponding double array
    not the number of bytes.
@param metadata The metadata to be associated with the new individual. This
    is a pointer to arbitrary memory. Can be ``NULL`` if ``metadata_length`` is 0.
@param metadata_length The size of the metadata array in bytes.
@return Return the ID of the newly added individual on success,
    or a negative value on failure.
*/
tsk_id_t tsk_individual_table_add_row(tsk_individual_table_t *self, tsk_flags_t flags,
    double *location, tsk_size_t location_length, const char *metadata,
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
@brief Returns true if the data in the specified table is identical to the data
       in this table.

@param self A pointer to a tsk_individual_table_t object.
@param other A pointer to a tsk_individual_table_t object.
@return Return true if the specified table is equal to this table.
*/
bool tsk_individual_table_equals(
    tsk_individual_table_t *self, tsk_individual_table_t *other);

/**
@brief Copies the state of this table into the specified destination.

@rst
By default the method initialises the specified destination table. If the
destination is already initialised, the :c:macro:`TSK_NO_INIT` option should
be supplied to avoid leaking memory.

Indexes that are present are also copied to the destination table.
@endrst

@param self A pointer to a tsk_individual_table_t object.
@param dest A pointer to a tsk_individual_table_t object. If the TSK_NO_INIT option
    is specified, this must be an initialised individual table. If not, it must
    be an uninitialised individual table.
@param options Bitwise option flags.
@return Return 0 on success or a negative value on failure.
*/
int tsk_individual_table_copy(
    tsk_individual_table_t *self, tsk_individual_table_t *dest, tsk_flags_t options);

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
    tsk_individual_table_t *self, tsk_id_t index, tsk_individual_t *row);

/**
@brief Set the metadata schema

@rst
Copies the metadata schema string to this table, replacing any existing.
@endrst

@param self A pointer to a tsk_individual_table_t object.
@param metadata_schema A pointer to a char array
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
void tsk_individual_table_print_state(tsk_individual_table_t *self, FILE *out);

/** @} */

/* Undocumented methods */

int tsk_individual_table_set_columns(tsk_individual_table_t *self, tsk_size_t num_rows,
    tsk_flags_t *flags, double *location, tsk_size_t *location_length,
    const char *metadata, tsk_size_t *metadata_length);
int tsk_individual_table_append_columns(tsk_individual_table_t *self,
    tsk_size_t num_rows, tsk_flags_t *flags, double *location,
    tsk_size_t *location_length, const char *metadata, tsk_size_t *metadata_length);
int tsk_individual_table_dump_text(tsk_individual_table_t *self, FILE *out);
int tsk_individual_table_set_max_rows_increment(
    tsk_individual_table_t *self, tsk_size_t max_rows_increment);
int tsk_individual_table_set_max_metadata_length_increment(
    tsk_individual_table_t *self, tsk_size_t max_metadata_length_increment);
int tsk_individual_table_set_max_location_length_increment(
    tsk_individual_table_t *self, tsk_size_t max_location_length_increment);

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
@param population The population for the new node. Set to TSK_NULL if not known.
@param individual The individual for the new node. Set to TSK_NULL if not known.
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
@brief Returns true if the data in the specified table is identical to the data
       in this table.

@param self A pointer to a tsk_node_table_t object.
@param other A pointer to a tsk_node_table_t object.
@return Return true if the specified table is equal to this table.
*/
bool tsk_node_table_equals(tsk_node_table_t *self, tsk_node_table_t *other);

/**
@brief Copies the state of this table into the specified destination.

@rst
By default the method initialises the specified destination table. If the
destination is already initialised, the :c:macro:`TSK_NO_INIT` option should
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
    tsk_node_table_t *self, tsk_node_table_t *dest, tsk_flags_t options);

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
int tsk_node_table_get_row(tsk_node_table_t *self, tsk_id_t index, tsk_node_t *row);

/**
@brief Set the metadata schema
@rst
Copies the metadata schema string to this table, replacing any existing.
@endrst
@param self A pointer to a tsk_node_table_t object.
@param metadata_schema A pointer to a char array
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
void tsk_node_table_print_state(tsk_node_table_t *self, FILE *out);

/** @} */

/* Undocumented methods */

int tsk_node_table_set_max_rows_increment(
    tsk_node_table_t *self, tsk_size_t max_rows_increment);
int tsk_node_table_set_max_metadata_length_increment(
    tsk_node_table_t *self, tsk_size_t max_metadata_length_increment);
int tsk_node_table_set_columns(tsk_node_table_t *self, tsk_size_t num_rows,
    tsk_flags_t *flags, double *time, tsk_id_t *population, tsk_id_t *individual,
    const char *metadata, tsk_size_t *metadata_length);
int tsk_node_table_append_columns(tsk_node_table_t *self, tsk_size_t num_rows,
    tsk_flags_t *flags, double *time, tsk_id_t *population, tsk_id_t *individual,
    const char *metadata, tsk_size_t *metadata_length);
int tsk_node_table_dump_text(tsk_node_table_t *self, FILE *out);

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

TSK_NO_METADATA
    Do not allocate space to store metadata in this table. Operations
    attempting to add non-empty metadata to the table will fail
    with error TSK_ERR_METADATA_DISABLED.

@endrst

@param self A pointer to an uninitialised tsk_edge_table_t object.
@param options Allocation time options. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
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
@brief Returns true if the data in the specified table is identical to the data
       in this table.

@param self A pointer to a tsk_edge_table_t object.
@param other A pointer to a tsk_edge_table_t object.
@return Return true if the specified table is equal to this table.
*/
bool tsk_edge_table_equals(tsk_edge_table_t *self, tsk_edge_table_t *other);

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
    tsk_edge_table_t *self, tsk_edge_table_t *dest, tsk_flags_t options);

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
int tsk_edge_table_get_row(tsk_edge_table_t *self, tsk_id_t index, tsk_edge_t *row);

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
void tsk_edge_table_print_state(tsk_edge_table_t *self, FILE *out);

/** @} */

/* Undocumented methods */

int tsk_edge_table_set_max_rows_increment(
    tsk_edge_table_t *self, tsk_size_t max_rows_increment);
int tsk_edge_table_set_max_metadata_length_increment(
    tsk_edge_table_t *self, tsk_size_t max_metadata_length_increment);
int tsk_edge_table_set_columns(tsk_edge_table_t *self, tsk_size_t num_rows, double *left,
    double *right, tsk_id_t *parent, tsk_id_t *child, const char *metadata,
    tsk_size_t *metadata_length);
int tsk_edge_table_append_columns(tsk_edge_table_t *self, tsk_size_t num_rows,
    double *left, double *right, tsk_id_t *parent, tsk_id_t *child, const char *metadata,
    tsk_size_t *metadata_length);
int tsk_edge_table_dump_text(tsk_edge_table_t *self, FILE *out);

int tsk_edge_table_squash(tsk_edge_table_t *self);

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
@brief Returns true if the data in the specified table is identical to the data
       in this table.

@param self A pointer to a tsk_migration_table_t object.
@param other A pointer to a tsk_migration_table_t object.
@return Return true if the specified table is equal to this table.
*/
bool tsk_migration_table_equals(
    tsk_migration_table_t *self, tsk_migration_table_t *other);

/**
@brief Copies the state of this table into the specified destination.

@rst
By default the method initialises the specified destination table. If the
destination is already initialised, the :c:macro:`TSK_NO_INIT` option should
be supplied to avoid leaking memory.
@endrst

@param self A pointer to a tsk_migration_table_t object.
@param dest A pointer to a tsk_migration_table_t object. If the TSK_NO_INIT option
    is specified, this must be an initialised migration table. If not, it must
    be an uninitialised migration table.
@param options Bitwise option flags.
@return Return 0 on success or a negative value on failure.
*/
int tsk_migration_table_copy(
    tsk_migration_table_t *self, tsk_migration_table_t *dest, tsk_flags_t options);

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
    tsk_migration_table_t *self, tsk_id_t index, tsk_migration_t *row);

/**
@brief Set the metadata schema
@rst
Copies the metadata schema string to this table, replacing any existing.
@endrst
@param self A pointer to a tsk_migration_table_t object.
@param metadata_schema A pointer to a char array
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
void tsk_migration_table_print_state(tsk_migration_table_t *self, FILE *out);

/** @} */

/* Undocumented methods */

int tsk_migration_table_init(tsk_migration_table_t *self, tsk_flags_t options);
int tsk_migration_table_set_max_rows_increment(
    tsk_migration_table_t *self, tsk_size_t max_rows_increment);
int tsk_migration_table_set_max_metadata_length_increment(
    tsk_migration_table_t *self, tsk_size_t max_metadata_length_increment);
int tsk_migration_table_set_columns(tsk_migration_table_t *self, tsk_size_t num_rows,
    double *left, double *right, tsk_id_t *node, tsk_id_t *source, tsk_id_t *dest,
    double *time, const char *metadata, tsk_size_t *metadata_length);
int tsk_migration_table_append_columns(tsk_migration_table_t *self, tsk_size_t num_rows,
    double *left, double *right, tsk_id_t *node, tsk_id_t *source, tsk_id_t *dest,
    double *time, const char *metadata, tsk_size_t *metadata_length);
int tsk_migration_table_dump_text(tsk_migration_table_t *self, FILE *out);

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
@brief Returns true if the data in the specified table is identical to the data
       in this table.

@param self A pointer to a tsk_site_table_t object.
@param other A pointer to a tsk_site_table_t object.
@return Return true if the specified table is equal to this table.
*/
bool tsk_site_table_equals(tsk_site_table_t *self, tsk_site_table_t *other);

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
    tsk_site_table_t *self, tsk_site_table_t *dest, tsk_flags_t options);

/**
@brief Get the row at the specified index.

@rst
Updates the specified site struct to reflect the values in the specified row.
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
int tsk_site_table_get_row(tsk_site_table_t *self, tsk_id_t index, tsk_site_t *row);

/**
@brief Set the metadata schema
@rst
Copies the metadata schema string to this table, replacing any existing.
@endrst
@param self A pointer to a tsk_site_table_t object.
@param metadata_schema A pointer to a char array
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
void tsk_site_table_print_state(tsk_site_table_t *self, FILE *out);

/** @} */

/* Undocumented methods */

int tsk_site_table_set_max_rows_increment(
    tsk_site_table_t *self, tsk_size_t max_rows_increment);
int tsk_site_table_set_max_metadata_length_increment(
    tsk_site_table_t *self, tsk_size_t max_metadata_length_increment);
int tsk_site_table_set_max_ancestral_state_length_increment(
    tsk_site_table_t *self, tsk_size_t max_ancestral_state_length_increment);
int tsk_site_table_set_columns(tsk_site_table_t *self, tsk_size_t num_rows,
    double *position, const char *ancestral_state, tsk_size_t *ancestral_state_length,
    const char *metadata, tsk_size_t *metadata_length);
int tsk_site_table_append_columns(tsk_site_table_t *self, tsk_size_t num_rows,
    double *position, const char *ancestral_state, tsk_size_t *ancestral_state_length,
    const char *metadata, tsk_size_t *metadata_length);
int tsk_site_table_dump_text(tsk_site_table_t *self, FILE *out);

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
@brief Returns true if the data in the specified table is identical to the data
       in this table.

@param self A pointer to a tsk_mutation_table_t object.
@param other A pointer to a tsk_mutation_table_t object.
@return Return true if the specified table is equal to this table.
*/
bool tsk_mutation_table_equals(tsk_mutation_table_t *self, tsk_mutation_table_t *other);

/**
@brief Copies the state of this table into the specified destination.

@rst
By default the method initialises the specified destination table. If the
destination is already initialised, the :c:macro:`TSK_NO_INIT` option should
be supplied to avoid leaking memory.
@endrst

@param self A pointer to a tsk_mutation_table_t object.
@param dest A pointer to a tsk_mutation_table_t object. If the TSK_NO_INIT option
    is specified, this must be an initialised mutation table. If not, it must
    be an uninitialised mutation table.
@param options Bitwise option flags.
@return Return 0 on success or a negative value on failure.
*/
int tsk_mutation_table_copy(
    tsk_mutation_table_t *self, tsk_mutation_table_t *dest, tsk_flags_t options);

/**
@brief Get the row at the specified index.

@rst
Updates the specified mutation struct to reflect the values in the specified row.
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
    tsk_mutation_table_t *self, tsk_id_t index, tsk_mutation_t *row);

/**
@brief Set the metadata schema
@rst
Copies the metadata schema string to this table, replacing any existing.
@endrst
@param self A pointer to a tsk_mutation_table_t object.
@param metadata_schema A pointer to a char array
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
void tsk_mutation_table_print_state(tsk_mutation_table_t *self, FILE *out);

/** @} */

/* Undocumented methods */

int tsk_mutation_table_set_max_rows_increment(
    tsk_mutation_table_t *self, tsk_size_t max_rows_increment);
int tsk_mutation_table_set_max_metadata_length_increment(
    tsk_mutation_table_t *self, tsk_size_t max_metadata_length_increment);
int tsk_mutation_table_set_max_derived_state_length_increment(
    tsk_mutation_table_t *self, tsk_size_t max_derived_state_length_increment);
int tsk_mutation_table_set_columns(tsk_mutation_table_t *self, tsk_size_t num_rows,
    tsk_id_t *site, tsk_id_t *node, tsk_id_t *parent, double *time,
    const char *derived_state, tsk_size_t *derived_state_length, const char *metadata,
    tsk_size_t *metadata_length);
int tsk_mutation_table_append_columns(tsk_mutation_table_t *self, tsk_size_t num_rows,
    tsk_id_t *site, tsk_id_t *node, tsk_id_t *parent, double *time,
    const char *derived_state, tsk_size_t *derived_state_length, const char *metadata,
    tsk_size_t *metadata_length);
bool tsk_mutation_table_equals(tsk_mutation_table_t *self, tsk_mutation_table_t *other);
int tsk_mutation_table_clear(tsk_mutation_table_t *self);
int tsk_mutation_table_truncate(tsk_mutation_table_t *self, tsk_size_t num_rows);
int tsk_mutation_table_copy(
    tsk_mutation_table_t *self, tsk_mutation_table_t *dest, tsk_flags_t options);
int tsk_mutation_table_free(tsk_mutation_table_t *self);
int tsk_mutation_table_dump_text(tsk_mutation_table_t *self, FILE *out);
void tsk_mutation_table_print_state(tsk_mutation_table_t *self, FILE *out);
int tsk_mutation_table_get_row(
    tsk_mutation_table_t *self, tsk_id_t index, tsk_mutation_t *row);

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
@brief Returns true if the data in the specified table is identical to the data
       in this table.

@param self A pointer to a tsk_population_table_t object.
@param other A pointer to a tsk_population_table_t object.
@return Return true if the specified table is equal to this table.
*/
bool tsk_population_table_equals(
    tsk_population_table_t *self, tsk_population_table_t *other);

/**
@brief Copies the state of this table into the specified destination.

@rst
By default the method initialises the specified destination table. If the
destination is already initialised, the :c:macro:`TSK_NO_INIT` option should
be supplied to avoid leaking memory.
@endrst

@param self A pointer to a tsk_population_table_t object.
@param dest A pointer to a tsk_population_table_t object. If the TSK_NO_INIT option
    is specified, this must be an initialised population table. If not, it must
    be an uninitialised population table.
@param options Bitwise option flags.
@return Return 0 on success or a negative value on failure.
*/
int tsk_population_table_copy(
    tsk_population_table_t *self, tsk_population_table_t *dest, tsk_flags_t options);

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
    tsk_population_table_t *self, tsk_id_t index, tsk_population_t *row);

/**
@brief Set the metadata schema
@rst
Copies the metadata schema string to this table, replacing any existing.
@endrst
@param self A pointer to a tsk_population_table_t object.
@param metadata_schema A pointer to a char array
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
void tsk_population_table_print_state(tsk_population_table_t *self, FILE *out);

/** @} */

/* Undocumented methods */

int tsk_population_table_set_max_rows_increment(
    tsk_population_table_t *self, tsk_size_t max_rows_increment);
int tsk_population_table_set_max_metadata_length_increment(
    tsk_population_table_t *self, tsk_size_t max_metadata_length_increment);
int tsk_population_table_set_columns(tsk_population_table_t *self, tsk_size_t num_rows,
    const char *metadata, tsk_size_t *metadata_offset);
int tsk_population_table_append_columns(tsk_population_table_t *self,
    tsk_size_t num_rows, const char *metadata, tsk_size_t *metadata_offset);
int tsk_population_table_dump_text(tsk_population_table_t *self, FILE *out);

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
@brief Returns true if the data in the specified table is identical to the data
       in this table.

@param self A pointer to a tsk_provenance_table_t object.
@param other A pointer to a tsk_provenance_table_t object.
@return Return true if the specified table is equal to this table.
*/
bool tsk_provenance_table_equals(
    tsk_provenance_table_t *self, tsk_provenance_table_t *other);

/**
@brief Copies the state of this table into the specified destination.

@rst
By default the method initialises the specified destination table. If the
destination is already initialised, the :c:macro:`TSK_NO_INIT` option should
be supplied to avoid leaking memory.
@endrst

@param self A pointer to a tsk_provenance_table_t object.
@param dest A pointer to a tsk_provenance_table_t object. If the TSK_NO_INIT option
    is specified, this must be an initialised provenance table. If not, it must
    be an uninitialised provenance table.
@param options Bitwise option flags.
@return Return 0 on success or a negative value on failure.
*/
int tsk_provenance_table_copy(
    tsk_provenance_table_t *self, tsk_provenance_table_t *dest, tsk_flags_t options);

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
    tsk_provenance_table_t *self, tsk_id_t index, tsk_provenance_t *row);

/**
@brief Print out the state of this table to the specified stream.

This method is intended for debugging purposes and should not be used
in production code. The format of the output should **not** be depended
on and may change arbitrarily between versions.

@param self A pointer to a tsk_provenance_table_t object.
@param out The stream to write the summary to.
*/
void tsk_provenance_table_print_state(tsk_provenance_table_t *self, FILE *out);

/** @} */

/* Undocumented methods */

int tsk_provenance_table_set_max_rows_increment(
    tsk_provenance_table_t *self, tsk_size_t max_rows_increment);
int tsk_provenance_table_set_max_timestamp_length_increment(
    tsk_provenance_table_t *self, tsk_size_t max_timestamp_length_increment);
int tsk_provenance_table_set_max_record_length_increment(
    tsk_provenance_table_t *self, tsk_size_t max_record_length_increment);
int tsk_provenance_table_set_columns(tsk_provenance_table_t *self, tsk_size_t num_rows,
    char *timestamp, tsk_size_t *timestamp_offset, char *record,
    tsk_size_t *record_offset);
int tsk_provenance_table_append_columns(tsk_provenance_table_t *self,
    tsk_size_t num_rows, char *timestamp, tsk_size_t *timestamp_offset, char *record,
    tsk_size_t *record_offset);
int tsk_provenance_table_dump_text(tsk_provenance_table_t *self, FILE *out);
void tsk_provenance_table_print_state(tsk_provenance_table_t *self, FILE *out);
bool tsk_provenance_table_equals(
    tsk_provenance_table_t *self, tsk_provenance_table_t *other);
int tsk_provenance_table_get_row(
    tsk_provenance_table_t *self, tsk_id_t index, tsk_provenance_t *row);

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

Options can be specified by providing one or more of the following bitwise
flags:

TSK_NO_EDGE_METADATA
    Do not allocate space to store metadata in the edge table. Operations
    attempting to add non-empty metadata to the edge table will fail
    with error TSK_ERR_METADATA_DISABLED.

@endrst

@param self A pointer to an uninitialised tsk_table_collection_t object.
@param options Allocation time options. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
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
@brief Clears all tables in this table collection.

@rst
No memory is freed as a result of this operation; please use
:c:func:`tsk_table_collection_free` to free internal resources.
@endrst

@param self A pointer to a tsk_table_collection_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_clear(tsk_table_collection_t *self);

/**
@brief Returns true if the data in the specified table collection is identical to the
data in this table.

@rst
Returns true if the data in all of the table columns are byte-by-byte equal
and the sequence lengths of the two table collections are equal. Indexes are not
considered when determining equality, since they are derived from the basic data.
@endrst

@param self A pointer to a tsk_table_collection_t object.
@param other A pointer to a tsk_table_collection_t object.
@return Return true if the specified table collection is equal to this table.
*/
bool tsk_table_collection_equals(
    tsk_table_collection_t *self, tsk_table_collection_t *other);

/**
@brief Copies the state of this table collection into the specified destination.

@rst
By default the method initialises the specified destination table. If the
destination is already initialised, the :c:macro:`TSK_NO_INIT` option should
be supplied to avoid leaking memory.
@endrst

@param self A pointer to a tsk_table_collection_t object.
@param dest A pointer to a tsk_table_collection_t object. If the TSK_NO_INIT option
    is specified, this must be an initialised provenance table. If not, it must
    be an uninitialised provenance table.
@param options Bitwise option flags.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_copy(
    tsk_table_collection_t *self, tsk_table_collection_t *dest, tsk_flags_t options);

/**
@brief Print out the state of this table collection to the specified stream.

This method is intended for debugging purposes and should not be used
in production code. The format of the output should **not** be depended
on and may change arbitrarily between versions.

@param self A pointer to a tsk_table_collection_t object.
@param out The stream to write the summary to.
*/
void tsk_table_collection_print_state(tsk_table_collection_t *self, FILE *out);

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

**Options**

Options can be specified by providing one or more of the following bitwise
flags:

TSK_NO_INIT
    Do not initialise this :c:type:`tsk_table_collection_t` before loading.

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

If the stream contains multiple table collection definitions, this function
will load the next table collection from the stream. If the stream contains no
more table collection definitions the error value :c:macro:`TSK_ERR_EOF` will
be returned. Note that EOF is only returned in the case where zero bytes are
read from the stream --- malformed files or other errors will result in
different error conditions. Please see the
:ref:`sec_c_api_examples_file_streaming` section for an example of how to
sequentially load tree sequences from a stream.

**Options**

Options can be specified by providing one or more of the following bitwise
flags:

TSK_NO_INIT
    Do not initialise this :c:type:`tsk_table_collection_t` before loading.

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
Usually we expect that data written to a file will be in a form that
can be read directly and used to create a tree sequence; that is, we
assume that by default the tables are :ref:`sorted <sec_table_sorting>`
and :ref:`indexed <sec_table_indexes>`. Following these
assumptions, if the tables are not already indexed, we index the tables
before writing to file to save the cost of building these indexes at
load time. This behaviour requires that the tables are sorted.
If this automatic indexing is not desired, it can be disabled using
the `TSK_NO_BUILD_INDEXES` option.

If an error occurs the file path is deleted, ensuring that only complete
and well formed files will be written.

**Options**

Options can be specified by providing one or more of the following bitwise
flags:

TSK_NO_BUILD_INDEXES
    Do not build indexes for this table before writing to file. This is useful
    if you wish to write unsorted tables to file, as building the indexes
    will raise an error if the table is unsorted.

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
@param options Bitwise options. See above for details.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_dump(
    tsk_table_collection_t *self, const char *filename, tsk_flags_t options);

/**
@brief Write a table collection to a stream.

@rst
Writes the data from this table collection to the specified FILE stream.
Semantics are identical to :c:func:`tsk_table_collection_dump`.

Please see the :ref:`sec_c_api_examples_file_streaming` section for an example
of how to sequentially dump and load tree sequences from a stream.

**Options**

Options can be specified by providing one or more of the following bitwise
flags:

TSK_NO_BUILD_INDEXES
    Do not build indexes for this table before writing to file. This is useful
    if you wish to write unsorted tables to file, as building the indexes
    will raise an error if the table is unsorted.

@endrst

@param self A pointer to an initialised tsk_table_collection_t object.
@param file A FILE stream opened in an appropriate mode for writing (e.g.
    "w", "a", "r+" or "w+").
@param options Bitwise options. See above for details.
@return Return 0 on success or a negative value on failure.
*/

int tsk_table_collection_dumpf(
    tsk_table_collection_t *self, FILE *file, tsk_flags_t options);

/**
@brief Record the number of rows in each table in the specified tsk_bookmark_t object.

@param self A pointer to an initialised tsk_table_collection_t object.
@param bookmark A pointer to a tsk_bookmark_t which is updated to contain the number of
    rows in all tables.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_record_num_rows(
    tsk_table_collection_t *self, tsk_bookmark_t *bookmark);

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
This method sorts the ``edge``, ``site`` and ``mutation`` tables such that
these requirements are guaranteed to be fulfilled. The ``individual``,
``node``, ``population`` and ``provenance`` tables do not have any sortedness
requirements, and are therefore ignored by this method.

.. note:: The current implementation **may** sort in such a way that exceeds
    these requirements, but this behaviour should not be relied upon and later
    versions may weaken the level of sortedness. However, the method does **guarantee**
    that the resulting tables describes a valid tree sequence.

.. warning:: Sorting migrations is currently not supported and an error will be raised
    if a table collection containing a non-empty migration table is specified.

The specified :c:type:`tsk_bookmark_t` allows us to specify a start position
for sorting in each of the tables; rows before this value are assumed to already be
in sorted order and this information is used to make sorting more efficient.
Positions in tables that are not sorted (``individual``, ``node``, ``population``
and ``provenance``) are ignored and can be set to arbitrary values.

.. warning:: The current implementation only supports specifying a start
    position for the ``edge`` table and in a limited form for the
    ``site`` and ``mutation`` tables. Specifying a non-zero ``migration``,
    start position results in an error. The start positions for the
    ``site`` and ``mutation`` tables can either be 0 or the length of the
    respective tables, allowing these tables to either be fully sorted, or
    not sorted at all.

The table collection will always be unindexed after sort successfully completes.

See the :ref:`table sorting <sec_table_sorting>` section for more details.
For more control over the sorting process, see the
:ref:`sec_c_api_low_level_sorting` section.

**Options**

Options can be specified by providing one or more of the following bitwise
flags:

TSK_NO_CHECK_INTEGRITY
    Do not run integrity checks using
    :c:func:`tsk_table_collection_check_integrity` before sorting,
    potentially leading to a small reduction in execution time. This
    performance optimisation should not be used unless the calling code can
    guarantee reference integrity within the table collection. References
    to rows not in the table or bad offsets will result in undefined
    behaviour.

@endrst

@param self A pointer to a tsk_individual_table_t object.
@param start The position to begin sorting in each table; all rows less than this
    position must fulfill the tree sequence sortedness requirements. If this is
    NULL, sort all rows.
@param options Sort options. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_sort(
    tsk_table_collection_t *self, tsk_bookmark_t *start, tsk_flags_t options);

/**
@brief Simplify the tables to remove redundant information.

@rst
Simplification transforms the tables to remove redundancy and canonicalise
tree sequence data. See the :ref:`simplification <sec_table_simplification>` section for
more details.

A mapping from the node IDs in the table before simplification to their equivalent
values after simplification can be obtained via the ``node_map`` argument. If this
is non NULL, ``node_map[u]`` will contain the new ID for node ``u`` after simplification,
or ``TSK_NULL`` if the node has been removed. Thus, ``node_map`` must be an array of
at least ``self->nodes.num_rows`` :c:type:`tsk_id_t` values.

**Options**:

Options can be specified by providing one or more of the following bitwise
flags:

TSK_FILTER_SITES
    Remove sites from the output if there are no mutations that reference them.
TSK_FILTER_POPULATIONS
    Remove populations from the output if there are no nodes or migrations that
    reference them.
TSK_FILTER_INDIVIDUALS
    Remove individuals from the output if there are no nodes that reference them.
TSK_REDUCE_TO_SITE_TOPOLOGY
    Reduce the topological information in the tables to the minimum necessary to
    represent the trees that contain sites. If there are zero sites this will
    result in an zero output edges. When the number of sites is greater than zero,
    every tree in the output tree sequence will contain at least one site.
    For a given site, the topology of the tree containing that site will be
    identical (up to node ID remapping) to the topology of the corresponding tree
    in the input.
TSK_KEEP_UNARY
    By default simplify removes unary nodes (i.e., nodes with exactly one child)
    along the path from samples to root. If this option is specified such unary
    nodes will be preserved in the output.
TSK_KEEP_INPUT_ROOTS
    By default simplify removes all topology ancestral the MRCAs of the samples.
    This option inserts edges from these MRCAs back to the roots of the input
    trees.

.. note:: Migrations are currently not supported by simplify, and an error will
    be raised if we attempt call simplify on a table collection with greater
    than zero migrations. See `<https://github.com/tskit-dev/tskit/issues/20>`_

The table collection will always be unindexed after simplify successfully
completes.
@endrst

@param self A pointer to a tsk_individual_table_t object.
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
int tsk_table_collection_simplify(tsk_table_collection_t *self, tsk_id_t *samples,
    tsk_size_t num_samples, tsk_flags_t options, tsk_id_t *node_map);

/**
@brief Subsets and reorders a table collection according to an array of nodes.

@rst
Reduces the table collection to contain only the entries referring to
the provided list of nodes, with nodes reordered according to the order
they appear in the ``nodes`` argument. Specifically, this subsets and reorders
each of the tables as follows:

1. Nodes: if in the list of nodes, and in the order provided.
2. Individuals and Populations: if referred to by a retained node,
   and in the order first seen when traversing the list of retained nodes.
3. Edges: if both parent and child are retained nodes.
4. Mutations: if the mutation's node is a retained node.
5. Sites: if any mutations remain at the site after removing mutations.

Retained edges, mutations, and sites appear in the same
order as in the original tables.

If ``nodes`` is the entire list of nodes in the tables, then the
resulting tables will be identical to the original tables, but with
nodes (and individuals and populations) reordered.

.. note:: Migrations are currently not supported by susbset, and an error will
    be raised if we attempt call subset on a table collection with greater
    than zero migrations.
@endrst

@param self A pointer to a tsk_table_collection_t object.
@param nodes An array of num_nodes valid node IDs.
@param num_nodes The number of node IDs in the input nodes array.
@return Return 0 on success or a negative value on failure.
*/
int tsk_table_collection_subset(
    tsk_table_collection_t *self, tsk_id_t *nodes, tsk_size_t num_nodes);

/**
@brief Forms the node-wise union of two table collections.

@rst
Expands this table collection by adding the non-shared portions of another table
collection to itself. The ``other_node_mapping`` encodes which nodes in ``other`` are
equivalent to a node in ``self``. The positions in the ``other_node_mapping`` array
correspond to node ids in ``other``, and the elements encode the equivalent
node id in ``self`` or TSK_NULL if the node is exclusive to ``other``. Nodes
that are exclusive ``other`` are added to ``self``, along with:

1. Individuals which are new to ``self``.
2. Edges whose parent or child are new to ``self``.
3. Sites which were not present in ``self``.
4. Mutations whose nodes are new to ``self``.

By default, populations of newly added nodes are assumed to be new populations,
and added to the population table as well.

This operation will also sort the resulting tables, so the tables may change
even if nothing new is added, if the original tables were not sorted.

**Options**:

Options can be specified by providing one or more of the following bitwise
flags:

TSK_UNION_NO_CHECK_SHARED
    By default, union checks that the portion of shared history between
    ``self`` and ``other``, as implied by ``other_node_mapping``, are indeed
    equivalent. It does so by subsetting both ``self`` and ``other`` on the
    equivalent nodes specified in ``other_node_mapping``, and then checking for
    equality of the subsets.
TSK_UNION_NO_ADD_POP
    By default, all nodes new to ``self`` are assigned new populations. If this
    option is specified, nodes that are added to ``self`` will retain the
    population IDs they have in ``other``.

.. note:: Migrations are currently not supported by union, and an error will
    be raised if we attempt call union on a table collection with migrations.
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
    tsk_table_collection_t *other, tsk_id_t *other_node_mapping, tsk_flags_t options);

/**
@brief Set the metadata
@rst
Copies the metadata string to this table collection, replacing any existing.
@endrst
@param self A pointer to a tsk_table_collection_t object.
@param metadata A pointer to a char array
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
@param metadata_schema A pointer to a char array
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
bool tsk_table_collection_has_index(tsk_table_collection_t *self, tsk_flags_t options);

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
table collection. All spatial values (along the genome) are checked
to see if they are finite values and within the required bounds. Time values
are checked to see if they are finite or marked as unknown.

To check if a set of tables fulfills the :ref:`requirements
<sec_valid_tree_sequence_requirements>` needed for a valid tree sequence, use
the TSK_CHECK_TREES option. When this method is called with TSK_CHECK_TREES,
the number of trees in the tree sequence is returned. Thus, to check for errors
client code should verify that the return value is less than zero. All other
options will return zero on success and a negative value on failure.

More fine-grained checks can be achieved using bitwise combinations of the
other options.

**Options**:

Options can be specified by providing one or more of the following bitwise
flags:

TSK_CHECK_EDGE_ORDERING
    Check edge ordering constraints for a tree sequence.
TSK_CHECK_SITE_ORDERING
    Check that sites are in nondecreasing position order.
TSK_CHECK_SITE_DUPLICATES
    Check for any duplicate site positions.
TSK_CHECK_MUTATION_ORDERING
    Check contraints on the ordering of mutations. Any non-null
    mutation parents and known times are checked for ordering
    constraints.
TSK_CHECK_INDEXES
    Check that the table indexes exist, and contain valid edge
    references.
TSK_CHECK_TREES
    All checks needed to define a valid tree sequence. Note that
    this implies all of the above checks.

It is sometimes useful to disregard some parts of the data model
when performing checks:

TSK_NO_CHECK_POPULATION_REFS
    Do not check integrity of references to populations. This
    can be safely combined with the other checks.
@endrst

@param self A pointer to a tsk_table_collection_t object.
@param options Bitwise options.
@return Return a negative error value on if any problems are detected
   in the tree sequence. If the TSK_CHECK_TREES option is provided,
   the number of trees in the tree sequence will be returned, on
   success.
*/
tsk_id_t tsk_table_collection_check_integrity(
    tsk_table_collection_t *self, tsk_flags_t options);

/** @} */

/* Undocumented methods */

int tsk_table_collection_link_ancestors(tsk_table_collection_t *self, tsk_id_t *samples,
    tsk_size_t num_samples, tsk_id_t *ancestors, tsk_size_t num_ancestors,
    tsk_flags_t options, tsk_edge_table_t *result);

int tsk_table_collection_deduplicate_sites(
    tsk_table_collection_t *tables, tsk_flags_t options);
int tsk_table_collection_compute_mutation_parents(
    tsk_table_collection_t *self, tsk_flags_t options);
int tsk_table_collection_compute_mutation_times(
    tsk_table_collection_t *self, double *random, tsk_flags_t TSK_UNUSED(options));

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
4. Sort the mutation table.

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
int tsk_table_sorter_run(struct _tsk_table_sorter_t *self, tsk_bookmark_t *start);

/**
@brief Free the internal memory for the specified table sorter.

@param self A pointer to an initialised tsk_table_sorter_t object.
@return Always returns 0.
*/
int tsk_table_sorter_free(struct _tsk_table_sorter_t *self);

/** @} */

int tsk_squash_edges(
    tsk_edge_t *edges, tsk_size_t num_edges, tsk_size_t *num_output_edges);

#ifdef __cplusplus
}
#endif
#endif
