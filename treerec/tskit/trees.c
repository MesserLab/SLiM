/*
 * MIT License
 *
 * Copyright (c) 2019 Tskit Developers
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
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include <tskit/trees.h>


/* ======================================================== *
 * tree sequence
 * ======================================================== */

static void
tsk_treeseq_check_state(tsk_treeseq_t *self)
{
    size_t j;
    tsk_size_t k, l;
    tsk_site_t site;
    tsk_id_t site_id = 0;

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
tsk_treeseq_print_state(tsk_treeseq_t *self, FILE *out)
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
    if (self->site_mutations_mem == NULL
            || self->site_mutations_length == NULL
            || self->site_mutations == NULL
            || self->tree_sites_mem == NULL) {
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

    self->individual_nodes_mem = malloc(TSK_MAX(1, total_node_refs) * sizeof(tsk_node_t));
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
            assert(node_array - self->individual_nodes_mem
                    < (tsk_id_t) (total_node_refs - node_count[ind]));
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
    const double * restrict site_position = self->tables->sites.position;
    const tsk_id_t * restrict I = self->tables->indexes.edge_insertion_order;
    const tsk_id_t * restrict O = self->tables->indexes.edge_removal_order;
    const double * restrict edge_right = self->tables->edges.right;
    const double * restrict edge_left = self->tables->edges.left;

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
            tree_right = TSK_MIN(tree_right, edge_left[I[j]]);
        }
        if (k < num_edges) {
             tree_right = TSK_MIN(tree_right, edge_right[O[k]]);
        }
        tree_left = tree_right;
        self->num_trees++;
    }
    assert(self->num_trees > 0);

    self->tree_sites_length = malloc(self->num_trees * sizeof(tsk_size_t));
    self->tree_sites = malloc(self->num_trees * sizeof(tsk_site_t *));
    if (self->tree_sites == NULL || self->tree_sites_length == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    memset(self->tree_sites_length, 0, self->num_trees * sizeof(tsk_size_t));
    memset(self->tree_sites, 0, self->num_trees * sizeof(tsk_site_t *));

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
    assert(site == num_sites);
    assert(tree_index == self->num_trees);
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
    assert(k == self->num_samples);
out:
    return ret;
}

/* TODO we need flags to be able to control how the input table is used.
 * - The default behaviour is to take a copy. TSK_BUILD_INDEXES is allowed
 *   in this case because we have an independent copy.
 * - Need an option to take 'ownership' of the tables so that we keep the
 *   tables and free them at the end of the treeseq's lifetime. This will be
 *   used in tsk_treeseq_load below, where we can take advantage of the read-only
 *   access directly into the store's memory and avoid copying the tree sequence.
 * - We should also allow a read-only "borrowed reference" where we use the
 *   tables directly, but don't free it at the end.
 */
