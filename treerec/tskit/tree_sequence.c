/*
** Copyright (C) 2015-2018 University of Oxford
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
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "trees.h"
#include "uuid.h"


/* ======================================================== *
 * tree sequence
 * ======================================================== */

static void
tree_sequence_check_state(tree_sequence_t *self)
{
    size_t j;
    table_size_t k, l;
    site_t site;
    site_id_t site_id = 0;

    for (j = 0; j < self->num_trees; j++) {
        for (k = 0; k < self->tree_sites_length[j]; k++) {
            site = self->tree_sites[j][k];
            assert(site.id == site_id);
            site_id++;
            for (l = 0; l < site.mutations_length; l++) {
                assert(site.mutations[l].site == site.id);
            }
        }
    }
}

void
tree_sequence_print_state(tree_sequence_t *self, FILE *out)
{
    size_t j;
    table_size_t k, l, m;
    site_t site;

    fprintf(out, "tree_sequence state\n");
    fprintf(out, "num_trees = %d\n", (int) self->num_trees);
    fprintf(out, "samples = (%d)\n", (int) self->num_samples);
    for (j = 0; j < self->num_samples; j++) {
        fprintf(out, "\t%d\n", (int) self->samples[j]);
    }
    table_collection_print_state(self->tables, out);
    fprintf(out, "tree_sites = \n");
    for (j = 0; j < self->num_trees; j++) {
        fprintf(out, "tree %d\t%d sites\n", (int) j, self->tree_sites_length[j]);
        for (k = 0; k < self->tree_sites_length[j]; k++) {
            site = self->tree_sites[j][k];
            fprintf(out, "\tsite %d pos = %f ancestral state = ", site.id, site.position);
            for (l = 0; l < site.ancestral_state_length; l++) {
                fprintf(out, "%c", site.ancestral_state[l]);
            }
            fprintf(out, " %d mutations\n", site.mutations_length);
            for (l = 0; l < site.mutations_length; l++) {
                fprintf(out, "\t\tmutation %d node = %d derived_state = ",
                        site.mutations[l].id, site.mutations[l].node);
                for (m = 0; m < site.mutations[l].derived_state_length; m++) {
                    fprintf(out, "%c", site.mutations[l].derived_state[m]);
                }
                fprintf(out, "\n");
            }
        }
    }
    tree_sequence_check_state(self);
}

int
tree_sequence_free(tree_sequence_t *self)
{
    if (self->tables != NULL) {
        table_collection_free(self->tables);
    }
    msp_safe_free(self->tables);
    msp_safe_free(self->samples);
    msp_safe_free(self->sample_index_map);
    msp_safe_free(self->tree_sites);
    msp_safe_free(self->tree_sites_length);
    msp_safe_free(self->tree_sites_mem);
    msp_safe_free(self->site_mutations_mem);
    msp_safe_free(self->site_mutations_length);
    msp_safe_free(self->site_mutations);
    msp_safe_free(self->individual_nodes_mem);
    msp_safe_free(self->individual_nodes_length);
    msp_safe_free(self->individual_nodes);
    return 0;
}

