/*
 * MIT License
 *
 * Copyright (c) 2019-2024 Tskit Developers
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

// clang-format off

/*
 * These are both undocumented options for tsk_tree_init
 */
#define TSK_SAMPLE_LISTS            (1 << 1)
#define TSK_NO_SAMPLE_COUNTS        (1 << 2)

#define TSK_STAT_SITE               (1 << 0)
#define TSK_STAT_BRANCH             (1 << 1)
#define TSK_STAT_NODE               (1 << 2)

/* Leave room for other stat types */
#define TSK_STAT_POLARISED               (1 << 10)
#define TSK_STAT_SPAN_NORMALISE          (1 << 11)
#define TSK_STAT_ALLOW_TIME_UNCALIBRATED (1 << 12)
#define TSK_STAT_PAIR_NORMALISE          (1 << 13)
#define TSK_STAT_NONCENTRED              (1 << 14)

/* Options for map_mutations */
#define TSK_MM_FIXED_ANCESTRAL_STATE (1 << 0)

#define TSK_DIR_FORWARD 1
#define TSK_DIR_REVERSE -1

/**
@defgroup API_FLAGS_TS_INIT_GROUP :c:func:`tsk_treeseq_init` specific flags.
@{
*/
/**
If specified edge indexes will be built and stored in the table collection
when the tree sequence is initialised. Indexes are required for a valid
tree sequence, and are not built by default for performance reasons.
*/
#define TSK_TS_INIT_BUILD_INDEXES (1 << 0)
/**
If specified, mutation parents in the table collection will be overwritten
with those computed from the topology when the tree sequence is initialised.
*/
#define TSK_TS_INIT_COMPUTE_MUTATION_PARENTS (1 << 1)
/** @} */

// clang-format on

/**
@brief The tree sequence object.
*/
typedef struct {
    tsk_size_t num_trees;
    tsk_size_t num_samples;
    tsk_id_t *samples;
    /* Does this tree sequence have time_units == "uncalibrated" */
    bool time_uncalibrated;
    /* Are all genome coordinates discrete? */
    bool discrete_genome;
    /* Are all time values discrete? */
    bool discrete_time;
    /* Min and max time in node table and mutation table */
    double min_time;
    double max_time;
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
    /** @brief  The table collection underlying this tree sequence, This table
     *  collection must be treated as read-only, and any changes to it will
     *  lead to undefined behaviour. */
    tsk_table_collection_t *tables;
} tsk_treeseq_t;

typedef struct {
    tsk_id_t index;
    struct {
        double left;
        double right;
    } interval;
    struct {
        tsk_id_t start;
        tsk_id_t stop;
        const tsk_id_t *order;
    } in;
    struct {
        tsk_id_t start;
        tsk_id_t stop;
        const tsk_id_t *order;
    } out;
    tsk_id_t left_current_index;
    tsk_id_t right_current_index;
    int direction;
    const tsk_treeseq_t *tree_sequence;
} tsk_tree_position_t;

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

On initialisation, a tree is in the :ref:`null state<sec_c_api_trees_null>` and
we must call one of the :ref:`seeking<sec_c_api_trees_seeking>` methods to make
the state of the tree object correspond to a particular tree in the sequence.
Please see the :ref:`sec_c_api_examples_tree_iteration` examples for
recommended usage.

@endrst
 */
typedef struct {
    /**
     * @brief The parent tree sequence.
     */
    const tsk_treeseq_t *tree_sequence;
    /**
     @brief The ID of the "virtual root" whose children are the roots of the
     tree.
     */
    tsk_id_t virtual_root;
    /**
     @brief The parent of node u is parent[u]. Equal to ``TSK_NULL`` if node u is
     a root or is not a node in the current tree.
     */
    tsk_id_t *parent;
    /**
     @brief The leftmost child of node u is left_child[u]. Equal to ``TSK_NULL``
     if node u is a leaf or is not a node in the current tree.
     */
    tsk_id_t *left_child;
    /**
     @brief The rightmost child of node u is right_child[u]. Equal to ``TSK_NULL``
     if node u is a leaf or is not a node in the current tree.
     */
    tsk_id_t *right_child;
    /**
     @brief The sibling to the left of node u is left_sib[u]. Equal to
     ``TSK_NULL`` if node u has no siblings to its left.
     */
    tsk_id_t *left_sib;
    /**
     @brief The sibling to the right of node u is right_sib[u]. Equal to
     ``TSK_NULL`` if node u has no siblings to its right.
     */
    tsk_id_t *right_sib;
    /**
     @brief The number of children of node u is num_children[u].
     */
    tsk_id_t *num_children;
    /**
     @brief Array of edge ids where ``edge[u]`` is the edge that encodes the
     relationship between the child node ``u`` and its parent. Equal to
     ``TSK_NULL`` if node ``u`` is a root, virtual root or is not a node in the
     current tree.
     */
    tsk_id_t *edge;
    /**
     @brief The total number of edges defining the topology of this tree.
     This is equal to the number of tree sequence edges that intersect with
     the tree's genomic interval.
     */
    tsk_size_t num_edges;
    /**
     @brief Left and right coordinates of the genomic interval that this
     tree covers. The left coordinate is inclusive and the right coordinate
     exclusive.

    @rst

    Example:

    .. code-block:: c

        tsk_tree_t tree;
        int ret;
        // initialise etc
        ret = tsk_tree_first(&tree);
        // Check for error
        assert(ret == TSK_TREE_OK);
        printf("Coordinates covered by first tree are left=%f, right=%f\n",
            tree.interval.left, tree.interval.right);

    @endrst

     */
    struct {
        double left;
        double right;
    } interval;
    /**
     @brief The index of this tree in the tree sequence.

     @rst
     This attribute provides the zero-based index of the tree represented by the
     current state of the struct within the parent tree sequence. For example,
     immediately after we call ``tsk_tree_first(&tree)``, ``tree.index`` will
     be zero, and after we call ``tsk_tree_last(&tree)``, ``tree.index`` will
     be the number of trees - 1 (see :c:func:`tsk_treeseq_get_num_trees`)
     When the tree is in the null state (immediately after initialisation,
     or after, e.g., calling :c:func:`tsk_tree_prev` on the first tree)
     the value of the ``index`` is -1.
     @endrst
     */
    tsk_id_t index;
    /* Attributes below are private and should not be used in client code. */
    tsk_size_t num_nodes;
    tsk_flags_t options;
    tsk_size_t root_threshold;
    const tsk_id_t *samples;
    /*
    These are involved in the optional sample tracking; num_samples counts
    all samples below a give node, and num_tracked_samples counts those
    from a specific subset. By default sample counts are tracked and roots
    maintained. If ``TSK_NO_SAMPLE_COUNTS`` is specified, then neither sample
    counts or roots are available.
    */
    tsk_size_t *num_samples;
    tsk_size_t *num_tracked_samples;
    /* These are for the optional sample list tracking. */
    tsk_id_t *left_sample;
    tsk_id_t *right_sample;
    tsk_id_t *next_sample;
    /* The sites on this tree */
    const tsk_site_t *sites;
    tsk_size_t sites_length;
    /* Counters needed for next() and prev() transformations. */
    int direction;
    tsk_id_t left_index;
    tsk_id_t right_index;
    tsk_tree_position_t tree_pos;
} tsk_tree_t;

/****************************************************************************/
/* Tree sequence.*/
/****************************************************************************/

/**
@defgroup TREESEQ_API_GROUP Tree sequence API
@{
*/

