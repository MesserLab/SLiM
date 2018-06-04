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
        for (k = 0; k < self->sites.tree_sites_length[j]; k++) {
            site = self->sites.tree_sites[j][k];
            assert(site.id == site_id);
            site_id++;
            for (l = 0; l < site.mutations_length; l++) {
                assert(site.mutations[l].site == site.id);
            }
        }
    }
    assert(self->nodes.metadata_offset[0] == 0);
    assert(self->sites.ancestral_state_offset[0] == 0);
    assert(self->sites.metadata_offset[0] == 0);
    assert(self->mutations.derived_state_offset[0] == 0);
    assert(self->mutations.metadata_offset[0] == 0);
}

void
tree_sequence_print_state(tree_sequence_t *self, FILE *out)
{
    size_t j;
    table_size_t k, l, m;
    site_t site;

    fprintf(out, "tree_sequence state\n");
    fprintf(out, "num_trees = %d\n", (int) self->num_trees);
    fprintf(out, "sequence_length = %f\n", self->sequence_length);
    fprintf(out, "samples = (%d)\n", (int) self->num_samples);
    for (j = 0; j < self->num_samples; j++) {
        fprintf(out, "\t%d\n", (int) self->samples[j]);
    }
    fprintf(out, "provenance = (%d)\n", (int) self->provenances.num_records);
    for (j = 0; j < self->provenances.num_records; j++) {
        fprintf(out, "\t");
        for (k = self->provenances.timestamp_offset[j];
                k < self->provenances.timestamp_offset[j + 1]; k++) {
            fprintf(out, "%c", self->provenances.timestamp[k]);
        }
        fprintf(out, "\t");
        for (k = self->provenances.record_offset[j];
                k < self->provenances.record_offset[j + 1]; k++) {
            fprintf(out, "%c", self->provenances.record[k]);
        }
        fprintf(out, "\n");
    }
    fprintf(out, "populations (%d)\n", (int) self->populations.num_records);
    for (j = 0; j < self->populations.num_records; j++) {
        fprintf(out, "\t%d\t", (int) j);
        for (k = self->populations.metadata_offset[j];
                k < self->populations.metadata_offset[j + 1]; k++) {
            fprintf(out, "%c", self->populations.metadata[k]);
        }
        fprintf(out, "\n");
    }
    fprintf(out, "individuals (%d)\n", (int) self->individuals.num_records);
    for (j = 0; j < self->individuals.num_records; j++) {
        fprintf(out, "\t%d\t%d\t", (int) j, self->individuals.flags[j]);
        for (k = self->individuals.location_offset[j];
                k < self->individuals.location_offset[j + 1]; k++) {
            fprintf(out, "%f,", self->individuals.location[k]);
        }
        fprintf(out, "\t");
        for (k = self->individuals.metadata_offset[j];
                k < self->individuals.metadata_offset[j + 1]; k++) {
            fprintf(out, "%c", self->individuals.metadata[k]);
        }
        fprintf(out, "\n");
    }
    fprintf(out, "nodes (%d)\n", (int) self->nodes.num_records);
    for (j = 0; j < self->nodes.num_records; j++) {
        fprintf(out, "\t%d\t%d\t%d\t%d\t%f\t%d\t", (int) j,
                self->nodes.flags[j],
                (int) self->nodes.population[j],
                (int) self->nodes.individual[j],
                self->nodes.time[j],
                self->nodes.sample_index_map[j]);
        for (k = self->nodes.metadata_offset[j]; k < self->nodes.metadata_offset[j + 1]; k++) {
            fprintf(out, "%c", self->nodes.metadata[k]);
        }
        fprintf(out, "\n");
    }
    fprintf(out, "edges = (%d records)\n", (int) self->edges.num_records);
    for (j = 0; j < self->edges.num_records; j++) {
        fprintf(out, "\t%d\t%f\t%f\t%d\t%d",
                (int) j,
                self->edges.left[j],
                self->edges.right[j],
                self->edges.parent[j],
                self->edges.child[j]);
        fprintf(out, "\t|\t%d\t%d\n",
                (int) self->edges.indexes.insertion_order[j],
                (int) self->edges.indexes.removal_order[j]);
    }
    fprintf(out, "sites = (%d records)\n", (int) self->sites.num_records);
    for (j = 0; j < self->sites.num_records; j++) {
        fprintf(out, "\t%d\t%f\t", (int) j, self->sites.position[j]);
        for (k = self->sites.ancestral_state_offset[j];
                k < self->sites.ancestral_state_offset[j + 1]; k++) {
            fprintf(out, "%c", self->sites.ancestral_state[k]);
        }
        fprintf(out, "\t");
        for (k = self->sites.metadata_offset[j];
                k < self->sites.metadata_offset[j + 1]; k++) {
            fprintf(out, "%c", self->sites.metadata[k]);
        }
        fprintf(out, "\n");
    }
    fprintf(out, "mutations = (%d records)\n", (int) self->mutations.num_records);
    for (j = 0; j < self->mutations.num_records; j++) {
        fprintf(out, "\t%d\t%d\t%d\t%d\t", (int) j, self->mutations.site[j],
                self->mutations.node[j], self->mutations.parent[j]);
        for (k = self->mutations.derived_state_offset[j];
                k < self->mutations.derived_state_offset[j + 1]; k++) {
            fprintf(out, "%c", self->mutations.derived_state[k]);
        }
        fprintf(out, "\t");
        for (k = self->mutations.metadata_offset[j];
                k < self->mutations.metadata_offset[j + 1]; k++) {
            fprintf(out, "%c", self->mutations.metadata[k]);
        }
        fprintf(out, "\n");
    }
    fprintf(out, "migrations.records = (%d records)\n",
            (int) self->migrations.num_records);
    for (j = 0; j < self->migrations.num_records; j++) {
        fprintf(out, "\t%d\t%f\t%f\t%d\t%d\t%d\t%f\n", (int) j,
                self->migrations.left[j],
                self->migrations.right[j],
                (int) self->migrations.node[j],
                (int) self->migrations.source[j],
                (int) self->migrations.dest[j],
                self->migrations.time[j]);
    }
    fprintf(out, "tree_sites = \n");
    for (j = 0; j < self->num_trees; j++) {
        fprintf(out, "tree %d\t%d sites\n", (int) j, self->sites.tree_sites_length[j]);
        for (k = 0; k < self->sites.tree_sites_length[j]; k++) {
            site = self->sites.tree_sites[j][k];
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

    fprintf(out, "memory\n");
    fprintf(out, "\tnum_samples = %d\n", (int) self->num_samples);
    fprintf(out, "\tnodes.num_records = %d\n", (int) self->nodes.num_records);
    fprintf(out, "\tedges.num_records = %d\n", (int) self->edges.num_records);
    fprintf(out, "\tmutations.num_records = %d\n", (int) self->mutations.num_records);
    fprintf(out, "\tmutations.derived_state_length = %d\n",
            (int) self->mutations.derived_state_length);
    fprintf(out, "\tmigrations.num_records = %d\n", (int) self->migrations.num_records);

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
    msp_safe_free(self->nodes.sample_index_map);
    msp_safe_free(self->sites.tree_sites);
    msp_safe_free(self->sites.tree_sites_length);
    msp_safe_free(self->sites.tree_sites_mem);
    msp_safe_free(self->sites.site_mutations_mem);
    msp_safe_free(self->sites.site_mutations_length);
    msp_safe_free(self->sites.site_mutations);
    msp_safe_free(self->individuals.individual_nodes_mem);
    msp_safe_free(self->individuals.individual_nodes_length);
    msp_safe_free(self->individuals.individual_nodes);
    return 0;
}

static int
tree_sequence_check(tree_sequence_t *self)
{
    int ret = MSP_ERR_GENERIC;
    node_id_t child, parent, last_parent, last_child;
    mutation_id_t parent_mut;
    size_t j;
    double left, last_left;
    double *time = self->nodes.time;
    bool *parent_seen = calloc(self->nodes.num_records, sizeof(bool));

    if (parent_seen == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }

    for (j = 0; j < self->edges.num_records; j++) {
        parent = self->edges.parent[j];
        child = self->edges.child[j];
        left = self->edges.left[j];
        if (parent == MSP_NULL_NODE) {
            ret = MSP_ERR_NULL_PARENT;
            goto out;
        }
        if (parent < 0 || parent >= (node_id_t) self->nodes.num_records) {
            ret = MSP_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        if (parent_seen[parent]) {
            ret = MSP_ERR_EDGES_NONCONTIGUOUS_PARENTS;
            goto out;
        }
        if (j > 0) {
            last_parent = self->edges.parent[j - 1];
            last_child = self->edges.child[j - 1];
            last_left = self->edges.left[j - 1];
            /* Input data must sorted by (time[parent], parent, child, left). */
            if (time[parent] < time[last_parent]) {
                ret = MSP_ERR_EDGES_NOT_SORTED_PARENT_TIME;
                goto out;
            }
            if (time[parent] == time[last_parent]) {
                /* if (parent < last_parent) { */
                /*     ret = MSP_ERR_EDGES_NOT_SORTED_PARENT; */
                /*     goto out; */
                /* } */
                if (parent == last_parent) {
                    if (child < last_child) {
                        ret = MSP_ERR_EDGES_NOT_SORTED_CHILD;
                        goto out;
                    }
                    if (child == last_child) {
                        if (left == last_left) {
                            ret = MSP_ERR_DUPLICATE_EDGES;
                            goto out;
                        } else if (left < last_left) {
                            ret = MSP_ERR_EDGES_NOT_SORTED_LEFT;
                            goto out;
                        }
                    }
                } else {
                    parent_seen[last_parent] = true;
                }
            }
        }
        if (child == MSP_NULL_NODE) {
            ret = MSP_ERR_NULL_CHILD;
            goto out;
        }
        if (child < 0 || child >= (node_id_t) self->nodes.num_records) {
            ret = MSP_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        /* time[child] must be < time[parent] */
        if (time[child] >= time[parent]) {
            ret = MSP_ERR_BAD_NODE_TIME_ORDERING;
            goto out;
        }
        if (self->edges.left[j] >= self->edges.right[j]) {
            ret = MSP_ERR_BAD_EDGE_INTERVAL;
            goto out;
        }
        if (self->edges.right[j] > self->sequence_length) {
            ret = MSP_ERR_RIGHT_GREATER_SEQ_LENGTH;
            goto out;
        }
    }

    /* Check the sites */
    for (j = 0; j < self->sites.num_records; j++) {
        if (self->sites.position[j] < 0
                || self->sites.position[j] >= self->sequence_length) {
            ret = MSP_ERR_BAD_SITE_POSITION;
            goto out;
        }
        if (j > 0) {
            if (self->sites.position[j - 1] >= self->sites.position[j]) {
                ret = MSP_ERR_UNSORTED_SITES;
                goto out;
            }
        }
    }
    for (j = 0; j < self->mutations.num_records; j++) {
        if (self->mutations.site[j] < 0
                || self->mutations.site[j] >= (mutation_id_t) self->sites.num_records) {
            ret = MSP_ERR_SITE_OUT_OF_BOUNDS;
            goto out;
        }
        if (self->mutations.node[j] < 0
                || self->mutations.node[j] >= (node_id_t) self->nodes.num_records) {
            ret = MSP_ERR_NODE_OUT_OF_BOUNDS;
            goto out;
        }
        parent_mut = self->mutations.parent[j];
        if (parent_mut < MSP_NULL_MUTATION
                || parent_mut >= (mutation_id_t) self->mutations.num_records) {
            ret = MSP_ERR_MUTATION_OUT_OF_BOUNDS;
            goto out;
        }
        if (parent_mut == (mutation_id_t) j) {
            ret = MSP_ERR_MUTATION_PARENT_EQUAL;
            goto out;
        }
        if (parent_mut != MSP_NULL_MUTATION) {
            /* Parents must be listed before their children */
            if (parent_mut > (mutation_id_t) j) {
                ret = MSP_ERR_MUTATION_PARENT_AFTER_CHILD;
                goto out;
            }
            if (self->mutations.site[parent_mut] != self->mutations.site[j]) {
                ret = MSP_ERR_MUTATION_PARENT_DIFFERENT_SITE;
                goto out;
            }
        }
        if (j > 0) {
            if (self->mutations.site[j - 1] > self->mutations.site[j]) {
                ret = MSP_ERR_UNSORTED_MUTATIONS;
                goto out;
            }
        }
    }
    ret = 0;
out:
    msp_safe_free(parent_seen);
    return ret;
}

static int
tree_sequence_init_sites(tree_sequence_t *self)
{
    site_id_t j;
    table_size_t k;
    int ret = 0;
    size_t offset = 0;

    self->sites.site_mutations_mem = malloc(
            self->mutations.num_records * sizeof(mutation_t));
    self->sites.site_mutations_length = malloc(
            self->sites.num_records * sizeof(table_size_t));
    self->sites.site_mutations = malloc(
            self->sites.num_records * sizeof(mutation_t *));
    self->sites.tree_sites_mem = malloc(
            self->sites.num_records * sizeof(site_t));
    if (self->sites.site_mutations_mem == NULL
            || self->sites.site_mutations_length == NULL
            || self->sites.site_mutations == NULL
            || self->sites.tree_sites_mem == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }

    for (k = 0; k < (table_size_t) self->mutations.num_records; k++) {
        ret = tree_sequence_get_mutation(self, (mutation_id_t) k,
                self->sites.site_mutations_mem + k);
        if (ret != 0) {
            goto out;
        }
    }
    k = 0;
    for (j = 0; j < (site_id_t) self->sites.num_records; j++) {
        if (self->sites.position[j] < 0
                || self->sites.position[j] >= self->sequence_length) {
            ret = MSP_ERR_BAD_SITE_POSITION;
            goto out;
        }
        if (j > 1) {
            if (self->sites.position[j - 1] >= self->sites.position[j]) {
                ret = MSP_ERR_UNSORTED_SITES;
                goto out;
            }
        }
        self->sites.site_mutations[j] = self->sites.site_mutations_mem + offset;
        self->sites.site_mutations_length[j] = 0;
        /* Go through all mutations for this site */
        while (k < (table_size_t) self->mutations.num_records
                && self->mutations.site[k] == j) {
            self->sites.site_mutations_length[j]++;
            offset++;
            k++;
        }
        ret = tree_sequence_get_site(self, j, self->sites.tree_sites_mem + j);
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
    individual_id_t j;
    node_id_t k;
    table_size_t offset = 0;
    table_size_t total_nodes = 0;
    table_size_t *num_nodes;
    node_id_t *node_array;
    size_t num_inds = self->individuals.num_records;

    // First find number of nodes per individual
    // TODO: if nodes for each individual were contiguous
    // this would require just one pass, not two
    self->individuals.individual_nodes_length = calloc(
            MSP_MAX(1, num_inds), sizeof(table_size_t));
    num_nodes = calloc(MSP_MAX(1, num_inds), sizeof(size_t));
    if (self->individuals.individual_nodes_length == NULL
            || num_nodes == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }

    for (k = 0; k < (node_id_t) self->nodes.num_records; k++) {
        j = self->nodes.individual[k];
        if (j != MSP_NULL_INDIVIDUAL) {
            if (j >= (individual_id_t) num_inds) {
                ret = MSP_ERR_BAD_INDIVIDUAL;
                goto out;
            }
            self->individuals.individual_nodes_length[j] += 1;
        }
        total_nodes++;
    }

    // now fill in the node IDs
    self->individuals.individual_nodes_mem = malloc(
            MSP_MAX(total_nodes, 1) * sizeof(node_id_t));
    self->individuals.individual_nodes = malloc(
            MSP_MAX(1, num_inds) * sizeof(node_id_t *));
    if (self->individuals.individual_nodes_mem == NULL
            || self->individuals.individual_nodes == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }

    for (j = 0; j < (individual_id_t) num_inds; j++) {
        self->individuals.individual_nodes[j] = self->individuals.individual_nodes_mem + offset;
        offset += self->individuals.individual_nodes_length[j];
    }

    for (k = 0; k < (node_id_t) self->nodes.num_records; k++) {
        j = self->nodes.individual[k];
        if (j != MSP_NULL_INDIVIDUAL) {
            node_array = self->individuals.individual_nodes[j];
            assert(node_array - self->individuals.individual_nodes_mem
                    < total_nodes - num_nodes[j]);
            node_array[num_nodes[j]] = k;
            num_nodes[j] += 1;
        }
    }
out:
    msp_safe_free(num_nodes);
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
    node_id_t *I = self->edges.indexes.insertion_order;
    node_id_t *O = self->edges.indexes.removal_order;

    tree_left = 0;
    tree_right = self->sequence_length;
    self->num_trees = 0;
    j = 0;
    k = 0;
    assert(I != NULL && O != NULL);
    while (j < self->edges.num_records || tree_left < self->sequence_length) {
        while (k < self->edges.num_records && self->edges.right[O[k]] == tree_left) {
            k++;
        }
        while (j < self->edges.num_records && self->edges.left[I[j]] == tree_left) {
            j++;
        }
        tree_right = self->sequence_length;
        if (j < self->edges.num_records) {
            tree_right = MSP_MIN(tree_right, self->edges.left[I[j]]);
        }
        if (k < self->edges.num_records) {
             tree_right = MSP_MIN(tree_right, self->edges.right[O[k]]);
        }
        tree_left = tree_right;
        self->num_trees++;
    }
    assert(self->num_trees > 0);

    self->sites.tree_sites_length = malloc(self->num_trees * sizeof(table_size_t));
    self->sites.tree_sites = malloc(self->num_trees * sizeof(site_t *));
    if (self->sites.tree_sites == NULL || self->sites.tree_sites_length == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    memset(self->sites.tree_sites_length, 0, self->num_trees * sizeof(table_size_t));
    memset(self->sites.tree_sites, 0, self->num_trees * sizeof(site_t *));

    tree_left = 0;
    tree_right = self->sequence_length;
    tree_index = 0;
    site = 0;
    j = 0;
    k = 0;
    while (j < self->edges.num_records || tree_left < self->sequence_length) {
        while (k < self->edges.num_records && self->edges.right[O[k]] == tree_left) {
            k++;
        }
        while (j < self->edges.num_records && self->edges.left[I[j]] == tree_left) {
            j++;
        }
        tree_right = self->sequence_length;
        if (j < self->edges.num_records) {
            tree_right = MSP_MIN(tree_right, self->edges.left[I[j]]);
        }
        if (k < self->edges.num_records) {
             tree_right = MSP_MIN(tree_right, self->edges.right[O[k]]);
        }
        self->sites.tree_sites[tree_index] = self->sites.tree_sites_mem + site;
        while (site < (site_id_t) self->sites.num_records
                && self->sites.position[site] < tree_right) {
            self->sites.tree_sites_length[tree_index]++;
            site++;
        }
        tree_left = tree_right;
        tree_index++;
    }
    assert(site == (site_id_t) self->sites.num_records);
    assert(tree_index == self->num_trees);
    ret = 0;
out:
    return ret;
}

static int
tree_sequence_init_nodes(tree_sequence_t *self)
{
    size_t j, k;
    int ret = 0;

    /* Determine the sample size */
    self->num_samples = 0;
    for (j = 0; j < self->nodes.num_records; j++) {
        if (self->nodes.flags[j] & MSP_NODE_IS_SAMPLE) {
            self->num_samples++;
        }
    }
    self->samples = malloc(self->num_samples * sizeof(node_id_t));
    self->nodes.sample_index_map = malloc(self->nodes.num_records * sizeof(node_id_t));
    if (self->samples == NULL || self->nodes.sample_index_map == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    k = 0;
    for (j = 0; j < self->nodes.num_records; j++) {
        self->nodes.sample_index_map[j] = -1;
        if (self->nodes.flags[j] & MSP_NODE_IS_SAMPLE) {
            self->samples[k] = (node_id_t) j;
            self->nodes.sample_index_map[j] = (node_id_t) k;
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
    size_t j;

    memset(self, 0, sizeof(*self));
    self->tables = malloc(sizeof(*self->tables));
    if (self->tables == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    ret = table_collection_alloc(self->tables, MSP_ALLOC_TABLES);
    if (ret != 0) {
        goto out;
    }
    if (tables == NULL) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    ret = table_collection_copy(tables, self->tables);
    if (ret != 0) {
        goto out;
    }
    if (flags & MSP_BUILD_INDEXES || !table_collection_is_indexed(tables)) {
        ret = table_collection_build_indexes(self->tables, 0);
        if (ret != 0) {
            goto out;
        }
    }
    assert(table_collection_is_indexed(self->tables));

    self->sequence_length = tables->sequence_length;
    if (tables->sequence_length == 0) {
        /* Infer the sequence_length as the maximum right value in the edges */
        for (j = 0; j < tables->edges->num_rows; j++) {
            self->sequence_length = MSP_MAX(self->sequence_length,
                    tables->edges->right[j]);
        }
    }
    if (self->sequence_length <= 0) {
        ret = MSP_ERR_BAD_SEQUENCE_LENGTH;
        goto out;
    }
    /* TODO It's messy having two copyies of this value. Should be in one place. */
    self->tables->sequence_length = self->sequence_length;

    self->individuals.num_records = self->tables->individuals->num_rows;
    self->individuals.flags = self->tables->individuals->flags;
    self->individuals.location = self->tables->individuals->location;
    self->individuals.location_offset = self->tables->individuals->location_offset;
    self->individuals.metadata = self->tables->individuals->metadata;
    self->individuals.metadata_offset = self->tables->individuals->metadata_offset;

    self->nodes.num_records = self->tables->nodes->num_rows;
    self->nodes.flags = self->tables->nodes->flags;
    self->nodes.time = self->tables->nodes->time;
    self->nodes.population = self->tables->nodes->population;
    self->nodes.individual = self->tables->nodes->individual;
    self->nodes.metadata = self->tables->nodes->metadata;
    self->nodes.metadata_offset = self->tables->nodes->metadata_offset;

    self->edges.num_records = self->tables->edges->num_rows;
    self->edges.left = self->tables->edges->left;
    self->edges.right = self->tables->edges->right;
    self->edges.parent = self->tables->edges->parent;
    self->edges.child = self->tables->edges->child;
    self->edges.indexes.removal_order = self->tables->indexes.edge_removal_order;
    self->edges.indexes.insertion_order = self->tables->indexes.edge_insertion_order;

    self->migrations.num_records = self->tables->migrations->num_rows;
    self->migrations.left = self->tables->migrations->left;
    self->migrations.right = self->tables->migrations->right;
    self->migrations.node = self->tables->migrations->node;
    self->migrations.source = self->tables->migrations->source;
    self->migrations.dest = self->tables->migrations->dest;
    self->migrations.time = self->tables->migrations->time;

    self->sites.num_records = self->tables->sites->num_rows;
    self->sites.position = self->tables->sites->position;
    self->sites.ancestral_state = self->tables->sites->ancestral_state;
    self->sites.ancestral_state_offset = self->tables->sites->ancestral_state_offset;
    self->sites.metadata = self->tables->sites->metadata;
    self->sites.metadata_offset = self->tables->sites->metadata_offset;

    self->mutations.num_records = self->tables->mutations->num_rows;
    self->mutations.site = self->tables->mutations->site;
    self->mutations.node = self->tables->mutations->node;
    self->mutations.parent = self->tables->mutations->parent;
    self->mutations.derived_state = self->tables->mutations->derived_state;
    self->mutations.derived_state_offset = self->tables->mutations->derived_state_offset;
    self->mutations.metadata = self->tables->mutations->metadata;
    self->mutations.metadata_offset = self->tables->mutations->metadata_offset;

    self->populations.num_records = self->tables->populations->num_rows;
    self->populations.metadata = self->tables->populations->metadata;
    self->populations.metadata_offset = self->tables->populations->metadata_offset;

    self->provenances.num_records = self->tables->provenances->num_rows;
    self->provenances.timestamp = self->tables->provenances->timestamp;
    self->provenances.timestamp_offset = self->tables->provenances->timestamp_offset;
    self->provenances.record = self->tables->provenances->record;
    self->provenances.record_offset = self->tables->provenances->record_offset;

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
    ret = tree_sequence_check(self);
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
    return self->sequence_length;
}

size_t
tree_sequence_get_num_samples(tree_sequence_t *self)
{
    return self->num_samples;
}

size_t
tree_sequence_get_num_nodes(tree_sequence_t *self)
{
    return self->nodes.num_records;
}

size_t
tree_sequence_get_num_edges(tree_sequence_t *self)
{
    return self->edges.num_records;
}

size_t
tree_sequence_get_num_migrations(tree_sequence_t *self)
{
    return self->migrations.num_records;
}

size_t
tree_sequence_get_num_sites(tree_sequence_t *self)
{
    return self->sites.num_records;
}

size_t
tree_sequence_get_num_mutations(tree_sequence_t *self)
{
    return self->mutations.num_records;
}

size_t
tree_sequence_get_num_populations(tree_sequence_t *self)
{
    return self->populations.num_records;
}

size_t
tree_sequence_get_num_individuals(tree_sequence_t *self)
{
    return self->individuals.num_records;
}

size_t
tree_sequence_get_num_provenances(tree_sequence_t *self)
{
    return self->provenances.num_records;
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

    if (u >= 0 && u < (node_id_t) self->nodes.num_records) {
        ret = self->nodes.flags[u] & MSP_NODE_IS_SAMPLE;
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
tree_sequence_get_node(tree_sequence_t *self, node_id_t index, node_t *node)
{
    int ret = 0;
    table_size_t offset, length;

    if (index < 0 || index >= (node_id_t) self->nodes.num_records) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    node->time = self->nodes.time[index];
    node->population = self->nodes.population[index];
    node->individual = self->nodes.individual[index];
    node->flags = self->nodes.flags[index];
    offset = self->nodes.metadata_offset[index];
    length = self->nodes.metadata_offset[index + 1] - offset;
    node->metadata = self->nodes.metadata+ offset;
    node->metadata_length = length;
out:
    return ret;
}

int WARN_UNUSED
tree_sequence_get_edge(tree_sequence_t *self, size_t index, edge_t *edge)
{
    int ret = 0;

    if (index >= self->edges.num_records) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    edge->left = self->edges.left[index];
    edge->right = self->edges.right[index];
    edge->parent = self->edges.parent[index];
    edge->child = self->edges.child[index];
out:
    return ret;
}

int WARN_UNUSED
tree_sequence_get_migration(tree_sequence_t *self, size_t index, migration_t *record)
{
    int ret = 0;

    if (index >= self->migrations.num_records) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    record->node = self->migrations.node[index];
    record->source = self->migrations.source[index];
    record->dest = self->migrations.dest[index];
    record->left = self->migrations.left[index];
    record->right = self->migrations.right[index];
    record->time = self->migrations.time[index];
out:
    return ret;
}

int WARN_UNUSED
tree_sequence_get_mutation(tree_sequence_t *self, mutation_id_t id, mutation_t *record)
{
    int ret = 0;
    table_size_t offset, length;

    if (id < 0 || id >= (mutation_id_t) self->mutations.num_records) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    record->id = id;
    record->index = (size_t) id; // TODO what is this for?
    record->site = self->mutations.site[id];
    record->node = self->mutations.node[id];
    record->parent = self->mutations.parent[id];
    offset = self->mutations.derived_state_offset[id];
    length = self->mutations.derived_state_offset[id + 1] - offset;
    record->derived_state = self->mutations.derived_state + offset;
    record->derived_state_length = length;
    offset = self->mutations.metadata_offset[id];
    length = self->mutations.metadata_offset[id + 1] - offset;
    record->metadata = self->mutations.metadata + offset;
    record->metadata_length = length;
out:
    return ret;
}

int WARN_UNUSED
tree_sequence_get_site(tree_sequence_t *self, site_id_t id, site_t *record)
{
    int ret = 0;
    table_size_t offset, length;

    if (id < 0 || id >= (site_id_t) self->sites.num_records) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    record->id = id;
    offset = self->sites.ancestral_state_offset[id];
    length = self->sites.ancestral_state_offset[id + 1] - offset;
    record->ancestral_state = self->sites.ancestral_state + offset;
    record->ancestral_state_length = length;
    offset = self->sites.metadata_offset[id];
    length = self->sites.metadata_offset[id + 1] - offset;
    record->metadata = self->sites.metadata + offset;
    record->metadata_length = length;
    record->position = self->sites.position[id];
    record->mutations = self->sites.site_mutations[id];
    record->mutations_length = self->sites.site_mutations_length[id];
out:
    return ret;
}

int WARN_UNUSED
tree_sequence_get_individual(tree_sequence_t *self, size_t index, individual_t *individual)
{
    int ret = 0;
    table_size_t offset, length;

    if (index >= self->individuals.num_records) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    individual->id = (individual_id_t) index;
    individual->flags = self->individuals.flags[index];
    offset = self->individuals.location_offset[index];
    length = self->individuals.location_offset[index + 1] - offset;
    individual->location = self->individuals.location + offset;
    individual->location_length = length;
    offset = self->individuals.metadata_offset[index];
    length = self->individuals.metadata_offset[index + 1] - offset;
    individual->metadata = self->individuals.metadata + offset;
    individual->metadata_length = length;
    individual->nodes = self->individuals.individual_nodes[index];
    individual->nodes_length = self->individuals.individual_nodes_length[index];
out:
    return ret;
}

int WARN_UNUSED
tree_sequence_get_population(tree_sequence_t *self, size_t index,
        tmp_population_t *population)
{
    int ret = 0;
    table_size_t offset, length;

    if (index >= self->populations.num_records) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    population->id = (table_size_t) index;
    offset = self->populations.metadata_offset[index];
    length = self->populations.metadata_offset[index + 1] - offset;
    population->metadata = self->populations.metadata + offset;
    population->metadata_length = length;
out:
    return ret;
}

int WARN_UNUSED
tree_sequence_get_provenance(tree_sequence_t *self, size_t index, provenance_t *provenance)
{
    int ret = 0;
    table_size_t offset, length;

    if (index >= self->provenances.num_records) {
        ret = MSP_ERR_OUT_OF_BOUNDS;
        goto out;
    }
    provenance->id = (table_size_t) index;
    offset = self->provenances.timestamp_offset[index];
    length = self->provenances.timestamp_offset[index + 1] - offset;
    provenance->timestamp = self->provenances.timestamp + offset;
    provenance->timestamp_length = length;
    offset = self->provenances.record_offset[index];
    length = self->provenances.record_offset[index + 1] - offset;
    provenance->record = self->provenances.record + offset;
    provenance->record_length = length;
out:
    return ret;
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
    *sample_index_map = self->nodes.sample_index_map;
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
    double left = self->tree_left;
    double right = self->tree_sequence->sequence_length;
    size_t next_edge_list_node = 0;
    tree_sequence_t *s = self->tree_sequence;
    edge_list_t *out_head = NULL;
    edge_list_t *out_tail = NULL;
    edge_list_t *in_head = NULL;
    edge_list_t *in_tail = NULL;
    edge_list_t *w = NULL;
    size_t num_trees = tree_sequence_get_num_trees(s);

    assert(s != NULL);

    if (self->tree_index + 1 < num_trees) {
        /* First we remove the stale records */
        while (self->removal_index < self->num_edges &&
                left == s->edges.right[
                    s->edges.indexes.removal_order[self->removal_index]]) {
            k = s->edges.indexes.removal_order[self->removal_index];
            assert(next_edge_list_node < self->num_edges);
            w = &self->edge_list_nodes[next_edge_list_node];
            next_edge_list_node++;
            w->edge.left = s->edges.left[k];
            w->edge.right = s->edges.right[k];
            w->edge.parent = s->edges.parent[k];
            w->edge.child = s->edges.child[k];
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
                left == s->edges.left[
                    s->edges.indexes.insertion_order[self->insertion_index]]) {
            k = s->edges.indexes.insertion_order[self->insertion_index];
            assert(next_edge_list_node < self->num_edges);
            w = &self->edge_list_nodes[next_edge_list_node];
            next_edge_list_node++;
            w->edge.left = s->edges.left[k];
            w->edge.right = s->edges.right[k];
            w->edge.parent = s->edges.parent[k];
            w->edge.child = s->edges.child[k];
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
        right = s->sequence_length;
        if (self->insertion_index < self->num_edges) {
            right = MSP_MIN(right, s->edges.left[
                    s->edges.indexes.insertion_order[self->insertion_index]]);
        }
        if (self->removal_index < self->num_edges) {
            right = MSP_MIN(right, s->edges.right[
                    s->edges.indexes.removal_order[self->removal_index]]);
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
    size_t N = self->num_nodes;
    size_t num_samples = self->tree_sequence->num_samples;
    size_t j;
    node_id_t u;
    node_list_t *w;

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
    if (self->flags & MSP_SAMPLE_COUNTS) {
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
    if (self->flags & MSP_SAMPLE_LISTS) {
        memset(self->sample_list_head, 0, N * sizeof(node_list_t *));
        memset(self->sample_list_tail, 0, N * sizeof(node_list_t *));
    }
    /* Set the sample attributes */
    self->left_root = MSP_NULL_NODE;
    if (num_samples > 0) {
        self->left_root = self->samples[0];
    }
    for (j = 0; j < num_samples; j++) {
        u = self->samples[j];
        self->above_sample[u] = true;
        if (self->flags & MSP_SAMPLE_COUNTS) {
            self->num_samples[u] = 1;
        }
        if (self->flags & MSP_SAMPLE_LISTS) {
            w = &self->sample_list_node_mem[j];
            w->next = NULL;
            w->node = (node_id_t) u;
            self->sample_list_head[u] = w;
            self->sample_list_tail[u] = w;
        }
        /* Set initial roots */
        if (j < num_samples - 1) {
            self->right_sib[self->samples[j]] = self->samples[j + 1];
        }
        if (j > 0) {
            self->left_sib[self->samples[j]] = self->samples[j - 1];
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
    num_nodes = tree_sequence->nodes.num_records;
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
    if (self->flags & MSP_SAMPLE_COUNTS) {
        self->num_samples = calloc(num_nodes, sizeof(node_id_t));
        self->num_tracked_samples = calloc(num_nodes, sizeof(node_id_t));
        self->marked = calloc(num_nodes, sizeof(uint8_t));
        if (self->num_samples == NULL || self->num_tracked_samples == NULL
                || self->marked == NULL) {
            goto out;
        }
    }
    if (self->flags & MSP_SAMPLE_LISTS) {
        self->sample_list_head = calloc(num_nodes, sizeof(node_list_t *));
        self->sample_list_tail = calloc(num_nodes, sizeof(node_list_t *));
        self->sample_list_node_mem = calloc(num_samples, sizeof(node_list_t));
        if (self->sample_list_head == NULL || self->sample_list_tail == NULL
                || self->sample_list_node_mem == NULL) {
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
    msp_safe_free(self->sample_list_head);
    msp_safe_free(self->sample_list_tail);
    msp_safe_free(self->sample_list_node_mem);
    return 0;
}

static int WARN_UNUSED
sparse_tree_reset_tracked_samples(sparse_tree_t *self)
{
    int ret = 0;

    if (!(self->flags & MSP_SAMPLE_COUNTS)) {
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
        node_list_t *head, node_list_t *tail)
{
    int ret = MSP_ERR_GENERIC;
    node_list_t *list_node = head;
    node_id_t u;
    int not_done;

    if (head == NULL || tail == NULL) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    /* TODO This is not needed when the sparse tree is new. We should use the
     * state machine to check and only reset the tracked samples when needed.
     */
    ret = sparse_tree_reset_tracked_samples(self);
    if (ret != 0) {
        goto out;
    }
    not_done = 1;
    while (not_done) {
        u = list_node->node;
        /* Propagate this upwards */
        assert(self->num_tracked_samples[u] == 0);
        while (u != MSP_NULL_NODE) {
            self->num_tracked_samples[u] += 1;
            u = self->parent[u];
        }
        not_done = list_node != tail;
        list_node = list_node->next;
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

int WARN_UNUSED
sparse_tree_get_sample_list(sparse_tree_t *self, node_id_t u,
        node_list_t **head, node_list_t **tail)
{
    int ret = 0;

    ret = sparse_tree_check_node(self, u);
    if (ret != 0) {
        goto out;
    }
    if (! (self->flags & MSP_SAMPLE_LISTS)) {
        ret = MSP_ERR_UNSUPPORTED_OPERATION;
        goto out;
    }
    *head = self->sample_list_head[u];
    *tail = self->sample_list_tail[u];
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

    ret = sparse_tree_check_node(self, u);
    if (ret != 0) {
        goto out;
    }
    *t = self->tree_sequence->nodes.time[u];
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
sparse_tree_get_newick(sparse_tree_t *self, size_t precision, double time_scale,
        int flags, size_t buffer_size, char *newick_buffer)
{
    int ret = 0;
    newick_converter_t nc;

    ret = newick_converter_alloc(&nc, self, precision, time_scale, flags);
    if (ret != 0) {
        goto out;
    }
    ret = newick_converter_run(&nc, buffer_size, newick_buffer);
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
        assert(self->sample_list_tail != NULL);
        assert(self->sample_list_head != NULL);
        assert(self->sample_list_node_mem != NULL);
    } else {
        assert(self->sample_list_tail == NULL);
        assert(self->sample_list_head == NULL);
        assert(self->sample_list_node_mem == NULL);
    }

    free(children);
    free(is_root);
}

void
sparse_tree_print_state(sparse_tree_t *self, FILE *out)
{
    size_t j;
    node_list_t *u;
    site_t site;

    fprintf(out, "Sparse tree state:\n");
    fprintf(out, "flags = %d\n", self->flags);
    fprintf(out, "left = %f\n", self->left);
    fprintf(out, "right = %f\n", self->right);
    fprintf(out, "left_root = %d\n", (int) self->left_root);
    fprintf(out, "index = %d\n", (int) self->index);
    fprintf(out, "node\tparent\tlchild\trchild\tlsib\trsib\n");

    for (j = 0; j < self->num_nodes; j++) {
        fprintf(out, "%d\t%d\t%d\t%d\t%d\t%d", (int) j, self->parent[j], self->left_child[j],
                self->right_child[j], self->left_sib[j], self->right_sib[j]);
        if (self->flags & MSP_SAMPLE_COUNTS) {
            fprintf(out, "\t%d\t%d\t%d", (int) self->num_samples[j],
                    (int) self->num_tracked_samples[j], self->marked[j]);
        }
        if (self->flags & MSP_SAMPLE_LISTS) {
            fprintf(out, "\t[");
            u = self->sample_list_head[j];
            if (u != NULL) {
                while (1) {
                    fprintf(out, "%d ", (int) u->node);
                    if (u == self->sample_list_tail[j]) {
                        break;
                    }
                    u = u->next;
                }
            } else {
                assert(self->sample_list_tail[j] == NULL);
            }

            fprintf(out, "]");
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
    const node_id_t all_samples_diff = self->num_samples[child];
    const node_id_t tracked_samples_diff = self->num_tracked_samples[child];
    const uint8_t mark = self->mark;
    node_id_t v = parent;

    /* propagate this loss up as far as we can */
    while (v != MSP_NULL_NODE) {
        self->num_samples[v] -= all_samples_diff;
        self->num_tracked_samples[v] -= tracked_samples_diff;
        self->marked[v] = mark;
        v = self->parent[v];
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

    /* propogate this gain up as far as we can */
    v = parent;
    while (v != MSP_NULL_NODE) {
        self->num_samples[v] += all_samples_diff;
        self->num_tracked_samples[v] += tracked_samples_diff;
        self->marked[v] = mark;
        v = self->parent[v];
    }
}

static inline void
sparse_tree_update_sample_lists(sparse_tree_t *self, node_id_t node)
{
    node_id_t u, v;
    node_list_t **head = self->sample_list_head;
    node_list_t **tail = self->sample_list_tail;

    u = node;
    while (u != MSP_NULL_NODE) {
        if (self->tree_sequence->nodes.flags[u] & MSP_NODE_IS_SAMPLE) {
            tail[u] = head[u];
        } else {
            head[u] = NULL;
            tail[u] = NULL;
        }
        v = self->left_child[u];
        while (v != MSP_NULL_NODE) {
            if (head[v] != NULL) {
                assert(tail[v] != NULL);
                if (head[u] == NULL) {
                    head[u] = head[v];
                    tail[u] = tail[v];
                } else {
                    tail[u]->next = head[v];
                    tail[u] = tail[v];
                }
            }
            v = self->right_sib[v];
        }
        u = self->parent[u];
    }
}

static int
sparse_tree_advance(sparse_tree_t *self, int direction,
        double *out_breakpoints, node_id_t *out_order, node_id_t *out_index,
        double *in_breakpoints, node_id_t *in_order, node_id_t *in_index,
        int MSP_UNUSED(first_tree))
{
    int ret = 0;
    int direction_change = direction * (direction != self->direction);
    node_id_t in = *in_index + direction_change;
    node_id_t out = *out_index + direction_change;
    node_id_t k, p, c, u, v, root, lsib, rsib, lroot, rroot;
    tree_sequence_t *s = self->tree_sequence;
    node_id_t R = (node_id_t) s->edges.num_records;
    double x;
    bool above_sample;

    if (direction == MSP_DIR_FORWARD) {
        x = self->right;
    } else {
        x = self->left;
    }
    while (out >= 0 && out < R && out_breakpoints[out_order[out]] == x) {
        assert(out < R);
        k = out_order[out];
        out += direction;
        p = s->edges.parent[k];
        c = s->edges.child[k];
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
                above_sample = s->nodes.flags[v] & MSP_NODE_IS_SAMPLE;
                u = self->left_child[v];
                while (u != MSP_NULL_NODE) {
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

    while (in >= 0 && in < R && in_breakpoints[in_order[in]] == x) {
        k = in_order[in];
        in += direction;
        p = s->edges.parent[k];
        c = s->edges.child[k];
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
                /* self->left_root = MSP_NULL_NODE; */
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

    /* Ensure that left_root is the left-most root */
    while (self->left_sib[self->left_root] != MSP_NULL_NODE) {
        self->left_root = self->left_sib[self->left_root];
    }

    self->direction = direction;
    self->index = (size_t) ((int64_t) self->index + direction);
    if (direction == MSP_DIR_FORWARD) {
        self->left = x;
        self->right = s->sequence_length;
        if (out >= 0 && out < R) {
            self->right = MSP_MIN(self->right, out_breakpoints[out_order[out]]);
        }
        if (in >= 0 && in < R) {
            self->right = MSP_MIN(self->right, in_breakpoints[in_order[in]]);
        }
    } else {
        self->right = x;
        self->left = 0;
        if (out >= 0 && out < R) {
            self->left = MSP_MAX(self->left, out_breakpoints[out_order[out]]);
        }
        if (in >= 0 && in < R) {
            self->left = MSP_MAX(self->left, in_breakpoints[in_order[in]]);
        }
    }
    assert(self->left < self->right);
    *out_index = out;
    *in_index = in;
    if (s->sites.num_records > 0) {
        self->sites = s->sites.tree_sites[self->index];
        self->sites_length = s->sites.tree_sites_length[self->index];
    }
    ret = 1;
out:
    return ret;
}

int WARN_UNUSED
sparse_tree_first(sparse_tree_t *self)
{
    int ret = 1;
    tree_sequence_t *s = self->tree_sequence;

    self->left = 0;
    self->index = 0;
    self->right = s->sequence_length;
    self->sites = s->sites.tree_sites[0];
    self->sites_length = s->sites.tree_sites_length[0];

    if (s->edges.num_records > 0) {
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
                s->edges.right, s->edges.indexes.removal_order,
                &self->right_index, s->edges.left,
                s->edges.indexes.insertion_order, &self->left_index, 1);
    }
out:
    return ret;
}

int WARN_UNUSED
sparse_tree_last(sparse_tree_t *self)
{
    int ret = 1;
    tree_sequence_t *s = self->tree_sequence;

    self->left = 0;
    self->right = s->sequence_length;
    self->index = 0;
    self->sites = s->sites.tree_sites[0];
    self->sites_length = s->sites.tree_sites_length[0];

    if (s->edges.num_records > 0) {
        /* TODO this is redundant if this is the first usage of the tree. We
         * should add a state machine here so we know what state the tree is
         * in and can take the appropriate actions.
         */
        ret = sparse_tree_clear(self);
        if (ret != 0) {
            goto out;
        }
        self->index = tree_sequence_get_num_trees(s);
        self->left_index = (node_id_t) s->edges.num_records - 1;
        self->right_index = (node_id_t) s->edges.num_records - 1;
        self->direction = MSP_DIR_REVERSE;
        self->left = s->sequence_length;
        self->right = 0;

        ret = sparse_tree_advance(self, MSP_DIR_REVERSE,
                s->edges.left, s->edges.indexes.insertion_order,
                &self->left_index, s->edges.right,
                s->edges.indexes.removal_order, &self->right_index, 1);
    }
out:
    return ret;
}

int WARN_UNUSED
sparse_tree_next(sparse_tree_t *self)
{
    int ret = 0;
    tree_sequence_t *s = self->tree_sequence;
    size_t num_trees = tree_sequence_get_num_trees(s);

    if (self->index < num_trees - 1) {
        ret = sparse_tree_advance(self, MSP_DIR_FORWARD,
                s->edges.right, s->edges.indexes.removal_order,
                &self->right_index, s->edges.left,
                s->edges.indexes.insertion_order, &self->left_index, 0);
    }
    return ret;
}

int WARN_UNUSED
sparse_tree_prev(sparse_tree_t *self)
{
    int ret = 0;
    tree_sequence_t *s = self->tree_sequence;

    if (self->index > 0) {
        ret = sparse_tree_advance(self, MSP_DIR_REVERSE,
                s->edges.left, s->edges.indexes.insertion_order,
                &self->left_index, s->edges.right,
                s->edges.indexes.removal_order, &self->right_index, 0);
    }
    return ret;
}