int TSK_WARN_UNUSED
tsk_treeseq_init(tsk_treeseq_t *self, tsk_table_collection_t *tables, tsk_flags_t options)
{
    int ret = 0;

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
    ret = tsk_table_collection_check_integrity(self->tables, TSK_CHECK_ALL);
    if (ret != 0) {
        goto out;
    }
    assert(tsk_table_collection_has_index(self->tables, 0));

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
tsk_treeseq_copy_tables(tsk_treeseq_t *self, tsk_table_collection_t *tables,
        tsk_flags_t options)
{
    return tsk_table_collection_copy(self->tables, tables, options);
}

int TSK_WARN_UNUSED
tsk_treeseq_load(tsk_treeseq_t *self, const char *filename,
        tsk_flags_t TSK_UNUSED(options))
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
tsk_treeseq_dump(tsk_treeseq_t *self, const char *filename, tsk_flags_t options)
{
    return tsk_table_collection_dump(self->tables, filename, options);
}

/* Simple attribute getters */

double
tsk_treeseq_get_sequence_length(tsk_treeseq_t *self)
{
    return self->tables->sequence_length;
}

char *
tsk_treeseq_get_file_uuid(tsk_treeseq_t *self)
{
    return self->tables->file_uuid;
}

tsk_size_t
tsk_treeseq_get_num_samples(tsk_treeseq_t *self)
{
    return self->num_samples;
}

tsk_size_t
tsk_treeseq_get_num_nodes(tsk_treeseq_t *self)
{
    return self->tables->nodes.num_rows;
}

tsk_size_t
tsk_treeseq_get_num_edges(tsk_treeseq_t *self)
{
    return self->tables->edges.num_rows;
}

tsk_size_t
tsk_treeseq_get_num_migrations(tsk_treeseq_t *self)
{
    return self->tables->migrations.num_rows;
}

tsk_size_t
tsk_treeseq_get_num_sites(tsk_treeseq_t *self)
{
    return self->tables->sites.num_rows;
}

tsk_size_t
tsk_treeseq_get_num_mutations(tsk_treeseq_t *self)
{
    return self->tables->mutations.num_rows;
}

tsk_size_t
tsk_treeseq_get_num_populations(tsk_treeseq_t *self)
{
    return self->tables->populations.num_rows;
}

tsk_size_t
tsk_treeseq_get_num_individuals(tsk_treeseq_t *self)
{
    return self->tables->individuals.num_rows;
}

tsk_size_t
tsk_treeseq_get_num_provenances(tsk_treeseq_t *self)
{
    return self->tables->provenances.num_rows;
}

tsk_size_t
tsk_treeseq_get_num_trees(tsk_treeseq_t *self)
{
    return self->num_trees;
}

bool
tsk_treeseq_is_sample(tsk_treeseq_t *self, tsk_id_t u)
{
    bool ret = false;

    if (u >= 0 && u < (tsk_id_t) self->tables->nodes.num_rows) {
        ret = !!(self->tables->nodes.flags[u] & TSK_NODE_IS_SAMPLE);
    }
    return ret;
}

/* Accessors for records */

int TSK_WARN_UNUSED
tsk_treeseq_get_pairwise_diversity(tsk_treeseq_t *self,
    tsk_id_t *samples, size_t num_samples, double *pi)
{
    int ret = 0;
    tsk_tree_t *tree = NULL;
    double result, denom, n, count;
    tsk_site_t *sites;
    tsk_size_t j, k, num_sites;

    if (num_samples < 2 || num_samples > self->num_samples) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    n = (double) num_samples;
    tree = malloc(sizeof(tsk_tree_t));
    if (tree == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    ret = tsk_tree_init(tree, self, TSK_SAMPLE_COUNTS);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_tree_set_tracked_samples(tree, num_samples, samples);
    if (ret != 0) {
        goto out;
    }
    /* Allocation done; move onto main algorithm. */
    result = 0.0;
    for (ret = tsk_tree_first(tree); ret == 1; ret = tsk_tree_next(tree)) {
        ret = tsk_tree_get_sites(tree, &sites, &num_sites);
        if (ret != 0) {
            goto out;
        }
        for (j = 0; j < num_sites; j++) {
            if (sites[j].mutations_length != 1) {
                ret = TSK_ERR_ONLY_INFINITE_SITES;
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
        tsk_tree_free(tree);
        free(tree);
    }
    return ret;
}

#define GET_2D_ROW(array, row_len, row) (array + (((size_t) (row_len)) * (size_t) row))

int TSK_WARN_UNUSED
tsk_treeseq_genealogical_nearest_neighbours(tsk_treeseq_t *self,
        tsk_id_t *focal, size_t num_focal,
        tsk_id_t **reference_sets, size_t *reference_set_size, size_t num_reference_sets,
        tsk_flags_t TSK_UNUSED(options), double *ret_array)
{
    int ret = 0;
    tsk_id_t u, v, p;
    size_t j;
    /* TODO It's probably not worth bothering with the int16_t here. */
    int16_t k, focal_reference_set;
    /* We use the K'th element of the array for the total. */
    const int16_t K = (int16_t) (num_reference_sets + 1);
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
    uint32_t *restrict row, *restrict child_row, total;
	
	/*	BCH 30 April 2019: the following two lines get rid of "uninitialized when used" warnings below.
		I think the correct fix might be to move the whole block that's inside "if (p != TSK_NULL)"
		(lines 748:761) to just before the "break" statement (line 743); if I understand the control
		flow correctly, that would be equivalent while also allowing the compiler to understand that
		the variables are initialized.  But I have refrained from making that change myself. */
	row = NULL;
	total = 0;

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
            p = parent[u];
            while (p != TSK_NULL) {
                row = GET_2D_ROW(ref_count, K, p);
                total = row[K - 1];
                if (total > 1) {
                    break;
                }
                p = parent[p];
            }
            if (p != TSK_NULL) {
                length[j] += tree_length;
                focal_reference_set = reference_set_map[u];
                scale = tree_length / (total - (focal_reference_set != -1));
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
tsk_treeseq_mean_descendants(tsk_treeseq_t *self,
        tsk_id_t **reference_sets, size_t *reference_set_size, size_t num_reference_sets,
        tsk_flags_t TSK_UNUSED(options), double *ret_array)
{
    int ret = 0;
    tsk_id_t u, v;
    size_t j;
    int32_t k;
    /* We use the K'th element of the array for the total. */
    const int32_t K = (int32_t) (num_reference_sets + 1);
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

int TSK_WARN_UNUSED
tsk_treeseq_get_node(tsk_treeseq_t *self, tsk_id_t index, tsk_node_t *node)
{
    return tsk_node_table_get_row(&self->tables->nodes, index, node);
}

int TSK_WARN_UNUSED
tsk_treeseq_get_edge(tsk_treeseq_t *self, tsk_id_t index, tsk_edge_t *edge)
{
    return tsk_edge_table_get_row(&self->tables->edges, index, edge);
}

int TSK_WARN_UNUSED
tsk_treeseq_get_migration(tsk_treeseq_t *self, tsk_id_t index, tsk_migration_t *migration)
{
    return tsk_migration_table_get_row(&self->tables->migrations, index, migration);
}

int TSK_WARN_UNUSED
tsk_treeseq_get_mutation(tsk_treeseq_t *self, tsk_id_t index, tsk_mutation_t *mutation)
{
    return tsk_mutation_table_get_row(&self->tables->mutations, index, mutation);
}

int TSK_WARN_UNUSED
tsk_treeseq_get_site(tsk_treeseq_t *self, tsk_id_t index, tsk_site_t *site)
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
tsk_treeseq_get_individual(tsk_treeseq_t *self, tsk_id_t index, tsk_individual_t *individual)
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
tsk_treeseq_get_population(tsk_treeseq_t *self, tsk_id_t index,
        tsk_population_t *population)
{
    return tsk_population_table_get_row(&self->tables->populations, index, population);
}

int TSK_WARN_UNUSED
tsk_treeseq_get_provenance(tsk_treeseq_t *self, tsk_id_t index, tsk_provenance_t *provenance)
{
   return tsk_provenance_table_get_row(&self->tables->provenances, index, provenance);
}

int TSK_WARN_UNUSED
tsk_treeseq_get_samples(tsk_treeseq_t *self, tsk_id_t **samples)
{
    *samples = self->samples;
    return 0;
}

int TSK_WARN_UNUSED
tsk_treeseq_get_sample_index_map(tsk_treeseq_t *self, tsk_id_t **sample_index_map)
{
    *sample_index_map = self->sample_index_map;
    return 0;
}

int TSK_WARN_UNUSED
tsk_treeseq_simplify(tsk_treeseq_t *self, tsk_id_t *samples, tsk_size_t num_samples,
        tsk_flags_t options, tsk_treeseq_t *output, tsk_id_t *node_map)
{
    int ret = 0;
    tsk_table_collection_t tables;

    ret = tsk_treeseq_copy_tables(self, &tables, 0);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_table_collection_simplify(&tables, samples, num_samples, options, node_map);
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
    const bool sample_counts = !!(self->options & TSK_SAMPLE_COUNTS);
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
    memset(self->above_sample, 0, N * sizeof(bool));
    if (sample_counts) {
        memset(self->num_samples, 0, N * sizeof(tsk_id_t));
        memset(self->marked, 0, N * sizeof(uint8_t));
        /* We can't reset the tracked samples via memset because we don't
         * know where the tracked samples are.
         */
        for (j = 0; j < self->num_nodes; j++) {
            if (! tsk_treeseq_is_sample(self->tree_sequence, (tsk_id_t) j)) {
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
    self->left_root = TSK_NULL;
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
            self->left_sample[u] = (tsk_id_t) j;
            self->right_sample[u] = (tsk_id_t) j;
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

int TSK_WARN_UNUSED
tsk_tree_init(tsk_tree_t *self, tsk_treeseq_t *tree_sequence, tsk_flags_t options)
{
    int ret = TSK_ERR_NO_MEMORY;
    tsk_size_t num_samples;
    tsk_size_t num_nodes;

    memset(self, 0, sizeof(tsk_tree_t));
    if (tree_sequence == NULL) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    num_nodes = tree_sequence->tables->nodes.num_rows;
    num_samples = tree_sequence->num_samples;
    self->num_nodes = num_nodes;
    self->tree_sequence = tree_sequence;
    self->samples = tree_sequence->samples;
    self->options = options;
    self->parent = malloc(num_nodes * sizeof(tsk_id_t));
    self->left_child = malloc(num_nodes * sizeof(tsk_id_t));
    self->right_child = malloc(num_nodes * sizeof(tsk_id_t));
    self->left_sib = malloc(num_nodes * sizeof(tsk_id_t));
    self->right_sib = malloc(num_nodes * sizeof(tsk_id_t));
    self->above_sample = malloc(num_nodes * sizeof(bool));
    if (self->parent == NULL || self->left_child == NULL || self->right_child == NULL
            || self->left_sib == NULL || self->right_sib == NULL
            || self->above_sample == NULL) {
        goto out;
    }
    /* the maximum possible height of the tree is num_nodes + 1, including
     * the null value. */
    self->stack1 = malloc((num_nodes + 1) * sizeof(tsk_id_t));
    self->stack2 = malloc((num_nodes + 1) * sizeof(tsk_id_t));
    if (self->stack1 == NULL || self->stack2 == NULL) {
        goto out;
    }
    if (self->options & TSK_SAMPLE_COUNTS) {
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

int TSK_WARN_UNUSED
tsk_tree_free(tsk_tree_t *self)
{
    tsk_safe_free(self->parent);
    tsk_safe_free(self->left_child);
    tsk_safe_free(self->right_child);
    tsk_safe_free(self->left_sib);
    tsk_safe_free(self->right_sib);
    tsk_safe_free(self->above_sample);
    tsk_safe_free(self->stack1);
    tsk_safe_free(self->stack2);
    tsk_safe_free(self->num_samples);
    tsk_safe_free(self->num_tracked_samples);
    tsk_safe_free(self->marked);
    tsk_safe_free(self->left_sample);
    tsk_safe_free(self->right_sample);
    tsk_safe_free(self->next_sample);
    return 0;
}

bool
tsk_tree_has_sample_lists(tsk_tree_t *self)
{
    return !!(self->options & TSK_SAMPLE_LISTS);
}

bool
tsk_tree_has_sample_counts(tsk_tree_t *self)
{
    return !!(self->options & TSK_SAMPLE_COUNTS);
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
tsk_tree_set_tracked_samples(tsk_tree_t *self, size_t num_tracked_samples,
        tsk_id_t *tracked_samples)
{
    int ret = TSK_ERR_GENERIC;
    size_t j;
    tsk_id_t u;

    /* TODO This is not needed when the sparse tree is new. We should use the
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
        if (! tsk_treeseq_is_sample(self->tree_sequence, u)) {
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
tsk_tree_set_tracked_samples_from_sample_list(tsk_tree_t *self,
        tsk_tree_t *other, tsk_id_t node)
{
    int ret = TSK_ERR_GENERIC;
    tsk_id_t u, stop, index;
    const tsk_id_t *next = other->next_sample;
    const tsk_id_t *samples = other->tree_sequence->samples;

    if (! tsk_tree_has_sample_lists(other)) {
        ret = TSK_ERR_UNSUPPORTED_OPERATION;
        goto out;
    }
    /* TODO This is not needed when the sparse tree is new. We should use the
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
            assert(self->num_tracked_samples[u] == 0);
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
tsk_tree_copy(tsk_tree_t *self, tsk_tree_t *dest, tsk_flags_t options)
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

    memcpy(dest->parent, self->parent, N * sizeof(tsk_id_t));
    memcpy(dest->left_child, self->left_child, N * sizeof(tsk_id_t));
    memcpy(dest->right_child, self->right_child, N * sizeof(tsk_id_t));
    memcpy(dest->left_sib, self->left_sib, N * sizeof(tsk_id_t));
    memcpy(dest->right_sib, self->right_sib, N * sizeof(tsk_id_t));
    memcpy(dest->above_sample, self->above_sample, N * sizeof(*self->above_sample));
    if (dest->options & TSK_SAMPLE_COUNTS) {
        if (!(self->options & TSK_SAMPLE_COUNTS)) {
            ret = TSK_ERR_UNSUPPORTED_OPERATION;
            goto out;
        }
        memcpy(dest->num_samples, self->num_samples,
                N * sizeof(*self->num_samples));
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
tsk_tree_equals(tsk_tree_t *self, tsk_tree_t *other)
{
    bool ret = false;

    if (self->tree_sequence == other->tree_sequence) {
        ret = self->index == other->index;
    }
    return ret;
}

static int
tsk_tree_check_node(tsk_tree_t *self, tsk_id_t u)
{
    int ret = 0;
    if (u < 0 || u >= (tsk_id_t) self->num_nodes) {
        ret = TSK_ERR_NODE_OUT_OF_BOUNDS;
    }
    return ret;
}

bool
tsk_tree_is_descendant(tsk_tree_t *self, tsk_id_t u, tsk_id_t v)
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

/* TODO need to either document that we're using state within the tree
 * here and so it's not threadsafe or we should just malloc the buffers
 * each time. */
int TSK_WARN_UNUSED
tsk_tree_get_mrca(tsk_tree_t *self, tsk_id_t u, tsk_id_t v, tsk_id_t *mrca)
{
    int ret = 0;
    tsk_id_t w = 0;
    tsk_id_t *s1 = self->stack1;
    tsk_id_t *s2 = self->stack2;
    tsk_id_t j;
    int l1, l2;

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
        assert(l1 < (int) self->num_nodes);
        s1[l1] = j;
        l1++;
        j = self->parent[j];
    }
    s1[l1] = TSK_NULL;
    j = v;
    l2 = 0;
    while (j != TSK_NULL) {
        assert(l2 < (int) self->num_nodes);
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
    return ret;
}

static int
tsk_tree_get_num_samples_by_traversal(tsk_tree_t *self, tsk_id_t u,
        size_t *num_samples)
{
    int ret = 0;
    tsk_id_t *stack = self->stack1;
    tsk_id_t v;
    size_t count = 0;
    int stack_top = 0;

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
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_get_num_samples(tsk_tree_t *self, tsk_id_t u, size_t *num_samples)
{
    int ret = 0;

    ret = tsk_tree_check_node(self, u);
    if (ret != 0) {
        goto out;
    }

    if (self->options & TSK_SAMPLE_COUNTS) {
        *num_samples = (size_t) self->num_samples[u];
    } else {
        ret = tsk_tree_get_num_samples_by_traversal(self, u, num_samples);
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_get_num_tracked_samples(tsk_tree_t *self, tsk_id_t u,
        size_t *num_tracked_samples)
{
    int ret = 0;

    ret = tsk_tree_check_node(self, u);
    if (ret != 0) {
        goto out;
    }
    if (! (self->options & TSK_SAMPLE_COUNTS)) {
        ret = TSK_ERR_UNSUPPORTED_OPERATION;
        goto out;
    }
    *num_tracked_samples = (size_t) self->num_tracked_samples[u];
out:
    return ret;
}

bool
tsk_tree_is_sample(tsk_tree_t *self, tsk_id_t u)
{
    return tsk_treeseq_is_sample(self->tree_sequence, u);
}

tsk_size_t
tsk_tree_get_num_roots(tsk_tree_t *self)
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
tsk_tree_get_index(tsk_tree_t *self)
{
    return self->index;
}


int TSK_WARN_UNUSED
tsk_tree_get_parent(tsk_tree_t *self, tsk_id_t u, tsk_id_t *parent)
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
tsk_tree_get_time(tsk_tree_t *self, tsk_id_t u, double *t)
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
tsk_tree_get_sites(tsk_tree_t *self, tsk_site_t **sites, tsk_size_t *sites_length)
{
    *sites = self->sites;
    *sites_length = self->sites_length;
    return 0;
}

static void
tsk_tree_check_state(tsk_tree_t *self)
{
    tsk_id_t u, v;
    size_t j, num_samples;
    int err, c;
    tsk_site_t site;
    tsk_id_t *children = malloc(self->num_nodes * sizeof(tsk_id_t));
    bool *is_root = calloc(self->num_nodes, sizeof(bool));

    assert(children != NULL);

    for (j = 0; j < self->tree_sequence->num_samples; j++) {
        u = self->samples[j];
        while (self->parent[u] != TSK_NULL) {
            u = self->parent[u];
        }
        is_root[u] = true;
    }
    if (self->tree_sequence->num_samples == 0) {
        assert(self->left_root == TSK_NULL);
    } else {
        assert(self->left_sib[self->left_root] == TSK_NULL);
    }
    /* Iterate over the roots and make sure they are set */
    for (u = self->left_root; u != TSK_NULL; u = self->right_sib[u]) {
        assert(is_root[u]);
        is_root[u] = false;
    }
    for (u = 0; u < (tsk_id_t) self->num_nodes; u++) {
        assert(!is_root[u]);
        c = 0;
        for (v = self->left_child[u]; v != TSK_NULL; v = self->right_sib[v]) {
            assert(self->parent[v] == u);
            children[c] = v;
            c++;
        }
        for (v = self->right_child[u]; v != TSK_NULL; v = self->left_sib[v]) {
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

    if (self->options & TSK_SAMPLE_COUNTS) {
        assert(self->num_samples != NULL);
        assert(self->num_tracked_samples != NULL);
        for (u = 0; u < (tsk_id_t) self->num_nodes; u++) {
            err = tsk_tree_get_num_samples_by_traversal(self, u, &num_samples);
            assert(err == 0);
            assert(num_samples == (size_t) self->num_samples[u]);
        }
    } else {
        assert(self->num_samples == NULL);
        assert(self->num_tracked_samples == NULL);
    }
    if (self->options & TSK_SAMPLE_LISTS) {
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
tsk_tree_print_state(tsk_tree_t *self, FILE *out)
{
    size_t j;
    tsk_site_t site;

    fprintf(out, "Sparse tree state:\n");
    fprintf(out, "options = %d\n", self->options);
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
        fprintf(out, "%d\t%d\t%d\t%d\t%d\t%d", (int) j, self->parent[j], self->left_child[j],
                self->right_child[j], self->left_sib[j], self->right_sib[j]);
        if (self->options & TSK_SAMPLE_LISTS) {
            fprintf(out, "\t%d\t%d\t", self->left_sample[j],
                    self->right_sample[j]);
        }
        if (self->options & TSK_SAMPLE_COUNTS) {
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

/* Implementation note: we're passing the parent array as a restrict pointer
 * argument here for performance reasons. The num_samples and num_tracked_samples
 * arrays can be accessed through local restrict pointers here because we're not
 * accessing them from the calling function.
 */

static inline void
tsk_tree_propagate_sample_count_loss(tsk_tree_t *self,
        const tsk_id_t * restrict tree_parent, tsk_id_t parent, tsk_id_t child)
{
    tsk_id_t v;
    const tsk_id_t all_samples_diff = self->num_samples[child];
    const tsk_id_t tracked_samples_diff = self->num_tracked_samples[child];
    const uint8_t mark = self->mark;
    tsk_id_t * restrict num_samples = self->num_samples;
    tsk_id_t * restrict num_tracked_samples = self->num_tracked_samples;
    uint8_t * restrict marked = self->marked;

    /* propagate this loss up as far as we can */
    v = parent;
    while (v != TSK_NULL) {
        num_samples[v] -= all_samples_diff;
        num_tracked_samples[v] -= tracked_samples_diff;
        marked[v] = mark;
        v = tree_parent[v];
    }
}

static inline void
tsk_tree_propagate_sample_count_gain(tsk_tree_t *self,
        const tsk_id_t * restrict tree_parent, tsk_id_t parent, tsk_id_t child)
{
    tsk_id_t v;
    const tsk_id_t all_samples_diff = self->num_samples[child];
    const tsk_id_t tracked_samples_diff = self->num_tracked_samples[child];
    const uint8_t mark = self->mark;
    tsk_id_t * restrict num_samples = self->num_samples;
    tsk_id_t * restrict num_tracked_samples = self->num_tracked_samples;
    uint8_t * restrict marked = self->marked;

    /* propogate this gain up as far as we can */
    v = parent;
    while (v != TSK_NULL) {
        num_samples[v] += all_samples_diff;
        num_tracked_samples[v] += tracked_samples_diff;
        marked[v] = mark;
        v = tree_parent[v];
    }
}

/* parent, left_child and right_sib are restrict pointers in the calling function,
 * so we pass these as parameters to ensure the relationships are clear to the
 * compiler. */
static inline void
tsk_tree_update_sample_lists(tsk_tree_t *self,
        const tsk_id_t * restrict parent,
        const tsk_id_t * restrict left_child,
        const tsk_id_t * restrict right_sib,
        tsk_id_t node)
{
    tsk_id_t u, v, sample_index;
    tsk_id_t * restrict left = self->left_sample;
    tsk_id_t * restrict right = self->right_sample;
    tsk_id_t * restrict next = self->next_sample;
    const tsk_id_t * restrict sample_index_map = self->tree_sequence->sample_index_map;

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
                assert(right[v] != TSK_NULL);
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

static int
tsk_tree_advance(tsk_tree_t *self, int direction,
        const double * restrict out_breakpoints,
        const tsk_id_t * restrict out_order,
        tsk_id_t *out_index,
        const double * restrict in_breakpoints,
        const tsk_id_t * restrict in_order,
        tsk_id_t *in_index)
{
    int ret = 0;
    const int direction_change = direction * (direction != self->direction);
    tsk_id_t in = *in_index + direction_change;
    tsk_id_t out = *out_index + direction_change;
    tsk_id_t k, p, c, u, v, root, lsib, rsib, lroot, rroot;
    const tsk_table_collection_t *tables = self->tree_sequence->tables;
    const double sequence_length = tables->sequence_length;
    const tsk_id_t num_edges = (tsk_id_t) tables->edges.num_rows;
    const tsk_id_t * restrict edge_parent = tables->edges.parent;
    const tsk_id_t * restrict edge_child = tables->edges.child;
    const tsk_flags_t * restrict node_flags = tables->nodes.flags;
    tsk_id_t * restrict parent = self->parent;
    tsk_id_t * restrict left_child = self->left_child;
    tsk_id_t * restrict right_child = self->right_child;
    tsk_id_t * restrict left_sib = self->left_sib;
    tsk_id_t * restrict right_sib = self->right_sib;
    bool * restrict above_sample = self->above_sample;
    bool currently_above_sample;
    double x;

    if (direction == TSK_DIR_FORWARD) {
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
        if (self->options & TSK_SAMPLE_COUNTS) {
            tsk_tree_propagate_sample_count_loss(self, parent, p, c);
        }
        if (self->options & TSK_SAMPLE_LISTS) {
            tsk_tree_update_sample_lists(self, parent, left_child, right_sib, p);
        }

        /* Update the roots. If c is not above a sample then we have nothing to do
         * as we cannot affect the status of any roots. */
        if (above_sample[c]) {
            /* Compute the new above sample status for the nodes from p up to root. */
            v = p;
            root = v;
            currently_above_sample = false;
            while (v != TSK_NULL && !currently_above_sample) {
                currently_above_sample = !!(node_flags[v] & TSK_NODE_IS_SAMPLE);
                u = left_child[v];
                while (u != TSK_NULL && !currently_above_sample) {
                    currently_above_sample = above_sample[u];
                    u = right_sib[u];
                }
                above_sample[v] = currently_above_sample;
                root = v;
                v = parent[v];
            }
            if (!currently_above_sample) {
                /* root is no longer above samples. Remove it from the root list */
                lroot = left_sib[root];
                rroot = right_sib[root];
                self->left_root = TSK_NULL;
                if (lroot != TSK_NULL) {
                    right_sib[lroot] = rroot;
                    self->left_root = lroot;
                }
                if (rroot != TSK_NULL) {
                    left_sib[rroot] = lroot;
                    self->left_root = rroot;
                }
                left_sib[root] = TSK_NULL;
                right_sib[root] = TSK_NULL;
            }
            /* Add c to the root list */
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

    while (in >= 0 && in < num_edges && in_breakpoints[in_order[in]] == x) {
        k = in_order[in];
        in += direction;
        p = edge_parent[k];
        c = edge_child[k];
        if (parent[c] != TSK_NULL) {
            ret = TSK_ERR_BAD_EDGES_CONTRADICTORY_CHILDREN;
            goto out;
        }
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
        if (self->options & TSK_SAMPLE_COUNTS) {
            tsk_tree_propagate_sample_count_gain(self, parent, p, c);
        }
        if (self->options & TSK_SAMPLE_LISTS) {
            tsk_tree_update_sample_lists(self, parent, left_child, right_sib, p);
        }

        /* Update the roots. */
        if (above_sample[c]) {
            v = p;
            root = v;
            currently_above_sample = false;
            while (v != TSK_NULL && !currently_above_sample) {
                currently_above_sample = above_sample[v];
                above_sample[v] = true;
                root = v;
                v = parent[v];
            }
            if (!currently_above_sample) {
                /* Replace c with root in root list */
                if (lsib != TSK_NULL) {
                    right_sib[lsib] = root;
                }
                if (rsib != TSK_NULL) {
                    left_sib[rsib] = root;
                }
                left_sib[root] = lsib;
                right_sib[root] = rsib;
                self->left_root = root;
            } else {
                /* Remove c from root list */
                self->left_root = TSK_NULL;
                if (lsib != TSK_NULL) {
                    right_sib[lsib] = rsib;
                    self->left_root = lsib;
                }
                if (rsib != TSK_NULL) {
                    left_sib[rsib] = lsib;
                    self->left_root = rsib;
                }
            }
        }
    }

    if (self->left_root != TSK_NULL) {
        /* Ensure that left_root is the left-most root */
        while (left_sib[self->left_root] != TSK_NULL) {
            self->left_root = left_sib[self->left_root];
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
    assert(self->left < self->right);
    *out_index = out;
    *in_index = in;
    if (tables->sites.num_rows > 0) {
        self->sites = self->tree_sequence->tree_sites[self->index];
        self->sites_length = self->tree_sequence->tree_sites_length[self->index];
    }
    ret = 1;
out:
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

        ret = tsk_tree_advance(self, TSK_DIR_FORWARD,
                tables->edges.right, tables->indexes.edge_removal_order,
                &self->right_index, tables->edges.left,
                tables->indexes.edge_insertion_order, &self->left_index);
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_last(tsk_tree_t *self)
{
    int ret = 1;
    tsk_treeseq_t *ts = self->tree_sequence;
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

        ret = tsk_tree_advance(self, TSK_DIR_REVERSE,
                tables->edges.left, tables->indexes.edge_insertion_order,
                &self->left_index, tables->edges.right,
                tables->indexes.edge_removal_order, &self->right_index);
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_tree_next(tsk_tree_t *self)
{
    int ret = 0;
    tsk_treeseq_t *ts = self->tree_sequence;
    const tsk_table_collection_t *tables = ts->tables;
    tsk_id_t num_trees = (tsk_id_t) tsk_treeseq_get_num_trees(ts);

    if (self->index == -1) {
        ret = tsk_tree_first(self);
    } else if (self->index < num_trees - 1) {
        ret = tsk_tree_advance(self, TSK_DIR_FORWARD,
                tables->edges.right, tables->indexes.edge_removal_order,
                &self->right_index, tables->edges.left,
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
        ret = tsk_tree_advance(self, TSK_DIR_REVERSE,
                tables->edges.left, tables->indexes.edge_insertion_order,
                &self->left_index, tables->edges.right,
                tables->indexes.edge_removal_order, &self->right_index);
    } else {
        ret = tsk_tree_clear(self);
    }
    return ret;
}

/* ======================================================== *
 * Tree diff iterator.
 * ======================================================== */

int TSK_WARN_UNUSED
tsk_diff_iter_init(tsk_diff_iter_t *self, tsk_treeseq_t *tree_sequence)
{
    int ret = 0;

    assert(tree_sequence != NULL);
    memset(self, 0, sizeof(tsk_diff_iter_t));
    self->num_nodes = tsk_treeseq_get_num_nodes(tree_sequence);
    self->num_edges = tsk_treeseq_get_num_edges(tree_sequence);
    self->tree_sequence = tree_sequence;
    self->insertion_index = 0;
    self->removal_index = 0;
    self->tree_left = 0;
    self->tree_index = (size_t) -1;
    self->edge_list_nodes = malloc(self->num_edges * sizeof(tsk_edge_list_t));
    if (self->edge_list_nodes == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_diff_iter_free(tsk_diff_iter_t *self)
{
    int ret = 0;
    tsk_safe_free(self->edge_list_nodes);
    return ret;
}

void
tsk_diff_iter_print_state(tsk_diff_iter_t *self, FILE *out)
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
        tsk_edge_list_t **edges_out, tsk_edge_list_t **edges_in)
{
    int ret = 0;
    tsk_id_t k;
    const double sequence_length = self->tree_sequence->tables->sequence_length;
    double left = self->tree_left;
    double right = sequence_length;
    size_t next_edge_list_node = 0;
    tsk_treeseq_t *s = self->tree_sequence;
    tsk_edge_list_t *out_head = NULL;
    tsk_edge_list_t *out_tail = NULL;
    tsk_edge_list_t *in_head = NULL;
    tsk_edge_list_t *in_tail = NULL;
    tsk_edge_list_t *w = NULL;
    size_t num_trees = tsk_treeseq_get_num_trees(s);
    const tsk_edge_table_t *edges = &s->tables->edges;
    const tsk_id_t *insertion_order = s->tables->indexes.edge_insertion_order;
    const tsk_id_t *removal_order = s->tables->indexes.edge_removal_order;

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
            right = TSK_MIN(right, edges->left[
                    insertion_order[self->insertion_index]]);
        }
        if (self->removal_index < self->num_edges) {
            right = TSK_MIN(right, edges->right[
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