/**
@brief Initialises the tree sequence based on the specified table collection.

@rst
This method will copy the supplied table collection unless :c:macro:`TSK_TAKE_OWNERSHIP`
is specified. The table collection will be checked for integrity and index maps built.

This must be called before any operations are performed on the tree sequence.
See the :ref:`sec_c_api_overview_structure` for details on how objects
are initialised and freed.

If specified, TSK_TAKE_OWNERSHIP takes immediate ownership of the tables, regardless
of error conditions.

**Options**

- :c:macro:`TSK_TS_INIT_BUILD_INDEXES`
- :c:macro:`TSK_TAKE_OWNERSHIP` (applies to the table collection).
@endrst

@param self A pointer to an uninitialised tsk_table_collection_t object.
@param tables A pointer to a tsk_table_collection_t object.
@param options Allocation time options. See above for details.
@return Return 0 on success or a negative value on failure.
*/
int tsk_treeseq_init(
    tsk_treeseq_t *self, tsk_table_collection_t *tables, tsk_flags_t options);

/**
@brief Load a tree sequence from a file path.

@rst
Loads the data from the specified file into this tree sequence.
The tree sequence is also initialised.
The resources allocated must be freed using
:c:func:`tsk_treeseq_free` even in error conditions.

Works similarly to :c:func:`tsk_table_collection_load` please see
that function's documentation for details and options.

**Examples**

.. code-block:: c

    int ret;
    tsk_treeseq_t ts;
    ret = tsk_treeseq_load(&ts, "data.trees", 0);
    if (ret != 0) {
        fprintf(stderr, "Load error:%s\n", tsk_strerror(ret));
        exit(EXIT_FAILURE);
    }

@endrst

@param self A pointer to an uninitialised tsk_treeseq_t object
@param filename A NULL terminated string containing the filename.
@param options Bitwise options. See above for details.
@return Return 0 on success or a negative value on failure.
*/
int tsk_treeseq_load(tsk_treeseq_t *self, const char *filename, tsk_flags_t options);

/**
@brief Load a tree sequence from a stream.

@rst
Loads a tree sequence from the specified file stream. The tree sequence
is also initialised. The resources allocated must be freed using
:c:func:`tsk_treeseq_free` even in error conditions.

Works similarly to :c:func:`tsk_table_collection_loadf` please
see that function's documentation for details and options.

@endrst

@param self A pointer to an uninitialised tsk_treeseq_t object.
@param file A FILE stream opened in an appropriate mode for reading (e.g.
    "r", "r+" or "w+") positioned at the beginning of a tree sequence
    definition.
@param options Bitwise options. See above for details.
@return Return 0 on success or a negative value on failure.
*/
int tsk_treeseq_loadf(tsk_treeseq_t *self, FILE *file, tsk_flags_t options);

/**
@brief Write a tree sequence to file.

@rst
Writes the data from this tree sequence to the specified file.

If an error occurs the file path is deleted, ensuring that only complete
and well formed files will be written.
@endrst

@param self A pointer to an initialised tsk_treeseq_t object.
@param filename A NULL terminated string containing the filename.
@param options Bitwise options. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_treeseq_dump(
    const tsk_treeseq_t *self, const char *filename, tsk_flags_t options);

/**
@brief Write a tree sequence to a stream.

@rst
Writes the data from this tree sequence to the specified FILE stream.
Semantics are identical to :c:func:`tsk_treeseq_dump`.

Please see the :ref:`sec_c_api_examples_file_streaming` section for an example
of how to sequentially dump and load tree sequences from a stream.
@endrst

@param self A pointer to an initialised tsk_treeseq_t object.
@param file A FILE stream opened in an appropriate mode for writing (e.g.
    "w", "a", "r+" or "w+").
@param options Bitwise options. Currently unused; should be
    set to zero to ensure compatibility with later versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_treeseq_dumpf(const tsk_treeseq_t *self, FILE *file, tsk_flags_t options);

/**
@brief Copies the state of the table collection underlying this tree sequence
into the specified destination table collection.

@rst
By default the method initialises the specified destination table collection. If the
destination is already initialised, the :c:macro:`TSK_NO_INIT` option should
be supplied to avoid leaking memory.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@param tables A pointer to a tsk_table_collection_t object. If the TSK_NO_INIT
option is specified, this must be an initialised table collection. If not, it must be an
uninitialised table collection.
@param options Bitwise option flags.
@return Return 0 on success or a negative value on failure.
*/
int tsk_treeseq_copy_tables(
    const tsk_treeseq_t *self, tsk_table_collection_t *tables, tsk_flags_t options);

/**
@brief Free the internal memory for the specified tree sequence.

@param self A pointer to an initialised tsk_treeseq_t object.
@return Always returns 0.
*/
int tsk_treeseq_free(tsk_treeseq_t *self);

/**
@brief Print out the state of this tree sequence to the specified stream.

This method is intended for debugging purposes and should not be used
in production code. The format of the output should **not** be depended
on and may change arbitrarily between versions.

@param self A pointer to a tsk_treeseq_t object.
@param out The stream to write the summary to.
*/
void tsk_treeseq_print_state(const tsk_treeseq_t *self, FILE *out);

/**
@brief Get the number of nodes

@rst
Returns the number of nodes in this tree sequence.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns the number of nodes.
*/
tsk_size_t tsk_treeseq_get_num_nodes(const tsk_treeseq_t *self);

/**
@brief Get the number of edges

@rst
Returns the number of edges in this tree sequence.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns the number of edges.
*/

tsk_size_t tsk_treeseq_get_num_edges(const tsk_treeseq_t *self);

/**
@brief Get the number of migrations

@rst
Returns the number of migrations in this tree sequence.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns the number of migrations.
*/
tsk_size_t tsk_treeseq_get_num_migrations(const tsk_treeseq_t *self);

/**
@brief Get the number of sites

@rst
Returns the number of sites in this tree sequence.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns the number of sites.
*/
tsk_size_t tsk_treeseq_get_num_sites(const tsk_treeseq_t *self);

/**
@brief Get the number of mutations

@rst
Returns the number of mutations in this tree sequence.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns the number of mutations.
*/
tsk_size_t tsk_treeseq_get_num_mutations(const tsk_treeseq_t *self);

/**
@brief Get the number of provenances

@rst
Returns the number of provenances in this tree sequence.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns the number of provenances.
*/
tsk_size_t tsk_treeseq_get_num_provenances(const tsk_treeseq_t *self);

/**
@brief Get the number of populations

@rst
Returns the number of populations in this tree sequence.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns the number of populations.
*/
tsk_size_t tsk_treeseq_get_num_populations(const tsk_treeseq_t *self);

/**
@brief Get the number of individuals

@rst
Returns the number of individuals in this tree sequence.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns the number of individuals.
*/
tsk_size_t tsk_treeseq_get_num_individuals(const tsk_treeseq_t *self);

/**
@brief Return the number of trees in this tree sequence.

@rst
This is a constant time operation.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return The number of trees in the tree sequence.
*/
tsk_size_t tsk_treeseq_get_num_trees(const tsk_treeseq_t *self);

/**
@brief Get the number of samples

@rst
Returns the number of nodes marked as samples in this tree sequence.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns the number of samples.
*/
tsk_size_t tsk_treeseq_get_num_samples(const tsk_treeseq_t *self);

/**
@brief Get the top-level tree sequence metadata.

@rst
Returns a pointer to the metadata string, which is owned by the tree sequence and
not null-terminated.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns a pointer to the metadata.
*/
const char *tsk_treeseq_get_metadata(const tsk_treeseq_t *self);

/**
@brief Get the length of top-level tree sequence metadata

@rst
Returns the length of the metadata string.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns the length of the metadata.
*/
tsk_size_t tsk_treeseq_get_metadata_length(const tsk_treeseq_t *self);

/**
@brief Get the top-level tree sequence metadata schema.

@rst
Returns a pointer to the metadata schema string, which is owned by the tree sequence and
not null-terminated.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns a pointer to the metadata schema.
*/
const char *tsk_treeseq_get_metadata_schema(const tsk_treeseq_t *self);

