/*
 * MIT License
 *
 * Copyright (c) 2019-2020 Tskit Developers
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

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include <tskit/trees.h>

/* ======================================================== *
 * tree sequence
 * ======================================================== */

static void
tsk_treeseq_check_state(const tsk_treeseq_t *self)
{
    size_t j;
    tsk_size_t k, l;
    tsk_site_t site;
    tsk_id_t site_id = 0;

    for (j = 0; j < self->num_trees; j++) {
        for (k = 0; k < self->tree_sites_length[j]; k++) {
            site = self->tree_sites[j][k];
            tsk_bug_assert(site.id == site_id);
            site_id++;
            for (l = 0; l < site.mutations_length; l++) {
                tsk_bug_assert(site.mutations[l].site == site.id);
            }
        }
    }
}

void
tsk_treeseq_print_state(const tsk_treeseq_t *self, FILE *out)
{
    size_t j;
    tsk_size_t k, l, m;
    tsk_site_t site;

    fprintf(out, "tree_sequence state\n");
    fprintf(out, "num_trees = %d\n", (int) self->num_trees);
    fprintf(out, "samples = (%d)\n", (int) self->num_samples);
    for (j = 0; j < self->num_samples; j++) {
        fprintf(out, "\t%d\n", (int) self->samples[j]);
    }
    tsk_table_collection_print_state(self->tables, out);
    fprintf(out, "tree_sites = \n");
    for (j = 0; j < self->num_trees; j++) {
        fprintf(out, "tree %d\t%d sites\n", (int) j, self->tree_sites_length[j]);
        for (k = 0; k < self->tree_sites_length[j]; k++) {
            site = self->tree_sites[j][k];
            fprintf(
                out, "\tsite %d pos = %f ancestral state = ", site.id, site.position);
            for (l = 0; l < site.ancestral_state_length; l++) {
                fprintf(out, "%c", site.ancestral_state[l]);
            }
            fprintf(out, " %d mutations\n", site.mutations_length);
            for (l = 0; l < site.mutations_length; l++) {
                fprintf(out,
                    "\t\tmutation %d node = %d derived_state = ", site.mutations[l].id,
                    site.mutations[l].node);
                for (m = 0; m < site.mutations[l].derived_state_length; m++) {
                    fprintf(out, "%c", site.mutations[l].derived_state[m]);
                }
                fprintf(out, "\n");
            }
        }
    }
    tsk_treeseq_check_state(self);
}

int
tsk_treeseq_free(tsk_treeseq_t *self)
{
    if (self->tables != NULL) {
        tsk_table_collection_free(self->tables);
    }
    tsk_safe_free(self->tables);
    tsk_safe_free(self->samples);
    tsk_safe_free(self->sample_index_map);
    tsk_safe_free(self->breakpoints);
    tsk_safe_free(self->tree_sites);
    tsk_safe_free(self->tree_sites_length);
    tsk_safe_free(self->tree_sites_mem);
    tsk_safe_free(self->site_mutations_mem);
    tsk_safe_free(self->site_mutations_length);
    tsk_safe_free(self->site_mutations);
    tsk_safe_free(self->individual_nodes_mem);
    tsk_safe_free(self->individual_nodes_length);
    tsk_safe_free(self->individual_nodes);
    return 0;
}