static int
tree_sequence_init_sites(tree_sequence_t *self)
{
    size_t j;
    table_size_t k;
    int ret = 0;
    size_t offset = 0;
    const table_size_t num_mutations = self->tables->mutations->num_rows;
    const table_size_t num_sites = self->tables->sites->num_rows;
    const site_id_t *restrict mutation_site = self->tables->mutations->site;

    self->site_mutations_mem = malloc(num_mutations * sizeof(mutation_t));
    self->site_mutations_length = malloc(num_sites * sizeof(table_size_t));
    self->site_mutations = malloc(num_sites * sizeof(mutation_t *));
    self->tree_sites_mem = malloc(num_sites * sizeof(site_t));
    if (self->site_mutations_mem == NULL
            || self->site_mutations_length == NULL
            || self->site_mutations == NULL
            || self->tree_sites_mem == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }

    for (k = 0; k < num_mutations; k++) {
        ret = tree_sequence_get_mutation(self, k, self->site_mutations_mem + k);
        if (ret != 0) {
            goto out;
        }
    }
    k = 0;
    for (j = 0; j < num_sites; j++) {
        self->site_mutations[j] = self->site_mutations_mem + offset;
        self->site_mutations_length[j] = 0;
        /* Go through all mutations for this site */
        while (k < num_mutations && mutation_site[k] == (site_id_t) j) {
            self->site_mutations_length[j]++;
            offset++;
            k++;
        }
        ret = tree_sequence_get_site(self, j, self->tree_sites_mem + j);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

static int
tree_sequence_init_individuals(tree_sequence_t *self)
{
    int ret = 0;
    node_id_t node;
    individual_id_t ind;
    table_size_t offset = 0;
    table_size_t total_node_refs = 0;
    table_size_t *node_count = NULL;
    node_id_t *node_array;
    const size_t num_inds = self->tables->individuals->num_rows;
    const size_t num_nodes = self->tables->nodes->num_rows;
    const individual_id_t *restrict node_individual = self->tables->nodes->individual;

    // First find number of nodes per individual
    self->individual_nodes_length = calloc(MSP_MAX(1, num_inds), sizeof(table_size_t));
    node_count = calloc(MSP_MAX(1, num_inds), sizeof(size_t));

    if (self->individual_nodes_length == NULL || node_count == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }

    for (node = 0; node < (node_id_t) num_nodes; node++) {
        ind = node_individual[node];
        if (ind != MSP_NULL_INDIVIDUAL) {
            self->individual_nodes_length[ind]++;
            total_node_refs++;
        }
    }

    self->individual_nodes_mem = malloc(MSP_MAX(1, total_node_refs) * sizeof(node_t));
    self->individual_nodes = malloc(MSP_MAX(1, num_inds) * sizeof(node_t *));
    if (self->individual_nodes_mem == NULL || self->individual_nodes == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }

    /* Now fill in the node IDs */
    for (ind = 0; ind < (individual_id_t) num_inds; ind++) {
        self->individual_nodes[ind] = self->individual_nodes_mem + offset;
        offset += self->individual_nodes_length[ind];
    }
    for (node = 0; node < (node_id_t) num_nodes; node++) {
        ind = node_individual[node];
        if (ind != MSP_NULL_INDIVIDUAL) {
            node_array = self->individual_nodes[ind];
            assert(node_array - self->individual_nodes_mem
                    < total_node_refs - node_count[ind]);
            node_array[node_count[ind]] = node;
            node_count[ind] += 1;
        }
    }
out:
    msp_safe_free(node_count);
    return ret;
}

/* Initialises memory associated with the trees.
 */
static int
tree_sequence_init_trees(tree_sequence_t *self)
{
    int ret = MSP_ERR_GENERIC;
    size_t j, k, tree_index;
    site_id_t site;
    double tree_left, tree_right;
    const double sequence_length = self->tables->sequence_length;
    const site_id_t num_sites = (site_id_t) self->tables->sites->num_rows;
    const size_t num_edges = self->tables->edges->num_rows;
    const double * restrict site_position = self->tables->sites->position;
    const node_id_t * restrict I = self->tables->indexes.edge_insertion_order;
    const node_id_t * restrict O = self->tables->indexes.edge_removal_order;
    const double * restrict edge_right = self->tables->edges->right;
    const double * restrict edge_left = self->tables->edges->left;

    tree_left = 0;
    tree_right = sequence_length;
    self->num_trees = 0;
    j = 0;
    k = 0;
    assert(I != NULL && O != NULL);
    while (j < num_edges || tree_left < sequence_length) {
        while (k < num_edges && edge_right[O[k]] == tree_left) {
            k++;
        }
        while (j < num_edges && edge_left[I[j]] == tree_left) {
            j++;
        }
        tree_right = sequence_length;
        if (j < num_edges) {
            tree_right = MSP_MIN(tree_right, edge_left[I[j]]);
        }
        if (k < num_edges) {
             tree_right = MSP_MIN(tree_right, edge_right[O[k]]);
        }
        tree_left = tree_right;
        self->num_trees++;
    }
    assert(self->num_trees > 0);

    self->tree_sites_length = malloc(self->num_trees * sizeof(table_size_t));
    self->tree_sites = malloc(self->num_trees * sizeof(site_t *));
    if (self->tree_sites == NULL || self->tree_sites_length == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    memset(self->tree_sites_length, 0, self->num_trees * sizeof(table_size_t));
    memset(self->tree_sites, 0, self->num_trees * sizeof(site_t *));

    tree_left = 0;
    tree_right = sequence_length;
    tree_index = 0;
    site = 0;
    j = 0;
    k = 0;
    while (j < num_edges || tree_left < sequence_length) {
        while (k < num_edges && edge_right[O[k]] == tree_left) {
            k++;
        }
        while (j < num_edges && edge_left[I[j]] == tree_left) {
            j++;
        }
        tree_right = sequence_length;
        if (j < num_edges) {
            tree_right = MSP_MIN(tree_right, edge_left[I[j]]);
        }
        if (k < num_edges) {
             tree_right = MSP_MIN(tree_right, edge_right[O[k]]);
        }
        self->tree_sites[tree_index] = self->tree_sites_mem + site;
        while (site < num_sites && site_position[site] < tree_right) {
            self->tree_sites_length[tree_index]++;
            site++;
        }
        tree_left = tree_right;
        tree_index++;
    }
    assert(site == num_sites);
    assert(tree_index == self->num_trees);
    ret = 0;
out:
    return ret;
}

static int
tree_sequence_init_nodes(tree_sequence_t *self)
{
    size_t j, k;
    size_t num_nodes = self->tables->nodes->num_rows;
    const uint32_t *restrict node_flags = self->tables->nodes->flags;
    int ret = 0;

    /* Determine the sample size */
    self->num_samples = 0;
    for (j = 0; j < num_nodes; j++) {
        if (!!(node_flags[j] & MSP_NODE_IS_SAMPLE)) {
            self->num_samples++;
        }
    }
    /* TODO raise an error if < 2 samples?? */
    self->samples = malloc(self->num_samples * sizeof(node_id_t));
    self->sample_index_map = malloc(num_nodes * sizeof(node_id_t));
    if (self->samples == NULL || self->sample_index_map == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    k = 0;
    for (j = 0; j < num_nodes; j++) {
        self->sample_index_map[j] = -1;
        if (!!(node_flags[j] & MSP_NODE_IS_SAMPLE)) {
            self->samples[k] = (node_id_t) j;
            self->sample_index_map[j] = (node_id_t) k;
            k++;
        }
    }
    assert(k == self->num_samples);
out:
    return ret;
}

/* TODO add a flag that allows us to use the tables directly without
 * making a copy */
int WARN_UNUSED
tree_sequence_load_tables(tree_sequence_t *self, table_collection_t *tables,
        int flags)
{
    int ret = 0;

    memset(self, 0, sizeof(*self));
    if (tables == NULL) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    self->tables = malloc(sizeof(*self->tables));
    if (self->tables == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    ret = table_collection_alloc(self->tables, MSP_ALLOC_TABLES);
    if (ret != 0) {
        goto out;
    }
    ret = table_collection_copy(tables, self->tables);
    if (ret != 0) {
        goto out;
    }
    /* TODO This should be removed: the tables should be read only. We'll
     * raise an error if the tables aren't indexed. */
    if (!!(flags & MSP_BUILD_INDEXES)) {
        ret = table_collection_build_indexes(self->tables, 0);
        if (ret != 0) {
            goto out;
        }
    }
    ret = table_collection_check_integrity(self->tables, MSP_CHECK_ALL);
    if (ret != 0) {
        goto out;
    }
    assert(table_collection_is_indexed(self->tables));

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
            ret = MSP_ERR_NO_MEMORY;
            goto out;
        }
        memcpy(self->tables->file_uuid, tables->file_uuid, TSK_UUID_SIZE + 1);
    }

    ret = tree_sequence_init_nodes(self);
    if (ret != 0) {
        goto out;
    }
    ret = tree_sequence_init_sites(self);
    if (ret != 0) {
        goto out;
    }
    ret = tree_sequence_init_individuals(self);
    if (ret != 0) {
        goto out;
    }
    ret = tree_sequence_init_trees(self);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

int WARN_UNUSED
tree_sequence_dump_tables(tree_sequence_t *self, table_collection_t *tables, int flags)
{
    int ret = 0;

    if (tables == NULL) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    if (flags & MSP_ALLOC_TABLES) {
        ret = table_collection_alloc(tables, flags);
        if (ret != 0) {
            goto out;
        }
    }
    ret = table_collection_copy(self->tables, tables);
out:
    return ret;
}

int WARN_UNUSED
tree_sequence_load(tree_sequence_t *self, const char *filename, int flags)
{
    int ret = 0;
    table_collection_t tables;
    /* TODO the implementation is wasteful here, as we don't need to allocate
     * a new table here but could load directly into the main table instead.
     * This avoids a copy in tree_sequence_load. However, we'd need to break
     * up the functionality in load_tables above a little bit */

    ret = table_collection_alloc(&tables, 0);
    if (ret != 0) {
        goto out;
    }
    ret = table_collection_load(&tables, filename, flags);
    if (ret != 0) {
        goto out;
    }
    ret = tree_sequence_load_tables(self, &tables, 0);
    if (ret != 0) {
        goto out;
    }
out:
    table_collection_free(&tables);
    return ret;
}

int WARN_UNUSED
tree_sequence_dump(tree_sequence_t *self, const char *filename, int flags)
{
    return table_collection_dump(self->tables, filename, flags);
}

/* Simple attribute getters */

double
tree_sequence_get_sequence_length(tree_sequence_t *self)
{
    return self->tables->sequence_length;
}

char *
tree_sequence_get_file_uuid(tree_sequence_t *self)
{
    return self->tables->file_uuid;
}

size_t
tree_sequence_get_num_samples(tree_sequence_t *self)
{
    return self->num_samples;
}

size_t
tree_sequence_get_num_nodes(tree_sequence_t *self)
{
    return self->tables->nodes->num_rows;
}

size_t
tree_sequence_get_num_edges(tree_sequence_t *self)
{
    return self->tables->edges->num_rows;
}

size_t
tree_sequence_get_num_migrations(tree_sequence_t *self)
{
    return self->tables->migrations->num_rows;
}

size_t
tree_sequence_get_num_sites(tree_sequence_t *self)
{
    return self->tables->sites->num_rows;
}

size_t
tree_sequence_get_num_mutations(tree_sequence_t *self)
{
    return self->tables->mutations->num_rows;
}

size_t
tree_sequence_get_num_populations(tree_sequence_t *self)
{
    return self->tables->populations->num_rows;
}

size_t
tree_sequence_get_num_individuals(tree_sequence_t *self)
{
    return self->tables->individuals->num_rows;
}

size_t
tree_sequence_get_num_provenances(tree_sequence_t *self)
{
    return self->tables->provenances->num_rows;
}

size_t
tree_sequence_get_num_trees(tree_sequence_t *self)
{
    return self->num_trees;
}

bool
tree_sequence_is_sample(tree_sequence_t *self, node_id_t u)
{
    bool ret = false;

    if (u >= 0 && u < (node_id_t) self->tables->nodes->num_rows) {
        ret = !!(self->tables->nodes->flags[u] & MSP_NODE_IS_SAMPLE);
    }
    return ret;
}

/* Accessors for records */

int WARN_UNUSED
tree_sequence_get_pairwise_diversity(tree_sequence_t *self,
    node_id_t *samples, size_t num_samples, double *pi)
{
    int ret = 0;
    sparse_tree_t *tree = NULL;
    double result, denom, n, count;
    site_t *sites;
    table_size_t j, k, num_sites;

    if (num_samples < 2 || num_samples > self->num_samples) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    n = (double) num_samples;
    tree = malloc(sizeof(sparse_tree_t));
    if (tree == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    ret = sparse_tree_alloc(tree, self, MSP_SAMPLE_COUNTS);
    if (ret != 0) {
        goto out;
    }
    ret = sparse_tree_set_tracked_samples(tree, num_samples, samples);
    if (ret != 0) {
        goto out;
    }
    /* Allocation done; move onto main algorithm. */
    result = 0.0;
    for (ret = sparse_tree_first(tree); ret == 1; ret = sparse_tree_next(tree)) {
        ret = sparse_tree_get_sites(tree, &sites, &num_sites);
        if (ret != 0) {
            goto out;
        }
        for (j = 0; j < num_sites; j++) {
            if (sites[j].mutations_length != 1) {
                ret = MSP_ERR_UNSUPPORTED_OPERATION;
                goto out;
            }
            for (k = 0; k < sites[j].mutations_length; k++) {
                count = (double) tree->num_tracked_samples[sites[j].mutations[k].node];
                result += count * (n - count);
            }
        }
    }
    if (ret != 0) {
        goto out;
    }
    denom = (n * (n - 1)) / 2.0;
    *pi = result / denom;
out:
    if (tree != NULL) {
        sparse_tree_free(tree);
        free(tree);
    }
    return ret;
}

int WARN_UNUSED
tree_sequence_get_node(tree_sequence_t *self, size_t index, node_t *node)
{
    return node_table_get_row(self->tables->nodes, index, node);
}

int WARN_UNUSED
tree_sequence_get_edge(tree_sequence_t *self, size_t index, edge_t *edge)
{
    return edge_table_get_row(self->tables->edges, index, edge);
}

int WARN_UNUSED
tree_sequence_get_migration(tree_sequence_t *self, size_t index, migration_t *migration)
{
    return migration_table_get_row(self->tables->migrations, index, migration);
}

int WARN_UNUSED
tree_sequence_get_mutation(tree_sequence_t *self, size_t index, mutation_t *mutation)
{
    return mutation_table_get_row(self->tables->mutations, index, mutation);
}

int WARN_UNUSED
tree_sequence_get_site(tree_sequence_t *self, size_t index, site_t *site)
{
    int ret = 0;

    ret = site_table_get_row(self->tables->sites, index, site);
    if (ret != 0) {
        goto out;
    }
    site->mutations = self->site_mutations[index];
    site->mutations_length = self->site_mutations_length[index];
out:
    return ret;
}

int WARN_UNUSED
tree_sequence_get_individual(tree_sequence_t *self, size_t index, individual_t *individual)
{
    int ret = 0;

    ret = individual_table_get_row(self->tables->individuals, index, individual);
    if (ret != 0) {
        goto out;
    }
    individual->nodes = self->individual_nodes[index];
    individual->nodes_length = self->individual_nodes_length[index];
out:
    return ret;
}

int WARN_UNUSED
tree_sequence_get_population(tree_sequence_t *self, size_t index,
        tmp_population_t *population)
{
    return population_table_get_row(self->tables->populations, index, population);
}

int WARN_UNUSED
tree_sequence_get_provenance(tree_sequence_t *self, size_t index, provenance_t *provenance)
{
   return provenance_table_get_row(self->tables->provenances, index, provenance);
}

int WARN_UNUSED
tree_sequence_get_samples(tree_sequence_t *self, node_id_t **samples)
{
    *samples = self->samples;
    return 0;
}

int WARN_UNUSED
tree_sequence_get_sample_index_map(tree_sequence_t *self, node_id_t **sample_index_map)
{
    *sample_index_map = self->sample_index_map;
    return 0;
}

int WARN_UNUSED
tree_sequence_simplify(tree_sequence_t *self, node_id_t *samples, size_t num_samples,
        int flags, tree_sequence_t *output, node_id_t *node_map)
{
    int ret = 0;
    table_collection_t tables;

    ret = tree_sequence_dump_tables(self, &tables, MSP_ALLOC_TABLES);
    if (ret != 0) {
        goto out;
    }
    ret = table_collection_simplify(&tables, samples, num_samples, flags, node_map);
    if (ret != 0) {
        goto out;
    }
    ret = tree_sequence_load_tables(output, &tables, MSP_BUILD_INDEXES);
out:
    table_collection_free(&tables);
    return ret;
}

/* ======================================================== *
 * Tree diff iterator.
 * ======================================================== */

int WARN_UNUSED
tree_diff_iterator_alloc(tree_diff_iterator_t *self,
        tree_sequence_t *tree_sequence)
{
    int ret = 0;

    assert(tree_sequence != NULL);
    memset(self, 0, sizeof(tree_diff_iterator_t));
    self->num_nodes = tree_sequence_get_num_nodes(tree_sequence);
    self->num_edges = tree_sequence_get_num_edges(tree_sequence);
    self->tree_sequence = tree_sequence;
    self->insertion_index = 0;
    self->removal_index = 0;
    self->tree_left = 0;
    self->tree_index = (size_t) -1;
    self->edge_list_nodes = malloc(self->num_edges * sizeof(edge_list_t));
    if (self->edge_list_nodes == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
out:
    return ret;
}

int WARN_UNUSED
tree_diff_iterator_free(tree_diff_iterator_t *self)
{
    int ret = 0;
    msp_safe_free(self->edge_list_nodes);
    return ret;
}

void
tree_diff_iterator_print_state(tree_diff_iterator_t *self, FILE *out)
{
    fprintf(out, "tree_diff_iterator state\n");
    fprintf(out, "num_edges = %d\n", (int) self->num_edges);
    fprintf(out, "insertion_index = %d\n", (int) self->insertion_index);
    fprintf(out, "removal_index = %d\n", (int) self->removal_index);
    fprintf(out, "tree_left = %f\n", self->tree_left);
    fprintf(out, "tree_index = %d\n", (int) self->tree_index);
}

int WARN_UNUSED
tree_diff_iterator_next(tree_diff_iterator_t *self, double *ret_left, double *ret_right,
        edge_list_t **edges_out, edge_list_t **edges_in)
{
    int ret = 0;
    node_id_t k;
    const double sequence_length = self->tree_sequence->tables->sequence_length;
    double left = self->tree_left;
    double right = sequence_length;
    size_t next_edge_list_node = 0;
    tree_sequence_t *s = self->tree_sequence;
    edge_list_t *out_head = NULL;
    edge_list_t *out_tail = NULL;
    edge_list_t *in_head = NULL;
    edge_list_t *in_tail = NULL;
    edge_list_t *w = NULL;
    size_t num_trees = tree_sequence_get_num_trees(s);
    const edge_table_t *edges = s->tables->edges;
    const node_id_t *insertion_order = s->tables->indexes.edge_insertion_order;
    const node_id_t *removal_order = s->tables->indexes.edge_removal_order;

    if (self->tree_index + 1 < num_trees) {
        /* First we remove the stale records */
        while (self->removal_index < self->num_edges &&
                left == edges->right[removal_order[self->removal_index]]) {
            k = removal_order[self->removal_index];
            assert(next_edge_list_node < self->num_edges);
            w = &self->edge_list_nodes[next_edge_list_node];
            next_edge_list_node++;
            w->edge.left = edges->left[k];
            w->edge.right = edges->right[k];
            w->edge.parent = edges->parent[k];
            w->edge.child = edges->child[k];
            w->next = NULL;
            if (out_head == NULL) {
                out_head = w;
                out_tail = w;
            } else {
                out_tail->next = w;
                out_tail = w;
            }
            self->removal_index++;
        }

        /* Now insert the new records */
        while (self->insertion_index < self->num_edges &&
                left == edges->left[insertion_order[self->insertion_index]]) {
            k = insertion_order[self->insertion_index];
            assert(next_edge_list_node < self->num_edges);
            w = &self->edge_list_nodes[next_edge_list_node];
            next_edge_list_node++;
            w->edge.left = edges->left[k];
            w->edge.right = edges->right[k];
            w->edge.parent = edges->parent[k];
            w->edge.child = edges->child[k];
            w->next = NULL;
            if (in_head == NULL) {
                in_head = w;
                in_tail = w;
            } else {
                in_tail->next = w;
                in_tail = w;
            }
            self->insertion_index++;
        }
        right = sequence_length;
        if (self->insertion_index < self->num_edges) {
            right = MSP_MIN(right, edges->left[
                    insertion_order[self->insertion_index]]);
        }
        if (self->removal_index < self->num_edges) {
            right = MSP_MIN(right, edges->right[
                    removal_order[self->removal_index]]);
        }
        self->tree_index++;
        ret = 1;
    }
    *edges_out = out_head;
    *edges_in = in_head;
    *ret_left = left;
    *ret_right = right;
    /* Set the left coordinate for the next tree */
    self->tree_left = right;
    return ret;
}

/* ======================================================== *
 * sparse tree
 * ======================================================== */

static int WARN_UNUSED
sparse_tree_clear(sparse_tree_t *self)
{
    int ret = 0;
    size_t j;
    node_id_t u;
    const size_t N = self->num_nodes;
    const size_t num_samples = self->tree_sequence->num_samples;
    const bool sample_counts = !!(self->flags & MSP_SAMPLE_COUNTS);
    const bool sample_lists = !!(self->flags & MSP_SAMPLE_LISTS);

    self->left = 0;
    self->right = 0;
    self->index = (size_t) -1;
    /* TODO we should profile this method to see if just doing a single loop over
     * the nodes would be more efficient than multiple memsets.
     */
    memset(self->parent, 0xff, N * sizeof(node_id_t));
    memset(self->left_child, 0xff, N * sizeof(node_id_t));
    memset(self->right_child, 0xff, N * sizeof(node_id_t));
    memset(self->left_sib, 0xff, N * sizeof(node_id_t));
    memset(self->right_sib, 0xff, N * sizeof(node_id_t));
    memset(self->above_sample, 0, N * sizeof(bool));
    if (sample_counts) {
        memset(self->num_samples, 0, N * sizeof(node_id_t));
        memset(self->marked, 0, N * sizeof(uint8_t));
        /* We can't reset the tracked samples via memset because we don't
         * know where the tracked samples are.
         */
        for (j = 0; j < self->num_nodes; j++) {
            if (! tree_sequence_is_sample(self->tree_sequence, (node_id_t) j)) {
                self->num_tracked_samples[j] = 0;
            }
        }
    }
    if (sample_lists) {
        memset(self->left_sample, 0xff, N * sizeof(node_id_t));
        memset(self->right_sample, 0xff, N * sizeof(node_id_t));
        memset(self->next_sample, 0xff, num_samples * sizeof(node_id_t));
    }
    /* Set the sample attributes */
    self->left_root = MSP_NULL_NODE;
    if (num_samples > 0) {
        self->left_root = self->samples[0];
    }
    for (j = 0; j < num_samples; j++) {
        u = self->samples[j];
        self->above_sample[u] = true;
        if (sample_counts) {
            self->num_samples[u] = 1;
        }
        if (sample_lists) {
            /* We are mapping to *indexes* into the list of samples here */
            self->left_sample[u] = (node_id_t) j;
            self->right_sample[u] = (node_id_t) j;
        }
        /* Set initial roots */
        if (j < num_samples - 1) {
            self->right_sib[u] = self->samples[j + 1];
        }
        if (j > 0) {
            self->left_sib[u] = self->samples[j - 1];
        }
    }
    return ret;
}

int WARN_UNUSED
sparse_tree_alloc(sparse_tree_t *self, tree_sequence_t *tree_sequence, int flags)
{
    int ret = MSP_ERR_NO_MEMORY;
    size_t num_samples;
    size_t num_nodes;

    memset(self, 0, sizeof(sparse_tree_t));
    if (tree_sequence == NULL) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    num_nodes = tree_sequence->tables->nodes->num_rows;
    num_samples = tree_sequence->num_samples;
    self->num_nodes = num_nodes;
    self->tree_sequence = tree_sequence;
    self->samples = tree_sequence->samples;
    self->flags = flags;
    self->parent = malloc(num_nodes * sizeof(node_id_t));
    self->left_child = malloc(num_nodes * sizeof(node_id_t));
    self->right_child = malloc(num_nodes * sizeof(node_id_t));
    self->left_sib = malloc(num_nodes * sizeof(node_id_t));
    self->right_sib = malloc(num_nodes * sizeof(node_id_t));
    self->above_sample = malloc(num_nodes * sizeof(bool));
    if (self->parent == NULL || self->left_child == NULL || self->right_child == NULL
            || self->left_sib == NULL || self->right_sib == NULL
            || self->above_sample == NULL) {
        goto out;
    }
    /* the maximum possible height of the tree is num_nodes + 1, including
     * the null value. */
    self->stack1 = malloc((num_nodes + 1) * sizeof(node_id_t));
    self->stack2 = malloc((num_nodes + 1) * sizeof(node_id_t));
    if (self->stack1 == NULL || self->stack2 == NULL) {
        goto out;
    }
    if (!!(self->flags & MSP_SAMPLE_COUNTS)) {
        self->num_samples = calloc(num_nodes, sizeof(node_id_t));
        self->num_tracked_samples = calloc(num_nodes, sizeof(node_id_t));
        self->marked = calloc(num_nodes, sizeof(uint8_t));
        if (self->num_samples == NULL || self->num_tracked_samples == NULL
                || self->marked == NULL) {
            goto out;
        }
    }
    if (!!(self->flags & MSP_SAMPLE_LISTS)) {
        self->left_sample = malloc(num_nodes * sizeof(*self->left_sample));
        self->right_sample = malloc(num_nodes * sizeof(*self->right_sample));
        self->next_sample = malloc(num_samples * sizeof(*self->next_sample));
        if (self->left_sample == NULL || self->right_sample == NULL
                || self->next_sample == NULL) {
            goto out;
        }
    }
    ret = sparse_tree_clear(self);
out:
    return ret;
}

int WARN_UNUSED
sparse_tree_free(sparse_tree_t *self)
{
    msp_safe_free(self->parent);
    msp_safe_free(self->left_child);
    msp_safe_free(self->right_child);
    msp_safe_free(self->left_sib);
    msp_safe_free(self->right_sib);
    msp_safe_free(self->above_sample);
    msp_safe_free(self->stack1);
    msp_safe_free(self->stack2);
    msp_safe_free(self->num_samples);
    msp_safe_free(self->num_tracked_samples);
    msp_safe_free(self->marked);
    msp_safe_free(self->left_sample);
    msp_safe_free(self->right_sample);
    msp_safe_free(self->next_sample);
    return 0;
}

bool
sparse_tree_has_sample_lists(sparse_tree_t *self)
{
    return !!(self->flags & MSP_SAMPLE_LISTS);
}

bool
sparse_tree_has_sample_counts(sparse_tree_t *self)
{
    return !!(self->flags & MSP_SAMPLE_COUNTS);
}

static int WARN_UNUSED
sparse_tree_reset_tracked_samples(sparse_tree_t *self)
{
    int ret = 0;

    if (!sparse_tree_has_sample_counts(self)) {
        ret = MSP_ERR_UNSUPPORTED_OPERATION;
        goto out;
    }
    memset(self->num_tracked_samples, 0, self->num_nodes * sizeof(node_id_t));
out:
    return ret;
}

int WARN_UNUSED
sparse_tree_set_tracked_samples(sparse_tree_t *self, size_t num_tracked_samples,
        node_id_t *tracked_samples)
{
    int ret = MSP_ERR_GENERIC;
    size_t j;
    node_id_t u;

    /* TODO This is not needed when the sparse tree is new. We should use the
     * state machine to check and only reset the tracked samples when needed.
     */
    ret = sparse_tree_reset_tracked_samples(self);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < num_tracked_samples; j++) {
        u = tracked_samples[j];
        if (u < 0 || u >= (node_id_t) self->num_nodes) {
            ret = MSP_ERR_OUT_OF_BOUNDS;
            goto out;
        }
        if (! tree_sequence_is_sample(self->tree_sequence, u)) {
            ret = MSP_ERR_BAD_SAMPLES;
            goto out;
        }
        if (self->num_tracked_samples[u] != 0) {
            ret = MSP_ERR_DUPLICATE_SAMPLE;
            goto out;
        }
        /* Propagate this upwards */
        while (u != MSP_NULL_NODE) {
            self->num_tracked_samples[u] += 1;
            u = self->parent[u];
        }
    }
out:
    return ret;
}

int WARN_UNUSED
sparse_tree_set_tracked_samples_from_sample_list(sparse_tree_t *self,
        sparse_tree_t *other, node_id_t node)
{
    int ret = MSP_ERR_GENERIC;
    node_id_t u, stop, index;
    const node_id_t *next = other->next_sample;
    const node_id_t *samples = other->tree_sequence->samples;

    if (! sparse_tree_has_sample_lists(other)) {
        ret = MSP_ERR_UNSUPPORTED_OPERATION;
        goto out;
    }
    /* TODO This is not needed when the sparse tree is new. We should use the
     * state machine to check and only reset the tracked samples when needed.
     */
    ret = sparse_tree_reset_tracked_samples(self);
    if (ret != 0) {
        goto out;
    }

    index = other->left_sample[node];
    if (index != MSP_NULL_NODE) {
        stop = other->right_sample[node];
        while (true) {
            u = samples[index];
            assert(self->num_tracked_samples[u] == 0);
            /* Propagate this upwards */
            while (u != MSP_NULL_NODE) {
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

int WARN_UNUSED
sparse_tree_copy(sparse_tree_t *self, sparse_tree_t *source)
{
    int ret = MSP_ERR_GENERIC;
    size_t N = self->num_nodes;

    if (self == source) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    if (self->tree_sequence != source->tree_sequence) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    self->left = source->left;
    self->right = source->right;
    self->left_root = source->left_root;
    self->index = source->index;
    self->sites = source->sites;
    self->sites_length = source->sites_length;

    memcpy(self->parent, source->parent, N * sizeof(node_id_t));
    memcpy(self->left_child, source->left_child, N * sizeof(node_id_t));
    memcpy(self->right_child, source->right_child, N * sizeof(node_id_t));
    memcpy(self->left_sib, source->left_sib, N * sizeof(node_id_t));
    memcpy(self->right_sib, source->right_sib, N * sizeof(node_id_t));
    if (self->flags & MSP_SAMPLE_COUNTS) {
        if (! (source->flags & MSP_SAMPLE_COUNTS)) {
            ret = MSP_ERR_UNSUPPORTED_OPERATION;
            goto out;
        }
        memcpy(self->num_samples, source->num_samples, N * sizeof(node_id_t));
    }
    if (self->flags & MSP_SAMPLE_LISTS) {
        ret = MSP_ERR_UNSUPPORTED_OPERATION;
        goto out;
    }
    ret = 0;
out:
    return ret;
}

/* Returns 0 if the specified sparse trees are equal, 1 if they are
 * not equal, and < 0 if an error occurs.
 *
 * We only consider topological properties of the tree. Optional
 * counts and sample lists are not considered for equality.
 */
int WARN_UNUSED
sparse_tree_equal(sparse_tree_t *self, sparse_tree_t *other)
{
    int ret = 1;
    int condition;
    size_t N = self->num_nodes;

    if (self->tree_sequence != other->tree_sequence) {
        /* It is an error to compare trees from different tree sequences. */
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    condition = self->index == other->index
        && self->left == other->left
        && self->right == other->right
        && self->sites_length == other->sites_length
        && self->sites == other->sites
        && memcmp(self->parent, other->parent, N * sizeof(node_id_t)) == 0;
    /* We do not check the children for equality here because
     * the ordering of the children within a parent are essentially irrelevant
     * in terms of topology. Depending on the way in which we approach a given
     * tree we can get different orderings within the children, and so the
     * same tree would not be equal to itself. */
    if (condition) {
        ret = 0;
    }
out:
    return ret;
}

static int
sparse_tree_check_node(sparse_tree_t *self, node_id_t u)
{
    int ret = 0;
    if (u < 0 || u >= (node_id_t) self->num_nodes) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
    }
    return ret;
}

int WARN_UNUSED
sparse_tree_get_mrca(sparse_tree_t *self, node_id_t u, node_id_t v,
        node_id_t *mrca)
{
    int ret = 0;
    node_id_t w = 0;
    node_id_t *s1 = self->stack1;
    node_id_t *s2 = self->stack2;
    node_id_t j;
    int l1, l2;

    ret = sparse_tree_check_node(self, u);
    if (ret != 0) {
        goto out;
    }
    ret = sparse_tree_check_node(self, v);
    if (ret != 0) {
        goto out;
    }
    j = u;
    l1 = 0;
    while (j != MSP_NULL_NODE) {
        assert(l1 < (int) self->num_nodes);
        s1[l1] = j;
        l1++;
        j = self->parent[j];
    }
    s1[l1] = MSP_NULL_NODE;
    j = v;
    l2 = 0;
    while (j != MSP_NULL_NODE) {
        assert(l2 < (int) self->num_nodes);
        s2[l2] = j;
        l2++;
        j = self->parent[j];
    }
    s2[l2] = MSP_NULL_NODE;
    do {
        w = s1[l1];
        l1--;
        l2--;
    } while (l1 >= 0 && l2 >= 0 && s1[l1] == s2[l2]);
    *mrca = w;
    ret = 0;
out:
    return ret;
}

static int
sparse_tree_get_num_samples_by_traversal(sparse_tree_t *self, node_id_t u,
        size_t *num_samples)
{
    int ret = 0;
    node_id_t *stack = self->stack1;
    node_id_t v;
    size_t count = 0;
    int stack_top = 0;

    stack[0] = u;
    while (stack_top >= 0) {
        v = stack[stack_top];
        stack_top--;
        if (tree_sequence_is_sample(self->tree_sequence, v)) {
            count++;
        }
        v = self->left_child[v];
        while (v != MSP_NULL_NODE) {
            stack_top++;
            stack[stack_top] = v;
            v = self->right_sib[v];
        }
    }
    *num_samples = count;
    return ret;
}

int WARN_UNUSED
sparse_tree_get_num_samples(sparse_tree_t *self, node_id_t u, size_t *num_samples)
{
    int ret = 0;

    ret = sparse_tree_check_node(self, u);
    if (ret != 0) {
        goto out;
    }

    if (self->flags & MSP_SAMPLE_COUNTS) {
        *num_samples = (size_t) self->num_samples[u];
    } else {
        ret = sparse_tree_get_num_samples_by_traversal(self, u, num_samples);
    }
out:
    return ret;
}

int WARN_UNUSED
sparse_tree_get_num_tracked_samples(sparse_tree_t *self, node_id_t u,
        size_t *num_tracked_samples)
{
    int ret = 0;

    ret = sparse_tree_check_node(self, u);
    if (ret != 0) {
        goto out;
    }
    if (! (self->flags & MSP_SAMPLE_COUNTS)) {
        ret = MSP_ERR_UNSUPPORTED_OPERATION;
        goto out;
    }
    *num_tracked_samples = (size_t) self->num_tracked_samples[u];
out:
    return ret;
}

bool
sparse_tree_is_sample(sparse_tree_t *self, node_id_t u)
{
    return tree_sequence_is_sample(self->tree_sequence, u);
}

size_t
sparse_tree_get_num_roots(sparse_tree_t *self)
{
    size_t num_roots = 0;
    node_id_t u = self->left_root;

    while (u != MSP_NULL_NODE) {
        u = self->right_sib[u];
        num_roots++;
    }
    return num_roots;
}

int WARN_UNUSED
sparse_tree_get_parent(sparse_tree_t *self, node_id_t u, node_id_t *parent)
{
    int ret = 0;

    ret = sparse_tree_check_node(self, u);
    if (ret != 0) {
        goto out;
    }
    *parent = self->parent[u];
out:
    return ret;
}

int WARN_UNUSED
sparse_tree_get_time(sparse_tree_t *self, node_id_t u, double *t)
{
    int ret = 0;
    node_t node;

    ret = tree_sequence_get_node(self->tree_sequence, (size_t) u, &node);
    if (ret != 0) {
        goto out;
    }
    *t = node.time;
out:
    return ret;
}

int WARN_UNUSED
sparse_tree_get_sites(sparse_tree_t *self, site_t **sites, table_size_t *sites_length)
{
    *sites = self->sites;
    *sites_length = self->sites_length;
    return 0;
}

int WARN_UNUSED
sparse_tree_get_newick(sparse_tree_t *self, node_id_t root, size_t precision,
        int flags, size_t buffer_size, char *newick_buffer)
{
    int ret = 0;
    newick_converter_t nc;

    ret = newick_converter_alloc(&nc, self, precision, flags);
    if (ret != 0) {
        goto out;
    }
    ret = newick_converter_run(&nc, root, buffer_size, newick_buffer);
out:
    newick_converter_free(&nc);
    return ret;
}

static void
sparse_tree_check_state(sparse_tree_t *self)
{
    node_id_t u, v;
    size_t j, num_samples;
    int err, c;
    site_t site;
    node_id_t *children = malloc(self->num_nodes * sizeof(node_id_t));
    bool *is_root = calloc(self->num_nodes, sizeof(bool));

    assert(children != NULL);

    for (j = 0; j < self->tree_sequence->num_samples; j++) {
        u = self->samples[j];
        while (self->parent[u] != MSP_NULL_NODE) {
            u = self->parent[u];
        }
        is_root[u] = true;
    }
    if (self->tree_sequence->num_samples == 0) {
        assert(self->left_root == MSP_NULL_NODE);
    } else {
        assert(self->left_sib[self->left_root] == MSP_NULL_NODE);
    }
    /* Iterate over the roots and make sure they are set */
    for (u = self->left_root; u != MSP_NULL_NODE; u = self->right_sib[u]) {
        assert(is_root[u]);
        is_root[u] = false;
    }
    for (u = 0; u < (node_id_t) self->num_nodes; u++) {
        assert(!is_root[u]);
        c = 0;
        for (v = self->left_child[u]; v != MSP_NULL_NODE; v = self->right_sib[v]) {
            assert(self->parent[v] == u);
            children[c] = v;
            c++;
        }
        for (v = self->right_child[u]; v != MSP_NULL_NODE; v = self->left_sib[v]) {
            assert(c > 0);
            c--;
            assert(v == children[c]);
        }
    }
    for (j = 0; j < self->sites_length; j++) {
        site = self->sites[j];
        assert(self->left <= site.position);
        assert(site.position < self->right);
    }

    if (self->flags & MSP_SAMPLE_COUNTS) {
        assert(self->num_samples != NULL);
        assert(self->num_tracked_samples != NULL);
        for (u = 0; u < (node_id_t) self->num_nodes; u++) {
            err = sparse_tree_get_num_samples_by_traversal(self, u, &num_samples);
            assert(err == 0);
            assert(num_samples == (size_t) self->num_samples[u]);
        }
    } else {
        assert(self->num_samples == NULL);
        assert(self->num_tracked_samples == NULL);
    }
    if (self->flags & MSP_SAMPLE_LISTS) {
        assert(self->right_sample != NULL);
        assert(self->left_sample != NULL);
        assert(self->next_sample != NULL);
    } else {
        assert(self->right_sample == NULL);
        assert(self->left_sample == NULL);
        assert(self->next_sample == NULL);
    }

    free(children);
    free(is_root);
}

void
sparse_tree_print_state(sparse_tree_t *self, FILE *out)
{
    size_t j;
    site_t site;

    fprintf(out, "Sparse tree state:\n");
    fprintf(out, "flags = %d\n", self->flags);
    fprintf(out, "left = %f\n", self->left);
    fprintf(out, "right = %f\n", self->right);
    fprintf(out, "left_root = %d\n", (int) self->left_root);
    fprintf(out, "index = %d\n", (int) self->index);
    fprintf(out, "node\tparent\tlchild\trchild\tlsib\trsib");
    if (self->flags & MSP_SAMPLE_LISTS) {
        fprintf(out, "\thead\ttail");
    }
    fprintf(out, "\n");

    for (j = 0; j < self->num_nodes; j++) {
        fprintf(out, "%d\t%d\t%d\t%d\t%d\t%d", (int) j, self->parent[j], self->left_child[j],
                self->right_child[j], self->left_sib[j], self->right_sib[j]);
        if (self->flags & MSP_SAMPLE_LISTS) {
            fprintf(out, "\t%d\t%d\t", self->left_sample[j],
                    self->right_sample[j]);
        }
        if (self->flags & MSP_SAMPLE_COUNTS) {
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
    sparse_tree_check_state(self);
}

/* Methods for positioning the tree along the sequence */

static inline void
sparse_tree_propagate_sample_count_loss(sparse_tree_t *self, node_id_t parent,
        node_id_t child)
{
    node_id_t v;
    const node_id_t all_samples_diff = self->num_samples[child];
    const node_id_t tracked_samples_diff = self->num_tracked_samples[child];
    const uint8_t mark = self->mark;
    const node_id_t * restrict tree_parent = self->parent;
    node_id_t * restrict num_samples = self->num_samples;
    node_id_t * restrict num_tracked_samples = self->num_tracked_samples;
    uint8_t * restrict marked = self->marked;

    /* propagate this loss up as far as we can */
    v = parent;
    while (v != MSP_NULL_NODE) {
        num_samples[v] -= all_samples_diff;
        num_tracked_samples[v] -= tracked_samples_diff;
        marked[v] = mark;
        v = tree_parent[v];
    }
}

static inline void
sparse_tree_propagate_sample_count_gain(sparse_tree_t *self, node_id_t parent,
        node_id_t child)
{
    node_id_t v;
    const node_id_t all_samples_diff = self->num_samples[child];
    const node_id_t tracked_samples_diff = self->num_tracked_samples[child];
    const uint8_t mark = self->mark;
    const node_id_t * restrict tree_parent = self->parent;
    node_id_t * restrict num_samples = self->num_samples;
    node_id_t * restrict num_tracked_samples = self->num_tracked_samples;
    uint8_t * restrict marked = self->marked;

    /* propogate this gain up as far as we can */
    v = parent;
    while (v != MSP_NULL_NODE) {
        num_samples[v] += all_samples_diff;
        num_tracked_samples[v] += tracked_samples_diff;
        marked[v] = mark;
        v = tree_parent[v];
    }
}

static inline void
sparse_tree_update_sample_lists(sparse_tree_t *self, node_id_t node)
{
    node_id_t u, v, sample_index;
    node_id_t * restrict left = self->left_sample;
    node_id_t * restrict right = self->right_sample;
    node_id_t * restrict next = self->next_sample;
    const node_id_t * restrict left_child = self->left_child;
    const node_id_t * restrict right_sib = self->right_sib;
    const node_id_t * restrict parent = self->parent;
    const node_id_t * restrict sample_index_map = self->tree_sequence->sample_index_map;

    for (u = node; u != MSP_NULL_NODE; u = parent[u]) {
        sample_index = sample_index_map[u];
        if (sample_index != MSP_NULL_NODE) {
            right[u] = left[u];
        } else {
            left[u] = MSP_NULL_NODE;
            right[u] = MSP_NULL_NODE;
        }
        for (v = left_child[u]; v != MSP_NULL_NODE; v = right_sib[v]) {
            if (left[v] != MSP_NULL_NODE) {
                assert(right[v] != MSP_NULL_NODE);
                if (left[u] == MSP_NULL_NODE) {
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

static int
sparse_tree_advance(sparse_tree_t *self, int direction,
        const double * restrict out_breakpoints,
        const node_id_t * restrict out_order,
        node_id_t *out_index,
        const double * restrict in_breakpoints,
        const node_id_t * restrict in_order,
        node_id_t *in_index)
{
    int ret = 0;
    const int direction_change = direction * (direction != self->direction);
    node_id_t in = *in_index + direction_change;
    node_id_t out = *out_index + direction_change;
    node_id_t k, p, c, u, v, root, lsib, rsib, lroot, rroot;
    table_collection_t *tables = self->tree_sequence->tables;
    const double sequence_length = tables->sequence_length;
    const node_id_t num_edges = (node_id_t) tables->edges->num_rows;
    const node_id_t * restrict edge_parent = tables->edges->parent;
    const node_id_t * restrict edge_child = tables->edges->child;
    const uint32_t * restrict node_flags = tables->nodes->flags;
    double x;
    bool above_sample;

    if (direction == MSP_DIR_FORWARD) {
        x = self->right;
    } else {
        x = self->left;
    }
    while (out >= 0 && out < num_edges && out_breakpoints[out_order[out]] == x) {
        assert(out < num_edges);
        k = out_order[out];
        out += direction;
        p = edge_parent[k];
        c = edge_child[k];
        lsib = self->left_sib[c];
        rsib = self->right_sib[c];
        if (lsib == MSP_NULL_NODE) {
            self->left_child[p] = rsib;
        } else {
            self->right_sib[lsib] = rsib;
        }
        if (rsib == MSP_NULL_NODE) {
            self->right_child[p] = lsib;
        } else {
            self->left_sib[rsib] = lsib;
        }
        self->parent[c] = MSP_NULL_NODE;
        self->left_sib[c] = MSP_NULL_NODE;
        self->right_sib[c] = MSP_NULL_NODE;
        if (self->flags & MSP_SAMPLE_COUNTS) {
            sparse_tree_propagate_sample_count_loss(self, p, c);
        }
        if (self->flags & MSP_SAMPLE_LISTS) {
            sparse_tree_update_sample_lists(self, p);
        }

        /* Update the roots. If c is not above a sample then we have nothing to do
         * as we cannot affect the status of any roots. */
        if (self->above_sample[c]) {
            /* Compute the new above sample status for the nodes from p up to root. */
            v = p;
            root = v;
            above_sample = false;
            while (v != MSP_NULL_NODE && !above_sample) {
                above_sample = !!(node_flags[v] & MSP_NODE_IS_SAMPLE);
                u = self->left_child[v];
                while (u != MSP_NULL_NODE && !above_sample) {
                    above_sample = above_sample || self->above_sample[u];
                    u = self->right_sib[u];
                }
                self->above_sample[v] = above_sample;
                root = v;
                v = self->parent[v];
            }
            if (!above_sample) {
                /* root is no longer above samples. Remove it from the root list */
                lroot = self->left_sib[root];
                rroot = self->right_sib[root];
                self->left_root = MSP_NULL_NODE;
                if (lroot != MSP_NULL_NODE) {
                    self->right_sib[lroot] = rroot;
                    self->left_root = lroot;
                }
                if (rroot != MSP_NULL_NODE) {
                    self->left_sib[rroot] = lroot;
                    self->left_root = rroot;
                }
                self->left_sib[root] = MSP_NULL_NODE;
                self->right_sib[root] = MSP_NULL_NODE;
            }
            /* Add c to the root list */
            if (self->left_root != MSP_NULL_NODE) {
                lroot = self->left_sib[self->left_root];
                if (lroot != MSP_NULL_NODE) {
                    self->right_sib[lroot] = c;
                }
                self->left_sib[c] = lroot;
                self->left_sib[self->left_root] = c;
            }
            self->right_sib[c] = self->left_root;
            self->left_root = c;
        }
    }

    while (in >= 0 && in < num_edges && in_breakpoints[in_order[in]] == x) {
        k = in_order[in];
        in += direction;
        p = edge_parent[k];
        c = edge_child[k];
        if (self->parent[c] != MSP_NULL_NODE) {
            ret = MSP_ERR_BAD_EDGESET_CONTRADICTORY_CHILDREN;
            goto out;
        }
        self->parent[c] = p;
        u = self->right_child[p];
        lsib = self->left_sib[c];
        rsib = self->right_sib[c];
        if (u == MSP_NULL_NODE) {
            self->left_child[p] = c;
            self->left_sib[c] = MSP_NULL_NODE;
            self->right_sib[c] = MSP_NULL_NODE;
        } else {
            self->right_sib[u] = c;
            self->left_sib[c] = u;
            self->right_sib[c] = MSP_NULL_NODE;
        }
        self->right_child[p] = c;
        if (self->flags & MSP_SAMPLE_COUNTS) {
            sparse_tree_propagate_sample_count_gain(self, p, c);
        }
        if (self->flags & MSP_SAMPLE_LISTS) {
            sparse_tree_update_sample_lists(self, p);
        }

        /* Update the roots. */
        if (self->above_sample[c]) {
            v = p;
            root = v;
            above_sample = false;
            while (v != MSP_NULL_NODE && !above_sample) {
                above_sample = self->above_sample[v];
                self->above_sample[v] = self->above_sample[v] || self->above_sample[c];
                root = v;
                v = self->parent[v];
            }
            if (! above_sample) {
                /* Replace c with root in root list */
                if (lsib != MSP_NULL_NODE) {
                    self->right_sib[lsib] = root;
                }
                if (rsib != MSP_NULL_NODE) {
                    self->left_sib[rsib] = root;
                }
                self->left_sib[root] = lsib;
                self->right_sib[root] = rsib;
                self->left_root = root;
            } else {
                /* Remove c from root list */
                self->left_root = MSP_NULL_NODE;
                if (lsib != MSP_NULL_NODE) {
                    self->right_sib[lsib] = rsib;
                    self->left_root = lsib;
                }
                if (rsib != MSP_NULL_NODE) {
                    self->left_sib[rsib] = lsib;
                    self->left_root = rsib;
                }
            }
        }
    }

    if (self->left_root != MSP_NULL_NODE) {
        /* Ensure that left_root is the left-most root */
        while (self->left_sib[self->left_root] != MSP_NULL_NODE) {
            self->left_root = self->left_sib[self->left_root];
        }
    }

    self->direction = direction;
    self->index = (size_t) ((int64_t) self->index + direction);
    if (direction == MSP_DIR_FORWARD) {
        self->left = x;
        self->right = sequence_length;
        if (out >= 0 && out < num_edges) {
            self->right = MSP_MIN(self->right, out_breakpoints[out_order[out]]);
        }
        if (in >= 0 && in < num_edges) {
            self->right = MSP_MIN(self->right, in_breakpoints[in_order[in]]);
        }
    } else {
        self->right = x;
        self->left = 0;
        if (out >= 0 && out < num_edges) {
            self->left = MSP_MAX(self->left, out_breakpoints[out_order[out]]);
        }
        if (in >= 0 && in < num_edges) {
            self->left = MSP_MAX(self->left, in_breakpoints[in_order[in]]);
        }
    }
    assert(self->left < self->right);
    *out_index = out;
    *in_index = in;
    if (tables->sites->num_rows > 0) {
        self->sites = self->tree_sequence->tree_sites[self->index];
        self->sites_length = self->tree_sequence->tree_sites_length[self->index];
    }
    ret = 1;
out:
    return ret;
}

int WARN_UNUSED
sparse_tree_first(sparse_tree_t *self)
{
    int ret = 1;
    table_collection_t *tables = self->tree_sequence->tables;

    self->left = 0;
    self->index = 0;
    self->right = tables->sequence_length;
    self->sites = self->tree_sequence->tree_sites[0];
    self->sites_length = self->tree_sequence->tree_sites_length[0];

    if (tables->edges->num_rows > 0) {
        /* TODO this is redundant if this is the first usage of the tree. We
         * should add a state machine here so we know what state the tree is
         * in and can take the appropriate actions.
         */
        ret = sparse_tree_clear(self);
        if (ret != 0) {
            goto out;
        }
        self->index = (size_t) -1;
        self->left_index = 0;
        self->right_index = 0;
        self->direction = MSP_DIR_FORWARD;
        self->right = 0;

        ret = sparse_tree_advance(self, MSP_DIR_FORWARD,
                tables->edges->right, tables->indexes.edge_removal_order,
                &self->right_index, tables->edges->left,
                tables->indexes.edge_insertion_order, &self->left_index);
    }
out:
    return ret;
}

int WARN_UNUSED
sparse_tree_last(sparse_tree_t *self)
{
    int ret = 1;
    tree_sequence_t *ts = self->tree_sequence;
    const table_collection_t *tables = ts->tables;

    self->left = 0;
    self->right = tables->sequence_length;
    self->index = 0;
    self->sites = ts->tree_sites[0];
    self->sites_length = ts->tree_sites_length[0];

    if (tables->edges->num_rows > 0) {
        /* TODO this is redundant if this is the first usage of the tree. We
         * should add a state machine here so we know what state the tree is
         * in and can take the appropriate actions.
         */
        ret = sparse_tree_clear(self);
        if (ret != 0) {
            goto out;
        }
        self->index = tree_sequence_get_num_trees(ts);
        self->left_index = (node_id_t) tables->edges->num_rows - 1;
        self->right_index = (node_id_t) tables->edges->num_rows - 1;
        self->direction = MSP_DIR_REVERSE;
        self->left = tables->sequence_length;
        self->right = 0;

        ret = sparse_tree_advance(self, MSP_DIR_REVERSE,
                tables->edges->left, tables->indexes.edge_insertion_order,
                &self->left_index, tables->edges->right,
                tables->indexes.edge_removal_order, &self->right_index);
    }
out:
    return ret;
}

int WARN_UNUSED
sparse_tree_next(sparse_tree_t *self)
{
    int ret = 0;
    tree_sequence_t *ts = self->tree_sequence;
    const table_collection_t *tables = ts->tables;
    size_t num_trees = tree_sequence_get_num_trees(ts);

    if (self->index < num_trees - 1) {
        ret = sparse_tree_advance(self, MSP_DIR_FORWARD,
                tables->edges->right, tables->indexes.edge_removal_order,
                &self->right_index, tables->edges->left,
                tables->indexes.edge_insertion_order, &self->left_index);
    }
    return ret;
}

int WARN_UNUSED
sparse_tree_prev(sparse_tree_t *self)
{
    int ret = 0;
    const table_collection_t *tables = self->tree_sequence->tables;

    if (self->index > 0) {
        ret = sparse_tree_advance(self, MSP_DIR_REVERSE,
                tables->edges->left, tables->indexes.edge_insertion_order,
                &self->left_index, tables->edges->right,
                tables->indexes.edge_removal_order, &self->right_index);
    }
    return ret;
}