/**
@brief Get the length of the top-level tree sequence metadata schema.

@rst
Returns the length of the metadata schema string.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns the length of the metadata schema.
*/
tsk_size_t tsk_treeseq_get_metadata_schema_length(const tsk_treeseq_t *self);

/**
@brief Get the time units string

@rst
Returns a pointer to the time units string, which is owned by the tree sequence and
not null-terminated.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns a pointer to the time units.
*/
const char *tsk_treeseq_get_time_units(const tsk_treeseq_t *self);

/**
@brief Get the length of time units string
@rst
Returns the length of the time units string.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns the length of the time units.
*/
tsk_size_t tsk_treeseq_get_time_units_length(const tsk_treeseq_t *self);

/**
@brief Get the file uuid

@rst
Returns a pointer to the null-terminated file uuid string, which is owned by the tree
sequence.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns a pointer to the time units.
*/
const char *tsk_treeseq_get_file_uuid(const tsk_treeseq_t *self);

/**
@brief Get the sequence length

@rst
Returns the sequence length of this tree sequence
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns the sequence length.
*/
double tsk_treeseq_get_sequence_length(const tsk_treeseq_t *self);

/**
@brief Get the breakpoints

@rst
Returns an array of breakpoint locations, the array is owned by the tree sequence.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns the pointer to the breakpoint array.
*/
const double *tsk_treeseq_get_breakpoints(const tsk_treeseq_t *self);

/**
@brief Get the samples

@rst
Returns an array of ids of sample nodes in this tree sequence.
I.e. nodes that have the :c:macro:`TSK_NODE_IS_SAMPLE` flag set.
The array is owned by the tree sequence and should not be modified or free'd.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns the pointer to the sample node id array.
*/
const tsk_id_t *tsk_treeseq_get_samples(const tsk_treeseq_t *self);

/**
@brief Get the map of node id to sample index

@rst
Returns the location of each node in the list of samples or
:c:macro:`TSK_NULL` for nodes that are not samples.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns the pointer to the array of sample indexes.
*/
const tsk_id_t *tsk_treeseq_get_sample_index_map(const tsk_treeseq_t *self);

/**
@brief Check if a node is a sample

@rst
Returns the sample status of a given node id.
@endrst
@param self A pointer to a tsk_treeseq_t object.
@param u The id of the node to be checked.
@return Returns true if the node is a sample.
*/
bool tsk_treeseq_is_sample(const tsk_treeseq_t *self, tsk_id_t u);

/**
@brief Get the discrete genome status

@rst
If all the genomic locations in the tree sequence are discrete integer values
then this flag will be true.
@endrst
@param self A pointer to a tsk_treeseq_t object.
@return Returns true if all genomic locations are discrete.
*/
bool tsk_treeseq_get_discrete_genome(const tsk_treeseq_t *self);

/**
@brief Get the discrete time status

@rst
If all times in the tree sequence are discrete integer values
then this flag will be true
@endrst
@param self A pointer to a tsk_treeseq_t object.
@return Returns true if all times are discrete.
*/
bool tsk_treeseq_get_discrete_time(const tsk_treeseq_t *self);

/**
@brief Get the min time in node table and mutation table

@rst
The times stored in both the node and mutation tables are considered.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns the min time of all nodes and mutations.
*/
double tsk_treeseq_get_min_time(const tsk_treeseq_t *self);

/**
@brief Get the max time in node table and mutation table

@rst
The times stored in both the node and mutation tables are considered.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@return Returns the max time of all nodes and mutations.
*/
double tsk_treeseq_get_max_time(const tsk_treeseq_t *self);

/**
@brief Get a node by its index

@rst
Copies a node from this tree sequence to the specified destination.
@endrst
@param self A pointer to a tsk_treeseq_t object.
@param index The node index to copy
@param node A pointer to a tsk_node_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_treeseq_get_node(const tsk_treeseq_t *self, tsk_id_t index, tsk_node_t *node);

/**
@brief Get a edge by its index

@rst
Copies a edge from this tree sequence to the specified destination.
@endrst
@param self A pointer to a tsk_treeseq_t object.
@param index The edge index to copy
@param edge A pointer to a tsk_edge_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_treeseq_get_edge(const tsk_treeseq_t *self, tsk_id_t index, tsk_edge_t *edge);

/**
@brief Get a edge by its index

@rst
Copies a migration from this tree sequence to the specified destination.
@endrst
@param self A pointer to a tsk_treeseq_t object.
@param index The migration index to copy
@param migration A pointer to a tsk_migration_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_treeseq_get_migration(
    const tsk_treeseq_t *self, tsk_id_t index, tsk_migration_t *migration);

/**
@brief Get a site by its index

@rst
Copies a site from this tree sequence to the specified destination.
@endrst
@param self A pointer to a tsk_treeseq_t object.
@param index The site index to copy
@param site A pointer to a tsk_site_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_treeseq_get_site(const tsk_treeseq_t *self, tsk_id_t index, tsk_site_t *site);

/**
@brief Get a mutation by its index

@rst
Copies a mutation from this tree sequence to the specified destination.
@endrst
@param self A pointer to a tsk_treeseq_t object.
@param index The mutation index to copy
@param mutation A pointer to a tsk_mutation_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_treeseq_get_mutation(
    const tsk_treeseq_t *self, tsk_id_t index, tsk_mutation_t *mutation);

/**
@brief Get a provenance by its index

@rst
Copies a provenance from this tree sequence to the specified destination.
@endrst
@param self A pointer to a tsk_treeseq_t object.
@param index The provenance index to copy
@param provenance A pointer to a tsk_provenance_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_treeseq_get_provenance(
    const tsk_treeseq_t *self, tsk_id_t index, tsk_provenance_t *provenance);

/**
@brief Get a population by its index

@rst
Copies a population from this tree sequence to the specified destination.
@endrst
@param self A pointer to a tsk_treeseq_t object.
@param index The population index to copy
@param population A pointer to a tsk_population_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_treeseq_get_population(
    const tsk_treeseq_t *self, tsk_id_t index, tsk_population_t *population);

/**
@brief Get a individual by its index

@rst
Copies a individual from this tree sequence to the specified destination.
@endrst
@param self A pointer to a tsk_treeseq_t object.
@param index The individual index to copy
@param individual A pointer to a tsk_individual_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_treeseq_get_individual(
    const tsk_treeseq_t *self, tsk_id_t index, tsk_individual_t *individual);

/**
@brief Create a simplified instance of this tree sequence

@rst
Copies this tree sequence to the specified destination and performs simplification.
The destination tree sequence should be uninitialised.
Simplification transforms the tables to remove redundancy and canonicalise
tree sequence data. See the :ref:`simplification <sec_simplification>` tutorial for
more details.

For full details and flags see :c:func:`tsk_table_collection_simplify` which performs
the same operation in place.

@endrst
@param self A pointer to a uninitialised tsk_treeseq_t object.
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
@param output A pointer to an uninitialised tsk_treeseq_t object.
@param node_map If not NULL, this array will be filled to define the mapping
    between nodes IDs in the table collection before and after simplification.
@return Return 0 on success or a negative value on failure.
*/
int tsk_treeseq_simplify(const tsk_treeseq_t *self, const tsk_id_t *samples,
    tsk_size_t num_samples, tsk_flags_t options, tsk_treeseq_t *output,
    tsk_id_t *node_map);

