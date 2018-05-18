/*
** Copyright (C) 2016-2017 University of Oxford
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

/*
 * Unit tests for the low-level msprime API.
 */

#include "msprime.h"

#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>

#include <gsl/gsl_math.h>
#include <gsl/gsl_randist.h>
#include <CUnit/Basic.h>


/* Global variables used for test in state in the test suite */

char * _tmp_file_name;
FILE * _devnull;


static void
verify_migrations_equal(migration_t *r1, migration_t *r2, double scale)
{
    double eps = 1e-6;

    CU_ASSERT_DOUBLE_EQUAL_FATAL(r1->left * scale, r2->left, eps);
    CU_ASSERT_DOUBLE_EQUAL_FATAL(r1->right * scale, r2->right, eps);
    CU_ASSERT_DOUBLE_EQUAL_FATAL(r1->time, r2->time, eps);
    CU_ASSERT_EQUAL_FATAL(r1->node, r2->node);
    CU_ASSERT_EQUAL_FATAL(r1->source, r2->source);
    CU_ASSERT_EQUAL_FATAL(r1->dest, r2->dest);
}


static void
verify_simulator_tree_sequence_equality(msp_t *msp, tree_sequence_t *tree_seq,
        mutgen_t *mutgen, double scale)
{
    int ret;
    uint32_t num_samples = msp_get_num_samples(msp);
    migration_t *sim_mig_records, ts_mig_record;
    uint32_t j;
    size_t num_migrations;
    node_t node;
    sample_t *samples;
    node_id_t *sample_ids;

    CU_ASSERT_EQUAL_FATAL(
            tree_sequence_get_num_samples(tree_seq),
            msp_get_num_samples(msp));
    CU_ASSERT_EQUAL_FATAL(
            tree_sequence_get_num_edges(tree_seq),
            msp_get_num_edges(msp));
    CU_ASSERT_EQUAL_FATAL(
            tree_sequence_get_num_migrations(tree_seq),
            msp_get_num_migrations(msp));
    ret = msp_get_samples(msp, &samples);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_FATAL(tree_sequence_get_num_nodes(tree_seq) >= num_samples);
    ret = msp_get_migrations(msp, &sim_mig_records);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    num_migrations = msp_get_num_migrations(msp);
    /* TODO add some tests for the edges */
    for (j = 0; j < num_migrations; j++) {
        ret = tree_sequence_get_migration(tree_seq, j, &ts_mig_record);
        CU_ASSERT_EQUAL(ret, 0);
        verify_migrations_equal(&sim_mig_records[j], &ts_mig_record, scale);
    }
    for (j = num_migrations; j < num_migrations + 10; j++) {
        ret = tree_sequence_get_migration(tree_seq, j, &ts_mig_record);
        CU_ASSERT_EQUAL(ret, MSP_ERR_OUT_OF_BOUNDS);
    }
    for (j = 0; j < num_samples; j++) {
        ret = tree_sequence_get_node(tree_seq, j, &node);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_EQUAL(node.population, samples[j].population_id);
        CU_ASSERT_EQUAL(node.time, samples[j].time);
    }
    /* Samples should always be 0..n - 1 here for simulations */
    ret = tree_sequence_get_samples(tree_seq, &sample_ids);
    CU_ASSERT_FATAL(sample_ids != NULL);
    for (j = 0; j < num_samples; j++) {
        CU_ASSERT_EQUAL(j, sample_ids[j]);
    }
    mutgen_print_state(mutgen, _devnull);
    tree_sequence_print_state(tree_seq, _devnull);
}



/* Simple unit tests for the Fenwick tree API. */
static void
test_fenwick(void)
{
    fenwick_t t;
    int64_t s;
    size_t j, n;
    for (n = 1; n < 100; n++) {
        s = 0;
        CU_ASSERT(fenwick_alloc(&t, n) == 0);
        for (j = 1; j <= n; j++) {
            fenwick_increment(&t, j, (int64_t) j);
            s = s + (int64_t) j;
            CU_ASSERT(fenwick_get_value(&t, j) == (int64_t) j);
            CU_ASSERT(fenwick_get_cumulative_sum(&t, j) == s);
            CU_ASSERT(fenwick_get_total(&t) == s);
            CU_ASSERT(fenwick_find(&t, s) == j);
            fenwick_set_value(&t, j, 0);
            CU_ASSERT(fenwick_get_value(&t, j) == 0);
            CU_ASSERT(fenwick_get_cumulative_sum(&t, j) == s - (int64_t) j);
            fenwick_set_value(&t, j, (int64_t) j);
            CU_ASSERT(fenwick_get_value(&t, j) == (int64_t) j);
            /* Just make sure that we're seeing the same values even when
             * we expand.
             */
            CU_ASSERT(fenwick_expand(&t, 1) == 0);
        }
        CU_ASSERT(fenwick_free(&t) == 0);
    }
}

static void
test_fenwick_expand(void)
{
    fenwick_t t1, t2;
    int64_t s;
    size_t j, n;

    for (n = 1; n < 100; n++) {
        s = n;
        CU_ASSERT(fenwick_alloc(&t1, n) == 0);
        CU_ASSERT(fenwick_alloc(&t2, 3 * n) == 0);
        for (j = 1; j <= n; j++) {
            fenwick_increment(&t1, j, s);
            fenwick_increment(&t2, j, s);
            CU_ASSERT(fenwick_get_value(&t1, j) == s);
            CU_ASSERT(fenwick_get_value(&t2, j) == s);
        }
        /* After we expand, the internal tree values should be identical */
        CU_ASSERT(t1.size != t2.size);
        CU_ASSERT(fenwick_expand(&t1, 2 * n) == 0);
        CU_ASSERT_EQUAL(t1.size, t2.size);
        CU_ASSERT_EQUAL(memcmp(t1.tree, t2.tree, (t2.size + 1) * sizeof(int64_t)), 0);
        CU_ASSERT_EQUAL(memcmp(t1.values, t2.values, (t2.size + 1) * sizeof(int64_t)), 0);
        CU_ASSERT(fenwick_free(&t1) == 0);
        CU_ASSERT(fenwick_free(&t2) == 0);
    }
}