static int
tsk_treeseq_init_sites(tsk_treeseq_t *self)
{
    tsk_id_t j, k;
    int ret = 0;
    size_t offset = 0;
    const tsk_size_t num_mutations = self->tables->mutations.num_rows;
    const tsk_size_t num_sites = self->tables->sites.num_rows;
    const tsk_id_t *restrict mutation_site = self->tables->mutations.site;

    self->site_mutations_mem = malloc(num_mutations * sizeof(tsk_mutation_t));
    self->site_mutations_length = malloc(num_sites * sizeof(tsk_size_t));
    self->site_mutations = malloc(num_sites * sizeof(tsk_mutation_t *));
    self->tree_sites_mem = malloc(num_sites * sizeof(tsk_site_t));
    if (self->site_mutations_mem == NULL || self->site_mutations_length == NULL
        || self->site_mutations == NULL || self->tree_sites_mem == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }

    for (k = 0; k < (tsk_id_t) num_mutations; k++) {
        ret = tsk_treeseq_get_mutation(self, k, self->site_mutations_mem + k);
        if (ret != 0) {
            goto out;
        }
    }
    k = 0;
    for (j = 0; j < (tsk_id_t) num_sites; j++) {
        self->site_mutations[j] = self->site_mutations_mem + offset;
        self->site_mutations_length[j] = 0;
        /* Go through all mutations for this site */
        while (k < (tsk_id_t) num_mutations && mutation_site[k] == j) {
            self->site_mutations_length[j]++;
            offset++;
            k++;
        }
        ret = tsk_treeseq_get_site(self, j, self->tree_sites_mem + j);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

static int
tsk_treeseq_init_individuals(tsk_treeseq_t *self)
{
    int ret = 0;
    tsk_id_t node;
    tsk_id_t ind;
    tsk_size_t offset = 0;
    tsk_size_t total_node_refs = 0;
    tsk_size_t *node_count = NULL;
    tsk_id_t *node_array;
    const size_t num_inds = self->tables->individuals.num_rows;
    const size_t num_nodes = self->tables->nodes.num_rows;
    const tsk_id_t *restrict node_individual = self->tables->nodes.individual;

    // First find number of nodes per individual
    self->individual_nodes_length = calloc(TSK_MAX(1, num_inds), sizeof(tsk_size_t));
    node_count = calloc(TSK_MAX(1, num_inds), sizeof(size_t));

    if (self->individual_nodes_length == NULL || node_count == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }

    for (node = 0; node < (tsk_id_t) num_nodes; node++) {
        ind = node_individual[node];
        if (ind != TSK_NULL) {
            self->individual_nodes_length[ind]++;
            total_node_refs++;
        }
    }

    self->individual_nodes_mem
        = malloc(TSK_MAX(1, total_node_refs) * sizeof(tsk_node_t));
    self->individual_nodes = malloc(TSK_MAX(1, num_inds) * sizeof(tsk_node_t *));
    if (self->individual_nodes_mem == NULL || self->individual_nodes == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }

    /* Now fill in the node IDs */
    for (ind = 0; ind < (tsk_id_t) num_inds; ind++) {
        self->individual_nodes[ind] = self->individual_nodes_mem + offset;
        offset += self->individual_nodes_length[ind];
    }
    for (node = 0; node < (tsk_id_t) num_nodes; node++) {
        ind = node_individual[node];
        if (ind != TSK_NULL) {
            node_array = self->individual_nodes[ind];
            tsk_bug_assert(node_array - self->individual_nodes_mem
                           < (tsk_id_t)(total_node_refs - node_count[ind]));
            node_array[node_count[ind]] = node;
            node_count[ind] += 1;
        }
    }
out:
    tsk_safe_free(node_count);
    return ret;
}

/* Initialises memory associated with the trees.
 */
static int
tsk_treeseq_init_trees(tsk_treeseq_t *self)
{
    int ret = TSK_ERR_GENERIC;
    size_t j, k, tree_index;
    tsk_id_t site;
    double tree_left, tree_right;
    const double sequence_length = self->tables->sequence_length;
    const tsk_id_t num_sites = (tsk_id_t) self->tables->sites.num_rows;
    const size_t num_edges = self->tables->edges.num_rows;
    const double *restrict site_position = self->tables->sites.position;
    const tsk_id_t *restrict I = self->tables->indexes.edge_insertion_order;
    const tsk_id_t *restrict O = self->tables->indexes.edge_removal_order;
    const double *restrict edge_right = self->tables->edges.right;
    const double *restrict edge_left = self->tables->edges.left;
    /* Avoid malloc(0) */
    size_t num_trees_alloc = self->num_trees + 1;

    self->tree_sites_length = malloc(num_trees_alloc * sizeof(*self->tree_sites_length));
    self->tree_sites = malloc(num_trees_alloc * sizeof(*self->tree_sites));
    self->breakpoints = malloc(num_trees_alloc * sizeof(*self->breakpoints));
    if (self->tree_sites == NULL || self->tree_sites_length == NULL
        || self->breakpoints == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    memset(
        self->tree_sites_length, 0, self->num_trees * sizeof(*self->tree_sites_length));
    memset(self->tree_sites, 0, self->num_trees * sizeof(*self->tree_sites));

    tree_left = 0;
    tree_right = sequence_length;
    tree_index = 0;
    site = 0;
    j = 0;
    k = 0;
    while (j < num_edges || tree_left < sequence_length) {
        self->breakpoints[tree_index] = tree_left;
        while (k < num_edges && edge_right[O[k]] == tree_left) {
            k++;
        }
        while (j < num_edges && edge_left[I[j]] == tree_left) {
            j++;
        }
        tree_right = sequence_length;
        if (j < num_edges) {
            tree_right = TSK_MIN(tree_right, edge_left[I[j]]);
        }
        if (k < num_edges) {
            tree_right = TSK_MIN(tree_right, edge_right[O[k]]);
        }
        self->tree_sites[tree_index] = self->tree_sites_mem + site;
        while (site < num_sites && site_position[site] < tree_right) {
            self->tree_sites_length[tree_index]++;
            site++;
        }
        tree_left = tree_right;
        tree_index++;
    }
    tsk_bug_assert(site == num_sites);
    tsk_bug_assert(tree_index == self->num_trees);
    self->breakpoints[tree_index] = tree_right;
    ret = 0;
out:
    return ret;
}

static int
tsk_treeseq_init_nodes(tsk_treeseq_t *self)
{
    size_t j, k;
    size_t num_nodes = self->tables->nodes.num_rows;
    const uint32_t *restrict node_flags = self->tables->nodes.flags;
    int ret = 0;

    /* Determine the sample size */
    self->num_samples = 0;
    for (j = 0; j < num_nodes; j++) {
        if (!!(node_flags[j] & TSK_NODE_IS_SAMPLE)) {
            self->num_samples++;
        }
    }
    /* TODO raise an error if < 2 samples?? */
    self->samples = malloc(self->num_samples * sizeof(tsk_id_t));
    self->sample_index_map = malloc(num_nodes * sizeof(tsk_id_t));
    if (self->samples == NULL || self->sample_index_map == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    k = 0;
    for (j = 0; j < num_nodes; j++) {
        self->sample_index_map[j] = -1;
        if (!!(node_flags[j] & TSK_NODE_IS_SAMPLE)) {
            self->samples[k] = (tsk_id_t) j;
            self->sample_index_map[j] = (tsk_id_t) k;
            k++;
        }
    }
    tsk_bug_assert(k == self->num_samples);
out:
    return ret;
}

/* TODO we need flags to be able to control how the input table is used.
 * - The default behaviour is to take a copy. TSK_BUILD_INDEXES is allowed
 *   in this case because we have an independent copy.
 * - Need an option to take 'ownership' of the tables so that we keep the
 *   tables and free them at the end of the treeseq's lifetime. This will be
 *   used in tsk_treeseq_load below where we can avoid copying the tree sequence.
 * - We should also allow a read-only "borrowed reference" where we use the
 *   tables directly, but don't free it at the end.
 */
int TSK_WARN_UNUSED
tsk_treeseq_init(
    tsk_treeseq_t *self, const tsk_table_collection_t *tables, tsk_flags_t options)
{
    int ret = 0;
    tsk_id_t num_trees;

    memset(self, 0, sizeof(*self));
    if (tables == NULL) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    self->tables = malloc(sizeof(*self->tables));
    if (self->tables == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }

    /* Note that this copy reinstates metadata for a table collection with
     * TSK_NO_EDGE_METADATA. Otherwise a table without metadata would
     * crash tsk_diff_iter_next. This is something we will need to
     * watch out for when we take a read-only view of the input table
     * rather than a copy. */
    ret = tsk_table_collection_copy(tables, self->tables, 0);

    if (ret != 0) {
        goto out;
    }
    if (!!(options & TSK_BUILD_INDEXES)) {
        ret = tsk_table_collection_build_index(self->tables, 0);
        if (ret != 0) {
            goto out;
        }
    }
    num_trees = tsk_table_collection_check_integrity(self->tables, TSK_CHECK_TREES);
    if (num_trees < 0) {
        ret = (int) num_trees;
        goto out;
    }
    self->num_trees = (tsk_size_t) num_trees;

    /* This is a hack to workaround the fact we're copying the tables here.
     * In general, we don't want the file_uuid to be copied, as this should
     * only be present if the tables are genuinely backed by a file and in
     * read-only mode (which we also need to do). So, we copy the file_uuid
     * into the local copy of the table for now until we have proper read-only
     * access to the tables set up, where any attempts to modify the tables
     * will fail. */
    if (tables->file_uuid != NULL) {
        self->tables->file_uuid = malloc(TSK_UUID_SIZE + 1);
        if (self->tables->file_uuid == NULL) {
            ret = TSK_ERR_NO_MEMORY;
            goto out;
        }
        memcpy(self->tables->file_uuid, tables->file_uuid, TSK_UUID_SIZE + 1);
    }

    ret = tsk_treeseq_init_nodes(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_init_sites(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_init_individuals(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_init_trees(self);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_treeseq_copy_tables(
    const tsk_treeseq_t *self, tsk_table_collection_t *tables, tsk_flags_t options)
{
    return tsk_table_collection_copy(self->tables, tables, options);
}

int TSK_WARN_UNUSED
tsk_treeseq_load(
    tsk_treeseq_t *self, const char *filename, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_table_collection_t tables;

    /* Need to make sure that we're zero'd out in case of error */
    memset(self, 0, sizeof(*self));
    ret = tsk_table_collection_load(&tables, filename, 0);
    if (ret != 0) {
        goto out;
    }
    /* TODO the implementation is wasteful here, as we don't need to allocate
     * a new table here but could load directly into the main table instead.
     * See notes on the owned reference for treeseq_alloc above.
     */
    ret = tsk_treeseq_init(self, &tables, 0);
    if (ret != 0) {
        goto out;
    }
out:
    tsk_table_collection_free(&tables);
    return ret;
}

int TSK_WARN_UNUSED
tsk_treeseq_loadf(tsk_treeseq_t *self, FILE *file, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_table_collection_t tables;

    /* Need to make sure that we're zero'd out in case of error */
    memset(self, 0, sizeof(*self));
    ret = tsk_table_collection_loadf(&tables, file, 0);
    if (ret != 0) {
        goto out;
    }
    /* TODO the implementation is wasteful here, as we don't need to allocate
     * a new table here but could load directly into the main table instead.
     * See notes on the owned reference for treeseq_alloc above.
     */
    ret = tsk_treeseq_init(self, &tables, 0);
    if (ret != 0) {
        goto out;
    }
out:
    tsk_table_collection_free(&tables);
    return ret;
}

int TSK_WARN_UNUSED
tsk_treeseq_dump(tsk_treeseq_t *self, const char *filename, tsk_flags_t options)
{
    return tsk_table_collection_dump(self->tables, filename, options);
}

int TSK_WARN_UNUSED
tsk_treeseq_dumpf(tsk_treeseq_t *self, FILE *file, tsk_flags_t options)
{
    return tsk_table_collection_dumpf(self->tables, file, options);
}

/* Simple attribute getters */

const char *
tsk_treeseq_get_metadata(const tsk_treeseq_t *self)
{
    return self->tables->metadata;
}

tsk_size_t
tsk_treeseq_get_metadata_length(const tsk_treeseq_t *self)
{
    return self->tables->metadata_length;
}

const char *
tsk_treeseq_get_metadata_schema(const tsk_treeseq_t *self)
{
    return self->tables->metadata_schema;
}

tsk_size_t
tsk_treeseq_get_metadata_schema_length(const tsk_treeseq_t *self)
{
    return self->tables->metadata_schema_length;
}

double
tsk_treeseq_get_sequence_length(const tsk_treeseq_t *self)
{
    return self->tables->sequence_length;
}

const char *
tsk_treeseq_get_file_uuid(const tsk_treeseq_t *self)
{
    return self->tables->file_uuid;
}

tsk_size_t
tsk_treeseq_get_num_samples(const tsk_treeseq_t *self)
{
    return self->num_samples;
}

tsk_size_t
tsk_treeseq_get_num_nodes(const tsk_treeseq_t *self)
{
    return self->tables->nodes.num_rows;
}

tsk_size_t
tsk_treeseq_get_num_edges(const tsk_treeseq_t *self)
{
    return self->tables->edges.num_rows;
}

tsk_size_t
tsk_treeseq_get_num_migrations(const tsk_treeseq_t *self)
{
    return self->tables->migrations.num_rows;
}

tsk_size_t
tsk_treeseq_get_num_sites(const tsk_treeseq_t *self)
{
    return self->tables->sites.num_rows;
}

tsk_size_t
tsk_treeseq_get_num_mutations(const tsk_treeseq_t *self)
{
    return self->tables->mutations.num_rows;
}

tsk_size_t
tsk_treeseq_get_num_populations(const tsk_treeseq_t *self)
{
    return self->tables->populations.num_rows;
}

tsk_size_t
tsk_treeseq_get_num_individuals(const tsk_treeseq_t *self)
{
    return self->tables->individuals.num_rows;
}

tsk_size_t
tsk_treeseq_get_num_provenances(const tsk_treeseq_t *self)
{
    return self->tables->provenances.num_rows;
}

tsk_size_t
tsk_treeseq_get_num_trees(const tsk_treeseq_t *self)
{
    return self->num_trees;
}

const double *
tsk_treeseq_get_breakpoints(const tsk_treeseq_t *self)
{
    return self->breakpoints;
}

const tsk_id_t *
tsk_treeseq_get_samples(const tsk_treeseq_t *self)
{
    return self->samples;
}

const tsk_id_t *
tsk_treeseq_get_sample_index_map(const tsk_treeseq_t *self)
{
    return self->sample_index_map;
}

bool
tsk_treeseq_is_sample(const tsk_treeseq_t *self, tsk_id_t u)
{
    bool ret = false;

    if (u >= 0 && u < (tsk_id_t) self->tables->nodes.num_rows) {
        ret = !!(self->tables->nodes.flags[u] & TSK_NODE_IS_SAMPLE);
    }
    return ret;
}

/* Stats functions */

#define GET_2D_ROW(array, row_len, row) (array + (((size_t)(row_len)) * (size_t) row))

static inline double *
GET_3D_ROW(
    double *base, size_t num_nodes, size_t output_dim, size_t window_index, tsk_id_t u)
{
    size_t offset = window_index * num_nodes * output_dim + ((size_t) u) * output_dim;
    return base + offset;
}

/* Increments the n-dimensional array with the specified shape by the specified value at
 * the specified coordinate. */
static inline void
increment_nd_array_value(double *array, tsk_size_t n, const tsk_size_t *shape,
    const tsk_size_t *coordinate, double value)
{
    size_t offset = 0;
    size_t product = 1;
    int k;

    for (k = (int) n - 1; k >= 0; k--) {
        tsk_bug_assert(coordinate[k] < shape[k]);
        offset += coordinate[k] * product;
        product *= shape[k];
    }
    array[offset] += value;
}

/* TODO flatten the reference sets input here and follow the same pattern used
 * in diversity, divergence, etc. */
int TSK_WARN_UNUSED
tsk_treeseq_genealogical_nearest_neighbours(const tsk_treeseq_t *self,
    const tsk_id_t *focal, size_t num_focal, const tsk_id_t *const *reference_sets,
    const size_t *reference_set_size, size_t num_reference_sets,
    tsk_flags_t TSK_UNUSED(options), double *ret_array)
{
    int ret = 0;
    tsk_id_t u, v, p;
    size_t j;
    /* TODO It's probably not worth bothering with the int16_t here. */
    int16_t k, focal_reference_set;
    /* We use the K'th element of the array for the total. */
    const int16_t K = (int16_t)(num_reference_sets + 1);
    size_t num_nodes = self->tables->nodes.num_rows;
    const tsk_id_t num_edges = (tsk_id_t) self->tables->edges.num_rows;
    const tsk_id_t *restrict I = self->tables->indexes.edge_insertion_order;
    const tsk_id_t *restrict O = self->tables->indexes.edge_removal_order;
    const double *restrict edge_left = self->tables->edges.left;
    const double *restrict edge_right = self->tables->edges.right;
    const tsk_id_t *restrict edge_parent = self->tables->edges.parent;
    const tsk_id_t *restrict edge_child = self->tables->edges.child;
    const double sequence_length = self->tables->sequence_length;
    tsk_id_t tj, tk, h;
    double left, right, *A_row, scale, tree_length;
    tsk_id_t *restrict parent = malloc(num_nodes * sizeof(*parent));
    double *restrict length = calloc(num_focal, sizeof(*length));
    uint32_t *restrict ref_count = calloc(((size_t) K) * num_nodes, sizeof(*ref_count));
    int16_t *restrict reference_set_map = malloc(num_nodes * sizeof(*reference_set_map));
    uint32_t *restrict row = NULL;
    uint32_t *restrict child_row = NULL;
    uint32_t total, delta;

    /* We support a max of 8K focal sets */
    if (num_reference_sets == 0 || num_reference_sets > (INT16_MAX - 1)) {
        /* TODO: more specific error */
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    if (parent == NULL || ref_count == NULL || reference_set_map == NULL
        || length == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }

    memset(parent, 0xff, num_nodes * sizeof(*parent));
    memset(reference_set_map, 0xff, num_nodes * sizeof(*reference_set_map));
    memset(ret_array, 0, num_focal * num_reference_sets * sizeof(*ret_array));

    total = 0; /* keep the compiler happy */

    /* Set the initial conditions and check the input. */
    for (k = 0; k < (int16_t) num_reference_sets; k++) {
        for (j = 0; j < reference_set_size[k]; j++) {
            u = reference_sets[k][j];
            if (u < 0 || u >= (tsk_id_t) num_nodes) {
                ret = TSK_ERR_NODE_OUT_OF_BOUNDS;
                goto out;
            }
            if (reference_set_map[u] != TSK_NULL) {
                /* FIXME Technically inaccurate here: duplicate focal not sample */
                ret = TSK_ERR_DUPLICATE_SAMPLE;
                goto out;
            }
            reference_set_map[u] = k;
            row = GET_2D_ROW(ref_count, K, u);
            row[k] = 1;
            /* Also set the count for the total among all sets */
            row[K - 1] = 1;
        }
    }
    for (j = 0; j < num_focal; j++) {
        u = focal[j];
        if (u < 0 || u >= (tsk_id_t) num_nodes) {
            ret = TSK_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
    }

    /* Iterate over the trees */
    tj = 0;
    tk = 0;
    left = 0;
    while (tj < num_edges || left < sequence_length) {
        while (tk < num_edges && edge_right[O[tk]] == left) {
            h = O[tk];
            tk++;
            u = edge_child[h];
            v = edge_parent[h];
            parent[u] = TSK_NULL;
            child_row = GET_2D_ROW(ref_count, K, u);
            while (v != TSK_NULL) {
                row = GET_2D_ROW(ref_count, K, v);
                for (k = 0; k < K; k++) {
                    row[k] -= child_row[k];
                }
                v = parent[v];
            }
        }
        while (tj < num_edges && edge_left[I[tj]] == left) {
            h = I[tj];
            tj++;
            u = edge_child[h];
            v = edge_parent[h];
            parent[u] = v;
            child_row = GET_2D_ROW(ref_count, K, u);
            while (v != TSK_NULL) {
                row = GET_2D_ROW(ref_count, K, v);
                for (k = 0; k < K; k++) {
                    row[k] += child_row[k];
                }
                v = parent[v];
            }
        }
        right = sequence_length;
        if (tj < num_edges) {
            right = TSK_MIN(right, edge_left[I[tj]]);
        }
        if (tk < num_edges) {
            right = TSK_MIN(right, edge_right[O[tk]]);
        }

        tree_length = right - left;
        /* Process this tree */
        for (j = 0; j < num_focal; j++) {
            u = focal[j];
            focal_reference_set = reference_set_map[u];
            delta = focal_reference_set != -1;
            p = u;
            while (p != TSK_NULL) {
                row = GET_2D_ROW(ref_count, K, p);
                total = row[K - 1];
                if (total > delta) {
                    break;
                }
                p = parent[p];
            }
            if (p != TSK_NULL) {
                length[j] += tree_length;
                scale = tree_length / (total - delta);
                A_row = GET_2D_ROW(ret_array, num_reference_sets, j);
                for (k = 0; k < K - 1; k++) {
                    A_row[k] += row[k] * scale;
                }
                if (focal_reference_set != -1) {
                    /* Remove the contribution for the reference set u belongs to and
                     * insert the correct value. The long-hand version is
                     * A_row[k] = A_row[k] - row[k] * scale + (row[k] - 1) * scale;
                     * which cancels to give: */
                    A_row[focal_reference_set] -= scale;
                }
            }
        }

        /* Move on to the next tree */
        left = right;
    }

    /* Divide by the accumulated length for each node to normalise */
    for (j = 0; j < num_focal; j++) {
        A_row = GET_2D_ROW(ret_array, num_reference_sets, j);
        if (length[j] > 0) {
            for (k = 0; k < K - 1; k++) {
                A_row[k] /= length[j];
            }
        }
    }
out:
    /* Can't use msp_safe_free here because of restrict */
    if (parent != NULL) {
        free(parent);
    }
    if (ref_count != NULL) {
        free(ref_count);
    }
    if (reference_set_map != NULL) {
        free(reference_set_map);
    }
    if (length != NULL) {
        free(length);
    }
    return ret;
}

int TSK_WARN_UNUSED
tsk_treeseq_mean_descendants(const tsk_treeseq_t *self,
    const tsk_id_t *const *reference_sets, const size_t *reference_set_size,
    size_t num_reference_sets, tsk_flags_t TSK_UNUSED(options), double *ret_array)
{
    int ret = 0;
    tsk_id_t u, v;
    size_t j;
    int32_t k;
    /* We use the K'th element of the array for the total. */
    const int32_t K = (int32_t)(num_reference_sets + 1);
    size_t num_nodes = self->tables->nodes.num_rows;
    const tsk_id_t num_edges = (tsk_id_t) self->tables->edges.num_rows;
    const tsk_id_t *restrict I = self->tables->indexes.edge_insertion_order;
    const tsk_id_t *restrict O = self->tables->indexes.edge_removal_order;
    const double *restrict edge_left = self->tables->edges.left;
    const double *restrict edge_right = self->tables->edges.right;
    const tsk_id_t *restrict edge_parent = self->tables->edges.parent;
    const tsk_id_t *restrict edge_child = self->tables->edges.child;
    const double sequence_length = self->tables->sequence_length;
    tsk_id_t tj, tk, h;
    double left, right, length, *restrict C_row;
    tsk_id_t *restrict parent = malloc(num_nodes * sizeof(*parent));
    uint32_t *restrict ref_count = calloc(num_nodes * ((size_t) K), sizeof(*ref_count));
    double *restrict last_update = calloc(num_nodes, sizeof(*last_update));
    double *restrict total_length = calloc(num_nodes, sizeof(*total_length));
    uint32_t *restrict row, *restrict child_row;

    if (num_reference_sets == 0 || num_reference_sets > (INT32_MAX - 1)) {
        /* TODO: more specific error */
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    if (parent == NULL || ref_count == NULL || last_update == NULL
        || total_length == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    /* TODO add check for duplicate values in the reference sets */

    memset(parent, 0xff, num_nodes * sizeof(*parent));
    memset(ret_array, 0, num_nodes * num_reference_sets * sizeof(*ret_array));

    /* Set the initial conditions and check the input. */
    for (k = 0; k < (int32_t) num_reference_sets; k++) {
        for (j = 0; j < reference_set_size[k]; j++) {
            u = reference_sets[k][j];
            if (u < 0 || u >= (tsk_id_t) num_nodes) {
                ret = TSK_ERR_NODE_OUT_OF_BOUNDS;
                goto out;
            }
            row = GET_2D_ROW(ref_count, K, u);
            row[k] = 1;
            /* Also set the count for the total among all sets */
            row[K - 1] = 1;
        }
    }

    /* Iterate over the trees */
    tj = 0;
    tk = 0;
    left = 0;
    while (tj < num_edges || left < sequence_length) {
        while (tk < num_edges && edge_right[O[tk]] == left) {
            h = O[tk];
            tk++;
            u = edge_child[h];
            v = edge_parent[h];
            parent[u] = TSK_NULL;
            child_row = GET_2D_ROW(ref_count, K, u);
            while (v != TSK_NULL) {
                row = GET_2D_ROW(ref_count, K, v);
                if (last_update[v] != left) {
                    if (row[K - 1] > 0) {
                        length = left - last_update[v];
                        C_row = GET_2D_ROW(ret_array, num_reference_sets, v);
                        for (k = 0; k < (int32_t) num_reference_sets; k++) {
                            C_row[k] += length * row[k];
                        }
                        total_length[v] += length;
                    }
                    last_update[v] = left;
                }
                for (k = 0; k < K; k++) {
                    row[k] -= child_row[k];
                }
                v = parent[v];
            }
        }
        while (tj < num_edges && edge_left[I[tj]] == left) {
            h = I[tj];
            tj++;
            u = edge_child[h];
            v = edge_parent[h];
            parent[u] = v;
            child_row = GET_2D_ROW(ref_count, K, u);
            while (v != TSK_NULL) {
                row = GET_2D_ROW(ref_count, K, v);
                if (last_update[v] != left) {
                    if (row[K - 1] > 0) {
                        length = left - last_update[v];
                        C_row = GET_2D_ROW(ret_array, num_reference_sets, v);
                        for (k = 0; k < (int32_t) num_reference_sets; k++) {
                            C_row[k] += length * row[k];
                        }
                        total_length[v] += length;
                    }
                    last_update[v] = left;
                }
                for (k = 0; k < K; k++) {
                    row[k] += child_row[k];
                }
                v = parent[v];
            }
        }
        right = sequence_length;
        if (tj < num_edges) {
            right = TSK_MIN(right, edge_left[I[tj]]);
        }
        if (tk < num_edges) {
            right = TSK_MIN(right, edge_right[O[tk]]);
        }
        left = right;
    }

    /* Add the stats for the last tree and divide by the total length that
     * each node was an ancestor to > 0 of the reference nodes. */
    for (v = 0; v < (tsk_id_t) num_nodes; v++) {
        row = GET_2D_ROW(ref_count, K, v);
        C_row = GET_2D_ROW(ret_array, num_reference_sets, v);
        if (row[K - 1] > 0) {
            length = sequence_length - last_update[v];
            total_length[v] += length;
            for (k = 0; k < (int32_t) num_reference_sets; k++) {
                C_row[k] += length * row[k];
            }
        }
        if (total_length[v] > 0) {
            length = total_length[v];
            for (k = 0; k < (int32_t) num_reference_sets; k++) {
                C_row[k] /= length;
            }
        }
    }

out:
    /* Can't use msp_safe_free here because of restrict */
    if (parent != NULL) {
        free(parent);
    }
    if (ref_count != NULL) {
        free(ref_count);
    }
    if (last_update != NULL) {
        free(last_update);
    }
    if (total_length != NULL) {
        free(total_length);
    }
    return ret;
}

/***********************************
 * General stats framework
 ***********************************/

static int
tsk_treeseq_check_windows(
    const tsk_treeseq_t *self, size_t num_windows, const double *windows)
{
    int ret = TSK_ERR_BAD_WINDOWS;
    size_t j;

    if (num_windows < 1) {
        ret = TSK_ERR_BAD_NUM_WINDOWS;
        goto out;
    }
    /* TODO these restrictions can be lifted later if we want a specific interval. */
    if (windows[0] != 0) {
        goto out;
    }
    if (windows[num_windows] != self->tables->sequence_length) {
        goto out;
    }
    for (j = 0; j < num_windows; j++) {
        if (windows[j] >= windows[j + 1]) {
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

/* TODO make these functions more consistent in how the arguments are ordered */

static inline void
update_state(double *X, size_t state_dim, tsk_id_t dest, tsk_id_t source, int sign)
{
    size_t k;
    double *X_dest = GET_2D_ROW(X, state_dim, dest);
    double *X_source = GET_2D_ROW(X, state_dim, source);

    for (k = 0; k < state_dim; k++) {
        X_dest[k] += sign * X_source[k];
    }
}

static inline int
update_node_summary(tsk_id_t u, size_t result_dim, double *node_summary, double *X,
    size_t state_dim, general_stat_func_t *f, void *f_params)
{
    double *X_u = GET_2D_ROW(X, state_dim, u);
    double *summary_u = GET_2D_ROW(node_summary, result_dim, u);

    return f(state_dim, X_u, result_dim, summary_u, f_params);
}

static inline void
update_running_sum(tsk_id_t u, double sign, const double *restrict branch_length,
    const double *summary, size_t result_dim, double *running_sum)
{
    const double *summary_u = GET_2D_ROW(summary, result_dim, u);
    const double x = sign * branch_length[u];
    size_t m;

    for (m = 0; m < result_dim; m++) {
        running_sum[m] += x * summary_u[m];
    }
}

static int
tsk_treeseq_branch_general_stat(const tsk_treeseq_t *self, size_t state_dim,
    const double *sample_weights, size_t result_dim, general_stat_func_t *f,
    void *f_params, size_t num_windows, const double *windows, double *result,
    tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_id_t u, v;
    size_t j, k, tree_index, window_index;
    size_t num_nodes = self->tables->nodes.num_rows;
    const tsk_id_t num_edges = (tsk_id_t) self->tables->edges.num_rows;
    const tsk_id_t *restrict I = self->tables->indexes.edge_insertion_order;
    const tsk_id_t *restrict O = self->tables->indexes.edge_removal_order;
    const double *restrict edge_left = self->tables->edges.left;
    const double *restrict edge_right = self->tables->edges.right;
    const tsk_id_t *restrict edge_parent = self->tables->edges.parent;
    const tsk_id_t *restrict edge_child = self->tables->edges.child;
    const double *restrict time = self->tables->nodes.time;
    const double sequence_length = self->tables->sequence_length;
    tsk_id_t *restrict parent = malloc(num_nodes * sizeof(*parent));
    double *restrict branch_length = calloc(num_nodes, sizeof(*branch_length));
    tsk_id_t tj, tk, h;
    double t_left, t_right, w_left, w_right, left, right, scale;
    const double *weight_u;
    double *state_u, *result_row, *summary_u;
    double *state = calloc(num_nodes * state_dim, sizeof(*state));
    double *summary = calloc(num_nodes * result_dim, sizeof(*summary));
    double *running_sum = calloc(result_dim, sizeof(*running_sum));

    if (parent == NULL || branch_length == NULL || state == NULL || running_sum == NULL
        || summary == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    memset(parent, 0xff, num_nodes * sizeof(*parent));

    /* Set the initial conditions */
    for (j = 0; j < self->num_samples; j++) {
        u = self->samples[j];
        state_u = GET_2D_ROW(state, state_dim, u);
        weight_u = GET_2D_ROW(sample_weights, state_dim, j);
        memcpy(state_u, weight_u, state_dim * sizeof(*state_u));
        summary_u = GET_2D_ROW(summary, result_dim, u);
        ret = f(state_dim, state_u, result_dim, summary_u, f_params);
        if (ret != 0) {
            goto out;
        }
    }
    memset(result, 0, num_windows * result_dim * sizeof(*result));

    /* Iterate over the trees */
    tj = 0;
    tk = 0;
    t_left = 0;
    tree_index = 0;
    window_index = 0;
    while (tj < num_edges || t_left < sequence_length) {
        while (tk < num_edges && edge_right[O[tk]] == t_left) {
            h = O[tk];
            tk++;

            u = edge_child[h];
            update_running_sum(u, -1, branch_length, summary, result_dim, running_sum);
            parent[u] = TSK_NULL;
            branch_length[u] = 0;

            u = edge_parent[h];
            while (u != TSK_NULL) {
                update_running_sum(
                    u, -1, branch_length, summary, result_dim, running_sum);
                update_state(state, state_dim, u, edge_child[h], -1);
                ret = update_node_summary(
                    u, result_dim, summary, state, state_dim, f, f_params);
                if (ret != 0) {
                    goto out;
                }
                update_running_sum(
                    u, +1, branch_length, summary, result_dim, running_sum);
                u = parent[u];
            }
        }

        while (tj < num_edges && edge_left[I[tj]] == t_left) {
            h = I[tj];
            tj++;

            u = edge_child[h];
            v = edge_parent[h];
            parent[u] = v;
            branch_length[u] = time[v] - time[u];
            update_running_sum(u, +1, branch_length, summary, result_dim, running_sum);

            u = v;
            while (u != TSK_NULL) {
                update_running_sum(
                    u, -1, branch_length, summary, result_dim, running_sum);
                update_state(state, state_dim, u, edge_child[h], +1);
                ret = update_node_summary(
                    u, result_dim, summary, state, state_dim, f, f_params);
                if (ret != 0) {
                    goto out;
                }
                update_running_sum(
                    u, +1, branch_length, summary, result_dim, running_sum);
                u = parent[u];
            }
        }

        t_right = sequence_length;
        if (tj < num_edges) {
            t_right = TSK_MIN(t_right, edge_left[I[tj]]);
        }
        if (tk < num_edges) {
            t_right = TSK_MIN(t_right, edge_right[O[tk]]);
        }

        while (windows[window_index] < t_right) {
            tsk_bug_assert(window_index < num_windows);
            w_left = windows[window_index];
            w_right = windows[window_index + 1];
            left = TSK_MAX(t_left, w_left);
            right = TSK_MIN(t_right, w_right);
            scale = (right - left);
            tsk_bug_assert(scale > 0);
            result_row = GET_2D_ROW(result, result_dim, window_index);
            for (k = 0; k < result_dim; k++) {
                result_row[k] += running_sum[k] * scale;
            }

            if (w_right <= t_right) {
                window_index++;
            } else {
                /* This interval crosses a tree boundary, so we update it again in the */
                /* for the next tree */
                break;
            }
        }
        /* Move to the next tree */
        t_left = t_right;
        tree_index++;
    }
    tsk_bug_assert(window_index == num_windows);
out:
    /* Can't use msp_safe_free here because of restrict */
    if (parent != NULL) {
        free(parent);
    }
    if (branch_length != NULL) {
        free(branch_length);
    }
    tsk_safe_free(state);
    tsk_safe_free(summary);
    tsk_safe_free(running_sum);
    return ret;
}

static int
get_allele_weights(const tsk_site_t *site, const double *state, size_t state_dim,
    const double *total_weight, tsk_size_t *ret_num_alleles, double **ret_allele_states)
{
    int ret = 0;
    size_t k;
    tsk_mutation_t mutation, parent_mut;
    tsk_size_t mutation_index, allele, num_alleles, alt_allele_length;
    /* The allele table */
    size_t max_alleles = site->mutations_length + 1;
    const char **alleles = malloc(max_alleles * sizeof(*alleles));
    tsk_size_t *allele_lengths = calloc(max_alleles, sizeof(*allele_lengths));
    double *allele_states = calloc(max_alleles * state_dim, sizeof(*allele_states));
    double *allele_row;
    const double *state_row;
    const char *alt_allele;

    if (alleles == NULL || allele_lengths == NULL || allele_states == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }

    tsk_bug_assert(state != NULL);
    alleles[0] = site->ancestral_state;
    allele_lengths[0] = site->ancestral_state_length;
    memcpy(allele_states, total_weight, state_dim * sizeof(*allele_states));
    num_alleles = 1;

    for (mutation_index = 0; mutation_index < site->mutations_length; mutation_index++) {
        mutation = site->mutations[mutation_index];
        /* Compute the allele index for this derived state value. */
        allele = 0;
        while (allele < num_alleles) {
            if (mutation.derived_state_length == allele_lengths[allele]
                && memcmp(
                       mutation.derived_state, alleles[allele], allele_lengths[allele])
                       == 0) {
                break;
            }
            allele++;
        }
        if (allele == num_alleles) {
            tsk_bug_assert(allele < max_alleles);
            alleles[allele] = mutation.derived_state;
            allele_lengths[allele] = mutation.derived_state_length;
            num_alleles++;
        }

        /* Add the state for the the mutation's node to this allele */
        state_row = GET_2D_ROW(state, state_dim, mutation.node);
        allele_row = GET_2D_ROW(allele_states, state_dim, allele);
        for (k = 0; k < state_dim; k++) {
            allele_row[k] += state_row[k];
        }

        /* Get the index for the alternate allele that we must substract from */
        alt_allele = site->ancestral_state;
        alt_allele_length = site->ancestral_state_length;
        if (mutation.parent != TSK_NULL) {
            parent_mut = site->mutations[mutation.parent - site->mutations[0].id];
            alt_allele = parent_mut.derived_state;
            alt_allele_length = parent_mut.derived_state_length;
        }
        allele = 0;
        while (allele < num_alleles) {
            if (alt_allele_length == allele_lengths[allele]
                && memcmp(alt_allele, alleles[allele], allele_lengths[allele]) == 0) {
                break;
            }
            allele++;
        }
        tsk_bug_assert(allele < num_alleles);

        allele_row = GET_2D_ROW(allele_states, state_dim, allele);
        for (k = 0; k < state_dim; k++) {
            allele_row[k] -= state_row[k];
        }
    }
    *ret_num_alleles = num_alleles;
    *ret_allele_states = allele_states;
    allele_states = NULL;
out:
    tsk_safe_free(alleles);
    tsk_safe_free(allele_lengths);
    tsk_safe_free(allele_states);
    return ret;
}

static int
compute_general_stat_site_result(tsk_site_t *site, double *state, size_t state_dim,
    size_t result_dim, general_stat_func_t *f, void *f_params, double *total_weight,
    bool polarised, double *result)
{
    int ret = 0;
    size_t k;
    tsk_size_t allele, num_alleles;
    double *allele_states;
    double *result_tmp = calloc(result_dim, sizeof(*result_tmp));

    if (result_tmp == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    memset(result, 0, result_dim * sizeof(*result));

    ret = get_allele_weights(
        site, state, state_dim, total_weight, &num_alleles, &allele_states);
    if (ret != 0) {
        goto out;
    }
    /* Sum over the allele weights. Skip the ancestral state if this is a polarised stat
     */
    for (allele = polarised ? 1 : 0; allele < num_alleles; allele++) {
        ret = f(state_dim, GET_2D_ROW(allele_states, state_dim, allele), result_dim,
            result_tmp, f_params);
        if (ret != 0) {
            goto out;
        }
        for (k = 0; k < result_dim; k++) {
            result[k] += result_tmp[k];
        }
    }
out:
    tsk_safe_free(result_tmp);
    tsk_safe_free(allele_states);
    return ret;
}

static int
tsk_treeseq_site_general_stat(const tsk_treeseq_t *self, size_t state_dim,
    const double *sample_weights, size_t result_dim, general_stat_func_t *f,
    void *f_params, size_t num_windows, const double *windows, double *result,
    tsk_flags_t options)
{
    int ret = 0;
    tsk_id_t u, v;
    size_t j, k, tree_site, tree_index, window_index;
    size_t num_nodes = self->tables->nodes.num_rows;
    const tsk_id_t num_edges = (tsk_id_t) self->tables->edges.num_rows;
    const tsk_id_t *restrict I = self->tables->indexes.edge_insertion_order;
    const tsk_id_t *restrict O = self->tables->indexes.edge_removal_order;
    const double *restrict edge_left = self->tables->edges.left;
    const double *restrict edge_right = self->tables->edges.right;
    const tsk_id_t *restrict edge_parent = self->tables->edges.parent;
    const tsk_id_t *restrict edge_child = self->tables->edges.child;
    const double sequence_length = self->tables->sequence_length;
    tsk_id_t *restrict parent = malloc(num_nodes * sizeof(*parent));
    tsk_site_t *site;
    tsk_id_t tj, tk, h;
    double t_left, t_right;
    const double *weight_u;
    double *state_u, *result_row;
    double *state = calloc(num_nodes * state_dim, sizeof(*state));
    double *total_weight = calloc(state_dim, sizeof(*total_weight));
    double *site_result = calloc(result_dim, sizeof(*site_result));
    bool polarised = false;

    if (parent == NULL || state == NULL || total_weight == NULL || site_result == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    memset(parent, 0xff, num_nodes * sizeof(*parent));

    if (options & TSK_STAT_POLARISED) {
        polarised = true;
    }

    /* Set the initial conditions */
    for (j = 0; j < self->num_samples; j++) {
        u = self->samples[j];
        state_u = GET_2D_ROW(state, state_dim, u);
        weight_u = GET_2D_ROW(sample_weights, state_dim, j);
        memcpy(state_u, weight_u, state_dim * sizeof(*state_u));
        for (k = 0; k < state_dim; k++) {
            total_weight[k] += weight_u[k];
        }
    }
    memset(result, 0, num_windows * result_dim * sizeof(*result));

    /* Iterate over the trees */
    tj = 0;
    tk = 0;
    t_left = 0;
    tree_index = 0;
    window_index = 0;
    while (tj < num_edges || t_left < sequence_length) {
        while (tk < num_edges && edge_right[O[tk]] == t_left) {
            h = O[tk];
            tk++;
            u = edge_child[h];
            v = edge_parent[h];
            while (v != TSK_NULL) {
                update_state(state, state_dim, v, u, -1);
                v = parent[v];
            }
            parent[u] = TSK_NULL;
        }

        while (tj < num_edges && edge_left[I[tj]] == t_left) {
            h = I[tj];
            tj++;
            u = edge_child[h];
            v = edge_parent[h];
            parent[u] = v;
            while (v != TSK_NULL) {
                update_state(state, state_dim, v, u, +1);
                v = parent[v];
            }
        }
        t_right = sequence_length;
        if (tj < num_edges) {
            t_right = TSK_MIN(t_right, edge_left[I[tj]]);
        }
        if (tk < num_edges) {
            t_right = TSK_MIN(t_right, edge_right[O[tk]]);
        }

        /* Update the sites */
        for (tree_site = 0; tree_site < self->tree_sites_length[tree_index];
             tree_site++) {
            site = self->tree_sites[tree_index] + tree_site;
            ret = compute_general_stat_site_result(site, state, state_dim, result_dim, f,
                f_params, total_weight, polarised, site_result);
            if (ret != 0) {
                goto out;
            }

            while (windows[window_index + 1] <= site->position) {
                window_index++;
                tsk_bug_assert(window_index < num_windows);
            }
            tsk_bug_assert(windows[window_index] <= site->position);
            tsk_bug_assert(site->position < windows[window_index + 1]);
            result_row = GET_2D_ROW(result, result_dim, window_index);
            for (k = 0; k < result_dim; k++) {
                result_row[k] += site_result[k];
            }
        }
        tree_index++;
        t_left = t_right;
    }
out:
    /* Can't use msp_safe_free here because of restrict */
    if (parent != NULL) {
        free(parent);
    }
    tsk_safe_free(state);
    tsk_safe_free(total_weight);
    tsk_safe_free(site_result);
    return ret;
}

static inline void
increment_row(size_t length, double multiplier, double *source, double *dest)
{
    size_t j;

    for (j = 0; j < length; j++) {
        dest[j] += multiplier * source[j];
    }
}

static int
tsk_treeseq_node_general_stat(const tsk_treeseq_t *self, size_t state_dim,
    const double *sample_weights, size_t result_dim, general_stat_func_t *f,
    void *f_params, size_t num_windows, const double *windows, double *result,
    tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_id_t u, v;
    size_t j, window_index;
    size_t num_nodes = self->tables->nodes.num_rows;
    const tsk_id_t num_edges = (tsk_id_t) self->tables->edges.num_rows;
    const tsk_id_t *restrict I = self->tables->indexes.edge_insertion_order;
    const tsk_id_t *restrict O = self->tables->indexes.edge_removal_order;
    const double *restrict edge_left = self->tables->edges.left;
    const double *restrict edge_right = self->tables->edges.right;
    const tsk_id_t *restrict edge_parent = self->tables->edges.parent;
    const tsk_id_t *restrict edge_child = self->tables->edges.child;
    const double sequence_length = self->tables->sequence_length;
    tsk_id_t *restrict parent = malloc(num_nodes * sizeof(*parent));
    tsk_id_t tj, tk, h;
    const double *weight_u;
    double *state_u;
    double *state = calloc(num_nodes * state_dim, sizeof(*state));
    double *node_summary = calloc(num_nodes * result_dim, sizeof(*node_summary));
    double *last_update = calloc(num_nodes, sizeof(*last_update));
    double t_left, t_right, w_right;

    if (parent == NULL || state == NULL || node_summary == NULL || last_update == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    memset(parent, 0xff, num_nodes * sizeof(*parent));
    memset(result, 0, num_windows * num_nodes * result_dim * sizeof(*result));

    /* Set the initial conditions */
    for (j = 0; j < self->num_samples; j++) {
        u = self->samples[j];
        state_u = GET_2D_ROW(state, state_dim, u);
        weight_u = GET_2D_ROW(sample_weights, state_dim, j);
        memcpy(state_u, weight_u, state_dim * sizeof(*state_u));
    }
    for (u = 0; u < (tsk_id_t) num_nodes; u++) {
        ret = update_node_summary(
            u, result_dim, node_summary, state, state_dim, f, f_params);
        if (ret != 0) {
            goto out;
        }
    }

    /* Iterate over the trees */
    tj = 0;
    tk = 0;
    t_left = 0;
    window_index = 0;
    while (tj < num_edges || t_left < sequence_length) {
        tsk_bug_assert(window_index < num_windows);
        while (tk < num_edges && edge_right[O[tk]] == t_left) {
            h = O[tk];
            tk++;
            u = edge_child[h];
            v = edge_parent[h];
            while (v != TSK_NULL) {
                increment_row(result_dim, t_left - last_update[v],
                    GET_2D_ROW(node_summary, result_dim, v),
                    GET_3D_ROW(result, num_nodes, result_dim, window_index, v));
                last_update[v] = t_left;
                update_state(state, state_dim, v, u, -1);
                ret = update_node_summary(
                    v, result_dim, node_summary, state, state_dim, f, f_params);
                if (ret != 0) {
                    goto out;
                }
                v = parent[v];
            }
            parent[u] = TSK_NULL;
        }

        while (tj < num_edges && edge_left[I[tj]] == t_left) {
            h = I[tj];
            tj++;
            u = edge_child[h];
            v = edge_parent[h];
            parent[u] = v;
            while (v != TSK_NULL) {
                increment_row(result_dim, t_left - last_update[v],
                    GET_2D_ROW(node_summary, result_dim, v),
                    GET_3D_ROW(result, num_nodes, result_dim, window_index, v));
                last_update[v] = t_left;
                update_state(state, state_dim, v, u, +1);
                ret = update_node_summary(
                    v, result_dim, node_summary, state, state_dim, f, f_params);
                if (ret != 0) {
                    goto out;
                }
                v = parent[v];
            }
        }

        t_right = sequence_length;
        if (tj < num_edges) {
            t_right = TSK_MIN(t_right, edge_left[I[tj]]);
        }
        if (tk < num_edges) {
            t_right = TSK_MIN(t_right, edge_right[O[tk]]);
        }

        while (window_index < num_windows && windows[window_index + 1] <= t_right) {
            w_right = windows[window_index + 1];
            /* Flush the contributions of all nodes to the current window */
            for (u = 0; u < (tsk_id_t) num_nodes; u++) {
                tsk_bug_assert(last_update[u] < w_right);
                increment_row(result_dim, w_right - last_update[u],
                    GET_2D_ROW(node_summary, result_dim, u),
                    GET_3D_ROW(result, num_nodes, result_dim, window_index, u));
                last_update[u] = w_right;
            }
            window_index++;
        }

        t_left = t_right;
    }
out:
    /* Can't use msp_safe_free here because of restrict */
    if (parent != NULL) {
        free(parent);
    }
    tsk_safe_free(state);
    tsk_safe_free(node_summary);
    tsk_safe_free(last_update);
    return ret;
}

static void
span_normalise(size_t num_windows, const double *windows, size_t row_size, double *array)
{
    size_t window_index, k;
    double span, *row;

    for (window_index = 0; window_index < num_windows; window_index++) {
        span = windows[window_index + 1] - windows[window_index];
        row = GET_2D_ROW(array, row_size, window_index);
        for (k = 0; k < row_size; k++) {
            row[k] /= span;
        }
    }
}

typedef struct {
    general_stat_func_t *f;
    void *f_params;
    double *total_weight;
    double *total_minus_state;
    double *result_tmp;
} unpolarised_summary_func_args;

static int
unpolarised_summary_func(size_t state_dim, const double *state, size_t result_dim,
    double *result, void *params)
{
    int ret = 0;
    unpolarised_summary_func_args *upargs = (unpolarised_summary_func_args *) params;
    const double *total_weight = upargs->total_weight;
    double *total_minus_state = upargs->total_minus_state;
    double *result_tmp = upargs->result_tmp;
    size_t k, m;

    ret = upargs->f(state_dim, state, result_dim, result, upargs->f_params);
    if (ret != 0) {
        goto out;
    }
    for (k = 0; k < state_dim; k++) {
        total_minus_state[k] = total_weight[k] - state[k];
    }
    ret = upargs->f(
        state_dim, total_minus_state, result_dim, result_tmp, upargs->f_params);
    if (ret != 0) {
        goto out;
    }
    for (m = 0; m < result_dim; m++) {
        result[m] += result_tmp[m];
    }
out:
    return ret;
}

/* Abstracts the running of node and branch stats where the summary function
 * is run twice when non-polarised. We replace the call to the input summary
 * function with a call of the required form when non-polarised, simplifying
 * the implementation and memory management for the node and branch stats.
 */
static int
tsk_polarisable_func_general_stat(const tsk_treeseq_t *self, size_t state_dim,
    const double *sample_weights, size_t result_dim, general_stat_func_t *f,
    void *f_params, size_t num_windows, const double *windows, double *result,
    tsk_flags_t options)
{
    int ret = 0;
    bool stat_branch = !!(options & TSK_STAT_BRANCH);
    bool polarised = options & TSK_STAT_POLARISED;
    general_stat_func_t *wrapped_f = f;
    void *wrapped_f_params = f_params;
    const double *weight_u;
    unpolarised_summary_func_args upargs;
    tsk_size_t j, k;

    memset(&upargs, 0, sizeof(upargs));
    if (!polarised) {
        upargs.f = f;
        upargs.f_params = f_params;
        upargs.total_weight = calloc(state_dim, sizeof(double));
        upargs.total_minus_state = calloc(state_dim, sizeof(double));
        upargs.result_tmp = calloc(result_dim, sizeof(double));

        if (upargs.total_weight == NULL || upargs.total_minus_state == NULL
            || upargs.result_tmp == NULL) {
            ret = TSK_ERR_NO_MEMORY;
            goto out;
        }

        /* Compute the total weight */
        for (j = 0; j < self->num_samples; j++) {
            weight_u = GET_2D_ROW(sample_weights, state_dim, j);
            for (k = 0; k < state_dim; k++) {
                upargs.total_weight[k] += weight_u[k];
            }
        }

        wrapped_f = unpolarised_summary_func;
        wrapped_f_params = &upargs;
    }

    if (stat_branch) {
        ret = tsk_treeseq_branch_general_stat(self, state_dim, sample_weights,
            result_dim, wrapped_f, wrapped_f_params, num_windows, windows, result,
            options);
    } else {
        ret = tsk_treeseq_node_general_stat(self, state_dim, sample_weights, result_dim,
            wrapped_f, wrapped_f_params, num_windows, windows, result, options);
    }
out:
    tsk_safe_free(upargs.total_weight);
    tsk_safe_free(upargs.total_minus_state);
    tsk_safe_free(upargs.result_tmp);
    return ret;
}

int
tsk_treeseq_general_stat(const tsk_treeseq_t *self, size_t state_dim,
    const double *sample_weights, size_t result_dim, general_stat_func_t *f,
    void *f_params, size_t num_windows, const double *windows, double *result,
    tsk_flags_t options)
{
    int ret = 0;
    bool stat_site = !!(options & TSK_STAT_SITE);
    bool stat_branch = !!(options & TSK_STAT_BRANCH);
    bool stat_node = !!(options & TSK_STAT_NODE);
    double default_windows[] = { 0, self->tables->sequence_length };
    size_t row_size;

    /* If no mode is specified, we default to site mode */
    if (!(stat_site || stat_branch || stat_node)) {
        stat_site = true;
    }
    /* It's an error to specify more than one mode */
    if (stat_site + stat_branch + stat_node > 1) {
        ret = TSK_ERR_MULTIPLE_STAT_MODES;
        goto out;
    }

    if (state_dim < 1) {
        ret = TSK_ERR_BAD_STATE_DIMS;
        goto out;
    }
    if (result_dim < 1) {
        ret = TSK_ERR_BAD_RESULT_DIMS;
        goto out;
    }
    if (windows == NULL) {
        num_windows = 1;
        windows = default_windows;
    } else {
        ret = tsk_treeseq_check_windows(self, num_windows, windows);
        if (ret != 0) {
            goto out;
        }
    }

    if (stat_site) {
        ret = tsk_treeseq_site_general_stat(self, state_dim, sample_weights, result_dim,
            f, f_params, num_windows, windows, result, options);
    } else {
        ret = tsk_polarisable_func_general_stat(self, state_dim, sample_weights,
            result_dim, f, f_params, num_windows, windows, result, options);
    }

    if (options & TSK_STAT_SPAN_NORMALISE) {
        row_size = result_dim;
        if (stat_node) {
            row_size = result_dim * tsk_treeseq_get_num_nodes(self);
        }
        span_normalise(num_windows, windows, row_size, result);
    }

out:
    return ret;
}

static int
check_set_indexes(
    tsk_size_t num_sets, tsk_size_t num_set_indexes, const tsk_id_t *set_indexes)
{
    int ret = 0;
    tsk_size_t j;

    for (j = 0; j < num_set_indexes; j++) {
        if (set_indexes[j] < 0 || set_indexes[j] >= (tsk_id_t) num_sets) {
            ret = TSK_ERR_BAD_SAMPLE_SET_INDEX;
            goto out;
        }
    }
out:
    return ret;
}

static int
tsk_treeseq_check_sample_sets(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets)
{
    int ret = 0;
    size_t j, k, l;
    const tsk_id_t num_nodes = (tsk_id_t) self->tables->nodes.num_rows;
    tsk_id_t u, sample_index;

    if (num_sample_sets == 0) {
        ret = TSK_ERR_INSUFFICIENT_SAMPLE_SETS;
        goto out;
    }
    j = 0;
    for (k = 0; k < num_sample_sets; k++) {
        if (sample_set_sizes[k] == 0) {
            ret = TSK_ERR_EMPTY_SAMPLE_SET;
            goto out;
        }
        for (l = 0; l < sample_set_sizes[k]; l++) {
            u = sample_sets[j];
            if (u < 0 || u >= num_nodes) {
                ret = TSK_ERR_NODE_OUT_OF_BOUNDS;
                goto out;
            }
            sample_index = self->sample_index_map[u];
            if (sample_index == TSK_NULL) {
                ret = TSK_ERR_BAD_SAMPLES;
                goto out;
            }
            j++;
        }
    }
out:
    return ret;
}

typedef struct {
    tsk_size_t num_samples;
} weight_stat_params_t;

typedef struct {
    tsk_size_t num_samples;
    tsk_size_t num_covariates;
    double *V;
} covariates_stat_params_t;

typedef struct {
    const tsk_id_t *sample_sets;
    tsk_size_t num_sample_sets;
    const tsk_size_t *sample_set_sizes;
    const tsk_id_t *set_indexes;
} sample_count_stat_params_t;

static int
tsk_treeseq_sample_count_stat(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t result_dim, const tsk_id_t *set_indexes, general_stat_func_t *f,
    tsk_size_t num_windows, const double *windows, double *result, tsk_flags_t options)
{
    int ret = 0;
    const size_t num_samples = self->num_samples;
    size_t j, k, l;
    tsk_id_t u, sample_index;
    double *weights = NULL;
    double *weight_row;
    sample_count_stat_params_t args = { .sample_sets = sample_sets,
        .num_sample_sets = num_sample_sets,
        .sample_set_sizes = sample_set_sizes,
        .set_indexes = set_indexes };

    ret = tsk_treeseq_check_sample_sets(
        self, num_sample_sets, sample_set_sizes, sample_sets);
    if (ret != 0) {
        goto out;
    }
    weights = calloc(num_samples * num_sample_sets, sizeof(*weights));
    if (weights == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    j = 0;
    for (k = 0; k < num_sample_sets; k++) {
        for (l = 0; l < sample_set_sizes[k]; l++) {
            u = sample_sets[j];
            sample_index = self->sample_index_map[u];
            weight_row = GET_2D_ROW(weights, num_sample_sets, sample_index);
            if (weight_row[k] != 0) {
                ret = TSK_ERR_DUPLICATE_SAMPLE;
                goto out;
            }
            weight_row[k] = 1;
            j++;
        }
    }
    ret = tsk_treeseq_general_stat(self, num_sample_sets, weights, result_dim, f, &args,
        num_windows, windows, result, options);
out:
    tsk_safe_free(weights);
    return ret;
}

/***********************************
 * Allele frequency spectrum
 ***********************************/

static inline void
fold(tsk_size_t *restrict coordinate, const tsk_size_t *restrict dims,
    tsk_size_t num_dims)
{
    tsk_size_t k;
    double n = 0;
    int s = 0;

    for (k = 0; k < num_dims; k++) {
        tsk_bug_assert(coordinate[k] < dims[k]);
        n += dims[k] - 1;
        s += (int) coordinate[k];
    }
    n /= 2;
    k = num_dims;
    while (s == n && k > 0) {
        k--;
        n -= ((double) (dims[k] - 1)) / 2;
        s -= (int) coordinate[k];
    }
    if (s > n) {
        for (k = 0; k < num_dims; k++) {
            s = (int) (dims[k] - 1 - coordinate[k]);
            tsk_bug_assert(s >= 0);
            coordinate[k] = (tsk_size_t) s;
        }
    }
}

static int
tsk_treeseq_update_site_afs(const tsk_treeseq_t *self, const tsk_site_t *site,
    const double *total_counts, const double *counts, tsk_size_t num_sample_sets,
    tsk_size_t window_index, tsk_size_t *result_dims, double *result,
    tsk_flags_t options)
{
    int ret = 0;
    tsk_size_t afs_size;
    tsk_size_t k, allele, num_alleles, all_samples;
    double increment, *afs, *allele_counts, *allele_count;
    tsk_size_t *coordinate = malloc(num_sample_sets * sizeof(*coordinate));
    bool polarised = !!(options & TSK_STAT_POLARISED);
    const tsk_size_t K = num_sample_sets + 1;

    if (coordinate == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    ret = get_allele_weights(
        site, counts, K, total_counts, &num_alleles, &allele_counts);
    if (ret != 0) {
        goto out;
    }

    afs_size = result_dims[num_sample_sets];
    afs = result + afs_size * window_index;

    increment = polarised ? 1 : 0.5;
    /* Sum over the allele weights. Skip the ancestral state if polarised. */
    for (allele = polarised ? 1 : 0; allele < num_alleles; allele++) {
        allele_count = GET_2D_ROW(allele_counts, K, allele);
        all_samples = (tsk_size_t) allele_count[num_sample_sets];
        if (all_samples > 0 && all_samples < self->num_samples) {
            for (k = 0; k < num_sample_sets; k++) {
                coordinate[k] = (tsk_size_t) allele_count[k];
            }
            if (!polarised) {
                fold(coordinate, result_dims, num_sample_sets);
            }
            increment_nd_array_value(
                afs, num_sample_sets, result_dims, coordinate, increment);
        }
    }
out:
    tsk_safe_free(coordinate);
    tsk_safe_free(allele_counts);
    return ret;
}

static int
tsk_treeseq_site_allele_frequency_spectrum(const tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, const tsk_size_t *sample_set_sizes, double *counts,
    tsk_size_t num_windows, const double *windows, tsk_size_t *result_dims,
    double *result, tsk_flags_t options)
{
    int ret = 0;
    tsk_id_t u, v;
    tsk_size_t tree_site, tree_index, window_index;
    size_t num_nodes = self->tables->nodes.num_rows;
    const tsk_id_t num_edges = (tsk_id_t) self->tables->edges.num_rows;
    const tsk_id_t *restrict I = self->tables->indexes.edge_insertion_order;
    const tsk_id_t *restrict O = self->tables->indexes.edge_removal_order;
    const double *restrict edge_left = self->tables->edges.left;
    const double *restrict edge_right = self->tables->edges.right;
    const tsk_id_t *restrict edge_parent = self->tables->edges.parent;
    const tsk_id_t *restrict edge_child = self->tables->edges.child;
    const double sequence_length = self->tables->sequence_length;
    tsk_id_t *restrict parent = malloc(num_nodes * sizeof(*parent));
    tsk_site_t *site;
    tsk_id_t tj, tk, h;
    tsk_size_t j;
    const tsk_size_t K = num_sample_sets + 1;
    double t_left, t_right;
    double *total_counts = malloc((1 + num_sample_sets) * sizeof(*total_counts));

    if (parent == NULL || total_counts == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    memset(parent, 0xff, num_nodes * sizeof(*parent));

    for (j = 0; j < num_sample_sets; j++) {
        total_counts[j] = sample_set_sizes[j];
    }
    total_counts[num_sample_sets] = self->num_samples;

    /* Iterate over the trees */
    tj = 0;
    tk = 0;
    t_left = 0;
    tree_index = 0;
    window_index = 0;
    while (tj < num_edges || t_left < sequence_length) {
        while (tk < num_edges && edge_right[O[tk]] == t_left) {
            h = O[tk];
            tk++;
            u = edge_child[h];
            v = edge_parent[h];
            while (v != TSK_NULL) {
                update_state(counts, K, v, u, -1);
                v = parent[v];
            }
            parent[u] = TSK_NULL;
        }

        while (tj < num_edges && edge_left[I[tj]] == t_left) {
            h = I[tj];
            tj++;
            u = edge_child[h];
            v = edge_parent[h];
            parent[u] = v;
            while (v != TSK_NULL) {
                update_state(counts, K, v, u, +1);
                v = parent[v];
            }
        }
        t_right = sequence_length;
        if (tj < num_edges) {
            t_right = TSK_MIN(t_right, edge_left[I[tj]]);
        }
        if (tk < num_edges) {
            t_right = TSK_MIN(t_right, edge_right[O[tk]]);
        }

        /* Update the sites */
        for (tree_site = 0; tree_site < self->tree_sites_length[tree_index];
             tree_site++) {
            site = self->tree_sites[tree_index] + tree_site;
            while (windows[window_index + 1] <= site->position) {
                window_index++;
                tsk_bug_assert(window_index < num_windows);
            }
            ret = tsk_treeseq_update_site_afs(self, site, total_counts, counts,
                num_sample_sets, window_index, result_dims, result, options);
            if (ret != 0) {
                goto out;
            }
            tsk_bug_assert(windows[window_index] <= site->position);
            tsk_bug_assert(site->position < windows[window_index + 1]);
        }
        tree_index++;
        t_left = t_right;
    }
out:
    /* Can't use msp_safe_free here because of restrict */
    if (parent != NULL) {
        free(parent);
    }
    tsk_safe_free(total_counts);
    return ret;
}

static int TSK_WARN_UNUSED
tsk_treeseq_update_branch_afs(const tsk_treeseq_t *self, tsk_id_t u, double right,
    const double *restrict branch_length, double *restrict last_update,
    const double *counts, tsk_size_t num_sample_sets, size_t window_index,
    const tsk_size_t *result_dims, double *result, tsk_flags_t options)
{
    int ret = 0;
    tsk_size_t afs_size;
    tsk_size_t k;
    double *afs;
    tsk_size_t *coordinate = malloc(num_sample_sets * sizeof(*coordinate));
    bool polarised = !!(options & TSK_STAT_POLARISED);
    const double *count_row = GET_2D_ROW(counts, num_sample_sets + 1, u);
    double x = (right - last_update[u]) * branch_length[u];
    const tsk_size_t all_samples = (tsk_size_t) count_row[num_sample_sets];

    if (coordinate == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }

    if (0 < all_samples && all_samples < self->num_samples) {
        if (!polarised) {
            x *= 0.5;
        }
        afs_size = result_dims[num_sample_sets];
        afs = result + afs_size * window_index;
        for (k = 0; k < num_sample_sets; k++) {
            coordinate[k] = (tsk_size_t) count_row[k];
        }
        if (!polarised) {
            fold(coordinate, result_dims, num_sample_sets);
        }
        increment_nd_array_value(afs, num_sample_sets, result_dims, coordinate, x);
    }
    last_update[u] = right;
out:
    tsk_safe_free(coordinate);
    return ret;
}

static int
tsk_treeseq_branch_allele_frequency_spectrum(const tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, double *counts, tsk_size_t num_windows,
    const double *windows, const tsk_size_t *result_dims, double *result,
    tsk_flags_t options)
{
    int ret = 0;
    tsk_id_t u, v;
    size_t window_index;
    size_t num_nodes = self->tables->nodes.num_rows;
    const tsk_id_t num_edges = (tsk_id_t) self->tables->edges.num_rows;
    const tsk_id_t *restrict I = self->tables->indexes.edge_insertion_order;
    const tsk_id_t *restrict O = self->tables->indexes.edge_removal_order;
    const double *restrict edge_left = self->tables->edges.left;
    const double *restrict edge_right = self->tables->edges.right;
    const tsk_id_t *restrict edge_parent = self->tables->edges.parent;
    const tsk_id_t *restrict edge_child = self->tables->edges.child;
    const double *restrict node_time = self->tables->nodes.time;
    const double sequence_length = self->tables->sequence_length;
    tsk_id_t *restrict parent = malloc(num_nodes * sizeof(*parent));
    double *restrict last_update = calloc(num_nodes, sizeof(*last_update));
    double *restrict branch_length = calloc(num_nodes, sizeof(*branch_length));
    tsk_id_t tj, tk, h;
    double t_left, t_right, w_right;
    const tsk_size_t K = num_sample_sets + 1;

    if (parent == NULL || last_update == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    memset(parent, 0xff, num_nodes * sizeof(*parent));

    /* Iterate over the trees */
    tj = 0;
    tk = 0;
    t_left = 0;
    window_index = 0;
    while (tj < num_edges || t_left < sequence_length) {
        tsk_bug_assert(window_index < num_windows);
        while (tk < num_edges && edge_right[O[tk]] == t_left) {
            h = O[tk];
            tk++;
            u = edge_child[h];
            v = edge_parent[h];
            ret = tsk_treeseq_update_branch_afs(self, u, t_left, branch_length,
                last_update, counts, num_sample_sets, window_index, result_dims, result,
                options);
            if (ret != 0) {
                goto out;
            }
            while (v != TSK_NULL) {
                ret = tsk_treeseq_update_branch_afs(self, v, t_left, branch_length,
                    last_update, counts, num_sample_sets, window_index, result_dims,
                    result, options);
                if (ret != 0) {
                    goto out;
                }
                update_state(counts, K, v, u, -1);
                v = parent[v];
            }
            parent[u] = TSK_NULL;
            branch_length[u] = 0;
        }

        while (tj < num_edges && edge_left[I[tj]] == t_left) {
            h = I[tj];
            tj++;
            u = edge_child[h];
            v = edge_parent[h];
            parent[u] = v;
            branch_length[u] = node_time[v] - node_time[u];
            while (v != TSK_NULL) {
                ret = tsk_treeseq_update_branch_afs(self, v, t_left, branch_length,
                    last_update, counts, num_sample_sets, window_index, result_dims,
                    result, options);
                if (ret != 0) {
                    goto out;
                }
                update_state(counts, K, v, u, +1);
                v = parent[v];
            }
        }

        t_right = sequence_length;
        if (tj < num_edges) {
            t_right = TSK_MIN(t_right, edge_left[I[tj]]);
        }
        if (tk < num_edges) {
            t_right = TSK_MIN(t_right, edge_right[O[tk]]);
        }

        while (window_index < num_windows && windows[window_index + 1] <= t_right) {
            w_right = windows[window_index + 1];
            /* Flush the contributions of all nodes to the current window */
            for (u = 0; u < (tsk_id_t) num_nodes; u++) {
                tsk_bug_assert(last_update[u] < w_right);
                ret = tsk_treeseq_update_branch_afs(self, u, w_right, branch_length,
                    last_update, counts, num_sample_sets, window_index, result_dims,
                    result, options);
                if (ret != 0) {
                    goto out;
                }
            }
            window_index++;
        }

        t_left = t_right;
    }
out:
    /* Can't use msp_safe_free here because of restrict */
    if (parent != NULL) {
        free(parent);
    }
    if (last_update != NULL) {
        free(last_update);
    }
    if (branch_length != NULL) {
        free(branch_length);
    }
    return ret;
}

int
tsk_treeseq_allele_frequency_spectrum(const tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, const tsk_size_t *sample_set_sizes,
    const tsk_id_t *sample_sets, tsk_size_t num_windows, const double *windows,
    double *result, tsk_flags_t options)
{
    int ret = 0;
    bool stat_site = !!(options & TSK_STAT_SITE);
    bool stat_branch = !!(options & TSK_STAT_BRANCH);
    bool stat_node = !!(options & TSK_STAT_NODE);
    double default_windows[] = { 0, self->tables->sequence_length };
    const size_t num_nodes = self->tables->nodes.num_rows;
    const size_t K = num_sample_sets + 1;
    size_t j, k, l, afs_size;
    tsk_id_t u;
    tsk_size_t *result_dims = NULL;
    /* These counts should really be ints, but we use doubles so that we can
     * reuse code from the general_stats code paths. */
    double *counts = NULL;
    double *count_row;

    if (stat_node) {
        ret = TSK_ERR_UNSUPPORTED_STAT_MODE;
        goto out;
    }
    /* If no mode is specified, we default to site mode */
    if (!(stat_site || stat_branch)) {
        stat_site = true;
    }
    /* It's an error to specify more than one mode */
    if (stat_site + stat_branch > 1) {
        ret = TSK_ERR_MULTIPLE_STAT_MODES;
        goto out;
    }
    if (windows == NULL) {
        num_windows = 1;
        windows = default_windows;
    } else {
        ret = tsk_treeseq_check_windows(self, num_windows, windows);
        if (ret != 0) {
            goto out;
        }
    }
    ret = tsk_treeseq_check_sample_sets(
        self, num_sample_sets, sample_set_sizes, sample_sets);
    if (ret != 0) {
        goto out;
    }

    /* the last element of result_dims stores the total size of the dimenensions */
    result_dims = malloc((num_sample_sets + 1) * sizeof(*result_dims));
    counts = calloc(num_nodes * K, sizeof(*counts));
    if (counts == NULL || result_dims == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    afs_size = 1;
    j = 0;
    for (k = 0; k < num_sample_sets; k++) {
        result_dims[k] = 1 + sample_set_sizes[k];
        afs_size *= result_dims[k];
        for (l = 0; l < sample_set_sizes[k]; l++) {
            u = sample_sets[j];
            count_row = GET_2D_ROW(counts, K, u);
            if (count_row[k] != 0) {
                ret = TSK_ERR_DUPLICATE_SAMPLE;
                goto out;
            }
            count_row[k] = 1;
            j++;
        }
    }
    for (j = 0; j < self->num_samples; j++) {
        u = self->samples[j];
        count_row = GET_2D_ROW(counts, K, u);
        count_row[num_sample_sets] = 1;
    }
    result_dims[num_sample_sets] = (tsk_size_t) afs_size;

    memset(result, 0, num_windows * afs_size * sizeof(*result));
    if (stat_site) {
        ret = tsk_treeseq_site_allele_frequency_spectrum(self, num_sample_sets,
            sample_set_sizes, counts, num_windows, windows, result_dims, result,
            options);
    } else {
        ret = tsk_treeseq_branch_allele_frequency_spectrum(self, num_sample_sets, counts,
            num_windows, windows, result_dims, result, options);
    }

    if (options & TSK_STAT_SPAN_NORMALISE) {
        span_normalise(num_windows, windows, afs_size, result);
    }
out:
    tsk_safe_free(counts);
    tsk_safe_free(result_dims);
    return ret;
}

/***********************************
 * One way stats
 ***********************************/

static int
diversity_summary_func(size_t state_dim, const double *state,
    size_t TSK_UNUSED(result_dim), double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    double n;
    size_t j;

    for (j = 0; j < state_dim; j++) {
        n = (double) args.sample_set_sizes[j];
        result[j] = x[j] * (n - x[j]) / (n * (n - 1));
    }
    return 0;
}

int
tsk_treeseq_diversity(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_windows, const double *windows, double *result, tsk_flags_t options)
{
    return tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_sample_sets, NULL, diversity_summary_func, num_windows, windows,
        result, options);
}

static int
trait_covariance_summary_func(size_t state_dim, const double *state,
    size_t TSK_UNUSED(result_dim), double *result, void *params)
{
    weight_stat_params_t args = *(weight_stat_params_t *) params;
    const double n = (double) args.num_samples;
    const double *x = state;
    size_t j;

    for (j = 0; j < state_dim; j++) {
        result[j] = (x[j] * x[j]) / (2 * (n - 1) * (n - 1));
    }
    return 0;
}

int
tsk_treeseq_trait_covariance(const tsk_treeseq_t *self, tsk_size_t num_weights,
    const double *weights, tsk_size_t num_windows, const double *windows, double *result,
    tsk_flags_t options)
{
    tsk_size_t num_samples = self->num_samples;
    size_t j, k;
    int ret;
    const double *row;
    double *new_row;
    double *means = calloc(num_weights, sizeof(double));
    double *new_weights = malloc((num_weights + 1) * num_samples * sizeof(double));
    weight_stat_params_t args = { num_samples = self->num_samples };

    if (new_weights == NULL || means == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }

    // center weights
    for (j = 0; j < num_samples; j++) {
        row = GET_2D_ROW(weights, num_weights, j);
        for (k = 0; k < num_weights; k++) {
            means[k] += row[k];
        }
    }
    for (k = 0; k < num_weights; k++) {
        means[k] /= (double) num_samples;
    }
    for (j = 0; j < num_samples; j++) {
        row = GET_2D_ROW(weights, num_weights, j);
        new_row = GET_2D_ROW(new_weights, num_weights, j);
        for (k = 0; k < num_weights; k++) {
            new_row[k] = row[k] - means[k];
        }
    }

    ret = tsk_treeseq_general_stat(self, num_weights, new_weights, num_weights,
        trait_covariance_summary_func, &args, num_windows, windows, result, options);

out:
    tsk_safe_free(means);
    tsk_safe_free(new_weights);
    return ret;
}

static int
trait_correlation_summary_func(size_t state_dim, const double *state,
    size_t TSK_UNUSED(result_dim), double *result, void *params)
{
    weight_stat_params_t args = *(weight_stat_params_t *) params;
    const double n = (double) args.num_samples;
    const double *x = state;
    double p;
    size_t j;

    p = x[state_dim - 1];
    for (j = 0; j < state_dim - 1; j++) {
        if ((p > 0.0) && (p < 1.0)) {
            result[j] = (x[j] * x[j]) / (2 * (p * (1 - p)) * n * (n - 1));
        } else {
            result[j] = 0.0;
        }
    }
    return 0;
}

int
tsk_treeseq_trait_correlation(const tsk_treeseq_t *self, tsk_size_t num_weights,
    const double *weights, tsk_size_t num_windows, const double *windows, double *result,
    tsk_flags_t options)
{
    tsk_size_t num_samples = self->num_samples;
    size_t j, k;
    int ret;
    double *means = calloc(num_weights, sizeof(double));
    double *meansqs = calloc(num_weights, sizeof(double));
    double *sds = calloc(num_weights, sizeof(double));
    const double *row;
    double *new_row;
    double *new_weights = malloc((num_weights + 1) * num_samples * sizeof(double));
    weight_stat_params_t args = { num_samples = self->num_samples };

    if (new_weights == NULL || means == NULL || meansqs == NULL || sds == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }

    if (num_weights < 1) {
        ret = TSK_ERR_BAD_STATE_DIMS;
        goto out;
    }

    // center and scale weights
    for (j = 0; j < num_samples; j++) {
        row = GET_2D_ROW(weights, num_weights, j);
        for (k = 0; k < num_weights; k++) {
            means[k] += row[k];
            meansqs[k] += row[k] * row[k];
        }
    }
    for (k = 0; k < num_weights; k++) {
        means[k] /= (double) num_samples;
        meansqs[k] -= means[k] * means[k] * (double) num_samples;
        meansqs[k] /= (double) (num_samples - 1);
        sds[k] = sqrt(meansqs[k]);
    }
    for (j = 0; j < num_samples; j++) {
        row = GET_2D_ROW(weights, num_weights, j);
        new_row = GET_2D_ROW(new_weights, num_weights + 1, j);
        for (k = 0; k < num_weights; k++) {
            new_row[k] = (row[k] - means[k]) / sds[k];
        }
        // set final row to 1/n to compute frequency
        new_row[num_weights] = 1.0 / (double) num_samples;
    }

    ret = tsk_treeseq_general_stat(self, num_weights + 1, new_weights, num_weights,
        trait_correlation_summary_func, &args, num_windows, windows, result, options);

out:
    tsk_safe_free(means);
    tsk_safe_free(meansqs);
    tsk_safe_free(sds);
    tsk_safe_free(new_weights);
    return ret;
}

static int
trait_linear_model_summary_func(size_t state_dim, const double *state, size_t result_dim,
    double *result, void *params)
{
    covariates_stat_params_t args = *(covariates_stat_params_t *) params;
    const double num_samples = (double) args.num_samples;
    const tsk_size_t k = args.num_covariates;
    const double *V = args.V;
    ;
    const double *x = state;
    const double *v;
    double m, a, denom, z;
    size_t i, j;
    // x[0], ..., x[result_dim - 1] contains the traits, W
    // x[result_dim], ..., x[state_dim - 2] contains the covariates, Z
    // x[state_dim - 1] has the number of samples below the node

    m = x[state_dim - 1];
    for (i = 0; i < result_dim; i++) {
        if ((m > 0.0) && (m < num_samples)) {
            v = GET_2D_ROW(V, k, i);
            a = x[i];
            denom = m;
            for (j = 0; j < k; j++) {
                z = x[result_dim + j];
                a -= z * v[j];
                denom -= z * z;
            }
            // denom is the length of projection of the trait onto the subspace
            // spanned by the covariates, so if it is zero then the system is
            // singular and the solution is nonunique. This numerical tolerance
            // could be smaller without hitting floating-point error, but being
            // a tiny bit conservative about when the trait is almost in the
            // span of the covariates is probably good.
            if (denom < 1e-8) {
                result[i] = 0.0;
            } else {
                result[i] = (a * a) / (2 * denom * denom);
            }
        } else {
            result[i] = 0.0;
        }
    }
    return 0;
}

int
tsk_treeseq_trait_linear_model(const tsk_treeseq_t *self, tsk_size_t num_weights,
    const double *weights, tsk_size_t num_covariates, const double *covariates,
    tsk_size_t num_windows, const double *windows, double *result, tsk_flags_t options)
{
    tsk_size_t num_samples = self->num_samples;
    size_t i, j, k;
    int ret;
    const double *w, *z;
    double *v, *new_row;
    double *V = calloc(num_covariates * num_weights, sizeof(double));
    double *new_weights
        = malloc((num_weights + num_covariates + 1) * num_samples * sizeof(double));

    covariates_stat_params_t args
        = { .num_samples = self->num_samples, .num_covariates = num_covariates, .V = V };

    // We assume that the covariates have been *already standardised*,
    // so that (a) 1 is in the span of the columns, and
    // (b) their crossproduct is the identity.
    // We could do this instead here with gsl linalg.

    if (new_weights == NULL || V == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }

    if (num_weights < 1) {
        ret = TSK_ERR_BAD_STATE_DIMS;
        goto out;
    }

    // V = weights^T (matrix mult) covariates
    for (k = 0; k < num_samples; k++) {
        w = GET_2D_ROW(weights, num_weights, k);
        z = GET_2D_ROW(covariates, num_covariates, k);
        for (i = 0; i < num_weights; i++) {
            v = GET_2D_ROW(V, num_covariates, i);
            for (j = 0; j < num_covariates; j++) {
                v[j] += w[i] * z[j];
            }
        }
    }

    for (k = 0; k < num_samples; k++) {
        w = GET_2D_ROW(weights, num_weights, k);
        z = GET_2D_ROW(covariates, num_covariates, k);
        new_row = GET_2D_ROW(new_weights, num_covariates + num_weights + 1, k);
        for (i = 0; i < num_weights; i++) {
            new_row[i] = w[i];
        }
        for (i = 0; i < num_covariates; i++) {
            new_row[i + num_weights] = z[i];
        }
        // set final row to 1 to count alleles
        new_row[num_weights + num_covariates] = 1.0;
    }

    ret = tsk_treeseq_general_stat(self, num_weights + num_covariates + 1, new_weights,
        num_weights, trait_linear_model_summary_func, &args, num_windows, windows,
        result, options);

out:
    tsk_safe_free(V);
    tsk_safe_free(new_weights);
    return ret;
}

static int
segregating_sites_summary_func(size_t state_dim, const double *state,
    size_t TSK_UNUSED(result_dim), double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    double n;
    size_t j;

    // this works because sum_{i=1}^k (1-p_i) = k-1
    for (j = 0; j < state_dim; j++) {
        n = (double) args.sample_set_sizes[j];
        result[j] = (x[j] > 0) * (1 - x[j] / n);
    }
    return 0;
}

int
tsk_treeseq_segregating_sites(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_windows, const double *windows, double *result, tsk_flags_t options)
{
    return tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_sample_sets, NULL, segregating_sites_summary_func, num_windows,
        windows, result, options);
}

static int
Y1_summary_func(size_t TSK_UNUSED(state_dim), const double *state, size_t result_dim,
    double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    double ni, denom, numer;
    size_t i;

    for (i = 0; i < result_dim; i++) {
        ni = args.sample_set_sizes[i];
        denom = ni * (ni - 1) * (ni - 2);
        numer = x[i] * (ni - x[i]) * (ni - x[i] - 1);
        result[i] = numer / denom;
    }
    return 0;
}

int
tsk_treeseq_Y1(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_windows, const double *windows, double *result, tsk_flags_t options)
{
    return tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_sample_sets, NULL, Y1_summary_func, num_windows, windows,
        result, options);
}

/***********************************
 * Two way stats
 ***********************************/

static int
check_sample_stat_inputs(tsk_size_t num_sample_sets, tsk_size_t tuple_size,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples)
{
    int ret = 0;

    if (num_sample_sets < tuple_size) {
        ret = TSK_ERR_INSUFFICIENT_SAMPLE_SETS;
        goto out;
    }
    if (num_index_tuples < 1) {
        ret = TSK_ERR_INSUFFICIENT_INDEX_TUPLES;
        goto out;
    }
    ret = check_set_indexes(
        num_sample_sets, tuple_size * num_index_tuples, index_tuples);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

static int
divergence_summary_func(size_t TSK_UNUSED(state_dim), const double *state,
    size_t result_dim, double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    double ni, nj, denom;
    tsk_id_t i, j;
    size_t k;

    for (k = 0; k < result_dim; k++) {
        i = args.set_indexes[2 * k];
        j = args.set_indexes[2 * k + 1];
        ni = args.sample_set_sizes[i];
        nj = args.sample_set_sizes[j];
        denom = ni * (nj - (i == j));
        result[k] = x[i] * (nj - x[j]) / denom;
    }
    return 0;
}

int
tsk_treeseq_divergence(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_windows,
    const double *windows, double *result, tsk_flags_t options)
{
    int ret = 0;
    ret = check_sample_stat_inputs(num_sample_sets, 2, num_index_tuples, index_tuples);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_index_tuples, index_tuples, divergence_summary_func,
        num_windows, windows, result, options);
out:
    return ret;
}

static int
genetic_relatedness_summary_func(size_t state_dim, const double *state,
    size_t result_dim, double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    tsk_id_t i, j;
    size_t k;
    double sumx = 0;
    double sumn = 0;
    double meanx, ni, nj;

    for (k = 0; k < state_dim; k++) {
        sumx += x[k];
        sumn += args.sample_set_sizes[k];
    }

    meanx = sumx / sumn;
    for (k = 0; k < result_dim; k++) {
        i = args.set_indexes[2 * k];
        j = args.set_indexes[2 * k + 1];
        ni = args.sample_set_sizes[i];
        nj = args.sample_set_sizes[j];
        result[k] = (x[i] - ni * meanx) * (x[j] - nj * meanx) / 2;
    }
    return 0;
}

int
tsk_treeseq_genetic_relatedness(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_windows,
    const double *windows, double *result, tsk_flags_t options)
{
    int ret = 0;
    ret = check_sample_stat_inputs(num_sample_sets, 2, num_index_tuples, index_tuples);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_index_tuples, index_tuples, genetic_relatedness_summary_func,
        num_windows, windows, result, options);
out:
    return ret;
}

static int
Y2_summary_func(size_t TSK_UNUSED(state_dim), const double *state, size_t result_dim,
    double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    double ni, nj, denom;
    tsk_id_t i, j;
    size_t k;

    for (k = 0; k < result_dim; k++) {
        i = args.set_indexes[2 * k];
        j = args.set_indexes[2 * k + 1];
        ni = args.sample_set_sizes[i];
        nj = args.sample_set_sizes[j];
        denom = ni * nj * (nj - 1);
        result[k] = x[i] * (nj - x[j]) * (nj - x[j] - 1) / denom;
    }
    return 0;
}

int
tsk_treeseq_Y2(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_windows,
    const double *windows, double *result, tsk_flags_t options)
{
    int ret = 0;
    ret = check_sample_stat_inputs(num_sample_sets, 2, num_index_tuples, index_tuples);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_index_tuples, index_tuples, Y2_summary_func, num_windows,
        windows, result, options);
out:
    return ret;
}

static int
f2_summary_func(size_t TSK_UNUSED(state_dim), const double *state, size_t result_dim,
    double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    double ni, nj, denom, numer;
    tsk_id_t i, j;
    size_t k;

    for (k = 0; k < result_dim; k++) {
        i = args.set_indexes[2 * k];
        j = args.set_indexes[2 * k + 1];
        ni = args.sample_set_sizes[i];
        nj = args.sample_set_sizes[j];
        denom = ni * (ni - 1) * nj * (nj - 1);
        numer = x[i] * (x[i] - 1) * (nj - x[j]) * (nj - x[j] - 1)
                - x[i] * (ni - x[i]) * (nj - x[j]) * x[j];
        result[k] = numer / denom;
    }
    return 0;
}

int
tsk_treeseq_f2(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_windows,
    const double *windows, double *result, tsk_flags_t options)
{
    int ret = 0;
    ret = check_sample_stat_inputs(num_sample_sets, 2, num_index_tuples, index_tuples);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_index_tuples, index_tuples, f2_summary_func, num_windows,
        windows, result, options);
out:
    return ret;
}

/***********************************
 * Three way stats
 ***********************************/

static int
Y3_summary_func(size_t TSK_UNUSED(state_dim), const double *state, size_t result_dim,
    double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    double ni, nj, nk, denom, numer;
    tsk_id_t i, j, k;
    size_t tuple_index;

    for (tuple_index = 0; tuple_index < result_dim; tuple_index++) {
        i = args.set_indexes[3 * tuple_index];
        j = args.set_indexes[3 * tuple_index + 1];
        k = args.set_indexes[3 * tuple_index + 2];
        ni = args.sample_set_sizes[i];
        nj = args.sample_set_sizes[j];
        nk = args.sample_set_sizes[k];
        denom = ni * nj * nk;
        numer = x[i] * (nj - x[j]) * (nk - x[k]);
        result[tuple_index] = numer / denom;
    }
    return 0;
}

int
tsk_treeseq_Y3(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_windows,
    const double *windows, double *result, tsk_flags_t options)
{
    int ret = 0;
    ret = check_sample_stat_inputs(num_sample_sets, 3, num_index_tuples, index_tuples);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_index_tuples, index_tuples, Y3_summary_func, num_windows,
        windows, result, options);
out:
    return ret;
}

static int
f3_summary_func(size_t TSK_UNUSED(state_dim), const double *state, size_t result_dim,
    double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    double ni, nj, nk, denom, numer;
    tsk_id_t i, j, k;
    size_t tuple_index;

    for (tuple_index = 0; tuple_index < result_dim; tuple_index++) {
        i = args.set_indexes[3 * tuple_index];
        j = args.set_indexes[3 * tuple_index + 1];
        k = args.set_indexes[3 * tuple_index + 2];
        ni = args.sample_set_sizes[i];
        nj = args.sample_set_sizes[j];
        nk = args.sample_set_sizes[k];
        denom = ni * (ni - 1) * nj * nk;
        numer = x[i] * (x[i] - 1) * (nj - x[j]) * (nk - x[k])
                - x[i] * (ni - x[i]) * (nj - x[j]) * x[k];
        result[tuple_index] = numer / denom;
    }
    return 0;
}

int
tsk_treeseq_f3(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_windows,
    const double *windows, double *result, tsk_flags_t options)
{
    int ret = 0;
    ret = check_sample_stat_inputs(num_sample_sets, 3, num_index_tuples, index_tuples);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_index_tuples, index_tuples, f3_summary_func, num_windows,
        windows, result, options);
out:
    return ret;
}

/***********************************
 * Four way stats
 ***********************************/

static int
f4_summary_func(size_t TSK_UNUSED(state_dim), const double *state, size_t result_dim,
    double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    double ni, nj, nk, nl, denom, numer;
    tsk_id_t i, j, k, l;
    size_t tuple_index;

    for (tuple_index = 0; tuple_index < result_dim; tuple_index++) {
        i = args.set_indexes[4 * tuple_index];
        j = args.set_indexes[4 * tuple_index + 1];
        k = args.set_indexes[4 * tuple_index + 2];
        l = args.set_indexes[4 * tuple_index + 3];
        ni = args.sample_set_sizes[i];
        nj = args.sample_set_sizes[j];
        nk = args.sample_set_sizes[k];
        nl = args.sample_set_sizes[l];
        denom = ni * nj * nk * nl;
        numer = x[i] * x[k] * (nj - x[j]) * (nl - x[l])
                - x[i] * x[l] * (nj - x[j]) * (nk - x[k]);
        result[tuple_index] = numer / denom;
    }
    return 0;
}

int
tsk_treeseq_f4(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_windows,
    const double *windows, double *result, tsk_flags_t options)
{
    int ret = 0;
    ret = check_sample_stat_inputs(num_sample_sets, 4, num_index_tuples, index_tuples);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_index_tuples, index_tuples, f4_summary_func, num_windows,
        windows, result, options);
out:
    return ret;
}

/* Error-raising getter functions */

int TSK_WARN_UNUSED
tsk_treeseq_get_node(const tsk_treeseq_t *self, tsk_id_t index, tsk_node_t *node)
{
    return tsk_node_table_get_row(&self->tables->nodes, index, node);
}

int TSK_WARN_UNUSED
tsk_treeseq_get_edge(const tsk_treeseq_t *self, tsk_id_t index, tsk_edge_t *edge)
{
    return tsk_edge_table_get_row(&self->tables->edges, index, edge);
}

int TSK_WARN_UNUSED
tsk_treeseq_get_migration(
    const tsk_treeseq_t *self, tsk_id_t index, tsk_migration_t *migration)
{
    return tsk_migration_table_get_row(&self->tables->migrations, index, migration);
}

int TSK_WARN_UNUSED
tsk_treeseq_get_mutation(
    const tsk_treeseq_t *self, tsk_id_t index, tsk_mutation_t *mutation)
{
    return tsk_mutation_table_get_row(&self->tables->mutations, index, mutation);
}

int TSK_WARN_UNUSED
tsk_treeseq_get_site(const tsk_treeseq_t *self, tsk_id_t index, tsk_site_t *site)
{
    int ret = 0;

    ret = tsk_site_table_get_row(&self->tables->sites, index, site);
    if (ret != 0) {
        goto out;
    }
    site->mutations = self->site_mutations[index];
    site->mutations_length = self->site_mutations_length[index];
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_treeseq_get_individual(
    const tsk_treeseq_t *self, tsk_id_t index, tsk_individual_t *individual)
{
    int ret = 0;

    ret = tsk_individual_table_get_row(&self->tables->individuals, index, individual);
    if (ret != 0) {
        goto out;
    }
    individual->nodes = self->individual_nodes[index];
    individual->nodes_length = self->individual_nodes_length[index];
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_treeseq_get_population(
    const tsk_treeseq_t *self, tsk_id_t index, tsk_population_t *population)
{
    return tsk_population_table_get_row(&self->tables->populations, index, population);
}

int TSK_WARN_UNUSED
tsk_treeseq_get_provenance(
    const tsk_treeseq_t *self, tsk_id_t index, tsk_provenance_t *provenance)
{
    return tsk_provenance_table_get_row(&self->tables->provenances, index, provenance);
}

int TSK_WARN_UNUSED
tsk_treeseq_simplify(const tsk_treeseq_t *self, const tsk_id_t *samples,
    tsk_size_t num_samples, tsk_flags_t options, tsk_treeseq_t *output,
    tsk_id_t *node_map)
{
    int ret = 0;
    tsk_table_collection_t tables;

    ret = tsk_treeseq_copy_tables(self, &tables, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_simplify(
        &tables, samples, num_samples, options, node_map);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_init(output, &tables, TSK_BUILD_INDEXES);
out:
    tsk_table_collection_free(&tables);
    return ret;
}

/* ======================================================== *
 * Tree
 * ======================================================== */

int TSK_WARN_UNUSED
tsk_tree_clear(tsk_tree_t *self)
{
    int ret = 0;
    tsk_size_t j;
    tsk_id_t u;
    const tsk_size_t N = self->num_nodes;
    const tsk_size_t num_samples = self->tree_sequence->num_samples;
    const bool sample_counts = !(self->options & TSK_NO_SAMPLE_COUNTS);
    const bool sample_lists = !!(self->options & TSK_SAMPLE_LISTS);

    self->left = 0;
    self->right = 0;
    self->index = -1;
    /* TODO we should profile this method to see if just doing a single loop over
     * the nodes would be more efficient than multiple memsets.
     */
    memset(self->parent, 0xff, N * sizeof(tsk_id_t));
    memset(self->left_child, 0xff, N * sizeof(tsk_id_t));
    memset(self->right_child, 0xff, N * sizeof(tsk_id_t));
    memset(self->left_sib, 0xff, N * sizeof(tsk_id_t));
    memset(self->right_sib, 0xff, N * sizeof(tsk_id_t));

    if (sample_counts) {
        memset(self->num_samples, 0, N * sizeof(tsk_id_t));
        memset(self->marked, 0, N * sizeof(uint8_t));
        /* We can't reset the tracked samples via memset because we don't
         * know where the tracked samples are.
         */
        for (j = 0; j < self->num_nodes; j++) {
            if (!tsk_treeseq_is_sample(self->tree_sequence, (tsk_id_t) j)) {
                self->num_tracked_samples[j] = 0;
            }
        }
    }
    if (sample_lists) {
        memset(self->left_sample, 0xff, N * sizeof(tsk_id_t));
        memset(self->right_sample, 0xff, N * sizeof(tsk_id_t));
        memset(self->next_sample, 0xff, num_samples * sizeof(tsk_id_t));
    }
    /* Set the sample attributes */
    for (j = 0; j < num_samples; j++) {
        u = self->samples[j];
        if (sample_counts) {
            self->num_samples[u] = 1;
        }
        if (sample_lists) {
            /* We are mapping to *indexes* into the list of samples here */
            self->left_sample[u] = (tsk_id_t) j;
            self->right_sample[u] = (tsk_id_t) j;
        }
    }
    self->left_root = TSK_NULL;
    if (sample_counts && self->root_threshold == 1 && num_samples > 0) {
        self->left_root = self->samples[0];
        for (j = 0; j < num_samples; j++) {
            /* Set initial roots */
            u = self->samples[j];
            if (j < num_samples - 1) {
                self->right_sib[u] = self->samples[j + 1];
            }
            if (j > 0) {
                self->left_sib[u] = self->samples[j - 1];
            }
        }
    }
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_init(tsk_tree_t *self, const tsk_treeseq_t *tree_sequence, tsk_flags_t options)
{
    int ret = TSK_ERR_NO_MEMORY;
    tsk_size_t num_samples;
    tsk_size_t num_nodes;

    memset(self, 0, sizeof(tsk_tree_t));
    if (tree_sequence == NULL) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    if (options & TSK_SAMPLE_COUNTS) {
        fprintf(stderr, "TSK_SAMPLE_COUNTS is no longer supported. "
                        "Sample counts are tracked by default since 0.99.3, "
                        "Please use TSK_NO_SAMPLE_COUNTS to turn off sample counts.");
    }
    num_nodes = tree_sequence->tables->nodes.num_rows;
    num_samples = tree_sequence->num_samples;
    self->num_nodes = num_nodes;
    self->tree_sequence = tree_sequence;
    self->samples = tree_sequence->samples;
    self->options = options;
    self->root_threshold = 1;
    self->parent = malloc(num_nodes * sizeof(tsk_id_t));
    self->left_child = malloc(num_nodes * sizeof(tsk_id_t));
    self->right_child = malloc(num_nodes * sizeof(tsk_id_t));
    self->left_sib = malloc(num_nodes * sizeof(tsk_id_t));
    self->right_sib = malloc(num_nodes * sizeof(tsk_id_t));
    if (self->parent == NULL || self->left_child == NULL || self->right_child == NULL
        || self->left_sib == NULL || self->right_sib == NULL) {
        goto out;
    }
    if (!(self->options & TSK_NO_SAMPLE_COUNTS)) {
        self->num_samples = calloc(num_nodes, sizeof(tsk_id_t));
        self->num_tracked_samples = calloc(num_nodes, sizeof(tsk_id_t));
        self->marked = calloc(num_nodes, sizeof(uint8_t));
        if (self->num_samples == NULL || self->num_tracked_samples == NULL
            || self->marked == NULL) {
            goto out;
        }
    }
    if (self->options & TSK_SAMPLE_LISTS) {
        self->left_sample = malloc(num_nodes * sizeof(*self->left_sample));
        self->right_sample = malloc(num_nodes * sizeof(*self->right_sample));
        self->next_sample = malloc(num_samples * sizeof(*self->next_sample));
        if (self->left_sample == NULL || self->right_sample == NULL
            || self->next_sample == NULL) {
            goto out;
        }
    }
    ret = tsk_tree_clear(self);
out:
    return ret;
}

int
tsk_tree_set_root_threshold(tsk_tree_t *self, tsk_size_t root_threshold)
{
    int ret = 0;

    if (root_threshold == 0) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    /* Don't allow the value to be set when the tree is out of the null
     * state */
    if (self->index != -1) {
        ret = TSK_ERR_UNSUPPORTED_OPERATION;
        goto out;
    }
    self->root_threshold = root_threshold;
out:
    return ret;
}

tsk_size_t
tsk_tree_get_root_threshold(const tsk_tree_t *self)
{
    return self->root_threshold;
}

int
tsk_tree_free(tsk_tree_t *self)
{
    tsk_safe_free(self->parent);
    tsk_safe_free(self->left_child);
    tsk_safe_free(self->right_child);
    tsk_safe_free(self->left_sib);
    tsk_safe_free(self->right_sib);
    tsk_safe_free(self->num_samples);
    tsk_safe_free(self->num_tracked_samples);
    tsk_safe_free(self->marked);
    tsk_safe_free(self->left_sample);
    tsk_safe_free(self->right_sample);
    tsk_safe_free(self->next_sample);
    return 0;
}

bool
tsk_tree_has_sample_lists(const tsk_tree_t *self)
{
    return !!(self->options & TSK_SAMPLE_LISTS);
}

bool
tsk_tree_has_sample_counts(const tsk_tree_t *self)
{
    return !(self->options & TSK_NO_SAMPLE_COUNTS);
}

static int TSK_WARN_UNUSED
tsk_tree_reset_tracked_samples(tsk_tree_t *self)
{
    int ret = 0;

    if (!tsk_tree_has_sample_counts(self)) {
        ret = TSK_ERR_UNSUPPORTED_OPERATION;
        goto out;
    }
    memset(self->num_tracked_samples, 0, self->num_nodes * sizeof(tsk_id_t));
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_set_tracked_samples(
    tsk_tree_t *self, size_t num_tracked_samples, const tsk_id_t *tracked_samples)
{
    int ret = TSK_ERR_GENERIC;
    size_t j;
    tsk_id_t u;

    /* TODO This is not needed when the tree is new. We should use the
     * state machine to check and only reset the tracked samples when needed.
     */
    ret = tsk_tree_reset_tracked_samples(self);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < num_tracked_samples; j++) {
        u = tracked_samples[j];
        if (u < 0 || u >= (tsk_id_t) self->num_nodes) {
            ret = TSK_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        if (!tsk_treeseq_is_sample(self->tree_sequence, u)) {
            ret = TSK_ERR_BAD_SAMPLES;
            goto out;
        }
        if (self->num_tracked_samples[u] != 0) {
            ret = TSK_ERR_DUPLICATE_SAMPLE;
            goto out;
        }
        /* Propagate this upwards */
        while (u != TSK_NULL) {
            self->num_tracked_samples[u] += 1;
            u = self->parent[u];
        }
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_set_tracked_samples_from_sample_list(
    tsk_tree_t *self, tsk_tree_t *other, tsk_id_t node)
{
    int ret = TSK_ERR_GENERIC;
    tsk_id_t u, stop, index;
    const tsk_id_t *next = other->next_sample;
    const tsk_id_t *samples = other->tree_sequence->samples;

    if (!tsk_tree_has_sample_lists(other)) {
        ret = TSK_ERR_UNSUPPORTED_OPERATION;
        goto out;
    }
    /* TODO This is not needed when the tree is new. We should use the
     * state machine to check and only reset the tracked samples when needed.
     */
    ret = tsk_tree_reset_tracked_samples(self);
    if (ret != 0) {
        goto out;
    }

    index = other->left_sample[node];
    if (index != TSK_NULL) {
        stop = other->right_sample[node];
        while (true) {
            u = samples[index];
            tsk_bug_assert(self->num_tracked_samples[u] == 0);
            /* Propagate this upwards */
            while (u != TSK_NULL) {
                self->num_tracked_samples[u] += 1;
                u = self->parent[u];
            }
            if (index == stop) {
                break;
            }
            index = next[index];
        }
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_copy(const tsk_tree_t *self, tsk_tree_t *dest, tsk_flags_t options)
{
    int ret = TSK_ERR_GENERIC;
    size_t N = self->num_nodes;

    if (!(options & TSK_NO_INIT)) {
        ret = tsk_tree_init(dest, self->tree_sequence, options);
        if (ret != 0) {
            goto out;
        }
    }
    if (self->tree_sequence != dest->tree_sequence) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    dest->left = self->left;
    dest->right = self->right;
    dest->left_root = self->left_root;
    dest->left_index = self->left_index;
    dest->right_index = self->right_index;
    dest->direction = self->direction;
    dest->index = self->index;
    dest->sites = self->sites;
    dest->sites_length = self->sites_length;
    dest->root_threshold = self->root_threshold;

    memcpy(dest->parent, self->parent, N * sizeof(tsk_id_t));
    memcpy(dest->left_child, self->left_child, N * sizeof(tsk_id_t));
    memcpy(dest->right_child, self->right_child, N * sizeof(tsk_id_t));
    memcpy(dest->left_sib, self->left_sib, N * sizeof(tsk_id_t));
    memcpy(dest->right_sib, self->right_sib, N * sizeof(tsk_id_t));
    if (!(dest->options & TSK_NO_SAMPLE_COUNTS)) {
        if (self->options & TSK_NO_SAMPLE_COUNTS) {
            ret = TSK_ERR_UNSUPPORTED_OPERATION;
            goto out;
        }
        memcpy(dest->num_samples, self->num_samples, N * sizeof(*self->num_samples));
        memcpy(dest->num_tracked_samples, self->num_tracked_samples,
            N * sizeof(*self->num_tracked_samples));
        memcpy(dest->marked, self->marked, N * sizeof(*self->marked));
    }
    if (dest->options & TSK_SAMPLE_LISTS) {
        if (!(self->options & TSK_SAMPLE_LISTS)) {
            ret = TSK_ERR_UNSUPPORTED_OPERATION;
            goto out;
        }
        memcpy(dest->left_sample, self->left_sample, N * sizeof(tsk_id_t));
        memcpy(dest->right_sample, self->right_sample, N * sizeof(tsk_id_t));
        memcpy(dest->next_sample, self->next_sample,
            self->tree_sequence->num_samples * sizeof(tsk_id_t));
    }
    ret = 0;
out:
    return ret;
}

bool TSK_WARN_UNUSED
tsk_tree_equals(const tsk_tree_t *self, const tsk_tree_t *other)
{
    bool ret = false;

    if (self->tree_sequence == other->tree_sequence) {
        ret = self->index == other->index;
    }
    return ret;
}

static int
tsk_tree_check_node(const tsk_tree_t *self, tsk_id_t u)
{
    int ret = 0;
    if (u < 0 || u >= (tsk_id_t) self->num_nodes) {
        ret = TSK_ERR_NODE_OUT_OF_BOUNDS;
    }
    return ret;
}

bool
tsk_tree_is_descendant(const tsk_tree_t *self, tsk_id_t u, tsk_id_t v)
{
    bool ret = false;
    tsk_id_t w = u;
    tsk_id_t *restrict parent = self->parent;

    if (tsk_tree_check_node(self, u) == 0 && tsk_tree_check_node(self, v) == 0) {
        while (w != v && w != TSK_NULL) {
            w = parent[w];
        }
        ret = w == v;
    }
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_get_mrca(const tsk_tree_t *self, tsk_id_t u, tsk_id_t v, tsk_id_t *mrca)
{
    int ret = 0;
    tsk_id_t w = 0;
    tsk_id_t *s1 = NULL;
    tsk_id_t *s2 = NULL;
    tsk_id_t j;
    int l1, l2;

    /* We have a NULL value in these stacks, so need to be sure that
     * we have an extra space allocated */
    s1 = malloc((1 + self->num_nodes) * sizeof(*s1));
    s2 = malloc((1 + self->num_nodes) * sizeof(*s2));
    if (s1 == NULL || s2 == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }

    ret = tsk_tree_check_node(self, u);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_tree_check_node(self, v);
    if (ret != 0) {
        goto out;
    }
    j = u;
    l1 = 0;
    while (j != TSK_NULL) {
        tsk_bug_assert(l1 < (int) self->num_nodes);
        s1[l1] = j;
        l1++;
        j = self->parent[j];
    }
    s1[l1] = TSK_NULL;
    j = v;
    l2 = 0;
    while (j != TSK_NULL) {
        tsk_bug_assert(l2 < (int) self->num_nodes);
        s2[l2] = j;
        l2++;
        j = self->parent[j];
    }
    s2[l2] = TSK_NULL;
    do {
        w = s1[l1];
        l1--;
        l2--;
    } while (l1 >= 0 && l2 >= 0 && s1[l1] == s2[l2]);
    *mrca = w;
    ret = 0;
out:
    tsk_safe_free(s1);
    tsk_safe_free(s2);
    return ret;
}

static int
tsk_tree_get_num_samples_by_traversal(
    const tsk_tree_t *self, tsk_id_t u, size_t *num_samples)
{
    int ret = 0;
    tsk_id_t *stack = NULL;
    tsk_id_t v;
    size_t count = 0;
    int stack_top = 0;

    stack = malloc(self->num_nodes * sizeof(*stack));
    if (stack == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }

    stack[0] = u;
    while (stack_top >= 0) {
        v = stack[stack_top];
        stack_top--;
        if (tsk_treeseq_is_sample(self->tree_sequence, v)) {
            count++;
        }
        v = self->left_child[v];
        while (v != TSK_NULL) {
            stack_top++;
            stack[stack_top] = v;
            v = self->right_sib[v];
        }
    }
    *num_samples = count;
out:
    tsk_safe_free(stack);
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_get_num_samples(const tsk_tree_t *self, tsk_id_t u, size_t *num_samples)
{
    int ret = 0;

    ret = tsk_tree_check_node(self, u);
    if (ret != 0) {
        goto out;
    }

    if (!(self->options & TSK_NO_SAMPLE_COUNTS)) {
        *num_samples = (size_t) self->num_samples[u];
    } else {
        ret = tsk_tree_get_num_samples_by_traversal(self, u, num_samples);
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_get_num_tracked_samples(
    const tsk_tree_t *self, tsk_id_t u, size_t *num_tracked_samples)
{
    int ret = 0;

    ret = tsk_tree_check_node(self, u);
    if (ret != 0) {
        goto out;
    }
    if (self->options & TSK_NO_SAMPLE_COUNTS) {
        ret = TSK_ERR_UNSUPPORTED_OPERATION;
        goto out;
    }
    *num_tracked_samples = (size_t) self->num_tracked_samples[u];
out:
    return ret;
}

bool
tsk_tree_is_sample(const tsk_tree_t *self, tsk_id_t u)
{
    return tsk_treeseq_is_sample(self->tree_sequence, u);
}

tsk_size_t
tsk_tree_get_num_roots(const tsk_tree_t *self)
{
    tsk_size_t num_roots = 0;
    tsk_id_t u = self->left_root;

    while (u != TSK_NULL) {
        u = self->right_sib[u];
        num_roots++;
    }
    return num_roots;
}

tsk_id_t
tsk_tree_get_index(const tsk_tree_t *self)
{
    return self->index;
}

int TSK_WARN_UNUSED
tsk_tree_get_parent(const tsk_tree_t *self, tsk_id_t u, tsk_id_t *parent)
{
    int ret = 0;

    ret = tsk_tree_check_node(self, u);
    if (ret != 0) {
        goto out;
    }
    *parent = self->parent[u];
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_get_time(const tsk_tree_t *self, tsk_id_t u, double *t)
{
    int ret = 0;
    tsk_node_t node;

    ret = tsk_treeseq_get_node(self->tree_sequence, u, &node);
    if (ret != 0) {
        goto out;
    }
    *t = node.time;
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_get_sites(
    const tsk_tree_t *self, const tsk_site_t **sites, tsk_size_t *sites_length)
{
    *sites = self->sites;
    *sites_length = self->sites_length;
    return 0;
}

/* u must be a valid node in the tree. For internal use */
static tsk_size_t
tsk_tree_get_depth(const tsk_tree_t *self, tsk_id_t u)
{

    tsk_id_t v;
    tsk_size_t depth = 0;

    for (v = self->parent[u]; v != TSK_NULL; v = self->parent[v]) {
        depth++;
    }

    return depth;
}

int TSK_WARN_UNUSED
tsk_tree_depth(const tsk_tree_t *self, tsk_id_t u, tsk_size_t *depth_ret)
{
    int ret = 0;

    ret = tsk_tree_check_node(self, u);
    if (ret != 0) {
        goto out;
    }

    *depth_ret = tsk_tree_get_depth(self, u);
out:
    return ret;
}

static int
tsk_tree_node_root(tsk_tree_t *self, tsk_id_t u)
{
    tsk_id_t v = u;
    while (self->parent[v] != TSK_NULL) {
        v = self->parent[v];
    }

    return v;
}

/* TODO Should push this through the stack since this is a python method and
 * it's here anyway */
static double
tsk_tree_branch_length(const tsk_tree_t *self, tsk_id_t u)
{
    const double *times = self->tree_sequence->tables->nodes.time;
    tsk_id_t parent = self->parent[u];

    return parent == TSK_NULL ? 0 : times[parent] - times[u];
}

static void
tsk_tree_check_state(const tsk_tree_t *self)
{
    tsk_id_t u, v;
    size_t j, num_samples;
    int err, c;
    tsk_site_t site;
    tsk_id_t *children = malloc(self->num_nodes * sizeof(tsk_id_t));
    bool *is_root = calloc(self->num_nodes, sizeof(bool));

    tsk_bug_assert(children != NULL);

    for (j = 0; j < self->tree_sequence->num_samples; j++) {
        u = self->samples[j];
        while (self->parent[u] != TSK_NULL) {
            u = self->parent[u];
        }
        is_root[u] = true;
    }
    if (self->tree_sequence->num_samples == 0) {
        tsk_bug_assert(self->left_root == TSK_NULL);
    } else {
        tsk_bug_assert(self->left_sib[self->left_root] == TSK_NULL);
    }
    /* Iterate over the roots and make sure they are set */
    for (u = self->left_root; u != TSK_NULL; u = self->right_sib[u]) {
        tsk_bug_assert(is_root[u]);
        is_root[u] = false;
    }
    for (u = 0; u < (tsk_id_t) self->num_nodes; u++) {
        tsk_bug_assert(!is_root[u]);
        c = 0;
        for (v = self->left_child[u]; v != TSK_NULL; v = self->right_sib[v]) {
            tsk_bug_assert(self->parent[v] == u);
            children[c] = v;
            c++;
        }
        for (v = self->right_child[u]; v != TSK_NULL; v = self->left_sib[v]) {
            tsk_bug_assert(c > 0);
            c--;
            tsk_bug_assert(v == children[c]);
        }
    }
    for (j = 0; j < self->sites_length; j++) {
        site = self->sites[j];
        tsk_bug_assert(self->left <= site.position);
        tsk_bug_assert(site.position < self->right);
    }

    if (!(self->options & TSK_NO_SAMPLE_COUNTS)) {
        tsk_bug_assert(self->num_samples != NULL);
        tsk_bug_assert(self->num_tracked_samples != NULL);
        for (u = 0; u < (tsk_id_t) self->num_nodes; u++) {
            err = tsk_tree_get_num_samples_by_traversal(self, u, &num_samples);
            tsk_bug_assert(err == 0);
            tsk_bug_assert(num_samples == (size_t) self->num_samples[u]);
        }
    } else {
        tsk_bug_assert(self->num_samples == NULL);
        tsk_bug_assert(self->num_tracked_samples == NULL);
    }
    if (self->options & TSK_SAMPLE_LISTS) {
        tsk_bug_assert(self->right_sample != NULL);
        tsk_bug_assert(self->left_sample != NULL);
        tsk_bug_assert(self->next_sample != NULL);
    } else {
        tsk_bug_assert(self->right_sample == NULL);
        tsk_bug_assert(self->left_sample == NULL);
        tsk_bug_assert(self->next_sample == NULL);
    }

    free(children);
    free(is_root);
}

void
tsk_tree_print_state(const tsk_tree_t *self, FILE *out)
{
    size_t j;
    tsk_site_t site;

    fprintf(out, "Tree state:\n");
    fprintf(out, "options = %d\n", self->options);
    fprintf(out, "root_threshold = %d\n", self->root_threshold);
    fprintf(out, "left = %f\n", self->left);
    fprintf(out, "right = %f\n", self->right);
    fprintf(out, "left_root = %d\n", (int) self->left_root);
    fprintf(out, "index = %d\n", (int) self->index);
    fprintf(out, "node\tparent\tlchild\trchild\tlsib\trsib");
    if (self->options & TSK_SAMPLE_LISTS) {
        fprintf(out, "\thead\ttail");
    }
    fprintf(out, "\n");

    for (j = 0; j < self->num_nodes; j++) {
        fprintf(out, "%d\t%d\t%d\t%d\t%d\t%d", (int) j, self->parent[j],
            self->left_child[j], self->right_child[j], self->left_sib[j],
            self->right_sib[j]);
        if (self->options & TSK_SAMPLE_LISTS) {
            fprintf(out, "\t%d\t%d\t", self->left_sample[j], self->right_sample[j]);
        }
        if (!(self->options & TSK_NO_SAMPLE_COUNTS)) {
            fprintf(out, "\t%d\t%d\t%d", (int) self->num_samples[j],
                (int) self->num_tracked_samples[j], self->marked[j]);
        }
        fprintf(out, "\n");
    }
    fprintf(out, "sites = \n");
    for (j = 0; j < self->sites_length; j++) {
        site = self->sites[j];
        fprintf(out, "\t%d\t%f\n", site.id, site.position);
    }
    tsk_tree_check_state(self);
}

/* Methods for positioning the tree along the sequence */

/* parent, left_child and right_sib are restrict pointers in the calling function,
 * so we pass these as parameters to ensure the relationships are clear to the
 * compiler. */
static inline void
tsk_tree_update_sample_lists(tsk_tree_t *self, const tsk_id_t *restrict parent,
    const tsk_id_t *restrict left_child, const tsk_id_t *restrict right_sib,
    tsk_id_t node)
{
    tsk_id_t u, v, sample_index;
    tsk_id_t *restrict left = self->left_sample;
    tsk_id_t *restrict right = self->right_sample;
    tsk_id_t *restrict next = self->next_sample;
    const tsk_id_t *restrict sample_index_map = self->tree_sequence->sample_index_map;

    for (u = node; u != TSK_NULL; u = parent[u]) {
        sample_index = sample_index_map[u];
        if (sample_index != TSK_NULL) {
            right[u] = left[u];
        } else {
            left[u] = TSK_NULL;
            right[u] = TSK_NULL;
        }
        for (v = left_child[u]; v != TSK_NULL; v = right_sib[v]) {
            if (left[v] != TSK_NULL) {
                tsk_bug_assert(right[v] != TSK_NULL);
                if (left[u] == TSK_NULL) {
                    left[u] = left[v];
                    right[u] = right[v];
                } else {
                    next[right[u]] = left[v];
                    right[u] = right[v];
                }
            }
        }
    }
}

static void
tsk_tree_remove_edge(tsk_tree_t *self, tsk_id_t p, tsk_id_t c)
{
    tsk_id_t *restrict parent = self->parent;
    tsk_id_t *restrict left_child = self->left_child;
    tsk_id_t *restrict right_child = self->right_child;
    tsk_id_t *restrict left_sib = self->left_sib;
    tsk_id_t *restrict right_sib = self->right_sib;
    tsk_id_t *restrict num_samples = self->num_samples;
    tsk_id_t *restrict num_tracked_samples = self->num_tracked_samples;
    uint8_t *restrict marked = self->marked;
    const uint8_t mark = self->mark;
    const tsk_id_t root_threshold = (tsk_id_t) self->root_threshold;
    tsk_id_t lsib, rsib, u, path_end, lroot, rroot;
    bool path_end_was_root;

#define IS_ROOT(U) (num_samples[U] >= root_threshold)

    lsib = left_sib[c];
    rsib = right_sib[c];
    if (lsib == TSK_NULL) {
        left_child[p] = rsib;
    } else {
        right_sib[lsib] = rsib;
    }
    if (rsib == TSK_NULL) {
        right_child[p] = lsib;
    } else {
        left_sib[rsib] = lsib;
    }
    parent[c] = TSK_NULL;
    left_sib[c] = TSK_NULL;
    right_sib[c] = TSK_NULL;

    if (!(self->options & TSK_NO_SAMPLE_COUNTS)) {
        /* keep the compiler happy */
        path_end_was_root = false;
        path_end = TSK_NULL;

        u = p;
        while (u != TSK_NULL) {
            path_end = u;
            path_end_was_root = IS_ROOT(u);
            num_samples[u] -= num_samples[c];
            num_tracked_samples[u] -= num_tracked_samples[c];
            marked[u] = mark;
            u = parent[u];
        }
        if (path_end_was_root && !IS_ROOT(path_end)) {
            /* remove path_end from the list of roots */
            lroot = left_sib[path_end];
            rroot = right_sib[path_end];
            self->left_root = TSK_NULL;
            if (lroot != TSK_NULL) {
                right_sib[lroot] = rroot;
                self->left_root = lroot;
            }
            if (rroot != TSK_NULL) {
                left_sib[rroot] = lroot;
                self->left_root = rroot;
            }
            left_sib[path_end] = TSK_NULL;
            right_sib[path_end] = TSK_NULL;
        }
        if (IS_ROOT(c)) {
            if (self->left_root != TSK_NULL) {
                lroot = left_sib[self->left_root];
                if (lroot != TSK_NULL) {
                    right_sib[lroot] = c;
                }
                left_sib[c] = lroot;
                left_sib[self->left_root] = c;
            }
            right_sib[c] = self->left_root;
            self->left_root = c;
        }
    }

    if (self->options & TSK_SAMPLE_LISTS) {
        tsk_tree_update_sample_lists(self, parent, left_child, right_sib, p);
    }
}

static void
tsk_tree_insert_edge(tsk_tree_t *self, tsk_id_t p, tsk_id_t c)
{
    tsk_id_t *restrict parent = self->parent;
    tsk_id_t *restrict left_child = self->left_child;
    tsk_id_t *restrict right_child = self->right_child;
    tsk_id_t *restrict left_sib = self->left_sib;
    tsk_id_t *restrict right_sib = self->right_sib;
    tsk_id_t *restrict num_samples = self->num_samples;
    tsk_id_t *restrict num_tracked_samples = self->num_tracked_samples;
    uint8_t *restrict marked = self->marked;
    const uint8_t mark = self->mark;
    const tsk_id_t root_threshold = (tsk_id_t) self->root_threshold;
    tsk_id_t lsib, rsib, u, path_end, lroot;
    bool path_end_was_root;

#define IS_ROOT(U) (num_samples[U] >= root_threshold)

    parent[c] = p;
    u = right_child[p];
    lsib = left_sib[c];
    rsib = right_sib[c];
    if (u == TSK_NULL) {
        left_child[p] = c;
        left_sib[c] = TSK_NULL;
        right_sib[c] = TSK_NULL;
    } else {
        right_sib[u] = c;
        left_sib[c] = u;
        right_sib[c] = TSK_NULL;
    }
    right_child[p] = c;

    if (!(self->options & TSK_NO_SAMPLE_COUNTS)) {

        /* keep compiler happy */
        path_end = TSK_NULL;
        path_end_was_root = false;

        u = p;
        while (u != TSK_NULL) {
            path_end = u;
            path_end_was_root = IS_ROOT(u);
            num_samples[u] += num_samples[c];
            num_tracked_samples[u] += num_tracked_samples[c];
            marked[u] = mark;
            u = parent[u];
        }

        if (IS_ROOT(c)) {
            if (path_end_was_root) {
                /* Remove c from the root list */
                self->left_root = TSK_NULL;
                if (lsib != TSK_NULL) {
                    right_sib[lsib] = rsib;
                    self->left_root = lsib;
                }
                if (rsib != TSK_NULL) {
                    left_sib[rsib] = lsib;
                    self->left_root = rsib;
                }
            } else {
                /* Replace c with path_end in root list */
                if (lsib != TSK_NULL) {
                    right_sib[lsib] = path_end;
                }
                if (rsib != TSK_NULL) {
                    left_sib[rsib] = path_end;
                }
                left_sib[path_end] = lsib;
                right_sib[path_end] = rsib;
                self->left_root = path_end;
            }
        } else {
            if (IS_ROOT(path_end) && !path_end_was_root) {
                /* Add a path_end as new root */
                if (self->left_root != TSK_NULL) {
                    lroot = left_sib[self->left_root];
                    if (lroot != TSK_NULL) {
                        right_sib[lroot] = path_end;
                    }
                    left_sib[path_end] = lroot;
                    left_sib[self->left_root] = path_end;
                }
                right_sib[path_end] = self->left_root;
                self->left_root = path_end;
            }
        }
    }
    if (self->options & TSK_SAMPLE_LISTS) {
        tsk_tree_update_sample_lists(self, parent, left_child, right_sib, p);
    }
}

static int
tsk_tree_advance(tsk_tree_t *self, int direction, const double *restrict out_breakpoints,
    const tsk_id_t *restrict out_order, tsk_id_t *out_index,
    const double *restrict in_breakpoints, const tsk_id_t *restrict in_order,
    tsk_id_t *in_index)
{
    int ret = 0;
    const int direction_change = direction * (direction != self->direction);
    tsk_id_t in = *in_index + direction_change;
    tsk_id_t out = *out_index + direction_change;
    tsk_id_t k;
    const tsk_table_collection_t *tables = self->tree_sequence->tables;
    const double sequence_length = tables->sequence_length;
    const tsk_id_t num_edges = (tsk_id_t) tables->edges.num_rows;
    const tsk_id_t *restrict edge_parent = tables->edges.parent;
    const tsk_id_t *restrict edge_child = tables->edges.child;
    double x;

    if (direction == TSK_DIR_FORWARD) {
        x = self->right;
    } else {
        x = self->left;
    }
    while (out >= 0 && out < num_edges && out_breakpoints[out_order[out]] == x) {
        tsk_bug_assert(out < num_edges);
        k = out_order[out];
        out += direction;
        tsk_tree_remove_edge(self, edge_parent[k], edge_child[k]);
    }

    while (in >= 0 && in < num_edges && in_breakpoints[in_order[in]] == x) {
        k = in_order[in];
        in += direction;
        tsk_tree_insert_edge(self, edge_parent[k], edge_child[k]);
    }

    if (self->left_root != TSK_NULL) {
        /* Ensure that left_root is the left-most root */
        while (self->left_sib[self->left_root] != TSK_NULL) {
            self->left_root = self->left_sib[self->left_root];
        }
    }

    self->direction = direction;
    self->index = self->index + direction;
    if (direction == TSK_DIR_FORWARD) {
        self->left = x;
        self->right = sequence_length;
        if (out >= 0 && out < num_edges) {
            self->right = TSK_MIN(self->right, out_breakpoints[out_order[out]]);
        }
        if (in >= 0 && in < num_edges) {
            self->right = TSK_MIN(self->right, in_breakpoints[in_order[in]]);
        }
    } else {
        self->right = x;
        self->left = 0;
        if (out >= 0 && out < num_edges) {
            self->left = TSK_MAX(self->left, out_breakpoints[out_order[out]]);
        }
        if (in >= 0 && in < num_edges) {
            self->left = TSK_MAX(self->left, in_breakpoints[in_order[in]]);
        }
    }
    tsk_bug_assert(self->left < self->right);
    *out_index = out;
    *in_index = in;
    if (tables->sites.num_rows > 0) {
        self->sites = self->tree_sequence->tree_sites[self->index];
        self->sites_length = self->tree_sequence->tree_sites_length[self->index];
    }
    ret = 1;
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_first(tsk_tree_t *self)
{
    int ret = 1;
    tsk_table_collection_t *tables = self->tree_sequence->tables;

    self->left = 0;
    self->index = 0;
    self->right = tables->sequence_length;
    self->sites = self->tree_sequence->tree_sites[0];
    self->sites_length = self->tree_sequence->tree_sites_length[0];

    if (tables->edges.num_rows > 0) {
        /* TODO this is redundant if this is the first usage of the tree. We
         * should add a state machine here so we know what state the tree is
         * in and can take the appropriate actions.
         */
        ret = tsk_tree_clear(self);
        if (ret != 0) {
            goto out;
        }
        self->index = -1;
        self->left_index = 0;
        self->right_index = 0;
        self->direction = TSK_DIR_FORWARD;
        self->right = 0;

        ret = tsk_tree_advance(self, TSK_DIR_FORWARD, tables->edges.right,
            tables->indexes.edge_removal_order, &self->right_index, tables->edges.left,
            tables->indexes.edge_insertion_order, &self->left_index);
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_last(tsk_tree_t *self)
{
    int ret = 1;
    const tsk_treeseq_t *ts = self->tree_sequence;
    const tsk_table_collection_t *tables = ts->tables;

    self->left = 0;
    self->right = tables->sequence_length;
    self->index = 0;
    self->sites = ts->tree_sites[0];
    self->sites_length = ts->tree_sites_length[0];

    if (tables->edges.num_rows > 0) {
        /* TODO this is redundant if this is the first usage of the tree. We
         * should add a state machine here so we know what state the tree is
         * in and can take the appropriate actions.
         */
        ret = tsk_tree_clear(self);
        if (ret != 0) {
            goto out;
        }
        self->index = (tsk_id_t) tsk_treeseq_get_num_trees(ts);
        self->left_index = (tsk_id_t) tables->edges.num_rows - 1;
        self->right_index = (tsk_id_t) tables->edges.num_rows - 1;
        self->direction = TSK_DIR_REVERSE;
        self->left = tables->sequence_length;
        self->right = 0;

        ret = tsk_tree_advance(self, TSK_DIR_REVERSE, tables->edges.left,
            tables->indexes.edge_insertion_order, &self->left_index, tables->edges.right,
            tables->indexes.edge_removal_order, &self->right_index);
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_next(tsk_tree_t *self)
{
    int ret = 0;
    const tsk_treeseq_t *ts = self->tree_sequence;
    const tsk_table_collection_t *tables = ts->tables;
    tsk_id_t num_trees = (tsk_id_t) tsk_treeseq_get_num_trees(ts);

    if (self->index == -1) {
        ret = tsk_tree_first(self);
    } else if (self->index < num_trees - 1) {
        ret = tsk_tree_advance(self, TSK_DIR_FORWARD, tables->edges.right,
            tables->indexes.edge_removal_order, &self->right_index, tables->edges.left,
            tables->indexes.edge_insertion_order, &self->left_index);
    } else {
        ret = tsk_tree_clear(self);
    }
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_prev(tsk_tree_t *self)
{
    int ret = 0;
    const tsk_table_collection_t *tables = self->tree_sequence->tables;

    if (self->index == -1) {
        ret = tsk_tree_last(self);
    } else if (self->index > 0) {
        ret = tsk_tree_advance(self, TSK_DIR_REVERSE, tables->edges.left,
            tables->indexes.edge_insertion_order, &self->left_index, tables->edges.right,
            tables->indexes.edge_removal_order, &self->right_index);
    } else {
        ret = tsk_tree_clear(self);
    }
    return ret;
}

/* Parsimony methods */

static inline uint64_t
set_bit(uint64_t value, int8_t bit)
{
    return value | (1ULL << bit);
}

static inline bool
bit_is_set(uint64_t value, int8_t bit)
{
    return (value & (1ULL << bit)) != 0;
}

static inline int8_t
get_smallest_set_bit(uint64_t v)
{
    /* This is an inefficient implementation, there are several better
     * approaches. On GCC we can use
     * return (uint8_t) (__builtin_ffsll((long long) v) - 1);
     */
    uint64_t t = 1;
    int8_t r = 0;

    assert(v != 0);
    while ((v & t) == 0) {
        t <<= 1;
        r++;
    }
    return r;
}

#define HARTIGAN_MAX_ALLELES 64

/* This interface is experimental. In the future, we should provide the option to
 * use a general cost matrix, in which case we'll use the Sankoff algorithm. For
 * now this is unused.
 *
 * The algorithm used here is Hartigan parsimony, "Minimum Mutation Fits to a
 * Given Tree", Biometrics 1973.
 */
int TSK_WARN_UNUSED
tsk_tree_map_mutations(tsk_tree_t *self, int8_t *genotypes,
    double *TSK_UNUSED(cost_matrix), tsk_flags_t TSK_UNUSED(options),
    int8_t *r_ancestral_state, tsk_size_t *r_num_transitions,
    tsk_state_transition_t **r_transitions)
{
    int ret = 0;
    struct stack_elem {
        tsk_id_t node;
        tsk_id_t transition_parent;
        int8_t state;
    };
    const tsk_size_t num_samples = self->tree_sequence->num_samples;
    const tsk_size_t num_nodes = self->num_nodes;
    const tsk_id_t *restrict left_child = self->left_child;
    const tsk_id_t *restrict right_sib = self->right_sib;
    const tsk_id_t *restrict parent = self->parent;
    const tsk_flags_t *restrict node_flags = self->tree_sequence->tables->nodes.flags;
    uint64_t optimal_root_set;
    uint64_t *restrict optimal_set = calloc(num_nodes, sizeof(*optimal_set));
    tsk_id_t *restrict postorder_stack = malloc(num_nodes * sizeof(*postorder_stack));
    struct stack_elem *restrict preorder_stack
        = malloc(num_nodes * sizeof(*preorder_stack));
    tsk_id_t postorder_parent, root, u, v;
    /* The largest possible number of transitions is one over every sample */
    tsk_state_transition_t *transitions = malloc(num_samples * sizeof(*transitions));
    int8_t allele, ancestral_state;
    int stack_top;
    struct stack_elem s;
    tsk_size_t j, num_transitions, max_allele_count;
    tsk_size_t allele_count[HARTIGAN_MAX_ALLELES];
    tsk_size_t non_missing = 0;
    int8_t num_alleles = 0;

    if (optimal_set == NULL || preorder_stack == NULL || postorder_stack == NULL
        || transitions == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    for (j = 0; j < num_samples; j++) {
        if (genotypes[j] >= HARTIGAN_MAX_ALLELES || genotypes[j] < TSK_MISSING_DATA) {
            ret = TSK_ERR_BAD_GENOTYPE;
            goto out;
        }
        u = self->tree_sequence->samples[j];
        if (genotypes[j] == TSK_MISSING_DATA) {
            /* All bits set */
            optimal_set[u] = UINT64_MAX;
        } else {
            optimal_set[u] = set_bit(optimal_set[u], genotypes[j]);
            num_alleles = TSK_MAX(genotypes[j], num_alleles);
            non_missing++;
        }
    }

    if (non_missing == 0) {
        ret = TSK_ERR_GENOTYPES_ALL_MISSING;
        goto out;
    }
    num_alleles++;

    for (root = self->left_root; root != TSK_NULL; root = self->right_sib[root]) {
        /* Do a post order traversal */
        postorder_stack[0] = root;
        stack_top = 0;
        postorder_parent = TSK_NULL;
        while (stack_top >= 0) {
            u = postorder_stack[stack_top];
            if (left_child[u] != TSK_NULL && u != postorder_parent) {
                for (v = left_child[u]; v != TSK_NULL; v = right_sib[v]) {
                    stack_top++;
                    postorder_stack[stack_top] = v;
                }
            } else {
                stack_top--;
                postorder_parent = parent[u];

                /* Visit u */
                memset(allele_count, 0, ((size_t) num_alleles) * sizeof(*allele_count));
                for (v = left_child[u]; v != TSK_NULL; v = right_sib[v]) {
                    for (allele = 0; allele < num_alleles; allele++) {
                        allele_count[allele] += bit_is_set(optimal_set[v], allele);
                    }
                }
                if (!(node_flags[u] & TSK_NODE_IS_SAMPLE)) {
                    max_allele_count = 0;
                    for (allele = 0; allele < num_alleles; allele++) {
                        max_allele_count
                            = TSK_MAX(max_allele_count, allele_count[allele]);
                    }
                    for (allele = 0; allele < num_alleles; allele++) {
                        if (allele_count[allele] == max_allele_count) {
                            optimal_set[u] = set_bit(optimal_set[u], allele);
                        }
                    }
                }
            }
        }
    }

    optimal_root_set = 0;
    /* TODO it's annoying that this is essentially the same as the
     * visit function above. It would be nice if we had an extra
     * node that was the parent of all roots, then the algorithm
     * would work as-is */
    memset(allele_count, 0, ((size_t) num_alleles) * sizeof(*allele_count));
    for (root = self->left_root; root != TSK_NULL; root = right_sib[root]) {
        for (allele = 0; allele < num_alleles; allele++) {
            allele_count[allele] += bit_is_set(optimal_set[root], allele);
        }
    }
    max_allele_count = 0;
    for (allele = 0; allele < num_alleles; allele++) {
        max_allele_count = TSK_MAX(max_allele_count, allele_count[allele]);
    }
    for (allele = 0; allele < num_alleles; allele++) {
        if (allele_count[allele] == max_allele_count) {
            optimal_root_set = set_bit(optimal_root_set, allele);
        }
    }
    ancestral_state = get_smallest_set_bit(optimal_root_set);

    num_transitions = 0;
    for (root = self->left_root; root != TSK_NULL; root = self->right_sib[root]) {
        /* Do a preorder traversal */
        preorder_stack[0].node = root;
        preorder_stack[0].state = ancestral_state;
        preorder_stack[0].transition_parent = TSK_NULL;
        stack_top = 0;
        while (stack_top >= 0) {
            s = preorder_stack[stack_top];
            stack_top--;

            if (!bit_is_set(optimal_set[s.node], s.state)) {
                s.state = get_smallest_set_bit(optimal_set[s.node]);
                transitions[num_transitions].node = s.node;
                transitions[num_transitions].parent = s.transition_parent;
                transitions[num_transitions].state = s.state;
                s.transition_parent = (tsk_id_t) num_transitions;
                num_transitions++;
            }
            for (v = left_child[s.node]; v != TSK_NULL; v = right_sib[v]) {
                stack_top++;
                s.node = v;
                preorder_stack[stack_top] = s;
            }
        }
    }

    *r_transitions = transitions;
    *r_num_transitions = num_transitions;
    *r_ancestral_state = ancestral_state;
    transitions = NULL;
out:
    tsk_safe_free(transitions);
    /* Cannot safe_free because of 'restrict' */
    if (optimal_set != NULL) {
        free(optimal_set);
    }
    if (preorder_stack != NULL) {
        free(preorder_stack);
    }
    if (postorder_stack != NULL) {
        free(postorder_stack);
    }
    return ret;
}

/* ======================================================== *
 * Tree diff iterator.
 * ======================================================== */

int TSK_WARN_UNUSED
tsk_diff_iter_init(
    tsk_diff_iter_t *self, const tsk_treeseq_t *tree_sequence, tsk_flags_t options)
{
    int ret = 0;

    tsk_bug_assert(tree_sequence != NULL);
    memset(self, 0, sizeof(tsk_diff_iter_t));
    self->num_nodes = tsk_treeseq_get_num_nodes(tree_sequence);
    self->num_edges = tsk_treeseq_get_num_edges(tree_sequence);
    self->tree_sequence = tree_sequence;
    self->insertion_index = 0;
    self->removal_index = 0;
    self->tree_left = 0;
    self->tree_index = -1;
    self->last_index = (tsk_id_t) tsk_treeseq_get_num_trees(tree_sequence);
    if (options & TSK_INCLUDE_TERMINAL) {
        self->last_index = self->last_index + 1;
    }
    self->edge_list_nodes = malloc(self->num_edges * sizeof(*self->edge_list_nodes));
    if (self->edge_list_nodes == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
out:
    return ret;
}

int
tsk_diff_iter_free(tsk_diff_iter_t *self)
{
    tsk_safe_free(self->edge_list_nodes);
    return 0;
}

void
tsk_diff_iter_print_state(const tsk_diff_iter_t *self, FILE *out)
{
    fprintf(out, "tree_diff_iterator state\n");
    fprintf(out, "num_edges = %d\n", (int) self->num_edges);
    fprintf(out, "insertion_index = %d\n", (int) self->insertion_index);
    fprintf(out, "removal_index = %d\n", (int) self->removal_index);
    fprintf(out, "tree_left = %f\n", self->tree_left);
    fprintf(out, "tree_index = %d\n", (int) self->tree_index);
}

int TSK_WARN_UNUSED
tsk_diff_iter_next(tsk_diff_iter_t *self, double *ret_left, double *ret_right,
    tsk_edge_list_t *edges_out_ret, tsk_edge_list_t *edges_in_ret)
{
    int ret = 0;
    tsk_id_t k;
    const double sequence_length = self->tree_sequence->tables->sequence_length;
    double left = self->tree_left;
    double right = sequence_length;
    tsk_size_t next_edge_list_node = 0;
    const tsk_treeseq_t *s = self->tree_sequence;
    tsk_edge_list_node_t *out_head = NULL;
    tsk_edge_list_node_t *out_tail = NULL;
    tsk_edge_list_node_t *in_head = NULL;
    tsk_edge_list_node_t *in_tail = NULL;
    tsk_edge_list_node_t *w = NULL;
    tsk_edge_list_t edges_out;
    tsk_edge_list_t edges_in;
    const tsk_edge_table_t *edges = &s->tables->edges;
    const tsk_id_t *insertion_order = s->tables->indexes.edge_insertion_order;
    const tsk_id_t *removal_order = s->tables->indexes.edge_removal_order;

    memset(&edges_out, 0, sizeof(edges_out));
    memset(&edges_in, 0, sizeof(edges_in));

    if (self->tree_index + 1 < self->last_index) {
        /* First we remove the stale records */
        while (self->removal_index < (tsk_id_t) self->num_edges
               && left == edges->right[removal_order[self->removal_index]]) {
            k = removal_order[self->removal_index];
            tsk_bug_assert(next_edge_list_node < self->num_edges);
            w = &self->edge_list_nodes[next_edge_list_node];
            next_edge_list_node++;
            w->edge.id = k;
            w->edge.left = edges->left[k];
            w->edge.right = edges->right[k];
            w->edge.parent = edges->parent[k];
            w->edge.child = edges->child[k];
            w->edge.metadata = edges->metadata + edges->metadata_offset[k];
            w->edge.metadata_length
                = edges->metadata_offset[k + 1] - edges->metadata_offset[k];
            w->next = NULL;
            w->prev = NULL;
            if (out_head == NULL) {
                out_head = w;
                out_tail = w;
            } else {
                out_tail->next = w;
                w->prev = out_tail;
                out_tail = w;
            }
            self->removal_index++;
        }
        edges_out.head = out_head;
        edges_out.tail = out_tail;

        /* Now insert the new records */
        while (self->insertion_index < (tsk_id_t) self->num_edges
               && left == edges->left[insertion_order[self->insertion_index]]) {
            k = insertion_order[self->insertion_index];
            tsk_bug_assert(next_edge_list_node < self->num_edges);
            w = &self->edge_list_nodes[next_edge_list_node];
            next_edge_list_node++;
            w->edge.id = k;
            w->edge.left = edges->left[k];
            w->edge.right = edges->right[k];
            w->edge.parent = edges->parent[k];
            w->edge.child = edges->child[k];
            w->edge.metadata = edges->metadata + edges->metadata_offset[k];
            w->edge.metadata_length
                = edges->metadata_offset[k + 1] - edges->metadata_offset[k];
            w->next = NULL;
            w->prev = NULL;
            if (in_head == NULL) {
                in_head = w;
                in_tail = w;
            } else {
                in_tail->next = w;
                w->prev = in_tail;
                in_tail = w;
            }
            self->insertion_index++;
        }
        edges_in.head = in_head;
        edges_in.tail = in_tail;

        right = sequence_length;
        if (self->insertion_index < (tsk_id_t) self->num_edges) {
            right = TSK_MIN(right, edges->left[insertion_order[self->insertion_index]]);
        }
        if (self->removal_index < (tsk_id_t) self->num_edges) {
            right = TSK_MIN(right, edges->right[removal_order[self->removal_index]]);
        }
        self->tree_index++;
        ret = 1;
    }
    *edges_out_ret = edges_out;
    *edges_in_ret = edges_in;
    *ret_left = left;
    *ret_right = right;
    /* Set the left coordinate for the next tree */
    self->tree_left = right;
    return ret;
}

/* ======================================================== *
 * KC Distance
 * ======================================================== */

typedef struct {
    tsk_size_t *m;
    double *M;
    tsk_id_t n;
    tsk_id_t N;
} kc_vectors;

static int
kc_vectors_alloc(kc_vectors *self, tsk_id_t n)
{
    int ret = 0;

    self->n = n;
    self->N = (n * (n - 1)) / 2;
    self->m = calloc((size_t)(self->N + self->n), sizeof(tsk_size_t));
    self->M = calloc((size_t)(self->N + self->n), sizeof(double));
    if (self->m == NULL || self->M == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }

out:
    return ret;
}

static void
kc_vectors_free(kc_vectors *self)
{
    tsk_safe_free(self->m);
    tsk_safe_free(self->M);
}

static inline void
update_kc_vectors_single_sample(
    const tsk_treeseq_t *ts, kc_vectors *kc_vecs, tsk_id_t u, double time)
{
    const tsk_id_t *sample_index_map = ts->sample_index_map;
    tsk_id_t u_index = sample_index_map[u];

    kc_vecs->m[kc_vecs->N + u_index] = 1;
    kc_vecs->M[kc_vecs->N + u_index] = time;
}

static inline void
update_kc_vectors_all_pairs(const tsk_tree_t *tree, kc_vectors *kc_vecs, tsk_id_t u,
    tsk_id_t v, tsk_size_t depth, double time)
{
    tsk_id_t sample1_index, sample2_index, n1, n2, tmp, pair_index;
    const tsk_id_t *restrict left_sample = tree->left_sample;
    const tsk_id_t *restrict right_sample = tree->right_sample;
    const tsk_id_t *restrict next_sample = tree->next_sample;
    tsk_size_t *restrict kc_m = kc_vecs->m;
    double *restrict kc_M = kc_vecs->M;

    sample1_index = left_sample[u];
    while (sample1_index != TSK_NULL) {
        sample2_index = left_sample[v];
        while (sample2_index != TSK_NULL) {
            n1 = sample1_index;
            n2 = sample2_index;
            if (n1 > n2) {
                tmp = n1;
                n1 = n2;
                n2 = tmp;
            }

            /* We spend ~40% of our time here because these accesses
             * are not in order and gets very poor cache behavior */
            pair_index = n2 - n1 - 1 + (-1 * n1 * (n1 - 2 * kc_vecs->n + 1)) / 2;
            kc_m[pair_index] = depth;
            kc_M[pair_index] = time;

            if (sample2_index == right_sample[v]) {
                break;
            }
            sample2_index = next_sample[sample2_index];
        }
        if (sample1_index == right_sample[u]) {
            break;
        }
        sample1_index = next_sample[sample1_index];
    }
}

struct kc_stack_elmt {
    tsk_id_t node;
    tsk_size_t depth;
};

static int
fill_kc_vectors(const tsk_tree_t *t, kc_vectors *kc_vecs)
{
    int stack_top;
    tsk_size_t depth;
    double time;
    const double *times;
    struct kc_stack_elmt *stack;
    tsk_id_t root, u, c1, c2;
    int ret = 0;
    const tsk_treeseq_t *ts = t->tree_sequence;

    stack = malloc(t->num_nodes * sizeof(*stack));
    if (stack == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }

    times = t->tree_sequence->tables->nodes.time;

    for (root = t->left_root; root != TSK_NULL; root = t->right_sib[root]) {
        stack_top = 0;
        stack[stack_top].node = root;
        stack[stack_top].depth = 0;
        while (stack_top >= 0) {
            u = stack[stack_top].node;
            depth = stack[stack_top].depth;
            stack_top--;

            if (tsk_tree_is_sample(t, u)) {
                time = tsk_tree_branch_length(t, u);
                update_kc_vectors_single_sample(ts, kc_vecs, u, time);
            }

            /* Don't bother going deeper if there are no samples under this node */
            if (t->left_sample[u] != TSK_NULL) {
                for (c1 = t->left_child[u]; c1 != TSK_NULL; c1 = t->right_sib[c1]) {
                    stack_top++;
                    stack[stack_top].node = c1;
                    stack[stack_top].depth = depth + 1;

                    for (c2 = t->right_sib[c1]; c2 != TSK_NULL; c2 = t->right_sib[c2]) {
                        time = times[root] - times[u];
                        update_kc_vectors_all_pairs(t, kc_vecs, c1, c2, depth, time);
                    }
                }
            }
        }
    }

out:
    tsk_safe_free(stack);
    return ret;
}

static double
norm_kc_vectors(kc_vectors *self, kc_vectors *other, double lambda)
{
    double vT1, vT2, distance_sum;
    tsk_id_t i;

    distance_sum = 0;
    for (i = 0; i < self->n + self->N; i++) {
        vT1 = (self->m[i] * (1 - lambda)) + (lambda * self->M[i]);
        vT2 = (other->m[i] * (1 - lambda)) + (lambda * other->M[i]);
        distance_sum += (vT1 - vT2) * (vT1 - vT2);
    }

    return sqrt(distance_sum);
}

static int
check_kc_distance_tree_inputs(const tsk_tree_t *self)
{
    tsk_id_t u, num_nodes, left_child;
    int ret = 0;

    if (tsk_tree_get_num_roots(self) != 1) {
        ret = TSK_ERR_MULTIPLE_ROOTS;
        goto out;
    }
    if (!tsk_tree_has_sample_lists(self)) {
        ret = TSK_ERR_NO_SAMPLE_LISTS;
        goto out;
    }

    num_nodes = (tsk_id_t) tsk_treeseq_get_num_nodes(self->tree_sequence);
    for (u = 0; u < num_nodes; u++) {
        left_child = self->left_child[u];
        if (left_child != TSK_NULL && left_child == self->right_child[u]) {
            ret = TSK_ERR_UNARY_NODES;
            goto out;
        }
    }
out:
    return ret;
}

static int
check_kc_distance_samples_inputs(const tsk_treeseq_t *self, const tsk_treeseq_t *other)
{
    const tsk_id_t *samples, *other_samples;
    tsk_id_t i, n;
    int ret = 0;

    if (self->num_samples != other->num_samples) {
        ret = TSK_ERR_SAMPLE_SIZE_MISMATCH;
        goto out;
    }

    samples = self->samples;
    other_samples = other->samples;
    n = (tsk_id_t) self->num_samples;
    for (i = 0; i < n; i++) {
        if (samples[i] != other_samples[i]) {
            ret = TSK_ERR_SAMPLES_NOT_EQUAL;
            goto out;
        }
    }
out:
    return ret;
}

int
tsk_tree_kc_distance(
    const tsk_tree_t *self, const tsk_tree_t *other, double lambda, double *result)
{
    tsk_id_t n, i;
    kc_vectors vecs[2];
    const tsk_tree_t *trees[2] = { self, other };
    int ret = 0;

    for (i = 0; i < 2; i++) {
        memset(&vecs[i], 0, sizeof(kc_vectors));
    }

    ret = check_kc_distance_samples_inputs(self->tree_sequence, other->tree_sequence);
    if (ret != 0) {
        goto out;
    }
    for (i = 0; i < 2; i++) {
        ret = check_kc_distance_tree_inputs(trees[i]);
        if (ret != 0) {
            goto out;
        }
    }

    n = (tsk_id_t) self->tree_sequence->num_samples;
    for (i = 0; i < 2; i++) {
        ret = kc_vectors_alloc(&vecs[i], n);
        if (ret != 0) {
            goto out;
        }
        ret = fill_kc_vectors(trees[i], &vecs[i]);
        if (ret != 0) {
            goto out;
        }
    }

    *result = norm_kc_vectors(&vecs[0], &vecs[1], lambda);
out:
    for (i = 0; i < 2; i++) {
        kc_vectors_free(&vecs[i]);
    }
    return ret;
}

static int
check_kc_distance_tree_sequence_inputs(
    const tsk_treeseq_t *self, const tsk_treeseq_t *other)
{
    int ret = 0;

    if (self->tables->sequence_length != other->tables->sequence_length) {
        ret = TSK_ERR_SEQUENCE_LENGTH_MISMATCH;
        goto out;
    }

    ret = check_kc_distance_samples_inputs(self, other);
    if (ret != 0) {
        goto out;
    }

out:
    return ret;
}

static void
update_kc_pair_with_sample(const tsk_tree_t *self, kc_vectors *kc, tsk_id_t sample,
    tsk_size_t *depths, double root_time)
{
    tsk_id_t c, p, sib;
    double time;
    tsk_size_t depth;
    double *times = self->tree_sequence->tables->nodes.time;

    c = sample;
    for (p = self->parent[sample]; p != TSK_NULL; p = self->parent[p]) {
        time = root_time - times[p];
        depth = depths[p];
        for (sib = self->left_child[p]; sib != TSK_NULL; sib = self->right_sib[sib]) {
            if (sib != c) {
                update_kc_vectors_all_pairs(self, kc, sample, sib, depth, time);
            }
        }
        c = p;
    }
}

static int
update_kc_subtree_state(
    tsk_tree_t *t, kc_vectors *kc, tsk_id_t u, tsk_size_t *depths, double root_time)
{
    int stack_top;
    tsk_id_t v, c;
    tsk_id_t *stack = NULL;
    int ret = 0;

    stack = malloc(t->num_nodes * sizeof(*stack));
    if (stack == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }

    stack_top = 0;
    stack[stack_top] = u;
    while (stack_top >= 0) {
        v = stack[stack_top];
        stack_top--;

        if (tsk_tree_is_sample(t, v)) {
            update_kc_pair_with_sample(t, kc, v, depths, root_time);
        }
        for (c = t->left_child[v]; c != TSK_NULL; c = t->right_sib[c]) {
            if (depths[c] != 0) {
                depths[c] = depths[v] + 1;
                stack_top++;
                stack[stack_top] = c;
            }
        }
    }

out:
    tsk_safe_free(stack);
    return ret;
}

static int
update_kc_incremental(tsk_tree_t *self, kc_vectors *kc, tsk_edge_list_t *edges_out,
    tsk_edge_list_t *edges_in, tsk_size_t *depths)
{
    int ret = 0;
    tsk_edge_list_node_t *record;
    tsk_edge_t *e;
    tsk_id_t u;
    double root_time, time;
    const double *times = self->tree_sequence->tables->nodes.time;

    /* Update state of detached subtrees */
    for (record = edges_out->tail; record != NULL; record = record->prev) {
        e = &record->edge;
        u = e->child;
        depths[u] = 0;

        if (self->parent[u] == TSK_NULL) {
            root_time = times[tsk_tree_node_root(self, u)];
            ret = update_kc_subtree_state(self, kc, u, depths, root_time);
            if (ret != 0) {
                goto out;
            }
        }
    }

    /* Propagate state change down into reattached subtrees. */
    for (record = edges_in->tail; record != NULL; record = record->prev) {
        e = &record->edge;
        u = e->child;

        tsk_bug_assert(depths[e->child] == 0);
        depths[u] = depths[e->parent] + 1;

        root_time = times[tsk_tree_node_root(self, u)];
        ret = update_kc_subtree_state(self, kc, u, depths, root_time);
        if (ret != 0) {
            goto out;
        }

        if (tsk_tree_is_sample(self, u)) {
            time = tsk_tree_branch_length(self, u);
            update_kc_vectors_single_sample(self->tree_sequence, kc, u, time);
        }
    }

out:
    return ret;
}

int
tsk_treeseq_kc_distance(const tsk_treeseq_t *self, const tsk_treeseq_t *other,
    double lambda_, double *result)
{
    int i;
    tsk_id_t n;
    tsk_size_t num_nodes;
    double left, span, total;
    const tsk_treeseq_t *treeseqs[2] = { self, other };
    tsk_tree_t trees[2];
    kc_vectors kcs[2];
    tsk_diff_iter_t diff_iters[2];
    tsk_edge_list_t edges_out[2];
    tsk_edge_list_t edges_in[2];
    tsk_size_t *depths[2];
    double t0_left, t0_right, t1_left, t1_right;
    int ret = 0;

    for (i = 0; i < 2; i++) {
        memset(&trees[i], 0, sizeof(trees[i]));
        memset(&diff_iters[i], 0, sizeof(diff_iters[i]));
        memset(&kcs[i], 0, sizeof(kcs[i]));
        memset(&edges_out[i], 0, sizeof(edges_out[i]));
        memset(&edges_in[i], 0, sizeof(edges_in[i]));
        depths[i] = NULL;
    }

    ret = check_kc_distance_tree_sequence_inputs(self, other);
    if (ret != 0) {
        goto out;
    }

    n = (tsk_id_t) self->num_samples;
    for (i = 0; i < 2; i++) {
        ret = tsk_tree_init(&trees[i], treeseqs[i], TSK_SAMPLE_LISTS);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_diff_iter_init(&diff_iters[i], treeseqs[i], false);
        if (ret != 0) {
            goto out;
        }
        ret = kc_vectors_alloc(&kcs[i], n);
        if (ret != 0) {
            goto out;
        }
        num_nodes = tsk_treeseq_get_num_nodes(treeseqs[i]);
        depths[i] = calloc(num_nodes, sizeof(*depths[i]));
        if (depths[i] == NULL) {
            ret = TSK_ERR_NO_MEMORY;
            goto out;
        }
    }

    total = 0;
    left = 0;

    ret = tsk_tree_first(&trees[0]);
    if (ret != 1) {
        goto out;
    }
    ret = check_kc_distance_tree_inputs(&trees[0]);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_diff_iter_next(
        &diff_iters[0], &t0_left, &t0_right, &edges_out[0], &edges_in[0]);
    tsk_bug_assert(ret == 1);
    ret = update_kc_incremental(
        &trees[0], &kcs[0], &edges_out[0], &edges_in[0], depths[0]);
    if (ret != 0) {
        goto out;
    }
    while ((ret = tsk_tree_next(&trees[1])) == 1) {
        ret = check_kc_distance_tree_inputs(&trees[1]);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_diff_iter_next(
            &diff_iters[1], &t1_left, &t1_right, &edges_out[1], &edges_in[1]);
        tsk_bug_assert(ret == 1);

        ret = update_kc_incremental(
            &trees[1], &kcs[1], &edges_out[1], &edges_in[1], depths[1]);
        if (ret != 0) {
            goto out;
        }
        while (t0_right < t1_right) {
            span = t0_right - left;
            total += norm_kc_vectors(&kcs[0], &kcs[1], lambda_) * span;

            left = t0_right;
            ret = tsk_tree_next(&trees[0]);
            tsk_bug_assert(ret == 1);
            ret = check_kc_distance_tree_inputs(&trees[0]);
            if (ret != 0) {
                goto out;
            }
            ret = tsk_diff_iter_next(
                &diff_iters[0], &t0_left, &t0_right, &edges_out[0], &edges_in[0]);
            tsk_bug_assert(ret == 1);
            ret = update_kc_incremental(
                &trees[0], &kcs[0], &edges_out[0], &edges_in[0], depths[0]);
            if (ret != 0) {
                goto out;
            }
        }
        span = t1_right - left;
        left = t1_right;
        total += norm_kc_vectors(&kcs[0], &kcs[1], lambda_) * span;
    }
    if (ret != 0) {
        goto out;
    }

    *result = total / self->tables->sequence_length;
out:
    for (i = 0; i < 2; i++) {
        tsk_tree_free(&trees[i]);
        tsk_diff_iter_free(&diff_iters[i]);
        kc_vectors_free(&kcs[i]);
        tsk_safe_free(depths[i]);
    }
    return ret;
}