/**
@brief Extends haplotypes

Returns a new tree sequence in which the span covered by ancestral nodes
is "extended" to regions of the genome according to the following rule:
If an ancestral segment corresponding to node `n` has ancestor `p` and
descendant `c` on some portion of the genome, and on an adjacent segment of
genome `p` is still an ancestor of `c`, then `n` is inserted into the
path from `p` to `c`. For instance, if `p` is the parent of `n` and `n`
is the parent of `c`, then the span of the edges from `p` to `n` and
`n` to `c` are extended, and the span of the edge from `p` to `c` is
reduced. However, any edges whose child node is a sample are not
modified.  See Fritze et al. (2025):
https://doi.org/10.1093/genetics/iyaf198 for more details.

The method works by iterating over the genome to look for edges that can
be extended in this way; the maximum number of such iterations is
controlled by ``max_iter``.

The `node` of certain mutations may also be remapped; to do this
unambiguously we need to know mutation times. If mutations times are unknown,
use `tsk_table_collection_compute_mutation_times` first.

The method will not affect any tables except the edge table, or the node
column in the mutation table.

@rst

**Options**: None currently defined.
@endrst

@param self A pointer to a tsk_treeseq_t object.
@param max_iter The maximum number of iterations over the tree sequence.
@param options Bitwise option flags. (UNUSED)
@param output A pointer to an uninitialised tsk_treeseq_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_treeseq_extend_haplotypes(
    const tsk_treeseq_t *self, int max_iter, tsk_flags_t options, tsk_treeseq_t *output);

/** @} */

int tsk_treeseq_split_edges(const tsk_treeseq_t *self, double time, tsk_flags_t flags,
    tsk_id_t population, const char *metadata, tsk_size_t metadata_length,
    tsk_flags_t options, tsk_treeseq_t *output);

bool tsk_treeseq_has_reference_sequence(const tsk_treeseq_t *self);

/**
@brief Decode full-length alignments for specified nodes over an interval.

@rst
Fills a caller-provided buffer with per-node sequence alignments for the interval
``[left, right)``. Each row is exactly ``L = right - left`` bytes with no trailing
terminator, and rows are tightly packed in row-major order in the output buffer.

The output at non-site positions comes from the provided ``ref_seq`` slice
(``ref_seq[left:right]``); per-site alleles are overlaid onto this for each node.

If the :c:macro:`TSK_ISOLATED_NOT_MISSING` option is
not set, nodes that are isolated (no parent and no children) within a tree
interval in ``[left, right)`` are rendered as the ``missing_data_character`` for
that interval. At site positions, decoded genotypes override any previous value;
if a genotype is missing (``TSK_MISSING_DATA``), the ``missing_data_character`` is
overlaid onto the reference base.

Requirements and validation:

- The tree sequence must have a discrete genome.
- ``left`` and ``right`` must be integers with ``0 <= left < right <= sequence_length``.
- ``ref_seq`` must be non-NULL and ``ref_seq_length == sequence_length``.
- Each allele at a site must be exactly one byte; alleles equal to
  ``missing_data_character`` are not permitted.

@endrst

@param self A pointer to a :c:type:`tsk_treeseq_t` object.
@param ref_seq Pointer to a reference sequence buffer of length ``ref_seq_length``.
@param ref_seq_length The total length of ``ref_seq``; must equal the tree sequence
length.
@param nodes Array of node IDs to decode (may include non-samples).
@param num_nodes The number of nodes in ``nodes`` and rows in the output.
@param left The inclusive-left genomic coordinate of the output interval.
@param right The exclusive-right genomic coordinate of the output interval.
@param missing_data_character The byte to use for missing data.
@param alignments_out Output buffer of size at least ``num_nodes * (right - left)``.
@param options Bitwise option flags; supports :c:macro:`TSK_ISOLATED_NOT_MISSING`.
@return Return 0 on success or a negative value on failure.
*/
int tsk_treeseq_decode_alignments(const tsk_treeseq_t *self, const char *ref_seq,
    tsk_size_t ref_seq_length, const tsk_id_t *nodes, tsk_size_t num_nodes, double left,
    double right, char missing_data_character, char *alignments_out,
    tsk_flags_t options);

int tsk_treeseq_get_individuals_population(const tsk_treeseq_t *self, tsk_id_t *output);
int tsk_treeseq_get_individuals_time(const tsk_treeseq_t *self, double *output);

int tsk_treeseq_kc_distance(const tsk_treeseq_t *self, const tsk_treeseq_t *other,
    double lambda_, double *result);

int tsk_treeseq_genealogical_nearest_neighbours(const tsk_treeseq_t *self,
    const tsk_id_t *focal, tsk_size_t num_focal, const tsk_id_t *const *reference_sets,
    const tsk_size_t *reference_set_size, tsk_size_t num_reference_sets,
    tsk_flags_t options, double *ret_array);
int tsk_treeseq_mean_descendants(const tsk_treeseq_t *self,
    const tsk_id_t *const *reference_sets, const tsk_size_t *reference_set_size,
    tsk_size_t num_reference_sets, tsk_flags_t options, double *ret_array);

typedef int general_stat_func_t(tsk_size_t state_dim, const double *state,
    tsk_size_t result_dim, double *result, void *params);

int tsk_treeseq_general_stat(const tsk_treeseq_t *self, tsk_size_t K, const double *W,
    tsk_size_t M, general_stat_func_t *f, void *f_params, tsk_size_t num_windows,
    const double *windows, tsk_flags_t options, double *result);

typedef int norm_func_t(tsk_size_t result_dim, const double *hap_weights, tsk_size_t n_a,
    tsk_size_t n_b, double *result, void *params);

int tsk_treeseq_two_locus_count_stat(const tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, const tsk_size_t *sample_set_sizes,
    const tsk_id_t *sample_sets, tsk_size_t result_dim, const tsk_id_t *set_indexes,
    general_stat_func_t *f, norm_func_t *norm_f, tsk_size_t out_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t out_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result);

/* One way weighted stats */

typedef int one_way_weighted_method(const tsk_treeseq_t *self, tsk_size_t num_weights,
    const double *weights, tsk_size_t num_windows, const double *windows,
    tsk_flags_t options, double *result);

int tsk_treeseq_trait_covariance(const tsk_treeseq_t *self, tsk_size_t num_weights,
    const double *weights, tsk_size_t num_windows, const double *windows,
    tsk_flags_t options, double *result);
int tsk_treeseq_trait_correlation(const tsk_treeseq_t *self, tsk_size_t num_weights,
    const double *weights, tsk_size_t num_windows, const double *windows,
    tsk_flags_t options, double *result);

/* One way weighted stats with covariates */

typedef int one_way_covariates_method(const tsk_treeseq_t *self, tsk_size_t num_weights,
    const double *weights, tsk_size_t num_covariates, const double *covariates,
    tsk_size_t num_windows, const double *windows, tsk_flags_t options, double *result);

int tsk_treeseq_trait_linear_model(const tsk_treeseq_t *self, tsk_size_t num_weights,
    const double *weights, tsk_size_t num_covariates, const double *covariates,
    tsk_size_t num_windows, const double *windows, tsk_flags_t options, double *result);

/* Two way weighted stats with covariates */

typedef int two_way_weighted_method(const tsk_treeseq_t *self, tsk_size_t num_weights,
    const double *weights, tsk_size_t num_index_tuples, const tsk_id_t *index_tuples,
    tsk_size_t num_windows, const double *windows, double *result, tsk_flags_t options);

int tsk_treeseq_genetic_relatedness_weighted(const tsk_treeseq_t *self,
    tsk_size_t num_weights, const double *weights, tsk_size_t num_index_tuples,
    const tsk_id_t *index_tuples, tsk_size_t num_windows, const double *windows,
    double *result, tsk_flags_t options);

/* One way weighted stats with vector output */

typedef int weighted_vector_method(const tsk_treeseq_t *self, tsk_size_t num_weights,
    const double *weights, tsk_size_t num_windows, const double *windows,
    tsk_size_t num_focal_nodes, const tsk_id_t *focal_nodes, double *result,
    tsk_flags_t options);