static void
test_single_locus_two_populations(void)
{
    int ret;
    msp_t msp;
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);
    sample_t samples[] = {{0, 0.0}, {0, 0.0}, {1, 40.0}};
    edge_t *edges;
    node_t *nodes;
    migration_t *migrations;
    size_t num_edges, num_migrations;
    uint32_t n = 3;
    double t0 = 30.0;
    double t1 = 30.5;
    double t2 = 40.5;

    CU_ASSERT_FATAL(rng != NULL);

    ret = msp_alloc(&msp, n, samples, rng);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_num_populations(&msp, 2);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_store_migrations(&msp, true);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_add_mass_migration(&msp, t0, 0, 1, 1.0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_add_mass_migration(&msp, t1, 1, 0, 1.0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_add_mass_migration(&msp, t2, 1, 0, 1.0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_initialise(&msp);
    CU_ASSERT_EQUAL(ret, 0);

    msp_print_state(&msp, _devnull);
    ret = msp_run(&msp, DBL_MAX, ULONG_MAX);
    CU_ASSERT_EQUAL(ret, 0);
    msp_verify(&msp);
    msp_print_state(&msp, _devnull);

    CU_ASSERT_EQUAL_FATAL(msp_get_num_nodes(&msp), 5);
    ret = msp_get_nodes(&msp, &nodes);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(nodes[0].time, 0);
    CU_ASSERT_EQUAL(nodes[0].population, 0);
    CU_ASSERT_EQUAL(nodes[1].time, 0);
    CU_ASSERT_EQUAL(nodes[1].population, 0);
    CU_ASSERT_EQUAL(nodes[2].time, 40.0);
    CU_ASSERT_EQUAL(nodes[2].population, 1);
    CU_ASSERT_TRUE(nodes[3].time < 40);
    CU_ASSERT_TRUE(nodes[3].population == 0);
    CU_ASSERT_TRUE(nodes[4].time > 40.5);
    CU_ASSERT_TRUE(nodes[4].population == 0);

    num_edges = msp_get_num_edges(&msp);
    CU_ASSERT_EQUAL_FATAL(num_edges, 4);
    ret = msp_get_edges(&msp, &edges);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(edges[0].parent, 3);
    CU_ASSERT_EQUAL(edges[1].parent, 3);
    CU_ASSERT_EQUAL(edges[2].parent, 4);
    CU_ASSERT_EQUAL(edges[3].parent, 4);

    num_migrations = msp_get_num_migrations(&msp);
    CU_ASSERT_EQUAL_FATAL(num_migrations, 3);
    ret = msp_get_migrations(&msp, &migrations);
    CU_ASSERT_EQUAL(migrations[0].time, t0)
    CU_ASSERT_EQUAL(migrations[0].source, 0);
    CU_ASSERT_EQUAL(migrations[0].dest, 1);
    CU_ASSERT_EQUAL(migrations[1].time, t1);
    CU_ASSERT_EQUAL(migrations[1].source, 1);
    CU_ASSERT_EQUAL(migrations[1].dest, 0);
    CU_ASSERT_EQUAL(migrations[2].time, t2);
    CU_ASSERT_EQUAL(migrations[2].source, 1);
    CU_ASSERT_EQUAL(migrations[2].dest, 0);

    ret = msp_free(&msp);
    CU_ASSERT_EQUAL(ret, 0);
    gsl_rng_free(rng);
}

static void
test_single_locus_many_populations(void)
{
    int ret;
    msp_t msp;
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);
    uint32_t num_populations = 500;
    sample_t samples[] = {{0, 0.0}, {num_populations - 1, 0.0}};
    edge_t *edges;
    node_t *nodes;
    uint32_t n = 2;

    CU_ASSERT_FATAL(rng != NULL);

    ret = msp_alloc(&msp, n, samples, rng);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_num_populations(&msp, num_populations);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = msp_add_mass_migration(&msp, 30.0, 0, num_populations - 1, 1.0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_initialise(&msp);
    CU_ASSERT_EQUAL(ret, 0);

    msp_print_state(&msp, _devnull);
    ret = msp_run(&msp, DBL_MAX, ULONG_MAX);
    CU_ASSERT_EQUAL(ret, 0);
    msp_verify(&msp);
    msp_print_state(&msp, _devnull);
    CU_ASSERT_EQUAL_FATAL(msp_get_num_edges(&msp), 2);
    CU_ASSERT_EQUAL_FATAL(msp_get_num_nodes(&msp), 3);
    ret = msp_get_edges(&msp, &edges);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(edges[0].parent, 2);
    CU_ASSERT_EQUAL(edges[1].parent, 2);
    ret = msp_get_nodes(&msp, &nodes);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_TRUE(nodes[2].time > 30.0);
    CU_ASSERT_EQUAL(nodes[2].population, num_populations - 1);

    ret = msp_free(&msp);
    CU_ASSERT_EQUAL(ret, 0);
    gsl_rng_free(rng);
}

static void
test_single_locus_historical_sample(void)
{
    int ret;
    msp_t msp;
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);
    sample_t samples[] = {{0, 0.0}, {0, 10.0}};
    edge_t *edges;
    node_t *nodes;
    uint32_t n = 2;

    CU_ASSERT_FATAL(rng != NULL);

    ret = msp_alloc(&msp, n, samples, rng);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_initialise(&msp);
    CU_ASSERT_EQUAL(ret, 0);

    msp_print_state(&msp, _devnull);
    ret = msp_run(&msp, DBL_MAX, ULONG_MAX);
    CU_ASSERT_EQUAL(ret, 0);
    msp_verify(&msp);
    msp_print_state(&msp, _devnull);

    CU_ASSERT_EQUAL_FATAL(msp_get_num_nodes(&msp), 3);
    ret = msp_get_nodes(&msp, &nodes);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(nodes[0].time, 0);
    CU_ASSERT_EQUAL(nodes[1].time, 10);
    CU_ASSERT_TRUE(nodes[2].time > 10);

    CU_ASSERT_EQUAL_FATAL(msp_get_num_edges(&msp), 2);
    ret = msp_get_edges(&msp, &edges);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(edges[0].left, 0);
    CU_ASSERT_EQUAL(edges[0].right, 1);
    CU_ASSERT_EQUAL(edges[0].parent, 2);
    CU_ASSERT_EQUAL(edges[1].left, 0);
    CU_ASSERT_EQUAL(edges[1].right, 1);
    CU_ASSERT_EQUAL(edges[1].parent, 2);

    ret = msp_free(&msp);
    CU_ASSERT_EQUAL(ret, 0);
    gsl_rng_free(rng);
}

static void
test_simulator_getters_setters(void)
{
    int ret;
    uint32_t j;
    uint32_t n = 10;
    uint32_t m = 10;
    sample_t *samples = malloc(n * sizeof(sample_t));
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);
    double migration_matrix[] = {0, 0, 0, 0};
    double matrix[4], growth_rate, initial_size;
    double Ne = 4;
    size_t migration_events[4];
    size_t breakpoints[m];
    msp_t msp;

    CU_ASSERT_FATAL(samples != NULL);
    CU_ASSERT_FATAL(rng != NULL);

    for (j = 0; j < n; j++) {
        samples[j].time = j;
        samples[j].population_id = j % 2;
    }
    CU_ASSERT_EQUAL(msp_alloc(&msp, 0, NULL, rng), MSP_ERR_BAD_PARAM_VALUE);
    msp_free(&msp);
    samples[0].time = 1.0;
    CU_ASSERT_EQUAL(msp_alloc(&msp, n, samples, rng), MSP_ERR_BAD_SAMPLES);
    msp_free(&msp);
    samples[0].time = -1.0;
    CU_ASSERT_EQUAL(msp_alloc(&msp, n, samples, rng), MSP_ERR_BAD_PARAM_VALUE);
    msp_free(&msp);
    samples[0].time = 0.0;

    ret = msp_alloc(&msp, n, samples, rng);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(msp_set_max_memory(&msp, 0), MSP_ERR_BAD_PARAM_VALUE);
    CU_ASSERT_EQUAL(msp_set_node_mapping_block_size(&msp, 0),
            MSP_ERR_BAD_PARAM_VALUE);
    CU_ASSERT_EQUAL(msp_set_segment_block_size(&msp, 0),
            MSP_ERR_BAD_PARAM_VALUE);
    CU_ASSERT_EQUAL(msp_set_avl_node_block_size(&msp, 0),
            MSP_ERR_BAD_PARAM_VALUE);
    CU_ASSERT_EQUAL(msp_set_edge_block_size(&msp, 0),
            MSP_ERR_BAD_PARAM_VALUE);
    CU_ASSERT_EQUAL(msp_set_migration_block_size(&msp, 0),
            MSP_ERR_BAD_PARAM_VALUE);
    CU_ASSERT_EQUAL(msp_set_num_loci(&msp, 0), MSP_ERR_BAD_PARAM_VALUE);
    CU_ASSERT_EQUAL(msp_set_num_populations(&msp, 0), MSP_ERR_BAD_PARAM_VALUE);
    CU_ASSERT_EQUAL(
            msp_set_recombination_rate(&msp, -1),
            MSP_ERR_BAD_PARAM_VALUE);
    CU_ASSERT_EQUAL(
            msp_set_population_configuration(&msp, -1, 0, 0),
            MSP_ERR_BAD_POPULATION_ID);
    CU_ASSERT_EQUAL(
            msp_set_population_configuration(&msp, 3, 0, 0),
            MSP_ERR_BAD_POPULATION_ID);

    ret = msp_set_simulation_model_hudson(&msp, Ne);
    CU_ASSERT_EQUAL(msp_get_model(&msp)->type, MSP_MODEL_HUDSON);
    CU_ASSERT_EQUAL(msp_get_model(&msp)->population_size, Ne);

    ret = msp_set_num_populations(&msp, 2);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(
            msp_get_population_configuration(&msp, 3, NULL, NULL),
            MSP_ERR_BAD_POPULATION_ID);
    ret = msp_set_population_configuration(&msp, 0, 2 * Ne, 0.5);
    CU_ASSERT_EQUAL(ret, 0);

    CU_ASSERT_EQUAL(
            msp_set_migration_matrix(&msp, 0, NULL),
            MSP_ERR_BAD_MIGRATION_MATRIX);
    CU_ASSERT_EQUAL(
            msp_set_migration_matrix(&msp, 3, migration_matrix),
            MSP_ERR_BAD_MIGRATION_MATRIX);
    migration_matrix[0] = 1;
    CU_ASSERT_EQUAL(
            msp_set_migration_matrix(&msp, 4, migration_matrix),
            MSP_ERR_BAD_MIGRATION_MATRIX);
    migration_matrix[0] = 0;
    migration_matrix[1] = -1;
    CU_ASSERT_EQUAL(
            msp_set_migration_matrix(&msp, 4, migration_matrix),
            MSP_ERR_BAD_MIGRATION_MATRIX);

    migration_matrix[1] = 1;
    migration_matrix[2] = 1;
    ret = msp_set_migration_matrix(&msp, 4, migration_matrix);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_num_loci(&msp, m);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_recombination_rate(&msp, 1.0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_store_migrations(&msp, true);
    CU_ASSERT_EQUAL(ret, 0);

    ret = msp_initialise(&msp);
    CU_ASSERT_EQUAL(ret, 0);

    ret = msp_get_population_configuration(&msp, 0, &initial_size, &growth_rate);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(initial_size, 2 * Ne);
    CU_ASSERT_EQUAL(growth_rate, 0.5);
    CU_ASSERT_EQUAL(msp_get_recombination_rate(&msp), 1.0);

    CU_ASSERT_TRUE(msp_get_store_migrations(&msp));
    CU_ASSERT_EQUAL(msp_get_num_avl_node_blocks(&msp), 1);
    CU_ASSERT_EQUAL(msp_get_num_node_mapping_blocks(&msp), 1);
    CU_ASSERT_EQUAL(msp_get_num_segment_blocks(&msp), 1);
    CU_ASSERT_EQUAL(msp_get_num_edge_blocks(&msp), 1);
    CU_ASSERT_EQUAL(msp_get_num_node_blocks(&msp), 1);
    CU_ASSERT_EQUAL(msp_get_num_migration_blocks(&msp), 1);
    CU_ASSERT(msp_get_used_memory(&msp) > 0);
    CU_ASSERT_EQUAL(msp_get_num_populations(&msp), 2);

    ret = msp_run(&msp, DBL_MAX, ULONG_MAX);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(msp_get_num_breakpoints(&msp), m - 1);
    ret = msp_get_breakpoints(&msp, breakpoints);
    CU_ASSERT_EQUAL(ret, 0);
    for (j = 0; j < m - 1; j++) {
        CU_ASSERT_EQUAL(breakpoints[j], j + 1);
    }
    ret = msp_get_num_migration_events(&msp, migration_events);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(migration_events[0], 0);
    CU_ASSERT(migration_events[1] > 0);
    CU_ASSERT(migration_events[2] > 0);
    CU_ASSERT_EQUAL(migration_events[3], 0);
    ret = msp_get_migration_matrix(&msp, matrix);
    CU_ASSERT_EQUAL(ret, 0);
    for (j = 0; j < 4; j++) {
        CU_ASSERT_EQUAL(matrix[j], migration_matrix[j]);
    }
    CU_ASSERT(msp_get_num_common_ancestor_events(&msp) > 0);
    CU_ASSERT(msp_get_num_recombination_events(&msp) > 0);

    free(samples);
    ret = msp_free(&msp);
    CU_ASSERT_EQUAL(ret, 0);
    gsl_rng_free(rng);
}

static void
test_simulator_model_errors(void)
{
    uint32_t n = 10;
    uint32_t j;
    sample_t *samples = malloc(n * sizeof(sample_t));
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);
    msp_t msp;

    CU_ASSERT_FATAL(samples != NULL);
    CU_ASSERT_FATAL(rng != NULL);
    for (j = 0; j < n; j++) {
        samples[j].time = 0;
        samples[j].population_id = 0;
    }

    CU_ASSERT_EQUAL(msp_alloc(&msp, n, samples, rng), 0);
    CU_ASSERT_EQUAL(msp_get_model(&msp)->type, MSP_MODEL_HUDSON);
    CU_ASSERT_EQUAL(msp_add_simple_bottleneck(&msp, 1, 0, 1), 0);
    CU_ASSERT_EQUAL(msp_add_instantaneous_bottleneck(&msp, 1, 0, 1), 0);
    CU_ASSERT_EQUAL(msp_initialise(&msp), 0);
    CU_ASSERT_EQUAL(msp_run(&msp, DBL_MAX, ULONG_MAX), 0);
    CU_ASSERT_EQUAL(msp_free(&msp), 0);

    for (j = 0; j < 2; j++) {
        CU_ASSERT_EQUAL(msp_alloc(&msp, n, samples, rng), 0);
        if (j == 0) {
            CU_ASSERT_EQUAL(msp_set_simulation_model_smc(&msp, 0.25), 0);
        } else {
            CU_ASSERT_EQUAL(msp_set_simulation_model_smc_prime(&msp, 0.25), 0);
        }
        CU_ASSERT_EQUAL(msp_add_simple_bottleneck(&msp, 1, 0, 1), MSP_ERR_BAD_MODEL);
        CU_ASSERT_EQUAL(msp_add_instantaneous_bottleneck(&msp, 1, 0, 1),
                MSP_ERR_BAD_MODEL);
        CU_ASSERT_EQUAL(msp_free(&msp), 0);
    }

    free(samples);
    gsl_rng_free(rng);
}

