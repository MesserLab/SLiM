/*
 * MIT License
 *
 * Copyright (c) 2019-2025 Tskit Developers
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
#include <tskit/genotypes.h>

static inline bool
is_discrete(double x)
{
    return trunc(x) == x;
}

/* ======================================================== *
 * tree sequence
 * ======================================================== */

static void
tsk_treeseq_check_state(const tsk_treeseq_t *self)
{
    tsk_size_t j;
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
    tsk_size_t j;
    tsk_size_t k, l, m;
    tsk_site_t site;

    fprintf(out, "tree_sequence state\n");
    fprintf(out, "num_trees = %lld\n", (long long) self->num_trees);
    fprintf(out, "samples = (%lld)\n", (long long) self->num_samples);
    for (j = 0; j < self->num_samples; j++) {
        fprintf(out, "\t%lld\n", (long long) self->samples[j]);
    }
    tsk_table_collection_print_state(self->tables, out);
    fprintf(out, "tree_sites = \n");
    for (j = 0; j < self->num_trees; j++) {
        fprintf(out, "tree %lld\t%lld sites\n", (long long) j,
            (long long) self->tree_sites_length[j]);
        for (k = 0; k < self->tree_sites_length[j]; k++) {
            site = self->tree_sites[j][k];
            fprintf(out, "\tsite %lld pos = %f ancestral state = ", (long long) site.id,
                site.position);
            for (l = 0; l < site.ancestral_state_length; l++) {
                fprintf(out, "%c", site.ancestral_state[l]);
            }
            fprintf(out, " %lld mutations\n", (long long) site.mutations_length);
            for (l = 0; l < site.mutations_length; l++) {
                fprintf(out, "\t\tmutation %lld node = %lld derived_state = ",
                    (long long) site.mutations[l].id,
                    (long long) site.mutations[l].node);
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
    tsk_size_t offset = 0;
    const tsk_size_t num_mutations = self->tables->mutations.num_rows;
    const tsk_size_t num_sites = self->tables->sites.num_rows;
    const tsk_id_t *restrict mutation_site = self->tables->mutations.site;
    const double *restrict site_position = self->tables->sites.position;
    bool discrete_sites = true;
    tsk_mutation_t *mutation;

    self->site_mutations_mem
        = tsk_malloc(num_mutations * sizeof(*self->site_mutations_mem));
    self->site_mutations_length
        = tsk_malloc(num_sites * sizeof(*self->site_mutations_length));
    self->site_mutations = tsk_malloc(num_sites * sizeof(*self->site_mutations));
    self->tree_sites_mem = tsk_malloc(num_sites * sizeof(*self->tree_sites_mem));
    if (self->site_mutations_mem == NULL || self->site_mutations_length == NULL
        || self->site_mutations == NULL || self->tree_sites_mem == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    for (k = 0; k < (tsk_id_t) num_mutations; k++) {
        mutation = self->site_mutations_mem + k;
        ret = tsk_treeseq_get_mutation(self, k, mutation);
        if (ret != 0) {
            goto out;
        }
    }
    k = 0;
    for (j = 0; j < (tsk_id_t) num_sites; j++) {
        discrete_sites = discrete_sites && is_discrete(site_position[j]);
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
    self->discrete_genome = self->discrete_genome && discrete_sites;
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
    const tsk_size_t num_inds = self->tables->individuals.num_rows;
    const tsk_size_t num_nodes = self->tables->nodes.num_rows;
    const tsk_id_t *restrict node_individual = self->tables->nodes.individual;

    // First find number of nodes per individual
    self->individual_nodes_length
        = tsk_calloc(TSK_MAX(1, num_inds), sizeof(*self->individual_nodes_length));
    node_count = tsk_calloc(TSK_MAX(1, num_inds), sizeof(*node_count));

    if (self->individual_nodes_length == NULL || node_count == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
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
        = tsk_malloc(TSK_MAX(1, total_node_refs) * sizeof(tsk_node_t));
    self->individual_nodes = tsk_malloc(TSK_MAX(1, num_inds) * sizeof(tsk_node_t *));
    if (self->individual_nodes_mem == NULL || self->individual_nodes == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
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
    tsk_size_t j, k, tree_index;
    tsk_id_t site_id, edge_id, mutation_id;
    double tree_left, tree_right;
    const double sequence_length = self->tables->sequence_length;
    const tsk_id_t num_sites = (tsk_id_t) self->tables->sites.num_rows;
    const tsk_id_t num_mutations = (tsk_id_t) self->tables->mutations.num_rows;
    const tsk_size_t num_edges = self->tables->edges.num_rows;
    const tsk_size_t num_nodes = self->tables->nodes.num_rows;
    const double *restrict site_position = self->tables->sites.position;
    const tsk_id_t *restrict mutation_site = self->tables->mutations.site;
    const tsk_id_t *restrict mutation_parent = self->tables->mutations.parent;
    const char *restrict sites_ancestral_state = self->tables->sites.ancestral_state;
    const tsk_size_t *restrict sites_ancestral_state_offset
        = self->tables->sites.ancestral_state_offset;
    const char *restrict mutations_derived_state = self->tables->mutations.derived_state;
    const tsk_size_t *restrict mutations_derived_state_offset
        = self->tables->mutations.derived_state_offset;
    const tsk_id_t *restrict I = self->tables->indexes.edge_insertion_order;
    const tsk_id_t *restrict O = self->tables->indexes.edge_removal_order;
    const double *restrict edge_right = self->tables->edges.right;
    const double *restrict edge_left = self->tables->edges.left;
    const tsk_id_t *restrict edge_child = self->tables->edges.child;
    tsk_size_t num_trees_alloc = self->num_trees + 1;
    bool discrete_breakpoints = true;
    tsk_id_t *node_edge_map = tsk_malloc(num_nodes * sizeof(*node_edge_map));
    tsk_mutation_t *mutation;
    tsk_id_t parent_id;

    self->tree_sites_length
        = tsk_malloc(num_trees_alloc * sizeof(*self->tree_sites_length));
    self->tree_sites = tsk_malloc(num_trees_alloc * sizeof(*self->tree_sites));
    self->breakpoints = tsk_malloc(num_trees_alloc * sizeof(*self->breakpoints));
    if (node_edge_map == NULL || self->tree_sites == NULL
        || self->tree_sites_length == NULL || self->breakpoints == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memset(
        self->tree_sites_length, 0, self->num_trees * sizeof(*self->tree_sites_length));
    tsk_memset(self->tree_sites, 0, self->num_trees * sizeof(*self->tree_sites));
    tsk_memset(node_edge_map, TSK_NULL, num_nodes * sizeof(*node_edge_map));

    tree_left = 0;
    tree_right = sequence_length;
    tree_index = 0;
    site_id = 0;
    mutation_id = 0;
    j = 0;
    k = 0;
    while (j < num_edges || tree_left < sequence_length) {
        discrete_breakpoints = discrete_breakpoints && is_discrete(tree_left);
        self->breakpoints[tree_index] = tree_left;
        while (k < num_edges && edge_right[O[k]] == tree_left) {
            edge_id = O[k];
            node_edge_map[edge_child[edge_id]] = TSK_NULL;
            k++;
        }
        while (j < num_edges && edge_left[I[j]] == tree_left) {
            edge_id = I[j];
            node_edge_map[edge_child[edge_id]] = edge_id;
            j++;
        }
        tree_right = sequence_length;
        if (j < num_edges) {
            tree_right = TSK_MIN(tree_right, edge_left[I[j]]);
        }
        if (k < num_edges) {
            tree_right = TSK_MIN(tree_right, edge_right[O[k]]);
        }
        self->tree_sites[tree_index] = self->tree_sites_mem + site_id;
        while (site_id < num_sites && site_position[site_id] < tree_right) {
            self->tree_sites_length[tree_index]++;
            while (
                mutation_id < num_mutations && mutation_site[mutation_id] == site_id) {
                mutation = self->site_mutations_mem + mutation_id;
                mutation->edge = node_edge_map[mutation->node];

                /* Compute inherited state */
                if (mutation_parent[mutation_id] == TSK_NULL) {
                    /* No parent: inherited state is the site's ancestral state */
                    mutation->inherited_state
                        = sites_ancestral_state + sites_ancestral_state_offset[site_id];
                    mutation->inherited_state_length
                        = sites_ancestral_state_offset[site_id + 1]
                          - sites_ancestral_state_offset[site_id];
                } else {
                    /* Has parent: inherited state is parent's derived state */
                    parent_id = mutation_parent[mutation_id];
                    mutation->inherited_state
                        = mutations_derived_state
                          + mutations_derived_state_offset[parent_id];
                    mutation->inherited_state_length
                        = mutations_derived_state_offset[parent_id + 1]
                          - mutations_derived_state_offset[parent_id];
                }

                mutation_id++;
            }
            site_id++;
        }
        tree_left = tree_right;
        tree_index++;
    }
    tsk_bug_assert(site_id == num_sites);
    tsk_bug_assert(tree_index == self->num_trees);
    self->breakpoints[tree_index] = tree_right;
    discrete_breakpoints = discrete_breakpoints && is_discrete(tree_right);
    self->discrete_genome = self->discrete_genome && discrete_breakpoints;
    ret = 0;
out:
    tsk_safe_free(node_edge_map);
    return ret;
}

static void
tsk_treeseq_init_migrations(tsk_treeseq_t *self)
{
    tsk_size_t j;
    tsk_size_t num_migrations = self->tables->migrations.num_rows;
    const double *restrict left = self->tables->migrations.left;
    const double *restrict right = self->tables->migrations.right;
    const double *restrict time = self->tables->migrations.time;
    bool discrete_breakpoints = true;
    bool discrete_times = true;

    for (j = 0; j < num_migrations; j++) {
        discrete_breakpoints
            = discrete_breakpoints && is_discrete(left[j]) && is_discrete(right[j]);
        discrete_times
            = discrete_times && (is_discrete(time[j]) || tsk_is_unknown_time(time[j]));
    }
    self->discrete_genome = self->discrete_genome && discrete_breakpoints;
    self->discrete_time = self->discrete_time && discrete_times;
}

static void
tsk_treeseq_init_mutations(tsk_treeseq_t *self)
{
    tsk_size_t j;
    tsk_size_t num_mutations = self->tables->mutations.num_rows;
    const double *restrict time = self->tables->mutations.time;
    bool discrete_times = true;

    for (j = 0; j < num_mutations; j++) {
        discrete_times
            = discrete_times && (is_discrete(time[j]) || tsk_is_unknown_time(time[j]));
    }
    self->discrete_time = self->discrete_time && discrete_times;

    for (j = 0; j < num_mutations; j++) {
        if (!tsk_is_unknown_time(time[j])) {
            self->min_time = TSK_MIN(self->min_time, time[j]);
            self->max_time = TSK_MAX(self->max_time, time[j]);
        }
    }
}

static int
tsk_treeseq_init_nodes(tsk_treeseq_t *self)
{
    tsk_size_t j, k;
    tsk_size_t num_nodes = self->tables->nodes.num_rows;
    const tsk_flags_t *restrict node_flags = self->tables->nodes.flags;
    const double *restrict time = self->tables->nodes.time;
    int ret = 0;
    bool discrete_times = true;

    /* Determine the sample size */
    self->num_samples = 0;
    for (j = 0; j < num_nodes; j++) {
        if (!!(node_flags[j] & TSK_NODE_IS_SAMPLE)) {
            self->num_samples++;
        }
    }
    /* TODO raise an error if < 2 samples?? */
    self->samples = tsk_malloc(self->num_samples * sizeof(tsk_id_t));
    self->sample_index_map = tsk_malloc(num_nodes * sizeof(tsk_id_t));
    if (self->samples == NULL || self->sample_index_map == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
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

    for (j = 0; j < num_nodes; j++) {
        discrete_times
            = discrete_times && (is_discrete(time[j]) || tsk_is_unknown_time(time[j]));
    }
    self->discrete_time = self->discrete_time && discrete_times;

    for (j = 0; j < num_nodes; j++) {
        if (!tsk_is_unknown_time(time[j])) {
            self->min_time = TSK_MIN(self->min_time, time[j]);
            self->max_time = TSK_MAX(self->max_time, time[j]);
        }
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_treeseq_init(
    tsk_treeseq_t *self, tsk_table_collection_t *tables, tsk_flags_t options)
{
    int ret = 0;
    tsk_id_t num_trees;

    tsk_memset(self, 0, sizeof(*self));
    if (options & TSK_TAKE_OWNERSHIP) {
        self->tables = tables;
        if (tables->edges.options & TSK_TABLE_NO_METADATA) {
            ret = tsk_trace_error(TSK_ERR_CANT_TAKE_OWNERSHIP_NO_EDGE_METADATA);
            goto out;
        }
    } else {
        self->tables = tsk_malloc(sizeof(*self->tables));
        if (self->tables == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }

        /* Note that this copy reinstates metadata for a table collection with
         * TSK_TC_NO_EDGE_METADATA. Otherwise a table without metadata would
         * crash tsk_diff_iter_next. */
        ret = tsk_table_collection_copy(tables, self->tables, TSK_COPY_FILE_UUID);
        if (ret != 0) {
            goto out;
        }
    }
    if (options & TSK_TS_INIT_BUILD_INDEXES) {
        ret = tsk_table_collection_build_index(self->tables, 0);
        if (ret != 0) {
            goto out;
        }
    }

    if (options & TSK_TS_INIT_COMPUTE_MUTATION_PARENTS) {
        /* As tsk_table_collection_compute_mutation_parents performs an
           integrity check, and we don't wish to do that twice we perform
           our own check here */
        num_trees = tsk_table_collection_check_integrity(self->tables, TSK_CHECK_TREES);
        if (num_trees < 0) {
            ret = (int) num_trees;
            goto out;
        }

        ret = tsk_table_collection_compute_mutation_parents(
            self->tables, TSK_NO_CHECK_INTEGRITY);
        if (ret != 0) {
            goto out;
        }
    } else {
        num_trees = tsk_table_collection_check_integrity(
            self->tables, TSK_CHECK_TREES | TSK_CHECK_MUTATION_PARENTS);
        if (num_trees < 0) {
            ret = (int) num_trees;
            goto out;
        }
    }
    self->num_trees = (tsk_size_t) num_trees;
    self->discrete_genome = true;
    self->discrete_time = true;
    self->min_time = INFINITY;
    self->max_time = -INFINITY;
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
    tsk_treeseq_init_migrations(self);
    tsk_treeseq_init_mutations(self);

    if (tsk_treeseq_get_time_units_length(self) == strlen(TSK_TIME_UNITS_UNCALIBRATED)
        && !strncmp(tsk_treeseq_get_time_units(self), TSK_TIME_UNITS_UNCALIBRATED,
               strlen(TSK_TIME_UNITS_UNCALIBRATED))) {
        self->time_uncalibrated = true;
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
tsk_treeseq_load(tsk_treeseq_t *self, const char *filename, tsk_flags_t options)
{
    int ret = 0;
    tsk_table_collection_t *tables = malloc(sizeof(*tables));

    /* Need to make sure that we're zero'd out in case of error */
    tsk_memset(self, 0, sizeof(*self));

    if (tables == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    ret = tsk_table_collection_load(tables, filename, options);
    if (ret != 0) {
        tsk_table_collection_free(tables);
        tsk_safe_free(tables);
        goto out;
    }
    /* TSK_TAKE_OWNERSHIP takes immediate ownership of the tables, regardless
     * of error conditions. */
    ret = tsk_treeseq_init(self, tables, TSK_TAKE_OWNERSHIP);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_treeseq_loadf(tsk_treeseq_t *self, FILE *file, tsk_flags_t options)
{
    int ret = 0;
    tsk_table_collection_t *tables = malloc(sizeof(*tables));

    /* Need to make sure that we're zero'd out in case of error */
    tsk_memset(self, 0, sizeof(*self));

    if (tables == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    ret = tsk_table_collection_loadf(tables, file, options);
    if (ret != 0) {
        tsk_table_collection_free(tables);
        tsk_safe_free(tables);
        goto out;
    }
    /* TSK_TAKE_OWNERSHIP takes immediate ownership of the tables, regardless
     * of error conditions. */
    ret = tsk_treeseq_init(self, tables, TSK_TAKE_OWNERSHIP);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_treeseq_dump(const tsk_treeseq_t *self, const char *filename, tsk_flags_t options)
{
    return tsk_table_collection_dump(self->tables, filename, options);
}

int TSK_WARN_UNUSED
tsk_treeseq_dumpf(const tsk_treeseq_t *self, FILE *file, tsk_flags_t options)
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

const char *
tsk_treeseq_get_time_units(const tsk_treeseq_t *self)
{
    return self->tables->time_units;
}

tsk_size_t
tsk_treeseq_get_time_units_length(const tsk_treeseq_t *self)
{
    return self->tables->time_units_length;
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

bool
tsk_treeseq_get_discrete_genome(const tsk_treeseq_t *self)
{
    return self->discrete_genome;
}

bool
tsk_treeseq_get_discrete_time(const tsk_treeseq_t *self)
{
    return self->discrete_time;
}

double
tsk_treeseq_get_min_time(const tsk_treeseq_t *self)
{
    return self->min_time;
}

double
tsk_treeseq_get_max_time(const tsk_treeseq_t *self)
{
    return self->max_time;
}

bool
tsk_treeseq_has_reference_sequence(const tsk_treeseq_t *self)
{
    return tsk_table_collection_has_reference_sequence(self->tables);
}

int
tsk_treeseq_get_individuals_population(const tsk_treeseq_t *self, tsk_id_t *output)
{
    int ret = 0;
    tsk_size_t i, j;
    tsk_individual_t ind;
    tsk_id_t ind_pop;
    const tsk_id_t *node_population = self->tables->nodes.population;
    const tsk_size_t num_individuals = self->tables->individuals.num_rows;

    tsk_memset(output, TSK_NULL, num_individuals * sizeof(*output));

    for (i = 0; i < num_individuals; i++) {
        ret = tsk_treeseq_get_individual(self, (tsk_id_t) i, &ind);
        tsk_bug_assert(ret == 0);
        if (ind.nodes_length > 0) {
            ind_pop = -2;
            for (j = 0; j < ind.nodes_length; j++) {
                if (ind_pop == -2) {
                    ind_pop = node_population[ind.nodes[j]];
                } else if (ind_pop != node_population[ind.nodes[j]]) {
                    ret = tsk_trace_error(TSK_ERR_INDIVIDUAL_POPULATION_MISMATCH);
                    goto out;
                }
            }
            output[ind.id] = ind_pop;
        }
    }
out:
    return ret;
}

int
tsk_treeseq_get_individuals_time(const tsk_treeseq_t *self, double *output)
{
    int ret = 0;
    tsk_size_t i, j;
    tsk_individual_t ind;
    double ind_time;
    const double *node_time = self->tables->nodes.time;
    const tsk_size_t num_individuals = self->tables->individuals.num_rows;

    for (i = 0; i < num_individuals; i++) {
        ret = tsk_treeseq_get_individual(self, (tsk_id_t) i, &ind);
        tsk_bug_assert(ret == 0);
        /* the default is UNKNOWN_TIME, but nodes cannot have
         * UNKNOWN _TIME so this is safe. */
        ind_time = TSK_UNKNOWN_TIME;
        for (j = 0; j < ind.nodes_length; j++) {
            if (j == 0) {
                ind_time = node_time[ind.nodes[j]];
            } else if (ind_time != node_time[ind.nodes[j]]) {
                ret = tsk_trace_error(TSK_ERR_INDIVIDUAL_TIME_MISMATCH);
                goto out;
            }
        }
        output[ind.id] = ind_time;
    }
out:
    return ret;
}

/* Stats functions */

#define GET_2D_ROW(array, row_len, row) (array + (((size_t)(row_len)) * (size_t)(row)))

static inline double *
GET_3D_ROW(double *base, tsk_size_t num_nodes, tsk_size_t output_dim,
    tsk_size_t window_index, tsk_id_t u)
{
    tsk_size_t offset
        = window_index * num_nodes * output_dim + ((tsk_size_t) u) * output_dim;
    return base + offset;
}

/* Increments the n-dimensional array with the specified shape by the specified value at
 * the specified coordinate. */
static inline void
increment_nd_array_value(double *array, tsk_size_t n, const tsk_size_t *shape,
    const tsk_size_t *coordinate, double value)
{
    tsk_size_t offset = 0;
    tsk_size_t product = 1;
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
    const tsk_id_t *focal, tsk_size_t num_focal, const tsk_id_t *const *reference_sets,
    const tsk_size_t *reference_set_size, tsk_size_t num_reference_sets,
    tsk_flags_t TSK_UNUSED(options), double *ret_array)
{
    int ret = 0;
    tsk_id_t u, v, p;
    tsk_size_t j;
    /* TODO It's probably not worth bothering with the int16_t here. */
    int16_t k, focal_reference_set;
    /* We use the K'th element of the array for the total. */
    const int16_t K = (int16_t)(num_reference_sets + 1);
    tsk_size_t num_nodes = self->tables->nodes.num_rows;
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
    tsk_id_t *restrict parent = tsk_malloc(num_nodes * sizeof(*parent));
    double *restrict length = tsk_calloc(num_focal, sizeof(*length));
    uint32_t *restrict ref_count
        = tsk_calloc(((tsk_size_t) K) * num_nodes, sizeof(*ref_count));
    int16_t *restrict reference_set_map
        = tsk_malloc(num_nodes * sizeof(*reference_set_map));
    uint32_t *restrict row = NULL;
    uint32_t *restrict child_row = NULL;
    uint32_t total, delta;

    /* We support a max of 8K focal sets */
    if (num_reference_sets == 0 || num_reference_sets > (INT16_MAX - 1)) {
        /* TODO: more specific error */
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    if (parent == NULL || ref_count == NULL || reference_set_map == NULL
        || length == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    tsk_memset(parent, 0xff, num_nodes * sizeof(*parent));
    tsk_memset(reference_set_map, 0xff, num_nodes * sizeof(*reference_set_map));
    tsk_memset(ret_array, 0, num_focal * num_reference_sets * sizeof(*ret_array));

    total = 0; /* keep the compiler happy */

    /* Set the initial conditions and check the input. */
    for (k = 0; k < (int16_t) num_reference_sets; k++) {
        for (j = 0; j < reference_set_size[k]; j++) {
            u = reference_sets[k][j];
            if (u < 0 || u >= (tsk_id_t) num_nodes) {
                ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
                goto out;
            }
            if (reference_set_map[u] != TSK_NULL) {
                /* FIXME Technically inaccurate here: duplicate focal not sample */
                ret = tsk_trace_error(TSK_ERR_DUPLICATE_SAMPLE);
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
            ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
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
    const tsk_id_t *const *reference_sets, const tsk_size_t *reference_set_size,
    tsk_size_t num_reference_sets, tsk_flags_t TSK_UNUSED(options), double *ret_array)
{
    int ret = 0;
    tsk_id_t u, v;
    tsk_size_t j;
    int32_t k;
    /* We use the K'th element of the array for the total. */
    const int32_t K = (int32_t)(num_reference_sets + 1);
    tsk_size_t num_nodes = self->tables->nodes.num_rows;
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
    tsk_id_t *restrict parent = tsk_malloc(num_nodes * sizeof(*parent));
    uint32_t *restrict ref_count
        = tsk_calloc(num_nodes * ((size_t) K), sizeof(*ref_count));
    double *restrict last_update = tsk_calloc(num_nodes, sizeof(*last_update));
    double *restrict total_length = tsk_calloc(num_nodes, sizeof(*total_length));
    uint32_t *restrict row, *restrict child_row;

    if (num_reference_sets == 0 || num_reference_sets > (INT32_MAX - 1)) {
        /* TODO: more specific error */
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    if (parent == NULL || ref_count == NULL || last_update == NULL
        || total_length == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    /* TODO add check for duplicate values in the reference sets */

    tsk_memset(parent, 0xff, num_nodes * sizeof(*parent));
    tsk_memset(ret_array, 0, num_nodes * num_reference_sets * sizeof(*ret_array));

    /* Set the initial conditions and check the input. */
    for (k = 0; k < (int32_t) num_reference_sets; k++) {
        for (j = 0; j < reference_set_size[k]; j++) {
            u = reference_sets[k][j];
            if (u < 0 || u >= (tsk_id_t) num_nodes) {
                ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
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

#define TSK_REQUIRE_FULL_SPAN 1

static int
tsk_treeseq_check_windows(const tsk_treeseq_t *self, tsk_size_t num_windows,
    const double *windows, tsk_flags_t options)
{
    int ret = 0;
    tsk_size_t j;

    if (num_windows < 1) {
        ret = tsk_trace_error(TSK_ERR_BAD_NUM_WINDOWS);
        goto out;
    }
    if (options & TSK_REQUIRE_FULL_SPAN) {
        /* TODO the general stat code currently requires that we include the
         * entire tree sequence span. This should be relaxed, so hopefully
         * this branch (and the option) can be removed at some point */
        if (windows[0] != 0) {
            ret = tsk_trace_error(TSK_ERR_BAD_WINDOWS);
            goto out;
        }
        if (windows[num_windows] != self->tables->sequence_length) {
            ret = tsk_trace_error(TSK_ERR_BAD_WINDOWS);
            goto out;
        }
    } else {
        if (windows[0] < 0) {
            ret = tsk_trace_error(TSK_ERR_BAD_WINDOWS);
            goto out;
        }
        if (windows[num_windows] > self->tables->sequence_length) {
            ret = tsk_trace_error(TSK_ERR_BAD_WINDOWS);
            goto out;
        }
    }
    for (j = 0; j < num_windows; j++) {
        if (windows[j] >= windows[j + 1]) {
            ret = tsk_trace_error(TSK_ERR_BAD_WINDOWS);
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}

static int
tsk_treeseq_check_time_windows(tsk_size_t num_windows, const double *windows)
{
    // This does not check the last window ends at infinity,
    // which is required for some time window functions.
    int ret = TSK_ERR_BAD_TIME_WINDOWS;
    tsk_size_t j;

    if (num_windows < 1) {
        ret = TSK_ERR_BAD_TIME_WINDOWS_DIM;
        goto out;
    }

    if (windows[0] != 0.0) {
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
update_state(double *X, tsk_size_t state_dim, tsk_id_t dest, tsk_id_t source, int sign)
{
    tsk_size_t k;
    double *X_dest = GET_2D_ROW(X, state_dim, dest);
    double *X_source = GET_2D_ROW(X, state_dim, source);

    for (k = 0; k < state_dim; k++) {
        X_dest[k] += sign * X_source[k];
    }
}

static inline int
update_node_summary(tsk_id_t u, tsk_size_t result_dim, double *node_summary, double *X,
    tsk_size_t state_dim, general_stat_func_t *f, void *f_params)
{
    double *X_u = GET_2D_ROW(X, state_dim, u);
    double *summary_u = GET_2D_ROW(node_summary, result_dim, u);

    return f(state_dim, X_u, result_dim, summary_u, f_params);
}

static inline void
update_running_sum(tsk_id_t u, double sign, const double *restrict branch_length,
    const double *summary, tsk_size_t result_dim, double *running_sum)
{
    const double *summary_u = GET_2D_ROW(summary, result_dim, u);
    const double x = sign * branch_length[u];
    tsk_size_t m;

    for (m = 0; m < result_dim; m++) {
        running_sum[m] += x * summary_u[m];
    }
}

static int
tsk_treeseq_branch_general_stat(const tsk_treeseq_t *self, tsk_size_t state_dim,
    const double *sample_weights, tsk_size_t result_dim, general_stat_func_t *f,
    void *f_params, tsk_size_t num_windows, const double *windows, tsk_flags_t options,
    double *result)
{
    int ret = 0;
    tsk_id_t u, v;
    tsk_size_t j, k, window_index;
    tsk_size_t num_nodes = self->tables->nodes.num_rows;
    const tsk_id_t num_edges = (tsk_id_t) self->tables->edges.num_rows;
    const tsk_id_t *restrict I = self->tables->indexes.edge_insertion_order;
    const tsk_id_t *restrict O = self->tables->indexes.edge_removal_order;
    const double *restrict edge_left = self->tables->edges.left;
    const double *restrict edge_right = self->tables->edges.right;
    const tsk_id_t *restrict edge_parent = self->tables->edges.parent;
    const tsk_id_t *restrict edge_child = self->tables->edges.child;
    const double *restrict time = self->tables->nodes.time;
    const double sequence_length = self->tables->sequence_length;
    tsk_id_t *restrict parent = tsk_malloc(num_nodes * sizeof(*parent));
    double *restrict branch_length = tsk_calloc(num_nodes, sizeof(*branch_length));
    tsk_id_t tj, tk, h;
    double t_left, t_right, w_left, w_right, left, right, scale;
    const double *weight_u;
    double *state_u, *result_row, *summary_u;
    double *state = tsk_calloc(num_nodes * state_dim, sizeof(*state));
    double *summary = tsk_calloc(num_nodes * result_dim, sizeof(*summary));
    double *running_sum = tsk_calloc(result_dim, sizeof(*running_sum));
    double *zero_state = tsk_calloc(state_dim, sizeof(*zero_state));
    double *zero_summary = tsk_calloc(result_dim, sizeof(*zero_state));

    if (self->time_uncalibrated && !(options & TSK_STAT_ALLOW_TIME_UNCALIBRATED)) {
        ret = tsk_trace_error(TSK_ERR_TIME_UNCALIBRATED);
        goto out;
    }

    if (parent == NULL || branch_length == NULL || state == NULL || running_sum == NULL
        || summary == NULL || zero_state == NULL || zero_summary == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memset(parent, 0xff, num_nodes * sizeof(*parent));

    /* If f is not strict, we may need to set conditions for non-sample nodes as well. */
    ret = f(state_dim, zero_state, result_dim, zero_summary, f_params);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < num_nodes; j++) { // we could skip this if zero_summary is zero
        summary_u = GET_2D_ROW(summary, result_dim, j);
        tsk_memcpy(summary_u, zero_summary, result_dim * sizeof(*zero_summary));
    }
    /* Set the initial conditions */
    for (j = 0; j < self->num_samples; j++) {
        u = self->samples[j];
        state_u = GET_2D_ROW(state, state_dim, u);
        weight_u = GET_2D_ROW(sample_weights, state_dim, j);
        tsk_memcpy(state_u, weight_u, state_dim * sizeof(*state_u));
        summary_u = GET_2D_ROW(summary, result_dim, u);
        ret = f(state_dim, state_u, result_dim, summary_u, f_params);
        if (ret != 0) {
            goto out;
        }
    }

    tsk_memset(result, 0, num_windows * result_dim * sizeof(*result));

    /* Iterate over the trees */
    tj = 0;
    tk = 0;
    t_left = 0;
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
    tsk_safe_free(zero_state);
    tsk_safe_free(zero_summary);
    return ret;
}

static int
get_allele_weights(const tsk_site_t *site, const double *state, tsk_size_t state_dim,
    const double *total_weight, tsk_size_t *ret_num_alleles, double **ret_allele_states)
{
    int ret = 0;
    tsk_size_t k;
    tsk_mutation_t mutation, parent_mut;
    tsk_size_t mutation_index, allele, num_alleles, alt_allele_length;
    /* The allele table */
    tsk_size_t max_alleles = site->mutations_length + 1;
    const char **alleles = tsk_malloc(max_alleles * sizeof(*alleles));
    tsk_size_t *allele_lengths = tsk_calloc(max_alleles, sizeof(*allele_lengths));
    double *allele_states = tsk_calloc(max_alleles * state_dim, sizeof(*allele_states));
    double *allele_row;
    const double *state_row;
    const char *alt_allele;

    if (alleles == NULL || allele_lengths == NULL || allele_states == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    tsk_bug_assert(state != NULL);
    alleles[0] = site->ancestral_state;
    allele_lengths[0] = site->ancestral_state_length;
    tsk_memcpy(allele_states, total_weight, state_dim * sizeof(*allele_states));
    num_alleles = 1;

    for (mutation_index = 0; mutation_index < site->mutations_length; mutation_index++) {
        mutation = site->mutations[mutation_index];
        /* Compute the allele index for this derived state value. */
        allele = 0;
        while (allele < num_alleles) {
            if (mutation.derived_state_length == allele_lengths[allele]
                && tsk_memcmp(
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

        /* Get the index for the alternate allele that we must subtract from */
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
                && tsk_memcmp(alt_allele, alleles[allele], allele_lengths[allele])
                       == 0) {
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
compute_general_stat_site_result(tsk_site_t *site, double *state, tsk_size_t state_dim,
    tsk_size_t result_dim, general_stat_func_t *f, void *f_params, double *total_weight,
    bool polarised, double *result)
{
    int ret = 0;
    tsk_size_t k;
    tsk_size_t allele, num_alleles;
    double *allele_states;
    double *result_tmp = tsk_calloc(result_dim, sizeof(*result_tmp));

    if (result_tmp == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memset(result, 0, result_dim * sizeof(*result));

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
tsk_treeseq_site_general_stat(const tsk_treeseq_t *self, tsk_size_t state_dim,
    const double *sample_weights, tsk_size_t result_dim, general_stat_func_t *f,
    void *f_params, tsk_size_t num_windows, const double *windows, tsk_flags_t options,
    double *result)
{
    int ret = 0;
    tsk_id_t u, v;
    tsk_size_t j, k, tree_site, tree_index, window_index;
    tsk_size_t num_nodes = self->tables->nodes.num_rows;
    const tsk_id_t num_edges = (tsk_id_t) self->tables->edges.num_rows;
    const tsk_id_t *restrict I = self->tables->indexes.edge_insertion_order;
    const tsk_id_t *restrict O = self->tables->indexes.edge_removal_order;
    const double *restrict edge_left = self->tables->edges.left;
    const double *restrict edge_right = self->tables->edges.right;
    const tsk_id_t *restrict edge_parent = self->tables->edges.parent;
    const tsk_id_t *restrict edge_child = self->tables->edges.child;
    const double sequence_length = self->tables->sequence_length;
    tsk_id_t *restrict parent = tsk_malloc(num_nodes * sizeof(*parent));
    tsk_site_t *site;
    tsk_id_t tj, tk, h;
    double t_left, t_right;
    const double *weight_u;
    double *state_u, *result_row;
    double *state = tsk_calloc(num_nodes * state_dim, sizeof(*state));
    double *total_weight = tsk_calloc(state_dim, sizeof(*total_weight));
    double *site_result = tsk_calloc(result_dim, sizeof(*site_result));
    bool polarised = false;

    if (parent == NULL || state == NULL || total_weight == NULL || site_result == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memset(parent, 0xff, num_nodes * sizeof(*parent));

    if (options & TSK_STAT_POLARISED) {
        polarised = true;
    }

    /* Set the initial conditions */
    for (j = 0; j < self->num_samples; j++) {
        u = self->samples[j];
        state_u = GET_2D_ROW(state, state_dim, u);
        weight_u = GET_2D_ROW(sample_weights, state_dim, j);
        tsk_memcpy(state_u, weight_u, state_dim * sizeof(*state_u));
        for (k = 0; k < state_dim; k++) {
            total_weight[k] += weight_u[k];
        }
    }
    tsk_memset(result, 0, num_windows * result_dim * sizeof(*result));

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
increment_row(tsk_size_t length, double multiplier, double *source, double *dest)
{
    tsk_size_t j;

    for (j = 0; j < length; j++) {
        dest[j] += multiplier * source[j];
    }
}

static int
tsk_treeseq_node_general_stat(const tsk_treeseq_t *self, tsk_size_t state_dim,
    const double *sample_weights, tsk_size_t result_dim, general_stat_func_t *f,
    void *f_params, tsk_size_t num_windows, const double *windows,
    tsk_flags_t TSK_UNUSED(options), double *result)
{
    int ret = 0;
    tsk_id_t u, v;
    tsk_size_t j, window_index;
    tsk_size_t num_nodes = self->tables->nodes.num_rows;
    const tsk_id_t num_edges = (tsk_id_t) self->tables->edges.num_rows;
    const tsk_id_t *restrict I = self->tables->indexes.edge_insertion_order;
    const tsk_id_t *restrict O = self->tables->indexes.edge_removal_order;
    const double *restrict edge_left = self->tables->edges.left;
    const double *restrict edge_right = self->tables->edges.right;
    const tsk_id_t *restrict edge_parent = self->tables->edges.parent;
    const tsk_id_t *restrict edge_child = self->tables->edges.child;
    const double sequence_length = self->tables->sequence_length;
    tsk_id_t *restrict parent = tsk_malloc(num_nodes * sizeof(*parent));
    tsk_id_t tj, tk, h;
    const double *weight_u;
    double *state_u;
    double *state = tsk_calloc(num_nodes * state_dim, sizeof(*state));
    double *node_summary = tsk_calloc(num_nodes * result_dim, sizeof(*node_summary));
    double *last_update = tsk_calloc(num_nodes, sizeof(*last_update));
    double t_left, t_right, w_right;

    if (parent == NULL || state == NULL || node_summary == NULL || last_update == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memset(parent, 0xff, num_nodes * sizeof(*parent));
    tsk_memset(result, 0, num_windows * num_nodes * result_dim * sizeof(*result));

    /* Set the initial conditions */
    for (j = 0; j < self->num_samples; j++) {
        u = self->samples[j];
        state_u = GET_2D_ROW(state, state_dim, u);
        weight_u = GET_2D_ROW(sample_weights, state_dim, j);
        tsk_memcpy(state_u, weight_u, state_dim * sizeof(*state_u));
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
span_normalise(
    tsk_size_t num_windows, const double *windows, tsk_size_t row_size, double *array)
{
    tsk_size_t window_index, k;
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
unpolarised_summary_func(tsk_size_t state_dim, const double *state,
    tsk_size_t result_dim, double *result, void *params)
{
    int ret = 0;
    unpolarised_summary_func_args *upargs = (unpolarised_summary_func_args *) params;
    const double *total_weight = upargs->total_weight;
    double *total_minus_state = upargs->total_minus_state;
    double *result_tmp = upargs->result_tmp;
    tsk_size_t k, m;

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
tsk_polarisable_func_general_stat(const tsk_treeseq_t *self, tsk_size_t state_dim,
    const double *sample_weights, tsk_size_t result_dim, general_stat_func_t *f,
    void *f_params, tsk_size_t num_windows, const double *windows, tsk_flags_t options,
    double *result)
{
    int ret = 0;
    bool stat_branch = !!(options & TSK_STAT_BRANCH);
    bool polarised = options & TSK_STAT_POLARISED;
    general_stat_func_t *wrapped_f = f;
    void *wrapped_f_params = f_params;
    const double *weight_u;
    unpolarised_summary_func_args upargs;
    tsk_size_t j, k;

    tsk_memset(&upargs, 0, sizeof(upargs));
    if (!polarised) {
        upargs.f = f;
        upargs.f_params = f_params;
        upargs.total_weight = tsk_calloc(state_dim, sizeof(double));
        upargs.total_minus_state = tsk_calloc(state_dim, sizeof(double));
        upargs.result_tmp = tsk_calloc(result_dim, sizeof(double));

        if (upargs.total_weight == NULL || upargs.total_minus_state == NULL
            || upargs.result_tmp == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
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
            result_dim, wrapped_f, wrapped_f_params, num_windows, windows, options,
            result);
    } else {
        ret = tsk_treeseq_node_general_stat(self, state_dim, sample_weights, result_dim,
            wrapped_f, wrapped_f_params, num_windows, windows, options, result);
    }
out:
    tsk_safe_free(upargs.total_weight);
    tsk_safe_free(upargs.total_minus_state);
    tsk_safe_free(upargs.result_tmp);
    return ret;
}

int
tsk_treeseq_general_stat(const tsk_treeseq_t *self, tsk_size_t state_dim,
    const double *sample_weights, tsk_size_t result_dim, general_stat_func_t *f,
    void *f_params, tsk_size_t num_windows, const double *windows, tsk_flags_t options,
    double *result)
{
    int ret = 0;
    bool stat_site = !!(options & TSK_STAT_SITE);
    bool stat_branch = !!(options & TSK_STAT_BRANCH);
    bool stat_node = !!(options & TSK_STAT_NODE);
    double default_windows[] = { 0, self->tables->sequence_length };
    tsk_size_t row_size;

    /* If no mode is specified, we default to site mode */
    if (!(stat_site || stat_branch || stat_node)) {
        stat_site = true;
    }
    /* It's an error to specify more than one mode */
    if (stat_site + stat_branch + stat_node > 1) {
        ret = tsk_trace_error(TSK_ERR_MULTIPLE_STAT_MODES);
        goto out;
    }

    if (state_dim < 1) {
        ret = tsk_trace_error(TSK_ERR_BAD_STATE_DIMS);
        goto out;
    }
    if (result_dim < 1) {
        ret = tsk_trace_error(TSK_ERR_BAD_RESULT_DIMS);
        goto out;
    }
    if (windows == NULL) {
        num_windows = 1;
        windows = default_windows;
    } else {
        ret = tsk_treeseq_check_windows(
            self, num_windows, windows, TSK_REQUIRE_FULL_SPAN);
        if (ret != 0) {
            goto out;
        }
    }

    if (stat_site) {
        ret = tsk_treeseq_site_general_stat(self, state_dim, sample_weights, result_dim,
            f, f_params, num_windows, windows, options, result);
    } else {
        ret = tsk_polarisable_func_general_stat(self, state_dim, sample_weights,
            result_dim, f, f_params, num_windows, windows, options, result);
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
            ret = tsk_trace_error(TSK_ERR_BAD_SAMPLE_SET_INDEX);
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
    tsk_size_t j, k, l;
    const tsk_id_t num_nodes = (tsk_id_t) self->tables->nodes.num_rows;
    tsk_id_t u, sample_index;

    if (num_sample_sets == 0) {
        ret = tsk_trace_error(TSK_ERR_INSUFFICIENT_SAMPLE_SETS);
        goto out;
    }
    j = 0;
    for (k = 0; k < num_sample_sets; k++) {
        if (sample_set_sizes[k] == 0) {
            ret = tsk_trace_error(TSK_ERR_EMPTY_SAMPLE_SET);
            goto out;
        }
        for (l = 0; l < sample_set_sizes[k]; l++) {
            u = sample_sets[j];
            if (u < 0 || u >= num_nodes) {
                ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
                goto out;
            }
            sample_index = self->sample_index_map[u];
            if (sample_index == TSK_NULL) {
                ret = tsk_trace_error(TSK_ERR_BAD_SAMPLES);
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

typedef struct {
    tsk_size_t num_samples;
    double *total_weights;
    const tsk_id_t *index_tuples;
} indexed_weight_stat_params_t;

static int
tsk_treeseq_sample_count_stat(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t result_dim, const tsk_id_t *set_indexes, general_stat_func_t *f,
    tsk_size_t num_windows, const double *windows, tsk_flags_t options, double *result)
{
    int ret = 0;
    const tsk_size_t num_samples = self->num_samples;
    tsk_size_t j, k, l;
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
    weights = tsk_calloc(num_samples * num_sample_sets, sizeof(*weights));
    if (weights == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    j = 0;
    for (k = 0; k < num_sample_sets; k++) {
        for (l = 0; l < sample_set_sizes[k]; l++) {
            u = sample_sets[j];
            sample_index = self->sample_index_map[u];
            weight_row = GET_2D_ROW(weights, num_sample_sets, sample_index);
            if (weight_row[k] != 0) {
                ret = tsk_trace_error(TSK_ERR_DUPLICATE_SAMPLE);
                goto out;
            }
            weight_row[k] = 1;
            j++;
        }
    }
    ret = tsk_treeseq_general_stat(self, num_sample_sets, weights, result_dim, f, &args,
        num_windows, windows, options, result);
out:
    tsk_safe_free(weights);
    return ret;
}

/***********************************
 * Two Locus Statistics
 ***********************************/

static int
get_allele_samples(const tsk_site_t *site, tsk_size_t site_offset,
    const tsk_bitset_t *state, tsk_bitset_t *out_allele_samples,
    tsk_size_t *out_num_alleles)
{
    int ret = 0;
    tsk_mutation_t mutation, parent_mut;
    tsk_size_t mutation_index, allele, alt_allele, alt_allele_length;
    /* The allele table */
    tsk_size_t max_alleles = site->mutations_length + 1;
    const char **alleles = tsk_malloc(max_alleles * sizeof(*alleles));
    tsk_size_t *allele_lengths = tsk_calloc(max_alleles, sizeof(*allele_lengths));
    const char *alt_allele_state;
    tsk_size_t num_alleles = 1;

    if (alleles == NULL || allele_lengths == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    tsk_bug_assert(state != NULL);
    alleles[0] = site->ancestral_state;
    allele_lengths[0] = site->ancestral_state_length;

    for (mutation_index = 0; mutation_index < site->mutations_length; mutation_index++) {
        mutation = site->mutations[mutation_index];
        /* Compute the allele index for this derived state value. */
        for (allele = 0; allele < num_alleles; allele++) {
            if (mutation.derived_state_length == allele_lengths[allele]
                && tsk_memcmp(
                       mutation.derived_state, alleles[allele], allele_lengths[allele])
                       == 0) {
                break;
            }
        }
        if (allele == num_alleles) {
            tsk_bug_assert(allele < max_alleles);
            alleles[allele] = mutation.derived_state;
            allele_lengths[allele] = mutation.derived_state_length;
            num_alleles++;
        }

        /* Add the mutation's samples to this allele */
        tsk_bitset_union(
            out_allele_samples, allele + site_offset, state, mutation_index);

        /* Get the index for the alternate allele that we must subtract from */
        alt_allele_state = site->ancestral_state;
        alt_allele_length = site->ancestral_state_length;
        if (mutation.parent != TSK_NULL) {
            parent_mut = site->mutations[mutation.parent - site->mutations[0].id];
            alt_allele_state = parent_mut.derived_state;
            alt_allele_length = parent_mut.derived_state_length;
        }
        for (alt_allele = 0; alt_allele < num_alleles; alt_allele++) {
            if (alt_allele_length == allele_lengths[alt_allele]
                && tsk_memcmp(
                       alt_allele_state, alleles[alt_allele], allele_lengths[alt_allele])
                       == 0) {
                break;
            }
        }
        tsk_bug_assert(allele < num_alleles);

        tsk_bitset_subtract(out_allele_samples, alt_allele + site_offset,
            out_allele_samples, allele + site_offset);
    }
    *out_num_alleles = num_alleles;
out:
    tsk_safe_free(alleles);
    tsk_safe_free(allele_lengths);
    return ret;
}

static int
norm_hap_weighted(tsk_size_t result_dim, const double *hap_weights,
    tsk_size_t TSK_UNUSED(n_a), tsk_size_t TSK_UNUSED(n_b), double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *weight_row;
    double n;
    tsk_size_t k;

    for (k = 0; k < result_dim; k++) {
        weight_row = GET_2D_ROW(hap_weights, 3, k);
        n = (double) args.sample_set_sizes[k];
        result[k] = weight_row[0] / n;
    }
    return 0;
}

static int
norm_hap_weighted_ij(tsk_size_t result_dim, const double *hap_weights,
    tsk_size_t TSK_UNUSED(n_a), tsk_size_t TSK_UNUSED(n_b), double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *weight_row;
    double ni, nj, wAB_i, wAB_j;
    tsk_id_t i, j;
    tsk_size_t k;

    for (k = 0; k < result_dim; k++) {
        i = args.set_indexes[2 * k];
        j = args.set_indexes[2 * k + 1];
        ni = (double) args.sample_set_sizes[i];
        nj = (double) args.sample_set_sizes[j];
        weight_row = GET_2D_ROW(hap_weights, 3, i);
        wAB_i = weight_row[0];
        weight_row = GET_2D_ROW(hap_weights, 3, j);
        wAB_j = weight_row[0];

        result[k] = (wAB_i + wAB_j) / (ni + nj);
    }

    return 0;
}

static int
norm_total_weighted(tsk_size_t result_dim, const double *TSK_UNUSED(hap_weights),
    tsk_size_t n_a, tsk_size_t n_b, double *result, void *TSK_UNUSED(params))
{
    tsk_size_t k;
    double norm = 1 / (double) (n_a * n_b);

    for (k = 0; k < result_dim; k++) {
        result[k] = norm;
    }
    return 0;
}

static void
get_all_samples_bits(tsk_bitset_t *all_samples, tsk_size_t n)
{
    tsk_size_t i;
    const tsk_bitset_val_t all = ~((tsk_bitset_val_t) 0);
    const tsk_bitset_val_t remainder_samples = n % TSK_BITSET_BITS;

    all_samples->data[all_samples->row_len - 1]
        = remainder_samples ? ~(all << remainder_samples) : all;
    for (i = 0; i < all_samples->row_len - 1; i++) {
        all_samples->data[i] = all;
    }
}

// Stores the intermediate values for computing two-locus statistics.
typedef struct {
    double *weights;
    double *norm;
    double *result_tmp;
    tsk_bitset_t AB_samples;
} two_locus_work_t;

static int
two_locus_work_init(tsk_size_t max_alleles, tsk_size_t num_samples,
    tsk_size_t result_dim, tsk_size_t state_dim, two_locus_work_t *out)
{
    int ret = 0;

    out->weights = tsk_malloc(3 * state_dim * sizeof(*out->weights));
    out->norm = tsk_malloc(result_dim * sizeof(*out->norm));
    out->result_tmp
        = tsk_malloc(result_dim * max_alleles * max_alleles * sizeof(*out->result_tmp));
    tsk_memset(&out->AB_samples, 0, sizeof(out->AB_samples));
    if (out->weights == NULL || out->norm == NULL || out->result_tmp == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    ret = tsk_bitset_init(&out->AB_samples, num_samples, 1);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

static void
two_locus_work_free(two_locus_work_t *work)
{
    tsk_safe_free(work->weights);
    tsk_safe_free(work->norm);
    tsk_safe_free(work->result_tmp);
    tsk_bitset_free(&work->AB_samples);
}

static int
compute_general_normed_two_site_stat_result(const tsk_bitset_t *state,
    const tsk_size_t *allele_counts, tsk_size_t a_off, tsk_size_t b_off,
    tsk_size_t num_a_alleles, tsk_size_t num_b_alleles, tsk_size_t state_dim,
    tsk_size_t result_dim, general_stat_func_t *f, sample_count_stat_params_t *f_params,
    norm_func_t *norm_f, bool polarised, two_locus_work_t *restrict work, double *result)
{
    int ret = 0;
    // Sample sets and b sites are rows, a sites are columns
    //       b1           b2           b3
    // a1   [s1, s2, s3] [s1, s2, s3] [s1, s2, s3]
    // a2   [s1, s2, s3] [s1, s2, s3] [s1, s2, s3]
    // a3   [s1, s2, s3] [s1, s2, s3] [s1, s2, s3]
    tsk_size_t k, mut_a, mut_b, result_row_len = num_b_alleles * result_dim;
    uint8_t is_polarised = polarised ? 1 : 0;
    double *restrict hap_row, *restrict result_tmp_row;
    double *restrict norm = work->norm;
    double *restrict weights = work->weights;
    double *restrict result_tmp = work->result_tmp;
    tsk_bitset_t AB_samples = work->AB_samples;

    for (mut_a = is_polarised; mut_a < num_a_alleles; mut_a++) {
        result_tmp_row = GET_2D_ROW(result_tmp, result_row_len, mut_a);
        for (mut_b = is_polarised; mut_b < num_b_alleles; mut_b++) {
            for (k = 0; k < state_dim; k++) {
                tsk_bitset_intersect(state, a_off + (mut_a * state_dim) + k, state,
                    b_off + (mut_b * state_dim) + k, &AB_samples);
                hap_row = GET_2D_ROW(weights, 3, k);
                hap_row[0] = (double) tsk_bitset_count(&AB_samples, 0);
                hap_row[1] = (double) allele_counts[a_off + (mut_a * state_dim) + k]
                             - hap_row[0];
                hap_row[2] = (double) allele_counts[b_off + (mut_b * state_dim) + k]
                             - hap_row[0];
            }
            ret = f(state_dim, weights, result_dim, result_tmp_row, f_params);
            if (ret != 0) {
                goto out;
            }
            ret = norm_f(result_dim, weights, num_a_alleles - is_polarised,
                num_b_alleles - is_polarised, norm, f_params);
            if (ret != 0) {
                goto out;
            }
            for (k = 0; k < result_dim; k++) {
                result[k] += result_tmp_row[k] * norm[k];
            }
            result_tmp_row += result_dim; // Advance to the next column
        }
    }
out:
    return ret;
}

static int
compute_general_two_site_stat_result(const tsk_bitset_t *state,
    const tsk_size_t *allele_counts, tsk_size_t a_off, tsk_size_t b_off,
    tsk_size_t state_dim, tsk_size_t result_dim, general_stat_func_t *f,
    sample_count_stat_params_t *f_params, two_locus_work_t *restrict work,
    double *result)
{
    int ret = 0;
    tsk_size_t k;
    tsk_bitset_t AB_samples = work->AB_samples;
    tsk_size_t mut_a = 1, mut_b = 1;
    double *restrict hap_row, *restrict weights = work->weights;

    for (k = 0; k < state_dim; k++) {
        tsk_bitset_intersect(state, a_off + (mut_a * state_dim) + k, state,
            b_off + (mut_b * state_dim) + k, &AB_samples);
        hap_row = GET_2D_ROW(weights, 3, k);
        hap_row[0] = (double) tsk_bitset_count(&AB_samples, 0);
        hap_row[1]
            = (double) allele_counts[a_off + (mut_a * state_dim) + k] - hap_row[0];
        hap_row[2]
            = (double) allele_counts[b_off + (mut_b * state_dim) + k] - hap_row[0];
    }
    ret = f(state_dim, weights, result_dim, result, f_params);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

static void
get_site_row_col_indices(tsk_size_t n_rows, const tsk_id_t *row_sites, tsk_size_t n_cols,
    const tsk_id_t *col_sites, tsk_id_t *sites, tsk_size_t *n_sites, tsk_size_t *row_idx,
    tsk_size_t *col_idx)
{
    tsk_size_t r = 0, c = 0, s = 0;

    // Iterate rows and columns until we've exhaused one of the lists
    while ((r < n_rows) && (c < n_cols)) {
        if (row_sites[r] < col_sites[c]) {
            sites[s] = row_sites[r];
            row_idx[r] = s;
            s++;
            r++;
        } else if (col_sites[c] < row_sites[r]) {
            sites[s] = col_sites[c];
            col_idx[c] = s;
            s++;
            c++;
        } else { // row == col
            sites[s] = row_sites[r];
            col_idx[c] = s;
            row_idx[r] = s;
            s++;
            r++;
            c++;
        }
    }

    // If there are any items remaining in the other list, drain it
    while (r < n_rows) {
        sites[s] = row_sites[r];
        row_idx[r] = s;
        s++;
        r++;
    }
    while (c < n_cols) {
        sites[s] = col_sites[c];
        col_idx[c] = s;
        s++;
        c++;
    }
    *n_sites = s;
}

static int
get_mutation_samples(const tsk_treeseq_t *ts, const tsk_id_t *sites, tsk_size_t n_sites,
    tsk_size_t *num_alleles, tsk_bitset_t *allele_samples)
{
    int ret = 0;
    const tsk_flags_t *restrict flags = ts->tables->nodes.flags;
    const tsk_size_t num_samples = tsk_treeseq_get_num_samples(ts);
    const tsk_size_t *restrict site_muts_len = ts->site_mutations_length;
    tsk_site_t site;
    tsk_tree_t tree;
    tsk_bitset_t all_samples_bits, mut_samples;
    tsk_size_t max_muts_len, site_offset, num_nodes, site_idx, s, m, n;
    tsk_id_t node, *nodes = NULL;
    void *tmp_nodes;

    tsk_memset(&mut_samples, 0, sizeof(mut_samples));
    tsk_memset(&all_samples_bits, 0, sizeof(all_samples_bits));

    max_muts_len = 0;
    for (s = 0; s < n_sites; s++) {
        max_muts_len = TSK_MAX(site_muts_len[sites[s]], max_muts_len);
    }
    // Allocate a bit array of size max alleles for all sites
    ret = tsk_bitset_init(&mut_samples, num_samples, max_muts_len);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_bitset_init(&all_samples_bits, num_samples, 1);
    if (ret != 0) {
        goto out;
    }
    get_all_samples_bits(&all_samples_bits, num_samples);
    ret = tsk_tree_init(&tree, ts, TSK_NO_SAMPLE_COUNTS);
    if (ret != 0) {
        goto out;
    }

    // For each mutation within each site, perform one preorder traversal to gather
    // the samples under each mutation's node.
    site_offset = 0;
    for (site_idx = 0; site_idx < n_sites; site_idx++) {
        tsk_treeseq_get_site(ts, sites[site_idx], &site);
        ret = tsk_tree_seek(&tree, site.position, 0);
        if (ret != 0) {
            goto out;
        }
        tmp_nodes = tsk_realloc(nodes, tsk_tree_get_size_bound(&tree) * sizeof(*nodes));
        if (tmp_nodes == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
        nodes = tmp_nodes;
        tsk_bitset_union(allele_samples, site_offset, &all_samples_bits, 0);
        // Zero out results before the start of each iteration
        tsk_memset(mut_samples.data, 0,
            mut_samples.row_len * max_muts_len * sizeof(tsk_bitset_val_t));
        for (m = 0; m < site.mutations_length; m++) {
            node = site.mutations[m].node;
            ret = tsk_tree_preorder_from(&tree, node, nodes, &num_nodes);
            if (ret != 0) {
                goto out;
            }
            for (n = 0; n < num_nodes; n++) {
                node = nodes[n];
                if (flags[node] & TSK_NODE_IS_SAMPLE) {
                    tsk_bitset_set_bit(
                        &mut_samples, m, (tsk_bitset_val_t) ts->sample_index_map[node]);
                }
            }
        }
        get_allele_samples(
            &site, site_offset, &mut_samples, allele_samples, &(num_alleles[site_idx]));
        site_offset += site.mutations_length + 1;
    }
// if adding code below, check ret before continuing
out:
    tsk_safe_free(nodes);
    tsk_tree_free(&tree);
    tsk_bitset_free(&mut_samples);
    tsk_bitset_free(&all_samples_bits);
    return ret == TSK_TREE_OK ? 0 : ret;
}

// Given the samples under each allele's node and the sample sets, get the samples under
// each allele's node for each sample set. We pack this data into a bitset
// (`allele_sample_sets`) that is size m x n, where m is (n_alleles * num_sample_sets)
// and n is the size of the largest sample set. In addition, we compute the number of
// samples contained in the intersection of each allele's samples and each sample set in
// an array (`allele_sample_sets`) of length (n_alleles * num_sample_sets).
static void
get_mutation_sample_sets(const tsk_bitset_t *allele_samples, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    const tsk_id_t *sample_index_map, tsk_bitset_t *allele_sample_sets,
    tsk_size_t *allele_sample_set_counts)
{
    tsk_bitset_val_t k, sample;
    tsk_size_t i, j, ss_off;

    for (i = 0; i < allele_samples->len; i++) {
        ss_off = 0;
        for (j = 0; j < num_sample_sets; j++) {
            for (k = 0; k < sample_set_sizes[j]; k++) {
                sample = (tsk_bitset_val_t) sample_index_map[sample_sets[k + ss_off]];
                if (tsk_bitset_contains(allele_samples, i, sample)) {
                    tsk_bitset_set_bit(allele_sample_sets, j + i * num_sample_sets, k);
                    allele_sample_set_counts[j + i * num_sample_sets]++;
                }
            }
            ss_off += sample_set_sizes[j];
        }
    }
}

static int
tsk_treeseq_two_site_count_stat(const tsk_treeseq_t *self, tsk_size_t state_dim,
    tsk_size_t num_sample_sets, const tsk_size_t *sample_set_sizes,
    const tsk_id_t *sample_sets, tsk_size_t result_dim, general_stat_func_t *f,
    sample_count_stat_params_t *f_params, norm_func_t *norm_f, tsk_size_t n_rows,
    const tsk_id_t *row_sites, tsk_size_t n_cols, const tsk_id_t *col_sites,
    tsk_flags_t options, double *result)
{
    int ret = 0;
    tsk_bitset_t allele_samples, allele_sample_sets;
    bool polarised = options & TSK_STAT_POLARISED;
    tsk_id_t *sites;
    tsk_size_t i, j, n_sites, *row_idx, *col_idx;
    double *result_row;
    const tsk_size_t num_samples = self->num_samples;
    tsk_size_t *num_alleles = NULL, *site_offsets = NULL, *allele_counts = NULL;
    tsk_size_t result_row_len = n_cols * result_dim;
    tsk_size_t max_ss_size = 0, max_alleles = 0, n_alleles = 0;
    two_locus_work_t work;

    tsk_memset(&work, 0, sizeof(work));
    tsk_memset(&allele_samples, 0, sizeof(allele_samples));
    tsk_memset(&allele_sample_sets, 0, sizeof(allele_sample_sets));
    sites = tsk_malloc(self->tables->sites.num_rows * sizeof(*sites));
    row_idx = tsk_malloc(self->tables->sites.num_rows * sizeof(*row_idx));
    col_idx = tsk_malloc(self->tables->sites.num_rows * sizeof(*col_idx));
    if (sites == NULL || row_idx == NULL || col_idx == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    get_site_row_col_indices(
        n_rows, row_sites, n_cols, col_sites, sites, &n_sites, row_idx, col_idx);
    // depends on n_sites
    num_alleles = tsk_malloc(n_sites * sizeof(*num_alleles));
    site_offsets = tsk_malloc(n_sites * sizeof(*site_offsets));
    if (num_alleles == NULL || site_offsets == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    for (i = 0; i < n_sites; i++) {
        site_offsets[i] = n_alleles * num_sample_sets;
        n_alleles += self->site_mutations_length[sites[i]] + 1;
        max_alleles = TSK_MAX(self->site_mutations_length[sites[i]], max_alleles);
    }
    max_alleles++; // add 1 for the ancestral allele
    // depends on n_alleles
    ret = tsk_bitset_init(&allele_samples, num_samples, n_alleles);
    if (ret != 0) {
        goto out;
    }
    for (i = 0; i < num_sample_sets; i++) {
        max_ss_size = TSK_MAX(sample_set_sizes[i], max_ss_size);
    }
    // depend on n_alleles and max_ss_size
    ret = tsk_bitset_init(&allele_sample_sets, max_ss_size, n_alleles * num_sample_sets);
    if (ret != 0) {
        goto out;
    }
    allele_counts = tsk_calloc(n_alleles * num_sample_sets, sizeof(*allele_counts));
    if (allele_counts == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    // depends on max_ss_size and max_alleles
    ret = two_locus_work_init(max_alleles, max_ss_size, result_dim, state_dim, &work);
    if (ret != 0) {
        goto out;
    }
    // we track the number of alleles to account for backmutations
    ret = get_mutation_samples(self, sites, n_sites, num_alleles, &allele_samples);
    if (ret != 0) {
        goto out;
    }
    get_mutation_sample_sets(&allele_samples, num_sample_sets, sample_set_sizes,
        sample_sets, self->sample_index_map, &allele_sample_sets, allele_counts);
    // For each row/column pair, fill in the sample set in the result matrix.
    for (i = 0; i < n_rows; i++) {
        result_row = GET_2D_ROW(result, result_row_len, i);
        for (j = 0; j < n_cols; j++) {
            if (num_alleles[row_idx[i]] == 2 && num_alleles[col_idx[j]] == 2) {
                // both sites are biallelic
                ret = compute_general_two_site_stat_result(&allele_sample_sets,
                    allele_counts, site_offsets[row_idx[i]], site_offsets[col_idx[j]],
                    state_dim, result_dim, f, f_params, &work,
                    &(result_row[j * result_dim]));
            } else {
                // at least one site is multiallelic
                ret = compute_general_normed_two_site_stat_result(&allele_sample_sets,
                    allele_counts, site_offsets[row_idx[i]], site_offsets[col_idx[j]],
                    num_alleles[row_idx[i]], num_alleles[col_idx[j]], state_dim,
                    result_dim, f, f_params, norm_f, polarised, &work,
                    &(result_row[j * result_dim]));
            }
            if (ret != 0) {
                goto out;
            }
        }
    }

out:
    tsk_safe_free(sites);
    tsk_safe_free(row_idx);
    tsk_safe_free(col_idx);
    tsk_safe_free(num_alleles);
    tsk_safe_free(site_offsets);
    tsk_safe_free(allele_counts);
    two_locus_work_free(&work);
    tsk_bitset_free(&allele_samples);
    tsk_bitset_free(&allele_sample_sets);
    return ret;
}

static int
sample_sets_to_bitset(const tsk_treeseq_t *self, const tsk_size_t *sample_set_sizes,
    const tsk_id_t *sample_sets, tsk_size_t num_sample_sets,
    tsk_bitset_t *sample_sets_bits)
{
    int ret;
    tsk_size_t j, k, l;
    tsk_id_t u, sample_index;

    ret = tsk_bitset_init(sample_sets_bits, self->num_samples, num_sample_sets);
    if (ret != 0) {
        return ret;
    }
    j = 0;
    for (k = 0; k < num_sample_sets; k++) {
        for (l = 0; l < sample_set_sizes[k]; l++) {
            u = sample_sets[j];
            sample_index = self->sample_index_map[u];
            if (tsk_bitset_contains(
                    sample_sets_bits, k, (tsk_bitset_val_t) sample_index)) {
                ret = tsk_trace_error(TSK_ERR_DUPLICATE_SAMPLE);
                goto out;
            }
            tsk_bitset_set_bit(sample_sets_bits, k, (tsk_bitset_val_t) sample_index);
            j++;
        }
    }

out:
    return ret;
}

static int
check_sites(const tsk_id_t *sites, tsk_size_t num_sites, tsk_size_t num_site_rows)
{
    int ret = 0;
    tsk_size_t i;

    if (num_sites == 0) {
        return ret; // No need to verify sites if there aren't any
    }

    for (i = 0; i < num_sites - 1; i++) {
        if (sites[i] < 0 || sites[i] >= (tsk_id_t) num_site_rows) {
            ret = tsk_trace_error(TSK_ERR_SITE_OUT_OF_BOUNDS);
            goto out;
        }
        if (sites[i] > sites[i + 1]) {
            ret = tsk_trace_error(TSK_ERR_STAT_UNSORTED_SITES);
            goto out;
        }
        if (sites[i] == sites[i + 1]) {
            ret = tsk_trace_error(TSK_ERR_STAT_DUPLICATE_SITES);
            goto out;
        }
    }
    // check the last value
    if (sites[i] < 0 || sites[i] >= (tsk_id_t) num_site_rows) {
        ret = tsk_trace_error(TSK_ERR_SITE_OUT_OF_BOUNDS);
        goto out;
    }
out:
    return ret;
}

static int
check_positions(
    const double *positions, tsk_size_t num_positions, double sequence_length)
{
    int ret = 0;
    tsk_size_t i;

    if (num_positions == 0) {
        return ret; // No need to verify positions if there aren't any
    }

    for (i = 0; i < num_positions - 1; i++) {
        if (positions[i] < 0 || positions[i] >= sequence_length) {
            ret = tsk_trace_error(TSK_ERR_POSITION_OUT_OF_BOUNDS);
            goto out;
        }
        if (positions[i] > positions[i + 1]) {
            ret = tsk_trace_error(TSK_ERR_STAT_UNSORTED_POSITIONS);
            goto out;
        }
        if (positions[i] == positions[i + 1]) {
            ret = tsk_trace_error(TSK_ERR_STAT_DUPLICATE_POSITIONS);
            goto out;
        }
    }
    // check bounds of last value
    if (positions[i] < 0 || positions[i] >= sequence_length) {
        ret = tsk_trace_error(TSK_ERR_POSITION_OUT_OF_BOUNDS);
        goto out;
    }
out:
    return ret;
}

static int
positions_to_tree_indexes(const tsk_treeseq_t *ts, const double *positions,
    tsk_size_t num_positions, tsk_id_t **tree_indexes)
{
    int ret = 0;
    tsk_id_t tree_index = 0;
    tsk_size_t i, num_trees = ts->num_trees;

    // This is tricky. If there are 0 positions, we calloc a size of 1
    // we must calloc, because memset will have no effect when called with size 0
    *tree_indexes = tsk_calloc(num_positions, sizeof(*tree_indexes));
    if (tree_indexes == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memset(*tree_indexes, TSK_NULL, num_positions * sizeof(**tree_indexes));
    for (i = 0; i < num_positions; i++) {
        while (ts->breakpoints[tree_index + 1] <= positions[i]) {
            tree_index++;
        }
        (*tree_indexes)[i] = tree_index;
    }
    tsk_bug_assert(tree_index <= (tsk_id_t)(num_trees - 1));

out:
    return ret;
}

static int
get_index_counts(
    const tsk_id_t *indexes, tsk_size_t num_indexes, tsk_size_t **out_counts)
{
    int ret = 0;
    tsk_id_t index = indexes[0];
    tsk_size_t count, i;
    tsk_size_t *counts = tsk_calloc(
        (tsk_size_t)(indexes[num_indexes ? num_indexes - 1 : 0] - indexes[0] + 1),
        sizeof(*counts));
    if (counts == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    count = 1;
    for (i = 1; i < num_indexes; i++) {
        if (indexes[i] == indexes[i - 1]) {
            count++;
        } else {
            counts[index - indexes[0]] = count;
            count = 1;
            index = indexes[i];
        }
    }
    counts[index - indexes[0]] = count;
    *out_counts = counts;
out:
    return ret;
}

typedef struct {
    tsk_tree_t tree;
    tsk_bitset_t *node_samples;
    tsk_id_t *parent;
    tsk_id_t *edges_out;
    tsk_id_t *edges_in;
    double *branch_len;
    tsk_size_t n_edges_out;
    tsk_size_t n_edges_in;
} iter_state;

static int
iter_state_init(iter_state *self, const tsk_treeseq_t *ts, tsk_size_t state_dim)
{
    int ret = 0;
    const tsk_size_t num_nodes = ts->tables->nodes.num_rows;

    ret = tsk_tree_init(&self->tree, ts, TSK_NO_SAMPLE_COUNTS);
    if (ret != 0) {
        goto out;
    }
    self->node_samples = tsk_calloc(1, sizeof(*self->node_samples));
    if (self->node_samples == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    ret = tsk_bitset_init(self->node_samples, ts->num_samples, state_dim * num_nodes);
    if (ret != 0) {
        goto out;
    }
    self->parent = tsk_malloc(num_nodes * sizeof(*self->parent));
    self->edges_out = tsk_malloc(num_nodes * sizeof(*self->edges_out));
    self->edges_in = tsk_malloc(num_nodes * sizeof(*self->edges_in));
    self->branch_len = tsk_calloc(num_nodes, sizeof(*self->branch_len));
    if (self->parent == NULL || self->edges_out == NULL || self->edges_in == NULL
        || self->branch_len == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
out:
    return ret;
}

static int
get_node_samples(const tsk_treeseq_t *ts, tsk_size_t state_dim,
    const tsk_bitset_t *sample_sets, tsk_bitset_t *node_samples)
{
    int ret = 0;
    tsk_size_t n, k;
    tsk_size_t num_nodes = ts->tables->nodes.num_rows;
    tsk_bitset_val_t sample;
    const tsk_id_t *restrict sample_index_map = ts->sample_index_map;
    const tsk_flags_t *restrict flags = ts->tables->nodes.flags;

    ret = tsk_bitset_init(node_samples, ts->num_samples, num_nodes * state_dim);
    if (ret != 0) {
        goto out;
    }
    for (k = 0; k < state_dim; k++) {
        for (n = 0; n < num_nodes; n++) {
            if (flags[n] & TSK_NODE_IS_SAMPLE) {
                sample = (tsk_bitset_val_t) sample_index_map[n];
                if (tsk_bitset_contains(sample_sets, k, sample)) {
                    tsk_bitset_set_bit(node_samples, (state_dim * n) + k, sample);
                }
            }
        }
    }
out:
    return ret;
}

static void
iter_state_clear(iter_state *self, tsk_size_t state_dim, tsk_size_t num_nodes,
    const tsk_bitset_t *node_samples)
{
    self->n_edges_out = 0;
    self->n_edges_in = 0;
    tsk_tree_clear(&self->tree);
    tsk_memset(self->parent, TSK_NULL, num_nodes * sizeof(*self->parent));
    tsk_memset(self->edges_out, TSK_NULL, num_nodes * sizeof(*self->edges_out));
    tsk_memset(self->edges_in, TSK_NULL, num_nodes * sizeof(*self->edges_in));
    tsk_memset(self->branch_len, 0, num_nodes * sizeof(*self->branch_len));
    tsk_memcpy(self->node_samples->data, node_samples->data,
        node_samples->row_len * state_dim * num_nodes * sizeof(*node_samples->data));
}

static void
iter_state_free(iter_state *self)
{
    tsk_tree_free(&self->tree);
    tsk_bitset_free(self->node_samples);
    tsk_safe_free(self->node_samples);
    tsk_safe_free(self->parent);
    tsk_safe_free(self->edges_out);
    tsk_safe_free(self->edges_in);
    tsk_safe_free(self->branch_len);
}

static int
advance_collect_edges(iter_state *s, tsk_id_t index)
{
    int ret = 0;
    tsk_id_t j, e;
    tsk_size_t i;
    double left, right;
    tsk_tree_position_t pos;
    tsk_tree_t *tree = &s->tree;
    const double *restrict edge_left = tree->tree_sequence->tables->edges.left;
    const double *restrict edge_right = tree->tree_sequence->tables->edges.right;

    // Either we're seeking forward one step from some nonzero position in the tree, or
    // from the beginning of the tree sequence.
    if (tree->index != TSK_NULL || index == 0) {
        ret = tsk_tree_next(tree);
        if (ret < 0) {
            goto out;
        }
        pos = tree->tree_pos;
        i = 0;
        for (j = pos.out.start; j != pos.out.stop; j++) {
            s->edges_out[i] = pos.out.order[j];
            i++;
        }
        s->n_edges_out = i;
        i = 0;
        for (j = pos.in.start; j != pos.in.stop; j++) {
            s->edges_in[i] = pos.in.order[j];
            i++;
        }
        s->n_edges_in = i;
    } else {
        // Seek from an arbitrary nonzero position from an uninitialized tree.
        tsk_bug_assert(tree->index == -1);
        ret = tsk_tree_seek_index(tree, index, 0);
        if (ret < 0) {
            goto out;
        }
        pos = tree->tree_pos;
        i = 0;
        if (pos.direction == TSK_DIR_FORWARD) {
            left = pos.interval.left;
            for (j = pos.in.start; j != pos.in.stop; j++) {
                e = pos.in.order[j];
                if (edge_left[e] <= left && left < edge_right[e]) {
                    s->edges_in[i] = pos.in.order[j];
                    i++;
                }
            }
        } else {
            right = pos.interval.right;
            for (j = pos.in.start; j != pos.in.stop; j--) {
                e = pos.in.order[j];
                if (edge_right[e] >= right && right > edge_left[e]) {
                    s->edges_in[i] = pos.in.order[j];
                    i++;
                }
            }
        }
        s->n_edges_out = 0;
        s->n_edges_in = i;
    }
    ret = 0;
out:
    return ret;
}

static int
compute_two_tree_branch_state_update(const tsk_treeseq_t *ts, tsk_id_t c,
    const iter_state *A_state, const iter_state *B_state, tsk_size_t state_dim,
    tsk_size_t result_dim, int sign, general_stat_func_t *f,
    sample_count_stat_params_t *f_params, two_locus_work_t *restrict work,
    double *result)
{
    int ret = 0;
    double a_len, b_len;
    double *restrict B_branch_len = B_state->branch_len;
    double *weights_row;
    tsk_size_t n, k, a_row, b_row;
    const double *restrict A_branch_len = A_state->branch_len;
    const tsk_bitset_t *restrict A_state_samples = A_state->node_samples;
    const tsk_bitset_t *restrict B_state_samples = B_state->node_samples;
    tsk_size_t num_nodes = ts->tables->nodes.num_rows;
    double *weights = work->weights;
    double *result_tmp = work->result_tmp;
    tsk_bitset_t AB_samples = work->AB_samples;

    b_len = B_branch_len[c] * sign;
    if (b_len == 0) {
        return ret;
    }
    for (n = 0; n < num_nodes; n++) {
        a_len = A_branch_len[n];
        if (a_len == 0) {
            continue;
        }
        for (k = 0; k < state_dim; k++) {
            a_row = (state_dim * n) + k;
            b_row = (state_dim * (tsk_size_t) c) + k;
            weights_row = GET_2D_ROW(weights, 3, k);
            tsk_bitset_intersect(
                A_state_samples, a_row, B_state_samples, b_row, &AB_samples);
            weights_row[0] = (double) tsk_bitset_count(&AB_samples, 0);
            weights_row[1]
                = (double) tsk_bitset_count(A_state_samples, a_row) - weights_row[0];
            weights_row[2]
                = (double) tsk_bitset_count(B_state_samples, b_row) - weights_row[0];
        }
        ret = f(state_dim, weights, result_dim, result_tmp, f_params);
        if (ret != 0) {
            goto out;
        }
        for (k = 0; k < result_dim; k++) {
            result[k] += result_tmp[k] * a_len * b_len;
        }
    }
out:
    return ret;
}

static int
compute_two_tree_branch_stat(const tsk_treeseq_t *ts, const iter_state *l_state,
    iter_state *r_state, general_stat_func_t *f, sample_count_stat_params_t *f_params,
    tsk_size_t result_dim, tsk_size_t state_dim, double *result)
{
    int ret = 0;
    tsk_id_t e, c, ec, p, *updated_nodes = NULL;
    tsk_size_t j, k, n_updates;
    const double *restrict time = ts->tables->nodes.time;
    const tsk_id_t *restrict edges_child = ts->tables->edges.child;
    const tsk_id_t *restrict edges_parent = ts->tables->edges.parent;
    const tsk_size_t num_nodes = ts->tables->nodes.num_rows;
    tsk_bitset_t updates, *r_samples = r_state->node_samples;
    two_locus_work_t work;

    tsk_memset(&work, 0, sizeof(work));
    tsk_memset(&updates, 0, sizeof(updates));
    // only two alleles are possible for branch stats
    ret = two_locus_work_init(2, ts->num_samples, result_dim, state_dim, &work);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_bitset_init(&updates, num_nodes, 1);
    if (ret != 0) {
        goto out;
    }
    updated_nodes = tsk_calloc(num_nodes, sizeof(*updated_nodes));
    if (updated_nodes == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    // Identify modified nodes both added and removed
    for (j = 0; j < r_state->n_edges_out + r_state->n_edges_in; j++) {
        e = j < r_state->n_edges_out ? r_state->edges_out[j]
                                     : r_state->edges_in[j - r_state->n_edges_out];
        p = edges_parent[e];
        c = edges_child[e];
        // Identify affected nodes above child
        while (p != TSK_NULL) {
            tsk_bitset_set_bit(&updates, 0, (tsk_bitset_val_t) c);
            c = p;
            p = r_state->parent[p];
        }
    }
    // Subtract the whole contribution from the child node
    tsk_bitset_get_items(&updates, 0, updated_nodes, &n_updates);
    while (n_updates != 0) {
        n_updates--;
        c = updated_nodes[n_updates];
        compute_two_tree_branch_state_update(ts, c, l_state, r_state, state_dim,
            result_dim, -1, f, f_params, &work, result);
    }
    // Remove samples under nodes from removed edges to parent nodes
    for (j = 0; j < r_state->n_edges_out; j++) {
        e = r_state->edges_out[j];
        p = edges_parent[e];
        ec = edges_child[e]; // edge child
        while (p != TSK_NULL) {
            for (k = 0; k < state_dim; k++) {
                tsk_bitset_subtract(r_samples, (state_dim * (tsk_size_t) p) + k,
                    r_samples, (state_dim * (tsk_size_t) ec) + k);
            }
            p = r_state->parent[p];
        }
        r_state->branch_len[ec] = 0;
        r_state->parent[ec] = TSK_NULL;
    }
    // Add samples under nodes from added edges
    for (j = 0; j < r_state->n_edges_in; j++) {
        e = r_state->edges_in[j];
        p = edges_parent[e];
        ec = c = edges_child[e];
        r_state->branch_len[c] = time[p] - time[c];
        r_state->parent[c] = p;
        while (p != TSK_NULL) {
            tsk_bitset_set_bit(&updates, 0, (tsk_bitset_val_t) c);
            for (k = 0; k < state_dim; k++) {
                tsk_bitset_union(r_samples, (state_dim * (tsk_size_t) p) + k, r_samples,
                    (state_dim * (tsk_size_t) ec) + k);
            }
            c = p;
            p = r_state->parent[p];
        }
    }
    // Update all affected child nodes (fully subtracted, deferred from addition)
    n_updates = 0;
    tsk_bitset_get_items(&updates, 0, updated_nodes, &n_updates);
    while (n_updates != 0) {
        n_updates--;
        c = updated_nodes[n_updates];
        compute_two_tree_branch_state_update(ts, c, l_state, r_state, state_dim,
            result_dim, +1, f, f_params, &work, result);
    }
out:
    tsk_safe_free(updated_nodes);
    two_locus_work_free(&work);
    tsk_bitset_free(&updates);
    return ret;
}

static int
tsk_treeseq_two_branch_count_stat(const tsk_treeseq_t *self, tsk_size_t state_dim,
    tsk_size_t num_sample_sets, const tsk_size_t *sample_set_sizes,
    const tsk_id_t *sample_sets, tsk_size_t result_dim, general_stat_func_t *f,
    sample_count_stat_params_t *f_params, norm_func_t *TSK_UNUSED(norm_f),
    tsk_size_t n_rows, const double *row_positions, tsk_size_t n_cols,
    const double *col_positions, tsk_flags_t TSK_UNUSED(options), double *result)
{
    int ret = 0;
    int r, c;
    tsk_id_t *row_indexes = NULL, *col_indexes = NULL;
    tsk_size_t i, j, k, row, col, *row_repeats = NULL, *col_repeats = NULL;
    tsk_bitset_t node_samples, sample_sets_bits;
    iter_state l_state, r_state;
    double *result_tmp = NULL, *result_row;
    const tsk_size_t num_nodes = self->tables->nodes.num_rows;

    tsk_memset(&sample_sets_bits, 0, sizeof(sample_sets_bits));
    tsk_memset(&node_samples, 0, sizeof(node_samples));
    tsk_memset(&l_state, 0, sizeof(l_state));
    tsk_memset(&r_state, 0, sizeof(r_state));
    result_tmp = tsk_malloc(result_dim * sizeof(*result_tmp));
    if (result_tmp == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    ret = iter_state_init(&l_state, self, state_dim);
    if (ret != 0) {
        goto out;
    }
    ret = iter_state_init(&r_state, self, state_dim);
    if (ret != 0) {
        goto out;
    }
    ret = sample_sets_to_bitset(
        self, sample_set_sizes, sample_sets, num_sample_sets, &sample_sets_bits);
    if (ret != 0) {
        goto out;
    }
    ret = positions_to_tree_indexes(self, row_positions, n_rows, &row_indexes);
    if (ret != 0) {
        goto out;
    }
    ret = positions_to_tree_indexes(self, col_positions, n_cols, &col_indexes);
    if (ret != 0) {
        goto out;
    }
    ret = get_index_counts(row_indexes, n_rows, &row_repeats);
    if (ret != 0) {
        goto out;
    }
    ret = get_index_counts(col_indexes, n_cols, &col_repeats);
    if (ret != 0) {
        goto out;
    }
    ret = get_node_samples(self, state_dim, &sample_sets_bits, &node_samples);
    if (ret != 0) {
        goto out;
    }
    iter_state_clear(&l_state, state_dim, num_nodes, &node_samples);
    row = 0;
    for (r = 0; r < (row_indexes[n_rows ? n_rows - 1U : 0] - row_indexes[0] + 1); r++) {
        tsk_memset(result_tmp, 0, result_dim * sizeof(*result_tmp));
        iter_state_clear(&r_state, state_dim, num_nodes, &node_samples);
        ret = advance_collect_edges(&l_state, (tsk_id_t) r + row_indexes[0]);
        if (ret != 0) {
            goto out;
        }
        result_row = GET_2D_ROW(result, result_dim * n_cols, row);
        ret = compute_two_tree_branch_stat(
            self, &r_state, &l_state, f, f_params, result_dim, state_dim, result_tmp);
        if (ret != 0) {
            goto out;
        }
        col = 0;
        for (c = 0; c < (col_indexes[n_cols ? n_cols - 1 : 0] - col_indexes[0] + 1);
             c++) {
            ret = advance_collect_edges(&r_state, (tsk_id_t) c + col_indexes[0]);
            if (ret != 0) {
                goto out;
            }
            ret = compute_two_tree_branch_stat(self, &l_state, &r_state, f, f_params,
                result_dim, state_dim, result_tmp);
            if (ret != 0) {
                goto out;
            }
            for (i = 0; i < row_repeats[r]; i++) {
                for (j = 0; j < col_repeats[c]; j++) {
                    result_row = GET_2D_ROW(result, result_dim * n_cols, row + i);
                    for (k = 0; k < result_dim; k++) {
                        result_row[col + (j * result_dim) + k] = result_tmp[k];
                    }
                }
            }
            col += (col_repeats[c] * result_dim);
        }
        row += row_repeats[r];
    }
out:
    tsk_safe_free(result_tmp);
    tsk_safe_free(row_indexes);
    tsk_safe_free(col_indexes);
    tsk_safe_free(row_repeats);
    tsk_safe_free(col_repeats);
    iter_state_free(&l_state);
    iter_state_free(&r_state);
    tsk_bitset_free(&node_samples);
    tsk_bitset_free(&sample_sets_bits);
    return ret;
}

static int
check_sample_set_dups(tsk_size_t num_sample_sets, const tsk_size_t *sample_set_sizes,
    const tsk_id_t *sample_sets, const tsk_id_t *restrict sample_index_map,
    tsk_size_t num_samples)
{
    int ret;
    tsk_size_t j, k, l;
    tsk_id_t u, sample_index;
    tsk_bitset_t tmp;

    tsk_memset(&tmp, 0, sizeof(tmp));
    ret = tsk_bitset_init(&tmp, num_samples, 1);
    if (ret != 0) {
        goto out;
    }
    j = 0;
    for (k = 0; k < num_sample_sets; k++) {
        tsk_memset(tmp.data, 0, sizeof(*tmp.data) * tmp.row_len);
        for (l = 0; l < sample_set_sizes[k]; l++) {
            u = sample_sets[j];
            sample_index = sample_index_map[u];
            if (tsk_bitset_contains(&tmp, 0, (tsk_bitset_val_t) sample_index)) {
                ret = tsk_trace_error(TSK_ERR_DUPLICATE_SAMPLE);
                goto out;
            }
            tsk_bitset_set_bit(&tmp, 0, (tsk_bitset_val_t) sample_index);
            j++;
        }
    }
out:
    tsk_bitset_free(&tmp);
    return ret;
}

int
tsk_treeseq_two_locus_count_stat(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t result_dim, const tsk_id_t *set_indexes, general_stat_func_t *f,
    norm_func_t *norm_f, tsk_size_t out_rows, const tsk_id_t *row_sites,
    const double *row_positions, tsk_size_t out_cols, const tsk_id_t *col_sites,
    const double *col_positions, tsk_flags_t options, double *result)
{
    // TODO: generalize this function if we ever decide to do weighted two_locus stats.
    //       We only implement count stats and therefore we don't handle weights.
    int ret = 0;
    bool stat_site = !!(options & TSK_STAT_SITE);
    bool stat_branch = !!(options & TSK_STAT_BRANCH);
    tsk_size_t state_dim = num_sample_sets;
    sample_count_stat_params_t f_params = { .sample_sets = sample_sets,
        .num_sample_sets = num_sample_sets,
        .sample_set_sizes = sample_set_sizes,
        .set_indexes = set_indexes };

    // We do not support two-locus node stats
    if (!!(options & TSK_STAT_NODE)) {
        ret = tsk_trace_error(TSK_ERR_UNSUPPORTED_STAT_MODE);
        goto out;
    }
    // If no mode is specified, we default to site mode
    if (!(stat_site || stat_branch)) {
        stat_site = true;
    }
    // It's an error to specify more than one mode
    if (stat_site + stat_branch > 1) {
        ret = tsk_trace_error(TSK_ERR_MULTIPLE_STAT_MODES);
        goto out;
    }
    ret = tsk_treeseq_check_sample_sets(
        self, num_sample_sets, sample_set_sizes, sample_sets);
    if (ret != 0) {
        goto out;
    }
    if (result_dim < 1) {
        ret = tsk_trace_error(TSK_ERR_BAD_RESULT_DIMS);
        goto out;
    }
    if (stat_site) {
        ret = check_sites(row_sites, out_rows, self->tables->sites.num_rows);
        if (ret != 0) {
            goto out;
        }
        ret = check_sites(col_sites, out_cols, self->tables->sites.num_rows);
        if (ret != 0) {
            goto out;
        }
        ret = check_sample_set_dups(num_sample_sets, sample_set_sizes, sample_sets,
            self->sample_index_map, self->num_samples);
        if (ret != 0) {
            goto out;
        }
        ret = tsk_treeseq_two_site_count_stat(self, state_dim, num_sample_sets,
            sample_set_sizes, sample_sets, result_dim, f, &f_params, norm_f, out_rows,
            row_sites, out_cols, col_sites, options, result);
    } else if (stat_branch) {
        ret = check_positions(
            row_positions, out_rows, tsk_treeseq_get_sequence_length(self));
        if (ret != 0) {
            goto out;
        }
        ret = check_positions(
            col_positions, out_cols, tsk_treeseq_get_sequence_length(self));
        if (ret != 0) {
            goto out;
        }
        ret = tsk_treeseq_two_branch_count_stat(self, state_dim, num_sample_sets,
            sample_set_sizes, sample_sets, result_dim, f, &f_params, norm_f, out_rows,
            row_positions, out_cols, col_positions, options, result);
    }
out:
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
        n += (double) dims[k] - 1;
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
    tsk_size_t window_index, tsk_size_t *result_dims, tsk_flags_t options,
    double *result)
{
    int ret = 0;
    tsk_size_t afs_size;
    tsk_size_t k, allele, num_alleles, all_samples;
    double increment, *afs, *allele_counts, *allele_count;
    tsk_size_t *coordinate = tsk_malloc(num_sample_sets * sizeof(*coordinate));
    bool polarised = !!(options & TSK_STAT_POLARISED);
    const tsk_size_t K = num_sample_sets + 1;

    if (coordinate == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
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
    tsk_flags_t options, double *result)
{
    int ret = 0;
    tsk_id_t u, v;
    tsk_size_t tree_site, tree_index, window_index;
    tsk_size_t num_nodes = self->tables->nodes.num_rows;
    const tsk_id_t num_edges = (tsk_id_t) self->tables->edges.num_rows;
    const tsk_id_t *restrict I = self->tables->indexes.edge_insertion_order;
    const tsk_id_t *restrict O = self->tables->indexes.edge_removal_order;
    const double *restrict edge_left = self->tables->edges.left;
    const double *restrict edge_right = self->tables->edges.right;
    const tsk_id_t *restrict edge_parent = self->tables->edges.parent;
    const tsk_id_t *restrict edge_child = self->tables->edges.child;
    const double sequence_length = self->tables->sequence_length;
    tsk_id_t *restrict parent = tsk_malloc(num_nodes * sizeof(*parent));
    tsk_site_t *site;
    tsk_id_t tj, tk, h;
    tsk_size_t j;
    const tsk_size_t K = num_sample_sets + 1;
    double t_left, t_right;
    double *total_counts = tsk_malloc((1 + num_sample_sets) * sizeof(*total_counts));

    if (parent == NULL || total_counts == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memset(parent, 0xff, num_nodes * sizeof(*parent));

    for (j = 0; j < num_sample_sets; j++) {
        total_counts[j] = (double) sample_set_sizes[j];
    }
    total_counts[num_sample_sets] = (double) self->num_samples;

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
                num_sample_sets, window_index, result_dims, options, result);
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

static void
tsk_treeseq_update_branch_afs(const tsk_treeseq_t *self, tsk_id_t u, double right,
    double *restrict last_update, const double *restrict time, tsk_id_t *restrict parent,
    tsk_size_t *restrict coordinate, const double *counts, tsk_size_t num_sample_sets,
    tsk_size_t num_time_windows, const double *time_windows, tsk_size_t window_index,
    const tsk_size_t *result_dims, tsk_flags_t options, double *result)
{
    tsk_size_t afs_size;
    tsk_size_t k;
    tsk_size_t time_window_index;
    double *afs;
    bool polarised = !!(options & TSK_STAT_POLARISED);
    const double *count_row = GET_2D_ROW(counts, num_sample_sets + 1, u);
    double x = 0;
    double t_u, t_v;
    double tw_branch_length = 0;
    const tsk_size_t all_samples = (tsk_size_t) count_row[num_sample_sets];
    if (parent[u] != TSK_NULL) {
        t_u = time[u];
        t_v = time[parent[u]];
        if (0 < all_samples && all_samples < self->num_samples) {
            time_window_index = 0;
            afs_size = result_dims[num_sample_sets];
            while (time_window_index < num_time_windows
                   && time_windows[time_window_index] < t_v) {
                afs = result
                      + afs_size * (window_index * num_time_windows + time_window_index);
                for (k = 0; k < num_sample_sets; k++) {
                    coordinate[k] = (tsk_size_t) count_row[k];
                }
                if (!polarised) {
                    fold(coordinate, result_dims, num_sample_sets);
                }
                tw_branch_length
                    = TSK_MAX(0.0, TSK_MIN(time_windows[time_window_index + 1], t_v)
                                       - TSK_MAX(time_windows[time_window_index], t_u));
                x = (right - last_update[u]) * tw_branch_length;
                increment_nd_array_value(
                    afs, num_sample_sets, result_dims, coordinate, x);
                time_window_index++;
            }
        }
    }
    last_update[u] = right;
}

static int
tsk_treeseq_branch_allele_frequency_spectrum(const tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, double *counts, tsk_size_t num_windows,
    const double *windows, tsk_size_t num_time_windows, const double *time_windows,
    const tsk_size_t *result_dims, tsk_flags_t options, double *result)
{
    int ret = 0;
    tsk_id_t u, v;
    tsk_size_t window_index;
    tsk_size_t num_nodes = self->tables->nodes.num_rows;
    const tsk_id_t num_edges = (tsk_id_t) self->tables->edges.num_rows;
    const tsk_id_t *restrict I = self->tables->indexes.edge_insertion_order;
    const tsk_id_t *restrict O = self->tables->indexes.edge_removal_order;
    const double *restrict edge_left = self->tables->edges.left;
    const double *restrict edge_right = self->tables->edges.right;
    const tsk_id_t *restrict edge_parent = self->tables->edges.parent;
    const tsk_id_t *restrict edge_child = self->tables->edges.child;
    const double *restrict node_time = self->tables->nodes.time;
    const double sequence_length = self->tables->sequence_length;
    tsk_id_t *restrict parent = tsk_malloc(num_nodes * sizeof(*parent));
    double *restrict last_update = tsk_calloc(num_nodes, sizeof(*last_update));
    double *restrict branch_length = tsk_calloc(num_nodes, sizeof(*branch_length));
    tsk_size_t *restrict coordinate = tsk_malloc(num_sample_sets * sizeof(*coordinate));
    tsk_id_t tj, tk, h;
    double t_left, t_right, w_right;
    const tsk_size_t K = num_sample_sets + 1;

    if (self->time_uncalibrated && !(options & TSK_STAT_ALLOW_TIME_UNCALIBRATED)) {
        ret = tsk_trace_error(TSK_ERR_TIME_UNCALIBRATED);
        goto out;
    }

    if (parent == NULL || last_update == NULL || coordinate == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memset(parent, 0xff, num_nodes * sizeof(*parent));

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
            tsk_treeseq_update_branch_afs(self, u, t_left, last_update, node_time,
                parent, coordinate, counts, num_sample_sets, num_time_windows,
                time_windows, window_index, result_dims, options, result);
            while (v != TSK_NULL) {
                tsk_treeseq_update_branch_afs(self, v, t_left, last_update, node_time,
                    parent, coordinate, counts, num_sample_sets, num_time_windows,
                    time_windows, window_index, result_dims, options, result);
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
                tsk_treeseq_update_branch_afs(self, v, t_left, last_update, node_time,
                    parent, coordinate, counts, num_sample_sets, num_time_windows,
                    time_windows, window_index, result_dims, options, result);
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
                tsk_treeseq_update_branch_afs(self, u, w_right, last_update, node_time,
                    parent, coordinate, counts, num_sample_sets, num_time_windows,
                    time_windows, window_index, result_dims, options, result);
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
    if (coordinate != NULL) {
        free(coordinate);
    }
    return ret;
}

int
tsk_treeseq_allele_frequency_spectrum(const tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, const tsk_size_t *sample_set_sizes,
    const tsk_id_t *sample_sets, tsk_size_t num_windows, const double *windows,
    tsk_size_t num_time_windows, const double *time_windows, tsk_flags_t options,
    double *result)
{
    int ret = 0;
    bool stat_site = !!(options & TSK_STAT_SITE);
    bool stat_branch = !!(options & TSK_STAT_BRANCH);
    bool stat_node = !!(options & TSK_STAT_NODE);
    const double default_windows[] = { 0, self->tables->sequence_length };
    const double default_time_windows[] = { 0, INFINITY };
    const tsk_size_t num_nodes = self->tables->nodes.num_rows;
    const tsk_size_t K = num_sample_sets + 1;
    tsk_size_t j, k, l, afs_size;
    tsk_id_t u;
    tsk_size_t *result_dims = NULL;
    /* These counts should really be ints, but we use doubles so that we can
     * reuse code from the general_stats code paths. */
    double *counts = NULL;
    double *count_row;
    if (stat_node) {
        ret = tsk_trace_error(TSK_ERR_UNSUPPORTED_STAT_MODE);
        goto out;
    }
    /* If no mode is specified, we default to site mode */
    if (!(stat_site || stat_branch)) {
        stat_site = true;
    }
    /* It's an error to specify more than one mode */
    if (stat_site + stat_branch > 1) {
        ret = tsk_trace_error(TSK_ERR_MULTIPLE_STAT_MODES);
        goto out;
    }
    if (windows == NULL) {
        num_windows = 1;
        windows = default_windows;
    } else {
        ret = tsk_treeseq_check_windows(
            self, num_windows, windows, TSK_REQUIRE_FULL_SPAN);
        if (ret != 0) {
            goto out;
        }
    }
    if (time_windows == NULL) {
        num_time_windows = 1;
        time_windows = default_time_windows;
    } else {
        ret = tsk_treeseq_check_time_windows(num_time_windows, time_windows);
        if (ret != 0) {
            goto out;
        }
        // Site mode does not support time windows
        if (stat_site && !(time_windows[0] == 0.0 && isinf((float) time_windows[1]))) {
            ret = TSK_ERR_UNSUPPORTED_STAT_MODE;
            goto out;
        }
    }
    ret = tsk_treeseq_check_sample_sets(
        self, num_sample_sets, sample_set_sizes, sample_sets);
    if (ret != 0) {
        goto out;
    }

    /* the last element of result_dims stores the total size of the dimensions */
    result_dims = tsk_malloc((num_sample_sets + 1) * sizeof(*result_dims));
    counts = tsk_calloc(num_nodes * K, sizeof(*counts));
    if (counts == NULL || result_dims == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
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
                ret = tsk_trace_error(TSK_ERR_DUPLICATE_SAMPLE);
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
    tsk_memset(result, 0, num_windows * num_time_windows * afs_size * sizeof(*result));

    if (stat_site) {
        ret = tsk_treeseq_site_allele_frequency_spectrum(self, num_sample_sets,
            sample_set_sizes, counts, num_windows, windows, result_dims, options,
            result);
    } else {
        ret = tsk_treeseq_branch_allele_frequency_spectrum(self, num_sample_sets, counts,
            num_windows, windows, num_time_windows, time_windows, result_dims, options,
            result);
    }

    if (options & TSK_STAT_SPAN_NORMALISE) {
        span_normalise(num_windows, windows, afs_size * num_time_windows, result);
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
diversity_summary_func(tsk_size_t state_dim, const double *state,
    tsk_size_t TSK_UNUSED(result_dim), double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    double n;
    tsk_size_t j;

    for (j = 0; j < state_dim; j++) {
        n = (double) args.sample_set_sizes[j];
        result[j] = x[j] * (n - x[j]) / (n * (n - 1));
    }
    return 0;
}

int
tsk_treeseq_diversity(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_windows, const double *windows, tsk_flags_t options, double *result)
{
    return tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_sample_sets, NULL, diversity_summary_func, num_windows, windows,
        options, result);
}

static int
trait_covariance_summary_func(tsk_size_t state_dim, const double *state,
    tsk_size_t TSK_UNUSED(result_dim), double *result, void *params)
{
    weight_stat_params_t args = *(weight_stat_params_t *) params;
    const double n = (double) args.num_samples;
    const double *x = state;
    tsk_size_t j;

    for (j = 0; j < state_dim; j++) {
        result[j] = (x[j] * x[j]) / (2 * (n - 1) * (n - 1));
    }
    return 0;
}

int
tsk_treeseq_trait_covariance(const tsk_treeseq_t *self, tsk_size_t num_weights,
    const double *weights, tsk_size_t num_windows, const double *windows,
    tsk_flags_t options, double *result)
{
    tsk_size_t num_samples = self->num_samples;
    tsk_size_t j, k;
    int ret;
    const double *row;
    double *new_row;
    double *means = tsk_calloc(num_weights, sizeof(double));
    double *new_weights = tsk_malloc((num_weights + 1) * num_samples * sizeof(double));
    weight_stat_params_t args = { num_samples = self->num_samples };

    if (new_weights == NULL || means == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    if (num_weights == 0) {
        ret = tsk_trace_error(TSK_ERR_INSUFFICIENT_WEIGHTS);
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
        trait_covariance_summary_func, &args, num_windows, windows, options, result);

out:
    tsk_safe_free(means);
    tsk_safe_free(new_weights);
    return ret;
}

static int
trait_correlation_summary_func(tsk_size_t state_dim, const double *state,
    tsk_size_t TSK_UNUSED(result_dim), double *result, void *params)
{
    weight_stat_params_t args = *(weight_stat_params_t *) params;
    const double n = (double) args.num_samples;
    const double *x = state;
    double p;
    tsk_size_t j;

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
    const double *weights, tsk_size_t num_windows, const double *windows,
    tsk_flags_t options, double *result)
{
    tsk_size_t num_samples = self->num_samples;
    tsk_size_t j, k;
    int ret;
    double *means = tsk_calloc(num_weights, sizeof(double));
    double *meansqs = tsk_calloc(num_weights, sizeof(double));
    double *sds = tsk_calloc(num_weights, sizeof(double));
    const double *row;
    double *new_row;
    double *new_weights = tsk_malloc((num_weights + 1) * num_samples * sizeof(double));
    weight_stat_params_t args = { num_samples = self->num_samples };

    if (new_weights == NULL || means == NULL || meansqs == NULL || sds == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    if (num_weights < 1) {
        ret = tsk_trace_error(TSK_ERR_INSUFFICIENT_WEIGHTS);
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
        trait_correlation_summary_func, &args, num_windows, windows, options, result);

out:
    tsk_safe_free(means);
    tsk_safe_free(meansqs);
    tsk_safe_free(sds);
    tsk_safe_free(new_weights);
    return ret;
}

static int
trait_linear_model_summary_func(tsk_size_t state_dim, const double *state,
    tsk_size_t result_dim, double *result, void *params)
{
    covariates_stat_params_t args = *(covariates_stat_params_t *) params;
    const double num_samples = (double) args.num_samples;
    const tsk_size_t k = args.num_covariates;
    const double *V = args.V;
    ;
    const double *x = state;
    const double *v;
    double m, a, denom, z;
    tsk_size_t i, j;
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
    tsk_size_t num_windows, const double *windows, tsk_flags_t options, double *result)
{
    tsk_size_t num_samples = self->num_samples;
    tsk_size_t i, j, k;
    int ret;
    const double *w, *z;
    double *v, *new_row;
    double *V = tsk_calloc(num_covariates * num_weights, sizeof(double));
    double *new_weights
        = tsk_malloc((num_weights + num_covariates + 1) * num_samples * sizeof(double));

    covariates_stat_params_t args
        = { .num_samples = self->num_samples, .num_covariates = num_covariates, .V = V };

    // We assume that the covariates have been *already standardised*,
    // so that (a) 1 is in the span of the columns, and
    // (b) their crossproduct is the identity.
    // We could do this instead here with gsl linalg.

    if (new_weights == NULL || V == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    if (num_weights < 1) {
        ret = tsk_trace_error(TSK_ERR_INSUFFICIENT_WEIGHTS);
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
        options, result);

out:
    tsk_safe_free(V);
    tsk_safe_free(new_weights);
    return ret;
}

static int
segregating_sites_summary_func(tsk_size_t state_dim, const double *state,
    tsk_size_t TSK_UNUSED(result_dim), double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    double n;
    tsk_size_t j;

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
    tsk_size_t num_windows, const double *windows, tsk_flags_t options, double *result)
{
    return tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_sample_sets, NULL, segregating_sites_summary_func, num_windows,
        windows, options, result);
}

static int
Y1_summary_func(tsk_size_t TSK_UNUSED(state_dim), const double *state,
    tsk_size_t result_dim, double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    double ni, denom, numer;
    tsk_size_t i;

    for (i = 0; i < result_dim; i++) {
        ni = (double) args.sample_set_sizes[i];
        denom = ni * (ni - 1) * (ni - 2);
        numer = x[i] * (ni - x[i]) * (ni - x[i] - 1);
        result[i] = numer / denom;
    }
    return 0;
}

int
tsk_treeseq_Y1(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_windows, const double *windows, tsk_flags_t options, double *result)
{
    return tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_sample_sets, NULL, Y1_summary_func, num_windows, windows,
        options, result);
}

static int
D_summary_func(tsk_size_t state_dim, const double *state,
    tsk_size_t TSK_UNUSED(result_dim), double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    double n;
    const double *state_row;
    tsk_size_t j;

    for (j = 0; j < state_dim; j++) {
        n = (double) args.sample_set_sizes[j];
        state_row = GET_2D_ROW(state, 3, j);
        double p_AB = state_row[0] / n;
        double p_Ab = state_row[1] / n;
        double p_aB = state_row[2] / n;

        double p_A = p_AB + p_Ab;
        double p_B = p_AB + p_aB;
        result[j] = p_AB - (p_A * p_B);
    }

    return 0;
}

int
tsk_treeseq_D(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result)
{
    options |= TSK_STAT_POLARISED; // TODO: allow user to pick?
    return tsk_treeseq_two_locus_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_sample_sets, NULL, D_summary_func, norm_total_weighted,
        num_rows, row_sites, row_positions, num_cols, col_sites, col_positions, options,
        result);
}

static int
D2_summary_func(tsk_size_t state_dim, const double *state,
    tsk_size_t TSK_UNUSED(result_dim), double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    double n;
    const double *state_row;
    tsk_size_t j;

    for (j = 0; j < state_dim; j++) {
        n = (double) args.sample_set_sizes[j];
        state_row = GET_2D_ROW(state, 3, j);
        double p_AB = state_row[0] / n;
        double p_Ab = state_row[1] / n;
        double p_aB = state_row[2] / n;

        double p_A = p_AB + p_Ab;
        double p_B = p_AB + p_aB;
        result[j] = p_AB - (p_A * p_B);
        result[j] *= result[j];
    }

    return 0;
}

int
tsk_treeseq_D2(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result)
{
    return tsk_treeseq_two_locus_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_sample_sets, NULL, D2_summary_func, norm_total_weighted,
        num_rows, row_sites, row_positions, num_cols, col_sites, col_positions, options,
        result);
}

static int
r2_summary_func(tsk_size_t state_dim, const double *state,
    tsk_size_t TSK_UNUSED(result_dim), double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    double n;
    const double *state_row;
    tsk_size_t j;

    for (j = 0; j < state_dim; j++) {
        n = (double) args.sample_set_sizes[j];
        state_row = GET_2D_ROW(state, 3, j);
        double p_AB = state_row[0] / n;
        double p_Ab = state_row[1] / n;
        double p_aB = state_row[2] / n;

        double p_A = p_AB + p_Ab;
        double p_B = p_AB + p_aB;

        double D = p_AB - (p_A * p_B);
        double denom = p_A * p_B * (1 - p_A) * (1 - p_B);

        result[j] = (D * D) / denom;
    }
    return 0;
}

int
tsk_treeseq_r2(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result)
{
    return tsk_treeseq_two_locus_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_sample_sets, NULL, r2_summary_func, norm_hap_weighted, num_rows,
        row_sites, row_positions, num_cols, col_sites, col_positions, options, result);
}

static int
D_prime_summary_func(tsk_size_t state_dim, const double *state,
    tsk_size_t TSK_UNUSED(result_dim), double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    double n;
    const double *state_row;
    tsk_size_t j;

    for (j = 0; j < state_dim; j++) {
        n = (double) args.sample_set_sizes[j];
        state_row = GET_2D_ROW(state, 3, j);
        double p_AB = state_row[0] / n;
        double p_Ab = state_row[1] / n;
        double p_aB = state_row[2] / n;

        double p_A = p_AB + p_Ab;
        double p_B = p_AB + p_aB;

        double D = p_AB - (p_A * p_B);

        if (D >= 0) {
            result[j] = D / TSK_MIN(p_A * (1 - p_B), (1 - p_A) * p_B);
        } else if (D < 0) {
            result[j] = D / TSK_MIN(p_A * p_B, (1 - p_A) * (1 - p_B));
        }
    }
    return 0;
}

int
tsk_treeseq_D_prime(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result)
{
    options |= TSK_STAT_POLARISED; // TODO: allow user to pick?
    return tsk_treeseq_two_locus_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_sample_sets, NULL, D_prime_summary_func, norm_total_weighted,
        num_rows, row_sites, row_positions, num_cols, col_sites, col_positions, options,
        result);
}

static int
r_summary_func(tsk_size_t state_dim, const double *state,
    tsk_size_t TSK_UNUSED(result_dim), double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    double n;
    const double *state_row;
    tsk_size_t j;

    for (j = 0; j < state_dim; j++) {
        n = (double) args.sample_set_sizes[j];
        state_row = GET_2D_ROW(state, 3, j);
        double p_AB = state_row[0] / n;
        double p_Ab = state_row[1] / n;
        double p_aB = state_row[2] / n;

        double p_A = p_AB + p_Ab;
        double p_B = p_AB + p_aB;

        double D = p_AB - (p_A * p_B);
        double denom = p_A * p_B * (1 - p_A) * (1 - p_B);

        result[j] = D / sqrt(denom);
    }
    return 0;
}

int
tsk_treeseq_r(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result)
{
    options |= TSK_STAT_POLARISED; // TODO: allow user to pick?
    return tsk_treeseq_two_locus_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_sample_sets, NULL, r_summary_func, norm_total_weighted,
        num_rows, row_sites, row_positions, num_cols, col_sites, col_positions, options,
        result);
}

static int
Dz_summary_func(tsk_size_t state_dim, const double *state,
    tsk_size_t TSK_UNUSED(result_dim), double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    double n;
    const double *state_row;
    tsk_size_t j;

    for (j = 0; j < state_dim; j++) {
        n = (double) args.sample_set_sizes[j];
        state_row = GET_2D_ROW(state, 3, j);
        double p_AB = state_row[0] / n;
        double p_Ab = state_row[1] / n;
        double p_aB = state_row[2] / n;

        double p_A = p_AB + p_Ab;
        double p_B = p_AB + p_aB;

        double D = p_AB - (p_A * p_B);

        result[j] = D * (1 - 2 * p_A) * (1 - 2 * p_B);
    }
    return 0;
}

int
tsk_treeseq_Dz(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result)
{
    return tsk_treeseq_two_locus_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_sample_sets, NULL, Dz_summary_func, norm_total_weighted,
        num_rows, row_sites, row_positions, num_cols, col_sites, col_positions, options,
        result);
}

static int
pi2_summary_func(tsk_size_t state_dim, const double *state,
    tsk_size_t TSK_UNUSED(result_dim), double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    double n;
    const double *state_row;
    tsk_size_t j;

    for (j = 0; j < state_dim; j++) {
        n = (double) args.sample_set_sizes[j];
        state_row = GET_2D_ROW(state, 3, j);
        double p_AB = state_row[0] / n;
        double p_Ab = state_row[1] / n;
        double p_aB = state_row[2] / n;

        double p_A = p_AB + p_Ab;
        double p_B = p_AB + p_aB;
        result[j] = p_A * (1 - p_A) * p_B * (1 - p_B);
    }
    return 0;
}

int
tsk_treeseq_pi2(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result)
{
    return tsk_treeseq_two_locus_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_sample_sets, NULL, pi2_summary_func, norm_total_weighted,
        num_rows, row_sites, row_positions, num_cols, col_sites, col_positions, options,
        result);
}

static int
D2_unbiased_summary_func(tsk_size_t state_dim, const double *state,
    tsk_size_t TSK_UNUSED(result_dim), double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    double n;
    const double *state_row;
    tsk_size_t j;

    for (j = 0; j < state_dim; j++) {
        n = (double) args.sample_set_sizes[j];
        state_row = GET_2D_ROW(state, 3, j);
        double w_AB = state_row[0];
        double w_Ab = state_row[1];
        double w_aB = state_row[2];
        double w_ab = n - (w_AB + w_Ab + w_aB);
        result[j] = (1 / (n * (n - 1) * (n - 2) * (n - 3)))
                    * ((w_aB * w_aB * (w_Ab - 1) * w_Ab)
                          + ((w_ab - 1) * w_ab * (w_AB - 1) * w_AB)
                          - (w_aB * w_Ab * (w_Ab + (2 * w_ab * w_AB) - 1)));
    }
    return 0;
}

int
tsk_treeseq_D2_unbiased(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result)
{
    return tsk_treeseq_two_locus_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_sample_sets, NULL, D2_unbiased_summary_func,
        norm_total_weighted, num_rows, row_sites, row_positions, num_cols, col_sites,
        col_positions, options, result);
}

static int
Dz_unbiased_summary_func(tsk_size_t state_dim, const double *state,
    tsk_size_t TSK_UNUSED(result_dim), double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    double n;
    const double *state_row;
    tsk_size_t j;

    for (j = 0; j < state_dim; j++) {
        n = (double) args.sample_set_sizes[j];
        state_row = GET_2D_ROW(state, 3, j);
        double w_AB = state_row[0];
        double w_Ab = state_row[1];
        double w_aB = state_row[2];
        double w_ab = n - (w_AB + w_Ab + w_aB);
        result[j] = (1 / (n * (n - 1) * (n - 2) * (n - 3)))
                    * ((((w_AB * w_ab) - (w_Ab * w_aB)) * (w_aB + w_ab - w_AB - w_Ab)
                           * (w_Ab + w_ab - w_AB - w_aB))
                          - ((w_AB * w_ab) * (w_AB + w_ab - w_Ab - w_aB - 2))
                          - ((w_Ab * w_aB) * (w_Ab + w_aB - w_AB - w_ab - 2)));
    }
    return 0;
}

int
tsk_treeseq_Dz_unbiased(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result)
{
    return tsk_treeseq_two_locus_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_sample_sets, NULL, Dz_unbiased_summary_func,
        norm_total_weighted, num_rows, row_sites, row_positions, num_cols, col_sites,
        col_positions, options, result);
}

static int
pi2_unbiased_summary_func(tsk_size_t state_dim, const double *state,
    tsk_size_t TSK_UNUSED(result_dim), double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    double n;
    const double *state_row;
    tsk_size_t j;

    for (j = 0; j < state_dim; j++) {
        n = (double) args.sample_set_sizes[j];
        state_row = GET_2D_ROW(state, 3, j);
        double w_AB = state_row[0];
        double w_Ab = state_row[1];
        double w_aB = state_row[2];
        double w_ab = n - (w_AB + w_Ab + w_aB);
        result[j]
            = (1 / (n * (n - 1) * (n - 2) * (n - 3)))
              * (((w_AB + w_Ab) * (w_aB + w_ab) * (w_AB + w_aB) * (w_Ab + w_ab))
                    - ((w_AB * w_ab) * (w_AB + w_ab + (3 * w_Ab) + (3 * w_aB) - 1))
                    - ((w_Ab * w_aB) * (w_Ab + w_aB + (3 * w_AB) + (3 * w_ab) - 1)));
    }
    return 0;
}

int
tsk_treeseq_pi2_unbiased(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result)
{
    return tsk_treeseq_two_locus_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_sample_sets, NULL, pi2_unbiased_summary_func,
        norm_total_weighted, num_rows, row_sites, row_positions, num_cols, col_sites,
        col_positions, options, result);
}

/***********************************
 * Two way stats
 ***********************************/

static int
check_sample_stat_inputs(tsk_size_t num_sample_sets, tsk_size_t tuple_size,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples)
{
    int ret = 0;

    if (num_sample_sets < 1) {
        ret = tsk_trace_error(TSK_ERR_INSUFFICIENT_SAMPLE_SETS);
        goto out;
    }
    if (num_index_tuples < 1) {
        ret = tsk_trace_error(TSK_ERR_INSUFFICIENT_INDEX_TUPLES);
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
divergence_summary_func(tsk_size_t TSK_UNUSED(state_dim), const double *state,
    tsk_size_t result_dim, double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    double ni, nj, denom;
    tsk_id_t i, j;
    tsk_size_t k;

    for (k = 0; k < result_dim; k++) {
        i = args.set_indexes[2 * k];
        j = args.set_indexes[2 * k + 1];
        ni = (double) args.sample_set_sizes[i];
        nj = (double) args.sample_set_sizes[j];
        denom = ni * (nj - (i == j));
        result[k] = x[i] * (nj - x[j]) / denom;
    }
    return 0;
}

int
tsk_treeseq_divergence(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_windows,
    const double *windows, tsk_flags_t options, double *result)
{
    int ret = 0;
    ret = check_sample_stat_inputs(num_sample_sets, 2, num_index_tuples, index_tuples);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_index_tuples, index_tuples, divergence_summary_func,
        num_windows, windows, options, result);
out:
    return ret;
}

static int
genetic_relatedness_summary_func(tsk_size_t state_dim, const double *state,
    tsk_size_t result_dim, double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    tsk_id_t i, j;
    tsk_size_t k;
    double sumx = 0;
    double meanx, ni, nj;

    for (k = 0; k < state_dim; k++) {
        sumx += x[k] / (double) args.sample_set_sizes[k];
    }

    meanx = sumx / (double) state_dim;
    for (k = 0; k < result_dim; k++) {
        i = args.set_indexes[2 * k];
        j = args.set_indexes[2 * k + 1];
        ni = (double) args.sample_set_sizes[i];
        nj = (double) args.sample_set_sizes[j];
        result[k] = (x[i] / ni - meanx) * (x[j] / nj - meanx);
    }
    return 0;
}

static int
genetic_relatedness_noncentred_summary_func(tsk_size_t TSK_UNUSED(state_dim),
    const double *state, tsk_size_t result_dim, double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    tsk_id_t i, j;
    tsk_size_t k;
    double ni, nj;

    for (k = 0; k < result_dim; k++) {
        i = args.set_indexes[2 * k];
        j = args.set_indexes[2 * k + 1];
        ni = (double) args.sample_set_sizes[i];
        nj = (double) args.sample_set_sizes[j];
        result[k] = x[i] * x[j] / (ni * nj);
    }
    return 0;
}

int
tsk_treeseq_genetic_relatedness(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_windows,
    const double *windows, tsk_flags_t options, double *result)
{
    int ret = 0;
    ret = check_sample_stat_inputs(num_sample_sets, 2, num_index_tuples, index_tuples);
    if (ret != 0) {
        goto out;
    }
    if (!(options & TSK_STAT_NONCENTRED)) {
        ret = tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
            sample_sets, num_index_tuples, index_tuples,
            genetic_relatedness_summary_func, num_windows, windows, options, result);
    } else {
        ret = tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
            sample_sets, num_index_tuples, index_tuples,
            genetic_relatedness_noncentred_summary_func, num_windows, windows, options,
            result);
    }
out:
    return ret;
}

static int
genetic_relatedness_weighted_summary_func(tsk_size_t state_dim, const double *state,
    tsk_size_t result_dim, double *result, void *params)
{
    indexed_weight_stat_params_t args = *(indexed_weight_stat_params_t *) params;
    const double *x = state;
    tsk_id_t i, j;
    tsk_size_t k;
    double pn, ni, nj;

    pn = state[state_dim - 1];
    for (k = 0; k < result_dim; k++) {
        i = args.index_tuples[2 * k];
        j = args.index_tuples[2 * k + 1];
        ni = args.total_weights[i];
        nj = args.total_weights[j];
        result[k] = (x[i] - ni * pn) * (x[j] - nj * pn);
    }
    return 0;
}

static int
genetic_relatedness_weighted_noncentred_summary_func(tsk_size_t TSK_UNUSED(state_dim),
    const double *state, tsk_size_t result_dim, double *result, void *params)
{
    indexed_weight_stat_params_t args = *(indexed_weight_stat_params_t *) params;
    const double *x = state;
    tsk_id_t i, j;
    tsk_size_t k;

    for (k = 0; k < result_dim; k++) {
        i = args.index_tuples[2 * k];
        j = args.index_tuples[2 * k + 1];
        result[k] = x[i] * x[j];
    }
    return 0;
}

int
tsk_treeseq_genetic_relatedness_weighted(const tsk_treeseq_t *self,
    tsk_size_t num_weights, const double *weights, tsk_size_t num_index_tuples,
    const tsk_id_t *index_tuples, tsk_size_t num_windows, const double *windows,
    double *result, tsk_flags_t options)
{
    int ret = 0;
    tsk_size_t num_samples = self->num_samples;
    size_t j, k;
    indexed_weight_stat_params_t args;
    const double *row;
    double *new_row;
    double *total_weights = tsk_calloc((num_weights + 1), sizeof(*total_weights));
    double *new_weights
        = tsk_malloc((num_weights + 1) * num_samples * sizeof(*new_weights));

    if (total_weights == NULL || new_weights == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    if (num_weights == 0) {
        ret = tsk_trace_error(TSK_ERR_INSUFFICIENT_WEIGHTS);
        goto out;
    }

    // Add a column of ones to W
    for (j = 0; j < num_samples; j++) {
        row = GET_2D_ROW(weights, num_weights, j);
        new_row = GET_2D_ROW(new_weights, num_weights + 1, j);
        for (k = 0; k < num_weights; k++) {
            new_row[k] = row[k];
            total_weights[k] += row[k];
        }
        new_row[num_weights] = 1.0 / (double) num_samples;
    }
    total_weights[num_weights] = 1.0;

    args.total_weights = total_weights;
    args.index_tuples = index_tuples;
    if (!(options & TSK_STAT_NONCENTRED)) {
        ret = tsk_treeseq_general_stat(self, num_weights + 1, new_weights,
            num_index_tuples, genetic_relatedness_weighted_summary_func, &args,
            num_windows, windows, options, result);
        if (ret != 0) {
            goto out;
        }
    } else {
        ret = tsk_treeseq_general_stat(self, num_weights + 1, new_weights,
            num_index_tuples, genetic_relatedness_weighted_noncentred_summary_func,
            &args, num_windows, windows, options, result);
        if (ret != 0) {
            goto out;
        }
    }

out:
    tsk_safe_free(total_weights);
    tsk_safe_free(new_weights);
    return ret;
}

static int
Y2_summary_func(tsk_size_t TSK_UNUSED(state_dim), const double *state,
    tsk_size_t result_dim, double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    double ni, nj, denom;
    tsk_id_t i, j;
    tsk_size_t k;

    for (k = 0; k < result_dim; k++) {
        i = args.set_indexes[2 * k];
        j = args.set_indexes[2 * k + 1];
        ni = (double) args.sample_set_sizes[i];
        nj = (double) args.sample_set_sizes[j];
        denom = ni * nj * (nj - 1);
        result[k] = x[i] * (nj - x[j]) * (nj - x[j] - 1) / denom;
    }
    return 0;
}

int
tsk_treeseq_Y2(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_windows,
    const double *windows, tsk_flags_t options, double *result)
{
    int ret = 0;
    ret = check_sample_stat_inputs(num_sample_sets, 2, num_index_tuples, index_tuples);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_index_tuples, index_tuples, Y2_summary_func, num_windows,
        windows, options, result);
out:
    return ret;
}

static int
f2_summary_func(tsk_size_t TSK_UNUSED(state_dim), const double *state,
    tsk_size_t result_dim, double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    double ni, nj, denom, numer;
    tsk_id_t i, j;
    tsk_size_t k;

    for (k = 0; k < result_dim; k++) {
        i = args.set_indexes[2 * k];
        j = args.set_indexes[2 * k + 1];
        ni = (double) args.sample_set_sizes[i];
        nj = (double) args.sample_set_sizes[j];
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
    const double *windows, tsk_flags_t options, double *result)
{
    int ret = 0;
    ret = check_sample_stat_inputs(num_sample_sets, 2, num_index_tuples, index_tuples);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_index_tuples, index_tuples, f2_summary_func, num_windows,
        windows, options, result);
out:
    return ret;
}

static int
D2_ij_summary_func(tsk_size_t TSK_UNUSED(state_dim), const double *state,
    tsk_size_t result_dim, double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *state_row;
    double n;
    tsk_size_t k;
    tsk_id_t i, j;
    double p_A, p_B, p_AB, p_Ab, p_aB, D_i, D_j;

    for (k = 0; k < result_dim; k++) {
        i = args.set_indexes[2 * k];
        j = args.set_indexes[2 * k + 1];

        n = (double) args.sample_set_sizes[i];
        state_row = GET_2D_ROW(state, 3, i);
        p_AB = state_row[0] / n;
        p_Ab = state_row[1] / n;
        p_aB = state_row[2] / n;
        p_A = p_AB + p_Ab;
        p_B = p_AB + p_aB;
        D_i = p_AB - (p_A * p_B);

        n = (double) args.sample_set_sizes[j];
        state_row = GET_2D_ROW(state, 3, j);
        p_AB = state_row[0] / n;
        p_Ab = state_row[1] / n;
        p_aB = state_row[2] / n;
        p_A = p_AB + p_Ab;
        p_B = p_AB + p_aB;
        D_j = p_AB - (p_A * p_B);

        result[k] = D_i * D_j;
    }

    return 0;
}

int
tsk_treeseq_D2_ij(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result)
{
    int ret = 0;
    ret = check_sample_stat_inputs(num_sample_sets, 2, num_index_tuples, index_tuples);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_two_locus_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_index_tuples, index_tuples, D2_ij_summary_func,
        norm_total_weighted, num_rows, row_sites, row_positions, num_cols, col_sites,
        col_positions, options, result);
out:
    return ret;
}

static int
D2_ij_unbiased_summary_func(tsk_size_t TSK_UNUSED(state_dim), const double *state,
    tsk_size_t result_dim, double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *state_row;
    tsk_size_t k;
    tsk_id_t i, j;
    double n_i, n_j;
    double w_AB_i, w_Ab_i, w_aB_i, w_ab_i;
    double w_AB_j, w_Ab_j, w_aB_j, w_ab_j;

    for (k = 0; k < result_dim; k++) {
        i = args.set_indexes[2 * k];
        j = args.set_indexes[2 * k + 1];
        if (i == j) {
            // We require disjoint sample sets because we test equality here
            n_i = (double) args.sample_set_sizes[i];
            state_row = GET_2D_ROW(state, 3, i);
            w_AB_i = state_row[0];
            w_Ab_i = state_row[1];
            w_aB_i = state_row[2];
            w_ab_i = n_i - (w_AB_i + w_Ab_i + w_aB_i);
            result[k] = (w_AB_i * (w_AB_i - 1) * w_ab_i * (w_ab_i - 1)
                            + w_Ab_i * (w_Ab_i - 1) * w_aB_i * (w_aB_i - 1)
                            - 2 * w_AB_i * w_Ab_i * w_aB_i * w_ab_i)
                        / n_i / (n_i - 1) / (n_i - 2) / (n_i - 3);
        }

        else {
            n_i = (double) args.sample_set_sizes[i];
            state_row = GET_2D_ROW(state, 3, i);
            w_AB_i = state_row[0];
            w_Ab_i = state_row[1];
            w_aB_i = state_row[2];
            w_ab_i = n_i - (w_AB_i + w_Ab_i + w_aB_i);

            n_j = (double) args.sample_set_sizes[j];
            state_row = GET_2D_ROW(state, 3, j);
            w_AB_j = state_row[0];
            w_Ab_j = state_row[1];
            w_aB_j = state_row[2];
            w_ab_j = n_j - (w_AB_j + w_Ab_j + w_aB_j);

            result[k] = (w_Ab_i * w_aB_i - w_AB_i * w_ab_i)
                        * (w_Ab_j * w_aB_j - w_AB_j * w_ab_j) / n_i / (n_i - 1) / n_j
                        / (n_j - 1);
        }
    }

    return 0;
}

int
tsk_treeseq_D2_ij_unbiased(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result)
{
    int ret = 0;
    ret = check_sample_stat_inputs(num_sample_sets, 2, num_index_tuples, index_tuples);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_two_locus_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_index_tuples, index_tuples, D2_ij_unbiased_summary_func,
        norm_total_weighted, num_rows, row_sites, row_positions, num_cols, col_sites,
        col_positions, options, result);
out:
    return ret;
}

static int
r2_ij_summary_func(tsk_size_t TSK_UNUSED(state_dim), const double *state,
    tsk_size_t result_dim, double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *state_row;
    tsk_size_t k;
    tsk_id_t i, j;
    double n, pAB, pAb, paB, pA, pB, D_i, D_j, denom_i, denom_j;

    for (k = 0; k < result_dim; k++) {
        i = args.set_indexes[2 * k];
        j = args.set_indexes[2 * k + 1];

        n = (double) args.sample_set_sizes[i];
        state_row = GET_2D_ROW(state, 3, i);
        pAB = state_row[0] / n;
        pAb = state_row[1] / n;
        paB = state_row[2] / n;
        pA = pAB + pAb;
        pB = pAB + paB;
        D_i = pAB - (pA * pB);
        denom_i = sqrt(pA * (1 - pA) * pB * (1 - pB));

        n = (double) args.sample_set_sizes[j];
        state_row = GET_2D_ROW(state, 3, j);
        pAB = state_row[0] / n;
        pAb = state_row[1] / n;
        paB = state_row[2] / n;
        pA = pAB + pAb;
        pB = pAB + paB;
        D_j = pAB - (pA * pB);
        denom_j = sqrt(pA * (1 - pA) * pB * (1 - pB));

        result[k] = (D_i * D_j) / (denom_i * denom_j);
    }
    return 0;
}

int
tsk_treeseq_r2_ij(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_index_tuples, const tsk_id_t *index_tuples, tsk_size_t num_rows,
    const tsk_id_t *row_sites, const double *row_positions, tsk_size_t num_cols,
    const tsk_id_t *col_sites, const double *col_positions, tsk_flags_t options,
    double *result)
{
    int ret = 0;
    ret = check_sample_stat_inputs(num_sample_sets, 2, num_index_tuples, index_tuples);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_two_locus_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_index_tuples, index_tuples, r2_ij_summary_func,
        norm_hap_weighted_ij, num_rows, row_sites, row_positions, num_cols, col_sites,
        col_positions, options, result);
out:
    return ret;
}

/***********************************
 * Three way stats
 ***********************************/

static int
Y3_summary_func(tsk_size_t TSK_UNUSED(state_dim), const double *state,
    tsk_size_t result_dim, double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    double ni, nj, nk, denom, numer;
    tsk_id_t i, j, k;
    tsk_size_t tuple_index;

    for (tuple_index = 0; tuple_index < result_dim; tuple_index++) {
        i = args.set_indexes[3 * tuple_index];
        j = args.set_indexes[3 * tuple_index + 1];
        k = args.set_indexes[3 * tuple_index + 2];
        ni = (double) args.sample_set_sizes[i];
        nj = (double) args.sample_set_sizes[j];
        nk = (double) args.sample_set_sizes[k];
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
    const double *windows, tsk_flags_t options, double *result)
{
    int ret = 0;
    ret = check_sample_stat_inputs(num_sample_sets, 3, num_index_tuples, index_tuples);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_index_tuples, index_tuples, Y3_summary_func, num_windows,
        windows, options, result);
out:
    return ret;
}

static int
f3_summary_func(tsk_size_t TSK_UNUSED(state_dim), const double *state,
    tsk_size_t result_dim, double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    double ni, nj, nk, denom, numer;
    tsk_id_t i, j, k;
    tsk_size_t tuple_index;

    for (tuple_index = 0; tuple_index < result_dim; tuple_index++) {
        i = args.set_indexes[3 * tuple_index];
        j = args.set_indexes[3 * tuple_index + 1];
        k = args.set_indexes[3 * tuple_index + 2];
        ni = (double) args.sample_set_sizes[i];
        nj = (double) args.sample_set_sizes[j];
        nk = (double) args.sample_set_sizes[k];
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
    const double *windows, tsk_flags_t options, double *result)
{
    int ret = 0;
    ret = check_sample_stat_inputs(num_sample_sets, 3, num_index_tuples, index_tuples);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_index_tuples, index_tuples, f3_summary_func, num_windows,
        windows, options, result);
out:
    return ret;
}

/***********************************
 * Four way stats
 ***********************************/

static int
f4_summary_func(tsk_size_t TSK_UNUSED(state_dim), const double *state,
    tsk_size_t result_dim, double *result, void *params)
{
    sample_count_stat_params_t args = *(sample_count_stat_params_t *) params;
    const double *x = state;
    double ni, nj, nk, nl, denom, numer;
    tsk_id_t i, j, k, l;
    tsk_size_t tuple_index;

    for (tuple_index = 0; tuple_index < result_dim; tuple_index++) {
        i = args.set_indexes[4 * tuple_index];
        j = args.set_indexes[4 * tuple_index + 1];
        k = args.set_indexes[4 * tuple_index + 2];
        l = args.set_indexes[4 * tuple_index + 3];
        ni = (double) args.sample_set_sizes[i];
        nj = (double) args.sample_set_sizes[j];
        nk = (double) args.sample_set_sizes[k];
        nl = (double) args.sample_set_sizes[l];
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
    const double *windows, tsk_flags_t options, double *result)
{
    int ret = 0;
    ret = check_sample_stat_inputs(num_sample_sets, 4, num_index_tuples, index_tuples);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_sample_count_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_index_tuples, index_tuples, f4_summary_func, num_windows,
        windows, options, result);
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
    int ret = 0;

    ret = tsk_mutation_table_get_row(&self->tables->mutations, index, mutation);
    if (ret != 0) {
        goto out;
    }
    mutation->edge = self->site_mutations_mem[index].edge;
    mutation->inherited_state = self->site_mutations_mem[index].inherited_state;
    mutation->inherited_state_length
        = self->site_mutations_mem[index].inherited_state_length;
out:
    return ret;
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
    tsk_table_collection_t *tables = tsk_malloc(sizeof(*tables));

    if (tables == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    ret = tsk_treeseq_copy_tables(self, tables, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_simplify(tables, samples, num_samples, options, node_map);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_init(
        output, tables, TSK_TS_INIT_BUILD_INDEXES | TSK_TAKE_OWNERSHIP);
    /* Once tsk_treeseq_init has returned ownership of tables is transferred */
    tables = NULL;
out:
    if (tables != NULL) {
        tsk_table_collection_free(tables);
        tsk_safe_free(tables);
    }
    return ret;
}

int TSK_WARN_UNUSED
tsk_treeseq_split_edges(const tsk_treeseq_t *self, double time, tsk_flags_t flags,
    tsk_id_t population, const char *metadata, tsk_size_t metadata_length,
    tsk_flags_t TSK_UNUSED(options), tsk_treeseq_t *output)
{
    int ret = 0;
    tsk_table_collection_t *tables = tsk_malloc(sizeof(*tables));
    const double *restrict node_time = self->tables->nodes.time;
    const tsk_size_t num_edges = self->tables->edges.num_rows;
    const tsk_size_t num_mutations = self->tables->mutations.num_rows;
    tsk_id_t *split_edge = tsk_malloc(num_edges * sizeof(*split_edge));
    tsk_id_t j, u, mapped_node, ret_id;
    double mutation_time;
    tsk_edge_t edge;
    tsk_mutation_t mutation;
    tsk_bookmark_t sort_start;

    memset(output, 0, sizeof(*output));
    if (split_edge == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    ret = tsk_treeseq_copy_tables(self, tables, 0);
    if (ret != 0) {
        goto out;
    }
    if (tables->migrations.num_rows > 0) {
        ret = tsk_trace_error(TSK_ERR_MIGRATIONS_NOT_SUPPORTED);
        goto out;
    }
    /* We could catch this below in add_row, but it's simpler to guarantee
     * that we always catch the error in corner cases where the values
     * aren't used. */
    if (population < -1 || population >= (tsk_id_t) self->tables->populations.num_rows) {
        ret = tsk_trace_error(TSK_ERR_POPULATION_OUT_OF_BOUNDS);
        goto out;
    }
    if (!tsk_isfinite(time)) {
        ret = tsk_trace_error(TSK_ERR_TIME_NONFINITE);
        goto out;
    }

    tsk_edge_table_clear(&tables->edges);
    tsk_memset(split_edge, TSK_NULL, num_edges * sizeof(*split_edge));

    for (j = 0; j < (tsk_id_t) num_edges; j++) {
        /* Would prefer to use tsk_edge_table_get_row_unsafe, but it's
         * currently static to tables.c */
        ret = tsk_edge_table_get_row(&self->tables->edges, j, &edge);
        tsk_bug_assert(ret == 0);
        if (node_time[edge.child] < time && time < node_time[edge.parent]) {
            u = tsk_node_table_add_row(&tables->nodes, flags, time, population, TSK_NULL,
                metadata, metadata_length);
            if (u < 0) {
                ret = (int) u;
                goto out;
            }
            ret_id = tsk_edge_table_add_row(&tables->edges, edge.left, edge.right, u,
                edge.child, edge.metadata, edge.metadata_length);
            if (ret_id < 0) {
                ret = (int) ret_id;
                goto out;
            }
            edge.child = u;
            split_edge[j] = u;
        }
        ret_id = tsk_edge_table_add_row(&tables->edges, edge.left, edge.right,
            edge.parent, edge.child, edge.metadata, edge.metadata_length);
        if (ret_id < 0) {
            ret = (int) ret_id;
            goto out;
        }
    }

    for (j = 0; j < (tsk_id_t) num_mutations; j++) {
        /* Note: we could speed this up a bit by accessing the local
         * memory for mutations directly. */
        ret = tsk_treeseq_get_mutation(self, j, &mutation);
        tsk_bug_assert(ret == 0);
        mapped_node = TSK_NULL;
        if (mutation.edge != TSK_NULL) {
            mapped_node = split_edge[mutation.edge];
        }
        mutation_time = tsk_is_unknown_time(mutation.time) ? node_time[mutation.node]
                                                           : mutation.time;
        if (mapped_node != TSK_NULL && mutation_time >= time) {
            /* Update the column in-place to save a bit of time. */
            tables->mutations.node[j] = mapped_node;
        }
    }

    /* Skip mutations and sites as they haven't been altered */
    /* Note we can probably optimise the edge sort a bit here also by
     * reasoning about when the first edge gets altered in the table.
     */
    memset(&sort_start, 0, sizeof(sort_start));
    sort_start.sites = tables->sites.num_rows;
    sort_start.mutations = tables->mutations.num_rows;
    ret = tsk_table_collection_sort(tables, &sort_start, 0);
    if (ret != 0) {
        goto out;
    }

    ret = tsk_treeseq_init(
        output, tables, TSK_TS_INIT_BUILD_INDEXES | TSK_TAKE_OWNERSHIP);
    tables = NULL;
out:
    if (tables != NULL) {
        tsk_table_collection_free(tables);
        tsk_safe_free(tables);
    }
    tsk_safe_free(split_edge);
    return ret;
}

/* ======================================================== *
 * tree_position
 * ======================================================== */

static void
tsk_tree_position_set_null(tsk_tree_position_t *self)
{
    self->index = -1;
    self->interval.left = 0;
    self->interval.right = 0;
}

int
tsk_tree_position_init(tsk_tree_position_t *self, const tsk_treeseq_t *tree_sequence,
    tsk_flags_t TSK_UNUSED(options))
{
    memset(self, 0, sizeof(*self));
    self->tree_sequence = tree_sequence;
    tsk_tree_position_set_null(self);
    return 0;
}

int
tsk_tree_position_free(tsk_tree_position_t *TSK_UNUSED(self))
{
    return 0;
}

int
tsk_tree_position_print_state(const tsk_tree_position_t *self, FILE *out)
{
    fprintf(out, "Tree position state\n");
    fprintf(out, "index = %d\n", (int) self->index);
    fprintf(out, "interval = [%f,\t%f)\n", self->interval.left, self->interval.right);
    fprintf(
        out, "out   = start=%d\tstop=%d\n", (int) self->out.start, (int) self->out.stop);
    fprintf(
        out, "in    = start=%d\tstop=%d\n", (int) self->in.start, (int) self->in.stop);
    return 0;
}

bool
tsk_tree_position_next(tsk_tree_position_t *self)
{
    const tsk_table_collection_t *tables = self->tree_sequence->tables;
    const tsk_id_t M = (tsk_id_t) tables->edges.num_rows;
    const tsk_id_t num_trees = (tsk_id_t) self->tree_sequence->num_trees;
    const double *restrict left_coords = tables->edges.left;
    const tsk_id_t *restrict left_order = tables->indexes.edge_insertion_order;
    const double *restrict right_coords = tables->edges.right;
    const tsk_id_t *restrict right_order = tables->indexes.edge_removal_order;
    const double *restrict breakpoints = self->tree_sequence->breakpoints;
    tsk_id_t j, left_current_index, right_current_index;
    double left;

    if (self->index == -1) {
        self->interval.right = 0;
        self->in.stop = 0;
        self->out.stop = 0;
        self->direction = TSK_DIR_FORWARD;
    }

    if (self->direction == TSK_DIR_FORWARD) {
        left_current_index = self->in.stop;
        right_current_index = self->out.stop;
    } else {
        left_current_index = self->out.stop + 1;
        right_current_index = self->in.stop + 1;
    }

    left = self->interval.right;

    j = right_current_index;
    self->out.start = j;
    while (j < M && right_coords[right_order[j]] == left) {
        j++;
    }
    self->out.stop = j;
    self->out.order = right_order;

    j = left_current_index;
    self->in.start = j;
    while (j < M && left_coords[left_order[j]] == left) {
        j++;
    }
    self->in.stop = j;
    self->in.order = left_order;

    self->direction = TSK_DIR_FORWARD;
    self->index++;
    if (self->index == num_trees) {
        tsk_tree_position_set_null(self);
    } else {
        self->interval.left = left;
        self->interval.right = breakpoints[self->index + 1];
    }
    return self->index != -1;
}

bool
tsk_tree_position_prev(tsk_tree_position_t *self)
{
    const tsk_table_collection_t *tables = self->tree_sequence->tables;
    const tsk_id_t M = (tsk_id_t) tables->edges.num_rows;
    const double sequence_length = tables->sequence_length;
    const tsk_id_t num_trees = (tsk_id_t) self->tree_sequence->num_trees;
    const double *restrict left_coords = tables->edges.left;
    const tsk_id_t *restrict left_order = tables->indexes.edge_insertion_order;
    const double *restrict right_coords = tables->edges.right;
    const tsk_id_t *restrict right_order = tables->indexes.edge_removal_order;
    const double *restrict breakpoints = self->tree_sequence->breakpoints;
    tsk_id_t j, left_current_index, right_current_index;
    double right;

    if (self->index == -1) {
        self->index = num_trees;
        self->interval.left = sequence_length;
        self->in.stop = M - 1;
        self->out.stop = M - 1;
        self->direction = TSK_DIR_REVERSE;
    }

    if (self->direction == TSK_DIR_REVERSE) {
        left_current_index = self->out.stop;
        right_current_index = self->in.stop;
    } else {
        left_current_index = self->in.stop - 1;
        right_current_index = self->out.stop - 1;
    }

    right = self->interval.left;

    j = left_current_index;
    self->out.start = j;
    while (j >= 0 && left_coords[left_order[j]] == right) {
        j--;
    }
    self->out.stop = j;
    self->out.order = left_order;

    j = right_current_index;
    self->in.start = j;
    while (j >= 0 && right_coords[right_order[j]] == right) {
        j--;
    }
    self->in.stop = j;
    self->in.order = right_order;

    self->index--;
    self->direction = TSK_DIR_REVERSE;
    if (self->index == -1) {
        tsk_tree_position_set_null(self);
    } else {
        self->interval.left = breakpoints[self->index];
        self->interval.right = right;
    }
    return self->index != -1;
}

int TSK_WARN_UNUSED
tsk_tree_position_seek_forward(tsk_tree_position_t *self, tsk_id_t index)
{
    int ret = 0;
    const tsk_table_collection_t *tables = self->tree_sequence->tables;
    const tsk_id_t M = (tsk_id_t) tables->edges.num_rows;
    const tsk_id_t num_trees = (tsk_id_t) self->tree_sequence->num_trees;
    const double *restrict left_coords = tables->edges.left;
    const tsk_id_t *restrict left_order = tables->indexes.edge_insertion_order;
    const double *restrict right_coords = tables->edges.right;
    const tsk_id_t *restrict right_order = tables->indexes.edge_removal_order;
    const double *restrict breakpoints = self->tree_sequence->breakpoints;
    tsk_id_t j, left_current_index, right_current_index;
    double left;

    tsk_bug_assert(index >= self->index && index < num_trees);

    if (self->index == -1) {
        self->interval.right = 0;
        self->in.stop = 0;
        self->out.stop = 0;
        self->direction = TSK_DIR_FORWARD;
    }

    if (self->direction == TSK_DIR_FORWARD) {
        left_current_index = self->in.stop;
        right_current_index = self->out.stop;
    } else {
        left_current_index = self->out.stop + 1;
        right_current_index = self->in.stop + 1;
    }

    self->direction = TSK_DIR_FORWARD;
    left = breakpoints[index];

    j = right_current_index;
    self->out.start = j;
    while (j < M && right_coords[right_order[j]] <= left) {
        j++;
    }
    self->out.stop = j;

    if (self->index == -1) {
        self->out.start = self->out.stop;
    }

    j = left_current_index;
    while (j < M && right_coords[left_order[j]] <= left) {
        j++;
    }
    self->in.start = j;
    while (j < M && left_coords[left_order[j]] <= left) {
        j++;
    }
    self->in.stop = j;

    self->interval.left = left;
    self->interval.right = breakpoints[index + 1];
    self->out.order = right_order;
    self->in.order = left_order;
    self->index = index;
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_position_seek_backward(tsk_tree_position_t *self, tsk_id_t index)
{
    int ret = 0;
    const tsk_table_collection_t *tables = self->tree_sequence->tables;
    const tsk_id_t M = (tsk_id_t) tables->edges.num_rows;
    const double sequence_length = tables->sequence_length;
    const tsk_id_t num_trees = (tsk_id_t) self->tree_sequence->num_trees;
    const double *restrict left_coords = tables->edges.left;
    const tsk_id_t *restrict left_order = tables->indexes.edge_insertion_order;
    const double *restrict right_coords = tables->edges.right;
    const tsk_id_t *restrict right_order = tables->indexes.edge_removal_order;
    const double *restrict breakpoints = self->tree_sequence->breakpoints;
    tsk_id_t j, left_current_index, right_current_index;
    double right;

    if (self->index == -1) {
        self->index = num_trees;
        self->interval.left = sequence_length;
        self->in.stop = M - 1;
        self->out.stop = M - 1;
        self->direction = TSK_DIR_REVERSE;
    }
    tsk_bug_assert(index <= self->index);

    if (self->direction == TSK_DIR_REVERSE) {
        left_current_index = self->out.stop;
        right_current_index = self->in.stop;
    } else {
        left_current_index = self->in.stop - 1;
        right_current_index = self->out.stop - 1;
    }

    self->direction = TSK_DIR_REVERSE;
    right = breakpoints[index + 1];

    j = left_current_index;
    self->out.start = j;
    while (j >= 0 && left_coords[left_order[j]] >= right) {
        j--;
    }
    self->out.stop = j;

    if (self->index == num_trees) {
        self->out.start = self->out.stop;
    }

    j = right_current_index;
    while (j >= 0 && left_coords[right_order[j]] >= right) {
        j--;
    }
    self->in.start = j;
    while (j >= 0 && right_coords[right_order[j]] >= right) {
        j--;
    }
    self->in.stop = j;

    self->interval.right = right;
    self->interval.left = breakpoints[index];
    self->out.order = left_order;
    self->in.order = right_order;
    self->index = index;

    return ret;
}

/* ======================================================== *
 * Tree
 * ======================================================== */

/* Return the root for the specified node.
 * NOTE: no bounds checking is done here.
 */
static tsk_id_t
tsk_tree_get_node_root(const tsk_tree_t *self, tsk_id_t u)
{
    const tsk_id_t *restrict parent = self->parent;

    while (parent[u] != TSK_NULL) {
        u = parent[u];
    }
    return u;
}

int TSK_WARN_UNUSED
tsk_tree_init(tsk_tree_t *self, const tsk_treeseq_t *tree_sequence, tsk_flags_t options)
{
    int ret = 0;
    tsk_size_t num_samples, num_nodes, N;

    tsk_memset(self, 0, sizeof(tsk_tree_t));
    if (tree_sequence == NULL) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    num_nodes = tree_sequence->tables->nodes.num_rows;
    num_samples = tree_sequence->num_samples;
    self->num_nodes = num_nodes;
    self->virtual_root = (tsk_id_t) num_nodes;
    self->tree_sequence = tree_sequence;
    self->samples = tree_sequence->samples;
    self->options = options;
    self->root_threshold = 1;

    /* Allocate space in the quintuply linked tree for the virtual root */
    N = num_nodes + 1;
    self->parent = tsk_malloc(N * sizeof(*self->parent));
    self->left_child = tsk_malloc(N * sizeof(*self->left_child));
    self->right_child = tsk_malloc(N * sizeof(*self->right_child));
    self->left_sib = tsk_malloc(N * sizeof(*self->left_sib));
    self->right_sib = tsk_malloc(N * sizeof(*self->right_sib));
    self->num_children = tsk_calloc(N, sizeof(*self->num_children));
    self->edge = tsk_malloc(N * sizeof(*self->edge));
    if (self->parent == NULL || self->left_child == NULL || self->right_child == NULL
        || self->left_sib == NULL || self->right_sib == NULL
        || self->num_children == NULL || self->edge == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    if (!(self->options & TSK_NO_SAMPLE_COUNTS)) {
        self->num_samples = tsk_calloc(N, sizeof(*self->num_samples));
        self->num_tracked_samples = tsk_calloc(N, sizeof(*self->num_tracked_samples));
        if (self->num_samples == NULL || self->num_tracked_samples == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
    }
    if (self->options & TSK_SAMPLE_LISTS) {
        self->left_sample = tsk_malloc(N * sizeof(*self->left_sample));
        self->right_sample = tsk_malloc(N * sizeof(*self->right_sample));
        self->next_sample = tsk_malloc(num_samples * sizeof(*self->next_sample));
        if (self->left_sample == NULL || self->right_sample == NULL
            || self->next_sample == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
    }

    ret = tsk_tree_position_init(&self->tree_pos, tree_sequence, 0);
    if (ret != 0) {
        goto out;
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
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    /* Don't allow the value to be set when the tree is out of the null
     * state */
    if (self->index != -1) {
        ret = tsk_trace_error(TSK_ERR_UNSUPPORTED_OPERATION);
        goto out;
    }
    self->root_threshold = root_threshold;
    /* Reset the roots */
    ret = tsk_tree_clear(self);
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
    tsk_safe_free(self->left_sample);
    tsk_safe_free(self->right_sample);
    tsk_safe_free(self->next_sample);
    tsk_safe_free(self->num_children);
    tsk_safe_free(self->edge);
    tsk_tree_position_free(&self->tree_pos);
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
        ret = tsk_trace_error(TSK_ERR_UNSUPPORTED_OPERATION);
        goto out;
    }
    tsk_memset(self->num_tracked_samples, 0,
        (self->num_nodes + 1) * sizeof(*self->num_tracked_samples));
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_set_tracked_samples(
    tsk_tree_t *self, tsk_size_t num_tracked_samples, const tsk_id_t *tracked_samples)
{
    int ret = TSK_ERR_GENERIC;
    tsk_size_t *tree_num_tracked_samples = self->num_tracked_samples;
    const tsk_id_t *parent = self->parent;
    tsk_size_t j;
    tsk_id_t u;

    /* TODO This is not needed when the tree is new. We should use the
     * state machine to check and only reset the tracked samples when needed.
     */
    ret = tsk_tree_reset_tracked_samples(self);
    if (ret != 0) {
        goto out;
    }
    self->num_tracked_samples[self->virtual_root] = num_tracked_samples;
    for (j = 0; j < num_tracked_samples; j++) {
        u = tracked_samples[j];
        if (u < 0 || u >= (tsk_id_t) self->num_nodes) {
            ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
            goto out;
        }
        if (!tsk_treeseq_is_sample(self->tree_sequence, u)) {
            ret = tsk_trace_error(TSK_ERR_BAD_SAMPLES);
            goto out;
        }
        if (self->num_tracked_samples[u] != 0) {
            ret = tsk_trace_error(TSK_ERR_DUPLICATE_SAMPLE);
            goto out;
        }
        /* Propagate this upwards */
        while (u != TSK_NULL) {
            tree_num_tracked_samples[u]++;
            u = parent[u];
        }
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_track_descendant_samples(tsk_tree_t *self, tsk_id_t node)
{
    int ret = 0;
    tsk_id_t *nodes = tsk_malloc(tsk_tree_get_size_bound(self) * sizeof(*nodes));
    const tsk_id_t *restrict parent = self->parent;
    const tsk_id_t *restrict left_child = self->left_child;
    const tsk_id_t *restrict right_sib = self->right_sib;
    const tsk_flags_t *restrict flags = self->tree_sequence->tables->nodes.flags;
    tsk_size_t *num_tracked_samples = self->num_tracked_samples;
    tsk_size_t n, j, num_nodes;
    tsk_id_t u, v;

    if (nodes == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    ret = tsk_tree_postorder_from(self, node, nodes, &num_nodes);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_tree_reset_tracked_samples(self);
    if (ret != 0) {
        goto out;
    }
    u = 0; /* keep the compiler happy */
    for (j = 0; j < num_nodes; j++) {
        u = nodes[j];
        for (v = left_child[u]; v != TSK_NULL; v = right_sib[v]) {
            num_tracked_samples[u] += num_tracked_samples[v];
        }
        num_tracked_samples[u] += flags[u] & TSK_NODE_IS_SAMPLE ? 1 : 0;
    }
    n = num_tracked_samples[u];
    u = parent[u];
    while (u != TSK_NULL) {
        num_tracked_samples[u] = n;
        u = parent[u];
    }
    num_tracked_samples[self->virtual_root] = n;
out:
    tsk_safe_free(nodes);
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_copy(const tsk_tree_t *self, tsk_tree_t *dest, tsk_flags_t options)
{
    int ret = TSK_ERR_GENERIC;
    tsk_size_t N = self->num_nodes + 1;

    if (!(options & TSK_NO_INIT)) {
        ret = tsk_tree_init(dest, self->tree_sequence, options);
        if (ret != 0) {
            goto out;
        }
    }
    if (self->tree_sequence != dest->tree_sequence) {
        ret = tsk_trace_error(TSK_ERR_BAD_PARAM_VALUE);
        goto out;
    }
    dest->interval = self->interval;
    dest->left_index = self->left_index;
    dest->right_index = self->right_index;
    dest->direction = self->direction;
    dest->index = self->index;
    dest->sites = self->sites;
    dest->sites_length = self->sites_length;
    dest->root_threshold = self->root_threshold;
    dest->num_edges = self->num_edges;
    dest->tree_pos = self->tree_pos;

    tsk_memcpy(dest->parent, self->parent, N * sizeof(*self->parent));
    tsk_memcpy(dest->left_child, self->left_child, N * sizeof(*self->left_child));
    tsk_memcpy(dest->right_child, self->right_child, N * sizeof(*self->right_child));
    tsk_memcpy(dest->left_sib, self->left_sib, N * sizeof(*self->left_sib));
    tsk_memcpy(dest->right_sib, self->right_sib, N * sizeof(*self->right_sib));
    tsk_memcpy(dest->num_children, self->num_children, N * sizeof(*self->num_children));
    tsk_memcpy(dest->edge, self->edge, N * sizeof(*self->edge));
    if (!(dest->options & TSK_NO_SAMPLE_COUNTS)) {
        if (self->options & TSK_NO_SAMPLE_COUNTS) {
            ret = tsk_trace_error(TSK_ERR_UNSUPPORTED_OPERATION);
            goto out;
        }
        tsk_memcpy(dest->num_samples, self->num_samples, N * sizeof(*self->num_samples));
        tsk_memcpy(dest->num_tracked_samples, self->num_tracked_samples,
            N * sizeof(*self->num_tracked_samples));
    }
    if (dest->options & TSK_SAMPLE_LISTS) {
        if (!(self->options & TSK_SAMPLE_LISTS)) {
            ret = tsk_trace_error(TSK_ERR_UNSUPPORTED_OPERATION);
            goto out;
        }
        tsk_memcpy(dest->left_sample, self->left_sample, N * sizeof(*self->left_sample));
        tsk_memcpy(
            dest->right_sample, self->right_sample, N * sizeof(*self->right_sample));
        tsk_memcpy(dest->next_sample, self->next_sample,
            self->tree_sequence->num_samples * sizeof(*self->next_sample));
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
    if (u < 0 || u > (tsk_id_t) self->num_nodes) {
        ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
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
    double tu, tv;
    const tsk_id_t *restrict parent = self->parent;
    const double *restrict time = self->tree_sequence->tables->nodes.time;

    ret = tsk_tree_check_node(self, u);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_tree_check_node(self, v);
    if (ret != 0) {
        goto out;
    }

    /* Simplest to make the virtual_root a special case here to avoid
     * doing the time lookup. */
    if (u == self->virtual_root || v == self->virtual_root) {
        *mrca = self->virtual_root;
        return 0;
    }

    tu = time[u];
    tv = time[v];
    while (u != v) {
        if (tu < tv) {
            u = parent[u];
            if (u == TSK_NULL) {
                break;
            }
            tu = time[u];
        } else {
            v = parent[v];
            if (v == TSK_NULL) {
                break;
            }
            tv = time[v];
        }
    }
    *mrca = u == v ? u : TSK_NULL;
out:
    return ret;
}

static int
tsk_tree_get_num_samples_by_traversal(
    const tsk_tree_t *self, tsk_id_t u, tsk_size_t *num_samples)
{
    int ret = 0;
    tsk_size_t num_nodes, j;
    tsk_size_t count = 0;
    const tsk_flags_t *restrict flags = self->tree_sequence->tables->nodes.flags;
    tsk_id_t *nodes = tsk_malloc(tsk_tree_get_size_bound(self) * sizeof(*nodes));
    tsk_id_t v;

    if (nodes == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    ret = tsk_tree_preorder_from(self, u, nodes, &num_nodes);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < num_nodes; j++) {
        v = nodes[j];
        if (flags[v] & TSK_NODE_IS_SAMPLE) {
            count++;
        }
    }
    *num_samples = count;
out:
    tsk_safe_free(nodes);
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_get_num_samples(const tsk_tree_t *self, tsk_id_t u, tsk_size_t *num_samples)
{
    int ret = 0;

    ret = tsk_tree_check_node(self, u);
    if (ret != 0) {
        goto out;
    }

    if (!(self->options & TSK_NO_SAMPLE_COUNTS)) {
        *num_samples = (tsk_size_t) self->num_samples[u];
    } else {
        ret = tsk_tree_get_num_samples_by_traversal(self, u, num_samples);
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_get_num_tracked_samples(
    const tsk_tree_t *self, tsk_id_t u, tsk_size_t *num_tracked_samples)
{
    int ret = 0;

    ret = tsk_tree_check_node(self, u);
    if (ret != 0) {
        goto out;
    }
    if (self->options & TSK_NO_SAMPLE_COUNTS) {
        ret = tsk_trace_error(TSK_ERR_UNSUPPORTED_OPERATION);
        goto out;
    }
    *num_tracked_samples = self->num_tracked_samples[u];
out:
    return ret;
}

bool
tsk_tree_is_sample(const tsk_tree_t *self, tsk_id_t u)
{
    return tsk_treeseq_is_sample(self->tree_sequence, u);
}

tsk_id_t
tsk_tree_get_left_root(const tsk_tree_t *self)
{
    return self->left_child[self->virtual_root];
}

tsk_id_t
tsk_tree_get_right_root(const tsk_tree_t *self)
{
    return self->right_child[self->virtual_root];
}

tsk_size_t
tsk_tree_get_num_roots(const tsk_tree_t *self)
{
    return (tsk_size_t) self->num_children[self->virtual_root];
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

    if (u == self->virtual_root) {
        *t = INFINITY;
    } else {
        ret = tsk_treeseq_get_node(self->tree_sequence, u, &node);
        if (ret != 0) {
            goto out;
        }
        *t = node.time;
    }
out:
    return ret;
}

static inline double
tsk_tree_get_branch_length_unsafe(const tsk_tree_t *self, tsk_id_t u)
{
    const double *times = self->tree_sequence->tables->nodes.time;
    const tsk_id_t parent = self->parent[u];

    return parent == TSK_NULL ? 0 : times[parent] - times[u];
}

int TSK_WARN_UNUSED
tsk_tree_get_branch_length(const tsk_tree_t *self, tsk_id_t u, double *ret_branch_length)
{
    int ret = 0;

    ret = tsk_tree_check_node(self, u);
    if (ret != 0) {
        goto out;
    }
    *ret_branch_length = tsk_tree_get_branch_length_unsafe(self, u);
out:
    return ret;
}

int
tsk_tree_get_total_branch_length(const tsk_tree_t *self, tsk_id_t node, double *ret_tbl)
{
    int ret = 0;
    tsk_size_t j, num_nodes;
    tsk_id_t u, v;
    const tsk_id_t *restrict parent = self->parent;
    const double *restrict time = self->tree_sequence->tables->nodes.time;
    tsk_id_t *nodes = tsk_malloc(tsk_tree_get_size_bound(self) * sizeof(*nodes));
    double sum = 0;

    if (nodes == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    ret = tsk_tree_preorder_from(self, node, nodes, &num_nodes);
    if (ret != 0) {
        goto out;
    }
    /* We always skip the first node because we don't return the branch length
     * over the input node. */
    for (j = 1; j < num_nodes; j++) {
        u = nodes[j];
        v = parent[u];
        if (v != TSK_NULL) {
            sum += time[v] - time[u];
        }
    }
    *ret_tbl = sum;
out:
    tsk_safe_free(nodes);
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
static int
tsk_tree_get_depth_unsafe(const tsk_tree_t *self, tsk_id_t u)
{
    tsk_id_t v;
    const tsk_id_t *restrict parent = self->parent;
    int depth = 0;

    if (u == self->virtual_root) {
        return -1;
    }
    for (v = parent[u]; v != TSK_NULL; v = parent[v]) {
        depth++;
    }
    return depth;
}

int TSK_WARN_UNUSED
tsk_tree_get_depth(const tsk_tree_t *self, tsk_id_t u, int *depth_ret)
{
    int ret = 0;

    ret = tsk_tree_check_node(self, u);
    if (ret != 0) {
        goto out;
    }

    *depth_ret = tsk_tree_get_depth_unsafe(self, u);
out:
    return ret;
}

static tsk_id_t
tsk_tree_node_root(tsk_tree_t *self, tsk_id_t u)
{
    tsk_id_t v = u;
    while (self->parent[v] != TSK_NULL) {
        v = self->parent[v];
    }

    return v;
}

static void
tsk_tree_check_state(const tsk_tree_t *self)
{
    tsk_id_t u, v;
    tsk_size_t j, num_samples;
    int err, c;
    tsk_site_t site;
    tsk_id_t *children = tsk_malloc(self->num_nodes * sizeof(tsk_id_t));
    bool *is_root = tsk_calloc(self->num_nodes, sizeof(bool));

    tsk_bug_assert(children != NULL);

    /* Check the virtual root properties */
    tsk_bug_assert(self->parent[self->virtual_root] == TSK_NULL);
    tsk_bug_assert(self->left_sib[self->virtual_root] == TSK_NULL);
    tsk_bug_assert(self->right_sib[self->virtual_root] == TSK_NULL);

    for (j = 0; j < self->tree_sequence->num_samples; j++) {
        u = self->samples[j];
        while (self->parent[u] != TSK_NULL) {
            u = self->parent[u];
        }
        is_root[u] = true;
    }
    if (self->tree_sequence->num_samples == 0) {
        tsk_bug_assert(self->left_child[self->virtual_root] == TSK_NULL);
    }

    /* Iterate over the roots and make sure they are set */
    for (u = tsk_tree_get_left_root(self); u != TSK_NULL; u = self->right_sib[u]) {
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
        tsk_bug_assert(self->interval.left <= site.position);
        tsk_bug_assert(site.position < self->interval.right);
    }

    if (!(self->options & TSK_NO_SAMPLE_COUNTS)) {
        tsk_bug_assert(self->num_samples != NULL);
        tsk_bug_assert(self->num_tracked_samples != NULL);
        for (u = 0; u < (tsk_id_t) self->num_nodes; u++) {
            err = tsk_tree_get_num_samples_by_traversal(self, u, &num_samples);
            tsk_bug_assert(err == 0);
            tsk_bug_assert(num_samples == (tsk_size_t) self->num_samples[u]);
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
    tsk_size_t j;
    tsk_site_t site;

    fprintf(out, "Tree state:\n");
    fprintf(out, "options = %d\n", self->options);
    fprintf(out, "root_threshold = %lld\n", (long long) self->root_threshold);
    fprintf(out, "left = %f\n", self->interval.left);
    fprintf(out, "right = %f\n", self->interval.right);
    fprintf(out, "index = %lld\n", (long long) self->index);
    fprintf(out, "num_edges = %d\n", (int) self->num_edges);
    fprintf(out, "node\tedge\tparent\tlchild\trchild\tlsib\trsib");
    if (self->options & TSK_SAMPLE_LISTS) {
        fprintf(out, "\thead\ttail");
    }
    fprintf(out, "\n");

    for (j = 0; j < self->num_nodes + 1; j++) {
        fprintf(out, "%lld\t%lld\t%lld\t%lld\t%lld\t%lld\t%lld", (long long) j,
            (long long) self->edge[j], (long long) self->parent[j],
            (long long) self->left_child[j], (long long) self->right_child[j],
            (long long) self->left_sib[j], (long long) self->right_sib[j]);
        if (self->options & TSK_SAMPLE_LISTS) {
            fprintf(out, "\t%lld\t%lld\t", (long long) self->left_sample[j],
                (long long) self->right_sample[j]);
        }
        if (!(self->options & TSK_NO_SAMPLE_COUNTS)) {
            fprintf(out, "\t%lld\t%lld", (long long) self->num_samples[j],
                (long long) self->num_tracked_samples[j]);
        }
        fprintf(out, "\n");
    }
    fprintf(out, "sites = \n");
    for (j = 0; j < self->sites_length; j++) {
        site = self->sites[j];
        fprintf(out, "\t%lld\t%f\n", (long long) site.id, site.position);
    }
    tsk_tree_check_state(self);
}

/* Methods for positioning the tree along the sequence */

/* The following methods are performance sensitive and so we use a
 * lot of restrict pointers. Because we are saying that we don't have
 * any aliases to these pointers, we pass around the reference to parent
 * since it's used in all the functions. */
static inline void
tsk_tree_update_sample_lists(
    tsk_tree_t *self, tsk_id_t node, const tsk_id_t *restrict parent)
{
    tsk_id_t u, v, sample_index;
    tsk_id_t *restrict left_child = self->left_child;
    tsk_id_t *restrict right_sib = self->right_sib;
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

static inline void
tsk_tree_remove_branch(
    tsk_tree_t *self, tsk_id_t p, tsk_id_t c, tsk_id_t *restrict parent)
{
    tsk_id_t *restrict left_child = self->left_child;
    tsk_id_t *restrict right_child = self->right_child;
    tsk_id_t *restrict left_sib = self->left_sib;
    tsk_id_t *restrict right_sib = self->right_sib;
    tsk_id_t *restrict num_children = self->num_children;
    tsk_id_t lsib = left_sib[c];
    tsk_id_t rsib = right_sib[c];

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
    num_children[p]--;
}

static inline void
tsk_tree_insert_branch(
    tsk_tree_t *self, tsk_id_t p, tsk_id_t c, tsk_id_t *restrict parent)
{
    tsk_id_t *restrict left_child = self->left_child;
    tsk_id_t *restrict right_child = self->right_child;
    tsk_id_t *restrict left_sib = self->left_sib;
    tsk_id_t *restrict right_sib = self->right_sib;
    tsk_id_t *restrict num_children = self->num_children;
    tsk_id_t u;

    parent[c] = p;
    u = right_child[p];
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
    num_children[p]++;
}

static inline void
tsk_tree_insert_root(tsk_tree_t *self, tsk_id_t root, tsk_id_t *restrict parent)
{
    tsk_tree_insert_branch(self, self->virtual_root, root, parent);
    parent[root] = TSK_NULL;
}

static inline void
tsk_tree_remove_root(tsk_tree_t *self, tsk_id_t root, tsk_id_t *restrict parent)
{
    tsk_tree_remove_branch(self, self->virtual_root, root, parent);
}

static void
tsk_tree_remove_edge(
    tsk_tree_t *self, tsk_id_t p, tsk_id_t c, tsk_id_t TSK_UNUSED(edge_id))
{
    tsk_id_t *restrict parent = self->parent;
    tsk_size_t *restrict num_samples = self->num_samples;
    tsk_size_t *restrict num_tracked_samples = self->num_tracked_samples;
    tsk_id_t *restrict edge = self->edge;
    const tsk_size_t root_threshold = self->root_threshold;
    tsk_id_t u;
    tsk_id_t path_end = TSK_NULL;
    bool path_end_was_root = false;

#define POTENTIAL_ROOT(U) (num_samples[U] >= root_threshold)

    tsk_tree_remove_branch(self, p, c, parent);
    self->num_edges--;
    edge[c] = TSK_NULL;

    if (!(self->options & TSK_NO_SAMPLE_COUNTS)) {
        u = p;
        while (u != TSK_NULL) {
            path_end = u;
            path_end_was_root = POTENTIAL_ROOT(u);
            num_samples[u] -= num_samples[c];
            num_tracked_samples[u] -= num_tracked_samples[c];
            u = parent[u];
        }

        if (path_end_was_root && !POTENTIAL_ROOT(path_end)) {
            tsk_tree_remove_root(self, path_end, parent);
        }
        if (POTENTIAL_ROOT(c)) {
            tsk_tree_insert_root(self, c, parent);
        }
    }

    if (self->options & TSK_SAMPLE_LISTS) {
        tsk_tree_update_sample_lists(self, p, parent);
    }
}

static void
tsk_tree_insert_edge(tsk_tree_t *self, tsk_id_t p, tsk_id_t c, tsk_id_t edge_id)
{
    tsk_id_t *restrict parent = self->parent;
    tsk_size_t *restrict num_samples = self->num_samples;
    tsk_size_t *restrict num_tracked_samples = self->num_tracked_samples;
    tsk_id_t *restrict edge = self->edge;
    const tsk_size_t root_threshold = self->root_threshold;
    tsk_id_t u;
    tsk_id_t path_end = TSK_NULL;
    bool path_end_was_root = false;

#define POTENTIAL_ROOT(U) (num_samples[U] >= root_threshold)

    if (!(self->options & TSK_NO_SAMPLE_COUNTS)) {
        u = p;
        while (u != TSK_NULL) {
            path_end = u;
            path_end_was_root = POTENTIAL_ROOT(u);
            num_samples[u] += num_samples[c];
            num_tracked_samples[u] += num_tracked_samples[c];
            u = parent[u];
        }

        if (POTENTIAL_ROOT(c)) {
            tsk_tree_remove_root(self, c, parent);
        }
        if (POTENTIAL_ROOT(path_end) && !path_end_was_root) {
            tsk_tree_insert_root(self, path_end, parent);
        }
    }

    tsk_tree_insert_branch(self, p, c, parent);
    self->num_edges++;
    edge[c] = edge_id;

    if (self->options & TSK_SAMPLE_LISTS) {
        tsk_tree_update_sample_lists(self, p, parent);
    }
}

int TSK_WARN_UNUSED
tsk_tree_first(tsk_tree_t *self)
{
    int ret = TSK_TREE_OK;

    ret = tsk_tree_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_tree_next(self);
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_last(tsk_tree_t *self)
{
    int ret = TSK_TREE_OK;

    ret = tsk_tree_clear(self);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_tree_prev(self);
out:
    return ret;
}

static void
tsk_tree_update_index_and_interval(tsk_tree_t *self)
{
    tsk_table_collection_t *tables = self->tree_sequence->tables;

    self->index = self->tree_pos.index;
    self->interval.left = self->tree_pos.interval.left;
    self->interval.right = self->tree_pos.interval.right;

    if (tables->sites.num_rows > 0) {
        self->sites = self->tree_sequence->tree_sites[self->index];
        self->sites_length = self->tree_sequence->tree_sites_length[self->index];
    }
}

int TSK_WARN_UNUSED
tsk_tree_next(tsk_tree_t *self)
{
    int ret = 0;
    tsk_table_collection_t *tables = self->tree_sequence->tables;
    const tsk_id_t *restrict edge_parent = tables->edges.parent;
    const tsk_id_t *restrict edge_child = tables->edges.child;
    tsk_id_t j, e;
    tsk_tree_position_t tree_pos;
    bool valid;

    valid = tsk_tree_position_next(&self->tree_pos);
    tree_pos = self->tree_pos;

    if (valid) {
        for (j = tree_pos.out.start; j != tree_pos.out.stop; j++) {
            e = tree_pos.out.order[j];
            tsk_tree_remove_edge(self, edge_parent[e], edge_child[e], e);
        }

        for (j = tree_pos.in.start; j != tree_pos.in.stop; j++) {
            e = tree_pos.in.order[j];
            tsk_tree_insert_edge(self, edge_parent[e], edge_child[e], e);
        }
        ret = TSK_TREE_OK;
        tsk_tree_update_index_and_interval(self);
    } else {
        ret = tsk_tree_clear(self);
    }
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_prev(tsk_tree_t *self)
{
    int ret = 0;
    tsk_table_collection_t *tables = self->tree_sequence->tables;
    const tsk_id_t *restrict edge_parent = tables->edges.parent;
    const tsk_id_t *restrict edge_child = tables->edges.child;
    tsk_id_t j, e;
    tsk_tree_position_t tree_pos;
    bool valid;

    valid = tsk_tree_position_prev(&self->tree_pos);
    tree_pos = self->tree_pos;

    if (valid) {
        for (j = tree_pos.out.start; j != tree_pos.out.stop; j--) {
            e = tree_pos.out.order[j];
            tsk_tree_remove_edge(self, edge_parent[e], edge_child[e], e);
        }

        for (j = tree_pos.in.start; j != tree_pos.in.stop; j--) {
            e = tree_pos.in.order[j];
            tsk_tree_insert_edge(self, edge_parent[e], edge_child[e], e);
        }
        ret = TSK_TREE_OK;
        tsk_tree_update_index_and_interval(self);
    } else {
        ret = tsk_tree_clear(self);
    }
    return ret;
}

static inline bool
tsk_tree_position_in_interval(const tsk_tree_t *self, double x)
{
    return self->interval.left <= x && x < self->interval.right;
}

static int
tsk_tree_seek_from_null(tsk_tree_t *self, double x, tsk_flags_t TSK_UNUSED(options))
{
    int ret = 0;
    tsk_table_collection_t *tables = self->tree_sequence->tables;
    const tsk_id_t *restrict edge_parent = tables->edges.parent;
    const tsk_id_t *restrict edge_child = tables->edges.child;
    const double *restrict edge_left = tables->edges.left;
    const double *restrict edge_right = tables->edges.right;
    double interval_left, interval_right;
    const double *restrict breakpoints = self->tree_sequence->breakpoints;
    const tsk_size_t num_trees = self->tree_sequence->num_trees;
    const double L = tsk_treeseq_get_sequence_length(self->tree_sequence);
    tsk_id_t j, e, index;
    tsk_tree_position_t tree_pos;

    index = (tsk_id_t) tsk_search_sorted(breakpoints, num_trees + 1, x);
    if (breakpoints[index] > x) {
        index--;
    }

    if (x <= L / 2.0) {
        ret = tsk_tree_position_seek_forward(&self->tree_pos, index);
        if (ret != 0) {
            goto out;
        }
        // Since we are seeking from null, there are no edges to remove
        tree_pos = self->tree_pos;
        interval_left = tree_pos.interval.left;
        for (j = tree_pos.in.start; j != tree_pos.in.stop; j++) {
            e = tree_pos.in.order[j];
            if (edge_left[e] <= interval_left && interval_left < edge_right[e]) {
                tsk_tree_insert_edge(self, edge_parent[e], edge_child[e], e);
            }
        }
    } else {
        ret = tsk_tree_position_seek_backward(&self->tree_pos, index);
        if (ret != 0) {
            goto out;
        }
        tree_pos = self->tree_pos;
        interval_right = tree_pos.interval.right;
        for (j = tree_pos.in.start; j != tree_pos.in.stop; j--) {
            e = tree_pos.in.order[j];
            if (edge_right[e] >= interval_right && interval_right > edge_left[e]) {
                tsk_tree_insert_edge(self, edge_parent[e], edge_child[e], e);
            }
        }
    }
    tsk_tree_update_index_and_interval(self);
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_tree_seek_forward(tsk_tree_t *self, tsk_id_t index)
{
    int ret = 0;
    tsk_table_collection_t *tables = self->tree_sequence->tables;
    const tsk_id_t *restrict edge_parent = tables->edges.parent;
    const tsk_id_t *restrict edge_child = tables->edges.child;
    const double *restrict edge_left = tables->edges.left;
    const double *restrict edge_right = tables->edges.right;
    double interval_left, e_left;
    const double old_right = self->interval.right;
    tsk_id_t j, e;
    tsk_tree_position_t tree_pos;

    ret = tsk_tree_position_seek_forward(&self->tree_pos, index);
    if (ret != 0) {
        goto out;
    }
    tree_pos = self->tree_pos;
    interval_left = tree_pos.interval.left;

    for (j = tree_pos.out.start; j != tree_pos.out.stop; j++) {
        e = tree_pos.out.order[j];
        e_left = edge_left[e];
        if (e_left < old_right) {
            tsk_bug_assert(edge_parent[e] != TSK_NULL);
            tsk_tree_remove_edge(self, edge_parent[e], edge_child[e], e);
        }
        tsk_bug_assert(e_left < interval_left);
    }

    for (j = tree_pos.in.start; j != tree_pos.in.stop; j++) {
        e = tree_pos.in.order[j];
        if (edge_left[e] <= interval_left && interval_left < edge_right[e]) {
            tsk_tree_insert_edge(self, edge_parent[e], edge_child[e], e);
        }
    }
    tsk_tree_update_index_and_interval(self);
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_tree_seek_backward(tsk_tree_t *self, tsk_id_t index)
{
    int ret = 0;
    tsk_table_collection_t *tables = self->tree_sequence->tables;
    const tsk_id_t *restrict edge_parent = tables->edges.parent;
    const tsk_id_t *restrict edge_child = tables->edges.child;
    const double *restrict edge_left = tables->edges.left;
    const double *restrict edge_right = tables->edges.right;
    double interval_right, e_right;
    const double old_right = self->interval.right;
    tsk_id_t j, e;
    tsk_tree_position_t tree_pos;

    ret = tsk_tree_position_seek_backward(&self->tree_pos, index);
    if (ret != 0) {
        goto out;
    }
    tree_pos = self->tree_pos;
    interval_right = tree_pos.interval.right;

    for (j = tree_pos.out.start; j != tree_pos.out.stop; j--) {
        e = tree_pos.out.order[j];
        e_right = edge_right[e];
        if (e_right >= old_right) {
            tsk_bug_assert(edge_parent[e] != TSK_NULL);
            tsk_tree_remove_edge(self, edge_parent[e], edge_child[e], e);
        }
        tsk_bug_assert(e_right > interval_right);
    }

    for (j = tree_pos.in.start; j != tree_pos.in.stop; j--) {
        e = tree_pos.in.order[j];
        if (edge_right[e] >= interval_right && interval_right > edge_left[e]) {
            tsk_tree_insert_edge(self, edge_parent[e], edge_child[e], e);
        }
    }
    tsk_tree_update_index_and_interval(self);
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_seek_index(tsk_tree_t *self, tsk_id_t tree, tsk_flags_t options)
{
    int ret = 0;
    double x;

    if (tree < 0 || tree >= (tsk_id_t) self->tree_sequence->num_trees) {
        ret = tsk_trace_error(TSK_ERR_SEEK_OUT_OF_BOUNDS);
        goto out;
    }
    x = self->tree_sequence->breakpoints[tree];
    ret = tsk_tree_seek(self, x, options);
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_tree_seek_linear(tsk_tree_t *self, double x)
{
    const double L = tsk_treeseq_get_sequence_length(self->tree_sequence);
    const double t_l = self->interval.left;
    const double t_r = self->interval.right;
    int ret = 0;
    double distance_left, distance_right;

    if (x < t_l) {
        /* |-----|-----|========|---------| */
        /* 0     x    t_l      t_r        L */
        distance_left = t_l - x;
        distance_right = L - t_r + x;
    } else {
        /* |------|========|------|-------| */
        /* 0     t_l      t_r     x       L */
        distance_right = x - t_r;
        distance_left = t_l + L - x;
    }
    if (distance_right <= distance_left) {
        while (!tsk_tree_position_in_interval(self, x)) {
            ret = tsk_tree_next(self);
            if (ret < 0) {
                goto out;
            }
        }
    } else {
        while (!tsk_tree_position_in_interval(self, x)) {
            ret = tsk_tree_prev(self);
            if (ret < 0) {
                goto out;
            }
        }
    }
    ret = 0;
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_tree_seek_skip(tsk_tree_t *self, double x)
{
    const double t_l = self->interval.left;
    int ret = 0;
    tsk_id_t index;
    const tsk_size_t num_trees = self->tree_sequence->num_trees;
    const double *restrict breakpoints = self->tree_sequence->breakpoints;

    index = (tsk_id_t) tsk_search_sorted(breakpoints, num_trees + 1, x);
    if (breakpoints[index] > x) {
        index--;
    }

    if (x < t_l) {
        ret = tsk_tree_seek_backward(self, index);
    } else {
        ret = tsk_tree_seek_forward(self, index);
    }
    tsk_bug_assert(tsk_tree_position_in_interval(self, x));
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_seek(tsk_tree_t *self, double x, tsk_flags_t options)
{
    int ret = 0;
    const double L = tsk_treeseq_get_sequence_length(self->tree_sequence);

    if (x < 0 || x >= L) {
        ret = tsk_trace_error(TSK_ERR_SEEK_OUT_OF_BOUNDS);
        goto out;
    }

    if (self->index == -1) {
        ret = tsk_tree_seek_from_null(self, x, options);
    } else {
        if (options & TSK_SEEK_SKIP) {
            ret = tsk_tree_seek_skip(self, x);
        } else {
            ret = tsk_tree_seek_linear(self, x);
        }
    }

out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_clear(tsk_tree_t *self)
{
    int ret = 0;
    tsk_size_t j;
    tsk_id_t u;
    const tsk_size_t N = self->num_nodes + 1;
    const tsk_size_t num_samples = self->tree_sequence->num_samples;
    const bool sample_counts = !(self->options & TSK_NO_SAMPLE_COUNTS);
    const bool sample_lists = !!(self->options & TSK_SAMPLE_LISTS);
    const tsk_flags_t *flags = self->tree_sequence->tables->nodes.flags;

    self->interval.left = 0;
    self->interval.right = 0;
    self->num_edges = 0;
    self->index = -1;
    tsk_tree_position_set_null(&self->tree_pos);
    /* TODO we should profile this method to see if just doing a single loop over
     * the nodes would be more efficient than multiple memsets.
     */
    tsk_memset(self->parent, 0xff, N * sizeof(*self->parent));
    tsk_memset(self->left_child, 0xff, N * sizeof(*self->left_child));
    tsk_memset(self->right_child, 0xff, N * sizeof(*self->right_child));
    tsk_memset(self->left_sib, 0xff, N * sizeof(*self->left_sib));
    tsk_memset(self->right_sib, 0xff, N * sizeof(*self->right_sib));
    tsk_memset(self->num_children, 0, N * sizeof(*self->num_children));
    tsk_memset(self->edge, 0xff, N * sizeof(*self->edge));

    if (sample_counts) {
        tsk_memset(self->num_samples, 0, N * sizeof(*self->num_samples));
        /* We can't reset the tracked samples via memset because we don't
         * know where the tracked samples are.
         */
        for (j = 0; j < self->num_nodes; j++) {
            if (!(flags[j] & TSK_NODE_IS_SAMPLE)) {
                self->num_tracked_samples[j] = 0;
            }
        }
        /* The total tracked_samples gets set in set_tracked_samples */
        self->num_samples[self->virtual_root] = num_samples;
    }
    if (sample_lists) {
        tsk_memset(self->left_sample, 0xff, N * sizeof(tsk_id_t));
        tsk_memset(self->right_sample, 0xff, N * sizeof(tsk_id_t));
        tsk_memset(self->next_sample, 0xff, num_samples * sizeof(tsk_id_t));
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
    if (sample_counts && self->root_threshold == 1 && num_samples > 0) {
        for (j = 0; j < num_samples; j++) {
            /* Set initial roots */
            if (self->root_threshold == 1) {
                tsk_tree_insert_root(self, self->samples[j], self->parent);
            }
        }
    }
    return ret;
}

tsk_size_t
tsk_tree_get_size_bound(const tsk_tree_t *self)
{
    tsk_size_t bound = 0;

    if (self->tree_sequence != NULL) {
        /* This is a safe upper bound which can be computed cheaply.
         * We have at most n roots and each edge adds at most one new
         * node to the tree. We also allow space for the virtual root,
         * to simplify client code.
         *
         * In the common case of a binary tree with a single root, we have
         * 2n - 1 nodes in total, and 2n - 2 edges. Therefore, we return
         * 3n - 1, which is an over-estimate of 1/2 and we allocate
         * 1.5 times as much memory as we need.
         *
         * Since tracking the exact number of nodes in the tree would require
         * storing the number of nodes beneath every node and complicate
         * the tree transition method, this seems like a good compromise
         * and will result in less memory usage overall in nearly all cases.
         */
        bound = 1 + self->tree_sequence->num_samples + self->num_edges;
    }
    return bound;
}

/* Traversal orders */
static tsk_id_t *
tsk_tree_alloc_node_stack(const tsk_tree_t *self)
{
    return tsk_malloc(tsk_tree_get_size_bound(self) * sizeof(tsk_id_t));
}

int
tsk_tree_preorder(const tsk_tree_t *self, tsk_id_t *nodes, tsk_size_t *num_nodes_ret)
{
    return tsk_tree_preorder_from(self, -1, nodes, num_nodes_ret);
}

int
tsk_tree_preorder_from(
    const tsk_tree_t *self, tsk_id_t root, tsk_id_t *nodes, tsk_size_t *num_nodes_ret)
{
    int ret = 0;
    const tsk_id_t *restrict right_child = self->right_child;
    const tsk_id_t *restrict left_sib = self->left_sib;
    tsk_id_t *stack = tsk_tree_alloc_node_stack(self);
    tsk_size_t num_nodes = 0;
    tsk_id_t u, v;
    int stack_top;

    if (stack == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    if ((root == -1 || root == self->virtual_root)
        && !tsk_tree_has_sample_counts(self)) {
        ret = tsk_trace_error(TSK_ERR_UNSUPPORTED_OPERATION);
        goto out;
    }
    if (root == -1) {
        stack_top = -1;
        for (u = right_child[self->virtual_root]; u != TSK_NULL; u = left_sib[u]) {
            stack_top++;
            stack[stack_top] = u;
        }
    } else {
        ret = tsk_tree_check_node(self, root);
        if (ret != 0) {
            goto out;
        }
        stack_top = 0;
        stack[stack_top] = root;
    }

    while (stack_top >= 0) {
        u = stack[stack_top];
        stack_top--;
        nodes[num_nodes] = u;
        num_nodes++;
        for (v = right_child[u]; v != TSK_NULL; v = left_sib[v]) {
            stack_top++;
            stack[stack_top] = v;
        }
    }
    *num_nodes_ret = num_nodes;
out:
    tsk_safe_free(stack);
    return ret;
}

/* We could implement this using the preorder function, but since it's
 * going to be performance critical we want to avoid the overhead
 * of mallocing the intermediate node list (which will be bigger than
 * the number of samples). */
int
tsk_tree_preorder_samples_from(
    const tsk_tree_t *self, tsk_id_t root, tsk_id_t *nodes, tsk_size_t *num_nodes_ret)
{
    int ret = 0;
    const tsk_id_t *restrict right_child = self->right_child;
    const tsk_id_t *restrict left_sib = self->left_sib;
    const tsk_flags_t *restrict flags = self->tree_sequence->tables->nodes.flags;
    tsk_id_t *stack = tsk_tree_alloc_node_stack(self);
    tsk_size_t num_nodes = 0;
    tsk_id_t u, v;
    int stack_top;

    if (stack == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    /* We could push the virtual_root onto the stack directly to simplify
     * the code a little, but then we'd have to check put an extra check
     * when looking up the flags array (which isn't defined for virtual_root).
     */
    if (root == -1 || root == self->virtual_root) {
        if (!tsk_tree_has_sample_counts(self)) {
            ret = tsk_trace_error(TSK_ERR_UNSUPPORTED_OPERATION);
            goto out;
        }
        stack_top = -1;
        for (u = right_child[self->virtual_root]; u != TSK_NULL; u = left_sib[u]) {
            stack_top++;
            stack[stack_top] = u;
        }
    } else {
        ret = tsk_tree_check_node(self, root);
        if (ret != 0) {
            goto out;
        }
        stack_top = 0;
        stack[stack_top] = root;
    }

    while (stack_top >= 0) {
        u = stack[stack_top];
        stack_top--;
        if (flags[u] & TSK_NODE_IS_SAMPLE) {
            nodes[num_nodes] = u;
            num_nodes++;
        }
        for (v = right_child[u]; v != TSK_NULL; v = left_sib[v]) {
            stack_top++;
            stack[stack_top] = v;
        }
    }
    *num_nodes_ret = num_nodes;
out:
    tsk_safe_free(stack);
    return ret;
}

int
tsk_tree_postorder(const tsk_tree_t *self, tsk_id_t *nodes, tsk_size_t *num_nodes_ret)
{
    return tsk_tree_postorder_from(self, -1, nodes, num_nodes_ret);
}
int
tsk_tree_postorder_from(
    const tsk_tree_t *self, tsk_id_t root, tsk_id_t *nodes, tsk_size_t *num_nodes_ret)
{
    int ret = 0;
    const tsk_id_t *restrict right_child = self->right_child;
    const tsk_id_t *restrict left_sib = self->left_sib;
    const tsk_id_t *restrict parent = self->parent;
    tsk_id_t *stack = tsk_tree_alloc_node_stack(self);
    tsk_size_t num_nodes = 0;
    tsk_id_t u, v, postorder_parent;
    int stack_top;
    bool is_virtual_root = root == self->virtual_root;

    if (stack == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    if (root == -1 || is_virtual_root) {
        if (!tsk_tree_has_sample_counts(self)) {
            ret = tsk_trace_error(TSK_ERR_UNSUPPORTED_OPERATION);
            goto out;
        }
        stack_top = -1;
        for (u = right_child[self->virtual_root]; u != TSK_NULL; u = left_sib[u]) {
            stack_top++;
            stack[stack_top] = u;
        }
    } else {
        ret = tsk_tree_check_node(self, root);
        if (ret != 0) {
            goto out;
        }
        stack_top = 0;
        stack[stack_top] = root;
    }

    postorder_parent = TSK_NULL;
    while (stack_top >= 0) {
        u = stack[stack_top];
        if (right_child[u] != TSK_NULL && u != postorder_parent) {
            for (v = right_child[u]; v != TSK_NULL; v = left_sib[v]) {
                stack_top++;
                stack[stack_top] = v;
            }
        } else {
            stack_top--;
            postorder_parent = parent[u];
            nodes[num_nodes] = u;
            num_nodes++;
        }
    }
    if (is_virtual_root) {
        nodes[num_nodes] = root;
        num_nodes++;
    }
    *num_nodes_ret = num_nodes;
out:
    tsk_safe_free(stack);
    return ret;
}

/* Balance/imbalance metrics */

/* Result is a tsk_size_t value here because we could imagine the total
 * depth overflowing a 32bit integer for a large tree. */
int
tsk_tree_sackin_index(const tsk_tree_t *self, tsk_size_t *result)
{
    /* Keep the size of the stack elements to 8 bytes in total in the
     * standard case. A tsk_id_t depth value is always safe, since
     * depth counts the number of nodes encountered on a path.
     */
    struct stack_elem {
        tsk_id_t node;
        tsk_id_t depth;
    };
    int ret = 0;
    const tsk_id_t *restrict right_child = self->right_child;
    const tsk_id_t *restrict left_sib = self->left_sib;
    struct stack_elem *stack
        = tsk_malloc(tsk_tree_get_size_bound(self) * sizeof(*stack));
    int stack_top;
    tsk_size_t total_depth;
    tsk_id_t u;
    struct stack_elem s = { .node = TSK_NULL, .depth = 0 };

    if (stack == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    stack_top = -1;
    for (u = right_child[self->virtual_root]; u != TSK_NULL; u = left_sib[u]) {
        stack_top++;
        s.node = u;
        stack[stack_top] = s;
    }
    total_depth = 0;
    while (stack_top >= 0) {
        s = stack[stack_top];
        stack_top--;
        u = right_child[s.node];
        if (u == TSK_NULL) {
            total_depth += (tsk_size_t) s.depth;
        } else {
            s.depth++;
            while (u != TSK_NULL) {
                stack_top++;
                s.node = u;
                stack[stack_top] = s;
                u = left_sib[u];
            }
        }
    }
    *result = total_depth;
out:
    tsk_safe_free(stack);
    return ret;
}

int
tsk_tree_colless_index(const tsk_tree_t *self, tsk_size_t *result)
{
    int ret = 0;
    const tsk_id_t *restrict right_child = self->right_child;
    const tsk_id_t *restrict left_sib = self->left_sib;
    tsk_id_t *nodes = tsk_malloc(tsk_tree_get_size_bound(self) * sizeof(*nodes));
    tsk_id_t *num_leaves = tsk_calloc(self->num_nodes, sizeof(*num_leaves));
    tsk_size_t j, num_nodes, total;
    tsk_id_t num_children, u, v;

    if (nodes == NULL || num_leaves == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    if (tsk_tree_get_num_roots(self) != 1) {
        ret = tsk_trace_error(TSK_ERR_UNDEFINED_MULTIROOT);
        goto out;
    }
    ret = tsk_tree_postorder(self, nodes, &num_nodes);
    if (ret != 0) {
        goto out;
    }

    total = 0;
    for (j = 0; j < num_nodes; j++) {
        u = nodes[j];
        /* Cheaper to compute this on the fly than to access the num_children array.
         * since we're already iterating over the children. */
        num_children = 0;
        for (v = right_child[u]; v != TSK_NULL; v = left_sib[v]) {
            num_children++;
            num_leaves[u] += num_leaves[v];
        }
        if (num_children == 0) {
            num_leaves[u] = 1;
        } else if (num_children == 2) {
            v = right_child[u];
            total += (tsk_size_t) llabs(num_leaves[v] - num_leaves[left_sib[v]]);
        } else {
            ret = tsk_trace_error(TSK_ERR_UNDEFINED_NONBINARY);
            goto out;
        }
    }
    *result = total;
out:
    tsk_safe_free(nodes);
    tsk_safe_free(num_leaves);
    return ret;
}

int
tsk_tree_b1_index(const tsk_tree_t *self, double *result)
{
    int ret = 0;
    const tsk_id_t *restrict parent = self->parent;
    const tsk_id_t *restrict right_child = self->right_child;
    const tsk_id_t *restrict left_sib = self->left_sib;
    tsk_id_t *nodes = tsk_malloc(tsk_tree_get_size_bound(self) * sizeof(*nodes));
    tsk_size_t *max_path_length = tsk_calloc(self->num_nodes, sizeof(*max_path_length));
    tsk_size_t j, num_nodes, mpl;
    double total = 0.0;
    tsk_id_t u, v;

    if (nodes == NULL || max_path_length == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    ret = tsk_tree_postorder(self, nodes, &num_nodes);
    if (ret != 0) {
        goto out;
    }

    for (j = 0; j < num_nodes; j++) {
        u = nodes[j];
        if (parent[u] != TSK_NULL && right_child[u] != TSK_NULL) {
            mpl = 0;
            for (v = right_child[u]; v != TSK_NULL; v = left_sib[v]) {
                mpl = TSK_MAX(mpl, max_path_length[v]);
            }
            max_path_length[u] = mpl + 1;
            total += 1 / (double) max_path_length[u];
        }
    }
    *result = total;
out:
    tsk_safe_free(nodes);
    tsk_safe_free(max_path_length);
    return ret;
}

static double
general_log(double x, double base)
{
    return log(x) / log(base);
}

int
tsk_tree_b2_index(const tsk_tree_t *self, double base, double *result)
{
    struct stack_elem {
        tsk_id_t node;
        double path_product;
    };
    int ret = 0;
    const tsk_id_t *restrict right_child = self->right_child;
    const tsk_id_t *restrict left_sib = self->left_sib;
    struct stack_elem *stack
        = tsk_malloc(tsk_tree_get_size_bound(self) * sizeof(*stack));
    int stack_top;
    double total_proba = 0;
    double num_children;
    tsk_id_t u;
    struct stack_elem s = { .node = TSK_NULL, .path_product = 1 };

    if (stack == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    if (tsk_tree_get_num_roots(self) != 1) {
        ret = tsk_trace_error(TSK_ERR_UNDEFINED_MULTIROOT);
        goto out;
    }

    stack_top = 0;
    s.node = tsk_tree_get_left_root(self);
    stack[stack_top] = s;

    while (stack_top >= 0) {
        s = stack[stack_top];
        stack_top--;
        u = right_child[s.node];
        if (u == TSK_NULL) {
            total_proba -= s.path_product * general_log(s.path_product, base);
        } else {
            num_children = 0;
            for (; u != TSK_NULL; u = left_sib[u]) {
                num_children++;
            }
            s.path_product *= 1 / num_children;
            for (u = right_child[s.node]; u != TSK_NULL; u = left_sib[u]) {
                stack_top++;
                s.node = u;
                stack[stack_top] = s;
            }
        }
    }
    *result = total_proba;
out:
    tsk_safe_free(stack);
    return ret;
}

int
tsk_tree_num_lineages(const tsk_tree_t *self, double t, tsk_size_t *result)
{
    int ret = 0;
    const tsk_id_t *restrict right_child = self->right_child;
    const tsk_id_t *restrict left_sib = self->left_sib;
    const double *restrict time = self->tree_sequence->tables->nodes.time;
    tsk_id_t *stack = tsk_tree_alloc_node_stack(self);
    tsk_size_t num_lineages = 0;
    int stack_top;
    tsk_id_t u, v;
    double child_time, parent_time;

    if (stack == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    if (!tsk_isfinite(t)) {
        ret = tsk_trace_error(TSK_ERR_TIME_NONFINITE);
        goto out;
    }
    /* Push the roots onto the stack */
    stack_top = -1;
    for (u = right_child[self->virtual_root]; u != TSK_NULL; u = left_sib[u]) {
        stack_top++;
        stack[stack_top] = u;
    }

    while (stack_top >= 0) {
        u = stack[stack_top];
        parent_time = time[u];
        stack_top--;
        for (v = right_child[u]; v != TSK_NULL; v = left_sib[v]) {
            child_time = time[v];
            /* Only traverse down the tree as far as we need to */
            if (child_time > t) {
                stack_top++;
                stack[stack_top] = v;
            } else if (t < parent_time) {
                num_lineages++;
            }
        }
    }
    *result = num_lineages;
out:
    tsk_safe_free(stack);
    return ret;
}

/* Parsimony methods */

static inline uint64_t
set_bit(uint64_t value, int32_t bit)
{
    return value | (1ULL << bit);
}

static inline bool
bit_is_set(uint64_t value, int32_t bit)
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
 * We should also vectorise the function so that several sites can be processed
 * at once.
 *
 * The algorithm used here is Hartigan parsimony, "Minimum Mutation Fits to a
 * Given Tree", Biometrics 1973.
 */
int TSK_WARN_UNUSED
tsk_tree_map_mutations(tsk_tree_t *self, int32_t *genotypes,
    double *TSK_UNUSED(cost_matrix), tsk_flags_t options, int32_t *r_ancestral_state,
    tsk_size_t *r_num_transitions, tsk_state_transition_t **r_transitions)
{
    int ret = 0;
    struct stack_elem {
        tsk_id_t node;
        tsk_id_t transition_parent;
        int32_t state;
    };
    const tsk_size_t num_samples = self->tree_sequence->num_samples;
    const tsk_id_t *restrict left_child = self->left_child;
    const tsk_id_t *restrict right_sib = self->right_sib;
    const tsk_size_t N = tsk_treeseq_get_num_nodes(self->tree_sequence);
    const tsk_flags_t *restrict node_flags = self->tree_sequence->tables->nodes.flags;
    tsk_id_t *nodes = tsk_malloc(tsk_tree_get_size_bound(self) * sizeof(*nodes));
    /* Note: to use less memory here and to improve cache performance we should
     * probably change to allocating exactly the number of nodes returned by
     * a preorder traversal, and then lay the memory out in this order. So, we'd
     * need a map from node ID to its index in the preorder traversal, but this
     * is trivial to compute. Probably doesn't matter so much at the moment
     * when we're doing a single site, but it would make a big difference if
     * we were vectorising over lots of sites. */
    uint64_t *restrict optimal_set = tsk_calloc(N + 1, sizeof(*optimal_set));
    struct stack_elem *restrict preorder_stack
        = tsk_malloc(tsk_tree_get_size_bound(self) * sizeof(*preorder_stack));
    tsk_id_t u, v;
    /* The largest possible number of transitions is one over every sample */
    tsk_state_transition_t *transitions = tsk_malloc(num_samples * sizeof(*transitions));
    int32_t allele, ancestral_state;
    int stack_top;
    struct stack_elem s;
    tsk_size_t j, num_transitions, max_allele_count, num_nodes;
    tsk_size_t allele_count[HARTIGAN_MAX_ALLELES];
    tsk_size_t non_missing = 0;
    int32_t num_alleles = 0;

    if (optimal_set == NULL || preorder_stack == NULL || transitions == NULL
        || nodes == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    for (j = 0; j < num_samples; j++) {
        if (genotypes[j] >= HARTIGAN_MAX_ALLELES || genotypes[j] < TSK_MISSING_DATA) {
            ret = tsk_trace_error(TSK_ERR_BAD_GENOTYPE);
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
        ret = tsk_trace_error(TSK_ERR_GENOTYPES_ALL_MISSING);
        goto out;
    }
    num_alleles++;

    ancestral_state = 0; /* keep compiler happy */
    if (options & TSK_MM_FIXED_ANCESTRAL_STATE) {
        ancestral_state = *r_ancestral_state;
        if ((ancestral_state < 0) || (ancestral_state >= HARTIGAN_MAX_ALLELES)) {
            ret = tsk_trace_error(TSK_ERR_BAD_ANCESTRAL_STATE);
            goto out;
        } else if (ancestral_state >= num_alleles) {
            num_alleles = (int32_t)(ancestral_state + 1);
        }
    }

    ret = tsk_tree_postorder_from(self, self->virtual_root, nodes, &num_nodes);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < num_nodes; j++) {
        u = nodes[j];
        tsk_memset(allele_count, 0, ((size_t) num_alleles) * sizeof(*allele_count));
        for (v = left_child[u]; v != TSK_NULL; v = right_sib[v]) {
            for (allele = 0; allele < num_alleles; allele++) {
                allele_count[allele] += bit_is_set(optimal_set[v], allele);
            }
        }
        /* the virtual root has no flags defined */
        if (u == (tsk_id_t) N || !(node_flags[u] & TSK_NODE_IS_SAMPLE)) {
            max_allele_count = 0;
            for (allele = 0; allele < num_alleles; allele++) {
                max_allele_count = TSK_MAX(max_allele_count, allele_count[allele]);
            }
            for (allele = 0; allele < num_alleles; allele++) {
                if (allele_count[allele] == max_allele_count) {
                    optimal_set[u] = set_bit(optimal_set[u], allele);
                }
            }
        }
    }
    if (!(options & TSK_MM_FIXED_ANCESTRAL_STATE)) {
        ancestral_state = get_smallest_set_bit(optimal_set[self->virtual_root]);
    } else {
        optimal_set[self->virtual_root] = UINT64_MAX;
    }

    num_transitions = 0;

    /* Do a preorder traversal */
    preorder_stack[0].node = self->virtual_root;
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
    if (nodes != NULL) {
        free(nodes);
    }
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
    self->m = tsk_calloc((size_t)(self->N + self->n), sizeof(*self->m));
    self->M = tsk_calloc((size_t)(self->N + self->n), sizeof(*self->M));
    if (self->m == NULL || self->M == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
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

    stack = tsk_malloc(tsk_tree_get_size_bound(t) * sizeof(*stack));
    if (stack == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    times = t->tree_sequence->tables->nodes.time;

    for (root = tsk_tree_get_left_root(t); root != TSK_NULL; root = t->right_sib[root]) {
        stack_top = 0;
        stack[stack_top].node = root;
        stack[stack_top].depth = 0;
        while (stack_top >= 0) {
            u = stack[stack_top].node;
            depth = stack[stack_top].depth;
            stack_top--;

            if (tsk_tree_is_sample(t, u)) {
                time = tsk_tree_get_branch_length_unsafe(t, u);
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
        vT1 = ((double) self->m[i] * (1 - lambda)) + (lambda * self->M[i]);
        vT2 = ((double) other->m[i] * (1 - lambda)) + (lambda * other->M[i]);
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
        ret = tsk_trace_error(TSK_ERR_MULTIPLE_ROOTS);
        goto out;
    }
    if (!tsk_tree_has_sample_lists(self)) {
        ret = tsk_trace_error(TSK_ERR_NO_SAMPLE_LISTS);
        goto out;
    }

    num_nodes = (tsk_id_t) tsk_treeseq_get_num_nodes(self->tree_sequence);
    for (u = 0; u < num_nodes; u++) {
        left_child = self->left_child[u];
        if (left_child != TSK_NULL && left_child == self->right_child[u]) {
            ret = tsk_trace_error(TSK_ERR_UNARY_NODES);
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
        ret = tsk_trace_error(TSK_ERR_SAMPLE_SIZE_MISMATCH);
        goto out;
    }

    samples = self->samples;
    other_samples = other->samples;
    n = (tsk_id_t) self->num_samples;
    for (i = 0; i < n; i++) {
        if (samples[i] != other_samples[i]) {
            ret = tsk_trace_error(TSK_ERR_SAMPLES_NOT_EQUAL);
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
        tsk_memset(&vecs[i], 0, sizeof(kc_vectors));
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
        ret = tsk_trace_error(TSK_ERR_SEQUENCE_LENGTH_MISMATCH);
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

    stack = tsk_malloc(tsk_tree_get_size_bound(t) * sizeof(*stack));
    if (stack == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
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
update_kc_incremental(tsk_tree_t *tree, kc_vectors *kc, tsk_size_t *depths)
{
    int ret = 0;
    tsk_id_t u, v, e, j;
    double root_time, time;
    const double *restrict times = tree->tree_sequence->tables->nodes.time;
    const tsk_id_t *restrict edges_child = tree->tree_sequence->tables->edges.child;
    const tsk_id_t *restrict edges_parent = tree->tree_sequence->tables->edges.parent;
    tsk_tree_position_t tree_pos = tree->tree_pos;

    /* Update state of detached subtrees */
    for (j = tree_pos.out.stop - 1; j >= tree_pos.out.start; j--) {
        e = tree_pos.out.order[j];
        u = edges_child[e];
        depths[u] = 0;

        if (tree->parent[u] == TSK_NULL) {
            root_time = times[tsk_tree_node_root(tree, u)];
            ret = update_kc_subtree_state(tree, kc, u, depths, root_time);
            if (ret != 0) {
                goto out;
            }
        }
    }

    /* Propagate state change down into reattached subtrees. */
    for (j = tree_pos.in.stop - 1; j >= tree_pos.in.start; j--) {
        e = tree_pos.in.order[j];
        u = edges_child[e];
        v = edges_parent[e];

        tsk_bug_assert(depths[u] == 0);
        depths[u] = depths[v] + 1;

        root_time = times[tsk_tree_node_root(tree, u)];
        ret = update_kc_subtree_state(tree, kc, u, depths, root_time);
        if (ret != 0) {
            goto out;
        }

        if (tsk_tree_is_sample(tree, u)) {
            time = tsk_tree_get_branch_length_unsafe(tree, u);
            update_kc_vectors_single_sample(tree->tree_sequence, kc, u, time);
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
    tsk_size_t *depths[2];
    int ret = 0;

    for (i = 0; i < 2; i++) {
        tsk_memset(&trees[i], 0, sizeof(trees[i]));
        tsk_memset(&kcs[i], 0, sizeof(kcs[i]));
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
        ret = kc_vectors_alloc(&kcs[i], n);
        if (ret != 0) {
            goto out;
        }
        num_nodes = tsk_treeseq_get_num_nodes(treeseqs[i]);
        depths[i] = tsk_calloc(num_nodes, sizeof(*depths[i]));
        if (depths[i] == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
    }

    total = 0;
    left = 0;

    ret = tsk_tree_first(&trees[0]);
    if (ret != TSK_TREE_OK) {
        goto out;
    }
    ret = check_kc_distance_tree_inputs(&trees[0]);
    if (ret != 0) {
        goto out;
    }

    ret = update_kc_incremental(&trees[0], &kcs[0], depths[0]);
    if (ret != 0) {
        goto out;
    }
    while ((ret = tsk_tree_next(&trees[1])) == TSK_TREE_OK) {
        ret = check_kc_distance_tree_inputs(&trees[1]);
        if (ret != 0) {
            goto out;
        }

        ret = update_kc_incremental(&trees[1], &kcs[1], depths[1]);
        if (ret != 0) {
            goto out;
        }
        while (trees[0].interval.right < trees[1].interval.right) {
            span = trees[0].interval.right - left;
            total += norm_kc_vectors(&kcs[0], &kcs[1], lambda_) * span;

            left = trees[0].interval.right;
            ret = tsk_tree_next(&trees[0]);
            tsk_bug_assert(ret == TSK_TREE_OK);
            ret = check_kc_distance_tree_inputs(&trees[0]);
            if (ret != 0) {
                goto out;
            }
            ret = update_kc_incremental(&trees[0], &kcs[0], depths[0]);
            if (ret != 0) {
                goto out;
            }
        }
        span = trees[1].interval.right - left;
        left = trees[1].interval.right;
        total += norm_kc_vectors(&kcs[0], &kcs[1], lambda_) * span;
    }
    if (ret != 0) {
        goto out;
    }

    *result = total / self->tables->sequence_length;
out:
    for (i = 0; i < 2; i++) {
        tsk_tree_free(&trees[i]);
        kc_vectors_free(&kcs[i]);
        tsk_safe_free(depths[i]);
    }
    return ret;
}

/*
 * Divergence matrix
 */

typedef struct {
    /* Note it's a waste storing the triply linked tree here, but the code
     * is written on the assumption of 1-based trees and the algorithm is
     * frighteningly subtle, so it doesn't seem worth messing with it
     * unless we really need to save some memory */
    tsk_id_t *parent;
    tsk_id_t *child;
    tsk_id_t *sib;
    tsk_id_t *lambda;
    tsk_id_t *pi;
    tsk_id_t *tau;
    tsk_id_t *beta;
    tsk_id_t *alpha;
} sv_tables_t;

static int
sv_tables_init(sv_tables_t *self, tsk_size_t n)
{
    int ret = 0;

    self->parent = tsk_malloc(n * sizeof(*self->parent));
    self->child = tsk_malloc(n * sizeof(*self->child));
    self->sib = tsk_malloc(n * sizeof(*self->sib));
    self->pi = tsk_malloc(n * sizeof(*self->pi));
    self->lambda = tsk_malloc(n * sizeof(*self->lambda));
    self->tau = tsk_malloc(n * sizeof(*self->tau));
    self->beta = tsk_malloc(n * sizeof(*self->beta));
    self->alpha = tsk_malloc(n * sizeof(*self->alpha));
    if (self->parent == NULL || self->child == NULL || self->sib == NULL
        || self->lambda == NULL || self->tau == NULL || self->beta == NULL
        || self->alpha == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
out:
    return ret;
}

static int
sv_tables_free(sv_tables_t *self)
{
    tsk_safe_free(self->parent);
    tsk_safe_free(self->child);
    tsk_safe_free(self->sib);
    tsk_safe_free(self->lambda);
    tsk_safe_free(self->pi);
    tsk_safe_free(self->tau);
    tsk_safe_free(self->beta);
    tsk_safe_free(self->alpha);
    return 0;
}
static void
sv_tables_reset(sv_tables_t *self, tsk_tree_t *tree)
{
    const tsk_size_t n = 1 + tree->num_nodes;
    tsk_memset(self->parent, 0, n * sizeof(*self->parent));
    tsk_memset(self->child, 0, n * sizeof(*self->child));
    tsk_memset(self->sib, 0, n * sizeof(*self->sib));
    tsk_memset(self->pi, 0, n * sizeof(*self->pi));
    tsk_memset(self->lambda, 0, n * sizeof(*self->lambda));
    tsk_memset(self->tau, 0, n * sizeof(*self->tau));
    tsk_memset(self->beta, 0, n * sizeof(*self->beta));
    tsk_memset(self->alpha, 0, n * sizeof(*self->alpha));
}

static void
sv_tables_convert_tree(sv_tables_t *self, tsk_tree_t *tree)
{
    const tsk_size_t n = 1 + tree->num_nodes;
    const tsk_id_t *restrict tsk_parent = tree->parent;
    tsk_id_t *restrict child = self->child;
    tsk_id_t *restrict parent = self->parent;
    tsk_id_t *restrict sib = self->sib;
    tsk_size_t j;
    tsk_id_t u, v;

    for (j = 0; j < n - 1; j++) {
        u = (tsk_id_t) j + 1;
        v = tsk_parent[j] + 1;
        sib[u] = child[v];
        child[v] = u;
        parent[u] = v;
    }
}

#define LAMBDA 0

static void
sv_tables_build_index(sv_tables_t *self)
{
    const tsk_id_t *restrict child = self->child;
    const tsk_id_t *restrict parent = self->parent;
    const tsk_id_t *restrict sib = self->sib;
    tsk_id_t *restrict lambda = self->lambda;
    tsk_id_t *restrict pi = self->pi;
    tsk_id_t *restrict tau = self->tau;
    tsk_id_t *restrict beta = self->beta;
    tsk_id_t *restrict alpha = self->alpha;
    tsk_id_t a, n, p, h;

    p = child[LAMBDA];
    n = 0;
    lambda[0] = -1;
    while (p != LAMBDA) {
        while (true) {
            n++;
            pi[p] = n;
            tau[n] = LAMBDA;
            lambda[n] = 1 + lambda[n >> 1];
            if (child[p] != LAMBDA) {
                p = child[p];
            } else {
                break;
            }
        }
        beta[p] = n;
        while (true) {
            tau[beta[p]] = parent[p];
            if (sib[p] != LAMBDA) {
                p = sib[p];
                break;
            } else {
                p = parent[p];
                if (p != LAMBDA) {
                    h = lambda[n & -pi[p]];
                    beta[p] = ((n >> h) | 1) << h;
                } else {
                    break;
                }
            }
        }
    }

    /* Begin the second traversal */
    lambda[0] = lambda[n];
    pi[LAMBDA] = 0;
    beta[LAMBDA] = 0;
    alpha[LAMBDA] = 0;
    p = child[LAMBDA];
    while (p != LAMBDA) {
        while (true) {
            a = alpha[parent[p]] | (beta[p] & -beta[p]);
            alpha[p] = a;
            if (child[p] != LAMBDA) {
                p = child[p];
            } else {
                break;
            }
        }
        while (true) {
            if (sib[p] != LAMBDA) {
                p = sib[p];
                break;
            } else {
                p = parent[p];
                if (p == LAMBDA) {
                    break;
                }
            }
        }
    }
}

static void
sv_tables_build(sv_tables_t *self, tsk_tree_t *tree)
{
    sv_tables_reset(self, tree);
    sv_tables_convert_tree(self, tree);
    sv_tables_build_index(self);
}

static tsk_id_t
sv_tables_mrca_one_based(const sv_tables_t *self, tsk_id_t x, tsk_id_t y)
{
    const tsk_id_t *restrict lambda = self->lambda;
    const tsk_id_t *restrict pi = self->pi;
    const tsk_id_t *restrict tau = self->tau;
    const tsk_id_t *restrict beta = self->beta;
    const tsk_id_t *restrict alpha = self->alpha;
    tsk_id_t h, k, xhat, yhat, ell, j, z;

    if (beta[x] <= beta[y]) {
        h = lambda[beta[y] & -beta[x]];
    } else {
        h = lambda[beta[x] & -beta[y]];
    }
    k = alpha[x] & alpha[y] & -(1 << h);
    h = lambda[k & -k];
    j = ((beta[x] >> h) | 1) << h;
    if (j == beta[x]) {
        xhat = x;
    } else {
        ell = lambda[alpha[x] & ((1 << h) - 1)];
        xhat = tau[((beta[x] >> ell) | 1) << ell];
    }
    if (j == beta[y]) {
        yhat = y;
    } else {
        ell = lambda[alpha[y] & ((1 << h) - 1)];
        yhat = tau[((beta[y] >> ell) | 1) << ell];
    }
    if (pi[xhat] <= pi[yhat]) {
        z = xhat;
    } else {
        z = yhat;
    }
    return z;
}

static tsk_id_t
sv_tables_mrca(const sv_tables_t *self, tsk_id_t x, tsk_id_t y)
{
    /* Convert to 1-based indexes and back */
    return sv_tables_mrca_one_based(self, x + 1, y + 1) - 1;
}

static int
tsk_treeseq_divergence_matrix_branch(const tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, const tsk_size_t *restrict sample_set_sizes,
    const tsk_id_t *restrict sample_sets, tsk_size_t num_windows,
    const double *restrict windows, tsk_flags_t options, double *restrict result)
{
    int ret = 0;
    tsk_tree_t tree;
    const double *restrict nodes_time = self->tables->nodes.time;
    const tsk_size_t N = num_sample_sets;
    tsk_size_t i, j, k, offset, sj, sk;
    tsk_id_t u, v, w, u_root, v_root;
    double tu, tv, d, span, left, right, span_left, span_right;
    double *restrict D;
    sv_tables_t sv;
    tsk_size_t *ss_offsets = tsk_malloc((num_sample_sets + 1) * sizeof(*ss_offsets));

    memset(&sv, 0, sizeof(sv));
    ret = tsk_tree_init(&tree, self, 0);
    if (ret != 0) {
        goto out;
    }
    ret = sv_tables_init(&sv, self->tables->nodes.num_rows + 1);
    if (ret != 0) {
        goto out;
    }
    if (ss_offsets == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    if (self->time_uncalibrated && !(options & TSK_STAT_ALLOW_TIME_UNCALIBRATED)) {
        ret = tsk_trace_error(TSK_ERR_TIME_UNCALIBRATED);
        goto out;
    }

    ss_offsets[0] = 0;
    offset = 0;
    for (j = 0; j < N; j++) {
        offset += sample_set_sizes[j];
        ss_offsets[j + 1] = offset;
    }

    for (i = 0; i < num_windows; i++) {
        left = windows[i];
        right = windows[i + 1];
        D = result + i * N * N;
        ret = tsk_tree_seek(&tree, left, 0);
        if (ret != 0) {
            goto out;
        }
        while (tree.interval.left < right && tree.index != -1) {
            span_left = TSK_MAX(tree.interval.left, left);
            span_right = TSK_MIN(tree.interval.right, right);
            span = span_right - span_left;
            sv_tables_build(&sv, &tree);
            for (sj = 0; sj < N; sj++) {
                for (j = ss_offsets[sj]; j < ss_offsets[sj + 1]; j++) {
                    u = sample_sets[j];
                    for (sk = sj; sk < N; sk++) {
                        for (k = ss_offsets[sk]; k < ss_offsets[sk + 1]; k++) {
                            v = sample_sets[k];
                            if (u == v) {
                                /* This case contributes zero to divergence, so
                                 * short-circuit to save time.
                                 * TODO is there a better way to do this? */
                                continue;
                            }
                            w = sv_tables_mrca(&sv, u, v);
                            if (w != TSK_NULL) {
                                u_root = w;
                                v_root = w;
                            } else {
                                /* Slow path - only happens for nodes in disconnected
                                 * subtrees in a tree with multiple roots */
                                u_root = tsk_tree_get_node_root(&tree, u);
                                v_root = tsk_tree_get_node_root(&tree, v);
                            }
                            tu = nodes_time[u_root] - nodes_time[u];
                            tv = nodes_time[v_root] - nodes_time[v];
                            d = (tu + tv) * span;
                            D[sj * N + sk] += d;
                        }
                    }
                }
            }
            ret = tsk_tree_next(&tree);
            if (ret < 0) {
                goto out;
            }
        }
    }
    ret = 0;
out:
    tsk_tree_free(&tree);
    sv_tables_free(&sv);
    tsk_safe_free(ss_offsets);
    return ret;
}

// FIXME see #2817
// Just including this here for now as it's the simplest option. Everything
// will probably move to stats.[c,h] in the near future though, and it
// can pull in ``genotypes.h`` without issues.
#include <tskit/genotypes.h>

static void
update_site_divergence(const tsk_variant_t *var, const tsk_id_t *restrict A,
    const tsk_size_t *restrict offsets, const tsk_size_t num_sample_sets, double *D)

{
    const tsk_size_t num_alleles = var->num_alleles;
    tsk_size_t a, b, j, k;
    tsk_id_t u, v;
    double increment;

    for (a = 0; a < num_alleles; a++) {
        for (b = a + 1; b < num_alleles; b++) {
            for (j = offsets[a]; j < offsets[a + 1]; j++) {
                for (k = offsets[b]; k < offsets[b + 1]; k++) {
                    u = A[j];
                    v = A[k];
                    /* Only increment the upper triangle to (hopefully) improve memory
                     * access patterns */
                    if (u > v) {
                        u = A[k];
                        v = A[j];
                    }
                    increment = 1;
                    if (u == v) {
                        increment = 2;
                    }
                    D[u * (tsk_id_t) num_sample_sets + v] += increment;
                }
            }
        }
    }
}

static void
group_alleles(const tsk_variant_t *var, tsk_id_t *restrict A, tsk_size_t *offsets)
{
    const tsk_size_t n = var->num_samples;
    const int32_t *restrict genotypes = var->genotypes;
    tsk_id_t a;
    tsk_size_t j, k;

    k = 0;
    offsets[0] = 0;
    for (a = 0; a < (tsk_id_t) var->num_alleles; a++) {
        offsets[a + 1] = offsets[a];
        for (j = 0; j < n; j++) {
            if (genotypes[j] == a) {
                offsets[a + 1]++;
                A[k] = (tsk_id_t) j;
                k++;
            }
        }
    }
}

static void
remap_to_sample_sets(const tsk_size_t num_samples, const tsk_id_t *restrict samples,
    const tsk_id_t *restrict sample_set_index_map, tsk_id_t *restrict A)
{
    tsk_size_t j;
    tsk_id_t u;
    for (j = 0; j < num_samples; j++) {
        u = samples[A[j]];
        tsk_bug_assert(u >= 0);
        tsk_bug_assert(sample_set_index_map[u] >= 0);
        A[j] = sample_set_index_map[u];
    }
}

static int
tsk_treeseq_divergence_matrix_site(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_id_t *restrict sample_set_index_map, const tsk_size_t num_samples,
    const tsk_id_t *restrict samples, tsk_size_t num_windows,
    const double *restrict windows, tsk_flags_t TSK_UNUSED(options),
    double *restrict result)
{
    int ret = 0;
    tsk_size_t i;
    tsk_id_t site_id;
    double left, right;
    double *restrict D;
    const tsk_id_t num_sites = (tsk_id_t) self->tables->sites.num_rows;
    const double *restrict sites_position = self->tables->sites.position;
    tsk_id_t *A = tsk_malloc(num_samples * sizeof(*A));
    /* Allocate the allele offsets at the first variant */
    tsk_size_t max_alleles = 0;
    tsk_size_t *allele_offsets = NULL;
    tsk_variant_t variant;

    /* FIXME it's not clear that using TSK_ISOLATED_NOT_MISSING is
     * correct here */
    ret = tsk_variant_init(
        &variant, self, samples, num_samples, NULL, TSK_ISOLATED_NOT_MISSING);
    if (ret != 0) {
        goto out;
    }
    if (A == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    site_id = 0;
    while (site_id < num_sites && sites_position[site_id] < windows[0]) {
        site_id++;
    }

    for (i = 0; i < num_windows; i++) {
        left = windows[i];
        right = windows[i + 1];
        D = result + i * num_sample_sets * num_sample_sets;

        if (site_id < num_sites) {
            tsk_bug_assert(sites_position[site_id] >= left);
        }
        while (site_id < num_sites && sites_position[site_id] < right) {
            ret = tsk_variant_decode(&variant, site_id, 0);
            if (ret != 0) {
                goto out;
            }
            if (variant.num_alleles > max_alleles) {
                /* could do some kind of doubling here, but there's no
                 * point - just keep it simple for testing. */
                max_alleles = variant.num_alleles;
                tsk_safe_free(allele_offsets);
                allele_offsets = tsk_malloc((max_alleles + 1) * sizeof(*allele_offsets));
                if (allele_offsets == NULL) {
                    ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
                    goto out;
                }
            }
            group_alleles(&variant, A, allele_offsets);
            remap_to_sample_sets(num_samples, samples, sample_set_index_map, A);
            update_site_divergence(&variant, A, allele_offsets, num_sample_sets, D);
            site_id++;
        }
    }
    ret = 0;
out:
    tsk_variant_free(&variant);
    tsk_safe_free(A);
    tsk_safe_free(allele_offsets);
    return ret;
}

/* Return the mapping from node IDs to the index of the sample set
 * they belong to, or -1 of none. Error if a node is in more than one
 * set.
 */
static int
get_sample_set_index_map(const tsk_treeseq_t *self, const tsk_size_t num_sample_sets,
    const tsk_size_t *restrict sample_set_sizes, const tsk_id_t *restrict sample_sets,
    tsk_size_t *ret_total_samples, tsk_id_t *restrict node_index_map)
{
    int ret = 0;
    tsk_size_t i, j, k;
    tsk_id_t u;
    tsk_size_t total_samples = 0;
    const tsk_size_t num_nodes = self->tables->nodes.num_rows;
    const tsk_flags_t *restrict node_flags = self->tables->nodes.flags;

    for (j = 0; j < num_nodes; j++) {
        node_index_map[j] = TSK_NULL;
    }
    i = 0;
    for (j = 0; j < num_sample_sets; j++) {
        total_samples += sample_set_sizes[j];
        for (k = 0; k < sample_set_sizes[j]; k++) {
            u = sample_sets[i];
            i++;
            if (u < 0 || u >= (tsk_id_t) num_nodes) {
                ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
                goto out;
            }
            /* Note: we require nodes to be samples because we have to think
             * about how to normalise by the length of genome that the node
             * is 'in' the tree for each window otherwise. */
            if (!(node_flags[u] & TSK_NODE_IS_SAMPLE)) {
                ret = tsk_trace_error(TSK_ERR_BAD_SAMPLES);
                goto out;
            }
            if (node_index_map[u] != TSK_NULL) {
                ret = tsk_trace_error(TSK_ERR_DUPLICATE_SAMPLE);
                goto out;
            }
            node_index_map[u] = (tsk_id_t) j;
        }
    }
    *ret_total_samples = total_samples;
out:
    return ret;
}

static void
fill_lower_triangle_count_normalise(const tsk_size_t num_windows, const tsk_size_t n,
    const tsk_size_t *set_sizes, double *restrict result)
{
    tsk_size_t i, j, k;
    double denom;
    double *restrict D;

    /* TODO there's probably a better striding pattern that could be used here */
    for (i = 0; i < num_windows; i++) {
        D = result + i * n * n;
        for (j = 0; j < n; j++) {
            denom = (double) set_sizes[j] * (double) (set_sizes[j] - 1);
            if (denom != 0) {
                D[j * n + j] /= denom;
            }
            for (k = j + 1; k < n; k++) {
                denom = (double) set_sizes[j] * (double) set_sizes[k];
                D[j * n + k] /= denom;
                D[k * n + j] = D[j * n + k];
            }
        }
    }
}

int
tsk_treeseq_divergence_matrix(const tsk_treeseq_t *self, tsk_size_t num_sample_sets_in,
    const tsk_size_t *sample_set_sizes_in, const tsk_id_t *sample_sets_in,
    tsk_size_t num_windows, const double *windows, tsk_flags_t options, double *result)
{
    int ret = 0;
    tsk_size_t N, total_samples;
    const tsk_size_t *sample_set_sizes;
    const tsk_id_t *sample_sets;
    tsk_size_t *tmp_sample_set_sizes = NULL;
    const double default_windows[] = { 0, self->tables->sequence_length };
    const tsk_size_t num_nodes = self->tables->nodes.num_rows;
    bool stat_site = !!(options & TSK_STAT_SITE);
    bool stat_branch = !!(options & TSK_STAT_BRANCH);
    bool stat_node = !!(options & TSK_STAT_NODE);
    tsk_id_t *sample_set_index_map
        = tsk_malloc(num_nodes * sizeof(*sample_set_index_map));
    tsk_size_t j;

    if (stat_node) {
        ret = tsk_trace_error(TSK_ERR_UNSUPPORTED_STAT_MODE);
        goto out;
    }
    /* If no mode is specified, we default to site mode */
    if (!(stat_site || stat_branch)) {
        stat_site = true;
    }
    /* It's an error to specify more than one mode */
    if (stat_site + stat_branch > 1) {
        ret = tsk_trace_error(TSK_ERR_MULTIPLE_STAT_MODES);
        goto out;
    }

    if (options & TSK_STAT_POLARISED) {
        ret = tsk_trace_error(TSK_ERR_STAT_POLARISED_UNSUPPORTED);
        goto out;
    }

    if (windows == NULL) {
        num_windows = 1;
        windows = default_windows;
    } else {
        ret = tsk_treeseq_check_windows(self, num_windows, windows, 0);
        if (ret != 0) {
            goto out;
        }
    }

    /* If sample_sets is NULL, use self->samples and ignore input
     * num_sample_sets */
    sample_sets = sample_sets_in;
    N = num_sample_sets_in;
    if (sample_sets_in == NULL) {
        sample_sets = self->samples;
        if (sample_set_sizes_in == NULL) {
            N = self->num_samples;
        }
    }
    sample_set_sizes = sample_set_sizes_in;
    /* If sample_set_sizes is NULL, assume its N 1S */
    if (sample_set_sizes_in == NULL) {
        tmp_sample_set_sizes = tsk_malloc(N * sizeof(*tmp_sample_set_sizes));
        if (tmp_sample_set_sizes == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
        for (j = 0; j < N; j++) {
            tmp_sample_set_sizes[j] = 1;
        }
        sample_set_sizes = tmp_sample_set_sizes;
    }

    ret = get_sample_set_index_map(
        self, N, sample_set_sizes, sample_sets, &total_samples, sample_set_index_map);
    if (ret != 0) {
        goto out;
    }

    tsk_memset(result, 0, num_windows * N * N * sizeof(*result));

    if (stat_branch) {
        ret = tsk_treeseq_divergence_matrix_branch(self, N, sample_set_sizes,
            sample_sets, num_windows, windows, options, result);
    } else {
        tsk_bug_assert(stat_site);
        ret = tsk_treeseq_divergence_matrix_site(self, N, sample_set_index_map,
            total_samples, sample_sets, num_windows, windows, options, result);
    }
    if (ret != 0) {
        goto out;
    }
    fill_lower_triangle_count_normalise(num_windows, N, sample_set_sizes, result);

    if (options & TSK_STAT_SPAN_NORMALISE) {
        span_normalise(num_windows, windows, N * N, result);
    }
out:
    tsk_safe_free(sample_set_index_map);
    tsk_safe_free(tmp_sample_set_sizes);
    return ret;
}

/* ======================================================== *
 * Extend haplotypes
 * ======================================================== */

typedef struct _edge_list_t {
    tsk_id_t edge;
    // the `extended` flags records whether we have decided to extend
    // this entry to the current tree?
    int extended;
    struct _edge_list_t *next;
} edge_list_t;

static void
edge_list_print(edge_list_t **head, tsk_edge_table_t *edges, FILE *out)
{
    int n = 0;
    edge_list_t *px;
    fprintf(out, "Edge list:\n");
    for (px = *head; px != NULL; px = px->next) {
        fprintf(out, "  %d: %d (%d); ", n, (int) px->edge, px->extended);
        if (px->edge >= 0 && edges != NULL) {
            fprintf(out, "%d->%d on [%.1f, %.1f)", (int) edges->child[px->edge],
                (int) edges->parent[px->edge], edges->left[px->edge],
                edges->right[px->edge]);
        } else {
            fprintf(out, "(null)");
        }
        fprintf(out, "\n");
        n += 1;
    }
    fprintf(out, "length = %d\n", n);
}

static void
edge_list_append_entry(
    edge_list_t **head, edge_list_t **tail, edge_list_t *x, tsk_id_t edge, int extended)
{
    x->edge = edge;
    x->extended = extended;
    x->next = NULL;

    if (*tail == NULL) {
        *head = x;
    } else {
        (*tail)->next = x;
    }
    *tail = x;
}

static void
remove_unextended(edge_list_t **head, edge_list_t **tail)
{
    edge_list_t *px, *x;

    px = *head;
    while (px != NULL && px->extended == 0) {
        px = px->next;
    }
    *head = px;
    if (px != NULL) {
        px->extended = 0;
        x = px->next;
        while (x != NULL) {
            if (x->extended > 0) {
                x->extended = 0;
                px->next = x;
                px = x;
            }
            x = x->next;
        }
        px->next = NULL;
    }
    *tail = px;
}

static void
edge_list_set_extended(edge_list_t **head, tsk_id_t edge_id)
{
    // finds the entry with edge 'edge_id'
    // and sets its 'extended' flag to 1
    edge_list_t *px;
    px = *head;
    tsk_bug_assert(px != NULL);
    while (px->edge != edge_id) {
        px = px->next;
        tsk_bug_assert(px != NULL);
    }
    tsk_bug_assert(px->edge == edge_id);
    px->extended = 1;
}

static int
tsk_treeseq_slide_mutation_nodes_up(
    const tsk_treeseq_t *self, tsk_mutation_table_t *mutations)
{
    int ret = 0;
    double t;
    tsk_id_t c, p, next_mut;
    const tsk_size_t num_nodes = self->tables->nodes.num_rows;
    const double *sites_position = self->tables->sites.position;
    const double *nodes_time = self->tables->nodes.time;
    tsk_tree_t tree;

    ret = tsk_tree_init(&tree, self, TSK_NO_SAMPLE_COUNTS);
    if (ret != 0) {
        goto out;
    }

    next_mut = 0;
    for (ret = tsk_tree_first(&tree); ret == TSK_TREE_OK; ret = tsk_tree_next(&tree)) {
        while (next_mut < (tsk_id_t) mutations->num_rows
               && sites_position[mutations->site[next_mut]] < tree.interval.right) {
            t = mutations->time[next_mut];
            if (tsk_is_unknown_time(t)) {
                ret = tsk_trace_error(TSK_ERR_DISALLOWED_UNKNOWN_MUTATION_TIME);
                goto out;
            }
            c = mutations->node[next_mut];
            tsk_bug_assert(c < (tsk_id_t) num_nodes);
            p = tree.parent[c];
            while (p != TSK_NULL && nodes_time[p] <= t) {
                c = p;
                p = tree.parent[c];
            }
            tsk_bug_assert(nodes_time[c] <= t);
            mutations->node[next_mut] = c;
            next_mut++;
        }
    }
    if (ret != 0) {
        goto out;
    }

out:
    tsk_tree_free(&tree);

    return ret;
}

typedef struct {
    const tsk_treeseq_t *ts;
    tsk_edge_table_t *edges;
    int direction;
    tsk_id_t *last_degree, *next_degree;
    tsk_id_t *last_nodes_edge, *next_nodes_edge;
    tsk_id_t *parent_out, *parent_in;
    bool *not_sample;
    double *near_side, *far_side;
    edge_list_t *edges_out_head, *edges_out_tail;
    edge_list_t *edges_in_head, *edges_in_tail;
    tsk_blkalloc_t edge_list_heap;
} haplotype_extender_t;

static int
haplotype_extender_init(haplotype_extender_t *self, const tsk_treeseq_t *ts,
    int direction, tsk_edge_table_t *edges)
{
    int ret = 0;
    tsk_id_t tj;
    tsk_size_t num_nodes = tsk_treeseq_get_num_nodes(ts);

    tsk_memset(self, 0, sizeof(haplotype_extender_t));

    self->ts = ts;
    self->edges = edges;
    ret = tsk_edge_table_copy(&ts->tables->edges, self->edges, TSK_NO_INIT);
    if (ret != 0) {
        goto out;
    }

    self->direction = direction;
    if (direction == TSK_DIR_FORWARD) {
        self->near_side = self->edges->left;
        self->far_side = self->edges->right;
    } else {
        self->near_side = self->edges->right;
        self->far_side = self->edges->left;
    }

    self->edges_in_head = NULL;
    self->edges_in_tail = NULL;
    self->edges_out_head = NULL;
    self->edges_out_tail = NULL;

    ret = tsk_blkalloc_init(&self->edge_list_heap, 8192);
    if (ret != 0) {
        goto out;
    }

    self->last_degree = tsk_calloc(num_nodes, sizeof(*self->last_degree));
    self->next_degree = tsk_calloc(num_nodes, sizeof(*self->next_degree));
    self->last_nodes_edge = tsk_malloc(num_nodes * sizeof(*self->last_nodes_edge));
    self->next_nodes_edge = tsk_malloc(num_nodes * sizeof(*self->next_nodes_edge));
    self->parent_out = tsk_malloc(num_nodes * sizeof(*self->parent_out));
    self->parent_in = tsk_malloc(num_nodes * sizeof(*self->parent_in));
    self->not_sample = tsk_malloc(num_nodes * sizeof(*self->not_sample));

    if (self->last_degree == NULL || self->next_degree == NULL
        || self->last_nodes_edge == NULL || self->next_nodes_edge == NULL
        || self->parent_out == NULL || self->parent_in == NULL
        || self->not_sample == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    tsk_memset(self->last_nodes_edge, 0xff, num_nodes * sizeof(*self->last_nodes_edge));
    tsk_memset(self->next_nodes_edge, 0xff, num_nodes * sizeof(*self->next_nodes_edge));
    tsk_memset(self->parent_out, 0xff, num_nodes * sizeof(*self->parent_out));
    tsk_memset(self->parent_in, 0xff, num_nodes * sizeof(*self->parent_in));

    for (tj = 0; tj < (tsk_id_t) num_nodes; tj++) {
        self->not_sample[tj] = ((ts->tables->nodes.flags[tj] & TSK_NODE_IS_SAMPLE) == 0);
    }

out:
    return ret;
}

static void
haplotype_extender_print_state(haplotype_extender_t *self, FILE *out)
{
    fprintf(out, "\n======= haplotype extender ===========\n");
    fprintf(out, "parent in:\n");
    for (int j = 0; j < (int) self->ts->tables->nodes.num_rows; j++) {
        fprintf(out, "   %d: %d\n", j, (int) self->parent_in[j]);
    }
    fprintf(out, "parent out:\n");
    for (int j = 0; j < (int) self->ts->tables->nodes.num_rows; j++) {
        fprintf(out, "   %d: %d\n", j, (int) self->parent_out[j]);
    }
    fprintf(out, "last nodes edge:\n");
    for (int j = 0; j < (int) self->ts->tables->nodes.num_rows; j++) {
        tsk_id_t ej = self->last_nodes_edge[j];
        fprintf(out, "   %d: %d, ", j, (int) ej);
        if (self->last_nodes_edge[j] != TSK_NULL) {
            fprintf(out, "(%d->%d, %.1f-%.1f)", (int) self->edges->child[ej],
                (int) self->edges->parent[ej], self->edges->left[ej],
                self->edges->right[ej]);
        } else {
            fprintf(out, "(null);");
        }
        fprintf(out, "\n");
    }
    fprintf(out, "next nodes edge:\n");
    for (int j = 0; j < (int) self->ts->tables->nodes.num_rows; j++) {
        tsk_id_t ej = self->next_nodes_edge[j];
        fprintf(out, "   %d: %d, ", j, (int) ej);
        if (self->next_nodes_edge[j] != TSK_NULL) {
            fprintf(out, "(%d->%d, %.1f-%.1f)", (int) self->edges->child[ej],
                (int) self->edges->parent[ej], self->edges->left[ej],
                self->edges->right[ej]);
        } else {
            fprintf(out, "(null);");
        }
        fprintf(out, "\n");
    }
    fprintf(out, "edges out:\n");
    edge_list_print(&self->edges_out_head, self->edges, out);
    fprintf(out, "edges in:\n");
    edge_list_print(&self->edges_in_head, self->edges, out);
}

static int
haplotype_extender_free(haplotype_extender_t *self)
{
    tsk_blkalloc_free(&self->edge_list_heap);
    tsk_safe_free(self->last_degree);
    tsk_safe_free(self->next_degree);
    tsk_safe_free(self->last_nodes_edge);
    tsk_safe_free(self->next_nodes_edge);
    tsk_safe_free(self->parent_out);
    tsk_safe_free(self->parent_in);
    tsk_safe_free(self->not_sample);
    return 0;
}

static int
haplotype_extender_next_tree(haplotype_extender_t *self, tsk_tree_position_t *tree_pos)
{
    int ret = 0;
    tsk_id_t tj, e;
    edge_list_t *ex_out, *ex_in;
    edge_list_t *new_ex;
    const tsk_id_t *edges_child = self->edges->child;
    const tsk_id_t *edges_parent = self->edges->parent;

    for (ex_out = self->edges_out_head; ex_out != NULL; ex_out = ex_out->next) {
        e = ex_out->edge;
        self->parent_out[edges_child[e]] = TSK_NULL;
        // note we only adjust near_side of edges_in, not edges_out,
        // so no need to check for zero-length edges
        if (ex_out->extended > 1) {
            // this is needed to catch newly-created edges
            self->last_nodes_edge[edges_child[e]] = e;
            self->last_degree[edges_child[e]] += 1;
            self->last_degree[edges_parent[e]] += 1;
        } else if (ex_out->extended == 0) {
            self->last_nodes_edge[edges_child[e]] = TSK_NULL;
            self->last_degree[edges_child[e]] -= 1;
            self->last_degree[edges_parent[e]] -= 1;
        }
    }
    remove_unextended(&self->edges_out_head, &self->edges_out_tail);
    for (ex_in = self->edges_in_head; ex_in != NULL; ex_in = ex_in->next) {
        e = ex_in->edge;
        self->parent_in[edges_child[e]] = TSK_NULL;
        if (ex_in->extended == 0 && self->near_side[e] != self->far_side[e]) {
            self->last_nodes_edge[edges_child[e]] = e;
            self->last_degree[edges_child[e]] += 1;
            self->last_degree[edges_parent[e]] += 1;
        }
    }
    remove_unextended(&self->edges_in_head, &self->edges_in_tail);

    // done cleanup from last tree transition;
    // now we set the state up for this tree transition
    for (tj = tree_pos->out.start; tj != tree_pos->out.stop; tj += self->direction) {
        e = tree_pos->out.order[tj];
        if (self->near_side[e] != self->far_side[e]) {
            new_ex = tsk_blkalloc_get(&self->edge_list_heap, sizeof(*new_ex));
            if (new_ex == NULL) {
                ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
                goto out;
            }
            edge_list_append_entry(
                &self->edges_out_head, &self->edges_out_tail, new_ex, e, 0);
        }
    }
    for (ex_out = self->edges_out_head; ex_out != NULL; ex_out = ex_out->next) {
        e = ex_out->edge;
        self->parent_out[edges_child[e]] = edges_parent[e];
        self->next_nodes_edge[edges_child[e]] = TSK_NULL;
        self->next_degree[edges_child[e]] -= 1;
        self->next_degree[edges_parent[e]] -= 1;
    }

    for (tj = tree_pos->in.start; tj != tree_pos->in.stop; tj += self->direction) {
        e = tree_pos->in.order[tj];
        // add edge to pending_in
        new_ex = tsk_blkalloc_get(&self->edge_list_heap, sizeof(*new_ex));
        if (new_ex == NULL) {
            ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
            goto out;
        }
        edge_list_append_entry(&self->edges_in_head, &self->edges_in_tail, new_ex, e, 0);
    }
    for (ex_in = self->edges_in_head; ex_in != NULL; ex_in = ex_in->next) {
        e = ex_in->edge;
        self->parent_in[edges_child[e]] = edges_parent[e];
        self->next_nodes_edge[edges_child[e]] = e;
        self->next_degree[edges_child[e]] += 1;
        self->next_degree[edges_parent[e]] += 1;
    }

out:
    return ret;
}

static int
haplotype_extender_add_or_extend_edge(haplotype_extender_t *self, tsk_id_t new_parent,
    tsk_id_t child, double left, double right)
{
    int ret = 0;
    double there;
    tsk_id_t old_edge, e_out, old_parent;
    edge_list_t *ex_in;
    edge_list_t *new_ex = NULL;
    tsk_id_t e_in;

    there = (self->direction == TSK_DIR_FORWARD) ? right : left;
    old_edge = self->next_nodes_edge[child];
    if (old_edge != TSK_NULL) {
        old_parent = self->edges->parent[old_edge];
    } else {
        old_parent = TSK_NULL;
    }
    if (new_parent != old_parent) {
        if (self->parent_out[child] == new_parent) {
            // if our new edge is in edges_out, it should be extended
            e_out = self->last_nodes_edge[child];
            self->far_side[e_out] = there;
            edge_list_set_extended(&self->edges_out_head, e_out);
        } else {
            e_out = tsk_edge_table_add_row(
                self->edges, left, right, new_parent, child, NULL, 0);
            if (e_out < 0) {
                ret = (int) e_out;
                goto out;
            }
            /* pointers to left/right might have changed! */
            if (self->direction == TSK_DIR_FORWARD) {
                self->near_side = self->edges->left;
                self->far_side = self->edges->right;
            } else {
                self->near_side = self->edges->right;
                self->far_side = self->edges->left;
            }
            new_ex = tsk_blkalloc_get(&self->edge_list_heap, sizeof(*new_ex));
            if (new_ex == NULL) {
                ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
                goto out;
            }
            edge_list_append_entry(
                &self->edges_out_head, &self->edges_out_tail, new_ex, e_out, 2);
        }
        self->next_nodes_edge[child] = e_out;
        self->next_degree[child] += 1;
        self->next_degree[new_parent] += 1;
        self->parent_out[child] = TSK_NULL;
        if (old_edge != TSK_NULL) {
            for (ex_in = self->edges_in_head; ex_in != NULL; ex_in = ex_in->next) {
                e_in = ex_in->edge;
                if (e_in == old_edge) {
                    self->near_side[e_in] = there;
                    if (self->far_side[e_in] != there) {
                        ex_in->extended = 1;
                    }
                    self->next_degree[child] -= 1;
                    self->next_degree[self->parent_in[child]] -= 1;
                    self->parent_in[child] = TSK_NULL;
                }
            }
        }
    }
out:
    return ret;
}

static float
haplotype_extender_mergeable(haplotype_extender_t *self, tsk_id_t c)
{
    // returns the number of new edges needed
    // if the paths in parent_in and parent_out
    // up through nodes that aren't in the other tree
    // end at the same place and don't have conflicting times;
    // otherwise, return infinity
    tsk_id_t p_in, p_out, child;
    float num_new_edges; // needs to be float so we can have infinity
    int num_extended;
    double t_in, t_out;
    bool climb_in, climb_out;
    const double *nodes_time = self->ts->tables->nodes.time;

    p_out = self->parent_out[c];
    p_in = self->parent_in[c];
    t_out = (p_out == TSK_NULL) ? INFINITY : nodes_time[p_out];
    t_in = (p_in == TSK_NULL) ? INFINITY : nodes_time[p_in];
    child = c;
    num_new_edges = 0;
    num_extended = 0;
    while (true) {
        climb_in = (p_in != TSK_NULL && self->last_degree[p_in] == 0
                    && self->not_sample[p_in] && t_in < t_out);
        climb_out = (p_out != TSK_NULL && self->next_degree[p_out] == 0
                     && self->not_sample[p_out] && t_out < t_in);
        if (climb_in) {
            if (self->parent_in[child] != p_in) {
                num_new_edges += 1;
            }
            child = p_in;
            p_in = self->parent_in[p_in];
            t_in = (p_in == TSK_NULL) ? INFINITY : nodes_time[p_in];
        } else if (climb_out) {
            if (self->parent_out[child] != p_out) {
                num_new_edges += 1;
            }
            child = p_out;
            p_out = self->parent_out[p_out];
            t_out = (p_out == TSK_NULL) ? INFINITY : nodes_time[p_out];
            num_extended += 1;
        } else {
            break;
        }
    }
    if ((num_extended == 0) || (p_in != p_out) || (p_in == TSK_NULL)) {
        num_new_edges = INFINITY;
    }
    return num_new_edges;
}

static int
haplotype_extender_merge_paths(
    haplotype_extender_t *self, tsk_id_t c, double left, double right)
{
    int ret = 0;
    tsk_id_t p_in, p_out, child;
    double t_in, t_out;
    bool climb_in, climb_out;
    const double *nodes_time = self->ts->tables->nodes.time;

    p_out = self->parent_out[c];
    p_in = self->parent_in[c];
    t_out = nodes_time[p_out];
    t_in = nodes_time[p_in];
    child = c;
    while (true) {
        climb_in = (p_in != TSK_NULL && self->last_degree[p_in] == 0
                    && self->not_sample[p_in] && t_in < t_out);
        climb_out = (p_out != TSK_NULL && self->next_degree[p_out] == 0
                     && self->not_sample[p_out] && t_out < t_in);
        if (climb_in) {
            ret = haplotype_extender_add_or_extend_edge(self, p_in, child, left, right);
            if (ret != 0) {
                goto out;
            }
            child = p_in;
            p_in = self->parent_in[p_in];
            t_in = (p_in == TSK_NULL) ? INFINITY : nodes_time[p_in];
        } else if (climb_out) {
            ret = haplotype_extender_add_or_extend_edge(self, p_out, child, left, right);
            if (ret != 0) {
                goto out;
            }
            child = p_out;
            p_out = self->parent_out[p_out];
            t_out = (p_out == TSK_NULL) ? INFINITY : nodes_time[p_out];
        } else {
            break;
        }
    }
    tsk_bug_assert(p_out == p_in);
    ret = haplotype_extender_add_or_extend_edge(self, p_out, child, left, right);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

static int
haplotype_extender_extend_paths(haplotype_extender_t *self)
{
    int ret = 0;
    bool valid;
    double left, right;
    float ne, max_new_edges, next_max_new_edges;
    tsk_tree_position_t tree_pos;
    edge_list_t *ex_in;
    tsk_id_t e_in, c, e;
    tsk_size_t num_edges;
    tsk_bool_t *keep = NULL;

    tsk_memset(&tree_pos, 0, sizeof(tree_pos));
    ret = tsk_tree_position_init(&tree_pos, self->ts, 0);
    if (ret != 0) {
        goto out;
    }

    if (self->direction == TSK_DIR_FORWARD) {
        valid = tsk_tree_position_next(&tree_pos);
    } else {
        valid = tsk_tree_position_prev(&tree_pos);
    }

    while (valid) {
        left = tree_pos.interval.left;
        right = tree_pos.interval.right;
        ret = haplotype_extender_next_tree(self, &tree_pos);
        if (ret != 0) {
            goto out;
        }
        max_new_edges = 0;
        next_max_new_edges = INFINITY;
        while (max_new_edges < INFINITY) {
            for (ex_in = self->edges_in_head; ex_in != NULL; ex_in = ex_in->next) {
                e_in = ex_in->edge;
                c = self->edges->child[e_in];
                if (self->last_degree[c] > 0) {
                    ne = haplotype_extender_mergeable(self, c);
                    if (ne <= max_new_edges) {
                        ret = haplotype_extender_merge_paths(self, c, left, right);
                        if (ret != 0) {
                            goto out;
                        }
                    } else {
                        next_max_new_edges = TSK_MIN(ne, next_max_new_edges);
                    }
                }
            }
            max_new_edges = next_max_new_edges;
            next_max_new_edges = INFINITY;
        }
        if (self->direction == TSK_DIR_FORWARD) {
            valid = tsk_tree_position_next(&tree_pos);
        } else {
            valid = tsk_tree_position_prev(&tree_pos);
        }
    }
    /* Get rid of adjacent, identical edges */
    /* note: we need to calloc this here instead of at the start
     * because we don't know how big it will need to be until now */
    num_edges = self->edges->num_rows;
    keep = tsk_calloc(num_edges, sizeof(*keep));
    if (keep == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    for (e = 0; e < (tsk_id_t) num_edges - 1; e++) {
        if (self->edges->parent[e] == self->edges->parent[e + 1]
            && self->edges->child[e] == self->edges->child[e + 1]
            && self->edges->right[e] == self->edges->left[e + 1]) {
            self->edges->right[e] = self->edges->right[e + 1];
            self->edges->left[e + 1] = self->edges->right[e + 1];
        }
    }
    for (e = 0; e < (tsk_id_t) num_edges; e++) {
        keep[e] = self->edges->left[e] < self->edges->right[e];
    }
    ret = tsk_edge_table_keep_rows(self->edges, keep, 0, NULL);
out:
    tsk_tree_position_free(&tree_pos);
    tsk_safe_free(keep);
    return ret;
}

static int
extend_haplotypes_iter(const tsk_treeseq_t *self, int direction, tsk_edge_table_t *edges,
    tsk_flags_t options)
{
    int ret = 0;
    haplotype_extender_t haplotype_extender;
    tsk_memset(&haplotype_extender, 0, sizeof(haplotype_extender));
    ret = haplotype_extender_init(&haplotype_extender, self, direction, edges);
    if (ret != 0) {
        goto out;
    }

    ret = haplotype_extender_extend_paths(&haplotype_extender);
    if (ret != 0) {
        goto out;
    }

    if (!!(options & TSK_DEBUG)) {
        haplotype_extender_print_state(&haplotype_extender, tsk_get_debug_stream());
    }

out:
    haplotype_extender_free(&haplotype_extender);
    return ret;
}

int TSK_WARN_UNUSED
tsk_treeseq_extend_haplotypes(
    const tsk_treeseq_t *self, int max_iter, tsk_flags_t options, tsk_treeseq_t *output)
{
    int ret = 0;
    tsk_table_collection_t tables;
    tsk_treeseq_t ts;
    int iter, j;
    tsk_size_t last_num_edges;
    tsk_bookmark_t sort_start;
    const int direction[] = { TSK_DIR_FORWARD, TSK_DIR_REVERSE };

    tsk_memset(&tables, 0, sizeof(tables));
    tsk_memset(&ts, 0, sizeof(ts));
    tsk_memset(output, 0, sizeof(*output));

    if (max_iter <= 0) {
        ret = tsk_trace_error(TSK_ERR_EXTEND_EDGES_BAD_MAXITER);
        goto out;
    }
    if (tsk_treeseq_get_num_migrations(self) != 0) {
        ret = tsk_trace_error(TSK_ERR_MIGRATIONS_NOT_SUPPORTED);
        goto out;
    }

    /* Note: there is a fair bit of copying of table data in this implementation
     * currently, as we create a new tree sequence for each iteration, which
     * takes a full copy of the input tables. We could streamline this by
     * adding a flag to treeseq_init which says "steal a reference to these
     * tables and *don't* free them at the end". Then, we would only need
     * one copy of the full tables, and could pass in a standalone edge
     * table to use for in-place updating.
     */
    ret = tsk_table_collection_copy(self->tables, &tables, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_mutation_table_clear(&tables.mutations);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_init(&ts, &tables, 0);
    if (ret != 0) {
        goto out;
    }

    last_num_edges = tsk_treeseq_get_num_edges(&ts);
    for (iter = 0; iter < max_iter; iter++) {
        for (j = 0; j < 2; j++) {
            ret = extend_haplotypes_iter(&ts, direction[j], &tables.edges, options);
            if (ret != 0) {
                goto out;
            }
            /* We're done with the current ts now */
            tsk_treeseq_free(&ts);
            /* no need to sort sites and mutations */
            memset(&sort_start, 0, sizeof(sort_start));
            sort_start.sites = tables.sites.num_rows;
            sort_start.mutations = tables.mutations.num_rows;
            ret = tsk_table_collection_sort(&tables, &sort_start, 0);
            if (ret != 0) {
                goto out;
            }
            ret = tsk_treeseq_init(&ts, &tables, TSK_TS_INIT_BUILD_INDEXES);
            if (ret != 0) {
                goto out;
            }
        }
        if (last_num_edges == tsk_treeseq_get_num_edges(&ts)) {
            break;
        }
        last_num_edges = tsk_treeseq_get_num_edges(&ts);
    }

    /* Remap mutation nodes */
    ret = tsk_mutation_table_copy(
        &self->tables->mutations, &tables.mutations, TSK_NO_INIT);
    if (ret != 0) {
        goto out;
    }
    /* Note: to allow migrations we'd also have to do this same operation
     * on the migration nodes; however it's a can of worms because the interval
     * covering the migration might no longer make sense. */
    ret = tsk_treeseq_slide_mutation_nodes_up(&ts, &tables.mutations);
    if (ret != 0) {
        goto out;
    }
    tsk_treeseq_free(&ts);
    ret = tsk_treeseq_init(&ts, &tables, TSK_TS_INIT_BUILD_INDEXES);
    if (ret != 0) {
        goto out;
    }

    /* Hand ownership of the tree sequence to the calling code */
    tsk_memcpy(output, &ts, sizeof(ts));
    tsk_memset(&ts, 0, sizeof(*output));
out:
    tsk_treeseq_free(&ts);
    tsk_table_collection_free(&tables);
    return ret;
}

/* ======================================================== *
 * Pair coalescence
 * ======================================================== */

static int
check_node_bin_map(
    const tsk_size_t num_nodes, const tsk_size_t num_bins, const tsk_id_t *node_bin_map)
{
    int ret = 0;
    tsk_id_t max_index, index;
    tsk_size_t i;

    max_index = TSK_NULL;
    for (i = 0; i < num_nodes; i++) {
        index = node_bin_map[i];
        if (index < TSK_NULL) {
            ret = tsk_trace_error(TSK_ERR_BAD_NODE_BIN_MAP);
            goto out;
        }
        if (index > max_index) {
            max_index = index;
        }
    }
    if (num_bins < 1 || (tsk_id_t) num_bins < max_index + 1) {
        ret = tsk_trace_error(TSK_ERR_BAD_NODE_BIN_MAP_DIM);
        goto out;
    }
out:
    return ret;
}

static inline void
TRANSPOSE_2D(tsk_size_t rows, tsk_size_t cols, const double *source, double *dest)
{
    tsk_size_t i, j;
    for (i = 0; i < rows; ++i) {
        for (j = 0; j < cols; ++j) {
            dest[j * rows + i] = source[i * cols + j];
        }
    }
}

static inline void
pair_coalescence_count(tsk_size_t num_set_indexes, const tsk_id_t *set_indexes,
    tsk_size_t num_sample_sets, const double *parent_count, const double *child_count,
    const double *parent_state, const double *inside, double *outside, double *result)
{
    tsk_size_t i;
    tsk_id_t j, k;
    for (i = 0; i < num_sample_sets; i++) {
        outside[i] = parent_count[i] - child_count[i] - parent_state[i];
    }
    for (i = 0; i < num_set_indexes; i++) {
        j = set_indexes[2 * i];
        k = set_indexes[2 * i + 1];
        result[i] = outside[j] * inside[k];
        if (j != k) {
            result[i] += outside[k] * inside[j];
        }
    }
}

int
tsk_treeseq_pair_coalescence_stat(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_set_indexes, const tsk_id_t *set_indexes, tsk_size_t num_windows,
    const double *windows, tsk_size_t num_bins, const tsk_id_t *node_bin_map,
    pair_coalescence_stat_func_t *summary_func, tsk_size_t summary_func_dim,
    void *summary_func_args, tsk_flags_t options, double *result)
{
    int ret = 0;
    double left, right, remaining_span, missing_span, window_span, denominator, x, t;
    tsk_id_t e, p, c, u, v, w, i, j;
    tsk_size_t num_samples, num_edges;
    tsk_tree_position_t tree_pos;
    const tsk_table_collection_t *tables = self->tables;
    const tsk_size_t num_nodes = tables->nodes.num_rows;
    const double *restrict nodes_time = self->tables->nodes.time;
    const double sequence_length = tables->sequence_length;
    const tsk_size_t num_outputs = summary_func_dim;

    /* buffers */
    bool *visited = NULL;
    tsk_id_t *nodes_sample_set = NULL;
    tsk_id_t *nodes_parent = NULL;
    double *coalescing_pairs = NULL;
    double *coalescence_time = NULL;
    double *nodes_sample = NULL;
    double *sample_count = NULL;
    double *bin_weight = NULL;
    double *bin_values = NULL;
    double *pair_count = NULL;
    double *total_pair = NULL;
    double *outside = NULL;

    /* row pointers */
    double *inside = NULL;
    double *weight = NULL;
    double *values = NULL;
    double *output = NULL;
    double *above = NULL;
    double *below = NULL;
    double *state = NULL;
    double *pairs = NULL;
    double *times = NULL;

    tsk_memset(&tree_pos, 0, sizeof(tree_pos));

    /* check inputs */
    ret = tsk_treeseq_check_windows(self, num_windows, windows, TSK_REQUIRE_FULL_SPAN);
    if (ret != 0) {
        goto out;
    }
    ret = check_set_indexes(num_sample_sets, 2 * num_set_indexes, set_indexes);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_treeseq_check_sample_sets(
        self, num_sample_sets, sample_set_sizes, sample_sets);
    if (ret != 0) {
        goto out;
    }
    ret = check_node_bin_map(num_nodes, num_bins, node_bin_map);
    if (ret != 0) {
        goto out;
    }

    /* map nodes to sample sets */
    nodes_sample_set = tsk_malloc(num_nodes * sizeof(*nodes_sample_set));
    if (nodes_sample_set == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    ret = get_sample_set_index_map(self, num_sample_sets, sample_set_sizes, sample_sets,
        &num_samples, nodes_sample_set);
    if (ret != 0) {
        goto out;
    }

    visited = tsk_malloc(num_nodes * sizeof(*visited));
    outside = tsk_malloc(num_sample_sets * sizeof(*outside));
    nodes_parent = tsk_malloc(num_nodes * sizeof(*nodes_parent));
    nodes_sample = tsk_calloc(num_nodes * num_sample_sets, sizeof(*nodes_sample));
    sample_count = tsk_malloc(num_nodes * num_sample_sets * sizeof(*sample_count));
    coalescing_pairs = tsk_calloc(num_bins * num_set_indexes, sizeof(*coalescing_pairs));
    coalescence_time = tsk_calloc(num_bins * num_set_indexes, sizeof(*coalescence_time));
    bin_weight = tsk_malloc(num_bins * num_set_indexes * sizeof(*bin_weight));
    bin_values = tsk_malloc(num_bins * num_set_indexes * sizeof(*bin_values));
    pair_count = tsk_malloc(num_set_indexes * sizeof(*pair_count));
    total_pair = tsk_malloc(num_set_indexes * sizeof(*total_pair));
    if (nodes_parent == NULL || nodes_sample == NULL || sample_count == NULL
        || coalescing_pairs == NULL || bin_weight == NULL || bin_values == NULL
        || outside == NULL || pair_count == NULL || visited == NULL
        || total_pair == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    for (i = 0; i < (tsk_id_t) num_set_indexes; i++) {
        u = set_indexes[2 * i];
        v = set_indexes[2 * i + 1];
        total_pair[i] = (double) sample_set_sizes[u] * (double) sample_set_sizes[v];
        if (u == v) {
            total_pair[i] -= (double) sample_set_sizes[v];
            total_pair[i] /= 2;
        }
    }

    /* initialize internal state */
    for (c = 0; c < (tsk_id_t) num_nodes; c++) {
        i = nodes_sample_set[c];
        if (i != TSK_NULL) {
            state = GET_2D_ROW(nodes_sample, num_sample_sets, c);
            state[i] = 1.0;
        }
        nodes_parent[c] = TSK_NULL;
        visited[c] = false;
    }
    tsk_memcpy(
        sample_count, nodes_sample, num_nodes * num_sample_sets * sizeof(*sample_count));

    ret = tsk_tree_position_init(&tree_pos, self, 0);
    if (ret != 0) {
        goto out;
    }

    num_edges = 0;
    missing_span = 0.0;
    w = 0;
    while (true) {
        tsk_tree_position_next(&tree_pos);
        if (tree_pos.index == TSK_NULL) {
            break;
        }

        left = tree_pos.interval.left;
        right = tree_pos.interval.right;
        remaining_span = sequence_length - left;

        for (u = tree_pos.out.start; u != tree_pos.out.stop; u++) {
            e = tree_pos.out.order[u];
            p = tables->edges.parent[e];
            c = tables->edges.child[e];
            nodes_parent[c] = TSK_NULL;
            inside = GET_2D_ROW(sample_count, num_sample_sets, c);
            while (p != TSK_NULL) { /* downdate statistic */
                v = node_bin_map[p];
                t = nodes_time[p];
                if (v != TSK_NULL) {
                    above = GET_2D_ROW(sample_count, num_sample_sets, p);
                    below = GET_2D_ROW(sample_count, num_sample_sets, c);
                    state = GET_2D_ROW(nodes_sample, num_sample_sets, p);
                    pairs = GET_2D_ROW(coalescing_pairs, num_set_indexes, v);
                    times = GET_2D_ROW(coalescence_time, num_set_indexes, v);
                    pair_coalescence_count(num_set_indexes, set_indexes, num_sample_sets,
                        above, below, state, inside, outside, pair_count);
                    for (i = 0; i < (tsk_id_t) num_set_indexes; i++) {
                        x = pair_count[i] * remaining_span;
                        pairs[i] -= x;
                        times[i] -= t * x;
                    }
                }
                c = p;
                p = nodes_parent[c];
            }
            p = tables->edges.parent[e];
            while (p != TSK_NULL) { /* downdate state */
                above = GET_2D_ROW(sample_count, num_sample_sets, p);
                for (i = 0; i < (tsk_id_t) num_sample_sets; i++) {
                    above[i] -= inside[i];
                }
                p = nodes_parent[p];
            }
            num_edges -= 1;
        }

        for (u = tree_pos.in.start; u != tree_pos.in.stop; u++) {
            e = tree_pos.in.order[u];
            p = tables->edges.parent[e];
            c = tables->edges.child[e];
            nodes_parent[c] = p;
            inside = GET_2D_ROW(sample_count, num_sample_sets, c);
            while (p != TSK_NULL) { /* update state */
                above = GET_2D_ROW(sample_count, num_sample_sets, p);
                for (i = 0; i < (tsk_id_t) num_sample_sets; i++) {
                    above[i] += inside[i];
                }
                p = nodes_parent[p];
            }
            p = tables->edges.parent[e];
            while (p != TSK_NULL) { /* update statistic */
                v = node_bin_map[p];
                t = nodes_time[p];
                if (v != TSK_NULL) {
                    above = GET_2D_ROW(sample_count, num_sample_sets, p);
                    below = GET_2D_ROW(sample_count, num_sample_sets, c);
                    state = GET_2D_ROW(nodes_sample, num_sample_sets, p);
                    pairs = GET_2D_ROW(coalescing_pairs, num_set_indexes, v);
                    times = GET_2D_ROW(coalescence_time, num_set_indexes, v);
                    pair_coalescence_count(num_set_indexes, set_indexes, num_sample_sets,
                        above, below, state, inside, outside, pair_count);
                    for (i = 0; i < (tsk_id_t) num_set_indexes; i++) {
                        x = pair_count[i] * remaining_span;
                        pairs[i] += x;
                        times[i] += t * x;
                    }
                }
                c = p;
                p = nodes_parent[c];
            }
            num_edges += 1;
        }

        if (num_edges == 0) {
            missing_span += right - left;
        }

        /* flush windows */
        while (w < (tsk_id_t) num_windows && windows[w + 1] <= right) {
            TRANSPOSE_2D(num_bins, num_set_indexes, coalescing_pairs, bin_weight);
            TRANSPOSE_2D(num_bins, num_set_indexes, coalescence_time, bin_values);
            tsk_memset(coalescing_pairs, 0,
                num_bins * num_set_indexes * sizeof(*coalescing_pairs));
            tsk_memset(coalescence_time, 0,
                num_bins * num_set_indexes * sizeof(*coalescence_time));
            remaining_span = sequence_length - windows[w + 1];
            for (j = 0; j < (tsk_id_t) num_samples; j++) { /* truncate at tree */
                c = sample_sets[j];
                p = nodes_parent[c];
                while (!visited[c] && p != TSK_NULL) {
                    v = node_bin_map[p];
                    t = nodes_time[p];
                    if (v != TSK_NULL) {
                        above = GET_2D_ROW(sample_count, num_sample_sets, p);
                        below = GET_2D_ROW(sample_count, num_sample_sets, c);
                        state = GET_2D_ROW(nodes_sample, num_sample_sets, p);
                        pairs = GET_2D_ROW(coalescing_pairs, num_set_indexes, v);
                        times = GET_2D_ROW(coalescence_time, num_set_indexes, v);
                        pair_coalescence_count(num_set_indexes, set_indexes,
                            num_sample_sets, above, below, state, below, outside,
                            pair_count);
                        for (i = 0; i < (tsk_id_t) num_set_indexes; i++) {
                            weight = GET_2D_ROW(bin_weight, num_bins, i);
                            values = GET_2D_ROW(bin_values, num_bins, i);
                            x = pair_count[i] * remaining_span / 2;
                            pairs[i] += x;
                            times[i] += t * x;
                            weight[v] -= x;
                            values[v] -= t * x;
                        }
                    }
                    visited[c] = true;
                    c = p;
                    p = nodes_parent[c];
                }
            }
            for (j = 0; j < (tsk_id_t) num_samples; j++) { /* reset tree */
                c = sample_sets[j];
                p = nodes_parent[c];
                while (visited[c] && p != TSK_NULL) {
                    visited[c] = false;
                    c = p;
                    p = nodes_parent[c];
                }
            }
            for (i = 0; i < (tsk_id_t) num_set_indexes; i++) { /* normalise values */
                weight = GET_2D_ROW(bin_weight, num_bins, i);
                values = GET_2D_ROW(bin_values, num_bins, i);
                for (v = 0; v < (tsk_id_t) num_bins; v++) {
                    values[v] /= weight[v];
                }
            }
            /* normalise weights */
            if (options & (TSK_STAT_SPAN_NORMALISE | TSK_STAT_PAIR_NORMALISE)) {
                window_span = windows[w + 1] - windows[w] - missing_span;
                missing_span = 0.0;
                if (num_edges == 0) {
                    /* missing interval, so remove overcounted missing span */
                    remaining_span = right - windows[w + 1];
                    window_span += remaining_span;
                    missing_span += remaining_span;
                }
                for (i = 0; i < (tsk_id_t) num_set_indexes; i++) {
                    denominator = 1.0;
                    if (options & TSK_STAT_SPAN_NORMALISE) {
                        denominator *= window_span;
                    }
                    if (options & TSK_STAT_PAIR_NORMALISE) {
                        denominator *= total_pair[i];
                    }
                    weight = GET_2D_ROW(bin_weight, num_bins, i);
                    for (v = 0; v < (tsk_id_t) num_bins; v++) {
                        weight[v] *= denominator == 0.0 ? 0.0 : 1 / denominator;
                    }
                }
            }
            for (i = 0; i < (tsk_id_t) num_set_indexes; i++) { /* summarise bins */
                weight = GET_2D_ROW(bin_weight, num_bins, i);
                values = GET_2D_ROW(bin_values, num_bins, i);
                output = GET_3D_ROW(
                    result, num_set_indexes, num_outputs, (tsk_size_t) w, i);
                ret = summary_func(
                    num_bins, weight, values, num_outputs, output, summary_func_args);
                if (ret != 0) {
                    goto out;
                }
            };
            w += 1;
        }
    }
out:
    tsk_tree_position_free(&tree_pos);
    tsk_safe_free(nodes_sample_set);
    tsk_safe_free(coalescing_pairs);
    tsk_safe_free(coalescence_time);
    tsk_safe_free(nodes_parent);
    tsk_safe_free(nodes_sample);
    tsk_safe_free(sample_count);
    tsk_safe_free(bin_weight);
    tsk_safe_free(bin_values);
    tsk_safe_free(pair_count);
    tsk_safe_free(total_pair);
    tsk_safe_free(visited);
    tsk_safe_free(outside);
    return ret;
}

static int
pair_coalescence_weights(tsk_size_t TSK_UNUSED(input_dim), const double *weight,
    const double *TSK_UNUSED(values), tsk_size_t output_dim, double *output,
    void *TSK_UNUSED(params))
{
    int ret = 0;
    tsk_memcpy(output, weight, output_dim * sizeof(*output));
    return ret;
}

int
tsk_treeseq_pair_coalescence_counts(const tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, const tsk_size_t *sample_set_sizes,
    const tsk_id_t *sample_sets, tsk_size_t num_set_indexes, const tsk_id_t *set_indexes,
    tsk_size_t num_windows, const double *windows, tsk_size_t num_bins,
    const tsk_id_t *node_bin_map, tsk_flags_t options, double *result)
{
    return tsk_treeseq_pair_coalescence_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_set_indexes, set_indexes, num_windows, windows, num_bins,
        node_bin_map, pair_coalescence_weights, num_bins, NULL, options, result);
}

static int
pair_coalescence_quantiles(tsk_size_t input_dim, const double *weight,
    const double *values, tsk_size_t output_dim, double *output, void *params)
{
    int ret = 0;
    double coalesced, timepoint;
    double *quantiles = (double *) params;
    tsk_size_t i, j;
    j = 0;
    coalesced = 0.0;
    timepoint = TSK_UNKNOWN_TIME;
    for (i = 0; i < output_dim; i++) {
        output[i] = NAN;
    }
    for (i = 0; i < input_dim; i++) {
        if (weight[i] > 0) {
            coalesced += weight[i];
            timepoint = values[i];
            while (j < output_dim && quantiles[j] <= coalesced) {
                output[j] = timepoint;
                j += 1;
            }
        }
    }
    if (quantiles[output_dim - 1] == 1.0) {
        output[output_dim - 1] = timepoint;
    }
    return ret;
}

static int
check_quantiles(const tsk_size_t num_quantiles, const double *quantiles)
{
    int ret = 0;
    tsk_size_t i;
    double last = -INFINITY;
    for (i = 0; i < num_quantiles; i++) {
        if (quantiles[i] <= last || quantiles[i] < 0.0 || quantiles[i] > 1.0) {
            ret = tsk_trace_error(TSK_ERR_BAD_QUANTILES);
            goto out;
        }
        last = quantiles[i];
    }
out:
    return ret;
}

static int
check_sorted_node_bin_map(
    const tsk_treeseq_t *self, tsk_size_t num_bins, const tsk_id_t *node_bin_map)
{
    int ret = 0;
    tsk_size_t num_nodes = self->tables->nodes.num_rows;
    const double *nodes_time = self->tables->nodes.time;
    double last;
    tsk_id_t i, j;
    double *min_time = tsk_malloc(num_bins * sizeof(*min_time));
    double *max_time = tsk_malloc(num_bins * sizeof(*max_time));
    if (min_time == NULL || max_time == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }
    for (j = 0; j < (tsk_id_t) num_bins; j++) {
        min_time[j] = TSK_UNKNOWN_TIME;
        max_time[j] = TSK_UNKNOWN_TIME;
    }
    for (i = 0; i < (tsk_id_t) num_nodes; i++) {
        j = node_bin_map[i];
        if (j < 0 || j >= (tsk_id_t) num_bins) {
            continue;
        }
        if (tsk_is_unknown_time(max_time[j]) || nodes_time[i] > max_time[j]) {
            max_time[j] = nodes_time[i];
        }
        if (tsk_is_unknown_time(min_time[j]) || nodes_time[i] < min_time[j]) {
            min_time[j] = nodes_time[i];
        }
    }
    last = -INFINITY;
    for (j = 0; j < (tsk_id_t) num_bins; j++) {
        if (tsk_is_unknown_time(min_time[j])) {
            continue;
        }
        if (min_time[j] < last) {
            ret = tsk_trace_error(TSK_ERR_UNSORTED_TIMES);
            goto out;
        } else {
            last = max_time[j];
        }
    }
out:
    tsk_safe_free(min_time);
    tsk_safe_free(max_time);
    return ret;
}

int
tsk_treeseq_pair_coalescence_quantiles(const tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, const tsk_size_t *sample_set_sizes,
    const tsk_id_t *sample_sets, tsk_size_t num_set_indexes, const tsk_id_t *set_indexes,
    tsk_size_t num_windows, const double *windows, tsk_size_t num_bins,
    const tsk_id_t *node_bin_map, tsk_size_t num_quantiles, double *quantiles,
    tsk_flags_t options, double *result)
{
    int ret = 0;
    void *params = (void *) quantiles;
    ret = check_quantiles(num_quantiles, quantiles);
    if (ret != 0) {
        goto out;
    }
    ret = check_sorted_node_bin_map(self, num_bins, node_bin_map);
    if (ret != 0) {
        goto out;
    }
    options |= TSK_STAT_SPAN_NORMALISE | TSK_STAT_PAIR_NORMALISE;
    ret = tsk_treeseq_pair_coalescence_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_set_indexes, set_indexes, num_windows, windows, num_bins,
        node_bin_map, pair_coalescence_quantiles, num_quantiles, params, options,
        result);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

static int
pair_coalescence_rates(tsk_size_t input_dim, const double *weight, const double *values,
    tsk_size_t output_dim, double *output, void *params)
{
    int ret = 0;
    double coalesced, rate, waiting_time, a, b;
    double *time_windows = (double *) params;
    tsk_id_t i, j;
    tsk_bug_assert(input_dim == output_dim);
    for (j = (tsk_id_t) output_dim; j > 0; j--) { /* find last window with data */
        if (weight[j - 1] == 0) {
            output[j - 1] = NAN; /* TODO: should fill value be zero instead? */
        } else {
            break;
        }
    }
    coalesced = 0.0;
    for (i = 0; i < j; i++) {
        a = time_windows[i];
        b = time_windows[i + 1];
        if (i + 1 == j) {
            waiting_time = values[i] < a ? 0.0 : values[i] - a;
            rate = 1 / waiting_time;
        } else {
            rate = log(1 - weight[i] / (1 - coalesced)) / (a - b);
        }
        // avoid tiny negative values from fp error
        output[i] = rate > 0 ? rate : 0;
        coalesced += weight[i];
    }
    return ret;
}

static int
check_coalescence_rate_time_windows(const tsk_treeseq_t *self,
    tsk_size_t num_sample_sets, const tsk_size_t *sample_set_sizes,
    const tsk_id_t *sample_sets, tsk_size_t num_time_windows,
    const tsk_id_t *node_time_window, const double *time_windows)
{
    int ret = 0;
    double timepoint;
    const double *nodes_time = self->tables->nodes.time;
    tsk_size_t num_nodes = self->tables->nodes.num_rows;
    tsk_id_t i, j, k;
    tsk_id_t n;
    if (num_time_windows == 0) {
        ret = tsk_trace_error(TSK_ERR_BAD_TIME_WINDOWS_DIM);
        goto out;
    }
    /* time windows are sorted */
    timepoint = time_windows[0];
    for (i = 0; i < (tsk_id_t) num_time_windows; i++) {
        if (time_windows[i + 1] <= timepoint) {
            ret = tsk_trace_error(TSK_ERR_BAD_TIME_WINDOWS);
            goto out;
        }
        timepoint = time_windows[i + 1];
    }
    if (timepoint != INFINITY) {
        ret = tsk_trace_error(TSK_ERR_BAD_TIME_WINDOWS_END);
        goto out;
    }
    /* all sample times align with start of first time window */
    k = 0;
    for (i = 0; i < (tsk_id_t) num_sample_sets; i++) {
        for (j = 0; j < (tsk_id_t) sample_set_sizes[i]; j++) {
            n = sample_sets[k++];
            if (nodes_time[n] != time_windows[0]) {
                ret = tsk_trace_error(TSK_ERR_BAD_SAMPLE_PAIR_TIMES);
                goto out;
            }
        }
    }
    /* nodes are correctly assigned to time windows */
    for (i = 0; i < (tsk_id_t) num_nodes; i++) {
        j = node_time_window[i];
        if (j < 0) {
            continue;
        }
        if (j >= (tsk_id_t) num_time_windows) {
            ret = tsk_trace_error(TSK_ERR_BAD_NODE_BIN_MAP_DIM);
            goto out;
        }
        if (nodes_time[i] < time_windows[j] || nodes_time[i] >= time_windows[j + 1]) {
            ret = tsk_trace_error(TSK_ERR_BAD_NODE_TIME_WINDOW);
            goto out;
        }
    }
out:
    return ret;
}

int
tsk_treeseq_pair_coalescence_rates(const tsk_treeseq_t *self, tsk_size_t num_sample_sets,
    const tsk_size_t *sample_set_sizes, const tsk_id_t *sample_sets,
    tsk_size_t num_set_indexes, const tsk_id_t *set_indexes, tsk_size_t num_windows,
    const double *windows, tsk_size_t num_time_windows, const tsk_id_t *node_time_window,
    double *time_windows, tsk_flags_t options, double *result)
{
    int ret = 0;
    void *params = (void *) time_windows;
    ret = check_coalescence_rate_time_windows(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_time_windows, node_time_window, time_windows);
    if (ret != 0) {
        goto out;
    }
    options |= TSK_STAT_SPAN_NORMALISE | TSK_STAT_PAIR_NORMALISE;
    ret = tsk_treeseq_pair_coalescence_stat(self, num_sample_sets, sample_set_sizes,
        sample_sets, num_set_indexes, set_indexes, num_windows, windows,
        num_time_windows, node_time_window, pair_coalescence_rates, num_time_windows,
        params, options, result);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

/* ======================================================== *
 * Relatedness matrix-vector product
 * ======================================================== */

typedef struct {
    const tsk_treeseq_t *ts;
    tsk_size_t num_weights;
    const double *weights;
    tsk_size_t num_windows;
    const double *windows;
    tsk_size_t num_focal_nodes;
    const tsk_id_t *focal_nodes;
    tsk_flags_t options;
    double *result;
    tsk_tree_position_t tree_pos;
    double position;
    tsk_size_t num_nodes;
    tsk_id_t *parent;
    double *x;
    double *w;
    double *v;
} tsk_matvec_calculator_t;

static void
tsk_matvec_calculator_print_state(const tsk_matvec_calculator_t *self, FILE *out)
{
    tsk_id_t j;
    tsk_size_t num_samples = tsk_treeseq_get_num_samples(self->ts);

    fprintf(out, "Matvec state:\n");
    fprintf(out, "options = %d\n", self->options);
    fprintf(out, "position = %f\n", self->position);
    fprintf(out, "focal nodes = %lld: [", (long long) self->num_focal_nodes);
    fprintf(out, "tree_pos:\n");
    tsk_tree_position_print_state(&self->tree_pos, out);
    fprintf(out, "samples = %lld: [", (long long) num_samples);
    fprintf(out, "]\n");
    fprintf(out, "node\tparent\tx\tv\tw");
    fprintf(out, "\n");

    for (j = 0; j < (tsk_id_t) self->num_nodes; j++) {
        fprintf(out, "%lld\t", (long long) j);
        fprintf(out, "%lld\t%g\t%g\t%g\n", (long long) self->parent[j], self->x[j],
            self->v[j], self->w[j]);
    }
}

static int
tsk_matvec_calculator_init(tsk_matvec_calculator_t *self, const tsk_treeseq_t *ts,
    tsk_size_t num_weights, const double *weights, tsk_size_t num_windows,
    const double *windows, tsk_size_t num_focal_nodes, const tsk_id_t *focal_nodes,
    tsk_flags_t options, double *result)
{
    int ret = 0;
    tsk_size_t num_samples = tsk_treeseq_get_num_samples(ts);
    const tsk_size_t num_nodes = ts->tables->nodes.num_rows;
    const double *row;
    double *new_row;
    tsk_size_t k;
    tsk_id_t index, u, j;
    double *weight_means = tsk_malloc(num_weights * sizeof(*weight_means));
    const tsk_size_t num_trees = ts->num_trees;
    const double *restrict breakpoints = ts->breakpoints;

    self->ts = ts;
    self->num_weights = num_weights;
    self->weights = weights;
    self->num_windows = num_windows;
    self->windows = windows;
    self->num_focal_nodes = num_focal_nodes;
    self->focal_nodes = focal_nodes;
    self->options = options;
    self->result = result;
    self->num_nodes = num_nodes;
    self->position = windows[0];

    self->parent = tsk_malloc(num_nodes * sizeof(*self->parent));
    self->x = tsk_calloc(num_nodes, sizeof(*self->x));
    self->v = tsk_calloc(num_nodes * num_weights, sizeof(*self->v));
    self->w = tsk_calloc(num_nodes * num_weights, sizeof(*self->w));

    if (self->parent == NULL || self->x == NULL || self->w == NULL || self->v == NULL
        || weight_means == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    tsk_memset(result, 0, num_windows * num_focal_nodes * num_weights * sizeof(*result));
    tsk_memset(self->parent, TSK_NULL, num_nodes * sizeof(*self->parent));

    for (j = 0; j < (tsk_id_t) num_focal_nodes; j++) {
        if (focal_nodes[j] < 0 || (tsk_size_t) focal_nodes[j] >= num_nodes) {
            ret = tsk_trace_error(TSK_ERR_NODE_OUT_OF_BOUNDS);
            goto out;
        }
    }

    ret = tsk_tree_position_init(&self->tree_pos, ts, 0);
    if (ret != 0) {
        goto out;
    }
    /* seek to the first window */
    index = (tsk_id_t) tsk_search_sorted(breakpoints, num_trees + 1, windows[0]);
    if (breakpoints[index] > windows[0]) {
        index--;
    }
    ret = tsk_tree_position_seek_forward(&self->tree_pos, index);
    if (ret != 0) {
        goto out;
    }

    for (k = 0; k < num_weights; k++) {
        weight_means[k] = 0.0;
    }
    /* centre the input */
    if (!(options & TSK_STAT_NONCENTRED)) {
        for (j = 0; j < (tsk_id_t) num_samples; j++) {
            row = GET_2D_ROW(weights, num_weights, j);
            for (k = 0; k < num_weights; k++) {
                weight_means[k] += row[k];
            }
        }
        for (k = 0; k < num_weights; k++) {
            weight_means[k] /= (double) num_samples;
        }
    }

    /* set the initial state */
    for (j = 0; j < (tsk_id_t) num_samples; j++) {
        u = ts->samples[j];
        row = GET_2D_ROW(weights, num_weights, j);
        new_row = GET_2D_ROW(self->w, num_weights, u);
        for (k = 0; k < num_weights; k++) {
            new_row[k] = row[k] - weight_means[k];
        }
    }
out:
    tsk_safe_free(weight_means);
    return ret;
}

static int
tsk_matvec_calculator_free(tsk_matvec_calculator_t *self)
{
    tsk_safe_free(self->parent);
    tsk_safe_free(self->x);
    tsk_safe_free(self->w);
    tsk_safe_free(self->v);
    tsk_tree_position_free(&self->tree_pos);

    /* Make this safe for multiple free calls */
    memset(self, 0, sizeof(*self));
    return 0;
}

static inline void
tsk_matvec_calculator_add_z(tsk_id_t u, tsk_id_t p, const double position,
    double *restrict x, const tsk_size_t num_weights, double *restrict w,
    double *restrict v, const double *restrict nodes_time)
{
    double t, span;
    tsk_size_t j;
    double *restrict v_row, *restrict w_row;

    if (p != TSK_NULL) {
        t = nodes_time[p] - nodes_time[u];
        span = position - x[u];
        // do this: self->v[u] += t * span * self->w[u];
        w_row = GET_2D_ROW(w, num_weights, u);
        v_row = GET_2D_ROW(v, num_weights, u);
        for (j = 0; j < num_weights; j++) {
            v_row[j] += t * span * w_row[j];
        }
    }
    x[u] = position;
}

static void
tsk_matvec_calculator_adjust_path_up(
    tsk_matvec_calculator_t *self, tsk_id_t p, tsk_id_t c, double sign)
{
    tsk_size_t j;
    double *p_row, *c_row;
    const tsk_id_t *restrict parent = self->parent;
    const double position = self->position;
    double *restrict x = self->x;
    const tsk_size_t num_weights = self->num_weights;
    double *restrict w = self->w;
    double *restrict v = self->v;
    const double *restrict nodes_time = self->ts->tables->nodes.time;

    // sign = -1 for removing edges, +1 for adding
    while (p != TSK_NULL) {
        tsk_matvec_calculator_add_z(
            p, parent[p], position, x, num_weights, w, v, nodes_time);
        // do this: self->v[c] -= sign * self->v[p];
        p_row = GET_2D_ROW(v, num_weights, p);
        c_row = GET_2D_ROW(v, num_weights, c);
        for (j = 0; j < num_weights; j++) {
            c_row[j] -= sign * p_row[j];
        }
        // do this: self->w[p] += sign * self->w[c];
        p_row = GET_2D_ROW(w, num_weights, p);
        c_row = GET_2D_ROW(w, num_weights, c);
        for (j = 0; j < num_weights; j++) {
            p_row[j] += sign * c_row[j];
        }
        p = parent[p];
    }
}

static void
tsk_matvec_calculator_remove_edge(tsk_matvec_calculator_t *self, tsk_id_t p, tsk_id_t c)
{
    tsk_id_t *parent = self->parent;
    const double position = self->position;
    double *restrict x = self->x;
    const tsk_size_t num_weights = self->num_weights;
    double *restrict w = self->w;
    double *restrict v = self->v;
    const double *restrict nodes_time = self->ts->tables->nodes.time;

    tsk_matvec_calculator_add_z(
        c, parent[c], position, x, num_weights, w, v, nodes_time);
    parent[c] = TSK_NULL;
    tsk_matvec_calculator_adjust_path_up(self, p, c, -1);
}

static void
tsk_matvec_calculator_insert_edge(tsk_matvec_calculator_t *self, tsk_id_t p, tsk_id_t c)
{
    tsk_id_t *parent = self->parent;

    tsk_matvec_calculator_adjust_path_up(self, p, c, +1);
    self->x[c] = self->position;
    parent[c] = p;
}

static int
tsk_matvec_calculator_write_output(tsk_matvec_calculator_t *self, double *restrict y)
{
    int ret = 0;
    tsk_id_t u;
    tsk_size_t j, k;
    const tsk_size_t n = self->num_focal_nodes;
    const tsk_size_t num_weights = self->num_weights;
    const double position = self->position;
    double *u_row, *out_row;
    double *out_means = tsk_malloc(num_weights * sizeof(*out_means));
    const tsk_id_t *restrict parent = self->parent;
    const double *restrict nodes_time = self->ts->tables->nodes.time;
    double *restrict x = self->x;
    double *restrict w = self->w;
    double *restrict v = self->v;
    const tsk_id_t *restrict focal_nodes = self->focal_nodes;

    if (out_means == NULL) {
        ret = tsk_trace_error(TSK_ERR_NO_MEMORY);
        goto out;
    }

    for (j = 0; j < n; j++) {
        out_row = GET_2D_ROW(y, num_weights, j);
        u = focal_nodes[j];
        while (u != TSK_NULL) {
            if (x[u] != position) {
                tsk_matvec_calculator_add_z(
                    u, parent[u], position, x, num_weights, w, v, nodes_time);
            }
            u_row = GET_2D_ROW(v, num_weights, u);
            for (k = 0; k < num_weights; k++) {
                out_row[k] += u_row[k];
            }
            u = parent[u];
        }
    }

    if (!(self->options & TSK_STAT_NONCENTRED)) {
        for (k = 0; k < num_weights; k++) {
            out_means[k] = 0.0;
        }
        for (j = 0; j < n; j++) {
            out_row = GET_2D_ROW(y, num_weights, j);
            for (k = 0; k < num_weights; k++) {
                out_means[k] += out_row[k];
            }
        }
        for (k = 0; k < num_weights; k++) {
            out_means[k] /= (double) n;
        }
        for (j = 0; j < n; j++) {
            out_row = GET_2D_ROW(y, num_weights, j);
            for (k = 0; k < num_weights; k++) {
                out_row[k] -= out_means[k];
            }
        }
    }
    /* zero out v */
    tsk_memset(self->v, 0, self->num_nodes * num_weights * sizeof(*self->v));
out:
    tsk_safe_free(out_means);
    return ret;
}

static int
tsk_matvec_calculator_run(tsk_matvec_calculator_t *self)
{
    int ret = 0;
    tsk_size_t j, k, m;
    tsk_id_t e, p, c;
    const tsk_size_t out_size = self->num_weights * self->num_focal_nodes;
    const tsk_size_t num_edges = self->ts->tables->edges.num_rows;
    const double *restrict edge_right = self->ts->tables->edges.right;
    const double *restrict edge_left = self->ts->tables->edges.left;
    const tsk_id_t *restrict edge_child = self->ts->tables->edges.child;
    const tsk_id_t *restrict edge_parent = self->ts->tables->edges.parent;
    const double *restrict windows = self->windows;
    double *restrict out;
    tsk_tree_position_t tree_pos = self->tree_pos;
    const tsk_id_t *restrict in_order = tree_pos.in.order;
    const tsk_id_t *restrict out_order = tree_pos.out.order;
    bool valid;
    double next_position;

    m = 0;
    self->position = windows[0];

    for (j = (tsk_size_t) tree_pos.in.start; j != (tsk_size_t) tree_pos.in.stop; j++) {
        e = in_order[j];
        tsk_bug_assert(edge_left[e] <= self->position);
        if (self->position < edge_right[e]) {
            p = edge_parent[e];
            c = edge_child[e];
            tsk_matvec_calculator_insert_edge(self, p, c);
        }
    }

    valid = tsk_tree_position_next(&tree_pos);
    j = (tsk_size_t) tree_pos.in.start;
    k = (tsk_size_t) tree_pos.out.start;
    while (m < self->num_windows) {
        if (valid && self->position == tree_pos.interval.left) {
            for (k = (tsk_size_t) tree_pos.out.start;
                 k != (tsk_size_t) tree_pos.out.stop; k++) {
                e = out_order[k];
                p = edge_parent[e];
                c = edge_child[e];
                tsk_matvec_calculator_remove_edge(self, p, c);
            }
            for (j = (tsk_size_t) tree_pos.in.start; j != (tsk_size_t) tree_pos.in.stop;
                 j++) {
                e = in_order[j];
                p = edge_parent[e];
                c = edge_child[e];
                tsk_matvec_calculator_insert_edge(self, p, c);
            }
            valid = tsk_tree_position_next(&tree_pos);
        }
        next_position = windows[m + 1];
        if (j < num_edges) {
            next_position = TSK_MIN(next_position, edge_left[in_order[j]]);
        }
        if (k < num_edges) {
            next_position = TSK_MIN(next_position, edge_right[out_order[k]]);
        }
        tsk_bug_assert(self->position < next_position);
        self->position = next_position;
        if (self->position == windows[m + 1]) {
            out = GET_2D_ROW(self->result, out_size, m);
            tsk_matvec_calculator_write_output(self, out);
            m += 1;
        }
        if (self->options & TSK_DEBUG) {
            tsk_matvec_calculator_print_state(self, tsk_get_debug_stream());
        }
    }
    if (!!(self->options & TSK_STAT_SPAN_NORMALISE)) {
        span_normalise(self->num_windows, windows, out_size, self->result);
    }

    /* out: */
    return ret;
}

int
tsk_treeseq_genetic_relatedness_vector(const tsk_treeseq_t *self, tsk_size_t num_weights,
    const double *weights, tsk_size_t num_windows, const double *windows,
    tsk_size_t num_focal_nodes, const tsk_id_t *focal_nodes, double *result,
    tsk_flags_t options)
{
    int ret = 0;
    bool stat_site = !!(options & TSK_STAT_SITE);
    bool stat_node = !!(options & TSK_STAT_NODE);
    tsk_matvec_calculator_t calc;

    memset(&calc, 0, sizeof(calc));

    if (stat_node || stat_site) {
        ret = tsk_trace_error(TSK_ERR_UNSUPPORTED_STAT_MODE);
        goto out;
    }
    ret = tsk_treeseq_check_windows(self, num_windows, windows, 0);
    if (ret != 0) {
        goto out;
    }

    ret = tsk_matvec_calculator_init(&calc, self, num_weights, weights, num_windows,
        windows, num_focal_nodes, focal_nodes, options, result);
    if (ret != 0) {
        goto out;
    }
    if (options & TSK_DEBUG) {
        tsk_matvec_calculator_print_state(&calc, tsk_get_debug_stream());
    }
    ret = tsk_matvec_calculator_run(&calc);
out:
    tsk_matvec_calculator_free(&calc);
    return ret;
}