int tsk_treeseq_genetic_relatedness_vector(const tsk_treeseq_t *self,
    tsk_size_t num_weights, const double *weights, tsk_size_t num_windows,
    const double *windows, tsk_size_t num_focal_nodes, const tsk_id_t *focal_nodes,
    double *result, tsk_flags_t options);

/* One way sample set stats */

typedef int one_way_sample_stat_method(const tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, const tsk_size_t *sample_set_sizes,
    const tsk_id_t *sample_sets, tsk_size_t num_windows, const double *windows,
    tsk_flags_t options, double *result);

int tsk_treeseq_diversity(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_windows, const double *windows, tsk_flags_t options, double *result);
int tsk_treeseq_segregating_sites(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_windows, const double *windows, tsk_flags_t options, double *result);
int tsk_treeseq_Y1(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_windows, const double *windows, tsk_flags_t options, double *result);
int tsk_treeseq_allele_frequency_spectrum(const tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, const tsk_size_t *sample_set_sizes,
    const tsk_id_t *sample_sets, tsk_size_t num_windows, const double *windows,
    tsk_size_t num_time_windows, const double *time_windows, tsk_flags_t options,
    double *result);

typedef int general_sample_stat_method(const tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, const tsk_size_t *sample_set_sizes,
    const tsk_id_t *sample_sets, tsk_size_t num_indexes, const tsk_id_t *indexes,
    tsk_size_t num_windows, const double *windows, tsk_flags_t options, double *result);

typedef int two_locus_count_stat_method(const tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, const tsk_size_t *sample_set_sizes,
    const tsk_id_t *sample_sets, tsk_size_t num_rows, const tsk_id_t *row_sites,
    const double *row_positions, tsk_size_t num_cols, const tsk_id_t *col_sites,
    const double *col_positions, tsk_flags_t options, double *result);

int tsk_treeseq_D(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result);
int tsk_treeseq_D2(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result);
int tsk_treeseq_r2(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result);
int tsk_treeseq_D_prime(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result);
int tsk_treeseq_r(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result);
int tsk_treeseq_Dz(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result);
int tsk_treeseq_pi2(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result);
int tsk_treeseq_D2_unbiased(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result);
int tsk_treeseq_Dz_unbiased(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result);
int tsk_treeseq_pi2_unbiased(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result);

typedef int k_way_two_locus_count_stat_method(const tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, const tsk_size_t *sample_set_sizes,
    const tsk_id_t *sample_sets, tsk_size_t num_index_tuples,
    const tsk_id_t *index_tuples, tsk_size_t num_rows, const tsk_id_t *row_sites,
    const double *row_positions, tsk_size_t num_cols, const tsk_id_t *col_sites,
    const double *col_positions, tsk_flags_t options, double *result);

/* Two way sample set stats */

int tsk_treeseq_divergence(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_windows,
    const double *windows, tsk_flags_t options, double *result);
int tsk_treeseq_Y2(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_windows,
    const double *windows, tsk_flags_t options, double *result);
int tsk_treeseq_f2(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_windows,
    const double *windows, tsk_flags_t options, double *result);
int tsk_treeseq_genetic_relatedness(const tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, const tsk_size_t *sample_set_sizes,
    const tsk_id_t *sample_sets, tsk_size_t num_index_tuples,
    const tsk_id_t *index_tuples, tsk_size_t num_windows, const double *windows,
    tsk_flags_t options, double *result);
int tsk_treeseq_D2_ij(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result);
int tsk_treeseq_D2_ij_unbiased(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result);
int tsk_treeseq_r2_ij(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result);

/* Three way sample set stats */
int tsk_treeseq_Y3(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_windows,
    const double *windows, tsk_flags_t options, double *result);
int tsk_treeseq_f3(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_windows,
    const double *windows, tsk_flags_t options, double *result);

/* Four way sample set stats */
int tsk_treeseq_f4(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_windows,
    const double *windows, tsk_flags_t options, double *result);

int tsk_treeseq_divergence_matrix(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_windows, const double *windows, tsk_flags_t options, double *result);

/* Coalescence rates */
typedef int pair_coalescence_stat_func_t(tsk_size_t input_dim, const double *atoms,
    const double *weights, tsk_size_t result_dim, double *result, void *params);
int tsk_treeseq_pair_coalescence_stat(const tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, const tsk_size_t *sample_set_sizes,
    const tsk_id_t *sample_sets, tsk_size_t num_set_indexes, const tsk_id_t *set_indexes,
    tsk_size_t num_windows, const double *windows, tsk_size_t num_bins,
    const tsk_id_t *node_bin_map, pair_coalescence_stat_func_t *summary_func,
    tsk_size_t summary_func_dim, void *summary_func_args, tsk_flags_t options,
    double *result);
int tsk_treeseq_pair_coalescence_counts(const tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, const tsk_size_t *sample_set_sizes,
    const tsk_id_t *sample_sets, tsk_size_t num_set_indexes, const tsk_id_t *set_indexes,
    tsk_size_t num_windows, const double *windows, tsk_size_t num_bins,
    const tsk_id_t *node_bin_map, tsk_flags_t options, double *result);
int tsk_treeseq_pair_coalescence_quantiles(const tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, const tsk_size_t *sample_set_sizes,
    const tsk_id_t *sample_sets, tsk_size_t num_set_indexes, const tsk_id_t *set_indexes,
    tsk_size_t num_windows, const double *windows, tsk_size_t num_bins,
    const tsk_id_t *node_bin_map, tsk_size_t num_quantiles, double *quantiles,
    tsk_flags_t options, double *result);
int tsk_treeseq_pair_coalescence_rates(const tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, const tsk_size_t *sample_set_sizes,
    const tsk_id_t *sample_sets, tsk_size_t num_set_indexes, const tsk_id_t *set_indexes,
    tsk_size_t num_windows, const double *windows, tsk_size_t num_time_windows,
    const tsk_id_t *node_time_window, double *time_windows, tsk_flags_t options,
    double *result);

/****************************************************************************/
/* Tree */
/****************************************************************************/

/**
@defgroup TREE_API_LIFECYCLE_GROUP Tree lifecycle
@{
*/

/**
@brief Initialises the tree by allocating internal memory and associating
    with the specified tree sequence.

@rst
This must be called before any operations are performed on the tree.

The specified tree sequence object must be initialised, and must be
valid for the full lifetime of this tree.

See the :ref:`sec_c_api_overview_structure` for details on how objects
are initialised and freed.

The ``options`` parameter is provided to support future expansions
of the API. A number of undocumented internal features are controlled
via this parameter, and it **must** be set to 0 to ensure that operations
work as expected and for compatibility with future versions of tskit.
@endrst

@param self A pointer to an uninitialised tsk_tree_t object.
@param tree_sequence A pointer to an initialised tsk_treeseq_t object.
@param options Allocation time options. Must be 0, or behaviour is undefined.
@return Return 0 on success or a negative value on failure.
*/
int tsk_tree_init(
    tsk_tree_t *self, const tsk_treeseq_t *tree_sequence, tsk_flags_t options);

/**
@brief Free the internal memory for the specified tree.

@param self A pointer to an initialised tsk_tree_t object.
@return Always returns 0.
*/
int tsk_tree_free(tsk_tree_t *self);