static void
test_demographic_events(void)
{
    int ret;
    uint32_t j, k;
    uint32_t n = 10;
    uint32_t m = 10;
    sample_t *samples = malloc(n * sizeof(sample_t));
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);
    double migration_matrix[] = {0, 1, 1, 0};
    double last_time, time, pop_size;
    msp_t msp;

    CU_ASSERT_FATAL(samples != NULL);
    CU_ASSERT_FATAL(rng != NULL);

    for (j = 0; j < n; j++) {
        samples[j].time = j;
        samples[j].population_id = j % 2;
    }
    ret = msp_alloc(&msp, n, samples, rng);
    CU_ASSERT_EQUAL(ret, 0);
    /* Zero or negative population sizes are not allowed */
    ret = msp_set_population_configuration(&msp, 0, -1, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = msp_set_population_configuration(&msp, 0, 0, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);

    ret = msp_set_num_populations(&msp, 2);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_population_configuration(&msp, 0, 1, 1);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_population_configuration(&msp, 1, 2, 2);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_migration_matrix(&msp, 4, migration_matrix);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_num_loci(&msp, m);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_recombination_rate(&msp, 1.0);
    CU_ASSERT_EQUAL(ret, 0);

    CU_ASSERT_EQUAL(
        msp_add_mass_migration(&msp, 10, -1, 0, 1),
        MSP_ERR_BAD_POPULATION_ID);
    CU_ASSERT_EQUAL(
        msp_add_mass_migration(&msp, 10, 2, 0, 1),
        MSP_ERR_BAD_POPULATION_ID);
    CU_ASSERT_EQUAL(
        msp_add_mass_migration(&msp, 10, 0, 0, 1),
        MSP_ERR_SOURCE_DEST_EQUAL);
    CU_ASSERT_EQUAL(
        msp_add_mass_migration(&msp, 10, 0, 1, -5),
        MSP_ERR_BAD_PARAM_VALUE);

    CU_ASSERT_EQUAL(
        msp_add_migration_rate_change(&msp, 10, -2, 2.0),
        MSP_ERR_BAD_MIGRATION_MATRIX_INDEX);
    CU_ASSERT_EQUAL(
        msp_add_migration_rate_change(&msp, 10, -1, -2.0),
        MSP_ERR_BAD_PARAM_VALUE);
    CU_ASSERT_EQUAL(
        msp_add_migration_rate_change(&msp, 10, 3, 2.0),
        MSP_ERR_DIAGONAL_MIGRATION_MATRIX_INDEX);

    CU_ASSERT_EQUAL(
        msp_add_population_parameters_change(&msp, 10, -2, 0, 0),
        MSP_ERR_BAD_POPULATION_ID);
    CU_ASSERT_EQUAL(
        msp_add_population_parameters_change(&msp, 10, -1, -1, 0),
        MSP_ERR_BAD_PARAM_VALUE);
    CU_ASSERT_EQUAL(
        msp_add_population_parameters_change(&msp, 10, -1, GSL_NAN, GSL_NAN),
        MSP_ERR_BAD_PARAM_VALUE);

    CU_ASSERT_EQUAL(
        msp_add_simple_bottleneck(&msp, 10, -1, 0),
        MSP_ERR_BAD_POPULATION_ID);
    CU_ASSERT_EQUAL(
        msp_add_simple_bottleneck(&msp, 10, 0, -1),
        MSP_ERR_BAD_PARAM_VALUE);
    CU_ASSERT_EQUAL(
        msp_add_simple_bottleneck(&msp, 10, 0, 1.1),
        MSP_ERR_BAD_PARAM_VALUE);

    CU_ASSERT_EQUAL(
        msp_add_instantaneous_bottleneck(&msp, 10, 2, 0),
        MSP_ERR_BAD_POPULATION_ID);
    CU_ASSERT_EQUAL(
        msp_add_simple_bottleneck(&msp, 10, 0, -1),
        MSP_ERR_BAD_PARAM_VALUE);
    CU_ASSERT_EQUAL_FATAL(
        msp_add_simple_bottleneck(&msp, 10, -1, 0),
        MSP_ERR_BAD_POPULATION_ID);

    ret = msp_add_mass_migration(&msp, 0.1, 0, 1, 0.5);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_add_migration_rate_change(&msp, 0.2, 1, 2.0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_add_migration_rate_change(&msp, 0.3, -1, 3.0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_add_population_parameters_change(&msp, 0.4, 0, 0.5, 1.0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_add_population_parameters_change(&msp, 0.5, -1, 0.5, 2.0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_add_population_parameters_change(&msp, 0.6, 0, GSL_NAN, 0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_add_population_parameters_change(&msp, 0.7, 1, 1, GSL_NAN);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_add_simple_bottleneck(&msp, 0.8, 0, 0.5);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_add_instantaneous_bottleneck(&msp, 0.9, 0, 2.0);
    CU_ASSERT_EQUAL(ret, 0);

    CU_ASSERT_EQUAL(
            msp_add_mass_migration(&msp, 0.1, 0, 1, 0.5),
            MSP_ERR_UNSORTED_DEMOGRAPHIC_EVENTS);
    CU_ASSERT_EQUAL(
            msp_add_migration_rate_change(&msp, 0.2, 1, 2.0),
            MSP_ERR_UNSORTED_DEMOGRAPHIC_EVENTS);
    CU_ASSERT_EQUAL(
            msp_add_population_parameters_change(&msp, 0.4, 0, 0.5, 1.0),
            MSP_ERR_UNSORTED_DEMOGRAPHIC_EVENTS);
    CU_ASSERT_EQUAL(
            msp_add_simple_bottleneck(&msp, 0.7, 0, 1.0),
            MSP_ERR_UNSORTED_DEMOGRAPHIC_EVENTS);
    CU_ASSERT_EQUAL(
            msp_add_instantaneous_bottleneck(&msp, 0.8, 0, 1.0),
            MSP_ERR_UNSORTED_DEMOGRAPHIC_EVENTS);

    CU_ASSERT_EQUAL(
        msp_debug_demography(&msp, &time),
        MSP_ERR_BAD_STATE);

    ret = msp_initialise(&msp);
    CU_ASSERT_EQUAL(ret, 0);

    j = 0;
    last_time = 0;
    do {
        ret = msp_debug_demography(&msp, &time);
        CU_ASSERT_EQUAL(ret, 0);
        msp_print_state(&msp, _devnull);
        ret = msp_compute_population_size(&msp, 10, last_time, &pop_size);
        CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_POPULATION_ID);
        for (k = 0; k < 2; k++) {
            ret = msp_compute_population_size(&msp, k, last_time, &pop_size);
            CU_ASSERT_EQUAL(ret, 0);
            CU_ASSERT_TRUE(pop_size >= 0);
            ret = msp_compute_population_size(&msp, k, time, &pop_size);
            CU_ASSERT_EQUAL(ret, 0);
            CU_ASSERT_TRUE(pop_size >= 0);
            ret = msp_compute_population_size(&msp, k, last_time + (time - last_time) / 2,
                    &pop_size);
            CU_ASSERT_EQUAL(ret, 0);
            CU_ASSERT_TRUE(pop_size >= 0);
        }
        j++;
        last_time = time;
    } while (! gsl_isinf(time));
    CU_ASSERT_EQUAL(j, 10);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(
        msp_run(&msp, DBL_MAX, ULONG_MAX),
        MSP_ERR_BAD_STATE);
    ret = msp_reset(&msp);
    CU_ASSERT_EQUAL(ret, 0);

    msp_print_state(&msp, _devnull);
    ret = msp_run(&msp, DBL_MAX, ULONG_MAX);
    CU_ASSERT_EQUAL(ret, 0);

    free(samples);
    ret = msp_free(&msp);
    CU_ASSERT_EQUAL(ret, 0);
    gsl_rng_free(rng);
}

static int
get_num_children(size_t node, size_t num_edges, edge_t *edges)
{
    int num_children = 0;
    size_t i;

    for (i = 0; i < num_edges; i++) {
        if (edges[i].parent == node) {
            num_children++;
        }
    }
    return num_children;
}

static void
test_dtwf_deterministic(void)
{
    int j, ret;
    uint32_t n = 10;
    uint32_t m = 2;
    unsigned long seed = 133;
    sample_t *samples = malloc(n * sizeof(sample_t));
    msp_t *msp = malloc(sizeof(msp_t));
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);
    node_table_t nodes[2];
    edge_table_t edges[2];
    migration_table_t migrations;
    population_table_t populations;

    CU_ASSERT_FATAL(msp != NULL);
    CU_ASSERT_FATAL(samples != NULL);
    CU_ASSERT_FATAL(rng != NULL);

    ret = migration_table_alloc(&migrations, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = population_table_alloc(&populations, 0, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    memset(samples, 0, n * sizeof(sample_t));
    for (j = 0; j < 2; j++) {
        ret = node_table_alloc(&nodes[j], 0, 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        ret = edge_table_alloc(&edges[j], 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);

        gsl_rng_set(rng, seed);
        ret = msp_alloc(msp, n, samples, rng);
        CU_ASSERT_EQUAL(ret, 0);
        ret = msp_set_simulation_model_dtwf(msp, n);
        CU_ASSERT_EQUAL(ret, 0);
        ret = msp_set_population_configuration(msp, 0, n, 0);
        CU_ASSERT_EQUAL(ret, 0);
        ret = msp_set_num_loci(msp, m);
        CU_ASSERT_EQUAL(ret, 0);
        ret = msp_set_recombination_rate(msp, 1.0);
        CU_ASSERT_EQUAL(ret, 0);
        ret = msp_initialise(msp);
        CU_ASSERT_EQUAL(ret, 0);
        ret = msp_run(msp, DBL_MAX, UINT32_MAX);
        CU_ASSERT_EQUAL(ret, 0);
        msp_verify(msp);
        ret = msp_populate_tables(msp, NULL, &nodes[j], &edges[j], &migrations,
                &populations);
        CU_ASSERT_EQUAL(ret, 0);
        msp_free(msp);
        CU_ASSERT_EQUAL(migrations.num_rows, 0);
        CU_ASSERT(nodes[j].num_rows > 0);
        CU_ASSERT(edges[j].num_rows > 0);

    }
    CU_ASSERT_TRUE(node_table_equal(&nodes[0], &nodes[1]));
    CU_ASSERT_TRUE(edge_table_equal(&edges[0], &edges[1]));

    CU_ASSERT_EQUAL(ret, 0);
    gsl_rng_free(rng);
    free(msp);
    free(samples);
    for (j = 0; j < 2; j++) {
        node_table_free(&nodes[j]);
        edge_table_free(&edges[j]);
    }
    migration_table_free(&migrations);
    population_table_free(&populations);
}

static void
test_mixed_model_simulation(void)
{
    int ret;
    uint32_t j, k;
    uint32_t n = 10;
    double N = 100;
    int model;
    double initial_size, growth_rate;
    const char *model_name;
    table_collection_t tables;
    tree_sequence_t ts;
    double g = -1.0 / 8192;
    sample_t *samples = malloc(n * sizeof(sample_t));
    msp_t *msp = malloc(sizeof(msp_t));
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);

    CU_ASSERT_FATAL(msp != NULL);
    CU_ASSERT_FATAL(samples != NULL);
    CU_ASSERT_FATAL(rng != NULL);

    memset(samples, 0, n * sizeof(sample_t));
    ret = msp_alloc(msp, n, samples, rng);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_num_populations(msp, 3);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_simulation_model_dtwf(msp, N);
    CU_ASSERT_EQUAL(ret, 0);
    /* Set the populations to 1, 2, and 3N. We don't simulate them,
     * but they should be equal at the end */
    ret = msp_set_population_configuration(msp, 0, N, 0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_population_configuration(msp, 1, 2 * N, g);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_population_configuration(msp, 2, 3 * N, 2 * g);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_num_loci(msp, 10);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_recombination_rate(msp, 1.0);
    CU_ASSERT_EQUAL(ret, 0);

    ret = msp_initialise(msp);
    CU_ASSERT_EQUAL(ret, 0);

    /* Run for 10 generations each for the different models, alternating */
    j = 0;
    while ((ret = msp_run(msp, j * 10, UINT32_MAX)) == 2) {
        msp_verify(msp);
        /* Check that our populations and growth rates are still correct */
        for (k = 0; k < 3; k++) {
            ret = msp_get_population_configuration(msp, k, &initial_size, &growth_rate);
            CU_ASSERT_EQUAL(ret, 0);
            CU_ASSERT_EQUAL(initial_size, (k + 1) * N);
            CU_ASSERT_EQUAL_FATAL(growth_rate, k * g);
        }
        CU_ASSERT_FALSE(msp_is_completed(msp));
        if (j % 2 == 1) {
            model = msp_get_model(msp)->type;
            CU_ASSERT_EQUAL(model, MSP_MODEL_HUDSON);
            model_name = msp_get_model_name(msp);
            CU_ASSERT_STRING_EQUAL(model_name, "hudson");
            ret = msp_set_simulation_model_dtwf(msp, N);
            CU_ASSERT_EQUAL(ret, 0);
        } else {
            model = msp_get_model(msp)->type;
            CU_ASSERT_EQUAL(model, MSP_MODEL_DTWF);
            model_name = msp_get_model_name(msp);
            CU_ASSERT_STRING_EQUAL(model_name, "dtwf");
            ret = msp_set_simulation_model_hudson(msp, N);
            CU_ASSERT_EQUAL(ret, 0);
        }
        j++;
    }
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_TRUE(msp_is_completed(msp));
    CU_ASSERT_TRUE(j > 5);

    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    /* Make sure we can build a tree sequence from the tables. */
    ret = msp_populate_tables(msp, NULL, &tables.nodes, &tables.edges,
            &tables.migrations, &tables.populations);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* TODO remove this when populate tables takes table_collection as arg */
    tables.sequence_length = msp->num_loci;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    tree_sequence_print_state(&ts, _devnull);
    tree_sequence_free(&ts);
    table_collection_free(&tables);

    ret = msp_free(msp);
    CU_ASSERT_EQUAL(ret, 0);
    gsl_rng_free(rng);
    free(msp);
    free(samples);
}

static void
test_dtwf_single_locus_simulation(void)
{
    int ret;
    const char *model_name;
    int i;
    uint32_t n = 10;
    sample_t *samples = malloc(n * sizeof(sample_t));
    msp_t *msp = malloc(sizeof(msp_t));
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);
    int num_coalescent_events = 0;
    int num_children;

    CU_ASSERT_FATAL(msp != NULL);
    CU_ASSERT_FATAL(samples != NULL);
    CU_ASSERT_FATAL(rng != NULL);

    memset(samples, 0, n * sizeof(sample_t));
    ret = msp_alloc(msp, n, samples, rng);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_initialise(msp);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_simulation_model_dtwf(msp, n);
    CU_ASSERT_EQUAL(ret, 0);
    model_name = msp_get_model_name(msp);
    CU_ASSERT_STRING_EQUAL(model_name, "dtwf");

    ret = msp_run(msp, DBL_MAX, UINT32_MAX);
    CU_ASSERT_EQUAL(ret, 0);
    msp_verify(msp);

    /* For the single locus sim we should have n-1 coalescent events,
     * counting multiple mergers as multiple coalescent events */
    for (i = 0; i < msp->num_nodes; i++) {
        num_children = get_num_children(i, msp->num_edges, msp->edges);
        if (num_children > 0) {
            num_coalescent_events += num_children - 1;
        }
    }
    CU_ASSERT_EQUAL(num_coalescent_events, n-1);

    ret = msp_free(msp);
    CU_ASSERT_EQUAL(ret, 0);
    gsl_rng_free(rng);
    free(msp);
    free(samples);
}

static void
test_single_locus_simulation(void)
{
    int ret;
    int model;
    const char *model_name;
    uint32_t j;
    uint32_t n = 10;
    sample_t *samples = malloc(n * sizeof(sample_t));
    msp_t *msp = malloc(sizeof(msp_t));
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);

    CU_ASSERT_FATAL(msp != NULL);
    CU_ASSERT_FATAL(samples != NULL);
    CU_ASSERT_FATAL(rng != NULL);

    memset(samples, 0, n * sizeof(sample_t));
    ret = msp_alloc(msp, n, samples, rng);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_initialise(msp);
    CU_ASSERT_EQUAL(ret, 0);

    /* For the single locus sim we should have exactly n - 1 events */
    for (j = 0; j < n - 2; j++) {
        ret = msp_run(msp, DBL_MAX, 1);
        CU_ASSERT_EQUAL(ret, 1);
        msp_verify(msp);
    }
    ret = msp_run(msp, DBL_MAX, 1);
    CU_ASSERT_EQUAL(ret, 0);
    msp_verify(msp);

    model = msp_get_model(msp)->type;
    CU_ASSERT_EQUAL(model, MSP_MODEL_HUDSON);
    model_name = msp_get_model_name(msp);
    CU_ASSERT_STRING_EQUAL(model_name, "hudson");

    ret = msp_free(msp);
    CU_ASSERT_EQUAL(ret, 0);
    gsl_rng_free(rng);
    free(msp);
    free(samples);
}

static void
test_simulation_memory_limit(void)
{
    int ret;
    uint32_t n = 1000;
    sample_t *samples = malloc(n * sizeof(sample_t));
    msp_t *msp = malloc(sizeof(msp_t));
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);

    CU_ASSERT_FATAL(msp != NULL);
    CU_ASSERT_FATAL(samples != NULL);
    CU_ASSERT_FATAL(rng != NULL);

    memset(samples, 0, n * sizeof(sample_t));
    ret = msp_alloc(msp, n, samples, rng);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_max_memory(msp, 1024 * 1024);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_num_loci(msp, 10000);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_recombination_rate(msp, 1.0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_initialise(msp);
    CU_ASSERT_EQUAL(ret, 0);

    ret = msp_run(msp, DBL_MAX, ULONG_MAX);
    CU_ASSERT_EQUAL(ret, MSP_ERR_NO_MEMORY);

    ret = msp_free(msp);
    CU_ASSERT_EQUAL(ret, 0);
    gsl_rng_free(rng);
    free(msp);
    free(samples);
}

static void
test_dtwf_multi_locus_simulation(void)
{
    int ret;
    uint32_t n = 100;
    uint32_t m = 100;
    long seed = 10;
    double migration_matrix[] = {0, 0.1, 0.1, 0};
    const char *model_name;
    size_t num_ca_events, num_re_events;
    double t;

    sample_t *samples = malloc(n * sizeof(sample_t));
    msp_t *msp = malloc(sizeof(msp_t));
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);

    CU_ASSERT_FATAL(msp != NULL);
    CU_ASSERT_FATAL(samples != NULL);
    CU_ASSERT_FATAL(rng != NULL);
    gsl_rng_set(rng, seed);
    memset(samples, 0, n * sizeof(sample_t));
    ret = msp_alloc(msp, n, samples, rng);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_simulation_model_dtwf(msp, n);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_num_populations(msp, 2);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_migration_matrix(msp, 4, migration_matrix);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_store_migrations(msp, true);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_num_loci(msp, m);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_recombination_rate(msp, 1.0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_initialise(msp);
    CU_ASSERT_EQUAL(ret, 0);
    model_name = msp_get_model_name(msp);
    CU_ASSERT_STRING_EQUAL(model_name, "dtwf");

    ret = msp_run(msp, DBL_MAX, ULONG_MAX);
    msp_verify(msp);
    num_ca_events = msp_get_num_common_ancestor_events(msp);
    num_re_events = msp_get_num_recombination_events(msp);
    CU_ASSERT_TRUE(num_ca_events > 0);
    CU_ASSERT_TRUE(num_re_events > 0);
    CU_ASSERT_EQUAL(ret, 0);
    msp_free(msp);

    /* Realloc the simulator under different memory params to see if
     * we get the same result. */
    gsl_rng_set(rng, seed);
    ret = msp_alloc(msp, n, samples, rng);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_simulation_model_dtwf(msp, n);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_num_populations(msp, 2);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_migration_matrix(msp, 4, migration_matrix);
    ret = msp_set_num_loci(msp, m);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_recombination_rate(msp, 1.0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_initialise(msp);
    CU_ASSERT_EQUAL(ret, 0);
    t = 1;
    /* We should be able to step backward here generation-by-generation until
     * coalescence.
     */
    while ((ret = msp_run(msp, t, ULONG_MAX)) > 0) {
        msp_verify(msp);
        CU_ASSERT_EQUAL_FATAL(msp->time, t);
        t++;
    }
    msp_verify(msp);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_TRUE(num_ca_events == msp_get_num_common_ancestor_events(msp));
    CU_ASSERT_TRUE(num_re_events == msp_get_num_recombination_events(msp));

    ret = msp_free(msp);
    CU_ASSERT_EQUAL(ret, 0);
    gsl_rng_free(rng);
    free(msp);
    free(samples);
}

static void
test_multi_locus_simulation(void)
{
    int ret;
    uint32_t num_events;
    uint32_t n = 100;
    uint32_t m = 100;
    long seed = 10;
    int model;
    int models[] = {MSP_MODEL_HUDSON, MSP_MODEL_SMC, MSP_MODEL_SMC_PRIME};
    double migration_matrix[] = {0, 1, 1, 0};
    size_t migration_events[4];
    const char *model_names[] = {"hudson", "smc", "smc_prime"};
    const char *model_name;
    size_t j;

    for (j = 0; j < sizeof(models) / sizeof(int); j++) {
        sample_t *samples = malloc(n * sizeof(sample_t));
        msp_t *msp = malloc(sizeof(msp_t));
        gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);

        CU_ASSERT_FATAL(msp != NULL);
        CU_ASSERT_FATAL(samples != NULL);
        CU_ASSERT_FATAL(rng != NULL);
        gsl_rng_set(rng, seed);
        memset(samples, 0, n * sizeof(sample_t));
        ret = msp_alloc(msp, n, samples, rng);
        CU_ASSERT_EQUAL(ret, 0);
        ret = msp_set_num_populations(msp, 2);
        CU_ASSERT_EQUAL(ret, 0);
        ret = msp_set_migration_matrix(msp, 4, migration_matrix);
        CU_ASSERT_EQUAL(ret, 0);
        ret = msp_set_store_migrations(msp, true);
        CU_ASSERT_EQUAL(ret, 0);
        /* set all the block sizes to something small to provoke the memory
         * expansions. */
        ret = msp_set_avl_node_block_size(msp, 1);
        CU_ASSERT_EQUAL(ret, 0);
        ret = msp_set_node_mapping_block_size(msp, 1);
        CU_ASSERT_EQUAL(ret, 0);
        ret = msp_set_segment_block_size(msp, 1);
        CU_ASSERT_EQUAL(ret, 0);
        ret = msp_set_edge_block_size(msp, 1);
        CU_ASSERT_EQUAL(ret, 0);
        ret = msp_set_migration_block_size(msp, 1);
        CU_ASSERT_EQUAL(ret, 0);
        ret = msp_set_num_loci(msp, m);
        CU_ASSERT_EQUAL(ret, 0);
        ret = msp_set_recombination_rate(msp, 1.0);
        CU_ASSERT_EQUAL(ret, 0);
        switch (j) {
            case 0:
                ret = msp_set_simulation_model_hudson(msp, 0.25);
                break;
            case 1:
                ret = msp_set_simulation_model_smc(msp, 0.25);
                break;
            case 2:
                ret = msp_set_simulation_model_smc_prime(msp, 0.25);
                break;
        }
        CU_ASSERT_EQUAL(ret, 0);
        ret = msp_initialise(msp);
        CU_ASSERT_EQUAL(ret, 0);

        num_events = 0;
        while ((ret = msp_run(msp, DBL_MAX, 1)) == 1) {
            msp_verify(msp);
            num_events++;
        }
        CU_ASSERT_EQUAL(ret, 0);
        msp_verify(msp);
        ret = msp_get_num_migration_events(msp, migration_events);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT(num_events > n - 1);
        CU_ASSERT_EQUAL(1 + num_events,
                /* diagonals must be zero here */
                migration_events[1] + migration_events[2] +
                msp_get_num_recombination_events(msp) +
                msp_get_num_common_ancestor_events(msp) +
                msp_get_num_rejected_common_ancestor_events(msp));
        if (models[j] == MSP_MODEL_HUDSON) {
            CU_ASSERT_EQUAL(msp_get_num_rejected_common_ancestor_events(msp), 0);
        }

        gsl_rng_set(rng, seed);
        ret = msp_reset(msp);
        num_events = 0;
        while ((ret = msp_run(msp, DBL_MAX, 1)) == 1) {
            msp_verify(msp);
            num_events++;
        }
        CU_ASSERT_EQUAL(ret, 0);
        ret = msp_get_num_migration_events(msp, migration_events);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_EQUAL(1 + num_events,
                migration_events[1] + migration_events[2] +
                msp_get_num_recombination_events(msp) +
                msp_get_num_common_ancestor_events(msp) +
                msp_get_num_rejected_common_ancestor_events(msp));
        if (models[j] == MSP_MODEL_HUDSON) {
            CU_ASSERT_EQUAL(msp_get_num_rejected_common_ancestor_events(msp), 0);
        }

        model = msp_get_model(msp)->type;
        CU_ASSERT_EQUAL(model, models[j]);
        model_name = msp_get_model_name(msp);
        CU_ASSERT_STRING_EQUAL(model_name, model_names[j]);

        ret = msp_free(msp);
        CU_ASSERT_EQUAL(ret, 0);
        gsl_rng_free(rng);
        free(msp);
        free(samples);
    }
}

static void
test_simulation_replicates(void)
{
    int ret;
    uint32_t n = 100;
    uint32_t m = 100;
    double mutation_rate = 2;
    size_t num_replicates = 10;
    long seed = 10;
    double migration_matrix[] = {0, 1, 1, 0};
    size_t j;
    sample_t *samples = malloc(n * sizeof(sample_t));
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);
    msp_t msp;
    tree_sequence_t ts;
    mutgen_t mutgen;
    table_collection_t tables;

    CU_ASSERT_FATAL(samples != NULL);
    CU_ASSERT_FATAL(rng != NULL);

    /* Set all the table block sizes to 1 to force reallocs */
    ret = table_collection_alloc(&tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = node_table_alloc(&tables.nodes, 1, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = edge_table_alloc(&tables.edges, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = site_table_alloc(&tables.sites, 1, 1, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = mutation_table_alloc(&tables.mutations, 1, 1, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = migration_table_alloc(&tables.migrations, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = provenance_table_alloc(&tables.provenances, 1, 1, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = individual_table_alloc(&tables.individuals, 1, 1, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = population_table_alloc(&tables.populations, 1, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    gsl_rng_set(rng, seed);
    memset(samples, 0, n * sizeof(sample_t));
    ret = msp_alloc(&msp, n, samples, rng);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_num_populations(&msp, 2);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_migration_matrix(&msp, 4, migration_matrix);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_store_migrations(&msp, true);
    CU_ASSERT_EQUAL(ret, 0);
    /* set all the block sizes to something small to provoke the memory
     * expansions. */
    ret = msp_set_avl_node_block_size(&msp, 3);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_node_mapping_block_size(&msp, 3);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_segment_block_size(&msp, 3);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_edge_block_size(&msp, 3);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_migration_block_size(&msp, 3);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_num_loci(&msp, m);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_recombination_rate(&msp, 0.5);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_initialise(&msp);
    CU_ASSERT_EQUAL(ret, 0);
    ret = mutgen_alloc(&mutgen, mutation_rate, rng, 0, 3);
    CU_ASSERT_EQUAL(ret, 0);

    for (j = 0; j < num_replicates; j++) {
        ret = msp_run(&msp, DBL_MAX, SIZE_MAX);
        CU_ASSERT_EQUAL(ret, 0);
        msp_verify(&msp);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        ret = msp_populate_tables(&msp, NULL, &tables.nodes, &tables.edges,
                &tables.migrations, &tables.populations);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        ret = mutgen_generate_tables_tmp(&mutgen, &tables.nodes, &tables.edges);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        ret = mutgen_populate_tables(&mutgen, &tables.sites, &tables.mutations);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        /* TODO remove this when populate tables takes table_collection as arg */
        tables.sequence_length = m;
        ret = tree_sequence_load_tables(&ts, &tables, 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        verify_simulator_tree_sequence_equality(&msp, &ts, &mutgen, 1.0);
        tree_sequence_print_state(&ts, _devnull);
        ret = msp_reset(&msp);
        CU_ASSERT_EQUAL_FATAL(msp_get_num_edges(&msp), 0);
        CU_ASSERT_EQUAL_FATAL(msp_get_num_migrations(&msp), 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        tree_sequence_free(&ts);
    }
    ret = msp_free(&msp);
    CU_ASSERT_EQUAL(ret, 0);
    ret = mutgen_free(&mutgen);
    CU_ASSERT_EQUAL(ret, 0);
    gsl_rng_free(rng);
    free(samples);
    table_collection_free(&tables);
}

static void
test_bottleneck_simulation(void)
{
    int ret;
    uint32_t n = 100;
    uint32_t m = 100;
    long seed = 10;
    sample_t *samples = malloc(n * sizeof(sample_t));
    msp_t *msp = malloc(sizeof(msp_t));
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);
    double t1 = 0.1;
    double t2 = 0.5;
    int t1_found = 0;

    CU_ASSERT_FATAL(msp != NULL);
    CU_ASSERT_FATAL(samples != NULL);
    CU_ASSERT_FATAL(rng != NULL);

    gsl_rng_set(rng, seed);
    memset(samples, 0, n * sizeof(sample_t));
    ret = msp_alloc(msp, n, samples, rng);
    CU_ASSERT_EQUAL(ret, 0);
    /* set all the block sizes to something small to provoke the memory
     * expansions. */
    ret = msp_set_avl_node_block_size(msp, 2);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_node_mapping_block_size(msp, 2);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_segment_block_size(msp, 2);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_node_block_size(msp, 2);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_edge_block_size(msp, 2);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_num_loci(msp, m);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_recombination_rate(msp, 1.0);
    CU_ASSERT_EQUAL(ret, 0);
    /* Add a bottleneck that does nothing at time t1 */
    ret = msp_add_simple_bottleneck(msp, t1, 0, 0);
    CU_ASSERT_EQUAL(ret, 0);
    /* Add a bottleneck that coalesces everything at t2 */
    ret = msp_add_simple_bottleneck(msp, t2, 0, 1);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_initialise(msp);
    CU_ASSERT_EQUAL(ret, 0);

    ret = msp_run(msp, t1, ULONG_MAX);
    CU_ASSERT_EQUAL(ret, 2);
    CU_ASSERT_FALSE(msp_is_completed(msp));
    msp_print_state(msp, _devnull);

    ret = msp_run(msp, DBL_MAX, ULONG_MAX);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_TRUE(msp_is_completed(msp));
    CU_ASSERT_EQUAL(msp->time, t2);
    msp_verify(msp);

    msp_reset(msp);
    while ((ret = msp_run(msp, DBL_MAX, 1)) == 1) {
        msp_verify(msp);
        if (msp->time == 0.1) {
            t1_found = 1;
        }
    }
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_TRUE(t1_found);
    CU_ASSERT_EQUAL(msp->time, t2);

    ret = msp_free(msp);
    CU_ASSERT_EQUAL(ret, 0);
    gsl_rng_free(rng);
    free(msp);
    free(samples);
}

#define SIMPLE_BOTTLENECK 0
#define INSTANTANEOUS_BOTTLENECK 1

typedef struct {
    int type;
    double time;
    uint32_t population_id;
    double parameter;
} bottleneck_desc_t;

static void
test_large_bottleneck_simulation(void)
{
    int ret;
    uint32_t j;
    uint32_t n = 10000;
    uint32_t m = 100;
    long seed = 10;
    sample_t *samples = malloc(n * sizeof(sample_t));
    msp_t *msp = malloc(sizeof(msp_t));
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);
    uint32_t num_bottlenecks = 10;
    bottleneck_desc_t bottlenecks[num_bottlenecks];
    double t;

    CU_ASSERT_FATAL(msp != NULL);
    CU_ASSERT_FATAL(samples != NULL);
    CU_ASSERT_FATAL(rng != NULL);

    t = 0.1;
    for (j = 0; j < num_bottlenecks; j++) {
        bottlenecks[j].type = SIMPLE_BOTTLENECK;
        bottlenecks[j].time = t;
        bottlenecks[j].parameter = 0.1;
        t += 0.01;
    }
    /* Set the last bottleneck to be full intensity */
    bottlenecks[num_bottlenecks - 1].parameter = 1.0;

    gsl_rng_set(rng, seed);
    memset(samples, 0, n * sizeof(sample_t));
    ret = msp_alloc(msp, n, samples, rng);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_recombination_rate(msp, 1.0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_num_loci(msp, m);
    CU_ASSERT_EQUAL(ret, 0);
    for (j = 0; j < num_bottlenecks; j++) {
        ret = msp_add_simple_bottleneck(msp, bottlenecks[j].time, 0,
                bottlenecks[j].parameter);
        CU_ASSERT_EQUAL(ret, 0);
    }
    ret = msp_initialise(msp);
    CU_ASSERT_EQUAL(ret, 0);

    for (j = 0; j < num_bottlenecks - 1; j++) {
        ret = msp_run(msp, bottlenecks[j].time, ULONG_MAX);
        CU_ASSERT_EQUAL(ret, 2);
        CU_ASSERT_FALSE(msp_is_completed(msp));
        CU_ASSERT_EQUAL(msp->time, bottlenecks[j].time);
        msp_verify(msp);
    }
    ret = msp_run(msp, DBL_MAX, ULONG_MAX);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_TRUE(msp_is_completed(msp));
    CU_ASSERT_EQUAL(msp->time, bottlenecks[num_bottlenecks - 1].time);
    msp_verify(msp);

    /* Test out resets on partially completed simulations. */
    ret = msp_reset(msp);
    CU_ASSERT_EQUAL(ret, 0);
    for (j = 0; j < num_bottlenecks - 1; j++) {
        ret = msp_run(msp, bottlenecks[j].time, ULONG_MAX);
        CU_ASSERT_EQUAL(ret, 2);
    }
    ret = msp_reset(msp);
    msp_verify(msp);

    ret = msp_free(msp);
    CU_ASSERT_EQUAL(ret, 0);
    gsl_rng_free(rng);
    free(msp);
    free(samples);
}

static void
test_multiple_mergers_simulation(void)
{
    CU_ASSERT_DOUBLE_EQUAL(compute_falling_factorial_log(0), 0, 0.000000);
    CU_ASSERT_DOUBLE_EQUAL(compute_falling_factorial_log(1), 1.386294, 0.000001);
    CU_ASSERT_DOUBLE_EQUAL(compute_falling_factorial_log(2), 2.484907, 0.000001);
    CU_ASSERT_DOUBLE_EQUAL(compute_falling_factorial_log(3), 3.178054, 0.000001);
    CU_ASSERT_DOUBLE_EQUAL(compute_falling_factorial_log(4), 3.178054, 0.000001);
    int ret;
    size_t j;
    uint32_t n = 100;
    uint32_t m = 100;
    long seed = 10;
    sample_t *samples = malloc(n * sizeof(sample_t));
    msp_t *msp = malloc(sizeof(msp_t));
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);

    CU_ASSERT_FATAL(msp != NULL);
    CU_ASSERT_FATAL(samples != NULL);
    CU_ASSERT_FATAL(rng != NULL);

    for (j = 0; j < 2; j++) {
        gsl_rng_set(rng, seed);
        /* TODO check non-zero sample times here to make sure they fail. */
        memset(samples, 0, n * sizeof(sample_t));
        ret = msp_alloc(msp, n, samples, rng);
        CU_ASSERT_EQUAL(ret, 0);
        /* TODO what are good parameters here?? */
        if (j == 0) {
            // Use psi = 0.5 for now, but should definitely test for 0 and 1 cases
            ret = msp_set_simulation_model_dirac(msp, 1, 0.5, 1);
        } else {
            ret = msp_set_simulation_model_beta(msp, 1, 1.0, 10.0);
        }
        CU_ASSERT_EQUAL(ret, 0);
        ret = msp_set_num_loci(msp, m);
        CU_ASSERT_EQUAL(ret, 0);
        ret = msp_set_recombination_rate(msp, 10.0);
        CU_ASSERT_EQUAL(ret, 0);
        /* TODO check for adding various complications like multiple populations etc
         * to ensure they fail.
         */
        ret = msp_initialise(msp);
        CU_ASSERT_EQUAL(ret, 0);
        msp_print_state(msp, _devnull);

        ret = msp_run(msp, DBL_MAX, ULONG_MAX);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_TRUE(msp_is_completed(msp));
        CU_ASSERT_TRUE(msp->time > 0);
        msp_verify(msp);

        msp_reset(msp);
        while ((ret = msp_run(msp, DBL_MAX, 1)) == 1) {
            msp_verify(msp);
        }
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_TRUE(msp_is_completed(msp));

        ret = msp_free(msp);
        CU_ASSERT_EQUAL(ret, 0);
    }
    gsl_rng_free(rng);
    free(msp);
    free(samples);
}


static void
test_simple_recomb_map(void)
{
    int ret;
    recomb_map_t recomb_map;
    double positions[] = {0.0, 1.0};
    double rates[] = {0.0, 1.0};
    uint32_t num_loci[] = {1, 100, UINT32_MAX};
    size_t j;

    for (j = 0; j < sizeof(num_loci) / sizeof(uint32_t); j++) {
        ret = recomb_map_alloc(&recomb_map, num_loci[j], 1.0,
                positions, rates, 2);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        recomb_map_print_state(&recomb_map, _devnull);
        CU_ASSERT_EQUAL(recomb_map_get_num_loci(&recomb_map), num_loci[j]);
        CU_ASSERT_EQUAL(recomb_map_get_size(&recomb_map), 2);
        CU_ASSERT_EQUAL(recomb_map_get_sequence_length(&recomb_map), 1.0);
        CU_ASSERT_EQUAL(
                recomb_map_genetic_to_phys(&recomb_map, num_loci[j]), 1.0);
        CU_ASSERT_EQUAL(
                recomb_map_phys_to_genetic(&recomb_map, 1.0), num_loci[j]);
        ret = recomb_map_free(&recomb_map);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
    }
}

static void
test_recomb_map_errors(void)
{
    int ret;
    recomb_map_t recomb_map;
    double positions[] = {0.0, 1.0, 2.0};
    double rates[] = {1.0, 2.0, 0.0};
    double values[] = {0.0, 1.0};

    ret = recomb_map_alloc(&recomb_map, 10, 1.0, positions, rates, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_RECOMBINATION_MAP);
    recomb_map_free(&recomb_map);

    ret = recomb_map_alloc(&recomb_map, 10, 1.0, positions, rates, 1);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_RECOMBINATION_MAP);
    recomb_map_free(&recomb_map);

    ret = recomb_map_alloc(&recomb_map, 0, 1.0, positions, rates, 2);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_RECOMBINATION_MAP);
    recomb_map_free(&recomb_map);

    ret = recomb_map_alloc(&recomb_map, 10, 2.0, positions, rates, 2);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_RECOMBINATION_MAP);
    recomb_map_free(&recomb_map);

    positions[0] = 1;
    ret = recomb_map_alloc(&recomb_map, 10, 1.0, positions, rates, 2);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_RECOMBINATION_MAP);
    recomb_map_free(&recomb_map);
    positions[0] = 0;

    positions[1] = 3.0;
    ret = recomb_map_alloc(&recomb_map, 10, 2.0, positions, rates, 3);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_RECOMBINATION_MAP);
    recomb_map_free(&recomb_map);
    positions[1] = 1.0;

    ret = recomb_map_alloc(&recomb_map, 10, 2.0, positions, rates, 3);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    ret = recomb_map_genetic_to_phys_bulk(&recomb_map, values, 2);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* values must be increasing */
    values[0] = 2.0;
    ret = recomb_map_genetic_to_phys_bulk(&recomb_map, values, 2);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_GENERIC);

    /* values must be <= num_loci */
    values[0] = 1000;
    ret = recomb_map_genetic_to_phys_bulk(&recomb_map, values, 2);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_GENERIC);

    recomb_map_free(&recomb_map);
}

static void
verify_recomb_map(uint32_t num_loci, double length, double *positions,
        double *rates, size_t size)
{

    int ret;
    recomb_map_t recomb_map;
    double total_rate, x, y, z;
    size_t j;
    size_t num_checks = 1000;
    double eps = 1e-6;
    double *ret_rates, *ret_positions, *bulk_x;

    ret_rates = malloc(size * sizeof(double));
    ret_positions = malloc(size * sizeof(double));
    bulk_x = malloc(num_checks * sizeof(double));

    CU_ASSERT_FATAL(ret_rates != NULL);
    CU_ASSERT_FATAL(ret_positions != NULL);

    ret = recomb_map_alloc(&recomb_map, num_loci, length,
           positions, rates, size);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    recomb_map_print_state(&recomb_map, _devnull);

    CU_ASSERT_EQUAL(recomb_map_get_num_loci(&recomb_map), num_loci);
    CU_ASSERT_EQUAL(recomb_map_get_size(&recomb_map), size);
    CU_ASSERT_EQUAL(recomb_map_genetic_to_phys(&recomb_map, 0), 0);
    CU_ASSERT_EQUAL(
            recomb_map_genetic_to_phys(&recomb_map, num_loci), length);
    total_rate = recomb_map_get_total_recombination_rate(&recomb_map);
    CU_ASSERT_DOUBLE_EQUAL(
            total_rate / (num_loci - 1),
            recomb_map_get_per_locus_recombination_rate(&recomb_map), eps)

    for (j = 0; j < num_checks; j++) {
        x = j * (num_loci / num_checks);
        bulk_x[j] = x;
        y = recomb_map_genetic_to_phys(&recomb_map, x);
        CU_ASSERT_TRUE(0 <= y && y <= length);
        z = recomb_map_phys_to_genetic(&recomb_map, y);
        CU_ASSERT_DOUBLE_EQUAL(x, z, eps);
    }
    ret = recomb_map_genetic_to_phys_bulk(&recomb_map, bulk_x, num_checks);
    CU_ASSERT_EQUAL(ret, 0);
    for (j = 0; j < num_checks; j++) {
        x = j * (num_loci / num_checks);
        y = recomb_map_genetic_to_phys(&recomb_map, x);
        CU_ASSERT_DOUBLE_EQUAL(bulk_x[j], y, 1e-12);
    }
    ret = recomb_map_get_positions(&recomb_map, ret_positions);
    CU_ASSERT_EQUAL(ret, 0);
    ret = recomb_map_get_rates(&recomb_map, ret_rates);
    CU_ASSERT_EQUAL(ret, 0);
    for (j = 0; j < size; j++) {
        CU_ASSERT_EQUAL(ret_rates[j], rates[j]);
        CU_ASSERT_EQUAL(ret_positions[j], positions[j]);
    }
    ret = recomb_map_free(&recomb_map);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    free(ret_rates);
    free(ret_positions);
    free(bulk_x);
}

static void
test_recomb_map_examples(void)
{
    double p1[] =  {0, 0.05019838393314813, 0.36933662489552865, 1};
    double r1[] = {3.5510784169955434, 4.184964179610539, 3.800808140657212, 0};
    double p2[] =  {0, 0.125, 0.875, 1, 4, 8, 16};
    double r2[] = {0.1, 6.0, 3.333, 2.1, 0.0, 2.2, 0};

    verify_recomb_map(2, 1.0, p1, r1, 4);
    verify_recomb_map(1000, 1.0, p1, r1, 4);
    verify_recomb_map(UINT32_MAX, 1.0, p1, r1, 4);

    verify_recomb_map(2, 16.0, p2, r2, 7);
    verify_recomb_map(100, 16.0, p2, r2, 7);
    verify_recomb_map(UINT32_MAX, 16.0, p2, r2, 7);
}




static int
msprime_suite_init(void)
{
    int fd;
    static char template[] = "/tmp/msp_sim_c_test_XXXXXX";

    _tmp_file_name = NULL;
    _devnull = NULL;

    _tmp_file_name = malloc(sizeof(template));
    if (_tmp_file_name == NULL) {
        return CUE_NOMEMORY;
    }
    strcpy(_tmp_file_name, template);
    fd = mkstemp(_tmp_file_name);
    if (fd == -1) {
        return CUE_SINIT_FAILED;
    }
    close(fd);
    _devnull = fopen("/dev/null", "w");
    if (_devnull == NULL) {
        return CUE_SINIT_FAILED;
    }
    return CUE_SUCCESS;
}

static int
msprime_suite_cleanup(void)
{
    if (_tmp_file_name != NULL) {
        unlink(_tmp_file_name);
        free(_tmp_file_name);
    }
    if (_devnull != NULL) {
        fclose(_devnull);
    }
    return CUE_SUCCESS;
}

static void
handle_cunit_error()
{
    fprintf(stderr, "CUnit error occured: %d: %s\n",
            CU_get_error(), CU_get_error_msg());
    exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
    int ret;
    CU_pTest test;
    CU_pSuite suite;
    CU_TestInfo tests[] = {
        {"test_fenwick", test_fenwick},
        {"test_fenwick_expand", test_fenwick_expand},
        {"test_single_locus_two_populations", test_single_locus_two_populations},
        {"test_single_locus_many_populations", test_single_locus_many_populations},
        {"test_single_locus_historical_sample", test_single_locus_historical_sample},
        {"test_simulator_getters_setters", test_simulator_getters_setters},
        {"test_model_errors", test_simulator_model_errors},
        {"test_demographic_events", test_demographic_events},
        {"test_single_locus_simulation", test_single_locus_simulation},
        {"test_mixed_model_simulation", test_mixed_model_simulation},
        {"test_dtwf_deterministic", test_dtwf_deterministic},
        {"test_dtwf_single_locus_simulation", test_dtwf_single_locus_simulation},
        {"test_simulation_memory_limit", test_simulation_memory_limit},
        {"test_multi_locus_simulation", test_multi_locus_simulation},
        {"test_dtwf_multi_locus_simulation", test_dtwf_multi_locus_simulation},
        {"test_simulation_replicates", test_simulation_replicates},
        {"test_bottleneck_simulation", test_bottleneck_simulation},
        {"test_multiple_mergers_simulation", test_multiple_mergers_simulation},
        {"test_large_bottleneck_simulation", test_large_bottleneck_simulation},
        {"test_simple_recombination_map", test_simple_recomb_map},
        {"test_recombination_map_errors", test_recomb_map_errors},
        {"test_recombination_map_examples", test_recomb_map_examples},
        CU_TEST_INFO_NULL,
    };

    /* We use initialisers here as the struct definitions change between
     * versions of CUnit */
    CU_SuiteInfo suites[] = {
        {
            .pName = "msprime",
            .pInitFunc = msprime_suite_init,
            .pCleanupFunc = msprime_suite_cleanup,
            .pTests = tests
        },
        CU_SUITE_INFO_NULL,
    };
    if (CUE_SUCCESS != CU_initialize_registry()) {
        handle_cunit_error();
    }
    if (CUE_SUCCESS != CU_register_suites(suites)) {
        handle_cunit_error();
    }
    CU_basic_set_mode(CU_BRM_VERBOSE);

    if (argc == 1) {
        CU_basic_run_tests();
    } else if (argc == 2) {
        suite = CU_get_suite_by_name("msprime", CU_get_registry());
        if (suite == NULL) {
            printf("Suite not found\n");
            return EXIT_FAILURE;
        }
        test = CU_get_test_by_name(argv[1], suite);
        if (test == NULL) {
            printf("Test '%s' not found\n", argv[1]);
            return EXIT_FAILURE;
        }
        CU_basic_run_test(suite, test);
    } else {
        printf("usage: ./simulation_tests <test_name>\n");
        return EXIT_FAILURE;
    }

    ret = EXIT_SUCCESS;
    if (CU_get_number_of_tests_failed() != 0) {
        printf("Test failed!\n");
        ret = EXIT_FAILURE;
    }
    CU_cleanup_registry();
    return ret;
}