/**
@brief Copies the state of this tree into the specified destination.

@rst
By default (``options`` = 0) the method initialises the specified destination
tree by calling :c:func:`tsk_tree_init`. If the destination is already
initialised, the :c:macro:`TSK_NO_INIT` option should be supplied to avoid
leaking memory. If :c:macro:`TSK_NO_INIT` is supplied and the tree sequence associated
with the ``dest`` tree is not equal to the tree sequence associated
with ``self``, an error is raised.

The destination tree will keep a reference to the tree sequence object
associated with the source tree, and this tree sequence must be
valid for the full lifetime of the destination tree.

**Options**

- :c:macro:`TSK_NO_INIT`

If :c:macro:`TSK_NO_INIT` is not specified, options for :c:func:`tsk_tree_init`
can be provided and will be passed on.

@endrst

@param self A pointer to an initialised tsk_tree_t object.
@param dest A pointer to a tsk_tree_t object. If the TSK_NO_INIT option
    is specified, this must be an initialised tree. If not, it must
    be an uninitialised tree.
@param options Copy and allocation time options. See the notes above for details.
@return Return 0 on success or a negative value on failure.
*/
int tsk_tree_copy(const tsk_tree_t *self, tsk_tree_t *dest, tsk_flags_t options);

/** @} */

/**
@defgroup TREE_API_SEEKING_GROUP Seeking along the sequence
@{
*/

/** @brief Option to seek by skipping to the target tree, adding and removing as few
   edges as possible. If not specified, a linear time algorithm is used instead.

    @ingroup TREE_API_SEEKING_GROUP
*/
#define TSK_SEEK_SKIP (1 << 0)

/**
@brief Seek to the first tree in the sequence.

@rst
Set the state of this tree to reflect the first tree in parent
tree sequence.
@endrst

@param self A pointer to an initialised tsk_tree_t object.
@return Return TSK_TREE_OK on success; or a negative value if an error occurs.
*/
int tsk_tree_first(tsk_tree_t *self);

/**
@brief Seek to the last tree in the sequence.

@rst
Set the state of this tree to reflect the last tree in parent
tree sequence.
@endrst

@param self A pointer to an initialised tsk_tree_t object.
@return Return TSK_TREE_OK on success; or a negative value if an error occurs.
*/
int tsk_tree_last(tsk_tree_t *self);

/**
@brief Seek to the next tree in the sequence.

@rst
Set the state of this tree to reflect the next tree in parent
tree sequence. If the index of the current tree is ``j``,
then the after this operation the index will be ``j + 1``.

Calling :c:func:`tsk_tree_next` a tree in the
:ref:`null state<sec_c_api_trees_null>` is equivalent to calling
:c:func:`tsk_tree_first`.

Calling :c:func:`tsk_tree_next` on the last tree in the
sequence will transform it into the
:ref:`null state<sec_c_api_trees_null>` (equivalent to
calling :c:func:`tsk_tree_clear`).

Please see the :ref:`sec_c_api_examples_tree_iteration` examples for
recommended usage.
@endrst

@param self A pointer to an initialised tsk_tree_t object.
@return Return TSK_TREE_OK on successfully transforming to a
non-null tree; 0 on successfully transforming into the null
tree; or a negative value if an error occurs.
*/
int tsk_tree_next(tsk_tree_t *self);

/**
@brief Seek to the previous tree in the sequence.

@rst
Set the state of this tree to reflect the previous tree in parent
tree sequence. If the index of the current tree is ``j``,
then the after this operation the index will be ``j - 1``.

Calling :c:func:`tsk_tree_prev` a tree in the
:ref:`null state<sec_c_api_trees_null>` is equivalent to calling
:c:func:`tsk_tree_last`.

Calling :c:func:`tsk_tree_prev` on the first tree in the
sequence will transform it into the
:ref:`null state<sec_c_api_trees_null>` (equivalent to
calling :c:func:`tsk_tree_clear`).

Please see the :ref:`sec_c_api_examples_tree_iteration` examples for
recommended usage.
@endrst

@param self A pointer to an initialised tsk_tree_t object.
@return Return TSK_TREE_OK on successfully transforming to a
non-null tree; 0 on successfully transforming into the null
tree; or a negative value if an error occurs.
*/
int tsk_tree_prev(tsk_tree_t *self);

/**
@brief Set the tree into the null state.

@rst
Transform this tree into the :ref:`null state<sec_c_api_trees_null>`.
@endrst

@param self A pointer to an initialised tsk_tree_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_tree_clear(tsk_tree_t *self);

/**
@brief Seek to a particular position on the genome.

@rst
Set the state of this tree to reflect the tree in parent
tree sequence covering the specified ``position``. That is, on success
we will have ``tree.interval.left <= position`` and
we will have ``position < tree.interval.right``.

Seeking to a position currently covered by the tree is
a constant time operation.

Seeking to a position from a non-null tree uses a linear time
algorithm by default, unless the option :c:macro:`TSK_SEEK_SKIP`
is specified. In this case, a faster algorithm is employed which skips
to the target tree by removing and adding the minimal number of edges
possible. However, this approach does not guarantee that edges are
inserted and removed in time-sorted order.

.. warning:: Using the :c:macro:`TSK_SEEK_SKIP` option
    may lead to edges not being inserted or removed in time-sorted order.

@endrst

@param self A pointer to an initialised tsk_tree_t object.
@param position The position in genome coordinates
@param options Seek options. See the notes above for details.
@return Return 0 on success or a negative value on failure.
*/
int tsk_tree_seek(tsk_tree_t *self, double position, tsk_flags_t options);

/**
@brief Seek to a specific tree in a tree sequence.

@rst
Set the state of this tree to reflect the tree in parent
tree sequence whose index is ``0 <= tree < num_trees``.
@endrst

@param self A pointer to an initialised tsk_tree_t object.
@param tree The target tree index.
@param options Seek options. Currently unused. Set to 0 for compatibility
    with future versions of tskit.
@return Return 0 on success or a negative value on failure.
*/
int tsk_tree_seek_index(tsk_tree_t *self, tsk_id_t tree, tsk_flags_t options);

/** @} */

/**
@defgroup TREE_API_TREE_QUERY_GROUP Tree Queries
@{
*/

/**
@brief Returns the number of roots in this tree.

@rst
See the :ref:`sec_data_model_tree_roots` section for more information
on how the roots of a tree are defined.
@endrst

@param self A pointer to an initialised tsk_tree_t object.
@return Returns the number roots in this tree.
*/
tsk_size_t tsk_tree_get_num_roots(const tsk_tree_t *self);

/**
@brief Returns the leftmost root in this tree.

@rst
See the :ref:`sec_data_model_tree_roots` section for more information
on how the roots of a tree are defined.

This function is equivalent to ``tree.left_child[tree.virtual_root]``.
@endrst

@param self A pointer to an initialised tsk_tree_t object.
@return Returns the leftmost root in the tree.
*/
tsk_id_t tsk_tree_get_left_root(const tsk_tree_t *self);

/**
@brief Returns the rightmost root in this tree.

@rst
See the :ref:`sec_data_model_tree_roots` section for more information
on how the roots of a tree are defined.

This function is equivalent to ``tree.right_child[tree.virtual_root]``.
@endrst

@param self A pointer to an initialised tsk_tree_t object.
@return Returns the rightmost root in the tree.
*/
tsk_id_t tsk_tree_get_right_root(const tsk_tree_t *self);

/**
@brief Get the list of sites for this tree.

@rst
Gets the list of :c:data:`tsk_site_t` objects in the parent tree sequence
for which the position lies within this tree's genomic interval.

The memory pointed to by the ``sites`` parameter is managed by the
``tsk_tree_t`` object and must not be altered or freed by client code.

.. code-block:: c

    static void
    print_sites(const tsk_tree_t *tree)
    {
        int ret;
        tsk_size_t j, num_sites;
        const tsk_site_t *sites;

        ret = tsk_tree_get_sites(tree, &sites, &num_sites);
        check_tsk_error(ret);
        for (j = 0; j < num_sites; j++) {
            printf("position = %f\n", sites[j].position);
        }
    }

This is a constant time operation.

@endrst

@param self A pointer to a tsk_tree_t object.
@param sites The destination pointer for the list of sites.
@param sites_length A pointer to a tsk_size_t value in which the number
    of sites is stored.
@return 0 on success or a negative value on failure.
*/
int tsk_tree_get_sites(
    const tsk_tree_t *self, const tsk_site_t **sites, tsk_size_t *sites_length);

/**
@brief Return an upper bound on the number of nodes reachable
    from the roots of this tree.

@rst
This function provides an upper bound on the number of nodes that
can be reached in tree traversals, and is intended to be used
for memory allocation purposes. If ``num_nodes`` is the number
of nodes visited in a tree traversal from the
:ref:`virtual root<sec_data_model_tree_roots>`
(e.g., ``tsk_tree_preorder_from(tree, tree->virtual_root, nodes,
&num_nodes)``), the bound ``N`` returned here is guaranteed to
be greater than or equal to ``num_nodes``.

.. warning:: The precise value returned is not defined and should
    not be depended on, as it may change from version-to-version.

@endrst

@param self A pointer to a tsk_tree_t object.
@return An upper bound on the number nodes reachable from the roots
    of this tree, or zero if this tree has not been initialised.
*/
tsk_size_t tsk_tree_get_size_bound(const tsk_tree_t *self);

/**
@brief Print out the state of this tree to the specified stream.

This method is intended for debugging purposes and should not be used
in production code. The format of the output should **not** be depended
on and may change arbitrarily between versions.

@param self A pointer to a tsk_tree_t object.
@param out The stream to write the summary to.
*/
void tsk_tree_print_state(const tsk_tree_t *self, FILE *out);

/** @} */

/**
@defgroup TREE_API_NODE_QUERY_GROUP Node Queries
@{
*/

/**
@brief Returns the parent of the specified node.

@rst
Equivalent to ``tree.parent[u]`` with bounds checking for the node u.
Performance sensitive code which can guarantee that the node u is
valid should use the direct array access in preference to this method.
@endrst

@param self A pointer to a tsk_tree_t object.
@param u The tree node.
@param parent A tsk_id_t pointer to store the returned parent node.
@return 0 on success or a negative value on failure.
*/
int tsk_tree_get_parent(const tsk_tree_t *self, tsk_id_t u, tsk_id_t *parent);

/**
@brief Returns the time of the specified node.

@rst
Equivalent to ``tables->nodes.time[u]`` with bounds checking for the node u.
Performance sensitive code which can guarantee that the node u is
valid should use the direct array access in preference to this method,
for example:

.. code-block:: c

    static void
    print_times(const tsk_tree_t *tree)
    {
        int ret;
        tsk_size_t num_nodes, j;
        const double *node_time = tree->tree_sequence->tables->nodes.time;
        tsk_id_t *nodes = malloc(tsk_tree_get_size_bound(tree) * sizeof(*nodes));

        if (nodes == NULL) {
            errx(EXIT_FAILURE, "Out of memory");
        }
        ret = tsk_tree_preorder(tree, nodes, &num_nodes);
        check_tsk_error(ret);
        for (j = 0; j < num_nodes; j++) {
            printf("time = %f\n", node_time[nodes[j]]);
        }
        free(nodes);
    }

@endrst

@param self A pointer to a tsk_tree_t object.
@param u The tree node.
@param ret_time A double pointer to store the returned node time.
@return 0 on success or a negative value on failure.
*/
int tsk_tree_get_time(const tsk_tree_t *self, tsk_id_t u, double *ret_time);

/**
@brief Return number of nodes on the path from the specified node to root.

@rst
Return the number of nodes on the path from u to root, not including u.
The depth of a root is therefore zero.

As a special case, the depth of the
:ref:`virtual root <sec_data_model_tree_roots>` is defined as -1.
@endrst

@param self A pointer to a tsk_tree_t object.
@param u The tree node.
@param ret_depth An int pointer to store the returned node depth.
@return 0 on success or a negative value on failure.
*/
int tsk_tree_get_depth(const tsk_tree_t *self, tsk_id_t u, int *ret_depth);

/**
@brief Return the length of the branch ancestral to the specified node.

@rst
Return the length of the branch ancestral to the specified node.
Branch length is defined as difference between the time
of a node and its parent. The branch length of a root is zero.
@endrst

@param self A pointer to a tsk_tree_t object.
@param u The tree node.
@param ret_branch_length A double pointer to store the returned branch length.
@return 0 on success or a negative value on failure.
*/
int tsk_tree_get_branch_length(
    const tsk_tree_t *self, tsk_id_t u, double *ret_branch_length);

/**
@brief Computes the sum of the lengths of all branches reachable from
    the specified node, or from all roots if ``u=TSK_NULL``.

@rst
Return the total branch length in a particular subtree or of the
entire tree. If the specified node is :c:macro:`TSK_NULL` (or the
:ref:`virtual root<sec_data_model_tree_roots>`)
the sum of the lengths of all branches reachable from roots
is returned. Branch length is defined as difference between the time
of a node and its parent. The branch length of a root is zero.

Note that if the specified node is internal its branch length is
*not* included, so that, e.g., the total branch length of a
leaf node is zero.
@endrst

@param self A pointer to a tsk_tree_t object.
@param u The root of the subtree of interest, or ``TSK_NULL`` to return the
    total branch length of the tree.
@param ret_tbl A double pointer to store the returned total branch length.
@return 0 on success or a negative value on failure.
*/
int tsk_tree_get_total_branch_length(
    const tsk_tree_t *self, tsk_id_t u, double *ret_tbl);

/**
@brief Counts the number of samples in the subtree rooted at a node.

@rst
Returns the number of samples descending from a particular node,
including the node itself.

This is a constant time operation.
@endrst

@param self A pointer to a tsk_tree_t object.
@param u The tree node.
@param ret_num_samples A tsk_size_t pointer to store the returned
    number of samples.
@return 0 on success or a negative value on failure.
*/
int tsk_tree_get_num_samples(
    const tsk_tree_t *self, tsk_id_t u, tsk_size_t *ret_num_samples);

/**
@brief Compute the most recent common ancestor of two nodes.

@rst
If two nodes do not share a common ancestor in the current tree, the MRCA
node is :c:macro:`TSK_NULL`.
@endrst

@param self A pointer to a tsk_tree_t object.
@param u A tree node.
@param v A tree node.
@param mrca A tsk_id_t pointer to store the returned most recent common ancestor node.
@return 0 on success or a negative value on failure.
*/
int tsk_tree_get_mrca(const tsk_tree_t *self, tsk_id_t u, tsk_id_t v, tsk_id_t *mrca);

/**
@brief Returns true if u is a descendant of v.

@rst
Returns true if u and v are both valid nodes in the tree sequence
and v lies on the path from u to root, and false otherwise.

Any node is a descendant of itself.
@endrst

@param self A pointer to a tsk_tree_t object.
@param u The descendant node.
@param v The ancestral node.
@return true if u is a descendant of v, and false otherwise.
*/
bool tsk_tree_is_descendant(const tsk_tree_t *self, tsk_id_t u, tsk_id_t v);

/** @} */

/**
@defgroup TREE_API_TRAVERSAL_GROUP Traversal orders.
@{
*/

/**
@brief Fill an array with the nodes of this tree in preorder.

@rst
Populate an array with the nodes in this tree in preorder. The array
must be pre-allocated and be sufficiently large to hold the array
of nodes visited. The recommended approach is to use the
:c:func:`tsk_tree_get_size_bound` function, as in the following example:

.. code-block:: c

    static void
    print_preorder(tsk_tree_t *tree)
    {
        int ret;
        tsk_size_t num_nodes, j;
        tsk_id_t *nodes = malloc(tsk_tree_get_size_bound(tree) * sizeof(*nodes));

        if (nodes == NULL) {
            errx(EXIT_FAILURE, "Out of memory");
        }
        ret = tsk_tree_preorder(tree, nodes, &num_nodes);
        check_tsk_error(ret);
        for (j = 0; j < num_nodes; j++) {
            printf("Visit preorder %lld\n", (long long) nodes[j]);
        }
        free(nodes);
    }

.. seealso::
    See the :ref:`sec_c_api_examples_tree_traversals` section for
    more examples.

@endrst

@param self A pointer to a tsk_tree_t object.
@param nodes The tsk_id_t array to store nodes in. See notes above for
    details.
@param num_nodes A pointer to a tsk_size_t value where we store the number
    of nodes in the traversal.
@return 0 on success or a negative value on failure.
*/
int tsk_tree_preorder(const tsk_tree_t *self, tsk_id_t *nodes, tsk_size_t *num_nodes);

/**
@brief Fill an array with the nodes of this tree starting from a particular node.

@rst
As for :c:func:`tsk_tree_preorder` but starting the traversal at a particular node
(which will be the first node in the traversal list). The
:ref:`virtual root<sec_data_model_tree_roots>` is a valid input for this function
and will be treated like any other tree node. The value ``-1`` is a special case,
in which we visit all nodes reachable from the roots, and equivalent to
calling :c:func:`tsk_tree_preorder`.

See :c:func:`tsk_tree_preorder` for details the requirements for the ``nodes``
array.
@endrst

@param self A pointer to a tsk_tree_t object.
@param root The root of the subtree to traverse, or -1 to visit all nodes.
@param nodes The tsk_id_t array to store nodes in.
@param num_nodes A pointer to a tsk_size_t value where we store the number
    of nodes in the traversal.
@return 0 on success or a negative value on failure.
*/
int tsk_tree_preorder_from(
    const tsk_tree_t *self, tsk_id_t root, tsk_id_t *nodes, tsk_size_t *num_nodes);

/**
@brief Fill an array with the nodes of this tree in postorder.

@rst
Populate an array with the nodes in this tree in postorder. The array
must be pre-allocated and be sufficiently large to hold the array
of nodes visited. The recommended approach is to use the
:c:func:`tsk_tree_get_size_bound` function, as in the following example:

.. code-block:: c

    static void
    print_postorder(tsk_tree_t *tree)
    {
        int ret;
        tsk_size_t num_nodes, j;
        tsk_id_t *nodes = malloc(tsk_tree_get_size_bound(tree) * sizeof(*nodes));

        if (nodes == NULL) {
            errx(EXIT_FAILURE, "Out of memory");
        }
        ret = tsk_tree_postorder(tree, nodes, &num_nodes);
        check_tsk_error(ret);
        for (j = 0; j < num_nodes; j++) {
            printf("Visit postorder %lld\n", (long long) nodes[j]);
        }
        free(nodes);
    }

.. seealso::
    See the :ref:`sec_c_api_examples_tree_traversals` section for
    more examples.

@endrst

@param self A pointer to a tsk_tree_t object.
@param nodes The tsk_id_t array to store nodes in. See notes above for
    details.
@param num_nodes A pointer to a tsk_size_t value where we store the number
    of nodes in the traversal.
@return 0 on success or a negative value on failure.
*/
int tsk_tree_postorder(const tsk_tree_t *self, tsk_id_t *nodes, tsk_size_t *num_nodes);

/**
@brief Fill an array with the nodes of this tree starting from a particular node.

@rst
As for :c:func:`tsk_tree_postorder` but starting the traversal at a particular node
(which will be the last node in the traversal list). The
:ref:`virtual root<sec_data_model_tree_roots>` is a valid input for this function
and will be treated like any other tree node. The value ``-1`` is a special case,
in which we visit all nodes reachable from the roots, and equivalent to
calling :c:func:`tsk_tree_postorder`.

See :c:func:`tsk_tree_postorder` for details the requirements for the ``nodes``
array.
@endrst

@param self A pointer to a tsk_tree_t object.
@param root The root of the subtree to traverse, or -1 to visit all nodes.
@param nodes The tsk_id_t array to store nodes in. See
    :c:func:`tsk_tree_postorder` for more details.
@param num_nodes A pointer to a tsk_size_t value where we store the number
    of nodes in the traversal.
@return 0 on success or a negative value on failure.
*/
int tsk_tree_postorder_from(
    const tsk_tree_t *self, tsk_id_t root, tsk_id_t *nodes, tsk_size_t *num_nodes);

/** @} */

/* Undocumented for now */

int tsk_tree_preorder_samples_from(
    const tsk_tree_t *self, tsk_id_t root, tsk_id_t *nodes, tsk_size_t *num_nodes);

int tsk_tree_set_root_threshold(tsk_tree_t *self, tsk_size_t root_threshold);
tsk_size_t tsk_tree_get_root_threshold(const tsk_tree_t *self);

bool tsk_tree_has_sample_counts(const tsk_tree_t *self);
bool tsk_tree_has_sample_lists(const tsk_tree_t *self);

int tsk_tree_get_num_tracked_samples(
    const tsk_tree_t *self, tsk_id_t u, tsk_size_t *num_tracked_samples);
int tsk_tree_set_tracked_samples(
    tsk_tree_t *self, tsk_size_t num_tracked_samples, const tsk_id_t *tracked_samples);
int tsk_tree_track_descendant_samples(tsk_tree_t *self, tsk_id_t node);

typedef struct {
    tsk_id_t node;
    tsk_id_t parent;
    int32_t state;
} tsk_state_transition_t;

int tsk_tree_map_mutations(tsk_tree_t *self, int32_t *genotypes, double *cost_matrix,
    tsk_flags_t options, int32_t *ancestral_state, tsk_size_t *num_transitions,
    tsk_state_transition_t **transitions);

int tsk_tree_kc_distance(
    const tsk_tree_t *self, const tsk_tree_t *other, double lambda, double *result);

/* Don't document these balance metrics for now so it doesn't get in the way of
 * C API 1.0, but should be straightforward to document based on Python docs. */
int tsk_tree_sackin_index(const tsk_tree_t *self, tsk_size_t *result);
int tsk_tree_colless_index(const tsk_tree_t *self, tsk_size_t *result);
int tsk_tree_b1_index(const tsk_tree_t *self, double *result);
/* NOTE: if we document this as part of the C API we'll have to be more careful
 * about the error behaviour on bad log bases. At the moment we're just returning
 * the resulting value which can be nan, inf etc, but some surprising results
 * happen like a base 0 seems to return a finite value. */
int tsk_tree_b2_index(const tsk_tree_t *self, double base, double *result);

int tsk_tree_num_lineages(const tsk_tree_t *self, double t, tsk_size_t *result);

/* Things to consider removing: */

/* This is redundant, really */
bool tsk_tree_is_sample(const tsk_tree_t *self, tsk_id_t u);

/* Not terribly useful, since the definition is
 * return (self->tree_sequence == other->tree_sequence) && (self->index == other->index)
 * Remove?
 */
bool tsk_tree_equals(const tsk_tree_t *self, const tsk_tree_t *other);

int tsk_tree_position_init(
    tsk_tree_position_t *self, const tsk_treeseq_t *tree_sequence, tsk_flags_t options);
int tsk_tree_position_free(tsk_tree_position_t *self);
int tsk_tree_position_print_state(const tsk_tree_position_t *self, FILE *out);
bool tsk_tree_position_next(tsk_tree_position_t *self);
bool tsk_tree_position_prev(tsk_tree_position_t *self);
int tsk_tree_position_seek_forward(tsk_tree_position_t *self, tsk_id_t index);
int tsk_tree_position_seek_backward(tsk_tree_position_t *self, tsk_id_t index);

#ifdef __cplusplus
}
#endif
#endif
