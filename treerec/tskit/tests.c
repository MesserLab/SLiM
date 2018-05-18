/*
** Copyright (C) 2016-2018 University of Oxford
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

#define SIMPLE_BOTTLENECK 0
#define INSTANTANEOUS_BOTTLENECK 1

/* Defining this here for now to avoid breaking code, but this should be
 * removed when we udpate the mutation generator inferface */
#define MSP_ALPHABET_BINARY 0
#define MSP_ALPHABET_ASCII 1

typedef struct {
    int type;
    double time;
    uint32_t population_id;
    double parameter;
} bottleneck_desc_t;

/* Example tree sequences used in some of the tests. */


/* Simple single tree example. */
const char *single_tree_ex_nodes =/*          6          */
    "1  0   0   -1\n"             /*         / \         */
    "1  0   0   -1\n"             /*        /   \        */
    "1  0   0   -1\n"             /*       /     \       */
    "1  0   0   -1\n"             /*      /       5      */
    "0  1   0   -1\n"             /*     4       / \     */
    "0  2   0   -1\n"             /*    / \     /   \    */
    "0  3   0   -1\n";            /*   0   1   2     3   */
const char *single_tree_ex_edges =
    "0  1   4   0,1\n"
    "0  1   5   2,3\n"
    "0  1   6   4,5\n";
const char *single_tree_ex_sites =
    "0.1  0\n"
    "0.2  0\n"
    "0.3  0\n";
const char *single_tree_ex_mutations =
    "0    2     1   -1\n"
    "1    4     1   -1\n"
    "1    0     0   1\n"  /* Back mutation over 0 */
    "2    0     1   -1\n"  /* recurrent mutations over samples */
    "2    1     1   -1\n"
    "2    2     1   -1\n"
    "2    3     1   -1\n";

/* Example from the PLOS paper */
const char *paper_ex_nodes =
    "1  0       0   0\n"
    "1  0       0   0\n"
    "1  0       0   1\n"
    "1  0       0   1\n"
    "0  0.071   0   -1\n"
    "0  0.090   0   -1\n"
    "0  0.170   0   -1\n"
    "0  0.202   0   -1\n"
    "0  0.253   0   -1\n";
const char *paper_ex_edges =
    "2 10 4 2\n"
    "2 10 4 3\n"
    "0 10 5 1\n"
    "0 2  5 3\n"
    "2 10 5 4\n"
    "0 7  6 0,5\n"
    "7 10 7 0,5\n"
    "0 2  8 2,6\n";
/* We make one mutation for each tree */
const char *paper_ex_sites =
    "1      0\n"
    "4.5    0\n"
    "8.5    0\n";
const char *paper_ex_mutations =
    "0      2   1\n"
    "1      0   1\n"
    "2      5   1\n";
/* Two (diploid) indivduals */
const char *paper_ex_individuals =
    "0      0.2,1.5\n"
    "0      0.0,0.0\n";

/* An example of a nonbinary tree sequence */
const char *nonbinary_ex_nodes =
    "1  0       0   -1\n"
    "1  0       0   -1\n"
    "1  0       0   -1\n"
    "1  0       0   -1\n"
    "1  0       0   -1\n"
    "1  0       0   -1\n"
    "1  0       0   -1\n"
    "1  0       0   -1\n"
    "0  0.01    0   -1\n"
    "0  0.068   0   -1\n"
    "0  0.130   0   -1\n"
    "0  0.279   0   -1\n"
    "0  0.405   0   -1\n";
const char *nonbinary_ex_edges =
    "0	100	8	0,1,2,3\n"
    "0	100	9	6,8\n"
    "0  100 10  4\n"
    "0  17  10  5\n"
    "0  100 10  7\n"
    "17	100	11	5,9\n"
    "0	17	12	9\n"
    "0  100 12  10\n"
    "17	100	12	11";
const char *nonbinary_ex_sites =
        "1  0\n"
        "18 0\n";
const char *nonbinary_ex_mutations =
    "0    2   1\n"
    "1    11  1";

/* An example of a tree sequence with unary nodes. */

const char *unary_ex_nodes =
    "1  0       0  -1\n"
    "1  0       0  -1\n"
    "1  0       0  -1\n"
    "1  0       0  -1\n"
    "0  0.071   0  -1\n"
    "0  0.090   0  -1\n"
    "0  0.170   0  -1\n"
    "0  0.202   0  -1\n"
    "0  0.253   0  -1\n";
const char *unary_ex_edges =
    "2 10 4 2,3\n"
    "0 10 5 1\n"
    "0 2  5 3\n"
    "2 10 5 4\n"
    "0 7  6 0,5\n"
    "7 10 7 0\n"
    "0 2  7 2\n"
    "7 10 7 5\n"
    "0 7  8 6\n"
    "0 2  8 7\n";

/* We make one mutation for each tree, over unary nodes if this exist */
const char *unary_ex_sites =
    "1.0    0\n"
    "4.5    0\n"
    "8.5    0\n";
const char *unary_ex_mutations =
    "0    2   1\n"
    "1    6   1\n"
    "2    5   1\n";

/* An example of a tree sequence with internally sampled nodes. */

/* TODO: find a way to draw these side-by-side */
/*
  7
+-+-+
|   5
| +-++
| |  4
| | +++
| | | 3
| | |
| 1 2
|
0

  8
+-+-+
|   5
| +-++
| |  4
| | +++
3 | | |
  | | |
  1 2 |
      |
      0

  6
+-+-+
|   5
| +-++
| |  4
| | +++
| | | 3
| | |
| 1 2
|
0
*/

const char *internal_sample_ex_nodes =
    "1  0.0   0   -1\n"
    "1  0.1   0   -1\n"
    "1  0.1   0   -1\n"
    "1  0.2   0   -1\n"
    "0  0.4   0   -1\n"
    "1  0.5   0   -1\n"
    "0  0.7   0   -1\n"
    "0  1.0   0   -1\n"
    "0  1.2   0   -1\n";
const char *internal_sample_ex_edges =
    "2 8  4 0\n"
    "0 10 4 2\n"
    "0 2  4 3\n"
    "8 10 4 3\n"
    "0 10 5 1,4\n"
    "8 10 6 0,5\n"
    "0 2  7 0,5\n"
    "2 8  8 3,5\n";
/* We make one mutation for each tree, some above the internal node */
const char *internal_sample_ex_sites =
    "1.0    0\n"
    "4.5    0\n"
    "8.5    0\n";
const char *internal_sample_ex_mutations =
    "0    2   1\n"
    "1    5   1\n"
    "2    5   1\n";


/* Simple utilities to parse text so we can write declaritive
 * tests. This is not intended as a robust general input mechanism.
 */

static void
parse_nodes(const char *text, node_table_t *node_table)
{
    int ret;
    size_t c, k;
    size_t MAX_LINE = 1024;
    char line[MAX_LINE];
    const char *whitespace = " \t";
    char *p;
    double time;
    int flags, population, individual;
    char *name;

    c = 0;
    while (text[c] != '\0') {
        /* Fill in the line */
        k = 0;
        while (text[c] != '\n' && text[c] != '\0') {
            CU_ASSERT_FATAL(k < MAX_LINE - 1);
            line[k] = text[c];
            c++;
            k++;
        }
        if (text[c] == '\n') {
            c++;
        }
        line[k] = '\0';
        p = strtok(line, whitespace);
        CU_ASSERT_FATAL(p != NULL);
        flags = atoi(p);
        p = strtok(NULL, whitespace);
        CU_ASSERT_FATAL(p != NULL);
        time = atof(p);
        p = strtok(NULL, whitespace);
        CU_ASSERT_FATAL(p != NULL);
        population = atoi(p);
        p = strtok(NULL, whitespace);
        if (p == NULL) {
            individual = -1;
        } else {
            individual = atoi(p);
            p = strtok(NULL, whitespace);
        }
        if (p == NULL) {
            name = "";
        } else {
            name = p;
        }
        ret = node_table_add_row(node_table, flags, time, population,
                individual, name, strlen(name));
        CU_ASSERT_FATAL(ret >= 0);
    }
}

static void
parse_edges(const char *text, edge_table_t *edge_table)
{
    int ret;
    size_t c, k;
    size_t MAX_LINE = 1024;
    char line[MAX_LINE], sub_line[MAX_LINE];
    const char *whitespace = " \t";
    char *p, *q;
    double left, right;
    node_id_t parent, child;
    uint32_t num_children;

    c = 0;
    while (text[c] != '\0') {
        /* Fill in the line */
        k = 0;
        while (text[c] != '\n' && text[c] != '\0') {
            CU_ASSERT_FATAL(k < MAX_LINE - 1);
            line[k] = text[c];
            c++;
            k++;
        }
        if (text[c] == '\n') {
            c++;
        }
        line[k] = '\0';
        p = strtok(line, whitespace);
        CU_ASSERT_FATAL(p != NULL);
        left = atof(p);
        p = strtok(NULL, whitespace);
        CU_ASSERT_FATAL(p != NULL);
        right = atof(p);
        p = strtok(NULL, whitespace);
        CU_ASSERT_FATAL(p != NULL);
        parent = atoi(p);
        num_children = 0;
        p = strtok(NULL, whitespace);
        CU_ASSERT_FATAL(p != NULL);

        num_children = 1;
        q = p;
        while (*q != '\0') {
            if (*q == ',') {
                num_children++;
            }
            q++;
        }
        CU_ASSERT_FATAL(num_children >= 1);
        strncpy(sub_line, p, MAX_LINE);
        q = strtok(sub_line, ",");
        for (k = 0; k < num_children; k++) {
            CU_ASSERT_FATAL(q != NULL);
            child = atoi(q);
            ret = edge_table_add_row(edge_table, left, right, parent, child);
            CU_ASSERT_FATAL(ret >= 0);
            q = strtok(NULL, ",");
        }
        CU_ASSERT_FATAL(q == NULL);
    }
}

static void
parse_sites(const char *text, site_table_t *site_table)
{
    int ret;
    size_t c, k;
    size_t MAX_LINE = 1024;
    char line[MAX_LINE];
    double position;
    char ancestral_state[MAX_LINE];
    const char *whitespace = " \t";
    char *p;

    c = 0;
    while (text[c] != '\0') {
        /* Fill in the line */
        k = 0;
        while (text[c] != '\n' && text[c] != '\0') {
            CU_ASSERT_FATAL(k < MAX_LINE - 1);
            line[k] = text[c];
            c++;
            k++;
        }
        if (text[c] == '\n') {
            c++;
        }
        line[k] = '\0';
        p = strtok(line, whitespace);
        CU_ASSERT_FATAL(p != NULL);
        position = atof(p);
        p = strtok(NULL, whitespace);
        CU_ASSERT_FATAL(p != NULL);
        strncpy(ancestral_state, p, MAX_LINE);
        ret = site_table_add_row(site_table, position, ancestral_state,
                strlen(ancestral_state), NULL, 0);
        CU_ASSERT_FATAL(ret >= 0);
    }
}

static void
parse_mutations(const char *text, mutation_table_t *mutation_table)
{
    int ret;
    size_t c, k;
    size_t MAX_LINE = 1024;
    char line[MAX_LINE];
    const char *whitespace = " \t";
    char *p;
    node_id_t node;
    site_id_t site;
    mutation_id_t parent;
    char derived_state[MAX_LINE];

    c = 0;
    while (text[c] != '\0') {
        /* Fill in the line */
        k = 0;
        while (text[c] != '\n' && text[c] != '\0') {
            CU_ASSERT_FATAL(k < MAX_LINE - 1);
            line[k] = text[c];
            c++;
            k++;
        }
        if (text[c] == '\n') {
            c++;
        }
        line[k] = '\0';
        p = strtok(line, whitespace);
        site = atoi(p);
        CU_ASSERT_FATAL(p != NULL);
        p = strtok(NULL, whitespace);
        CU_ASSERT_FATAL(p != NULL);
        node = atoi(p);
        p = strtok(NULL, whitespace);
        CU_ASSERT_FATAL(p != NULL);
        strncpy(derived_state, p, MAX_LINE);
        parent = MSP_NULL_MUTATION;
        p = strtok(NULL, whitespace);
        if (p != NULL) {
            parent = atoi(p);
        }
        ret = mutation_table_add_row(mutation_table, site, node, parent,
                derived_state, strlen(derived_state), NULL, 0);
        CU_ASSERT_FATAL(ret >= 0);
    }
}

static void
parse_individuals(const char *text, individual_table_t *individual_table)
{
    int ret;
    size_t c, k;
    size_t MAX_LINE = 1024;
    char line[MAX_LINE];
    char sub_line[MAX_LINE];
    const char *whitespace = " \t";
    char *p, *q;
    double location[MAX_LINE];
    int location_len;
    int flags;
    char *name;

    c = 0;
    while (text[c] != '\0') {
        /* Fill in the line */
        k = 0;
        while (text[c] != '\n' && text[c] != '\0') {
            CU_ASSERT_FATAL(k < MAX_LINE - 1);
            line[k] = text[c];
            c++;
            k++;
        }
        if (text[c] == '\n') {
            c++;
        }
        line[k] = '\0';
        p = strtok(line, whitespace);
        CU_ASSERT_FATAL(p != NULL);
        flags = atoi(p);
        // the locations are comma-separated
        location_len = 1;
        q = p;
        while (*q != '\0') {
            if (*q == ',') {
                location_len++;
            }
            q++;
        }
        CU_ASSERT_FATAL(location_len >= 1);
        strncpy(sub_line, p, MAX_LINE);
        q = strtok(sub_line, ",");
        for (k = 0; k < location_len; k++) {
            CU_ASSERT_FATAL(q != NULL);
            location[k] = atoi(q);
            q = strtok(NULL, ",");
        }
        CU_ASSERT_FATAL(q == NULL);
        p = strtok(NULL, whitespace);
        if (p == NULL) {
            name = "";
        } else {
            name = p;
        }
        ret = individual_table_add_row(individual_table, flags, location, location_len,
                name, strlen(name));
        CU_ASSERT_FATAL(ret >= 0);
    }
}

static void
tree_sequence_from_text(tree_sequence_t *ts, double sequence_length,
        const char *nodes, const char *edges,
        const char *migrations, const char *sites, const char *mutations,
        const char *individuals, const char *provenance)
{
    int ret;
    table_collection_t tables;

    CU_ASSERT_FATAL(ts != NULL);
    CU_ASSERT_FATAL(nodes != NULL);
    CU_ASSERT_FATAL(edges != NULL);
    /* Not supporting provenance here for now */
    CU_ASSERT_FATAL(provenance == NULL);

    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    tables.sequence_length = sequence_length;
    parse_nodes(nodes, &tables.nodes);
    parse_edges(edges, &tables.edges);
    if (sites != NULL) {
        parse_sites(sites, &tables.sites);
    }
    if (mutations != NULL) {
        parse_mutations(mutations, &tables.mutations);
    }
    if (individuals != NULL) {
        parse_individuals(individuals, &tables.individuals);
    }
    ret = tree_sequence_load_tables(ts, &tables, 0);
    /* tree_sequence_print_state(ts, stdout); */
    /* printf("ret = %s\n", msp_strerror(ret)); */
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    table_collection_free(&tables);
}

static int
get_max_site_mutations(tree_sequence_t *ts)
{
    int ret;
    int max_mutations = 0;
    size_t j;
    site_t site;

    for (j = 0; j < tree_sequence_get_num_sites(ts); j++) {
        ret = tree_sequence_get_site(ts, j, &site);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        max_mutations = GSL_MAX(max_mutations, site.mutations_length);
    }
    return max_mutations;
}

static bool
multi_mutations_exist(tree_sequence_t *ts, size_t start, size_t end)
{
    int ret;
    size_t j;
    site_t site;

    for (j = 0; j < GSL_MIN(tree_sequence_get_num_sites(ts), end); j++) {
        ret = tree_sequence_get_site(ts, j, &site);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        if (site.mutations_length > 1) {
            return true;
        }
    }
    return false;
}

static void
unsort_edges(edge_table_t *edges, size_t start)
{
    size_t j, k;
    size_t n = edges->num_rows - start;
    edge_t *buff = malloc(n * sizeof(edge_t));
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);

    CU_ASSERT_FATAL(edges != NULL);
    CU_ASSERT_FATAL(rng != NULL);
    gsl_rng_set(rng, 1);

    for (j = 0; j < n; j++) {
        k = start + j;
        buff[j].left = edges->left[k];
        buff[j].right = edges->right[k];
        buff[j].parent = edges->parent[k];
        buff[j].child = edges->child[k];
    }
    gsl_ran_shuffle(rng, buff, n, sizeof(edge_t));
    for (j = 0; j < n; j++) {
        k = start + j;
        edges->left[k] = buff[j].left;
        edges->right[k] = buff[j].right;
        edges->parent[k] = buff[j].parent;
        edges->child[k] = buff[j].child;
    }
    free(buff);
    gsl_rng_free(rng);
}

static void
unsort_sites(site_table_t *sites, mutation_table_t *mutations)
{
    double position;
    char *ancestral_state = NULL;
    size_t j, k, length;

    if (sites->num_rows > 1) {
        /* Swap the first two sites */
        CU_ASSERT_EQUAL_FATAL(sites->ancestral_state_offset[0], 0);

        position = sites->position[0];
        length = sites->ancestral_state_offset[1];
        /* Save a copy of the first ancestral state */
        ancestral_state = malloc(length);
        CU_ASSERT_FATAL(ancestral_state != NULL);
        memcpy(ancestral_state, sites->ancestral_state, length);
        /* Now write the ancestral state for the site 1 here */
        k = 0;
        for (j = sites->ancestral_state_offset[1]; j < sites->ancestral_state_offset[2];
                j++) {
            sites->ancestral_state[k] = sites->ancestral_state[j];
            k++;
        }
        sites->ancestral_state_offset[1] = k;
        memcpy(sites->ancestral_state + k, ancestral_state, length);
        sites->position[0] = sites->position[1];
        sites->position[1] = position;

        /* Update the mutations for these sites */
        j = 0;
        while (j < mutations->num_rows && mutations->site[j] == 0) {
            mutations->site[j] = 1;
            j++;
        }
        while (j < mutations->num_rows && mutations->site[j] == 1) {
            mutations->site[j] = 0;

            j++;
        }
    }
    msp_safe_free(ancestral_state);
}

static void
verify_nodes_equal(node_t *n1, node_t *n2)
{
    double eps = 1e-6;

    CU_ASSERT_DOUBLE_EQUAL_FATAL(n1->time, n1->time, eps);
    CU_ASSERT_EQUAL_FATAL(n1->population, n2->population);
    CU_ASSERT_EQUAL_FATAL(n1->flags, n2->flags);
    CU_ASSERT_FATAL(n1->metadata_length == n2->metadata_length);
    CU_ASSERT_NSTRING_EQUAL_FATAL(n1->metadata, n2->metadata, n1->metadata_length);
}

static void
verify_edges_equal(edge_t *r1, edge_t *r2, double scale)
{
    double eps = 1e-6;

    CU_ASSERT_DOUBLE_EQUAL_FATAL(r1->left * scale, r2->left, eps);
    CU_ASSERT_DOUBLE_EQUAL_FATAL(r1->right * scale, r2->right, eps);
    CU_ASSERT_EQUAL_FATAL(r1->parent, r2->parent);
    CU_ASSERT_EQUAL_FATAL(r1->child, r2->child);
}

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
verify_site_tables_equal(site_table_t *s1, site_table_t *s2)
{
    double eps = 1e-6;
    table_size_t j, k;
    table_size_t as1_length, as2_length;
    table_size_t md1_length, md2_length;

    CU_ASSERT_EQUAL_FATAL(s1->num_rows, s2->num_rows);
    CU_ASSERT_EQUAL_FATAL(s1->ancestral_state_length, s2->ancestral_state_length);
    CU_ASSERT_EQUAL_FATAL(s1->metadata_length, s2->metadata_length);
    for (j = 0; j < s1->num_rows; j++) {
        CU_ASSERT_DOUBLE_EQUAL_FATAL(s1->position[j], s2->position[j], eps);
        CU_ASSERT_EQUAL_FATAL(s1->ancestral_state_offset[j], s2->ancestral_state_offset[j]);
        as1_length = s1->ancestral_state_offset[j+1] - s1->ancestral_state_offset[j];
        as2_length = s2->ancestral_state_offset[j+1] - s2->ancestral_state_offset[j];
        CU_ASSERT_EQUAL_FATAL(as1_length, as2_length);
        for (k = 0; k < as1_length; k++) {
            CU_ASSERT_EQUAL_FATAL(s1->ancestral_state[k], s2->ancestral_state[k]);
        }
        CU_ASSERT_EQUAL_FATAL(s1->metadata_offset[j], s2->metadata_offset[j]);
        md1_length = s1->metadata_offset[j+1] - s1->metadata_offset[j];
        md2_length = s2->metadata_offset[j+1] - s2->metadata_offset[j];
        CU_ASSERT_EQUAL_FATAL(md1_length, md2_length);
        for (k = 0; k < md1_length; k++) {
            CU_ASSERT_EQUAL_FATAL(s1->metadata[k], s2->metadata[k]);
        }
    }
}

static void
verify_mutation_tables_equal(mutation_table_t *m1, mutation_table_t *m2)
{
    // NOTE: does not check parent column
    table_size_t j, k;
    table_size_t m1_length, m2_length;

    CU_ASSERT_EQUAL_FATAL(m1->num_rows, m2->num_rows);
    CU_ASSERT_EQUAL_FATAL(m1->derived_state_length, m2->derived_state_length);
    CU_ASSERT_EQUAL_FATAL(m1->metadata_length, m2->metadata_length);

    for (j = 0; j < m1->num_rows; j++) {
        CU_ASSERT_EQUAL_FATAL(m1->site[j], m2->site[j]);
        m1_length = m1->derived_state_offset[j+1] - m1->derived_state_offset[j];
        m2_length = m2->derived_state_offset[j+1] - m2->derived_state_offset[j];
        CU_ASSERT_EQUAL_FATAL(m1_length, m2_length);
        for (k = 0; k < m1_length; k++) {
            CU_ASSERT_EQUAL_FATAL(m1->derived_state[k], m2->derived_state[k]);
        }
        m1_length = m1->metadata_offset[j+1] - m1->metadata_offset[j];
        m2_length = m2->metadata_offset[j+1] - m2->metadata_offset[j];
        CU_ASSERT_EQUAL_FATAL(m1_length, m2_length);
        for (k = 0; k < m1_length; k++) {
            CU_ASSERT_EQUAL_FATAL(m1->metadata[k], m2->metadata[k]);
        }
    }
}

static void
verify_provenances_equal(provenance_t *p1, provenance_t *p2)
{
    CU_ASSERT_FATAL(p1->timestamp_length == p2->timestamp_length);
    CU_ASSERT_NSTRING_EQUAL_FATAL(p1->timestamp, p2->timestamp, p1->timestamp_length);
    CU_ASSERT_FATAL(p1->record_length == p2->record_length);
    CU_ASSERT_NSTRING_EQUAL_FATAL(p1->record, p2->record, p1->record_length);
}

static void
verify_individuals_equal(individual_t *i1, individual_t *i2)
{
    table_size_t j;

    CU_ASSERT_FATAL(i1->id == i2->id);
    CU_ASSERT_FATAL(i1->flags == i2->flags);
    CU_ASSERT_FATAL(i1->metadata_length == i2->metadata_length);
    CU_ASSERT_NSTRING_EQUAL_FATAL(i1->metadata, i2->metadata, i1->metadata_length);
    CU_ASSERT_FATAL(i1->location_length == i2->location_length);
    for (j = 0; j < i1->location_length; j++) {
        CU_ASSERT_EQUAL_FATAL(i1->location[j], i2->location[j]);
    }
}

static void
verify_populations_equal(tmp_population_t *p1, tmp_population_t *p2)
{
    CU_ASSERT_FATAL(p1->id == p2->id);
    CU_ASSERT_FATAL(p1->metadata_length == p2->metadata_length);
    CU_ASSERT_NSTRING_EQUAL_FATAL(p1->metadata, p2->metadata, p1->metadata_length);
}

static sparse_tree_t *
get_tree_list(tree_sequence_t *ts)
{
    int ret;
    sparse_tree_t t, *trees;
    size_t num_trees;

    num_trees = tree_sequence_get_num_trees(ts);
    ret = sparse_tree_alloc(&t, ts, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    trees = malloc(num_trees * sizeof(sparse_tree_t));
    CU_ASSERT_FATAL(trees != NULL);
    for (ret = sparse_tree_first(&t); ret == 1; ret = sparse_tree_next(&t)) {
        CU_ASSERT_FATAL(t.index < num_trees);
        ret = sparse_tree_alloc(&trees[t.index], ts, 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        ret = sparse_tree_copy(&trees[t.index], &t);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        ret = sparse_tree_equal(&trees[t.index], &t);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        /* Make sure the left and right coordinates are also OK */
        CU_ASSERT_DOUBLE_EQUAL(trees[t.index].left, t.left, 1e-6);
        CU_ASSERT_DOUBLE_EQUAL(trees[t.index].right, t.right, 1e-6);
    }
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = sparse_tree_free(&t);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    return trees;
}

static void
verify_tree_next_prev(tree_sequence_t *ts)
{
    int ret;
    sparse_tree_t *trees, t;
    size_t j;
    size_t num_trees = tree_sequence_get_num_trees(ts);

    trees = get_tree_list(ts);
    ret = sparse_tree_alloc(&t, ts, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Single forward pass */
    j = 0;
    for (ret = sparse_tree_first(&t); ret == 1; ret = sparse_tree_next(&t)) {
        CU_ASSERT_EQUAL_FATAL(j, t.index);
        ret = sparse_tree_equal(&t, &trees[t.index]);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        j++;
    }
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL_FATAL(j, num_trees);

    /* Single reverse pass */
    j = num_trees;
    for (ret = sparse_tree_last(&t); ret == 1; ret = sparse_tree_prev(&t)) {
        CU_ASSERT_EQUAL_FATAL(j - 1, t.index);
        ret = sparse_tree_equal(&t, &trees[t.index]);
        if (ret != 0) {
            printf("trees differ\n");
            printf("REVERSE tree::\n");
            sparse_tree_print_state(&t, stdout);
            printf("FORWARD tree::\n");
            sparse_tree_print_state(&trees[t.index], stdout);
        }
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        j--;
    }
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL_FATAL(j, 0);

    /* Full forward, then reverse */
    j = 0;
    for (ret = sparse_tree_first(&t); ret == 1; ret = sparse_tree_next(&t)) {
        CU_ASSERT_EQUAL_FATAL(j, t.index);
        ret = sparse_tree_equal(&t, &trees[t.index]);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        j++;
    }
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL_FATAL(j, num_trees);
    j--;
    while ((ret = sparse_tree_prev(&t)) == 1) {
        CU_ASSERT_EQUAL_FATAL(j - 1, t.index);
        ret = sparse_tree_equal(&t, &trees[t.index]);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        j--;
    }
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL_FATAL(j, 0);
    CU_ASSERT_EQUAL_FATAL(t.index, 0);
    /* Calling prev should return 0 and have no effect. */
    for (j = 0; j < 10; j++) {
        ret = sparse_tree_prev(&t);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        CU_ASSERT_EQUAL_FATAL(t.index, 0);
        ret = sparse_tree_equal(&t, &trees[t.index]);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
    }

    /* Full reverse then forward */
    j = num_trees;
    for (ret = sparse_tree_last(&t); ret == 1; ret = sparse_tree_prev(&t)) {
        CU_ASSERT_EQUAL_FATAL(j - 1, t.index);
        ret = sparse_tree_equal(&t, &trees[t.index]);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        j--;
    }
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL_FATAL(j, 0);
    j++;
    while ((ret = sparse_tree_next(&t)) == 1) {
        CU_ASSERT_EQUAL_FATAL(j, t.index);
        ret = sparse_tree_equal(&t, &trees[t.index]);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        j++;
    }
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL_FATAL(j, num_trees);
    CU_ASSERT_EQUAL_FATAL(t.index, num_trees - 1);
    /* Calling next should return 0 and have no effect. */
    for (j = 0; j < 10; j++) {
        ret = sparse_tree_next(&t);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        CU_ASSERT_EQUAL_FATAL(t.index, num_trees - 1);
        ret = sparse_tree_equal(&t, &trees[t.index]);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
    }

    /* Do a zigzagging traversal */
    ret = sparse_tree_first(&t);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    for (j = 1; j < GSL_MIN(10, num_trees / 2); j++) {
        while (t.index < num_trees - j) {
            ret = sparse_tree_next(&t);
            CU_ASSERT_EQUAL_FATAL(ret, 1);
        }
        CU_ASSERT_EQUAL_FATAL(t.index, num_trees - j);
        ret = sparse_tree_equal(&t, &trees[t.index]);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        while (t.index > j) {
            ret = sparse_tree_prev(&t);
            CU_ASSERT_EQUAL_FATAL(ret, 1);
        }
        CU_ASSERT_EQUAL_FATAL(t.index, j);
        ret = sparse_tree_equal(&t, &trees[t.index]);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
    }

    /* Free the trees. */
    ret = sparse_tree_free(&t);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    for (j = 0; j < tree_sequence_get_num_trees(ts); j++) {
        ret = sparse_tree_free(&trees[j]);
    }
    free(trees);
}

static void
verify_hapgen(tree_sequence_t *ts)
{
    int ret;
    hapgen_t hapgen;
    char *haplotype;
    size_t num_samples = tree_sequence_get_num_samples(ts);
    size_t num_sites = tree_sequence_get_num_sites(ts);
    site_t site;
    size_t j;
    int k;
    bool single_char = true;

    for (j = 0; j < num_sites; j++) {
        ret = tree_sequence_get_site(ts, j, &site);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        if (site.ancestral_state_length != 1) {
            single_char = false;
        }
        for (k = 0; k < site.mutations_length; k++) {
            if (site.mutations[k].derived_state_length != 1) {
                single_char = false;
            }
        }
    }

    ret = hapgen_alloc(&hapgen, ts);
    if (single_char) {
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        hapgen_print_state(&hapgen, _devnull);

        for (j = 0; j < num_samples; j++) {
            ret = hapgen_get_haplotype(&hapgen, j, &haplotype);
            CU_ASSERT_EQUAL(ret, 0);
            CU_ASSERT_EQUAL(strlen(haplotype), num_sites);
        }
        for (j = num_samples; j < num_samples + 10; j++) {
            ret = hapgen_get_haplotype(&hapgen, j, &haplotype);
            CU_ASSERT_EQUAL(ret, MSP_ERR_OUT_OF_BOUNDS);
        }
    } else {
        CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_NON_SINGLE_CHAR_MUTATION);
    }
    ret = hapgen_free(&hapgen);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
}

static void
verify_vargen(tree_sequence_t *ts)
{
    int ret;
    vargen_t vargen;
    size_t num_samples = tree_sequence_get_num_samples(ts);
    size_t num_sites = tree_sequence_get_num_sites(ts);
    variant_t *var;
    size_t j, k, f;
    int flags[] = {0, MSP_16_BIT_GENOTYPES};

    for (f = 0; f < sizeof(flags) / sizeof(*flags); f++) {
        ret = vargen_alloc(&vargen, ts, flags[f]);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        vargen_print_state(&vargen, _devnull);
        j = 0;
        while ((ret = vargen_next(&vargen, &var)) == 1) {
            CU_ASSERT_EQUAL(var->site->id, j);
            if (var->site->mutations_length == 0) {
                CU_ASSERT_EQUAL(var->num_alleles, 1);
            } else {
                CU_ASSERT_TRUE(var->num_alleles > 1);
            }
            CU_ASSERT_EQUAL(var->allele_lengths[0], var->site->ancestral_state_length);
            CU_ASSERT_NSTRING_EQUAL_FATAL(var->alleles[0], var->site->ancestral_state,
                    var->allele_lengths[0]);
            for (k = 0; k < var->num_alleles; k++) {
                CU_ASSERT_TRUE(var->allele_lengths[k] >= 0);
            }
            for (k = 0; k < num_samples; k++) {
                if (flags[f] == MSP_16_BIT_GENOTYPES) {
                    CU_ASSERT(var->genotypes.u16[k] <= var->num_alleles);
                } else  {
                    CU_ASSERT(var->genotypes.u8[k] <= var->num_alleles);
                }
            }
            j++;
        }
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        CU_ASSERT_EQUAL(j, num_sites);
        CU_ASSERT_EQUAL_FATAL(vargen_next(&vargen, &var), 0);
        ret = vargen_free(&vargen);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
    }
}

static void
verify_stats(tree_sequence_t *ts)
{
    int ret;
    uint32_t num_samples = tree_sequence_get_num_samples(ts);
    node_id_t *samples;
    uint32_t j;
    double pi;
    int max_site_mutations = get_max_site_mutations(ts);

    ret = tree_sequence_get_pairwise_diversity(ts, NULL, 0, &pi);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = tree_sequence_get_pairwise_diversity(ts, NULL, 1, &pi);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = tree_sequence_get_pairwise_diversity(ts, NULL, num_samples + 1, &pi);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_PARAM_VALUE);

    ret = tree_sequence_get_samples(ts, &samples);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    for (j = 2; j < num_samples; j++) {
        ret = tree_sequence_get_pairwise_diversity(ts, samples, j, &pi);
        if (max_site_mutations <= 1) {
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            CU_ASSERT_TRUE_FATAL(pi >= 0);
        } else {
            CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_UNSUPPORTED_OPERATION);
        }
    }
}

static void
verify_compute_mutation_parents(tree_sequence_t *ts)
{
    int ret;
    size_t size = tree_sequence_get_num_mutations(ts) * sizeof(mutation_id_t);
    mutation_id_t *parent = malloc(size);
    table_collection_t tables;

    CU_ASSERT_FATAL(parent != NULL);
    ret = tree_sequence_dump_tables(ts, &tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    memcpy(parent, tables.mutations.parent, size);
    /* table_collection_print_state(&tables, stdout); */
    /* Make sure the tables are actually updated */
    memset(tables.mutations.parent, 0, size);

    ret = table_collection_compute_mutation_parents(&tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL_FATAL(memcmp(parent, tables.mutations.parent, size), 0);
    /* printf("after\n"); */
    /* table_collection_print_state(&tables, stdout); */

    free(parent);
    table_collection_free(&tables);
}

static void
verify_trees(tree_sequence_t *ts, uint32_t num_trees, node_id_t* parents)
{
    int ret;
    node_id_t u, v;
    uint32_t j, mutation_index, site_index;
    table_size_t k, l, tree_sites_length;
    site_t *sites = NULL;
    sparse_tree_t tree;
    size_t num_nodes = tree_sequence_get_num_nodes(ts);
    size_t num_sites = tree_sequence_get_num_sites(ts);
    size_t num_mutations = tree_sequence_get_num_mutations(ts);

    ret = sparse_tree_alloc(&tree, ts, 0);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(ts), num_trees);

    site_index = 0;
    mutation_index = 0;
    j = 0;
    for (ret = sparse_tree_first(&tree); ret == 1; ret = sparse_tree_next(&tree)) {
        CU_ASSERT_EQUAL(j, tree.index);
        sparse_tree_print_state(&tree, _devnull);
        /* sparse_tree_print_state(&tree, stdout); */
        for (u = 0; u < num_nodes; u++) {
            ret = sparse_tree_get_parent(&tree, u, &v);
            CU_ASSERT_EQUAL(ret, 0);
            CU_ASSERT_EQUAL(v, parents[j * num_nodes + u]);
        }
        ret = sparse_tree_get_sites(&tree, &sites, &tree_sites_length);
        CU_ASSERT_EQUAL(ret, 0);
        for (k = 0; k < tree_sites_length; k++) {
            CU_ASSERT_EQUAL(sites[k].id, site_index);
            for (l = 0; l < sites[k].mutations_length; l++) {
                CU_ASSERT_EQUAL(sites[k].mutations[l].index, mutation_index);
                CU_ASSERT_EQUAL(sites[k].mutations[l].site, site_index);
                mutation_index++;
            }
            site_index++;
        }
        j++;
    }
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(site_index, num_sites);
    CU_ASSERT_EQUAL(mutation_index, num_mutations);

    ret = sparse_tree_next(&tree);
    CU_ASSERT_EQUAL(ret, 0);

    sparse_tree_free(&tree);
}

/* When we keep all sites in simplify, the genotypes for the subset of the
 * samples should be the same as the original */
static void
verify_simplify_genotypes(tree_sequence_t *ts, tree_sequence_t *subset,
        node_id_t *samples, uint32_t num_samples, node_id_t *node_map)
{
    int ret;
    size_t m = tree_sequence_get_num_sites(ts);
    vargen_t vargen, subset_vargen;
    variant_t *variant, *subset_variant;
    size_t j, k;
    node_id_t *all_samples;
    uint8_t a1, a2;
    node_id_t *sample_index_map;

    tree_sequence_get_sample_index_map(ts, &sample_index_map);

    /* tree_sequence_print_state(ts, stdout); */
    /* tree_sequence_print_state(subset, stdout); */

    ret = vargen_alloc(&vargen, ts, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = vargen_alloc(&subset_vargen, subset, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL_FATAL(m, tree_sequence_get_num_sites(subset));
    tree_sequence_get_samples(ts, &all_samples);

    for (j = 0; j < m; j++) {
        ret = vargen_next(&vargen, &variant);
        CU_ASSERT_EQUAL_FATAL(ret, 1);
        ret = vargen_next(&subset_vargen, &subset_variant);
        CU_ASSERT_EQUAL_FATAL(ret, 1);
        CU_ASSERT_EQUAL(variant->site->id, j)
        CU_ASSERT_EQUAL(subset_variant->site->id, j)
        CU_ASSERT_EQUAL(variant->site->position, subset_variant->site->position);
        for (k = 0; k < num_samples; k++) {
            CU_ASSERT_FATAL(sample_index_map[samples[k]] < ts->num_samples);
            a1 = variant->genotypes.u8[sample_index_map[samples[k]]];
            a2 = subset_variant->genotypes.u8[k];
            /* printf("a1 = %d, a2 = %d\n", a1, a2); */
            /* printf("k = %d original node = %d " */
            /*         "original_index = %d a1=%.*s a2=%.*s\n", */
            /*         (int) k, samples[k], sample_index_map[samples[k]], */
            /*         variant->allele_lengths[a1], variant->alleles[a1], */
            /*         subset_variant->allele_lengths[a2], subset_variant->alleles[a2]); */
            CU_ASSERT_FATAL(a1 < variant->num_alleles);
            CU_ASSERT_FATAL(a2 < subset_variant->num_alleles);
            CU_ASSERT_EQUAL_FATAL(variant->allele_lengths[a1],
                    subset_variant->allele_lengths[a2]);
            CU_ASSERT_NSTRING_EQUAL_FATAL(
                variant->alleles[a1], subset_variant->alleles[a2],
                variant->allele_lengths[a1]);
        }
    }
    vargen_free(&vargen);
    vargen_free(&subset_vargen);
}


static void
verify_simplify_properties(tree_sequence_t *ts, tree_sequence_t *subset,
        node_id_t *samples, uint32_t num_samples, node_id_t *node_map)
{
    int ret;
    node_t n1, n2;
    sparse_tree_t full_tree, subset_tree;
    site_t *tree_sites;
    table_size_t tree_sites_length;
    uint32_t j, k;
    node_id_t u, mrca1, mrca2;
    size_t total_sites;

    CU_ASSERT_EQUAL(
        tree_sequence_get_sequence_length(ts),
        tree_sequence_get_sequence_length(subset));
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(subset), num_samples);
    CU_ASSERT(
        tree_sequence_get_num_nodes(ts) >= tree_sequence_get_num_nodes(subset));
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(subset), num_samples);

    /* Check the sample properties */
    for (j = 0; j < num_samples; j++) {
        ret = tree_sequence_get_node(ts, samples[j], &n1);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        CU_ASSERT_EQUAL(node_map[samples[j]], j);
        ret = tree_sequence_get_node(subset, node_map[samples[j]], &n2);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        CU_ASSERT_EQUAL_FATAL(n1.population, n2.population);
        CU_ASSERT_EQUAL_FATAL(n1.time, n2.time);
        CU_ASSERT_EQUAL_FATAL(n1.flags, n2.flags);
        CU_ASSERT_EQUAL_FATAL(n1.metadata_length, n2.metadata_length);
        CU_ASSERT_NSTRING_EQUAL(n1.metadata, n2.metadata, n2.metadata_length);
    }
    /* Check that node mappings are correct */
    for (j = 0; j < tree_sequence_get_num_nodes(ts); j++) {
        ret = tree_sequence_get_node(ts, j, &n1);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        if (node_map[j] != MSP_NULL_NODE) {
            ret = tree_sequence_get_node(subset, node_map[j], &n2);
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            CU_ASSERT_EQUAL_FATAL(n1.population, n2.population);
            CU_ASSERT_EQUAL_FATAL(n1.time, n2.time);
            CU_ASSERT_EQUAL_FATAL(n1.flags, n2.flags);
            CU_ASSERT_EQUAL_FATAL(n1.metadata_length, n2.metadata_length);
            CU_ASSERT_NSTRING_EQUAL(n1.metadata, n2.metadata, n2.metadata_length);
        }
    }
    if (num_samples == 0) {
        CU_ASSERT_EQUAL(tree_sequence_get_num_edges(subset), 0);
        CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(subset), 0);
    } else if (num_samples == 1) {
        CU_ASSERT_EQUAL(tree_sequence_get_num_edges(subset), 0);
        CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(subset), 1);
    }
    /* Check the pairwise MRCAs */
    ret = sparse_tree_alloc(&full_tree, ts, 0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = sparse_tree_alloc(&subset_tree, subset, 0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = sparse_tree_first(&full_tree);
    CU_ASSERT_EQUAL(ret, 1);
    ret = sparse_tree_first(&subset_tree);
    CU_ASSERT_EQUAL(ret, 1);

    total_sites = 0;
    while (1) {
        while (full_tree.right <= subset_tree.right) {
            for (j = 0; j < num_samples; j++) {
                for (k = j + 1; k < num_samples; k++) {
                    ret = sparse_tree_get_mrca(&full_tree, samples[j], samples[k], &mrca1);
                    CU_ASSERT_EQUAL_FATAL(ret, 0);
                    ret = sparse_tree_get_mrca(&subset_tree,
                            node_map[samples[j]], node_map[samples[k]], &mrca2);
                    CU_ASSERT_EQUAL_FATAL(ret, 0);
                    if (mrca1 == MSP_NULL_NODE) {
                        CU_ASSERT_EQUAL_FATAL(mrca2, MSP_NULL_NODE);
                    } else {
                        CU_ASSERT_EQUAL(node_map[mrca1], mrca2);
                    }
                }
            }
            ret = sparse_tree_next(&full_tree);
            CU_ASSERT_FATAL(ret >= 0);
            if (ret != 1) {
                break;
            }
        }
        /* Check the sites in this tree */
        ret = sparse_tree_get_sites(&subset_tree, &tree_sites, &tree_sites_length);
        CU_ASSERT_EQUAL_FATAL(ret, 0);

        for (j = 0; j < tree_sites_length; j++) {
            CU_ASSERT(subset_tree.left <= tree_sites[j].position);
            CU_ASSERT(tree_sites[j].position < subset_tree.right);
            for (k = 0; k < tree_sites[j].mutations_length; k++) {
                ret = sparse_tree_get_parent(&subset_tree,
                        tree_sites[j].mutations[k].node, &u);
                CU_ASSERT_EQUAL(ret, 0);
            }
            total_sites++;
        }
        ret = sparse_tree_next(&subset_tree);
        if (ret != 1) {
            break;
        }
    }
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_sites(subset), total_sites);

    sparse_tree_free(&subset_tree);
    sparse_tree_free(&full_tree);
    verify_vargen(subset);
    verify_hapgen(subset);
}

static void
verify_simplify(tree_sequence_t *ts)
{
    int ret;
    uint32_t n = tree_sequence_get_num_samples(ts);
    uint32_t num_samples[] = {0, 1, 2, 3, n / 2, n - 1, n};
    size_t j;
    node_id_t *sample;
    node_id_t *node_map = malloc(tree_sequence_get_num_nodes(ts) * sizeof(node_id_t));
    tree_sequence_t subset;
    int flags = MSP_FILTER_ZERO_MUTATION_SITES;

    CU_ASSERT_FATAL(node_map != NULL);
    ret = tree_sequence_get_samples(ts, &sample);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    for (j = 0; j < sizeof(num_samples) / sizeof(uint32_t); j++) {
        if (num_samples[j] <= n) {
            ret = tree_sequence_simplify(ts, sample, num_samples[j], flags, &subset,
                    node_map);
            /* printf("ret = %s\n", msp_strerror(ret)); */
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            verify_simplify_properties(ts, &subset, sample, num_samples[j], node_map);
            tree_sequence_free(&subset);

            /* Keep all sites */
            ret = tree_sequence_simplify(ts, sample, num_samples[j], 0, &subset,
                    node_map);
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            verify_simplify_properties(ts, &subset, sample, num_samples[j], node_map);
            verify_simplify_genotypes(ts, &subset, sample, num_samples[j], node_map);
            tree_sequence_free(&subset);
        }
    }
    free(node_map);
}

/* Utility function to return a tree sequence for testing. It is the
 * callers responsilibility to free all memory.
 */
static tree_sequence_t *
get_example_tree_sequence(uint32_t num_samples,
        uint32_t num_historical_samples, uint32_t num_loci,
        double sequence_length, double recombination_rate,
        double mutation_rate, uint32_t num_bottlenecks,
        bottleneck_desc_t *bottlenecks, int alphabet)
{
    int ret;
    msp_t *msp = malloc(sizeof(msp_t));
    sample_t *samples = malloc(num_samples * sizeof(sample_t));
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);
    /* TODO convert tree_seq etc to local variables rather than mallocing memory */
    tree_sequence_t *tree_seq = malloc(sizeof(tree_sequence_t));
    recomb_map_t *recomb_map = malloc(sizeof(recomb_map_t));
    mutgen_t *mutgen = malloc(sizeof(mutgen_t));
    table_collection_t tables;
    char *timestamp = "timestamp";
    char *record = "get_example_tree_sequence";
    char *metadata = "metadata";
    double location[2] = {0, 1};
    uint32_t j;
    size_t num_populations = 3;
    double migration_matrix[] = {
        0.0, 1.0, 1.0,
        1.0, 0.0, 1.0,
        1.0, 1.0, 0.0
    };
    double positions[] = {0.0, 0.0};
    double rates[] = {0.0, 0.0};

    CU_ASSERT_FATAL(msp != NULL);
    CU_ASSERT_FATAL(samples != NULL);
    CU_ASSERT_FATAL(rng != NULL);
    CU_ASSERT_FATAL(recomb_map != NULL);
    gsl_rng_set(rng, 1);

    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL(ret, 0);

    ret = mutgen_alloc(mutgen, mutation_rate, rng, alphabet, 10);
    CU_ASSERT_EQUAL(ret, 0);
    /* initialise the samples to zero for the default configuration */
    memset(samples, 0, num_samples * sizeof(sample_t));
    for (j = 0; j < num_historical_samples; j++) {
        samples[j].time = 0.1 * (j + 1);
    }
    ret = msp_alloc(msp, num_samples, samples, rng);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_num_loci(msp, num_loci);
    CU_ASSERT_EQUAL(ret, 0);
    ret = msp_set_recombination_rate(msp, recombination_rate);
    CU_ASSERT_EQUAL(ret, 0);
    for (j = 0; j < num_bottlenecks; j++) {
        if (bottlenecks[j].type == SIMPLE_BOTTLENECK) {
            ret = msp_add_simple_bottleneck(msp, bottlenecks[j].time,
                    bottlenecks[j].population_id, bottlenecks[j].parameter);
            CU_ASSERT_EQUAL(ret, 0);
        } else if(bottlenecks[j].type == INSTANTANEOUS_BOTTLENECK) {
            ret = msp_add_instantaneous_bottleneck(msp, bottlenecks[j].time,
                    bottlenecks[j].population_id, bottlenecks[j].parameter);
            CU_ASSERT_EQUAL(ret, 0);
        } else {
            CU_ASSERT_FATAL(0 == 1);
        }
    }
    ret = msp_set_num_populations(msp, num_populations);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = msp_set_migration_matrix(msp, num_populations * num_populations,
            migration_matrix);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = msp_set_store_migrations(msp, true);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = msp_initialise(msp);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = msp_run(msp, DBL_MAX, ULONG_MAX);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    rates[0] = recombination_rate;
    positions[1] = sequence_length;
    ret = recomb_map_alloc(recomb_map, num_loci, sequence_length,
            positions, rates, 2);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Create the tree_sequence from the state of the simulator.
     * We want to use coalescent time here, so use an Ne of 1/4
     * to cancel scaling factor. */
    tables.sequence_length = sequence_length;
    ret = msp_populate_tables(msp, recomb_map, &tables.nodes, &tables.edges,
            &tables.migrations, &tables.populations);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = mutgen_generate_tables_tmp(mutgen, &tables.nodes, &tables.edges);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = mutgen_populate_tables(mutgen, &tables.sites, &tables.mutations);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = provenance_table_add_row(&tables.provenances,
            timestamp, strlen(timestamp), record, strlen(record));
    CU_ASSERT_FATAL(ret >= 0);
    /* Add an individual for each sample and update the node table to
     * point to it. */
    for (j = 0; j < num_samples; j++) {
        ret = individual_table_add_row(&tables.individuals, 0,
            location, j % 3, metadata, j % strlen(metadata));
        CU_ASSERT_FATAL(ret >= 0);
        tables.nodes.individual[j] = j;
    }
    /* Clear out the populations added by msprime so that we can
     * add metadata */
    population_table_clear(&tables.populations);
    for (j = 0; j < num_samples; j++) {
        ret = population_table_add_row(&tables.populations, metadata,
                j % strlen(metadata));
        CU_ASSERT_FATAL(ret >= 0);
    }
    ret = tree_sequence_load_tables(tree_seq, &tables, 0);
    /* edge_table_print_state(edges, stdout); */
    /* printf("ret = %s\n", msp_strerror(ret)); */
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    gsl_rng_free(rng);
    free(samples);
    msp_free(msp);
    free(msp);
    recomb_map_free(recomb_map);
    free(recomb_map);
    mutgen_free(mutgen);
    free(mutgen);
    table_collection_free(&tables);
    return tree_seq;
}

tree_sequence_t **
get_example_nonbinary_tree_sequences(void)
{
    size_t max_examples = 1024;
    tree_sequence_t **ret = malloc(max_examples * sizeof(tree_sequence_t *));
    bottleneck_desc_t bottlenecks[] = {
        {SIMPLE_BOTTLENECK, 0.1, 0, 0.5},
        {INSTANTANEOUS_BOTTLENECK, 0.4, 0, 10.0},
    };
    bottleneck_desc_t other_bottlenecks[] = {
        {SIMPLE_BOTTLENECK, 0.1, 0, 0.1},
        {SIMPLE_BOTTLENECK, 0.15, 0, 0.75},
        {INSTANTANEOUS_BOTTLENECK, 0.2, 0, 0.1},
    };

    CU_ASSERT_FATAL(ret != NULL);
    ret[0] = get_example_tree_sequence(100, 0, 100, 100.0, 10.0, 1.0,
            1, bottlenecks, MSP_ALPHABET_BINARY);
    ret[1] = get_example_tree_sequence(10, 2, 100, 10.0, 1.0, 2.0,
            1, bottlenecks, MSP_ALPHABET_BINARY);
    ret[2] = get_example_tree_sequence(500, 10, 10, 1000.0, 0.5, 3.0,
            2, bottlenecks, MSP_ALPHABET_ASCII);
    ret[3] = get_example_tree_sequence(100, 0, 100, 1.0, 1.0, 0.0,
            3, other_bottlenecks, MSP_ALPHABET_ASCII);
    ret[4] = NULL;
    return ret;
}

tree_sequence_t *
make_recurrent_and_back_mutations_copy(tree_sequence_t *ts)
{
    int ret;
    tree_sequence_t *new_ts = malloc(sizeof(tree_sequence_t));
    sparse_tree_t tree;
    table_collection_t tables;
    node_id_t *stack;
    mutation_id_t *mutation, parent;
    node_id_t u, v, root;
    site_id_t site_id;
    char *state = NULL;
    int stack_top = 0;
    size_t j;
    size_t num_mutations_per_branch = 2;
    char *timestamp = "timestamp";
    char *record = "make_recurrent_and_back_mutations_copy";
    char *metadata;

    CU_ASSERT_FATAL(new_ts != NULL);
    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = sparse_tree_alloc(&tree, ts, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    state = malloc(tree_sequence_get_num_nodes(ts) * sizeof(char));
    CU_ASSERT_FATAL(state != NULL);
    mutation = malloc(tree_sequence_get_num_nodes(ts) * sizeof(mutation_id_t));
    CU_ASSERT_FATAL(mutation != NULL);
    ret = tree_sequence_dump_tables(ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    site_table_clear(&tables.sites);
    mutation_table_clear(&tables.mutations);

    stack = tree.stack1;
    site_id = 0;
    for (ret = sparse_tree_first(&tree); ret == 1; ret = sparse_tree_next(&tree)) {
        /* add some fake metadata here to make sure we have cases with site metadata
         * in our examples */
        ret = site_table_add_row(&tables.sites, tree.left, "0", 1, "recurrent", 9);
        CU_ASSERT_FATAL(ret >= 0);
        for (root = tree.left_root; root != MSP_NULL_NODE; root = tree.right_sib[root]) {
            /* Traverse down the tree and put a mutation on every branch. */
            memset(mutation, 0xff, tree_sequence_get_num_nodes(ts) * sizeof(mutation_id_t));
            stack_top = 0;
            stack[0] = root;
            state[root] = 0;
            while (stack_top >= 0) {
                u = stack[stack_top];
                stack_top--;
                for (v = tree.left_child[u]; v != MSP_NULL_NODE; v = tree.right_sib[v]) {
                    stack_top++;
                    stack[stack_top] = v;
                }
                v = tree.parent[u];
                if (v != MSP_NULL_NODE) {
                    state[u] = state[v];
                    parent = mutation[v];
                    for (j = 0; j < num_mutations_per_branch; j++) {
                        state[u] = (state[u] + 1) % 2;
                        mutation[u] = tables.mutations.num_rows;
                        metadata = state[u] == 0? "back": "forward";
                        ret = mutation_table_add_row(&tables.mutations, site_id, u,
                                parent, state[u] == 0? "0": "1", 1,
                                metadata, strlen(metadata));
                        parent = mutation[u];
                        CU_ASSERT_FATAL(ret >= 0);
                    }
                }
            }
            site_id++;
        }
    }
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    ret = provenance_table_add_row(&tables.provenances,
            timestamp, strlen(timestamp), record, strlen(record));
    CU_ASSERT_FATAL(ret >= 0);
    ret = tree_sequence_load_tables(new_ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    table_collection_free(&tables);
    sparse_tree_free(&tree);
    free(state);
    free(mutation);
    return new_ts;
}

tree_sequence_t *
make_permuted_nodes_copy(tree_sequence_t *ts)
{
    int ret;
    size_t j;
    tree_sequence_t *new_ts = malloc(sizeof(tree_sequence_t));
    table_collection_t tables;
    node_id_t *node_map;
    node_t node;
    edge_t edge;
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);
    size_t num_nodes = tree_sequence_get_num_nodes(ts);
    char *timestamp = "timestamp";
    char *record = "make_permuted_nodes_copy";

    CU_ASSERT_FATAL(new_ts != NULL);
    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    node_map = malloc(num_nodes * sizeof(node_id_t));
    CU_ASSERT_FATAL(node_map != NULL);

    ret = tree_sequence_dump_tables(ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    for (j = 0; j < num_nodes; j++) {
        node_map[j] = j;
    }
    gsl_rng_set(rng, 1);
    gsl_ran_shuffle(rng, node_map, num_nodes, sizeof(node_id_t));
    for (j = 0; j < num_nodes; j++) {
        ret = tree_sequence_get_node(ts, j, &node);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        tables.nodes.flags[node_map[j]] = node.flags;
        tables.nodes.time[node_map[j]] = node.time;
        tables.nodes.population[node_map[j]] = node.population;
        /* Assume all metadata is 0 length */
    }
    edge_table_clear(&tables.edges);
    for (j = 0; j < tree_sequence_get_num_edges(ts); j++) {
        ret = tree_sequence_get_edge(ts, j, &edge);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        ret = edge_table_add_row(&tables.edges, edge.left, edge.right,
                node_map[edge.parent], node_map[edge.child]);
        CU_ASSERT_FATAL(ret >= 0);
    }
    for (j = 0; j < tables.mutations.num_rows; j++) {
        tables.mutations.node[j] = node_map[tables.mutations.node[j]];
    }
    ret = sort_tables(&tables.nodes, &tables.edges, &tables.migrations,
            &tables.sites, &tables.mutations, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = provenance_table_add_row(&tables.provenances,
            timestamp, strlen(timestamp), record, strlen(record));
    CU_ASSERT_FATAL(ret >= 0);
    ret = tree_sequence_load_tables(new_ts, &tables, MSP_BUILD_INDEXES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    table_collection_free(&tables);
    gsl_rng_free(rng);
    free(node_map);
    return new_ts;
}

/* Insert some gaps into the specified tree sequence, i.e., positions
 * that no edge covers. */
tree_sequence_t *
make_gappy_copy(tree_sequence_t *ts)
{
    int ret;
    size_t j;
    tree_sequence_t *new_ts = malloc(sizeof(tree_sequence_t));
    table_collection_t tables;
    edge_t edge;
    double left, right;
    double gap_size = 1e-4;
    char *timestamp = "timestamp";
    char *record = "make_gappy_copy";

    CU_ASSERT_FATAL(new_ts != NULL);
    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = tree_sequence_dump_tables(ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    edge_table_clear(&tables.edges);
    for (j = 0; j < tree_sequence_get_num_edges(ts); j++) {
        ret = tree_sequence_get_edge(ts, j, &edge);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        /* Move all coordiantes to the right to create an initial gap. */
        left = edge.left + gap_size;
        right = edge.right + gap_size;
        ret = edge_table_add_row(&tables.edges, left, right, edge.parent, edge.child);
        CU_ASSERT_FATAL(ret >= 0);
    }
    for (j = 0; j < tables.mutations.num_rows; j++) {
        tables.sites.position[j] += gap_size;
    }
    /* Add a site into the gap at the end. */
    ret = site_table_add_row(&tables.sites, ts->sequence_length + 0.5, "0", 1, "end-gap", 7);
    CU_ASSERT_FATAL(ret >= 0);
    ret = mutation_table_add_row(&tables.mutations, ret, 0, MSP_NULL_MUTATION,
            "1", 1, NULL, 0);
    CU_ASSERT_FATAL(ret >= 0);
    ret = provenance_table_add_row(&tables.provenances,
            timestamp, strlen(timestamp), record, strlen(record));
    CU_ASSERT_FATAL(ret >= 0);
    tables.sequence_length = ts->sequence_length + 1.0;
    ret = tree_sequence_load_tables(new_ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    table_collection_free(&tables);
    return new_ts;
}

/* Return a copy of the tree sequence after deleting half of its edges.
 */
tree_sequence_t *
make_decapitated_copy(tree_sequence_t *ts)
{
    int ret;
    tree_sequence_t *new_ts = malloc(sizeof(tree_sequence_t));
    table_collection_t tables;
    size_t j;
    node_id_t oldest_node;
    char *timestamp = "timestamp";
    char *record = "make_decapitated_copy";

    CU_ASSERT_FATAL(new_ts != NULL);
    ret = tree_sequence_dump_tables(ts, &tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    tables.edges.num_rows -= tables.edges.num_rows / 4;
    oldest_node = tables.edges.parent[tables.edges.num_rows - 1];
    j = 0;
    while (j < tables.mutations.num_rows && tables.mutations.node[j] < oldest_node) {
        j++;
    }
    tables.mutations.num_rows = j;
    tables.mutations.derived_state_length = j;
    tables.sites.num_rows = j;
    tables.sites.ancestral_state_length = j;

    ret = provenance_table_add_row(&tables.provenances,
            timestamp, strlen(timestamp), record, strlen(record));
    CU_ASSERT_FATAL(ret >= 0);
    ret = tree_sequence_load_tables(new_ts, &tables, MSP_BUILD_INDEXES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    table_collection_free(&tables);
    return new_ts;
}

tree_sequence_t *
make_multichar_mutations_copy(tree_sequence_t *ts)
{
    int ret;
    tree_sequence_t *new_ts = malloc(sizeof(tree_sequence_t));
    table_collection_t tables;
    size_t j;
    char *timestamp = "timestamp";
    char *record = "make_multichar_mutations_copy";
    char string[] = "ACCCTTAAGGAAGGCCGG";

    CU_ASSERT_FATAL(new_ts != NULL);
    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = tree_sequence_dump_tables(ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    site_table_clear(&tables.sites);
    mutation_table_clear(&tables.mutations);

    for (j = 0; j < GSL_MIN(strlen(string), ts->num_samples); j++) {
        ret = site_table_add_row(&tables.sites,
                j * (ts->sequence_length / strlen(string)),
                string, j, NULL, 0);
        CU_ASSERT_FATAL(ret >= 0);
        ret = mutation_table_add_row(&tables.mutations, j, j, MSP_NULL_NODE,
                string, j + 1, NULL, 0);
        CU_ASSERT_FATAL(ret >= 0);
    }
    ret = provenance_table_add_row(&tables.provenances,
            timestamp, strlen(timestamp), record, strlen(record));
    CU_ASSERT_FATAL(ret >= 0);
    ret = tree_sequence_load_tables(new_ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    table_collection_free(&tables);
    return new_ts;
}

tree_sequence_t **
get_example_tree_sequences(int include_nonbinary)
{
    size_t max_examples = 1024;
    int j, k;
    tree_sequence_t **ret = malloc(max_examples * sizeof(tree_sequence_t *));
    tree_sequence_t **nonbinary = NULL;
    CU_ASSERT_FATAL(ret != NULL);

    ret[0] = get_example_tree_sequence(10, 0, 100, 100.0, 1.0, 1.0, 0, NULL,
            MSP_ALPHABET_BINARY);
    ret[1] = get_example_tree_sequence(2, 0, 1, 0.1, 1.0, 1.0, 0, NULL,
            MSP_ALPHABET_BINARY);
    ret[2] = get_example_tree_sequence(3, 0, 3, 10.0, 10.0, 0.0, 0, NULL,
            MSP_ALPHABET_ASCII);
    ret[3] = get_example_tree_sequence(10, 0, UINT32_MAX, 10.0,
            9.31322575049e-08, 10.0, 0, NULL, MSP_ALPHABET_ASCII);
    ret[4] = make_recurrent_and_back_mutations_copy(ret[0]);
    ret[5] = make_permuted_nodes_copy(ret[0]);
    ret[6] = make_gappy_copy(ret[0]);
    ret[7] = make_decapitated_copy(ret[0]);
    ret[8] = make_multichar_mutations_copy(ret[0]);
    k = 9;
    if (include_nonbinary) {
        nonbinary = get_example_nonbinary_tree_sequences();
        for (j = 0; nonbinary[j] != NULL; j++) {
            ret[k] = nonbinary[j];
            k++;
        }
        free(nonbinary);
    }
    ret[k] = NULL;
    return ret;
}

static void
verify_vcf_converter(tree_sequence_t *ts, unsigned int ploidy)
{
    int ret;
    char *str = NULL;
    vcf_converter_t vc;
    unsigned int num_variants;

    ret = vcf_converter_alloc(&vc, ts, ploidy, "chr1234");
    CU_ASSERT_FATAL(ret ==  0);
    vcf_converter_print_state(&vc, _devnull);
    ret = vcf_converter_get_header(&vc, &str);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_NSTRING_EQUAL("##", str, 2);
    num_variants = 0;
    while ((ret = vcf_converter_next(&vc, &str)) == 1) {
        CU_ASSERT_NSTRING_EQUAL("chr1234\t", str, 2);
        num_variants++;
    }
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_TRUE(num_variants == tree_sequence_get_num_mutations(ts));
    vcf_converter_free(&vc);
}

static void
test_vcf(void)
{
    int ret;
    unsigned int ploidy;
    vcf_converter_t *vc = malloc(sizeof(vcf_converter_t));
    tree_sequence_t *ts = get_example_tree_sequence(10, 0, 100, 100.0, 1.0, 1.0,
            0, NULL, MSP_ALPHABET_BINARY);

    CU_ASSERT_FATAL(ts != NULL);
    CU_ASSERT_FATAL(vc != NULL);

    ret = vcf_converter_alloc(vc, ts, 0, "1");
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = vcf_converter_alloc(vc, ts, 3, "1");
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = vcf_converter_alloc(vc, ts, 11, "1");
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);

    for (ploidy = 1; ploidy < 3; ploidy++) {
        verify_vcf_converter(ts, ploidy);
    }

    free(vc);
    tree_sequence_free(ts);
    free(ts);
}

static void
test_vcf_no_mutations(void)
{
    int ret;
    char *str = NULL;
    vcf_converter_t *vc = malloc(sizeof(vcf_converter_t));
    tree_sequence_t *ts = get_example_tree_sequence(100, 0, 1, 1.0, 0.0, 0.0, 0, NULL,
            MSP_ALPHABET_BINARY);

    CU_ASSERT_FATAL(ts != NULL);
    CU_ASSERT_FATAL(vc != NULL);
    CU_ASSERT_EQUAL_FATAL(tree_sequence_get_num_mutations(ts), 0);

    ret = vcf_converter_alloc(vc, ts, 1, "1");
    CU_ASSERT_FATAL(ret ==  0);
    vcf_converter_print_state(vc, _devnull);
    ret = vcf_converter_get_header(vc, &str);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_NSTRING_EQUAL("##", str, 2);
    ret = vcf_converter_next(vc, &str);
    CU_ASSERT_EQUAL(ret, 0);
    vcf_converter_free(vc);

    free(vc);
    tree_sequence_free(ts);
    free(ts);
}

static void
test_node_metadata(void)
{
    const char *nodes =
        "1  0   0   -1   n1\n"
        "1  0   0   -1   n2\n"
        "0  1   0   -1   A_much_longer_name\n"
        "0  1   0   -1\n"
        "0  1   0   -1   n4";
    const char *edges =
        "0  1   2   0,1\n";
    tree_sequence_t ts;
    int ret;
    node_t node;

    tree_sequence_from_text(&ts, 0, nodes, edges, NULL, NULL, NULL, NULL, NULL);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&ts), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), 5);

    ret = tree_sequence_get_node(&ts, 0, &node);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_NSTRING_EQUAL(node.metadata, "n1", 2);

    ret = tree_sequence_get_node(&ts, 1, &node);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_NSTRING_EQUAL(node.metadata, "n2", 2);

    ret = tree_sequence_get_node(&ts, 2, &node);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_NSTRING_EQUAL(node.metadata, "A_much_longer_name", 18);

    ret = tree_sequence_get_node(&ts, 3, &node);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_NSTRING_EQUAL(node.metadata, "", 0);

    ret = tree_sequence_get_node(&ts, 4, &node);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_NSTRING_EQUAL(node.metadata, "n4", 2);

    tree_sequence_free(&ts);
}

static void
verify_trees_consistent(tree_sequence_t *ts)
{
    int ret;
    size_t num_trees;
    sparse_tree_t tree;

    ret = sparse_tree_alloc(&tree, ts, 0);
    CU_ASSERT_EQUAL(ret, 0);

    num_trees = 0;
    for (ret = sparse_tree_first(&tree); ret == 1; ret = sparse_tree_next(&tree)) {
        sparse_tree_print_state(&tree, _devnull);
        CU_ASSERT_EQUAL(tree.index, num_trees);
        num_trees++;
    }
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(ts), num_trees);

    sparse_tree_free(&tree);
}

static void
verify_ld(tree_sequence_t *ts)
{
    int ret;
    size_t num_sites = tree_sequence_get_num_sites(ts);
    site_t *sites = malloc(num_sites * sizeof(site_t));
    int *num_site_mutations = malloc(num_sites * sizeof(int));
    ld_calc_t ld_calc;
    double *r2, *r2_prime, x;
    size_t j, num_r2_values;
    double eps = 1e-6;

    r2 = calloc(num_sites, sizeof(double));
    r2_prime = calloc(num_sites, sizeof(double));
    CU_ASSERT_FATAL(r2 != NULL);
    CU_ASSERT_FATAL(r2_prime != NULL);
    CU_ASSERT_FATAL(sites != NULL);
    CU_ASSERT_FATAL(num_site_mutations != NULL);

    ret = ld_calc_alloc(&ld_calc, ts);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ld_calc_print_state(&ld_calc, _devnull);

    for (j = 0; j < num_sites; j++) {
        ret = tree_sequence_get_site(ts, j, sites + j);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        num_site_mutations[j] = sites[j].mutations_length;
        ret = ld_calc_get_r2(&ld_calc, j, j, &x);
        if (num_site_mutations[j] <= 1) {
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            CU_ASSERT_DOUBLE_EQUAL_FATAL(x, 1.0, eps);
        } else {
            CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_UNSUPPORTED_OPERATION);
        }
    }

    if (num_sites > 0) {
        /* Some checks in the forward direction */
        ret = ld_calc_get_r2_array(&ld_calc, 0, MSP_DIR_FORWARD,
                num_sites, DBL_MAX, r2, &num_r2_values);
        if (multi_mutations_exist(ts, 0, num_sites)) {
            CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_UNSUPPORTED_OPERATION);
        } else {
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            CU_ASSERT_EQUAL_FATAL(num_r2_values, num_sites - 1);
        }
        ld_calc_print_state(&ld_calc, _devnull);

        ret = ld_calc_get_r2_array(&ld_calc, num_sites - 2, MSP_DIR_FORWARD,
                num_sites, DBL_MAX, r2_prime, &num_r2_values);
        if (multi_mutations_exist(ts, num_sites - 2, num_sites)) {
            CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_UNSUPPORTED_OPERATION);
        } else {
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            CU_ASSERT_EQUAL_FATAL(num_r2_values, 1);
        }
        ld_calc_print_state(&ld_calc, _devnull);

        ret = ld_calc_get_r2_array(&ld_calc, 0, MSP_DIR_FORWARD,
                num_sites, DBL_MAX, r2_prime, &num_r2_values);
        if (multi_mutations_exist(ts, 0, num_sites)) {
            CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_UNSUPPORTED_OPERATION);
        } else {
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            CU_ASSERT_EQUAL_FATAL(num_r2_values, num_sites - 1);
            ld_calc_print_state(&ld_calc, _devnull);
            for (j = 0; j < num_r2_values; j++) {
                CU_ASSERT_EQUAL_FATAL(r2[j], r2_prime[j]);
                ret = ld_calc_get_r2(&ld_calc, 0, j + 1, &x);
                CU_ASSERT_EQUAL_FATAL(ret, 0);
                CU_ASSERT_DOUBLE_EQUAL_FATAL(r2[j], x, eps);
            }

        }

        /* Some checks in the reverse direction */
        ret = ld_calc_get_r2_array(&ld_calc, num_sites - 1,
                MSP_DIR_REVERSE, num_sites, DBL_MAX,
                r2, &num_r2_values);
        if (multi_mutations_exist(ts, 0, num_sites)) {
            CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_UNSUPPORTED_OPERATION);
        } else {
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            CU_ASSERT_EQUAL_FATAL(num_r2_values, num_sites - 1);
        }
        ld_calc_print_state(&ld_calc, _devnull);

        ret = ld_calc_get_r2_array(&ld_calc, 1, MSP_DIR_REVERSE,
                num_sites, DBL_MAX, r2_prime, &num_r2_values);
        if (multi_mutations_exist(ts, 0, 1)) {
            CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_UNSUPPORTED_OPERATION);
        } else {
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            CU_ASSERT_EQUAL_FATAL(num_r2_values, 1);
        }
        ld_calc_print_state(&ld_calc, _devnull);

        ret = ld_calc_get_r2_array(&ld_calc, num_sites - 1,
                MSP_DIR_REVERSE, num_sites, DBL_MAX,
                r2_prime, &num_r2_values);
        if (multi_mutations_exist(ts, 0, num_sites)) {
            CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_UNSUPPORTED_OPERATION);
        } else {
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            CU_ASSERT_EQUAL_FATAL(num_r2_values, num_sites - 1);
            ld_calc_print_state(&ld_calc, _devnull);

            for (j = 0; j < num_r2_values; j++) {
                CU_ASSERT_EQUAL_FATAL(r2[j], r2_prime[j]);
                ret = ld_calc_get_r2(&ld_calc, num_sites - 1,
                        num_sites - j - 2, &x);
                CU_ASSERT_EQUAL_FATAL(ret, 0);
                CU_ASSERT_DOUBLE_EQUAL_FATAL(r2[j], x, eps);
            }
        }

        /* Check some error conditions */
        ret = ld_calc_get_r2_array(&ld_calc, 0, 0, num_sites, DBL_MAX,
            r2, &num_r2_values);
        CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    }

    if (num_sites > 3) {
        /* Check for some basic distance calculations */
        j = num_sites / 2;
        x = sites[j + 1].position - sites[j].position;
        ret = ld_calc_get_r2_array(&ld_calc, j, MSP_DIR_FORWARD, num_sites,
                x, r2, &num_r2_values);
        if (multi_mutations_exist(ts, j, num_sites)) {
            CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_UNSUPPORTED_OPERATION);
        } else {
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            CU_ASSERT_EQUAL_FATAL(num_r2_values, 1);
        }

        x = sites[j].position - sites[j - 1].position;
        ret = ld_calc_get_r2_array(&ld_calc, j, MSP_DIR_REVERSE, num_sites,
                x, r2, &num_r2_values);
        if (multi_mutations_exist(ts, 0, j + 1)) {
            CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_UNSUPPORTED_OPERATION);
        } else {
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            CU_ASSERT_EQUAL_FATAL(num_r2_values, 1);
        }
    }

    /* Check some error conditions */
    for (j = num_sites; j < num_sites + 2; j++) {
        ret = ld_calc_get_r2_array(&ld_calc, j, MSP_DIR_FORWARD,
                num_sites, DBL_MAX, r2, &num_r2_values);
        CU_ASSERT_EQUAL(ret, MSP_ERR_OUT_OF_BOUNDS);
        ret = ld_calc_get_r2(&ld_calc, j, 0, r2);
        CU_ASSERT_EQUAL(ret, MSP_ERR_OUT_OF_BOUNDS);
        ret = ld_calc_get_r2(&ld_calc, 0, j, r2);
        CU_ASSERT_EQUAL(ret, MSP_ERR_OUT_OF_BOUNDS);
    }

    ld_calc_free(&ld_calc);
    free(r2);
    free(r2_prime);
    free(sites);
    free(num_site_mutations);
}

static void
verify_empty_tree_sequence(tree_sequence_t *ts, double sequence_length)
{
    CU_ASSERT_EQUAL(tree_sequence_get_num_edges(ts), 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(ts), 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(ts), 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_migrations(ts), 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(ts), 0);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(ts), sequence_length);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(ts), 1);
    verify_trees_consistent(ts);
    verify_ld(ts);
    verify_stats(ts);
    verify_hapgen(ts);
    verify_vargen(ts);
    verify_vcf_converter(ts, 1);
}

static void
test_empty_tree_sequence(void)
{
    tree_sequence_t ts;
    table_collection_t tables;
    sparse_tree_t t;
    node_id_t v;
    int ret;

    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_SEQUENCE_LENGTH);
    tree_sequence_free(&ts);
    tables.sequence_length = 1.0;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    verify_empty_tree_sequence(&ts, 1.0);

    ret = sparse_tree_alloc(&t, &ts, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = sparse_tree_first(&t);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL_FATAL(t.left_root, MSP_NULL_NODE);
    CU_ASSERT_EQUAL_FATAL(t.left, 0);
    CU_ASSERT_EQUAL_FATAL(t.right, 1);
    CU_ASSERT_EQUAL_FATAL(sparse_tree_get_parent(&t, 0, &v), MSP_ERR_OUT_OF_BOUNDS);
    sparse_tree_free(&t);

    ret = sparse_tree_alloc(&t, &ts, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = sparse_tree_last(&t);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL_FATAL(t.left_root, MSP_NULL_NODE);
    CU_ASSERT_EQUAL_FATAL(t.left, 0);
    CU_ASSERT_EQUAL_FATAL(t.right, 1);
    CU_ASSERT_EQUAL_FATAL(sparse_tree_get_parent(&t, 0, &v), MSP_ERR_OUT_OF_BOUNDS);
    sparse_tree_free(&t);

    tree_sequence_free(&ts);
    table_collection_free(&tables);
}

static void
test_zero_edges(void)
{
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n";
    const char *edges = "";
    const char *sites =
        "0.1  0\n"
        "0.2  0\n";
    const char *mutations =
        "0    0     1\n"
        "1    1     1\n";
    tree_sequence_t ts, tss;
    sparse_tree_t t;
    const char *haplotypes[] = {"10", "01"};
    char *haplotype;
    hapgen_t hapgen;
    unsigned int j;
    node_id_t samples, node_map;
    const node_id_t z = MSP_NULL_NODE;
    node_id_t parents[] = {
        z, z,
    };
    int ret;

    tree_sequence_from_text(&ts, 2, nodes, edges, NULL, sites, mutations, NULL, NULL);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&ts), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&ts), 2.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_num_sites(&ts), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 1);
    tree_sequence_print_state(&ts, _devnull);

    verify_trees(&ts, 1, parents);

    ret = sparse_tree_alloc(&t, &ts, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = sparse_tree_first(&t);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL(t.left, 0);
    CU_ASSERT_EQUAL(t.right, 2);
    CU_ASSERT_EQUAL(t.parent[0], MSP_NULL_NODE);
    CU_ASSERT_EQUAL(t.parent[1], MSP_NULL_NODE);
    CU_ASSERT_EQUAL(t.left_root, 0);
    CU_ASSERT_EQUAL(t.left_sib[0], MSP_NULL_NODE);
    CU_ASSERT_EQUAL(t.right_sib[0], 1);
    sparse_tree_print_state(&t, _devnull);
    sparse_tree_free(&t);

    ret = sparse_tree_alloc(&t, &ts, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = sparse_tree_last(&t);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL(t.left, 0);
    CU_ASSERT_EQUAL(t.right, 2);
    CU_ASSERT_EQUAL(t.parent[0], MSP_NULL_NODE);
    CU_ASSERT_EQUAL(t.parent[1], MSP_NULL_NODE);
    CU_ASSERT_EQUAL(t.left_root, 0);
    CU_ASSERT_EQUAL(t.left_sib[0], MSP_NULL_NODE);
    CU_ASSERT_EQUAL(t.right_sib[0], 1);
    sparse_tree_print_state(&t, _devnull);
    sparse_tree_free(&t);

    /* We give pointers ot samples and node_map here as they must be non null */
    ret = tree_sequence_simplify(&ts, &samples, 0, 0, &tss, &node_map);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&tss), 0);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&tss), 2.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&tss), 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_sites(&tss), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&tss), 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&tss), 1);
    tree_sequence_print_state(&ts, _devnull);

    ret = hapgen_alloc(&hapgen, &ts);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    hapgen_print_state(&hapgen, _devnull);
    for (j = 0; j < 2; j++) {
        ret = hapgen_get_haplotype(&hapgen, j, &haplotype);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_STRING_EQUAL(haplotype, haplotypes[j]);
    }
    hapgen_free(&hapgen);
    tree_sequence_free(&ts);
    tree_sequence_free(&tss);
}

static void
test_simplest_records(void)
{
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0";
    const char *edges =
        "0  1   2   0,1\n";
    tree_sequence_t ts;

    tree_sequence_from_text(&ts, 0, nodes, edges, NULL, NULL, NULL, NULL, NULL);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&ts), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&ts), 1.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 1);
    tree_sequence_free(&ts);
}

static void
test_simplest_nonbinary_records(void)
{
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0";
    const char *edges =
        "0  1   4   0,1,2,3\n";
    tree_sequence_t ts;

    tree_sequence_from_text(&ts, 0, nodes, edges, NULL, NULL, NULL, NULL, NULL);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&ts), 4);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&ts), 1.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), 5);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 1);
    tree_sequence_free(&ts);
}

static void
test_simplest_unary_records(void)
{
    int ret;
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0\n"
        "0  1   0\n"
        "0  2   0";
    const char *edges =
        "0  1   2   0\n"
        "0  1   3   1\n"
        "0  1   4   2,3\n";
    tree_sequence_t ts, simplified;
    node_id_t sample_ids[] = {0, 1};

    tree_sequence_from_text(&ts, 0, nodes, edges, NULL, NULL, NULL, NULL, NULL);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&ts), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&ts), 1.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), 5);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 1);

    ret = tree_sequence_simplify(&ts, sample_ids, 2, 0, &simplified, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&simplified), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&simplified), 1.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&simplified), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&simplified), 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&simplified), 1);

    tree_sequence_free(&ts);
    tree_sequence_free(&simplified);
}

static void
test_simplest_non_sample_leaf_records(void)
{
    int ret;
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0\n"
        "0  0   0\n"
        "0  0   0";
    const char *edges =
        "0  1   2   0,1,3,4\n";
    const char *sites =
        "0.1  0\n"
        "0.2  0\n"
        "0.3  0\n"
        "0.4  0\n";
    const char *mutations =
        "0    0     1\n"
        "1    1     1\n"
        "2    3     1\n"
        "3    4     1";
    tree_sequence_t ts, simplified;
    node_id_t sample_ids[] = {0, 1};
    hapgen_t hapgen;
    vargen_t vargen;
    char *haplotype;
    variant_t *var;

    tree_sequence_from_text(&ts, 0, nodes, edges, NULL, sites, mutations, NULL, NULL);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&ts), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&ts), 1.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), 5);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 4);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 1);

    ret = hapgen_alloc(&hapgen, &ts);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = hapgen_get_haplotype(&hapgen, 0, &haplotype);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_STRING_EQUAL(haplotype, "1000");
    ret = hapgen_get_haplotype(&hapgen, 1, &haplotype);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_STRING_EQUAL(haplotype, "0100");
    hapgen_free(&hapgen);

    ret = vargen_alloc(&vargen, &ts, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    vargen_print_state(&vargen, _devnull);
    ret = vargen_next(&vargen, &var);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[0], "0", 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[1], "1", 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[0], 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[1], 0);

    ret = vargen_next(&vargen, &var);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[0], "0", 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[1], "1", 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[0], 0);
    CU_ASSERT_EQUAL(var->genotypes.u8[1], 1);

    ret = vargen_next(&vargen, &var);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[0], "0", 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[1], "1", 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[0], 0);
    CU_ASSERT_EQUAL(var->genotypes.u8[1], 0);

    ret = vargen_next(&vargen, &var);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[0], "0", 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[1], "1", 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[0], 0);
    CU_ASSERT_EQUAL(var->genotypes.u8[1], 0);

    ret = vargen_next(&vargen, &var);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    vargen_free(&vargen);

    ret = tree_sequence_simplify(&ts, sample_ids, 2, 0, &simplified, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&simplified), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&simplified), 1.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&simplified), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&simplified), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&simplified), 1);

    tree_sequence_free(&ts);
    tree_sequence_free(&simplified);
}

static void
test_simplest_degenerate_multiple_root_records(void)
{

    int ret;
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0\n"
        "0  1   0\n";
    const char *edges =
        "0  1   2   0\n"
        "0  1   3   1\n";
    tree_sequence_t ts, simplified;
    sparse_tree_t t;
    node_id_t sample_ids[] = {0, 1};

    tree_sequence_from_text(&ts, 0, nodes, edges, NULL, NULL, NULL, NULL, NULL);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&ts), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&ts), 1.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), 4);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 1);

    ret = sparse_tree_alloc(&t, &ts, 0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = sparse_tree_first(&t);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_EQUAL(sparse_tree_get_num_roots(&t), 2);
    CU_ASSERT_EQUAL(t.left_root, 2);
    CU_ASSERT_EQUAL(t.right_sib[2], 3);
    CU_ASSERT_EQUAL(t.right_sib[3], MSP_NULL_NODE);

    ret = tree_sequence_simplify(&ts, sample_ids, 2, 0, &simplified, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&simplified), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&simplified), 1.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&simplified), 2);

    tree_sequence_free(&simplified);
    tree_sequence_free(&ts);
    sparse_tree_free(&t);
}

static void
test_simplest_multiple_root_records(void)
{
    int ret;
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0\n"
        "0  1   0\n";
    const char *edges =
        "0  1   4   0,1\n"
        "0  1   5   2,3\n";
    tree_sequence_t ts, simplified;
    node_id_t sample_ids[] = {0, 1, 2, 3};

    tree_sequence_from_text(&ts, 0, nodes, edges, NULL, NULL, NULL, NULL, NULL);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&ts), 4);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&ts), 1.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), 6);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 1);

    ret = tree_sequence_simplify(&ts, sample_ids, 4, 0, &simplified, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&simplified), 4);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&simplified), 1.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&simplified), 6);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&simplified), 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&simplified), 1);
    tree_sequence_free(&simplified);

    /* Make one tree degenerate */
    ret = tree_sequence_simplify(&ts, sample_ids, 3, 0, &simplified, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&simplified), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&simplified), 1.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&simplified), 4);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&simplified), 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&simplified), 1);
    tree_sequence_free(&simplified);
    tree_sequence_free(&ts);
}

static void
test_simplest_root_mutations(void)
{
    int ret;
    uint32_t j;
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0\n";
    const char *edges =
        "0  1   2   0,1\n";
    const char *sites =
        "0.1 0";
    const char *mutations =
        "0    2     1";
    hapgen_t hapgen;
    char *haplotype;
    int flags = 0;
    node_id_t sample_ids[] = {0, 1};
    tree_sequence_t ts, simplified;

    tree_sequence_from_text(&ts, 0, nodes, edges, NULL, sites, mutations, NULL, NULL);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&ts), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&ts), 1.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_sites(&ts), 1);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 1);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 1);

    ret = hapgen_alloc(&hapgen, &ts);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    hapgen_print_state(&hapgen, _devnull);
    for (j = 0; j < 2; j++) {
        ret = hapgen_get_haplotype(&hapgen, j, &haplotype);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_STRING_EQUAL(haplotype, "1");
    }
    hapgen_free(&hapgen);

    ret = tree_sequence_simplify(&ts, sample_ids, 2, flags, &simplified, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&simplified), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&simplified), 1.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&simplified), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_sites(&simplified), 1);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&simplified), 1);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&simplified), 1);
    tree_sequence_free(&simplified);

    tree_sequence_free(&ts);
}

static void
test_simplest_back_mutations(void)
{
    int ret;
    uint32_t j;
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0\n"
        "0  2   0\n";
    const char *edges =
        "0  1   3   0,1\n"
        "0  1   4   2,3\n";
    const char *sites =
        "0.5 0";
    const char *mutations =
        "0    3     1   -1\n"
        "0    0     0   0";
    hapgen_t hapgen;
    const char *haplotypes[] = {"0", "1", "0"};
    char *haplotype;
    tree_sequence_t ts;
    vargen_t vargen;
    variant_t *var;

    tree_sequence_from_text(&ts, 0, nodes, edges, NULL, sites, mutations, NULL, NULL);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&ts), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&ts), 1.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), 5);
    CU_ASSERT_EQUAL(tree_sequence_get_num_sites(&ts), 1);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 1);

    ret = hapgen_alloc(&hapgen, &ts);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    hapgen_print_state(&hapgen, _devnull);
    for (j = 0; j < 2; j++) {
        ret = hapgen_get_haplotype(&hapgen, j, &haplotype);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_STRING_EQUAL(haplotype, haplotypes[j]);
    }
    hapgen_free(&hapgen);

    ret = vargen_alloc(&vargen, &ts, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    ret = vargen_next(&vargen, &var);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL(var->num_alleles, 2);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[0], "0", 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[1], "1", 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[0], 0);
    CU_ASSERT_EQUAL(var->genotypes.u8[1], 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[2], 0);
    CU_ASSERT_EQUAL(var->site->id, 0);
    CU_ASSERT_EQUAL(var->site->mutations_length, 2);
    vargen_free(&vargen);

    tree_sequence_free(&ts);
}

static void
test_simplest_general_samples(void)
{
    const char *nodes =
        "1  0   0\n"
        "0  1   0\n"
        "1  0   0";
    const char *edges =
        "0  1   1   0,2\n";
    const char *sites =
        "0.5  0\n"
        "0.75 0\n";
    const char *mutations =
        "0    2     1\n"
        "1    0     1";
    const char *haplotypes[] = {"01", "10"};
    char *haplotype;
    unsigned int j;
    node_id_t samples[2] = {0, 2};
    node_id_t *s;
    int ret;

    tree_sequence_t ts, simplified;
    hapgen_t hapgen;

    tree_sequence_from_text(&ts, 0, nodes, edges, NULL, sites, mutations, NULL, NULL);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&ts), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&ts), 1.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_sites(&ts), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 1);

    ret = tree_sequence_get_samples(&ts, &s);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_FATAL(s != NULL);
    CU_ASSERT_EQUAL(s[0], 0);
    CU_ASSERT_EQUAL(s[1], 2);

    ret = hapgen_alloc(&hapgen, &ts);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    hapgen_print_state(&hapgen, _devnull);
    for (j = 0; j < 2; j++) {
        ret = hapgen_get_haplotype(&hapgen, j, &haplotype);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_STRING_EQUAL(haplotype, haplotypes[j]);
    }
    hapgen_free(&hapgen);

    ret = tree_sequence_simplify(&ts, samples, 2, 0, &simplified, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    ret = tree_sequence_get_samples(&simplified, &s);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_FATAL(s != NULL);
    CU_ASSERT_EQUAL(s[0], 0);
    CU_ASSERT_EQUAL(s[1], 1);

    ret = hapgen_alloc(&hapgen, &simplified);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    hapgen_print_state(&hapgen, _devnull);
    for (j = 0; j < 2; j++) {
        ret = hapgen_get_haplotype(&hapgen, j, &haplotype);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_STRING_EQUAL(haplotype, haplotypes[j]);
    }
    hapgen_free(&hapgen);

    tree_sequence_free(&simplified);
    tree_sequence_free(&ts);
}

static void
test_simplest_holey_tree_sequence(void)
{
    const char *nodes_txt =
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0";
    const char *edges_txt =
        "0  1   2   0\n"
        "2  3   2   0\n"
        "0  1   2   1\n"
        "2  3   2   1\n";
    const char *sites_txt =
        "0.5  0\n"
        "1.5  0\n"
        "2.5  0\n";
    const char *mutations_txt =
        "0    0     1\n"
        "1    1     1\n"
        "2    2     1\n";
    const char *haplotypes[] = {"101", "011"};
    char *haplotype;
    unsigned int j;
    int ret;
    tree_sequence_t ts;
    hapgen_t hapgen;

    tree_sequence_from_text(&ts, 0, nodes_txt, edges_txt, NULL, sites_txt,
            mutations_txt, NULL, NULL);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&ts), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&ts), 3.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_sites(&ts), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 3);

    ret = hapgen_alloc(&hapgen, &ts);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    hapgen_print_state(&hapgen, _devnull);
    for (j = 0; j < 2; j++) {
        ret = hapgen_get_haplotype(&hapgen, j, &haplotype);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_STRING_EQUAL(haplotype, haplotypes[j]);
    }

    hapgen_free(&hapgen);
    tree_sequence_free(&ts);
}

static void
test_simplest_holey_tree_sequence_mutation_parents(void)
{
    const char *nodes_txt =
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0";
    const char *edges_txt =
        "0  1   2   0\n"
        "2  3   2   0\n"
        "0  1   2   1\n"
        "2  3   2   1\n";
    const char *sites_txt =
        "0.5  0\n"
        "1.5  0\n"
        "2.5  0\n";
    const char *mutations_txt =
        "0    0     1\n"
        "0    0     1\n"
        "1    1     1\n"
        "1    1     1\n"
        "2    2     1\n"
        "2    2     1\n";
    tree_sequence_t ts;
    table_collection_t tables;
    int ret;

    tree_sequence_from_text(&ts, 0, nodes_txt, edges_txt, NULL, sites_txt,
            mutations_txt, NULL, NULL);
    CU_ASSERT_EQUAL(tree_sequence_get_num_sites(&ts), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 6);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 3);
    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = tree_sequence_dump_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_compute_mutation_parents(&tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(tables.mutations.parent[0], -1);
    CU_ASSERT_EQUAL(tables.mutations.parent[1], 0);
    CU_ASSERT_EQUAL(tables.mutations.parent[2], -1);
    CU_ASSERT_EQUAL(tables.mutations.parent[3], 2);
    CU_ASSERT_EQUAL(tables.mutations.parent[4], -1);
    CU_ASSERT_EQUAL(tables.mutations.parent[5], 4);
    table_collection_free(&tables);
    tree_sequence_free(&ts);
}

static void
test_simplest_initial_gap_tree_sequence(void)
{
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0";
    const char *edges =
        "2  3   2   0,1\n";
    const char *sites =
        "0.5  0\n"
        "1.5  0\n"
        "2.5  0\n";
    const char *mutations =
        "0    0     1\n"
        "1    1     1\n"
        "2    2     1";
    const char *haplotypes[] = {"101", "011"};
    char *haplotype;
    unsigned int j;
    int ret;
    tree_sequence_t ts;
    hapgen_t hapgen;
    const node_id_t z = MSP_NULL_NODE;
    node_id_t parents[] = {
        z, z, z,
        2, 2, z,
    };
    uint32_t num_trees = 2;

    tree_sequence_from_text(&ts, 0, nodes, edges, NULL, sites, mutations, NULL, NULL);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&ts), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&ts), 3.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_sites(&ts), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 2);

    verify_trees(&ts, num_trees, parents);

    ret = hapgen_alloc(&hapgen, &ts);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    hapgen_print_state(&hapgen, _devnull);
    for (j = 0; j < 2; j++) {
        ret = hapgen_get_haplotype(&hapgen, j, &haplotype);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_STRING_EQUAL(haplotype, haplotypes[j]);
    }
    hapgen_free(&hapgen);
    tree_sequence_free(&ts);
}

static void
test_simplest_initial_gap_tree_sequence_mutation_parents(void)
{
    const char *nodes_txt =
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0";
    const char *edges_txt =
        "2  3   2   0,1\n";
    const char *sites_txt =
        "0.5  0\n"
        "1.5  0\n"
        "2.5  0\n";
    const char *mutations_txt =
        "0    0     1\n"
        "0    0     1\n"
        "1    1     1\n"
        "1    1     1\n"
        "2    2     1\n"
        "2    2     1\n";
    tree_sequence_t ts;
    table_collection_t tables;
    int ret;

    tree_sequence_from_text(&ts, 0, nodes_txt, edges_txt, NULL, sites_txt,
            mutations_txt, NULL, NULL);
    CU_ASSERT_EQUAL(tree_sequence_get_num_sites(&ts), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 6);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 2);
    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = tree_sequence_dump_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_compute_mutation_parents(&tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(tables.mutations.parent[0], -1);
    CU_ASSERT_EQUAL(tables.mutations.parent[1], 0);
    CU_ASSERT_EQUAL(tables.mutations.parent[2], -1);
    CU_ASSERT_EQUAL(tables.mutations.parent[3], 2);
    CU_ASSERT_EQUAL(tables.mutations.parent[4], -1);
    CU_ASSERT_EQUAL(tables.mutations.parent[5], 4);
    table_collection_free(&tables);
    tree_sequence_free(&ts);
}

static void
test_simplest_final_gap_tree_sequence(void)
{
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0";
    const char *edges =
        "0  2   2   0,1\n";
    const char *sites =
        "0.5  0\n"
        "1.5  0\n"
        "2.5  0\n";
    const char *mutations =
        "0    0     1\n"
        "1    1     1\n"
        "2    0     1";
    const char *haplotypes[] = {"101", "010"};
    char *haplotype;
    unsigned int j;
    int ret;
    tree_sequence_t ts;
    hapgen_t hapgen;
    const node_id_t z = MSP_NULL_NODE;
    node_id_t parents[] = {
        2, 2, z,
        z, z, z,
    };
    uint32_t num_trees = 2;

    tree_sequence_from_text(&ts, 3, nodes, edges, NULL, sites, mutations, NULL, NULL);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&ts), 2);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&ts), 3.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_sites(&ts), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 2);

    verify_trees(&ts, num_trees, parents);

    ret = hapgen_alloc(&hapgen, &ts);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    hapgen_print_state(&hapgen, _devnull);
    for (j = 0; j < 2; j++) {
        ret = hapgen_get_haplotype(&hapgen, j, &haplotype);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_STRING_EQUAL(haplotype, haplotypes[j]);
    }
    hapgen_free(&hapgen);
    tree_sequence_free(&ts);
}

static void
test_simplest_final_gap_tree_sequence_mutation_parents(void)
{
    const char *nodes_txt =
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0";
    const char *edges_txt =
        "0  2   2   0,1\n";
    const char *sites_txt =
        "0.5  0\n"
        "1.5  0\n"
        "2.5  0\n";
    const char *mutations_txt =
        "0    0     1\n"
        "0    0     1\n"
        "1    1     1\n"
        "1    1     1\n"
        "2    0     1\n"
        "2    0     1\n";
    tree_sequence_t ts;
    table_collection_t tables;
    int ret;

    tree_sequence_from_text(&ts, 3, nodes_txt, edges_txt, NULL, sites_txt,
            mutations_txt, NULL, NULL);
    CU_ASSERT_EQUAL(tree_sequence_get_num_sites(&ts), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 6);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 2);
    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = tree_sequence_dump_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_compute_mutation_parents(&tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(tables.mutations.parent[0], -1);
    CU_ASSERT_EQUAL(tables.mutations.parent[1], 0);
    CU_ASSERT_EQUAL(tables.mutations.parent[2], -1);
    CU_ASSERT_EQUAL(tables.mutations.parent[3], 2);
    CU_ASSERT_EQUAL(tables.mutations.parent[4], -1);
    CU_ASSERT_EQUAL(tables.mutations.parent[5], 4);
    table_collection_free(&tables);
    tree_sequence_free(&ts);
}

static void
test_simplest_bad_records(void)
{
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0\n"
        "1  0   0\n"
        "0  1   0\n";
    const char *edges =
        "0  1   2   0\n"
        "0  1   2   1\n"
        "0  1   4   3\n";
    tree_sequence_t ts;
    table_collection_t tables;
    int ret;

    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    tables.sequence_length = 1.0;
    parse_nodes(nodes, &tables.nodes);
    CU_ASSERT_EQUAL_FATAL(tables.nodes.num_rows, 5);
    parse_edges(edges, &tables.edges);
    CU_ASSERT_EQUAL_FATAL(tables.edges.num_rows, 3);

    /* Make sure we have a good set of records */
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, 0);
    tree_sequence_free(&ts);

    /* NULL for tables should be an error */
    ret = tree_sequence_load_tables(&ts, NULL, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    tree_sequence_free(&ts);

    /* Bad interval */
    tables.edges.right[0] = 0.0;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_EDGE_INTERVAL);
    tree_sequence_free(&ts);
    tables.edges.right[0]= 1.0;

    /* Right coordinate > sequence length. */
    tables.edges.right[0] = 2.0;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_RIGHT_GREATER_SEQ_LENGTH);
    tree_sequence_free(&ts);
    tables.edges.right[0]= 1.0;

    /* Duplicate records */
    tables.edges.child[0] = 1;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_DUPLICATE_EDGES);
    tree_sequence_free(&ts);
    tables.edges.child[0] = 0;

    /* Duplicate records */
    tables.edges.child[0] = 1;
    tables.edges.left[0] = 0.5;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_EDGES_NOT_SORTED_LEFT);
    tree_sequence_free(&ts);
    tables.edges.child[0] = 0;
    tables.edges.left[0] = 0.0;

    /* child node == parent */
    tables.edges.child[1] = 2;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_NODE_TIME_ORDERING);
    tree_sequence_free(&ts);
    tables.edges.child[1] = 1;

    /* Unsorted child nodes */
    tables.edges.child[0] = 1;
    tables.edges.child[1] = 0;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_EDGES_NOT_SORTED_CHILD);
    tree_sequence_free(&ts);
    tables.edges.child[0] = 0;
    tables.edges.child[1] = 1;

    /* discontinuous parent nodes */
    /* Swap rows 1 and 2 */
    tables.edges.parent[1] = 4;
    tables.edges.child[1] = 3;
    tables.edges.parent[2] = 2;
    tables.edges.child[2] = 1;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_EDGES_NONCONTIGUOUS_PARENTS);
    tree_sequence_free(&ts);
    tables.edges.parent[2] = 4;
    tables.edges.child[2] = 3;
    tables.edges.parent[1] = 2;
    tables.edges.child[1] = 1;

    /* Null parent */
    tables.edges.parent[0] = MSP_NULL_NODE;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_NULL_PARENT);
    tree_sequence_free(&ts);
    tables.edges.parent[0] = 2;

    /* parent not in nodes list */
    tables.nodes.num_rows = 2;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_NODE_OUT_OF_BOUNDS);
    tree_sequence_free(&ts);
    tables.nodes.num_rows = 5;

    /* parent negative */
    tables.edges.parent[0] = -2;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_NODE_OUT_OF_BOUNDS);
    tree_sequence_free(&ts);
    tables.edges.parent[0] = 2;

    /* Null child */
    tables.edges.child[0] = MSP_NULL_NODE;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_NULL_CHILD);
    tree_sequence_free(&ts);
    tables.edges.child[0] = 0;

    /* child node reference out of bounds */
    tables.edges.child[0] = 100;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_NODE_OUT_OF_BOUNDS);
    tree_sequence_free(&ts);
    tables.edges.child[0] = 0;

    /* child node reference negative */
    tables.edges.child[0] = -2;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_NODE_OUT_OF_BOUNDS);
    tree_sequence_free(&ts);
    tables.edges.child[0] = 0;

    /* Make sure we've preserved a good tree sequence */
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, 0);
    tree_sequence_free(&ts);

    table_collection_free(&tables);
}

static void
test_simplest_overlapping_parents(void)
{
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0\n";
    const char *edges =
        "0  1   2   0\n"
        "0  1   2   1\n";
    tree_sequence_t ts;
    table_collection_t tables;
    sparse_tree_t tree;
    int ret;

    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    tables.sequence_length = 1;
    parse_nodes(nodes, &tables.nodes);
    CU_ASSERT_EQUAL_FATAL(tables.nodes.num_rows, 3);
    parse_edges(edges, &tables.edges);
    CU_ASSERT_EQUAL_FATAL(tables.edges.num_rows, 2);

    tables.edges.left[0] = 0;
    tables.edges.parent[0] = 2;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = sparse_tree_alloc(&tree, &ts, 0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = sparse_tree_first(&tree);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_EQUAL(tree.parent[0], 2);
    CU_ASSERT_EQUAL(tree.parent[1], 2);
    CU_ASSERT_EQUAL(tree.left_sib[2], MSP_NULL_NODE);
    CU_ASSERT_EQUAL(tree.right_sib[2], MSP_NULL_NODE);
    CU_ASSERT_EQUAL(tree.left_child[2], 0);
    CU_ASSERT_EQUAL(tree.right_child[2], 1);
    CU_ASSERT_EQUAL(tree.left_sib[0], MSP_NULL_NODE);
    CU_ASSERT_EQUAL(tree.right_sib[0], 1);
    CU_ASSERT_EQUAL(tree.left_sib[1], 0);
    CU_ASSERT_EQUAL(tree.right_sib[1], MSP_NULL_NODE);

    sparse_tree_free(&tree);
    tree_sequence_free(&ts);
    table_collection_free(&tables);
}

static void
test_simplest_contradictory_children(void)
{
    const char *nodes =
        "1  0   0\n"
        "1  1   0\n"
        "0  1   0\n";
    const char *edges =
        "0  1   1   0\n"
        "0  1   2   0\n";
    tree_sequence_t ts;
    table_collection_t tables;
    sparse_tree_t tree;
    int ret;

    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    parse_nodes(nodes, &tables.nodes);
    CU_ASSERT_EQUAL_FATAL(tables.nodes.num_rows, 3);
    parse_edges(edges, &tables.edges);
    CU_ASSERT_EQUAL_FATAL(tables.edges.num_rows, 2);
    tables.sequence_length = 1.0;

    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = sparse_tree_alloc(&tree, &ts, 0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = sparse_tree_first(&tree);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_EDGESET_CONTRADICTORY_CHILDREN);

    sparse_tree_free(&tree);
    tree_sequence_free(&ts);
    table_collection_free(&tables);
}

static void
test_simplest_overlapping_edges_simplify(void)
{
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0";
    const char *edges =
        "0  2   3   0\n"
        "1  3   3   1\n"
        "0  3   3   2\n";
    node_id_t samples[] = {0, 1, 2};
    node_table_t node_table;
    edge_table_t edge_table;
    migration_table_t migration_table;
    site_table_t site_table;
    mutation_table_t mutation_table;
    simplifier_t simplifier;
    int ret;

    ret = node_table_alloc(&node_table, 0, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = edge_table_alloc(&edge_table, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = migration_table_alloc(&migration_table, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = site_table_alloc(&site_table, 0, 0, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = mutation_table_alloc(&mutation_table, 0, 0, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    parse_nodes(nodes, &node_table);
    CU_ASSERT_EQUAL_FATAL(node_table.num_rows, 4);
    parse_edges(edges, &edge_table);
    CU_ASSERT_EQUAL_FATAL(edge_table.num_rows, 3);

    ret = simplifier_alloc(&simplifier, 0.0, samples, 3,
            &node_table, &edge_table, &migration_table,
            &site_table, &mutation_table, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    simplifier_print_state(&simplifier, _devnull);
    ret = simplifier_run(&simplifier, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    simplifier_print_state(&simplifier, _devnull);
    ret = simplifier_free(&simplifier);

    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(node_table.num_rows, 4);
    CU_ASSERT_EQUAL(edge_table.num_rows, 3);

    /* Identical to the input.
    0  2   3   0
    1  3   3   1
    0  3   3   2
    */
    CU_ASSERT_EQUAL(edge_table.left[0], 0);
    CU_ASSERT_EQUAL(edge_table.left[1], 1);
    CU_ASSERT_EQUAL(edge_table.left[2], 0);
    CU_ASSERT_EQUAL(edge_table.right[0], 2);
    CU_ASSERT_EQUAL(edge_table.right[1], 3);
    CU_ASSERT_EQUAL(edge_table.right[2], 3);
    CU_ASSERT_EQUAL(edge_table.parent[0], 3);
    CU_ASSERT_EQUAL(edge_table.parent[1], 3);
    CU_ASSERT_EQUAL(edge_table.parent[2], 3);
    CU_ASSERT_EQUAL(edge_table.child[0], 0);
    CU_ASSERT_EQUAL(edge_table.child[1], 1);
    CU_ASSERT_EQUAL(edge_table.child[2], 2);

    node_table_free(&node_table);
    edge_table_free(&edge_table);
    migration_table_free(&migration_table);
    site_table_free(&site_table);
    mutation_table_free(&mutation_table);
}

static void
test_simplest_overlapping_unary_edges_simplify(void)
{
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0";
    const char *edges =
        "0  2   2   0\n"
        "1  3   2   1\n";
    node_id_t samples[] = {0, 1};
    node_table_t node_table;
    edge_table_t edge_table;
    migration_table_t migration_table;
    site_table_t site_table;
    mutation_table_t mutation_table;
    simplifier_t simplifier;
    int ret;

    ret = node_table_alloc(&node_table, 0, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = edge_table_alloc(&edge_table, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = migration_table_alloc(&migration_table, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = site_table_alloc(&site_table, 0, 0, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = mutation_table_alloc(&mutation_table, 0, 0, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    parse_nodes(nodes, &node_table);
    CU_ASSERT_EQUAL_FATAL(node_table.num_rows, 3);
    parse_edges(edges, &edge_table);
    CU_ASSERT_EQUAL_FATAL(edge_table.num_rows, 2);

    ret = simplifier_alloc(&simplifier, 0.0, samples, 2,
            &node_table, &edge_table, &migration_table,
            &site_table, &mutation_table, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    simplifier_print_state(&simplifier, _devnull);
    ret = simplifier_run(&simplifier, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    simplifier_print_state(&simplifier, _devnull);
    ret = simplifier_free(&simplifier);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    CU_ASSERT_EQUAL(node_table.num_rows, 3);
    CU_ASSERT_EQUAL(edge_table.num_rows, 2);

    /* Because we only sample 0 and 1, the flanking unary edges are removed
     1       2       2       0
     1       2       2       1
     */
    CU_ASSERT_EQUAL(edge_table.left[0], 1);
    CU_ASSERT_EQUAL(edge_table.right[0], 2);
    CU_ASSERT_EQUAL(edge_table.parent[0], 2);
    CU_ASSERT_EQUAL(edge_table.child[0], 0);
    CU_ASSERT_EQUAL(edge_table.left[1], 1);
    CU_ASSERT_EQUAL(edge_table.right[1], 2);
    CU_ASSERT_EQUAL(edge_table.parent[1], 2);
    CU_ASSERT_EQUAL(edge_table.child[1], 1);

    node_table_free(&node_table);
    edge_table_free(&edge_table);
    migration_table_free(&migration_table);
    site_table_free(&site_table);
    mutation_table_free(&mutation_table);
}

static void
test_simplest_overlapping_unary_edges_internal_samples_simplify(void)
{
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "1  1   0";
    const char *edges =
        "0  2   2   0\n"
        "1  3   2   1\n";
    node_id_t samples[] = {0, 1, 2};
    node_table_t node_table;
    edge_table_t edge_table;
    migration_table_t migration_table;
    site_table_t site_table;
    mutation_table_t mutation_table;
    simplifier_t simplifier;
    int ret;

    ret = node_table_alloc(&node_table, 1, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = edge_table_alloc(&edge_table, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = migration_table_alloc(&migration_table, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = site_table_alloc(&site_table, 1, 1, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = mutation_table_alloc(&mutation_table, 1, 1, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    parse_nodes(nodes, &node_table);
    CU_ASSERT_EQUAL_FATAL(node_table.num_rows, 3);
    parse_edges(edges, &edge_table);
    CU_ASSERT_EQUAL_FATAL(edge_table.num_rows, 2);

    ret = simplifier_alloc(&simplifier, 0.0, samples, 3,
            &node_table, &edge_table, &migration_table,
            &site_table, &mutation_table, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    simplifier_print_state(&simplifier, _devnull);
    ret = simplifier_run(&simplifier, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    /* simplifier_print_state(&simplifier, stdout); */
    simplifier_print_state(&simplifier, _devnull);
    ret = simplifier_free(&simplifier);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    CU_ASSERT_EQUAL(node_table.num_rows, 3);
    CU_ASSERT_EQUAL(edge_table.num_rows, 2);
    /* Identical to the input.
        0  2   2   0
        1  3   2   1
     */
    CU_ASSERT_EQUAL(edge_table.left[0], 0);
    CU_ASSERT_EQUAL(edge_table.left[1], 1);
    CU_ASSERT_EQUAL(edge_table.right[0], 2);
    CU_ASSERT_EQUAL(edge_table.right[1], 3);
    CU_ASSERT_EQUAL(edge_table.parent[0], 2);
    CU_ASSERT_EQUAL(edge_table.parent[1], 2);
    CU_ASSERT_EQUAL(edge_table.child[0], 0);
    CU_ASSERT_EQUAL(edge_table.child[1], 1);

    node_table_free(&node_table);
    edge_table_free(&edge_table);
    migration_table_free(&migration_table);
    site_table_free(&site_table);
    mutation_table_free(&mutation_table);
}


static void
test_single_tree_good_records(void)
{
    tree_sequence_t ts;

    tree_sequence_from_text(&ts, 0, single_tree_ex_nodes, single_tree_ex_edges,
            NULL, NULL, NULL, NULL, NULL);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&ts), 4);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&ts), 1.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), 7);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 1);
    tree_sequence_free(&ts);
}


static void
test_single_nonbinary_tree_good_records(void)
{
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0\n"
        "0  2   0\n"
        "0  3   0\n";
    const char *edges =
        "0 1 7 0,1,2,3\n"
        "0 1 8 4,5\n"
        "0 1 9 6,7,8";
    tree_sequence_t ts;

    tree_sequence_from_text(&ts, 0, nodes, edges, NULL, NULL, NULL, NULL, NULL);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&ts), 7);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&ts), 1.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), 10);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 1);
    tree_sequence_free(&ts);
}

static void
test_single_tree_bad_records(void)
{
    int ret = 0;
    tree_sequence_t ts;
    table_collection_t tables;

    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    parse_nodes(single_tree_ex_nodes, &tables.nodes);
    CU_ASSERT_EQUAL_FATAL(tables.nodes.num_rows, 7);
    parse_edges(single_tree_ex_edges, &tables.edges);
    CU_ASSERT_EQUAL_FATAL(tables.edges.num_rows, 6);

    /* Not sorted in time order */
    tables.nodes.time[5] = 0.5;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_EDGES_NOT_SORTED_PARENT_TIME);
    tree_sequence_free(&ts);
    tables.nodes.time[5] = 2.0;

    /* Left value greater than sequence right */
    tables.edges.left[2] = 2.0;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_EDGE_INTERVAL);
    tree_sequence_free(&ts);
    tables.edges.left[2] = 0.0;

    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, 0);
    tree_sequence_free(&ts);
    table_collection_free(&tables);
}


static void
test_single_tree_good_mutations(void)
{
    tree_sequence_t ts;
    size_t j;
    size_t num_sites = 3;
    size_t num_mutations = 7;
    site_t other_sites[num_sites];
    mutation_t other_mutations[num_mutations];
    int ret;

    tree_sequence_from_text(&ts, 0, single_tree_ex_nodes, single_tree_ex_edges,
            NULL, single_tree_ex_sites, single_tree_ex_mutations, NULL, NULL);
    CU_ASSERT_EQUAL(tree_sequence_get_num_samples(&ts), 4);
    CU_ASSERT_EQUAL(tree_sequence_get_sequence_length(&ts), 1.0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), 7);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 1);
    CU_ASSERT_EQUAL(tree_sequence_get_num_sites(&ts), num_sites);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), num_mutations);

    for (j = 0; j < num_sites; j++) {
        ret = tree_sequence_get_site(&ts, j, other_sites + j);
        CU_ASSERT_EQUAL(ret, 0);
    }
    for (j = 0; j < num_mutations; j++) {
        ret = tree_sequence_get_mutation(&ts, j, other_mutations + j);
        CU_ASSERT_EQUAL(ret, 0);
    }
    CU_ASSERT_EQUAL(other_sites[0].position, 0.1);
    CU_ASSERT_NSTRING_EQUAL(other_sites[0].ancestral_state, "0", 1);
    CU_ASSERT_EQUAL(other_sites[1].position, 0.2);
    CU_ASSERT_NSTRING_EQUAL(other_sites[1].ancestral_state, "0", 1);
    CU_ASSERT_EQUAL(other_sites[2].position, 0.3);
    CU_ASSERT_NSTRING_EQUAL(other_sites[2].ancestral_state, "0", 1);

    CU_ASSERT_EQUAL(other_mutations[0].index, 0);
    CU_ASSERT_EQUAL(other_mutations[0].node, 2);
    CU_ASSERT_NSTRING_EQUAL(other_mutations[0].derived_state, "1", 1);
    CU_ASSERT_EQUAL(other_mutations[1].index, 1);
    CU_ASSERT_EQUAL(other_mutations[1].node, 4);
    CU_ASSERT_NSTRING_EQUAL(other_mutations[1].derived_state, "1", 1);
    CU_ASSERT_EQUAL(other_mutations[2].index, 2);
    CU_ASSERT_EQUAL(other_mutations[2].node, 0);
    CU_ASSERT_NSTRING_EQUAL(other_mutations[2].derived_state, "0", 1);
    CU_ASSERT_EQUAL(other_mutations[3].index, 3);
    CU_ASSERT_EQUAL(other_mutations[3].node, 0);
    CU_ASSERT_NSTRING_EQUAL(other_mutations[3].derived_state, "1", 1);
    CU_ASSERT_EQUAL(other_mutations[4].index, 4);
    CU_ASSERT_EQUAL(other_mutations[4].node, 1);
    CU_ASSERT_NSTRING_EQUAL(other_mutations[4].derived_state, "1", 1);
    CU_ASSERT_EQUAL(other_mutations[5].index, 5);
    CU_ASSERT_EQUAL(other_mutations[5].node, 2);
    CU_ASSERT_NSTRING_EQUAL(other_mutations[5].derived_state, "1", 1);
    CU_ASSERT_EQUAL(other_mutations[6].index, 6);
    CU_ASSERT_EQUAL(other_mutations[6].node, 3);
    CU_ASSERT_NSTRING_EQUAL(other_mutations[6].derived_state, "1", 1);

    tree_sequence_free(&ts);
}

static void
test_single_tree_bad_mutations(void)
{
    int ret = 0;
    const char *sites =
        "0       0\n"
        "0.1     0\n"
        "0.2     0\n";
    const char *mutations =
        "0   0  1  -1\n"
        "1   1  1  -1\n"
        "2   4  1  -1\n"
        "2   1  0  2\n"
        "2   1  1  3\n"
        "2   2  1  -1\n";
    tree_sequence_t ts;
    table_collection_t tables;

    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    parse_nodes(single_tree_ex_nodes, &tables.nodes);
    CU_ASSERT_EQUAL_FATAL(tables.nodes.num_rows, 7);
    parse_edges(single_tree_ex_edges, &tables.edges);
    CU_ASSERT_EQUAL_FATAL(tables.edges.num_rows, 6);
    parse_sites(sites, &tables.sites);
    parse_mutations(mutations, &tables.mutations);
    CU_ASSERT_EQUAL_FATAL(tables.sites.num_rows, 3);
    CU_ASSERT_EQUAL_FATAL(tables.mutations.num_rows, 6);
    tables.sequence_length = 1.0;

    /* Check to make sure we have legal mutations */
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_sites(&ts), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 6);
    tree_sequence_free(&ts);

    /* negative coordinate */
    tables.sites.position[0] = -1.0;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_SITE_POSITION);
    tree_sequence_free(&ts);
    tables.sites.position[0] = 0.0;

    /* coordinate == sequence length */
    tables.sites.position[2] = 1.0;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_SITE_POSITION);
    tree_sequence_free(&ts);
    tables.sites.position[2] = 0.2;

    /* coordinate > sequence length */
    tables.sites.position[2] = 1.1;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_SITE_POSITION);
    tree_sequence_free(&ts);
    tables.sites.position[2] = 0.2;

    /* Unsorted positions */
    tables.sites.position[0] = 0.3;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_UNSORTED_SITES);
    tree_sequence_free(&ts);
    tables.sites.position[0] = 0.0;

    /* site < 0 */
    tables.mutations.site[0] = -2;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_SITE_OUT_OF_BOUNDS);
    tree_sequence_free(&ts);
    tables.mutations.site[0] = 0;

    /* site == num_sites */
    tables.mutations.site[0] = 3;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_SITE_OUT_OF_BOUNDS);
    tree_sequence_free(&ts);
    tables.mutations.site[0] = 0;

    /* node = NULL */
    tables.mutations.node[0] = MSP_NULL_NODE;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_NODE_OUT_OF_BOUNDS);
    tree_sequence_free(&ts);
    tables.mutations.node[0] = 0;

    /* node >= num_nodes */
    tables.mutations.node[0] = 7;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_NODE_OUT_OF_BOUNDS);
    tree_sequence_free(&ts);
    tables.mutations.node[0] = 0;

    /* parent < -1 */
    tables.mutations.parent[0] = -2;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_MUTATION_OUT_OF_BOUNDS);
    tree_sequence_free(&ts);
    tables.mutations.parent[0] = MSP_NULL_MUTATION;

    /* parent >= num_mutations */
    tables.mutations.parent[0] = 7;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_MUTATION_OUT_OF_BOUNDS);
    tree_sequence_free(&ts);
    tables.mutations.parent[0] = MSP_NULL_MUTATION;

    /* parent on a different site */
    tables.mutations.parent[1] = 0;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_MUTATION_PARENT_DIFFERENT_SITE);
    tree_sequence_free(&ts);
    tables.mutations.parent[1] = MSP_NULL_MUTATION;

    /* parent is the same mutation */
    tables.mutations.parent[0] = 0;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_MUTATION_PARENT_EQUAL);
    tree_sequence_free(&ts);
    tables.mutations.parent[0] = MSP_NULL_MUTATION;

    /* parent_id > mutation id */
    tables.mutations.parent[2] = 3;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_MUTATION_PARENT_AFTER_CHILD);
    tree_sequence_free(&ts);
    tables.mutations.parent[2] = MSP_NULL_MUTATION;

    /* Check to make sure we've maintained legal mutations */
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_sites(&ts), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 6);
    tree_sequence_free(&ts);

    table_collection_free(&tables);
}

static void
test_single_tree_iter(void)
{
    int ret;
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0\n"
        "0  2   0\n"
        "0  3   0\n";
    const char *edges =
        "0  6   4   0,1\n"
        "0  6   5   2,3\n"
        "0  6   6   4,5\n";
    node_id_t parents[] = {4, 4, 5, 5, 6, 6, MSP_NULL_NODE};
    tree_sequence_t ts;
    sparse_tree_t tree;
    node_id_t u, v, w;
    size_t num_samples;
    uint32_t num_nodes = 7;

    tree_sequence_from_text(&ts, 0, nodes, edges, NULL, NULL, NULL, NULL, NULL);
    ret = sparse_tree_alloc(&tree, &ts, 0);
    CU_ASSERT_EQUAL(ret, 0);

    ret = sparse_tree_first(&tree);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), num_nodes);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 1);
    sparse_tree_print_state(&tree, _devnull);

    for (u = 0; u < num_nodes; u++) {
        ret = sparse_tree_get_parent(&tree, u, &v);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_EQUAL(v, parents[u]);
    }
    ret = sparse_tree_get_num_samples(&tree, 0, &num_samples);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(num_samples, 1);
    ret = sparse_tree_get_num_samples(&tree, 4, &num_samples);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(num_samples, 2);
    ret = sparse_tree_get_num_samples(&tree, 6, &num_samples);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(num_samples, 4);
    ret = sparse_tree_get_mrca(&tree, 0, 1, &w);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(w, 4);
    ret = sparse_tree_get_mrca(&tree, 0, 2, &w);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(w, 6);

    ret = sparse_tree_next(&tree);
    CU_ASSERT_EQUAL(ret, 0);

    sparse_tree_free(&tree);
    tree_sequence_free(&ts);
}

static void
test_single_nonbinary_tree_iter(void)
{
    int ret;
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0\n"
        "0  2   0\n"
        "0  3   0\n";
    const char *edges =
        "0  1   7   0,1,2,3\n"
        "0  1   8   4,5\n"
        "0  1   9   6,7,8\n";
    node_id_t parents[] = {7, 7, 7, 7, 8, 8, 9, 9, 9, MSP_NULL_NODE};
    tree_sequence_t ts;
    sparse_tree_t tree;
    node_id_t u, v, w;
    size_t num_samples;
    size_t num_nodes = 10;
    size_t total_samples = 7;

    tree_sequence_from_text(&ts, 0, nodes, edges, NULL, NULL, NULL, NULL, NULL);
    ret = sparse_tree_alloc(&tree, &ts, 0);
    CU_ASSERT_EQUAL(ret, 0);

    ret = sparse_tree_first(&tree);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), num_nodes);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 1);
    sparse_tree_print_state(&tree, _devnull);

    for (u = 0; u < num_nodes; u++) {
        ret = sparse_tree_get_parent(&tree, u, &v);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_EQUAL(v, parents[u]);
    }
    for (u = 0; u < total_samples; u++) {
        ret = sparse_tree_get_num_samples(&tree, u, &num_samples);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_EQUAL(num_samples, 1);
        CU_ASSERT_EQUAL(tree.left_child[u], MSP_NULL_NODE);
    }

    u = 7;
    ret = sparse_tree_get_num_samples(&tree, u, &num_samples);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(num_samples, 4);
    CU_ASSERT_EQUAL(tree.right_child[u], 3);
    CU_ASSERT_EQUAL(tree.left_sib[3], 2);
    CU_ASSERT_EQUAL(tree.left_sib[2], 1);
    CU_ASSERT_EQUAL(tree.left_sib[1], 0);
    CU_ASSERT_EQUAL(tree.left_sib[0], MSP_NULL_NODE);

    u = 8;
    ret = sparse_tree_get_num_samples(&tree, u, &num_samples);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(num_samples, 2);
    CU_ASSERT_EQUAL(tree.right_child[u], 5);
    CU_ASSERT_EQUAL(tree.left_sib[5], 4);
    CU_ASSERT_EQUAL(tree.left_sib[4], MSP_NULL_NODE);

    u = 9;
    ret = sparse_tree_get_num_samples(&tree, u, &num_samples);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(num_samples, 7);
    CU_ASSERT_EQUAL(tree.right_child[u], 8);
    CU_ASSERT_EQUAL(tree.left_sib[8], 7);
    CU_ASSERT_EQUAL(tree.left_sib[7], 6);
    CU_ASSERT_EQUAL(tree.left_sib[6], MSP_NULL_NODE);

    CU_ASSERT_EQUAL(sparse_tree_get_num_roots(&tree), 1);
    CU_ASSERT_EQUAL(tree.left_root, 9);

    ret = sparse_tree_get_mrca(&tree, 0, 1, &w);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(w, 7);
    ret = sparse_tree_get_mrca(&tree, 0, 4, &w);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(w, 9);

    ret = sparse_tree_next(&tree);
    CU_ASSERT_EQUAL(ret, 0);

    sparse_tree_free(&tree);
    tree_sequence_free(&ts);
}

static void
test_single_tree_general_samples_iter(void)
{
    int ret;
    const char *nodes =
        "0  3   0\n"
        "0  2   0\n"
        "0  1   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n";
    const char *edges =
        "0  6   2   3,4\n"
        "0  6   1   5,6\n"
        "0  6   0   1,2\n";
    node_id_t parents[] = {MSP_NULL_NODE, 0, 0, 2, 2, 1, 1};
    node_id_t *samples;
    tree_sequence_t ts;
    sparse_tree_t tree;
    node_id_t u, v, w;
    size_t num_samples;
    uint32_t num_nodes = 7;

    tree_sequence_from_text(&ts, 0, nodes, edges, NULL, NULL, NULL, NULL, NULL);
    ret = tree_sequence_get_samples(&ts, &samples);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(samples[0], 3);
    CU_ASSERT_EQUAL(samples[1], 4);
    CU_ASSERT_EQUAL(samples[2], 5);
    CU_ASSERT_EQUAL(samples[3], 6);

    ret = sparse_tree_alloc(&tree, &ts, 0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = sparse_tree_first(&tree);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), num_nodes);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 1);
    sparse_tree_print_state(&tree, _devnull);

    for (u = 0; u < num_nodes; u++) {
        ret = sparse_tree_get_parent(&tree, u, &v);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_EQUAL(v, parents[u]);
    }
    ret = sparse_tree_get_num_samples(&tree, 3, &num_samples);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(num_samples, 1);
    ret = sparse_tree_get_num_samples(&tree, 2, &num_samples);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(num_samples, 2);
    ret = sparse_tree_get_num_samples(&tree, 0, &num_samples);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(num_samples, 4);
    ret = sparse_tree_get_mrca(&tree, 3, 4, &w);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(w, 2);
    ret = sparse_tree_get_mrca(&tree, 3, 6, &w);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(w, 0);

    ret = sparse_tree_next(&tree);
    CU_ASSERT_EQUAL(ret, 0);

    sparse_tree_free(&tree);
    tree_sequence_free(&ts);
}

static void
test_single_tree_iter_times(void)
{
    int ret = 0;
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "1  2   0\n"
        "1  3   0\n"
        "0  1   0\n"
        "0  4   0\n"
        "0  5   0\n";
    const char *edges =
        "0  6   4   0,1\n"
        "0  6   5   2,3\n"
        "0  6   6   4,5\n";
    node_id_t parents[] = {4, 4, 5, 5, 6, 6, MSP_NULL_NODE};
    double times[] = {0.0, 0.0, 2.0, 3.0, 1.0, 4.0, 5.0};
    double t;
    tree_sequence_t ts;
    sparse_tree_t tree;
    node_id_t u, v;
    uint32_t num_nodes = 7;

    tree_sequence_from_text(&ts, 0, nodes, edges, NULL, NULL, NULL, NULL, NULL);
    ret = sparse_tree_alloc(&tree, &ts, 0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = sparse_tree_first(&tree);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_EQUAL(tree_sequence_get_num_nodes(&ts), num_nodes);
    CU_ASSERT_EQUAL(tree_sequence_get_num_trees(&ts), 1);
    sparse_tree_print_state(&tree, _devnull);

    for (u = 0; u < num_nodes; u++) {
        ret = sparse_tree_get_parent(&tree, u, &v);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_EQUAL(v, parents[u]);
        ret = sparse_tree_get_time(&tree, u, &t);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_EQUAL(t, times[u]);
    }
    ret = sparse_tree_next(&tree);
    CU_ASSERT_EQUAL(ret, 0);

    sparse_tree_free(&tree);
    tree_sequence_free(&ts);
}

static void
test_single_tree_hapgen_char_alphabet(void)
{
    int ret = 0;
    const char *sites =
        "0.0    A\n"
        "0.1    A\n"
        "0.2    C\n"
        "0.4    A\n";
    const char *mutations =
        "0    0     T\n"
        "1    1     T\n"
        "2    0     G\n"
        "2    1     A\n"
        "2    2     T\n"  // A bunch of different sample mutations
        "3    4     T\n"
        "3    0     A\n"; // A back mutation from T -> A
    uint32_t num_samples = 4;
    tree_sequence_t ts;
    char *haplotype;
    size_t j;
    hapgen_t hapgen;

    tree_sequence_from_text(&ts, 0, single_tree_ex_nodes, single_tree_ex_edges, NULL,
            NULL, NULL, NULL, NULL);
    ret = hapgen_alloc(&hapgen, &ts);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    hapgen_print_state(&hapgen, _devnull);
    for (j = 0; j < num_samples; j++) {
        ret = hapgen_get_haplotype(&hapgen, j, &haplotype);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_STRING_EQUAL(haplotype, "");
    }
    ret = hapgen_free(&hapgen);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    tree_sequence_free(&ts);

    tree_sequence_from_text(&ts, 0, single_tree_ex_nodes, single_tree_ex_edges, NULL,
            sites, mutations, NULL, NULL);
    ret = hapgen_alloc(&hapgen, &ts);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    hapgen_print_state(&hapgen, _devnull);

    ret = hapgen_get_haplotype(&hapgen, 0, &haplotype);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_STRING_EQUAL(haplotype, "TAGA");
    ret = hapgen_get_haplotype(&hapgen, 1, &haplotype);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_STRING_EQUAL(haplotype, "ATAT");
    ret = hapgen_get_haplotype(&hapgen, 2, &haplotype);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_STRING_EQUAL(haplotype, "AATA");
    ret = hapgen_get_haplotype(&hapgen, 3, &haplotype);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_STRING_EQUAL(haplotype, "AACA");

    ret = hapgen_free(&hapgen);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    tree_sequence_free(&ts);
}

static void
test_single_tree_vargen_char_alphabet(void)
{
    int ret = 0;
    const char *sites =
        "0.0    A\n"
        "0.1    A\n"
        "0.2    C\n"
        "0.4    A\n";
    const char *mutations =
        "0    0     T   -1\n"
        "1    1     TTTAAGGG   -1\n"
        "2    0     G   -1\n"
        "2    1     AT  -1\n"
        "2    2     T   -1\n"  // A bunch of different sample mutations
        "3    4     T   -1\n"
        "3    0     A   5\n"; // A back mutation from T -> A
    tree_sequence_t ts;
    vargen_t vargen;
    variant_t *var;

    tree_sequence_from_text(&ts, 0, single_tree_ex_nodes, single_tree_ex_edges, NULL,
            sites, mutations, NULL, NULL);
    ret = vargen_alloc(&vargen, &ts, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    ret = vargen_next(&vargen, &var);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL(var->site->position, 0.0);
    CU_ASSERT_EQUAL(var->num_alleles, 2);
    CU_ASSERT_EQUAL(var->allele_lengths[0], 1);
    CU_ASSERT_EQUAL(var->allele_lengths[1], 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[0], "A", 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[1], "T", 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[0], 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[1], 0);
    CU_ASSERT_EQUAL(var->genotypes.u8[2], 0);
    CU_ASSERT_EQUAL(var->genotypes.u8[3], 0);

    ret = vargen_next(&vargen, &var);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL(var->site->position, 0.1);
    CU_ASSERT_EQUAL(var->num_alleles, 2);
    CU_ASSERT_EQUAL(var->allele_lengths[0], 1);
    CU_ASSERT_EQUAL(var->allele_lengths[1], 8);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[0], "A", 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[1], "TTTAAGGG", 8);
    CU_ASSERT_EQUAL(var->genotypes.u8[0], 0);
    CU_ASSERT_EQUAL(var->genotypes.u8[1], 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[2], 0);
    CU_ASSERT_EQUAL(var->genotypes.u8[3], 0);

    ret = vargen_next(&vargen, &var);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL(var->site->position, 0.2);
    CU_ASSERT_EQUAL(var->num_alleles, 4);
    CU_ASSERT_EQUAL(var->allele_lengths[0], 1);
    CU_ASSERT_EQUAL(var->allele_lengths[1], 1);
    CU_ASSERT_EQUAL(var->allele_lengths[2], 2);
    CU_ASSERT_EQUAL(var->allele_lengths[3], 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[0], "C", 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[1], "G", 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[2], "AT", 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[3], "T", 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[0], 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[1], 2);
    CU_ASSERT_EQUAL(var->genotypes.u8[2], 3);
    CU_ASSERT_EQUAL(var->genotypes.u8[3], 0);

    ret = vargen_next(&vargen, &var);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL(var->site->position, 0.4);
    CU_ASSERT_EQUAL(var->num_alleles, 2);
    CU_ASSERT_EQUAL(var->allele_lengths[0], 1);
    CU_ASSERT_EQUAL(var->allele_lengths[1], 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[0], "A", 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[1], "T", 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[0], 0);
    CU_ASSERT_EQUAL(var->genotypes.u8[1], 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[2], 0);
    CU_ASSERT_EQUAL(var->genotypes.u8[3], 0);

    ret = vargen_next(&vargen, &var);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    vargen_free(&vargen);
    tree_sequence_free(&ts);
}

static void
test_single_tree_hapgen_binary_alphabet(void)
{
    int ret = 0;
    uint32_t num_samples = 4;
    tree_sequence_t ts;
    char *haplotype;
    size_t j;
    hapgen_t hapgen;

    tree_sequence_from_text(&ts, 0, single_tree_ex_nodes, single_tree_ex_edges, NULL,
            NULL, NULL, NULL, NULL);
    ret = hapgen_alloc(&hapgen, &ts);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    hapgen_print_state(&hapgen, _devnull);
    for (j = 0; j < num_samples; j++) {
        ret = hapgen_get_haplotype(&hapgen, j, &haplotype);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_STRING_EQUAL(haplotype, "");
    }
    ret = hapgen_free(&hapgen);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    tree_sequence_free(&ts);

    tree_sequence_from_text(&ts, 0, single_tree_ex_nodes, single_tree_ex_edges, NULL,
            single_tree_ex_sites, single_tree_ex_mutations, NULL, NULL);
    ret = hapgen_alloc(&hapgen, &ts);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    hapgen_print_state(&hapgen, _devnull);

    ret = hapgen_get_haplotype(&hapgen, 0, &haplotype);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_STRING_EQUAL(haplotype, "001");
    ret = hapgen_get_haplotype(&hapgen, 1, &haplotype);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_STRING_EQUAL(haplotype, "011");
    ret = hapgen_get_haplotype(&hapgen, 2, &haplotype);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_STRING_EQUAL(haplotype, "101");
    ret = hapgen_get_haplotype(&hapgen, 3, &haplotype);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_STRING_EQUAL(haplotype, "001");

    ret = hapgen_free(&hapgen);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    tree_sequence_free(&ts);
}

static void
test_single_tree_vargen_binary_alphabet(void)
{
    int ret = 0;
    tree_sequence_t ts;
    vargen_t vargen;
    variant_t *var;

    tree_sequence_from_text(&ts, 0, single_tree_ex_nodes, single_tree_ex_edges, NULL,
            single_tree_ex_sites, single_tree_ex_mutations, NULL, NULL);
    ret = vargen_alloc(&vargen, &ts, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    vargen_print_state(&vargen, _devnull);

    ret = vargen_next(&vargen, &var);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[0], 0);
    CU_ASSERT_EQUAL(var->genotypes.u8[1], 0);
    CU_ASSERT_EQUAL(var->genotypes.u8[2], 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[3], 0);
    CU_ASSERT_EQUAL(var->num_alleles, 2);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[0], "0", 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[1], "1", 1);
    CU_ASSERT_EQUAL(var->site->id, 0);
    CU_ASSERT_EQUAL(var->site->mutations_length, 1);

    ret = vargen_next(&vargen, &var);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[0], 0);
    CU_ASSERT_EQUAL(var->genotypes.u8[1], 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[2], 0);
    CU_ASSERT_EQUAL(var->genotypes.u8[3], 0);
    CU_ASSERT_EQUAL(var->num_alleles, 2);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[0], "0", 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[1], "1", 1);
    CU_ASSERT_EQUAL(var->site->id, 1);
    CU_ASSERT_EQUAL(var->site->mutations_length, 2);

    ret = vargen_next(&vargen, &var);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[0], 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[1], 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[2], 1);
    CU_ASSERT_EQUAL(var->genotypes.u8[3], 1);
    CU_ASSERT_EQUAL(var->num_alleles, 2);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[0], "0", 1);
    CU_ASSERT_NSTRING_EQUAL(var->alleles[1], "1", 1);
    CU_ASSERT_EQUAL(var->site->id, 2);
    CU_ASSERT_EQUAL(var->site->mutations_length, 4);

    ret = vargen_next(&vargen, &var);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    ret = vargen_free(&vargen);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    tree_sequence_free(&ts);
}

static void
test_single_tree_vargen_many_alleles(void)
{
    int ret = 0;
    tree_sequence_t ts;
    vargen_t vargen;
    variant_t *var;
    int num_alleles = 257;
    int j, k, l, flags;
    char alleles[num_alleles];
    table_collection_t tables;

    tree_sequence_from_text(&ts, 0, single_tree_ex_nodes, single_tree_ex_edges, NULL,
            NULL, NULL, NULL, NULL);
    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_FATAL(ret == 0);
    ret = tree_sequence_dump_tables(&ts, &tables, 0);
    CU_ASSERT_FATAL(ret == 0);
    tree_sequence_free(&ts);
    memset(alleles, 'X', num_alleles);
    ret = site_table_add_row(&tables.sites, 0, "Y", 1, NULL, 0);
    CU_ASSERT_FATAL(ret >= 0);

    /* Add j mutations over a single node. */
    for (j = 0; j < num_alleles; j++) {
        /* When j = 0 we get a parent of -1, which is the NULL_NODE */
        ret = mutation_table_add_row(&tables.mutations, 0, 0, j - 1, alleles, j, NULL, 0);
        CU_ASSERT_FATAL(ret >= 0);
        ret = tree_sequence_load_tables(&ts, &tables, 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        for (l = 0; l < 2; l++) {
            flags = 0;
            if (l == 1) {
                flags = MSP_16_BIT_GENOTYPES;
            }
            ret = vargen_alloc(&vargen, &ts, flags);
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            vargen_print_state(&vargen, _devnull);
            ret = vargen_next(&vargen, &var);
            /* We have j + 2 alleles. So, if j >= 254, we should fail with 8bit
             * genotypes */
            if (l == 0 && j >= 254) {
                CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_TOO_MANY_ALLELES);
            } else {
                CU_ASSERT_EQUAL_FATAL(ret, 1);
                CU_ASSERT_NSTRING_EQUAL(var->alleles[0], "Y", 1);
                for (k = 1; k < var->num_alleles; k++) {
                    CU_ASSERT_EQUAL(k - 1, var->allele_lengths[k]);
                    CU_ASSERT_NSTRING_EQUAL(var->alleles[k], alleles, var->allele_lengths[k]);
                }
                CU_ASSERT_EQUAL(var->num_alleles, j + 2);
            }
            ret = vargen_free(&vargen);
            CU_ASSERT_EQUAL_FATAL(ret, 0);
        }
        tree_sequence_free(&ts);
    }
    table_collection_free(&tables);
}

static void
test_single_tree_simplify(void)
{
    tree_sequence_t ts;
    table_collection_t tables;
    int ret;
    simplifier_t simplifier;
    node_id_t samples[] = {0, 1};

    tree_sequence_from_text(&ts, 0, single_tree_ex_nodes, single_tree_ex_edges, NULL,
            single_tree_ex_sites, single_tree_ex_mutations, NULL, NULL);
    verify_simplify(&ts);
    ret = tree_sequence_dump_tables(&ts, &tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    ret = simplifier_alloc(&simplifier, 0.0, samples, 2,
            &tables.nodes, &tables.edges, &tables.migrations, &tables.sites,
            &tables.mutations, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    simplifier_print_state(&simplifier, _devnull);
    ret = simplifier_run(&simplifier, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    simplifier_print_state(&simplifier, _devnull);
    ret = simplifier_free(&simplifier);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(tables.nodes.num_rows, 3);
    CU_ASSERT_EQUAL(tables.edges.num_rows, 2);

    /* Make sure we detect unsorted edges */
    ret = tree_sequence_dump_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    unsort_edges(&tables.edges, 0);
    ret = simplifier_alloc(&simplifier, 0.0, samples, 2,
            &tables.nodes, &tables.edges, &tables.migrations, &tables.sites,
            &tables.mutations, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_EDGES_NOT_SORTED_PARENT_TIME);
    ret = simplifier_free(&simplifier);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* detect bad parents */
    ret = tree_sequence_dump_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    tables.edges.parent[0] = -1;
    ret = simplifier_alloc(&simplifier, 0.0, samples, 2,
            &tables.nodes, &tables.edges, &tables.migrations, &tables.sites,
            &tables.mutations, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_NODE_OUT_OF_BOUNDS);
    ret = simplifier_free(&simplifier);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* detect bad children */
    ret = tree_sequence_dump_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    tables.edges.child[0] = -1;
    ret = simplifier_alloc(&simplifier, 0.0, samples, 2,
            &tables.nodes, &tables.edges, &tables.migrations, &tables.sites,
            &tables.mutations, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_NODE_OUT_OF_BOUNDS);
    ret = simplifier_free(&simplifier);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* detect loops */
    ret = tree_sequence_dump_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    tables.edges.child[0] = tables.edges.parent[0];
    ret = simplifier_alloc(&simplifier, 0.0, samples, 2,
            &tables.nodes, &tables.edges, &tables.migrations, &tables.sites,
            &tables.mutations, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_NODE_TIME_ORDERING);
    ret = simplifier_free(&simplifier);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* detect bad tables.sites */
    ret = tree_sequence_dump_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_FATAL(tables.mutations.num_rows > 0 && tables.sites.num_rows > 0);
    tables.mutations.site[0] = -1;
    ret = simplifier_alloc(&simplifier, 0.0, samples, 2,
            &tables.nodes, &tables.edges, &tables.migrations, &tables.sites,
            &tables.mutations, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_SITE_OUT_OF_BOUNDS);
    ret = simplifier_free(&simplifier);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* detect bad mutation tables.nodes */
    ret = tree_sequence_dump_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_FATAL(tables.mutations.num_rows > 0 && tables.sites.num_rows > 0);
    tables.mutations.node[0] = -1;
    ret = simplifier_alloc(&simplifier, 0.0, samples, 2,
            &tables.nodes, &tables.edges, &tables.migrations, &tables.sites,
            &tables.mutations, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_NODE_OUT_OF_BOUNDS);
    ret = simplifier_free(&simplifier);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Test the interface for NULL inputs */
    ret = tree_sequence_dump_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = simplifier_alloc(&simplifier, 0.0, NULL, 2,
            &tables.nodes, &tables.edges, &tables.migrations, &tables.sites,
            &tables.mutations, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = simplifier_alloc(&simplifier, 0.0, samples, 2,
            NULL, &tables.edges, &tables.migrations, &tables.sites, &tables.mutations, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = simplifier_alloc(&simplifier, 0.0, samples, 2,
            &tables.nodes, NULL, &tables.migrations, &tables.sites, &tables.mutations, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = simplifier_alloc(&simplifier, 0.0, samples, 2,
            &tables.nodes, &tables.edges, &tables.migrations, NULL, &tables.mutations, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = simplifier_alloc(&simplifier, 0.0, samples, 2,
            &tables.nodes, &tables.edges, &tables.migrations, &tables.sites, NULL, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = simplifier_alloc(&simplifier, 0.0, samples, 2,
            &tables.nodes, &tables.edges, NULL, &tables.sites, &tables.mutations, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = simplifier_free(&simplifier);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    tree_sequence_free(&ts);
    table_collection_free(&tables);
}

static void
test_single_tree_compute_mutation_parents(void)
{
    int ret = 0;
    const char *sites =
        "0       0\n"
        "0.1     0\n"
        "0.2     0\n";
    const char *mutations =
        "0   0  1  -1\n"
        "1   1  1  -1\n"
        "2   4  1  -1\n"
        "2   1  0  2\n"
        "2   1  1  3\n"
        "2   2  1  -1\n";
    tree_sequence_t ts;
    table_collection_t tables;

    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    parse_nodes(single_tree_ex_nodes, &tables.nodes);
    CU_ASSERT_EQUAL_FATAL(tables.nodes.num_rows, 7);
    parse_edges(single_tree_ex_edges, &tables.edges);
    CU_ASSERT_EQUAL_FATAL(tables.edges.num_rows, 6);
    parse_sites(sites, &tables.sites);
    parse_mutations(mutations, &tables.mutations);
    CU_ASSERT_EQUAL_FATAL(tables.sites.num_rows, 3);
    CU_ASSERT_EQUAL_FATAL(tables.mutations.num_rows, 6);
    tables.sequence_length = 1.0;

    /* Check to make sure we have legal mutations */
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_sites(&ts), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 6);

    /* Compute the mutation parents */
    verify_compute_mutation_parents(&ts);
    tree_sequence_free(&ts);

    /* Bad site reference */
    tables.mutations.site[0] = -1;
    ret = table_collection_compute_mutation_parents(&tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_SITE_OUT_OF_BOUNDS);
    tables.mutations.site[0] = 0;

    /* Bad site reference */
    tables.mutations.site[0] = -1;
    ret = table_collection_compute_mutation_parents(&tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_SITE_OUT_OF_BOUNDS);
    tables.mutations.site[0] = 0;

    /* mutation sites out of order */
    tables.mutations.site[0] = 2;
    ret = table_collection_compute_mutation_parents(&tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_UNSORTED_MUTATIONS);
    tables.mutations.site[0] = 0;

    /* sites out of order */
    tables.sites.position[0] = 0.11;
    ret = table_collection_compute_mutation_parents(&tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_UNSORTED_SITES);
    tables.sites.position[0] = 0;

    /* Bad node reference */
    tables.mutations.node[0] = -1;
    ret = table_collection_compute_mutation_parents(&tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_NODE_OUT_OF_BOUNDS);
    tables.mutations.node[0] = 0;

    /* Bad node reference */
    tables.mutations.node[0] = tables.nodes.num_rows;
    ret = table_collection_compute_mutation_parents(&tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_NODE_OUT_OF_BOUNDS);
    tables.mutations.node[0] = 0;

    /* Mutations not ordered by tree */
    tables.mutations.node[2] = 1;
    tables.mutations.node[3] = 4;
    ret = table_collection_compute_mutation_parents(&tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_MUTATION_PARENT_AFTER_CHILD);
    tables.mutations.node[2] = 4;
    tables.mutations.node[3] = 1;
    tree_sequence_free(&ts);

    /* Mutations not ordered by site */
    tables.mutations.site[3] = 1;
    ret = table_collection_compute_mutation_parents(&tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_UNSORTED_MUTATIONS);
    tables.mutations.site[3] = 2;

    /* Check to make sure we still have legal mutations */
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(tree_sequence_get_num_sites(&ts), 3);
    CU_ASSERT_EQUAL(tree_sequence_get_num_mutations(&ts), 6);
    tree_sequence_free(&ts);

    tree_sequence_free(&ts);
    table_collection_free(&tables);
}

static void
test_single_tree_inconsistent_mutations(void)
{
    const char *sites =
        "0.0     0\n"
        "0.1     0\n"
        "0.2     0\n";
    const char *mutations =
        "0    0     1\n"
        "1    1     1\n"
        "2    4     1\n"
        "2    0     1\n";
    tree_sequence_t ts;
    variant_t *var;
    vargen_t vargen;
    hapgen_t hapgen;
    int ret;

    tree_sequence_from_text(&ts, 0, single_tree_ex_nodes, single_tree_ex_edges, NULL,
            sites, mutations, NULL, NULL);

    ret = hapgen_alloc(&hapgen, &ts);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_INCONSISTENT_MUTATIONS);
    ret = hapgen_free(&hapgen);

    ret = vargen_alloc(&vargen, &ts, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = vargen_next(&vargen, &var);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    ret = vargen_next(&vargen, &var);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    ret = vargen_next(&vargen, &var);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_INCONSISTENT_MUTATIONS);
    ret = vargen_free(&vargen);

    tree_sequence_free(&ts);
}

static void
test_single_unary_tree_hapgen(void)
{
    int ret = 0;
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0\n"
        "0  1   0\n"
        "0  2   0\n"
        "0  3   0\n"
        "0  4   0\n";
    const char *edges =
        "0 1 2 0\n"
        "0 1 3 1\n"
        "0 1 4 2,3\n"
        "0 1 5 4\n"
        "0 1 6 5\n";
    const char *sites =
        "0     0\n"
        "0.1   0\n"
        "0.2   0\n"
        "0.3   0\n";
    const char *mutations =
        "0    0     1\n"
        "1    1     1\n"
        "2    4     1\n"
        "3    5     1\n";
    tree_sequence_t ts;
    size_t num_samples = 2;
    size_t j;
    hapgen_t hapgen;
    char *haplotype;

    tree_sequence_from_text(&ts, 0, nodes, edges, NULL, NULL, NULL, NULL, NULL);

    ret = hapgen_alloc(&hapgen, &ts);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    hapgen_print_state(&hapgen, _devnull);
    for (j = 0; j < num_samples; j++) {
        ret = hapgen_get_haplotype(&hapgen, j, &haplotype);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_STRING_EQUAL(haplotype, "");
    }
    ret = hapgen_free(&hapgen);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    tree_sequence_free(&ts);

    tree_sequence_from_text(&ts, 0, nodes, edges, NULL, sites, mutations, NULL, NULL);
    ret = hapgen_alloc(&hapgen, &ts);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    hapgen_print_state(&hapgen, _devnull);

    ret = hapgen_get_haplotype(&hapgen, 0, &haplotype);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_STRING_EQUAL(haplotype, "1011");
    ret = hapgen_get_haplotype(&hapgen, 1, &haplotype);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_STRING_EQUAL(haplotype, "0111");

    ret = hapgen_free(&hapgen);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    tree_sequence_free(&ts);
}

static void
test_single_tree_mutgen(void)
{
    int ret = 0;
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "0  1   0\n"
        "0  2   0\n"
        "0  3   0\n";
    const char *edges =
        "0  1   4   0,1\n"
        "0  1   5   2,3\n"
        "0  1   6   4,5\n";
    size_t j;
    mutgen_t mutgen;
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_default);
    node_table_t node_table;
    edge_table_t edge_table;
    site_table_t sites, sites_after;
    mutation_table_t mutations, mutations_after;

    CU_ASSERT_FATAL(rng != NULL);
    ret = node_table_alloc(&node_table, 0, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = edge_table_alloc(&edge_table, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    parse_nodes(nodes, &node_table);
    parse_edges(edges, &edge_table);
    ret = site_table_alloc(&sites, 0, 0, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = mutation_table_alloc(&mutations, 0, 0, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = site_table_alloc(&sites_after, 0, 0, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = mutation_table_alloc(&mutations_after, 0, 0, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    ret = mutgen_alloc(&mutgen, 0.0, rng, MSP_ALPHABET_BINARY, 100);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = mutgen_generate_tables_tmp(&mutgen, &node_table, &edge_table);
    CU_ASSERT_EQUAL(ret, 0);
    ret = mutgen_populate_tables(&mutgen, &sites, &mutations);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_TRUE(mutations.num_rows == 0);
    mutgen_print_state(&mutgen, _devnull);
    ret = mutgen_free(&mutgen);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    gsl_rng_set(rng, 1);
    ret = mutgen_alloc(&mutgen, 10.0, rng, MSP_ALPHABET_BINARY, 100);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = mutgen_generate_tables_tmp(&mutgen, &node_table, &edge_table);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    mutgen_print_state(&mutgen, _devnull);
    ret = mutgen_populate_tables(&mutgen, &sites, &mutations);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_TRUE(mutations.num_rows > 0);
    CU_ASSERT_TRUE(mutations.num_rows == sites.num_rows);
    for (j = 0; j < mutations.num_rows; j++) {
        CU_ASSERT_TRUE(mutations.site[j] == j);
        CU_ASSERT_TRUE(sites.position[j] <= 1.0);
        CU_ASSERT_TRUE(mutations.node[j] < 6);
        CU_ASSERT_EQUAL(sites.ancestral_state[j], '0');
        CU_ASSERT_EQUAL(mutations.derived_state[j], '1');
    }
    ret = mutgen_free(&mutgen);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Test the reallocing behavior by setting a very small
     * block size.
     */
    gsl_rng_set(rng, 1);
    ret = mutgen_alloc(&mutgen, 10.0, rng, MSP_ALPHABET_BINARY, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = mutgen_alloc(&mutgen, 10.0, rng, MSP_ALPHABET_BINARY, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = mutgen_generate_tables_tmp(&mutgen, &node_table, &edge_table);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = mutgen_populate_tables(&mutgen, &sites_after, &mutations_after);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_TRUE(mutation_table_equal(&mutations, &mutations_after));
    CU_ASSERT_TRUE(site_table_equal(&sites, &sites_after));
    ret = mutgen_free(&mutgen);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    edge_table_free(&edge_table);
    node_table_free(&node_table);
    mutation_table_free(&mutations);
    site_table_free(&sites);
    mutation_table_free(&mutations_after);
    site_table_free(&sites_after);
    gsl_rng_free(rng);
}

static void
test_sparse_tree_errors(void)
{
    int ret;
    size_t j;
    uint32_t num_nodes = 9;
    uint32_t u;
    node_t node;
    tree_sequence_t ts, other_ts;
    sparse_tree_t t, other_t;
    node_id_t bad_nodes[] = {num_nodes, num_nodes + 1, -1};
    node_id_t tracked_samples[] = {0, 0, 0};

    tree_sequence_from_text(&ts, 0, paper_ex_nodes, paper_ex_edges, NULL, NULL, NULL, NULL, NULL);

    ret = sparse_tree_alloc(&t, NULL, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = sparse_tree_alloc(&t, &ts, MSP_SAMPLE_COUNTS);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = sparse_tree_first(&t);
    CU_ASSERT_EQUAL_FATAL(ret, 1);

    /* Out-of-bounds queries */
    for (j = 0; j < sizeof(bad_nodes) / sizeof(node_id_t); j++) {
        u = bad_nodes[j];
        ret = sparse_tree_get_parent(&t, u, NULL);
        CU_ASSERT_EQUAL(ret, MSP_ERR_OUT_OF_BOUNDS);
        ret = sparse_tree_get_time(&t, u, NULL);
        CU_ASSERT_EQUAL(ret, MSP_ERR_OUT_OF_BOUNDS);
        ret = sparse_tree_get_mrca(&t, u, 0, NULL);
        CU_ASSERT_EQUAL(ret, MSP_ERR_OUT_OF_BOUNDS);
        ret = sparse_tree_get_mrca(&t, 0, u, NULL);
        CU_ASSERT_EQUAL(ret, MSP_ERR_OUT_OF_BOUNDS);
        ret = sparse_tree_get_num_samples(&t, u, NULL);
        CU_ASSERT_EQUAL(ret, MSP_ERR_OUT_OF_BOUNDS);
        ret = sparse_tree_get_num_tracked_samples(&t, u, NULL);
        CU_ASSERT_EQUAL(ret, MSP_ERR_OUT_OF_BOUNDS);
        ret = sparse_tree_get_sample_list(&t, u, NULL, NULL);
        CU_ASSERT_EQUAL(ret, MSP_ERR_OUT_OF_BOUNDS);
        /* Also check tree sequence methods */
        ret = tree_sequence_get_node(&ts, u, &node);
        CU_ASSERT_EQUAL(ret, MSP_ERR_OUT_OF_BOUNDS);
        CU_ASSERT(!tree_sequence_is_sample(&ts, u));
        CU_ASSERT(!sparse_tree_is_sample(&t, u));
    }

    tracked_samples[0] = 0;
    tracked_samples[1] = tree_sequence_get_num_samples(&ts);
    ret = sparse_tree_set_tracked_samples(&t, 2, tracked_samples);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_SAMPLES);
    tracked_samples[1] = tree_sequence_get_num_nodes(&ts);
    ret = sparse_tree_set_tracked_samples(&t, 2, tracked_samples);
    CU_ASSERT_EQUAL(ret, MSP_ERR_OUT_OF_BOUNDS);
    tracked_samples[1] = 0;
    ret = sparse_tree_set_tracked_samples(&t, 2, tracked_samples);
    CU_ASSERT_EQUAL(ret, MSP_ERR_DUPLICATE_SAMPLE);
    ret = sparse_tree_set_tracked_samples_from_sample_list(&t, NULL, NULL);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);

    tree_sequence_from_text(&other_ts, 0, paper_ex_nodes, paper_ex_edges, NULL, NULL, NULL, NULL, NULL);
    ret = sparse_tree_alloc(&other_t, &other_ts, 0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = sparse_tree_copy(&t, &t);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = sparse_tree_copy(&t, &other_t);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);

    /* TODO run checks for the various unsupported operations with flags */

    sparse_tree_free(&t);
    sparse_tree_free(&other_t);
    tree_sequence_free(&other_ts);
    tree_sequence_free(&ts);
}

static void
test_tree_sequence_iter(void)
{
    node_id_t parents[] = {
        6, 5, 8, 5, MSP_NULL_NODE, 6, 8, MSP_NULL_NODE, MSP_NULL_NODE,
        6, 5, 4, 4, 5, 6, MSP_NULL_NODE, MSP_NULL_NODE, MSP_NULL_NODE,
        7, 5, 4, 4, 5, 7, MSP_NULL_NODE, MSP_NULL_NODE, MSP_NULL_NODE,
    };
    uint32_t num_trees = 3;
    tree_sequence_t ts;

    tree_sequence_from_text(&ts, 0, paper_ex_nodes, paper_ex_edges, NULL,
            paper_ex_sites, paper_ex_mutations, NULL, NULL);
    verify_trees(&ts, num_trees, parents);
    tree_sequence_free(&ts);
}

static void
test_unary_tree_sequence_iter(void)
{
    node_id_t parents[] = {
        6, 5, 7, 5, MSP_NULL_NODE, 6, 8, 8, MSP_NULL_NODE,
        6, 5, 4, 4, 5, 6, 8, MSP_NULL_NODE, MSP_NULL_NODE,
        7, 5, 4, 4, 5, 7, MSP_NULL_NODE, MSP_NULL_NODE, MSP_NULL_NODE,
    };
    tree_sequence_t ts;
    uint32_t num_trees = 3;

    tree_sequence_from_text(&ts, 0, unary_ex_nodes, unary_ex_edges, NULL,
            unary_ex_sites, unary_ex_mutations, NULL, NULL);
    verify_trees(&ts, num_trees, parents);
    tree_sequence_free(&ts);
}

static void
test_internal_sample_tree_sequence_iter(void)
{
    node_id_t parents[] = {
        7, 5, 4, 4, 5, 7, MSP_NULL_NODE, MSP_NULL_NODE, MSP_NULL_NODE,
        4, 5, 4, 8, 5, 8, MSP_NULL_NODE, MSP_NULL_NODE, MSP_NULL_NODE,
        6, 5, 4, 4, 5, 6, MSP_NULL_NODE, MSP_NULL_NODE, MSP_NULL_NODE,
    };
    tree_sequence_t ts;
    uint32_t num_trees = 3;

    tree_sequence_from_text(&ts, 0, internal_sample_ex_nodes, internal_sample_ex_edges, NULL,
            internal_sample_ex_sites, internal_sample_ex_mutations, NULL, NULL);
    verify_trees(&ts, num_trees, parents);
    tree_sequence_free(&ts);
}

static void
test_internal_sample_simplified_tree_sequence_iter(void)
{
    int ret;
    tree_sequence_t ts, simplified;
    node_id_t samples[] = {2, 3, 5};
    node_id_t node_map[9];
    node_id_t z = MSP_NULL_NODE;
    node_id_t parents[] = {
    /*  0  1  2  3  4 */
        3, 3, z, 2, z,
        2, 4, 4, z, z,
        3, 3, z, 2, z,
    };
    uint32_t num_trees = 3;

    tree_sequence_from_text(&ts, 0, internal_sample_ex_nodes, internal_sample_ex_edges, NULL,
            internal_sample_ex_sites, internal_sample_ex_mutations, NULL, NULL);
    ret = tree_sequence_simplify(&ts, samples, 3, 0, &simplified, node_map);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(node_map[2], 0);
    CU_ASSERT_EQUAL(node_map[3], 1);
    CU_ASSERT_EQUAL(node_map[5], 2);

    verify_trees(&simplified, num_trees, parents);
    tree_sequence_free(&simplified);
    tree_sequence_free(&ts);
}

static void
test_nonbinary_tree_sequence_iter(void)
{
    /* We make one mutation for each tree */
    node_id_t parents[] = {
        8, 8, 8, 8, 10, 10, 9, 10, 9, 12, 12, MSP_NULL_NODE, MSP_NULL_NODE,
        8, 8, 8, 8, 10, 11, 9, 10, 9, 11, 12, 12, MSP_NULL_NODE,
    };

    tree_sequence_t ts;
    uint32_t num_trees = 2;

    tree_sequence_from_text(&ts, 0, nonbinary_ex_nodes, nonbinary_ex_edges, NULL,
            nonbinary_ex_sites, nonbinary_ex_mutations, NULL, NULL);
    verify_trees(&ts, num_trees, parents);
    tree_sequence_free(&ts);
}

static void
test_left_to_right_tree_sequence_iter(void)
{
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "0  0.090   0\n"
        "0  0.170   0\n"
        "0  0.253   0\n"
        "0  0.071   0\n"
        "0  0.202   0\n";
    const char *edges =
        "2 10 7 2,3\n"
        "0 2  4 1\n"
        "2 10 4 1\n"
        "0 2  4 3\n"
        "2 10 4 7\n"
        "0 7  5 0,4\n"
        "7 10 8 0,4\n"
        "0 2  6 2,5\n";
    const char *sites =
        "1      0\n"
        "4.5    0\n"
        "8.5    0\n";
    const char *mutations =
        "0    2    1\n"
        "1    0    1\n"
        "2    4    1\n";

    node_id_t parents[] = {
        5, 4, 6, 4, 5, 6, MSP_NULL_NODE, MSP_NULL_NODE, MSP_NULL_NODE,
        5, 4, 7, 7, 5, MSP_NULL_NODE, MSP_NULL_NODE, 4, MSP_NULL_NODE,
        8, 4, 7, 7, 8, MSP_NULL_NODE, MSP_NULL_NODE, 4, MSP_NULL_NODE,
    };
    tree_sequence_t ts;
    uint32_t num_trees = 3;

    tree_sequence_from_text(&ts, 0, nodes, edges, NULL, sites, mutations, NULL, NULL);
    verify_trees(&ts, num_trees, parents);
    verify_tree_next_prev(&ts);
    tree_sequence_free(&ts);
}

static void
test_gappy_tree_sequence_iter(void)
{
    const char *nodes =
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "1  0   0\n"
        "0  0.090   0\n"
        "0  0.170   0\n"
        "0  0.253   0\n"
        "0  0.071   0\n"
        "0  0.202   0\n";
    const char *edges =
        "2 7  7 2\n"
        "8 10 7 2\n"
        "2 7  7 3\n"
        "8 10 7 3\n"
        "1 2  4 1\n"
        "2 7  4 1\n"
        "8 10 4 1\n"
        "1 2  4 3\n"
        "2 7  4 7\n"
        "8 10 4 7\n"
        "1 7  5 0,4\n"
        "8 10 8 0,4\n"
        "1 2  6 2,5\n";
    node_id_t z = MSP_NULL_NODE;
    node_id_t parents[] = {
        z, z, z, z, z, z, z, z, z,
        5, 4, 6, 4, 5, 6, z, z, z,
        5, 4, 7, 7, 5, z, z, 4, z,
        z, z, z, z, z, z, z, z, z,
        8, 4, 7, 7, 8, z, z, 4, z,
        z, z, z, z, z, z, z, z, z,
    };
    tree_sequence_t ts;
    uint32_t num_trees = 6;

    tree_sequence_from_text(&ts, 12, nodes, edges, NULL, NULL, NULL, NULL, NULL);
    verify_trees(&ts, num_trees, parents);
    verify_tree_next_prev(&ts);
    tree_sequence_free(&ts);
}

typedef struct {
    uint32_t tree_index;
    uint32_t node;
    uint32_t count;
} sample_count_test_t;

static void
verify_sample_counts(tree_sequence_t *ts, size_t num_tests, sample_count_test_t *tests)
{
    int ret;
    size_t j, num_samples, n, k;
    sparse_tree_t tree;
    node_list_t *u, *head, *tail;
    node_id_t *samples;

    n = tree_sequence_get_num_samples(ts);
    ret = tree_sequence_get_samples(ts, &samples);
    CU_ASSERT_EQUAL(ret, 0);

    /* First run without the MSP_SAMPLE_COUNTS feature */
    ret = sparse_tree_alloc(&tree, ts, 0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = sparse_tree_first(&tree);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    for (j = 0; j < num_tests; j++) {
        while (tree.index < tests[j].tree_index) {
            ret = sparse_tree_next(&tree);
            CU_ASSERT_EQUAL_FATAL(ret, 1);
        }
        ret = sparse_tree_get_num_samples(&tree, tests[j].node, &num_samples);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        CU_ASSERT_EQUAL(tests[j].count, num_samples);
        /* all operations depending on tracked samples should fail. */
        ret = sparse_tree_get_num_tracked_samples(&tree, 0, &num_samples);
        CU_ASSERT_EQUAL(ret, MSP_ERR_UNSUPPORTED_OPERATION);
        ret = sparse_tree_get_sample_list(&tree, 0, NULL, NULL);
        CU_ASSERT_EQUAL(ret, MSP_ERR_UNSUPPORTED_OPERATION);
    }
    sparse_tree_free(&tree);

    /* Now run with MSP_SAMPLE_COUNTS but with no samples tracked. */
    ret = sparse_tree_alloc(&tree, ts, MSP_SAMPLE_COUNTS);
    CU_ASSERT_EQUAL(ret, 0);
    ret = sparse_tree_first(&tree);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    for (j = 0; j < num_tests; j++) {
        while (tree.index < tests[j].tree_index) {
            ret = sparse_tree_next(&tree);
            CU_ASSERT_EQUAL_FATAL(ret, 1);
        }
        ret = sparse_tree_get_num_samples(&tree, tests[j].node, &num_samples);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        CU_ASSERT_EQUAL(tests[j].count, num_samples);
        /* all operations depending on tracked samples should fail. */
        ret = sparse_tree_get_num_tracked_samples(&tree, 0, &num_samples);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_EQUAL(num_samples, 0);
        /* Getting sample lists should still fail, as it's not enabled. */
        ret = sparse_tree_get_sample_list(&tree, 0, NULL, NULL);
        CU_ASSERT_EQUAL(ret, MSP_ERR_UNSUPPORTED_OPERATION);
    }
    sparse_tree_free(&tree);

    /* Run with MSP_SAMPLE_LISTS, but without MSP_SAMPLE_COUNTS */
    ret = sparse_tree_alloc(&tree, ts, MSP_SAMPLE_LISTS);
    CU_ASSERT_EQUAL(ret, 0);
    ret = sparse_tree_first(&tree);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    for (j = 0; j < num_tests; j++) {
        while (tree.index < tests[j].tree_index) {
            ret = sparse_tree_next(&tree);
            CU_ASSERT_EQUAL_FATAL(ret, 1);
        }
        ret = sparse_tree_get_num_samples(&tree, tests[j].node, &num_samples);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        CU_ASSERT_EQUAL(tests[j].count, num_samples);
        /* all operations depending on tracked samples should fail. */
        ret = sparse_tree_get_num_tracked_samples(&tree, 0, &num_samples);
        CU_ASSERT_EQUAL(ret, MSP_ERR_UNSUPPORTED_OPERATION);
        ret = sparse_tree_get_sample_list(&tree, tests[j].node, &head, &tail);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        u = head;
        k = 0;
        while (1) {
            k++;
            if (u == tail) {
                break;
            }
            CU_ASSERT_TRUE(sparse_tree_is_sample(&tree, u->node));
            u = u->next;
        }
        CU_ASSERT_EQUAL(tests[j].count, k);
    }
    sparse_tree_free(&tree);

    /* Now use MSP_SAMPLE_COUNTS|MSP_SAMPLE_LISTS */
    ret = sparse_tree_alloc(&tree, ts, MSP_SAMPLE_COUNTS|MSP_SAMPLE_LISTS);
    CU_ASSERT_EQUAL(ret, 0);
    ret = sparse_tree_set_tracked_samples(&tree, n, samples);
    CU_ASSERT_EQUAL(ret, 0);
    ret = sparse_tree_first(&tree);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    for (j = 0; j < num_tests; j++) {
        while (tree.index < tests[j].tree_index) {
            ret = sparse_tree_next(&tree);
            CU_ASSERT_EQUAL_FATAL(ret, 1);
        }
        ret = sparse_tree_get_num_samples(&tree, tests[j].node, &num_samples);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        CU_ASSERT_EQUAL(tests[j].count, num_samples);

        /* We're tracking all samples, so the count should be the same */
        ret = sparse_tree_get_num_tracked_samples(&tree, tests[j].node, &num_samples);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        CU_ASSERT_EQUAL(tests[j].count, num_samples);
        ret = sparse_tree_get_sample_list(&tree, tests[j].node, &head, &tail);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        u = head;
        k = 0;
        while (1) {
            k++;
            if (u == tail) {
                break;
            }
            CU_ASSERT_TRUE(sparse_tree_is_sample(&tree, u->node));
            u = u->next;
        }
        CU_ASSERT_EQUAL(tests[j].count, k);
    }
    sparse_tree_free(&tree);
}


static void
verify_sample_sets_for_tree(sparse_tree_t *tree)
{
    int ret, stack_top, j;
    node_id_t u, v, n, num_nodes, num_samples;
    size_t tmp;
    node_id_t *stack, *samples;
    node_list_t *z, *head, *tail;
    tree_sequence_t *ts = tree->tree_sequence;

    n = tree_sequence_get_num_samples(ts);
    num_nodes = tree_sequence_get_num_nodes(ts);
    stack = malloc(n * sizeof(node_id_t));
    samples = malloc(n * sizeof(node_id_t));
    CU_ASSERT_FATAL(stack != NULL);
    CU_ASSERT_FATAL(samples != NULL);
    for (u = 0; u < num_nodes; u++) {
        if (tree->left_child[u] == MSP_NULL_NODE && !tree_sequence_is_sample(ts, u)) {
            ret = sparse_tree_get_sample_list(tree, u, &head, &tail);
            CU_ASSERT_EQUAL(ret, 0);
            CU_ASSERT_EQUAL(head, NULL);
            CU_ASSERT_EQUAL(tail, NULL);
        } else {
            stack_top = 0;
            num_samples = 0;
            stack[stack_top] = u;
            while (stack_top >= 0) {
                v = stack[stack_top];
                stack_top--;
                if (tree_sequence_is_sample(ts, v)) {
                    samples[num_samples] = v;
                    num_samples++;
                }
                for (v = tree->right_child[v]; v != MSP_NULL_NODE; v = tree->left_sib[v]) {
                    stack_top++;
                    stack[stack_top] = v;
                }
            }
            ret = sparse_tree_get_num_samples(tree, u, &tmp);
            CU_ASSERT_EQUAL(ret, 0);
            CU_ASSERT_EQUAL_FATAL(num_samples, tmp);
            ret = sparse_tree_get_sample_list(tree, u, &head, &tail);

            CU_ASSERT_EQUAL(ret, 0);
            z = head;
            j = 0;
            while (1) {
                CU_ASSERT_TRUE_FATAL(j < n);
                CU_ASSERT_EQUAL_FATAL(samples[j], z->node);
                j++;
                if (z == tail) {
                    break;
                }
                z = z->next;
            }
            CU_ASSERT_EQUAL(j, num_samples);
        }
    }
    free(stack);
    free(samples);
}

static void
verify_sample_sets(tree_sequence_t *ts)
{
    int ret;
    sparse_tree_t t;

    ret = sparse_tree_alloc(&t, ts, MSP_SAMPLE_COUNTS|MSP_SAMPLE_LISTS);
    CU_ASSERT_EQUAL(ret, 0);

    for (ret = sparse_tree_first(&t); ret == 1; ret = sparse_tree_next(&t)) {
        verify_sample_sets_for_tree(&t);
    }
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    for (ret = sparse_tree_last(&t); ret == 1; ret = sparse_tree_prev(&t)) {
        verify_sample_sets_for_tree(&t);
    }
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    sparse_tree_free(&t);
}

static void
verify_tree_equals(tree_sequence_t *ts)
{
    int ret;
    sparse_tree_t *trees, t;
    size_t j, k;
    tree_sequence_t *other_ts = get_example_tree_sequence(
            10, 0, 100, 100.0, 1.0, 1.0, 0, NULL, MSP_ALPHABET_BINARY);
    int flags[] = {0, MSP_SAMPLE_LISTS, MSP_SAMPLE_COUNTS,
        MSP_SAMPLE_LISTS | MSP_SAMPLE_COUNTS};

    trees = get_tree_list(ts);
    ret = sparse_tree_alloc(&t, other_ts, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    for (j = 0; j < tree_sequence_get_num_trees(ts); j++) {
        ret = sparse_tree_equal(&t, &trees[j]);
        CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
        for (k = 0; k < tree_sequence_get_num_trees(ts); k++) {
            ret = sparse_tree_equal(&trees[j], &trees[k]);
            if (j == k) {
                CU_ASSERT_EQUAL_FATAL(ret, 0);
            } else {
                CU_ASSERT_EQUAL_FATAL(ret, 1);
            }
        }
    }
    ret = sparse_tree_free(&t);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    for (j = 0; j < sizeof(flags) / sizeof(int); j++) {
        ret = sparse_tree_alloc(&t, ts, flags[j]);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        for (ret = sparse_tree_first(&t); ret == 1;
                ret = sparse_tree_next(&t)) {
            for (k = 0; k < tree_sequence_get_num_trees(ts); k++) {
                ret = sparse_tree_equal(&t, &trees[k]);
                if (t.index == k) {
                    CU_ASSERT_EQUAL_FATAL(ret, 0);
                } else {
                    CU_ASSERT_EQUAL_FATAL(ret, 1);
                }
            }
        }
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        ret = sparse_tree_free(&t);
        CU_ASSERT_EQUAL(ret, 0);
    }
    for (j = 0; j < tree_sequence_get_num_trees(ts); j++) {
        ret = sparse_tree_free(&trees[j]);
    }
    free(trees);
    tree_sequence_free(other_ts);
    free(other_ts);
}

static void
test_sample_sets(void)
{
    sample_count_test_t tests[] = {
        {0, 0, 1}, {0, 5, 2}, {0, 6, 3},
        {1, 4, 2}, {1, 5, 3}, {1, 6, 4}};
    uint32_t num_tests = 6;
    tree_sequence_t ts;

    tree_sequence_from_text(&ts, 0, paper_ex_nodes, paper_ex_edges, NULL, NULL, NULL, NULL, NULL);
    verify_sample_counts(&ts, num_tests, tests);
    verify_sample_sets(&ts);

    tree_sequence_free(&ts);
}

static void
test_nonbinary_sample_sets(void)
{
    sample_count_test_t tests[] = {
        {0, 0, 1}, {0, 8, 4}, {0, 9, 5}, {0, 10, 3}, {0, 12, 8},
        {1, 5, 1}, {1, 8, 4}, {1, 9, 5}, {0, 10, 2}, {0, 11, 1}};
    uint32_t num_tests = 8;
    tree_sequence_t ts;

    tree_sequence_from_text(&ts, 0, nonbinary_ex_nodes, nonbinary_ex_edges, NULL,
            NULL, NULL, NULL, NULL);
    verify_sample_counts(&ts, num_tests, tests);
    verify_sample_sets(&ts);

    tree_sequence_free(&ts);
}

static void
test_internal_sample_sample_sets(void)
{
    sample_count_test_t tests[] = {
        {0, 0, 1}, {0, 5, 4}, {0, 4, 2}, {0, 7, 5},
        {1, 4, 2}, {1, 5, 4}, {1, 8, 5},
        {2, 5, 4}, {2, 6, 5}};
    uint32_t num_tests = 9;
    tree_sequence_t ts;

    tree_sequence_from_text(&ts, 0, internal_sample_ex_nodes, internal_sample_ex_edges,
            NULL, NULL, NULL, NULL, NULL);
    verify_sample_counts(&ts, num_tests, tests);
    verify_sample_sets(&ts);

    tree_sequence_free(&ts);
}

static void
test_tree_sequence_bad_records(void)
{
    int ret = 0;
    tree_sequence_t ts;
    table_collection_t tables;
    uint32_t num_trees = 3;
    node_id_t parents[] = {
        6, 5, 8, 5, MSP_NULL_NODE, 6, 8, MSP_NULL_NODE, MSP_NULL_NODE,
        6, 5, 4, 4, 5, 6, MSP_NULL_NODE, MSP_NULL_NODE, MSP_NULL_NODE,
        7, 5, 4, 4, 5, 7, MSP_NULL_NODE, MSP_NULL_NODE, MSP_NULL_NODE,
    };

    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    parse_nodes(paper_ex_nodes, &tables.nodes);
    parse_edges(paper_ex_edges, &tables.edges);

    /* Make sure we have a good set of records */
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL_FATAL(ts.num_trees, 3);
    verify_trees(&ts, num_trees, parents);
    tree_sequence_free(&ts);

    /* Left value greater than right */
    tables.edges.left[0] = 10.0;
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_EDGE_INTERVAL);
    tree_sequence_free(&ts);
    tables.edges.left[0] = 2.0;

    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL(ret, 0);
    verify_trees(&ts, num_trees, parents);
    tree_sequence_free(&ts);

    table_collection_free(&tables);
}

static void
verify_tree_diffs(tree_sequence_t *ts)
{
    int ret;
    tree_diff_iterator_t iter;
    sparse_tree_t tree;
    edge_list_t *record, *records_out, *records_in;
    size_t num_nodes = tree_sequence_get_num_nodes(ts);
    size_t j, num_trees;
    double left, right;
    node_id_t *parent = malloc(num_nodes * sizeof(node_id_t));
    node_id_t *child = malloc(num_nodes * sizeof(node_id_t));
    node_id_t *sib = malloc(num_nodes * sizeof(node_id_t));
    node_id_t *samples;

    CU_ASSERT_FATAL(parent != NULL);
    CU_ASSERT_FATAL(child != NULL);
    CU_ASSERT_FATAL(sib != NULL);
    for (j = 0; j < num_nodes; j++) {
        parent[j] = MSP_NULL_NODE;
        child[j] = MSP_NULL_NODE;
        sib[j] = MSP_NULL_NODE;
    }
    ret = tree_sequence_get_samples(ts, &samples);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = tree_diff_iterator_alloc(&iter, ts);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = sparse_tree_alloc(&tree, ts, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = sparse_tree_first(&tree);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    tree_diff_iterator_print_state(&iter, _devnull);

    num_trees = 0;
    while ((ret = tree_diff_iterator_next(
                &iter, &left, &right, &records_out, &records_in)) == 1) {
        tree_diff_iterator_print_state(&iter, _devnull);
        num_trees++;
        for (record = records_out; record != NULL; record = record->next) {
            parent[record->edge.child] = MSP_NULL_NODE;
        }
        for (record = records_in; record != NULL; record = record->next) {
            parent[record->edge.child] = record->edge.parent;
        }
        /* Now check against the sparse tree iterator. */
        for (j = 0; j < num_nodes; j++) {
            CU_ASSERT_EQUAL(parent[j], tree.parent[j]);
        }
        CU_ASSERT_EQUAL(tree.left, left);
        CU_ASSERT_EQUAL(tree.right, right);
        ret = sparse_tree_next(&tree);
        if (num_trees < tree_sequence_get_num_trees(ts)) {
            CU_ASSERT_EQUAL(ret, 1);
        } else {
            CU_ASSERT_EQUAL(ret, 0);
        }
    }
    CU_ASSERT_EQUAL(num_trees, tree_sequence_get_num_trees(ts));
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = sparse_tree_next(&tree);
    CU_ASSERT_EQUAL(ret, 0);
    ret = tree_diff_iterator_free(&iter);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = sparse_tree_free(&tree);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    free(parent);
    free(child);
    free(sib);
}

static void
test_tree_sequence_diff_iter(void)
{
    int ret;
    tree_sequence_t ts;

    tree_sequence_from_text(&ts, 0, paper_ex_nodes, paper_ex_edges, NULL, NULL, NULL, NULL, NULL);
    verify_tree_diffs(&ts);

    ret = tree_sequence_free(&ts);
    CU_ASSERT_EQUAL(ret, 0);
}

static void
test_nonbinary_tree_sequence_diff_iter(void)
{
    int ret;
    tree_sequence_t ts;

    tree_sequence_from_text(&ts, 0, nonbinary_ex_nodes, nonbinary_ex_edges, NULL,
            NULL, NULL, NULL, NULL);
    verify_tree_diffs(&ts);

    ret = tree_sequence_free(&ts);
    CU_ASSERT_EQUAL(ret, 0);
}

static void
test_unary_tree_sequence_diff_iter(void)
{
    int ret;
    tree_sequence_t ts;

    tree_sequence_from_text(&ts, 0, unary_ex_nodes, unary_ex_edges, NULL,
            NULL, NULL, NULL, NULL);
    verify_tree_diffs(&ts);

    ret = tree_sequence_free(&ts);
    CU_ASSERT_EQUAL(ret, 0);
}

static void
test_internal_sample_tree_sequence_diff_iter(void)
{
    int ret;
    tree_sequence_t ts;

    tree_sequence_from_text(&ts, 0, internal_sample_ex_nodes, internal_sample_ex_edges, NULL,
            NULL, NULL, NULL, NULL);
    verify_tree_diffs(&ts);

    ret = tree_sequence_free(&ts);
    CU_ASSERT_EQUAL(ret, 0);
}

static void
test_diff_iter_from_examples(void)
{
    tree_sequence_t **examples = get_example_tree_sequences(1);
    uint32_t j;

    CU_ASSERT_FATAL(examples != NULL);
    for (j = 0; examples[j] != NULL; j++) {
        verify_tree_diffs(examples[j]);
        tree_sequence_free(examples[j]);
        free(examples[j]);
    }
    free(examples);
}

static void
test_tree_iter_from_examples(void)
{

    tree_sequence_t **examples = get_example_tree_sequences(1);
    uint32_t j;

    CU_ASSERT_FATAL(examples != NULL);
    for (j = 0; examples[j] != NULL; j++) {
        verify_trees_consistent(examples[j]);
        tree_sequence_free(examples[j]);
        free(examples[j]);
    }
    free(examples);
}

static void
test_sample_sets_from_examples(void)
{
    tree_sequence_t **examples = get_example_tree_sequences(1);
    uint32_t j;

    CU_ASSERT_FATAL(examples != NULL);
    for (j = 0; examples[j] != NULL; j++) {
        verify_sample_sets(examples[j]);
        tree_sequence_free(examples[j]);
        free(examples[j]);
    }
    free(examples);
}

static void
test_tree_equals_from_examples(void)
{
    tree_sequence_t **examples = get_example_tree_sequences(1);
    uint32_t j;

    CU_ASSERT_FATAL(examples != NULL);
    for (j = 0; examples[j] != NULL; j++) {
        verify_tree_equals(examples[j]);
        tree_sequence_free(examples[j]);
        free(examples[j]);
    }
    free(examples);
}

static void
test_next_prev_from_examples(void)
{

    tree_sequence_t **examples = get_example_tree_sequences(1);
    uint32_t j;

    CU_ASSERT_FATAL(examples != NULL);
    for (j = 0; examples[j] != NULL; j++) {
        verify_tree_next_prev(examples[j]);
        tree_sequence_free(examples[j]);
        free(examples[j]);
    }
    free(examples);
}

static void
test_hapgen_from_examples(void)
{
    tree_sequence_t **examples = get_example_tree_sequences(1);
    uint32_t j;

    CU_ASSERT_FATAL(examples != NULL);
    for (j = 0; examples[j] != NULL; j++) {
        verify_hapgen(examples[j]);
        tree_sequence_free(examples[j]);
        free(examples[j]);
    }
    free(examples);
}

static void
test_ld_from_examples(void)
{
    tree_sequence_t **examples = get_example_tree_sequences(1);
    uint32_t j;

    CU_ASSERT_FATAL(examples != NULL);
    for (j = 0; examples[j] != NULL; j++) {
        verify_ld(examples[j]);
        tree_sequence_free(examples[j]);
        free(examples[j]);
    }
    free(examples);
}

static void
test_vargen_from_examples(void)
{
    tree_sequence_t **examples = get_example_tree_sequences(1);
    uint32_t j;

    CU_ASSERT_FATAL(examples != NULL);
    for (j = 0; examples[j] != NULL; j++) {
        verify_vargen(examples[j]);
        tree_sequence_free(examples[j]);
        free(examples[j]);
    }
    free(examples);
}

static void
test_stats_from_examples(void)
{
    tree_sequence_t **examples = get_example_tree_sequences(1);
    uint32_t j;

    CU_ASSERT_FATAL(examples != NULL);
    for (j = 0; examples[j] != NULL; j++) {
        verify_stats(examples[j]);
        tree_sequence_free(examples[j]);
        free(examples[j]);
    }
    free(examples);
}

static void
test_compute_mutation_parents_from_examples(void)
{
    tree_sequence_t **examples = get_example_tree_sequences(1);
    uint32_t j;

    CU_ASSERT_FATAL(examples != NULL);
    for (j = 0; examples[j] != NULL; j++) {
        verify_compute_mutation_parents(examples[j]);
        tree_sequence_free(examples[j]);
        free(examples[j]);
    }
    free(examples);
}

static void
verify_simplify_errors(tree_sequence_t *ts)
{
    int ret;
    node_id_t *s;
    node_id_t u;
    tree_sequence_t subset;
    node_id_t sample[2];

    ret = tree_sequence_get_samples(ts, &s);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    memcpy(sample, s, 2 * sizeof(node_id_t));

    for (u = 0; u < (node_id_t) tree_sequence_get_num_nodes(ts); u++) {
        if (! tree_sequence_is_sample(ts, u)) {
            sample[1] = u;
            ret = tree_sequence_simplify(ts, sample, 2, 0, &subset, NULL);
            CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_SAMPLES);
        }
    }
    sample[0] = -1;
    ret = tree_sequence_simplify(ts, sample, 2, 0, &subset, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_NODE_OUT_OF_BOUNDS);
    sample[0] = s[0];
    sample[1] = s[0];
    ret = tree_sequence_simplify(ts, sample, 2, 0, &subset, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_DUPLICATE_SAMPLE);
}

static void
test_simplify_from_examples(void)
{
    tree_sequence_t **examples = get_example_tree_sequences(1);
    uint32_t j;

    CU_ASSERT_FATAL(examples != NULL);
    for (j = 0; examples[j] != NULL; j++) {
        verify_simplify(examples[j]);
        verify_simplify_errors(examples[j]);
        tree_sequence_free(examples[j]);
        free(examples[j]);
    }
    free(examples);
}

static void
verify_newick(tree_sequence_t *ts)
{
    int ret, err;
    sparse_tree_t t;
    size_t precision = 4;
    size_t buffer_size = 1024 * 1024;
    char *newick = malloc(buffer_size);
    size_t j, size;

    CU_ASSERT_FATAL(newick != NULL);

    ret = sparse_tree_alloc(&t, ts, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = sparse_tree_first(&t);
    CU_ASSERT_FATAL(ret == 1);
    if (sparse_tree_get_num_roots(&t) == 1) {
        err = sparse_tree_get_newick(&t, precision, 1.0, 0, buffer_size, newick);
        CU_ASSERT_EQUAL_FATAL(err, 0);
        size = strlen(newick);
        CU_ASSERT_TRUE(size > 0);
        CU_ASSERT_TRUE(size < buffer_size);
        for (j = 0; j <= size; j++) {
            err = sparse_tree_get_newick(&t, precision, 1.0, 0, j, newick);
            CU_ASSERT_EQUAL_FATAL(err, MSP_ERR_BUFFER_OVERFLOW);
        }
        err = sparse_tree_get_newick(&t, precision, 1.0, 0, size + 1, newick);
        CU_ASSERT_EQUAL_FATAL(err, 0);
    }

    for (ret = sparse_tree_first(&t); ret == 1; ret = sparse_tree_next(&t)) {
        err = sparse_tree_get_newick(&t, precision, 1.0, 0, 0, NULL);
        if (sparse_tree_get_num_roots(&t) == 1) {
            CU_ASSERT_EQUAL_FATAL(err, MSP_ERR_BAD_PARAM_VALUE);
            err = sparse_tree_get_newick(&t, precision, 1.0, 0, buffer_size, newick);
            CU_ASSERT_EQUAL_FATAL(err, 0);
            size = strlen(newick);
            CU_ASSERT_EQUAL(newick[size - 1], ';');
        } else {
            CU_ASSERT_EQUAL(err, MSP_ERR_MULTIROOT_NEWICK);
        }
    }
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    sparse_tree_free(&t);
    free(newick);
}

static void
test_newick_from_examples(void)
{
    tree_sequence_t **examples = get_example_tree_sequences(1);
    uint32_t j;

    CU_ASSERT_FATAL(examples != NULL);
    for (j = 0; examples[j] != NULL; j++) {
        verify_newick(examples[j]);
        tree_sequence_free(examples[j]);
        free(examples[j]);
    }
    free(examples);
}

static void
verify_tree_sequences_equal(tree_sequence_t *ts1, tree_sequence_t *ts2,
        bool check_migrations, bool check_mutations,
        bool check_provenance)
{
    int ret, err1, err2;
    size_t j;
    edge_t r1, r2;
    node_t n1, n2;
    migration_t m1, m2;
    provenance_t p1, p2;
    individual_t i1, i2;
    tmp_population_t pop1, pop2;
    size_t num_mutations = tree_sequence_get_num_mutations(ts1);
    site_t site_1, site_2;
    mutation_t mutation_1, mutation_2;
    sparse_tree_t t1, t2;

    /* tree_sequence_print_state(ts1, stdout); */
    /* tree_sequence_print_state(ts2, stdout); */

    CU_ASSERT_EQUAL(
        tree_sequence_get_num_samples(ts1),
        tree_sequence_get_num_samples(ts2));
    CU_ASSERT_EQUAL(
        tree_sequence_get_sequence_length(ts1),
        tree_sequence_get_sequence_length(ts2));
    CU_ASSERT_EQUAL(
        tree_sequence_get_num_edges(ts1),
        tree_sequence_get_num_edges(ts2));
    CU_ASSERT_EQUAL(
        tree_sequence_get_num_nodes(ts1),
        tree_sequence_get_num_nodes(ts2));
    CU_ASSERT_EQUAL(
        tree_sequence_get_num_trees(ts1),
        tree_sequence_get_num_trees(ts2));

    for (j = 0; j < tree_sequence_get_num_nodes(ts1); j++) {
        ret = tree_sequence_get_node(ts1, j, &n1);
        CU_ASSERT_EQUAL(ret, 0);
        ret = tree_sequence_get_node(ts2, j, &n2);
        CU_ASSERT_EQUAL(ret, 0);
        verify_nodes_equal(&n1, &n2);
    }
    for (j = 0; j < tree_sequence_get_num_edges(ts1); j++) {
        ret = tree_sequence_get_edge(ts1, j, &r1);
        CU_ASSERT_EQUAL(ret, 0);
        ret = tree_sequence_get_edge(ts2, j, &r2);
        CU_ASSERT_EQUAL(ret, 0);
        verify_edges_equal(&r1, &r2, 1.0);
    }
    if (check_mutations) {
        CU_ASSERT_EQUAL_FATAL(
            tree_sequence_get_num_sites(ts1),
            tree_sequence_get_num_sites(ts2));
        for (j = 0; j < tree_sequence_get_num_sites(ts1); j++) {
            ret = tree_sequence_get_site(ts1, j, &site_1);
            CU_ASSERT_EQUAL(ret, 0);
            ret = tree_sequence_get_site(ts2, j, &site_2);
            CU_ASSERT_EQUAL(ret, 0);
            CU_ASSERT_EQUAL(site_1.position, site_2.position);
            CU_ASSERT_EQUAL(site_1.ancestral_state_length, site_2.ancestral_state_length);
            CU_ASSERT_NSTRING_EQUAL(site_1.ancestral_state, site_2.ancestral_state,
                    site_1.ancestral_state_length);
            CU_ASSERT_EQUAL(site_1.metadata_length, site_2.metadata_length);
            CU_ASSERT_NSTRING_EQUAL(site_1.metadata, site_2.metadata,
                    site_1.metadata_length);
        }
        CU_ASSERT_EQUAL_FATAL(
            tree_sequence_get_num_mutations(ts1),
            tree_sequence_get_num_mutations(ts2));
        for (j = 0; j < num_mutations; j++) {
            ret = tree_sequence_get_mutation(ts1, j, &mutation_1);
            CU_ASSERT_EQUAL(ret, 0);
            ret = tree_sequence_get_mutation(ts2, j, &mutation_2);
            CU_ASSERT_EQUAL(ret, 0);
            CU_ASSERT_EQUAL(mutation_1.id, j);
            CU_ASSERT_EQUAL(mutation_1.id, mutation_2.id);
            CU_ASSERT_EQUAL(mutation_1.index, j);
            CU_ASSERT_EQUAL(mutation_1.index, mutation_2.index);
            CU_ASSERT_EQUAL(mutation_1.site, mutation_2.site);
            CU_ASSERT_EQUAL(mutation_1.node, mutation_2.node);
            CU_ASSERT_EQUAL_FATAL(mutation_1.parent, mutation_2.parent);
            CU_ASSERT_EQUAL_FATAL(mutation_1.derived_state_length,
                    mutation_2.derived_state_length);
            CU_ASSERT_NSTRING_EQUAL(mutation_1.derived_state,
                    mutation_2.derived_state, mutation_1.derived_state_length);
            CU_ASSERT_EQUAL_FATAL(mutation_1.metadata_length,
                    mutation_2.metadata_length);
            CU_ASSERT_NSTRING_EQUAL(mutation_1.metadata,
                    mutation_2.metadata, mutation_1.metadata_length);
        }
    }
    if (check_migrations) {
        CU_ASSERT_EQUAL_FATAL(
            tree_sequence_get_num_migrations(ts1),
            tree_sequence_get_num_migrations(ts2));
        for (j = 0; j < tree_sequence_get_num_migrations(ts1); j++) {
            ret = tree_sequence_get_migration(ts1, j, &m1);
            CU_ASSERT_EQUAL(ret, 0);
            ret = tree_sequence_get_migration(ts2, j, &m2);
            CU_ASSERT_EQUAL(ret, 0);
            verify_migrations_equal(&m1, &m2, 1.0);
        }
    }
    if (check_provenance) {
        CU_ASSERT_EQUAL_FATAL(
            tree_sequence_get_num_provenances(ts1),
            tree_sequence_get_num_provenances(ts2));
        for (j = 0; j < tree_sequence_get_num_provenances(ts1); j++) {
            ret = tree_sequence_get_provenance(ts1, j, &p1);
            CU_ASSERT_EQUAL(ret, 0);
            ret = tree_sequence_get_provenance(ts2, j, &p2);
            CU_ASSERT_EQUAL(ret, 0);
            verify_provenances_equal(&p1, &p2);
        }
    }

    CU_ASSERT_EQUAL_FATAL(
        tree_sequence_get_num_individuals(ts1),
        tree_sequence_get_num_individuals(ts2));
    for (j = 0; j < tree_sequence_get_num_individuals(ts1); j++) {
        ret = tree_sequence_get_individual(ts1, j, &i1);
        CU_ASSERT_EQUAL(ret, 0);
        ret = tree_sequence_get_individual(ts2, j, &i2);
        CU_ASSERT_EQUAL(ret, 0);
        verify_individuals_equal(&i1, &i2);
    }

    CU_ASSERT_EQUAL_FATAL(
        tree_sequence_get_num_populations(ts1),
        tree_sequence_get_num_populations(ts2));
    for (j = 0; j < tree_sequence_get_num_populations(ts1); j++) {
        ret = tree_sequence_get_population(ts1, j, &pop1);
        CU_ASSERT_EQUAL(ret, 0);
        ret = tree_sequence_get_population(ts2, j, &pop2);
        CU_ASSERT_EQUAL(ret, 0);
        verify_populations_equal(&pop1, &pop2);
    }

    ret = sparse_tree_alloc(&t1, ts1, 0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = sparse_tree_alloc(&t2, ts2, 0);
    CU_ASSERT_EQUAL(ret, 0);
    ret = sparse_tree_first(&t1);
    CU_ASSERT_EQUAL(ret, 1);
    ret = sparse_tree_first(&t2);
    CU_ASSERT_EQUAL(ret, 1);
    while (1) {
        err1 = sparse_tree_next(&t1);
        err2 = sparse_tree_next(&t2);
        CU_ASSERT_EQUAL_FATAL(err1, err2);
        if (err1 != 1) {
            break;
        }
    }
    sparse_tree_free(&t1);
    sparse_tree_free(&t2);
}

static void
test_save_empty_kas(void)
{
    int ret;
    tree_sequence_t ts1, ts2;
    double sequence_length = 1234.00;
    table_collection_t tables;

    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    tables.sequence_length = sequence_length;

    ret = tree_sequence_load_tables(&ts1, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    ret = tree_sequence_dump(&ts1, _tmp_file_name, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    verify_empty_tree_sequence(&ts1, sequence_length);
    ret = tree_sequence_load(&ts2, _tmp_file_name, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    verify_empty_tree_sequence(&ts2, sequence_length);

    tree_sequence_free(&ts1);
    tree_sequence_free(&ts2);
    table_collection_free(&tables);
}

static void
test_save_kas(void)
{
    int ret;
    size_t j, k;
    tree_sequence_t **examples = get_example_tree_sequences(1);
    tree_sequence_t ts2;
    tree_sequence_t *ts1;
    int dump_flags[] = {0, MSP_DUMP_ZLIB_COMPRESSION};

    CU_ASSERT_FATAL(examples != NULL);

    for (j = 0; examples[j] != NULL; j++) {
        ts1 = examples[j];
        for (k = 0; k < sizeof(dump_flags) / sizeof(int); k++) {
            ret = tree_sequence_dump(ts1, _tmp_file_name, dump_flags[k]);
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            ret = tree_sequence_load(&ts2, _tmp_file_name, MSP_LOAD_EXTENDED_CHECKS);
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            verify_tree_sequences_equal(ts1, &ts2, true, true, true);
            tree_sequence_print_state(&ts2, _devnull);
            verify_hapgen(&ts2);
            verify_vargen(&ts2);
            tree_sequence_free(&ts2);
        }
        tree_sequence_free(ts1);
        free(ts1);
    }
    free(examples);
}

static void
test_save_kas_tables(void)
{
    int ret;
    size_t j, k;
    tree_sequence_t **examples = get_example_tree_sequences(1);
    tree_sequence_t ts2;
    tree_sequence_t *ts1;
    table_collection_t t1, t2;
    int dump_flags[] = {0, MSP_DUMP_ZLIB_COMPRESSION};

    CU_ASSERT_FATAL(examples != NULL);

    for (j = 0; examples[j] != NULL; j++) {
        ts1 = examples[j];
        ret = table_collection_alloc(&t1, 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        ret = tree_sequence_dump_tables(ts1, &t1, MSP_ALLOC_TABLES);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        t1.sequence_length = ts1->sequence_length;
        for (k = 0; k < sizeof(dump_flags) / sizeof(int); k++) {
            ret = table_collection_dump(&t1, _tmp_file_name, dump_flags[k]);
            CU_ASSERT_EQUAL_FATAL(ret, 0);

            ret = table_collection_alloc(&t2, 0);
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            ret = table_collection_load(&t2, _tmp_file_name, 0);
            CU_ASSERT_EQUAL_FATAL(ret, 0);

            ret = tree_sequence_load_tables(&ts2, &t2, 0);
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            verify_tree_sequences_equal(ts1, &ts2, true, true, true);

            tree_sequence_free(&ts2);
            table_collection_free(&t2);
        }
        table_collection_free(&t1);
        tree_sequence_free(ts1);
        free(ts1);
    }
    free(examples);
}

static void
test_sort_tables(void)
{
    int ret;
    tree_sequence_t **examples = get_example_tree_sequences(1);
    tree_sequence_t ts2;
    tree_sequence_t *ts1;
    size_t j, k, start, starts[3];
    table_collection_t tables;

    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_FATAL(examples != NULL);

    for (j = 0; examples[j] != NULL; j++) {
        ts1 = examples[j];

        ret = tree_sequence_dump_tables(ts1, &tables, 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);

        /* Check the input validation */
        ret = sort_tables(NULL, &tables.edges, &tables.migrations, &tables.sites,
                &tables.mutations, 0);
        CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_PARAM_VALUE);
        ret = sort_tables(&tables.nodes, NULL, &tables.migrations, &tables.sites,
                &tables.mutations, 0);
        CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_PARAM_VALUE);
        ret = sort_tables(&tables.nodes, &tables.edges, &tables.migrations,
                &tables.sites, NULL, 0);
        CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_PARAM_VALUE);
        ret = sort_tables(&tables.nodes, &tables.edges, &tables.migrations, &tables.sites,
                &tables.mutations, 2 * tables.edges.num_rows);
        CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_OUT_OF_BOUNDS);
        ret = sort_tables(&tables.nodes, &tables.edges, &tables.migrations, &tables.sites,
                &tables.mutations, tables.edges.num_rows + 1);
        CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_OUT_OF_BOUNDS);

        /* Check edge sorting */
        if (tables.edges.num_rows == 2) {
            starts[0] = 0;
            starts[1] = 0;
            starts[2] = 0;
        } else {
            starts[0] = 0;
            starts[1] = tables.edges.num_rows / 2;
            starts[2] = tables.edges.num_rows - 2;
        }
        for (k = 0; k < 3; k++) {
            start = starts[k];
            unsort_edges(&tables.edges, start);
            ret = tree_sequence_load_tables(&ts2, &tables, 0);
            CU_ASSERT_NOT_EQUAL_FATAL(ret, 0);
            tree_sequence_free(&ts2);

            ret = sort_tables(&tables.nodes, &tables.edges, &tables.migrations, &tables.sites, &tables.mutations, start);
            CU_ASSERT_EQUAL_FATAL(ret, 0);

            ret = tree_sequence_load_tables(&ts2, &tables, 0);
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            verify_tree_sequences_equal(ts1, &ts2, true, true, false);
            tree_sequence_free(&ts2);
        }

        /* A start value of num_tables.edges should have no effect */
        ret = sort_tables(&tables.nodes, &tables.edges, &tables.migrations, &tables.sites, &tables.mutations,
                tables.edges.num_rows);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        ret = tree_sequence_load_tables(&ts2, &tables, 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        verify_tree_sequences_equal(ts1, &ts2, true, true, false);
        tree_sequence_free(&ts2);

        if (tables.sites.num_rows > 1) {
            /* Check site sorting */
            unsort_sites(&tables.sites, &tables.mutations);
            ret = tree_sequence_load_tables(&ts2, &tables, 0);
            CU_ASSERT_NOT_EQUAL(ret, 0);
            tree_sequence_free(&ts2);

            ret = sort_tables(&tables.nodes, &tables.edges, &tables.migrations, &tables.sites, &tables.mutations, 0);
            CU_ASSERT_EQUAL_FATAL(ret, 0);

            ret = tree_sequence_load_tables(&ts2, &tables, 0);
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            verify_tree_sequences_equal(ts1, &ts2, true, true, false);
            tree_sequence_free(&ts2);

            /* Check for site bounds error */
            tables.mutations.site[0] = tables.sites.num_rows;
            ret = sort_tables(&tables.nodes, &tables.edges, &tables.migrations, &tables.sites, &tables.mutations, 0);
            CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_OUT_OF_BOUNDS);
            tables.mutations.site[0] = 0;
            ret = sort_tables(&tables.nodes, &tables.edges, &tables.migrations, &tables.sites, &tables.mutations, 0);
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            /* Check for edge node bounds error */
            tables.edges.parent[0] = tables.nodes.num_rows;
            ret = sort_tables(&tables.nodes, &tables.edges, &tables.migrations, &tables.sites, &tables.mutations, 0);
            CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_OUT_OF_BOUNDS);
            tables.edges.parent[0] = 0;
            ret = sort_tables(&tables.nodes, &tables.edges, &tables.migrations, &tables.sites, &tables.mutations, 0);
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            /* Check for mutation node bounds error */
            tables.mutations.node[0] = tables.nodes.num_rows;
            ret = sort_tables(&tables.nodes, &tables.edges, &tables.migrations, &tables.sites, &tables.mutations, 0);
            CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_OUT_OF_BOUNDS);
            tables.mutations.node[0] = 0;
            /* Check for mutation parent bounds error */
            tables.mutations.parent[0] = tables.mutations.num_rows;
            ret = sort_tables(&tables.nodes, &tables.edges, &tables.migrations, &tables.sites, &tables.mutations, 0);
            CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_MUTATION_OUT_OF_BOUNDS);
            tables.mutations.parent[0] = MSP_NULL_MUTATION;
            ret = sort_tables(&tables.nodes, &tables.edges, &tables.migrations, &tables.sites, &tables.mutations, 0);
            CU_ASSERT_EQUAL_FATAL(ret, 0);
        }
        tree_sequence_free(ts1);
        free(ts1);
    }
    free(examples);
    table_collection_free(&tables);
}

static void
test_deduplicate_sites(void)
{
    int ret;
    // Modified from paper_ex
    const char *tidy_sites =
        "1      0\n"
        "4.5    0\n"
        "8.5    0\n";
    const char *tidy_mutations =
        "0      2   1\n"
        "0      1   2\n"
        "0      6   3\n"
        "0      3   4\n"
        "1      0   1\n"
        "1      2   2\n"
        "1      4   3\n"
        "1      5   4\n"
        "2      5   1\n"
        "2      7   2\n"
        "2      1   3\n"
        "2      0   4\n";
    const char *messy_sites =
        "1      0\n"
        "1      0\n"
        "1      0\n"
        "1      0\n"
        "4.5    0\n"
        "4.5    0\n"
        "4.5    0\n"
        "4.5    0\n"
        "8.5    0\n"
        "8.5    0\n"
        "8.5    0\n"
        "8.5    0\n";
    const char *messy_mutations =
        "0      2   1\n"
        "1      1   2\n"
        "2      6   3\n"
        "3      3   4\n"
        "4      0   1\n"
        "5      2   2\n"
        "6      4   3\n"
        "7      5   4\n"
        "8      5   1\n"
        "9      7   2\n"
        "10     1   3\n"
        "11     0   4\n";
    table_collection_t tidy, messy;

    ret = table_collection_alloc(&tidy, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_alloc(&messy, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    parse_sites(tidy_sites, &tidy.sites);
    parse_mutations(tidy_mutations, &tidy.mutations);
    // test cleaning doesn't mess up the tidy one
    parse_sites(tidy_sites, &messy.sites);
    parse_mutations(tidy_mutations, &messy.mutations);

    ret = table_collection_deduplicate_sites(&messy, 0);
    CU_ASSERT_EQUAL(ret, 0);
    verify_site_tables_equal(&tidy.sites, &messy.sites);
    verify_mutation_tables_equal(&tidy.mutations, &messy.mutations);

    site_table_clear(&messy.sites);
    mutation_table_clear(&messy.mutations);

    // test with the actual messy one
    parse_sites(messy_sites, &messy.sites);
    parse_mutations(messy_mutations, &messy.mutations);

    ret = table_collection_deduplicate_sites(&messy, 0);
    CU_ASSERT_EQUAL(ret, 0);
    verify_site_tables_equal(&tidy.sites, &messy.sites);
    verify_mutation_tables_equal(&tidy.mutations, &messy.mutations);

    table_collection_free(&tidy);
    table_collection_free(&messy);
}

static void
test_deduplicate_sites_errors(void)
{
    int ret;
    table_collection_t tables;

    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    ret = site_table_add_row(&tables.sites, 2, "A", 1, "m", 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = site_table_add_row(&tables.sites, 2, "TT", 2, "MM", 2);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    ret = mutation_table_add_row(&tables.mutations, 0, 0, -1,
            "T", 1, NULL, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Negative position */
    tables.sites.position[0] = -1;
    ret = table_collection_deduplicate_sites(&tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_SITE_POSITION);
    tables.sites.position[0] = 2;

    /* unsorted position */
    tables.sites.position[1] = 0.5;
    ret = table_collection_deduplicate_sites(&tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_UNSORTED_SITES);
    tables.sites.position[1] = 2;

    /* negative site ID */
    tables.mutations.site[0] = -1;
    ret = table_collection_deduplicate_sites(&tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_SITE_OUT_OF_BOUNDS);
    tables.mutations.site[0] = 0;

     /* site ID out of bounds */
    tables.mutations.site[0] = 2;
    ret = table_collection_deduplicate_sites(&tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_SITE_OUT_OF_BOUNDS);
    tables.mutations.site[0] = 0;

    /* Bad offset in metadata */
    tables.sites.metadata_offset[0] = 2;
    ret = table_collection_deduplicate_sites(&tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_OFFSET);
    tables.sites.metadata_offset[0] = 0;

    /* Bad length in metadata */
    tables.sites.metadata_offset[2] = 100;
    ret = table_collection_deduplicate_sites(&tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_OFFSET);
    tables.sites.metadata_offset[2] = 3;

    /* Bad offset in ancestral_state */
    tables.sites.ancestral_state_offset[0] = 2;
    ret = table_collection_deduplicate_sites(&tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_OFFSET);
    tables.sites.ancestral_state_offset[0] = 0;

    /* Bad length in ancestral_state */
    tables.sites.ancestral_state_offset[2] = 100;
    ret = table_collection_deduplicate_sites(&tables, 0);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_OFFSET);
    tables.sites.ancestral_state_offset[2] = 3;

    ret = table_collection_deduplicate_sites(&tables, 0);
    CU_ASSERT_EQUAL(ret, 0);

    table_collection_free(&tables);
}

static void
test_dump_tables(void)
{
    int ret;
    tree_sequence_t **examples = get_example_tree_sequences(1);
    tree_sequence_t ts2;
    tree_sequence_t *ts1;
    table_collection_t tables;
    size_t j;

    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_FATAL(examples != NULL);

    for (j = 0; examples[j] != NULL; j++) {
        ts1 = examples[j];

        ret = tree_sequence_dump_tables(ts1, NULL, 0);
        CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_PARAM_VALUE);

        ret = tree_sequence_dump_tables(ts1, &tables, 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        ret = tree_sequence_load_tables(&ts2, &tables, 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        verify_tree_sequences_equal(ts1, &ts2, true, true, true);
        tree_sequence_print_state(&ts2, _devnull);
        tree_sequence_free(&ts2);
        tree_sequence_free(ts1);
        free(ts1);
    }

    free(examples);
    table_collection_free(&tables);
}

static void
test_dump_tables_kas(void)
{
    int ret;
    size_t k;
    tree_sequence_t *ts1, ts2, ts3, **examples;
    table_collection_t tables;

    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    examples = get_example_tree_sequences(1);
    for (k = 0; examples[k] != NULL; k++) {
        ts1 = examples[k];
        CU_ASSERT_FATAL(ts1 != NULL);
        ret = tree_sequence_dump_tables(ts1, &tables, 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        ret = tree_sequence_load_tables(&ts2, &tables, 0);
        ret = tree_sequence_dump(&ts2, _tmp_file_name, 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        ret = tree_sequence_load(&ts3, _tmp_file_name, MSP_LOAD_EXTENDED_CHECKS);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        verify_tree_sequences_equal(ts1, &ts3, true, true, true);
        tree_sequence_print_state(&ts2, _devnull);

        tree_sequence_free(&ts2);
        tree_sequence_free(&ts3);
        tree_sequence_free(ts1);
        free(ts1);
    }
    free(examples);
    table_collection_free(&tables);
}


static void
test_strerror(void)
{
    int ret;
    int j;
    const char *msg;
    int max_error_code = 1024; /* totally arbitrary */
    tree_sequence_t ts;

    for (j = 0; j < max_error_code; j++) {
        msg = msp_strerror(-j);
        CU_ASSERT_FATAL(msg != NULL);
        CU_ASSERT(strlen(msg) > 0);
    }

    /* Provoke an IO error error */
    ret = tree_sequence_load(&ts, "/file/does/not/exist", 0);
    CU_ASSERT(msp_is_kas_error(ret));
    CU_ASSERT_EQUAL(ret, KAS_ERR_IO ^ (1 << MSP_KAS_ERR_BIT));
    msg = msp_strerror(ret);
    CU_ASSERT_FATAL(msg != NULL);
    CU_ASSERT(strlen(msg) > 0);
}

static void
test_node_table(void)
{
    int ret;
    node_table_t table;
    size_t num_rows = 100;
    size_t j;
    uint32_t *flags;
    population_id_t *population;
    double *time;
    individual_id_t *individual;
    char *metadata;
    uint32_t *metadata_offset;
    const char *test_metadata = "test";
    size_t test_metadata_length = 4;
    char metadata_copy[test_metadata_length + 1];

    metadata_copy[test_metadata_length] = '\0';
    ret = node_table_alloc(&table, 1, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    node_table_print_state(&table, _devnull);
    node_table_dump_text(&table, _devnull);

    for (j = 0; j < num_rows; j++) {
        ret = node_table_add_row(&table, j, j, j, j, test_metadata, test_metadata_length);
        CU_ASSERT_EQUAL_FATAL(ret, j);
        CU_ASSERT_EQUAL(table.flags[j], j);
        CU_ASSERT_EQUAL(table.time[j], j);
        CU_ASSERT_EQUAL(table.population[j], j);
        CU_ASSERT_EQUAL(table.individual[j], j);
        CU_ASSERT_EQUAL(table.num_rows, j + 1);
        CU_ASSERT_EQUAL(table.metadata_length, (j + 1) * test_metadata_length);
        CU_ASSERT_EQUAL(table.metadata_offset[j + 1], table.metadata_length);
        /* check the metadata */
        memcpy(metadata_copy, table.metadata + table.metadata_offset[j], test_metadata_length);
        CU_ASSERT_NSTRING_EQUAL(metadata_copy, test_metadata, test_metadata_length);
    }
    node_table_print_state(&table, _devnull);
    node_table_dump_text(&table, _devnull);

    node_table_clear(&table);
    CU_ASSERT_EQUAL(table.num_rows, 0);
    CU_ASSERT_EQUAL(table.metadata_length, 0);

    num_rows *= 2;
    flags = malloc(num_rows * sizeof(uint32_t));
    CU_ASSERT_FATAL(flags != NULL);
    memset(flags, 1, num_rows * sizeof(uint32_t));
    population = malloc(num_rows * sizeof(uint32_t));
    CU_ASSERT_FATAL(population != NULL);
    memset(population, 2, num_rows * sizeof(uint32_t));
    time = malloc(num_rows * sizeof(double));
    CU_ASSERT_FATAL(time != NULL);
    memset(time, 0, num_rows * sizeof(double));
    individual = malloc(num_rows * sizeof(uint32_t));
    CU_ASSERT_FATAL(individual != NULL);
    memset(individual, 3, num_rows * sizeof(uint32_t));
    metadata = malloc(num_rows * sizeof(char));
    memset(metadata, 'a', num_rows * sizeof(char));
    CU_ASSERT_FATAL(metadata != NULL);
    metadata_offset = malloc((num_rows + 1) * sizeof(table_size_t));
    CU_ASSERT_FATAL(metadata_offset != NULL);
    for (j = 0; j < num_rows + 1; j++) {
        metadata_offset[j] = j;
    }
    ret = node_table_set_columns(&table, num_rows, flags, time, population,
            individual, metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.flags, flags, num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.population, population, num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.time, time, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.individual, individual, num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata, metadata, num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata_offset, metadata_offset,
                (num_rows + 1) * sizeof(table_size_t)), 0);
    CU_ASSERT_EQUAL(table.num_rows, num_rows);
    CU_ASSERT_EQUAL(table.metadata_length, num_rows);
    node_table_print_state(&table, _devnull);
    node_table_dump_text(&table, _devnull);

    /* Append another num_rows onto the end */
    ret = node_table_append_columns(&table, num_rows, flags, time, population,
            individual, metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.flags, flags, num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.flags + num_rows, flags, num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.population, population, num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.population + num_rows, population,
                num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.time, time, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.time + num_rows, time, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.individual, individual, num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.individual + num_rows, individual,
                num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata, metadata, num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata + num_rows, metadata, num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(table.num_rows, 2 * num_rows);
    CU_ASSERT_EQUAL(table.metadata_length, 2 * num_rows);
    node_table_print_state(&table, _devnull);
    node_table_dump_text(&table, _devnull);

    /* If population is NULL it should be set to -1. If metadata is NULL all metadatas
     * should be set to the empty string. If individual is NULL it should be set to -1. */
    num_rows = 10;
    memset(population, 0xff, num_rows * sizeof(uint32_t));
    memset(individual, 0xff, num_rows * sizeof(uint32_t));
    ret = node_table_set_columns(&table, num_rows, flags, time, NULL, NULL, metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.flags, flags, num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.population, population, num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.time, time, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.individual, individual, num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata, metadata, num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata_offset, metadata_offset, num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(table.num_rows, num_rows);
    CU_ASSERT_EQUAL(table.metadata_length, num_rows);

    /* flags and time cannot be NULL */
    ret = node_table_set_columns(&table, num_rows, NULL, time, population, individual,
            metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = node_table_set_columns(&table, num_rows, flags, NULL, population, individual,
            metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = node_table_set_columns(&table, num_rows, flags, time, population, individual,
            NULL, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = node_table_set_columns(&table, num_rows, flags, time, population, individual,
            metadata, NULL);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);

    /* if metadata and metadata_offset are both null, all metadatas are zero length */
    num_rows = 10;
    memset(metadata_offset, 0, (num_rows + 1) * sizeof(table_size_t));
    ret = node_table_set_columns(&table, num_rows, flags, time, NULL, NULL, NULL, NULL);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.flags, flags, num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.time, time, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata_offset, metadata_offset,
                (num_rows + 1) * sizeof(table_size_t)), 0);
    CU_ASSERT_EQUAL(table.num_rows, num_rows);
    CU_ASSERT_EQUAL(table.metadata_length, 0);
    ret = node_table_append_columns(&table, num_rows, flags, time, NULL, NULL, NULL, NULL);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.flags, flags, num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.flags + num_rows, flags, num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.time, time, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.time + num_rows, time, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata_offset, metadata_offset,
                (num_rows + 1) * sizeof(table_size_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata_offset + num_rows, metadata_offset,
                num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(table.num_rows, 2 * num_rows);
    CU_ASSERT_EQUAL(table.metadata_length, 0);
    node_table_print_state(&table, _devnull);
    node_table_dump_text(&table, _devnull);

    node_table_free(&table);
    free(flags);
    free(population);
    free(time);
    free(metadata);
    free(metadata_offset);
    free(individual);
}

static void
test_edge_table(void)
{
    int ret;
    edge_table_t table;
    size_t num_rows = 100;
    size_t j;
    node_id_t *parent, *child;
    double *left, *right;

    ret = edge_table_alloc(&table, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    edge_table_print_state(&table, _devnull);
    edge_table_dump_text(&table, _devnull);

    for (j = 0; j < num_rows; j++) {
        ret = edge_table_add_row(&table, j, j, j, j);
        CU_ASSERT_EQUAL_FATAL(ret, j);
        CU_ASSERT_EQUAL(table.left[j], j);
        CU_ASSERT_EQUAL(table.right[j], j);
        CU_ASSERT_EQUAL(table.parent[j], j);
        CU_ASSERT_EQUAL(table.child[j], j);
        CU_ASSERT_EQUAL(table.num_rows, j + 1);
    }
    edge_table_print_state(&table, _devnull);
    edge_table_dump_text(&table, _devnull);

    num_rows *= 2;
    left = malloc(num_rows * sizeof(double));
    CU_ASSERT_FATAL(left != NULL);
    memset(left, 0, num_rows * sizeof(double));
    right = malloc(num_rows * sizeof(double));
    CU_ASSERT_FATAL(right != NULL);
    memset(right, 0, num_rows * sizeof(double));
    parent = malloc(num_rows * sizeof(node_id_t));
    CU_ASSERT_FATAL(parent != NULL);
    memset(parent, 1, num_rows * sizeof(node_id_t));
    child = malloc(num_rows * sizeof(node_id_t));
    CU_ASSERT_FATAL(child != NULL);
    memset(child, 1, num_rows * sizeof(node_id_t));

    ret = edge_table_set_columns(&table, num_rows, left, right, parent, child);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.left, left, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.right, right, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.parent, parent, num_rows * sizeof(node_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.child, child, num_rows * sizeof(node_id_t)), 0);
    CU_ASSERT_EQUAL(table.num_rows, num_rows);
    /* Append another num_rows to the end. */
    ret = edge_table_append_columns(&table, num_rows, left, right, parent, child);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.left, left, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.left + num_rows, left, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.right, right, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.right + num_rows, right, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.parent, parent, num_rows * sizeof(node_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.parent + num_rows, parent, num_rows * sizeof(node_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.child, child, num_rows * sizeof(node_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.child + num_rows, child, num_rows * sizeof(node_id_t)), 0);
    CU_ASSERT_EQUAL(table.num_rows, 2 * num_rows);

    /* Inputs cannot be NULL */
    ret = edge_table_set_columns(&table, num_rows, NULL, right, parent, child);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = edge_table_set_columns(&table, num_rows, left, NULL, parent, child);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = edge_table_set_columns(&table, num_rows, left, right, NULL, child);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = edge_table_set_columns(&table, num_rows, left, right, parent, NULL);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);

    edge_table_clear(&table);
    CU_ASSERT_EQUAL(table.num_rows, 0);

    edge_table_free(&table);
    free(left);
    free(right);
    free(parent);
    free(child);
}

static void
test_site_table(void)
{
    int ret;
    site_table_t table;
    size_t num_rows, j;
    char *ancestral_state;
    char *metadata;
    double *position;
    table_size_t *ancestral_state_offset;
    table_size_t *metadata_offset;

    ret = site_table_alloc(&table, 1, 1, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    site_table_print_state(&table, _devnull);
    site_table_dump_text(&table, _devnull);

    ret = site_table_add_row(&table, 0, "A", 1, NULL, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(table.position[0], 0);
    CU_ASSERT_EQUAL(table.ancestral_state_offset[0], 0);
    CU_ASSERT_EQUAL(table.ancestral_state_offset[1], 1);
    CU_ASSERT_EQUAL(table.ancestral_state_length, 1);
    CU_ASSERT_EQUAL(table.metadata_offset[0], 0);
    CU_ASSERT_EQUAL(table.metadata_offset[1], 0);
    CU_ASSERT_EQUAL(table.metadata_length, 0);
    CU_ASSERT_EQUAL(table.num_rows, 1);

    ret = site_table_add_row(&table, 1, "AA", 2, "{}", 2);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL(table.position[1], 1);
    CU_ASSERT_EQUAL(table.ancestral_state_offset[2], 3);
    CU_ASSERT_EQUAL(table.metadata_offset[1], 0);
    CU_ASSERT_EQUAL(table.metadata_offset[2], 2);
    CU_ASSERT_EQUAL(table.metadata_length, 2);
    CU_ASSERT_EQUAL(table.num_rows, 2);

    ret = site_table_add_row(&table, 2, "A", 1, "metadata", 8);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    CU_ASSERT_EQUAL(table.position[1], 1);
    CU_ASSERT_EQUAL(table.ancestral_state_offset[3], 4);
    CU_ASSERT_EQUAL(table.ancestral_state_length, 4);
    CU_ASSERT_EQUAL(table.metadata_offset[3], 10);
    CU_ASSERT_EQUAL(table.metadata_length, 10);
    CU_ASSERT_EQUAL(table.num_rows, 3);

    site_table_print_state(&table, _devnull);
    site_table_dump_text(&table, _devnull);
    site_table_clear(&table);
    CU_ASSERT_EQUAL(table.num_rows, 0);
    CU_ASSERT_EQUAL(table.ancestral_state_length, 0);
    CU_ASSERT_EQUAL(table.metadata_length, 0);
    CU_ASSERT_EQUAL(table.ancestral_state_offset[0], 0);
    CU_ASSERT_EQUAL(table.metadata_offset[0], 0);

    num_rows = 100;
    position = malloc(num_rows * sizeof(double));
    CU_ASSERT_FATAL(position != NULL);
    ancestral_state = malloc(num_rows * sizeof(char));
    CU_ASSERT_FATAL(ancestral_state != NULL);
    ancestral_state_offset = malloc((num_rows + 1) * sizeof(uint32_t));
    CU_ASSERT_FATAL(ancestral_state_offset != NULL);
    metadata = malloc(num_rows * sizeof(char));
    CU_ASSERT_FATAL(metadata != NULL);
    metadata_offset = malloc((num_rows + 1) * sizeof(uint32_t));
    CU_ASSERT_FATAL(metadata_offset != NULL);

    for (j = 0; j < num_rows; j++) {
        position[j] = j;
        ancestral_state[j] = j;
        ancestral_state_offset[j] = j;
        metadata[j] = 'A' + j;
        metadata_offset[j] = j;
    }
    ancestral_state_offset[num_rows] = num_rows;
    metadata_offset[num_rows] = num_rows;

    ret = site_table_set_columns(&table, num_rows, position,
            ancestral_state, ancestral_state_offset,
            metadata, metadata_offset);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.position, position,
                num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.ancestral_state, ancestral_state,
                num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(table.ancestral_state_length, num_rows);
    CU_ASSERT_EQUAL(memcmp(table.metadata, metadata, num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(table.metadata_length, num_rows);
    CU_ASSERT_EQUAL(table.num_rows, num_rows);

    /* Append another num rows */
    ret = site_table_append_columns(&table, num_rows, position, ancestral_state,
            ancestral_state_offset, metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.position, position,
                num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.position + num_rows, position,
                num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.ancestral_state, ancestral_state,
                num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(memcmp(table.ancestral_state + num_rows, ancestral_state,
                num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata, metadata,
                num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata + num_rows, metadata,
                num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(table.num_rows, 2 * num_rows);
    CU_ASSERT_EQUAL(table.ancestral_state_length, 2 * num_rows);

    /* Inputs cannot be NULL */
    ret = site_table_set_columns(&table, num_rows, NULL, ancestral_state,
            ancestral_state_offset, metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = site_table_set_columns(&table, num_rows, position, NULL, ancestral_state_offset,
            metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = site_table_set_columns(&table, num_rows, position, ancestral_state, NULL,
            metadata, metadata_offset);
    /* Metadata and metadata_offset must both be null */
    ret = site_table_set_columns(&table, num_rows, position, ancestral_state,
            ancestral_state_offset, NULL, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = site_table_set_columns(&table, num_rows, position, ancestral_state,
            ancestral_state_offset, metadata, NULL);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);

    /* Set metadata to NULL */
    ret = site_table_set_columns(&table, num_rows, position,
            ancestral_state, ancestral_state_offset, NULL, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    memset(metadata_offset, 0, (num_rows + 1) * sizeof(uint32_t));
    CU_ASSERT_EQUAL(memcmp(table.position, position,
                num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.ancestral_state, ancestral_state,
                num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(table.ancestral_state_length, num_rows);
    CU_ASSERT_EQUAL(memcmp(table.metadata_offset, metadata_offset,
                (num_rows + 1) * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(table.metadata_length, 0);
    CU_ASSERT_EQUAL(table.num_rows, num_rows);

    /* Test for bad offsets */
    ancestral_state_offset[0] = 1;
    ret = site_table_set_columns(&table, num_rows, position,
            ancestral_state, ancestral_state_offset,
            metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_OFFSET);
    ancestral_state_offset[0] = 0;
    ancestral_state_offset[num_rows] = 0;
    ret = site_table_set_columns(&table, num_rows, position,
            ancestral_state, ancestral_state_offset,
            metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_OFFSET);
    ancestral_state_offset[0] = 0;

    metadata_offset[0] = 0;
    ret = site_table_set_columns(&table, num_rows, position,
            ancestral_state, ancestral_state_offset,
            metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_OFFSET);
    metadata_offset[0] = 0;
    metadata_offset[num_rows] = 0;
    ret = site_table_set_columns(&table, num_rows, position,
            ancestral_state, ancestral_state_offset,
            metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_OFFSET);

    ret = site_table_clear(&table);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(table.num_rows, 0);
    CU_ASSERT_EQUAL(table.ancestral_state_length, 0);
    CU_ASSERT_EQUAL(table.metadata_length, 0);

    site_table_free(&table);
    free(position);
    free(ancestral_state);
    free(ancestral_state_offset);
    free(metadata);
    free(metadata_offset);
}

static void
test_mutation_table(void)
{
    int ret;
    mutation_table_t table;
    size_t num_rows = 100;
    size_t max_len = 20;
    size_t j, k, len;
    node_id_t *node;
    mutation_id_t *parent;
    site_id_t *site;
    char *derived_state, *metadata;
    char c[max_len + 1];
    table_size_t *derived_state_offset, *metadata_offset;

    for (j = 0; j < max_len; j++) {
        c[j] = 'A' + j;
    }

    ret = mutation_table_alloc(&table, 1, 1, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    mutation_table_print_state(&table, _devnull);
    mutation_table_dump_text(&table, _devnull);

    len = 0;
    for (j = 0; j < num_rows; j++) {
        k = GSL_MIN(j + 1, max_len);
        ret = mutation_table_add_row(&table, j, j, j, c, k, c, k);
        CU_ASSERT_EQUAL_FATAL(ret, j);
        CU_ASSERT_EQUAL(table.site[j], j);
        CU_ASSERT_EQUAL(table.node[j], j);
        CU_ASSERT_EQUAL(table.parent[j], j);
        CU_ASSERT_EQUAL(table.derived_state_offset[j], len);
        CU_ASSERT_EQUAL(table.metadata_offset[j], len);
        CU_ASSERT_EQUAL(table.num_rows, j + 1);
        len += k;
        CU_ASSERT_EQUAL(table.derived_state_offset[j + 1], len);
        CU_ASSERT_EQUAL(table.derived_state_length, len);
        CU_ASSERT_EQUAL(table.metadata_offset[j + 1], len);
        CU_ASSERT_EQUAL(table.metadata_length, len);
    }
    mutation_table_print_state(&table, _devnull);
    mutation_table_dump_text(&table, _devnull);

    num_rows *= 2;
    site = malloc(num_rows * sizeof(site_id_t));
    CU_ASSERT_FATAL(site != NULL);
    node = malloc(num_rows * sizeof(node_id_t));
    CU_ASSERT_FATAL(node != NULL);
    parent = malloc(num_rows * sizeof(mutation_id_t));
    CU_ASSERT_FATAL(parent != NULL);
    derived_state = malloc(num_rows * sizeof(char));
    CU_ASSERT_FATAL(derived_state != NULL);
    derived_state_offset = malloc((num_rows + 1) * sizeof(table_size_t));
    CU_ASSERT_FATAL(derived_state_offset != NULL);
    metadata = malloc(num_rows * sizeof(char));
    CU_ASSERT_FATAL(metadata != NULL);
    metadata_offset = malloc((num_rows + 1) * sizeof(table_size_t));
    CU_ASSERT_FATAL(metadata_offset != NULL);

    for (j = 0; j < num_rows; j++) {
        node[j] = j;
        site[j] = j + 1;
        parent[j] = j + 2;
        derived_state[j] = 'Y';
        derived_state_offset[j] = j;
        metadata[j] = 'M';
        metadata_offset[j] = j;
    }

    derived_state_offset[num_rows] = num_rows;
    metadata_offset[num_rows] = num_rows;
    ret = mutation_table_set_columns(&table, num_rows, site, node, parent,
            derived_state, derived_state_offset, metadata, metadata_offset);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.site, site, num_rows * sizeof(site_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.node, node, num_rows * sizeof(node_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.parent, parent, num_rows * sizeof(mutation_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.derived_state, derived_state,
                num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata, metadata, num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(table.num_rows, num_rows);
    CU_ASSERT_EQUAL(table.derived_state_length, num_rows);
    CU_ASSERT_EQUAL(table.metadata_length, num_rows);

    /* Append another num_rows */
    ret = mutation_table_append_columns(&table, num_rows, site, node, parent, derived_state,
            derived_state_offset, metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.site, site, num_rows * sizeof(site_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.site + num_rows, site, num_rows * sizeof(site_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.node, node, num_rows * sizeof(node_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.node + num_rows, node, num_rows * sizeof(node_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.parent, parent, num_rows * sizeof(mutation_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.parent + num_rows, parent,
                num_rows * sizeof(mutation_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.derived_state, derived_state,
                num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(memcmp(table.derived_state, derived_state,
                num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(table.derived_state_length, 2 * num_rows);
    CU_ASSERT_EQUAL(memcmp(table.metadata, metadata,
                num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata, metadata,
                num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(table.metadata_length, 2 * num_rows);
    CU_ASSERT_EQUAL(table.num_rows, 2 * num_rows);

    /* Check all this again, except with parent == NULL and metadata == NULL. */
    memset(parent, 0xff, num_rows * sizeof(mutation_id_t));
    memset(metadata_offset, 0, (num_rows + 1) * sizeof(table_size_t));
    ret = mutation_table_set_columns(&table, num_rows, site, node, NULL,
            derived_state, derived_state_offset, NULL, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.site, site, num_rows * sizeof(site_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.node, node, num_rows * sizeof(node_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.parent, parent, num_rows * sizeof(mutation_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.derived_state, derived_state,
                num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(memcmp(table.derived_state_offset, derived_state_offset,
                num_rows * sizeof(table_size_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata_offset, metadata_offset,
                (num_rows + 1) * sizeof(table_size_t)), 0);
    CU_ASSERT_EQUAL(table.num_rows, num_rows);
    CU_ASSERT_EQUAL(table.derived_state_length, num_rows);
    CU_ASSERT_EQUAL(table.metadata_length, 0);

    /* Append another num_rows */
    ret = mutation_table_append_columns(&table, num_rows, site, node, NULL, derived_state,
            derived_state_offset, NULL, NULL);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.site, site, num_rows * sizeof(site_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.site + num_rows, site, num_rows * sizeof(site_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.node, node, num_rows * sizeof(node_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.node + num_rows, node, num_rows * sizeof(node_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.parent, parent, num_rows * sizeof(mutation_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.parent + num_rows, parent,
                num_rows * sizeof(mutation_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.derived_state, derived_state,
                num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(memcmp(table.derived_state + num_rows, derived_state,
                num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(table.num_rows, 2 * num_rows);
    CU_ASSERT_EQUAL(table.derived_state_length, 2 * num_rows);
    CU_ASSERT_EQUAL(table.metadata_length, 0);


    /* Inputs except parent, metadata and metadata_offset cannot be NULL*/
    ret = mutation_table_set_columns(&table, num_rows, NULL, node, parent,
            derived_state, derived_state_offset, metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = mutation_table_set_columns(&table, num_rows, site, NULL, parent,
            derived_state, derived_state_offset, metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = mutation_table_set_columns(&table, num_rows, site, node, parent,
            NULL, derived_state_offset, metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = mutation_table_set_columns(&table, num_rows, site, node, parent,
            derived_state, NULL, metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = mutation_table_set_columns(&table, num_rows, site, node, parent,
            derived_state, derived_state_offset, NULL, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = mutation_table_set_columns(&table, num_rows, site, node, parent,
            derived_state, derived_state_offset, metadata, NULL);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);

    /* Inputs except parent, metadata and metadata_offset cannot be NULL*/
    ret = mutation_table_append_columns(&table, num_rows, NULL, node, parent,
            derived_state, derived_state_offset, metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = mutation_table_append_columns(&table, num_rows, site, NULL, parent,
            derived_state, derived_state_offset, metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = mutation_table_append_columns(&table, num_rows, site, node, parent,
            NULL, derived_state_offset, metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = mutation_table_append_columns(&table, num_rows, site, node, parent,
            derived_state, NULL, metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = mutation_table_append_columns(&table, num_rows, site, node, parent,
            derived_state, derived_state_offset, NULL, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = mutation_table_append_columns(&table, num_rows, site, node, parent,
            derived_state, derived_state_offset, metadata, NULL);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);

    /* Test for bad offsets */
    derived_state_offset[0] = 1;
    ret = mutation_table_set_columns(&table, num_rows, site, node, parent,
            derived_state, derived_state_offset, NULL, NULL);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_OFFSET);
    derived_state_offset[0] = 0;
    derived_state_offset[num_rows] = 0;
    ret = mutation_table_set_columns(&table, num_rows, site, node, parent,
            derived_state, derived_state_offset, NULL, NULL);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_OFFSET);

    mutation_table_clear(&table);
    CU_ASSERT_EQUAL(table.num_rows, 0);
    CU_ASSERT_EQUAL(table.derived_state_length, 0);
    CU_ASSERT_EQUAL(table.metadata_length, 0);

    mutation_table_free(&table);
    free(site);
    free(node);
    free(parent);
    free(derived_state);
    free(derived_state_offset);
    free(metadata);
    free(metadata_offset);
}

static void
test_migration_table(void)
{
    int ret;
    migration_table_t table;
    size_t num_rows = 100;
    size_t j;
    node_id_t *node;
    population_id_t *source, *dest;
    double *left, *right, *time;

    ret = migration_table_alloc(&table, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    migration_table_print_state(&table, _devnull);
    migration_table_dump_text(&table, _devnull);

    for (j = 0; j < num_rows; j++) {
        ret = migration_table_add_row(&table, j, j, j, j, j, j);
        CU_ASSERT_EQUAL_FATAL(ret, j);
        CU_ASSERT_EQUAL(table.left[j], j);
        CU_ASSERT_EQUAL(table.right[j], j);
        CU_ASSERT_EQUAL(table.node[j], j);
        CU_ASSERT_EQUAL(table.source[j], j);
        CU_ASSERT_EQUAL(table.dest[j], j);
        CU_ASSERT_EQUAL(table.time[j], j);
        CU_ASSERT_EQUAL(table.num_rows, j + 1);
    }
    migration_table_print_state(&table, _devnull);
    migration_table_dump_text(&table, _devnull);

    num_rows *= 2;
    left = malloc(num_rows * sizeof(double));
    CU_ASSERT_FATAL(left != NULL);
    memset(left, 1, num_rows * sizeof(double));
    right = malloc(num_rows * sizeof(double));
    CU_ASSERT_FATAL(right != NULL);
    memset(right, 2, num_rows * sizeof(double));
    time = malloc(num_rows * sizeof(double));
    CU_ASSERT_FATAL(time != NULL);
    memset(time, 3, num_rows * sizeof(double));
    node = malloc(num_rows * sizeof(node_id_t));
    CU_ASSERT_FATAL(node != NULL);
    memset(node, 4, num_rows * sizeof(node_id_t));
    source = malloc(num_rows * sizeof(population_id_t));
    CU_ASSERT_FATAL(source != NULL);
    memset(source, 5, num_rows * sizeof(population_id_t));
    dest = malloc(num_rows * sizeof(population_id_t));
    CU_ASSERT_FATAL(dest != NULL);
    memset(dest, 6, num_rows * sizeof(population_id_t));

    ret = migration_table_set_columns(&table, num_rows, left, right, node, source,
            dest, time);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.left, left, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.right, right, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.time, time, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.node, node, num_rows * sizeof(node_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.source, source, num_rows * sizeof(population_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.dest, dest, num_rows * sizeof(population_id_t)), 0);
    CU_ASSERT_EQUAL(table.num_rows, num_rows);
    /* Append another num_rows */
    ret = migration_table_append_columns(&table, num_rows, left, right, node, source,
            dest, time);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.left, left, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.left + num_rows, left, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.right, right, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.right + num_rows, right, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.time, time, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.time + num_rows, time, num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.node, node, num_rows * sizeof(node_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.node + num_rows, node, num_rows * sizeof(node_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.source, source, num_rows * sizeof(population_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.source + num_rows, source,
                num_rows * sizeof(population_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.dest, dest, num_rows * sizeof(population_id_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.dest + num_rows, dest,
                num_rows * sizeof(population_id_t)), 0);
    CU_ASSERT_EQUAL(table.num_rows, 2 * num_rows);

    /* inputs cannot be NULL */
    ret = migration_table_set_columns(&table, num_rows, NULL, right, node, source,
            dest, time);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = migration_table_set_columns(&table, num_rows, left, NULL, node, source,
            dest, time);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = migration_table_set_columns(&table, num_rows, left, right, NULL, source,
            dest, time);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = migration_table_set_columns(&table, num_rows, left, right, node, NULL,
            dest, time);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = migration_table_set_columns(&table, num_rows, left, right, node, source,
            NULL, time);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = migration_table_set_columns(&table, num_rows, left, right, node, source,
            dest, NULL);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);

    migration_table_clear(&table);
    CU_ASSERT_EQUAL(table.num_rows, 0);

    migration_table_free(&table);
    free(left);
    free(right);
    free(time);
    free(node);
    free(source);
    free(dest);
}

static void
test_individual_table(void)
{
    int ret = 0;
    individual_table_t table;
    table_collection_t tables, tables2;
    tree_sequence_t ts;
    size_t num_rows = 100;
    size_t j, k;
    uint32_t *flags;
    double *location;
    char *metadata;
    uint32_t *metadata_offset;
    uint32_t *location_offset;

    const char *test_metadata = "test";
    size_t test_metadata_length = 4;
    char metadata_copy[test_metadata_length + 1];
    size_t spatial_dimension = 2;
    double test_location[spatial_dimension];

    for (k = 0; k < spatial_dimension; k++) {
        test_location[k] = (double) k;
    }
    metadata_copy[test_metadata_length] = '\0';
    ret = individual_table_alloc(&table, 1, 1, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    individual_table_print_state(&table, _devnull);

    for (j = 0; j < num_rows; j++) {
        ret = individual_table_add_row(&table, j, test_location, spatial_dimension,
                test_metadata, test_metadata_length);
        CU_ASSERT_EQUAL_FATAL(ret, j);
        CU_ASSERT_EQUAL(table.flags[j], j);
        for (k = 0; k < spatial_dimension; k++) {
            test_location[k] = (double) k;
            CU_ASSERT_EQUAL(table.location[spatial_dimension * j + k], test_location[k]);
        }
        CU_ASSERT_EQUAL(table.metadata_length, (j + 1) * test_metadata_length);
        CU_ASSERT_EQUAL(table.metadata_offset[j + 1], table.metadata_length);
        /* check the metadata */
        memcpy(metadata_copy, table.metadata + table.metadata_offset[j],
                test_metadata_length);
        CU_ASSERT_NSTRING_EQUAL(metadata_copy, test_metadata, test_metadata_length);
    }
    individual_table_print_state(&table, _devnull);
    individual_table_clear(&table);
    CU_ASSERT_EQUAL(table.num_rows, 0);
    CU_ASSERT_EQUAL(table.metadata_length, 0);

    num_rows *= 2;
    flags = malloc(num_rows * sizeof(uint32_t));
    CU_ASSERT_FATAL(flags != NULL);
    memset(flags, 1, num_rows * sizeof(uint32_t));
    location = malloc(spatial_dimension * num_rows * sizeof(double));
    CU_ASSERT_FATAL(location != NULL);
    memset(location, 0, spatial_dimension * num_rows * sizeof(double));
    location_offset = malloc((num_rows + 1) * sizeof(table_size_t));
    CU_ASSERT_FATAL(location_offset != NULL);
    for (j = 0; j < num_rows + 1; j++) {
        location_offset[j] = j * spatial_dimension;
    }
    metadata = malloc(num_rows * sizeof(char));
    memset(metadata, 'a', num_rows * sizeof(char));
    CU_ASSERT_FATAL(metadata != NULL);
    metadata_offset = malloc((num_rows + 1) * sizeof(table_size_t));
    CU_ASSERT_FATAL(metadata_offset != NULL);
    for (j = 0; j < num_rows + 1; j++) {
        metadata_offset[j] = j;
    }
    ret = individual_table_set_columns(&table, num_rows, flags,
            location, location_offset, metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.flags, flags, num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.location, location,
                spatial_dimension * num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.location_offset, location_offset,
                (num_rows + 1) * sizeof(table_size_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata, metadata, num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata_offset, metadata_offset,
                (num_rows + 1) * sizeof(table_size_t)), 0);
    CU_ASSERT_EQUAL(table.num_rows, num_rows);
    CU_ASSERT_EQUAL(table.location_length, spatial_dimension * num_rows);
    CU_ASSERT_EQUAL(table.metadata_length, num_rows);
    individual_table_print_state(&table, _devnull);

    /* Append another num_rows onto the end */
    ret = individual_table_append_columns(&table, num_rows, flags, location,
            location_offset, metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.flags, flags, num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.flags + num_rows, flags, num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata, metadata, num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata + num_rows, metadata, num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(memcmp(table.location, location,
                spatial_dimension * num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.location + spatial_dimension * num_rows,
                location, spatial_dimension * num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(table.num_rows, 2 * num_rows);
    CU_ASSERT_EQUAL(table.metadata_length, 2 * num_rows);
    individual_table_print_state(&table, _devnull);
    individual_table_dump_text(&table, _devnull);

    /* flags can't be NULL */
    ret = individual_table_set_columns(&table, num_rows, NULL,
            location, location_offset, metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    /* location and location offset must be simultaneously NULL or not */
    ret = individual_table_set_columns(&table, num_rows, flags,
            location, NULL, metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = individual_table_set_columns(&table, num_rows, flags,
            NULL, location_offset, metadata, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    /* metadata and metadata offset must be simultaneously NULL or not */
    ret = individual_table_set_columns(&table, num_rows, flags,
            location, location_offset, NULL, metadata_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = individual_table_set_columns(&table, num_rows, flags,
            location, location_offset, metadata, NULL);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);

    /* if location and location_offset are both null, all locations are zero length */
    num_rows = 10;
    memset(location_offset, 0, (num_rows + 1) * sizeof(table_size_t));
    ret = individual_table_set_columns(&table, num_rows, flags,
            NULL, NULL, NULL, NULL);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.location_offset, location_offset,
                (num_rows + 1) * sizeof(table_size_t)), 0);
    CU_ASSERT_EQUAL(table.num_rows, num_rows);
    CU_ASSERT_EQUAL(table.location_length, 0);
    ret = individual_table_append_columns(&table, num_rows, flags, NULL, NULL, NULL, NULL);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.location_offset, location_offset,
                (num_rows + 1) * sizeof(table_size_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.location_offset + num_rows, location_offset,
                num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(table.num_rows, 2 * num_rows);
    CU_ASSERT_EQUAL(table.location_length, 0);
    individual_table_print_state(&table, _devnull);
    individual_table_dump_text(&table, _devnull);

    /* if metadata and metadata_offset are both null, all metadatas are zero length */
    num_rows = 10;
    memset(metadata_offset, 0, (num_rows + 1) * sizeof(table_size_t));
    ret = individual_table_set_columns(&table, num_rows, flags,
            location, location_offset, NULL, NULL);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.flags, flags, num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.location, location,
                spatial_dimension * num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata_offset, metadata_offset,
                (num_rows + 1) * sizeof(table_size_t)), 0);
    CU_ASSERT_EQUAL(table.num_rows, num_rows);
    CU_ASSERT_EQUAL(table.metadata_length, 0);
    ret = individual_table_append_columns(&table, num_rows, flags, location,
            location_offset, NULL, NULL);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.location, location,
                spatial_dimension * num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.location + spatial_dimension * num_rows,
                location, spatial_dimension * num_rows * sizeof(double)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata_offset, metadata_offset,
                (num_rows + 1) * sizeof(table_size_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.metadata_offset + num_rows, metadata_offset,
                num_rows * sizeof(uint32_t)), 0);
    CU_ASSERT_EQUAL(table.num_rows, 2 * num_rows);
    CU_ASSERT_EQUAL(table.metadata_length, 0);
    individual_table_print_state(&table, _devnull);
    individual_table_dump_text(&table, _devnull);

    // Get example from paper
    individual_table_clear(&table);
    parse_individuals(paper_ex_individuals, &table);
    individual_table_print_state(&table, _devnull);
    individual_table_dump_text(&table, _devnull);

    // dump table from tree sequence
    tree_sequence_from_text(&ts, 0, paper_ex_nodes, paper_ex_edges, NULL, NULL, NULL,
            paper_ex_individuals, NULL);
    tree_sequence_dump_tables(&ts, &tables, MSP_ALLOC_TABLES);
    CU_ASSERT_TRUE_FATAL(individual_table_equal(&tables.individuals, &table));
    tree_sequence_free(&ts);

    // copy the table
    individual_table_clear(&table);
    ret = individual_table_copy(&tables.individuals, &table);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_TRUE_FATAL(individual_table_equal(&tables.individuals, &table));

    // Round trip tables -> tree sequence -> tables
    ret = tree_sequence_load_tables(&ts, &tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = tree_sequence_dump_tables(&ts, &tables2, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_TRUE_FATAL(individual_table_equal(&tables2.individuals, &tables.individuals));
    tree_sequence_free(&ts);
    table_collection_free(&tables2);

    // Round trip tables -> kastore -> tables
    ret = table_collection_dump(&tables, _tmp_file_name, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_load(&tables2, _tmp_file_name, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_TRUE_FATAL(individual_table_equal(&tables2.individuals, &tables.individuals));

    ret = table_collection_free(&tables);
    CU_ASSERT_EQUAL(ret, 0);
    ret = table_collection_free(&tables2);
    CU_ASSERT_EQUAL(ret, 0);
    ret = tree_sequence_free(&ts);
    CU_ASSERT_EQUAL(ret, 0);
    ret = individual_table_free(&table);
    CU_ASSERT_EQUAL(ret, 0);
    free(flags);
    free(location);
    free(location_offset);
    free(metadata);
    free(metadata_offset);
}

static void
test_population_table(void)
{
    printf("\n\n\nIMPLEMENT TEST POPULATION TABLE\n\n\n");
}

static void
test_provenance_table(void)
{
    int ret;
    provenance_table_t table;
    size_t num_rows = 100;
    size_t j;
    char *timestamp;
    uint32_t *timestamp_offset;
    const char *test_timestamp = "2017-12-06T20:40:25+00:00";
    size_t test_timestamp_length = strlen(test_timestamp);
    char timestamp_copy[test_timestamp_length + 1];
    char *record;
    uint32_t *record_offset;
    const char *test_record = "{\"json\"=1234}";
    size_t test_record_length = strlen(test_record);
    char record_copy[test_record_length + 1];

    timestamp_copy[test_timestamp_length] = '\0';
    record_copy[test_record_length] = '\0';
    ret = provenance_table_alloc(&table, 1, 1, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    provenance_table_print_state(&table, _devnull);

    for (j = 0; j < num_rows; j++) {
        ret = provenance_table_add_row(&table, test_timestamp, test_timestamp_length,
                test_record, test_record_length);
        CU_ASSERT_EQUAL_FATAL(ret, j);
        CU_ASSERT_EQUAL(table.timestamp_length, (j + 1) * test_timestamp_length);
        CU_ASSERT_EQUAL(table.timestamp_offset[j + 1], table.timestamp_length);
        CU_ASSERT_EQUAL(table.record_length, (j + 1) * test_record_length);
        CU_ASSERT_EQUAL(table.record_offset[j + 1], table.record_length);
        /* check the timestamp */
        memcpy(timestamp_copy, table.timestamp + table.timestamp_offset[j],
                test_timestamp_length);
        CU_ASSERT_NSTRING_EQUAL(timestamp_copy, test_timestamp, test_timestamp_length);
        /* check the record */
        memcpy(record_copy, table.record + table.record_offset[j],
                test_record_length);
        CU_ASSERT_NSTRING_EQUAL(record_copy, test_record, test_record_length);
    }
    provenance_table_print_state(&table, _devnull);
    provenance_table_clear(&table);
    CU_ASSERT_EQUAL(table.num_rows, 0);
    CU_ASSERT_EQUAL(table.timestamp_length, 0);
    CU_ASSERT_EQUAL(table.record_length, 0);

    num_rows *= 2;
    timestamp = malloc(num_rows * sizeof(char));
    memset(timestamp, 'a', num_rows * sizeof(char));
    CU_ASSERT_FATAL(timestamp != NULL);
    timestamp_offset = malloc((num_rows + 1) * sizeof(table_size_t));
    CU_ASSERT_FATAL(timestamp_offset != NULL);
    record = malloc(num_rows * sizeof(char));
    memset(record, 'a', num_rows * sizeof(char));
    CU_ASSERT_FATAL(record != NULL);
    record_offset = malloc((num_rows + 1) * sizeof(table_size_t));
    CU_ASSERT_FATAL(record_offset != NULL);
    for (j = 0; j < num_rows + 1; j++) {
        timestamp_offset[j] = j;
        record_offset[j] = j;
    }
    ret = provenance_table_set_columns(&table, num_rows,
            timestamp, timestamp_offset, record, record_offset);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.timestamp, timestamp, num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(memcmp(table.timestamp_offset, timestamp_offset,
                (num_rows + 1) * sizeof(table_size_t)), 0);
    CU_ASSERT_EQUAL(memcmp(table.record, record, num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(memcmp(table.record_offset, record_offset,
                (num_rows + 1) * sizeof(table_size_t)), 0);
    CU_ASSERT_EQUAL(table.num_rows, num_rows);
    CU_ASSERT_EQUAL(table.timestamp_length, num_rows);
    CU_ASSERT_EQUAL(table.record_length, num_rows);
    provenance_table_print_state(&table, _devnull);

    /* Append another num_rows onto the end */
    ret = provenance_table_append_columns(&table, num_rows,
            timestamp, timestamp_offset, record, record_offset);
    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(memcmp(table.timestamp, timestamp, num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(memcmp(table.timestamp + num_rows, timestamp, num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(memcmp(table.record, record, num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(memcmp(table.record + num_rows, record, num_rows * sizeof(char)), 0);
    CU_ASSERT_EQUAL(table.num_rows, 2 * num_rows);
    CU_ASSERT_EQUAL(table.timestamp_length, 2 * num_rows);
    CU_ASSERT_EQUAL(table.record_length, 2 * num_rows);
    provenance_table_print_state(&table, _devnull);

    /* No arguments can be null */
    ret = provenance_table_set_columns(&table, num_rows, NULL, timestamp_offset,
            record, record_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = provenance_table_set_columns(&table, num_rows, timestamp, NULL,
            record, record_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = provenance_table_set_columns(&table, num_rows, timestamp, timestamp_offset,
            NULL, record_offset);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);
    ret = provenance_table_set_columns(&table, num_rows, timestamp, timestamp_offset,
            record, NULL);
    CU_ASSERT_EQUAL(ret, MSP_ERR_BAD_PARAM_VALUE);

    provenance_table_free(&table);
    free(timestamp);
    free(timestamp_offset);
    free(record);
    free(record_offset);
}

typedef struct {
    const char *name;
    void *array;
    table_size_t len;
    int type;
} write_table_col_t;

static void
write_table_cols(kastore_t *store, write_table_col_t *write_cols, size_t num_cols)
{
    size_t j;
    int ret;

    for (j = 0; j < num_cols; j++) {
        ret = kastore_puts(store, write_cols[j].name, write_cols[j].array,
                write_cols[j].len, write_cols[j].type, 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
    }
}

static void
test_format_data_load_errors(void)
{
    char format_name[MSP_FILE_FORMAT_NAME_LENGTH];
    double L[2];
    uint32_t version[2] = {
        MSP_FILE_FORMAT_VERSION_MAJOR, MSP_FILE_FORMAT_VERSION_MINOR};
    write_table_col_t write_cols[] = {
        {"format/name", (void *) format_name, sizeof(format_name), KAS_INT8},
        {"format/version", (void *) version, 2, KAS_UINT32},
        {"sequence_length", (void *) L, 1, KAS_FLOAT64},
    };
    table_collection_t tables;
    kastore_t store;
    size_t j;
    int ret;

    L[0] = 1;
    L[1] = 0;
    memcpy(format_name, MSP_FILE_FORMAT_NAME, sizeof(format_name));

    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_table_cols(&store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_load(&tables, _tmp_file_name, 0);
    /* We've only defined the format headers, so we should fail immediately
     * after with key not found */
    CU_ASSERT_TRUE(msp_is_kas_error(ret));
    CU_ASSERT_EQUAL_FATAL(ret ^ (1 << MSP_KAS_ERR_BIT), KAS_ERR_KEY_NOT_FOUND);
    ret = table_collection_free(&tables);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Version too old */
    version[0] = MSP_FILE_FORMAT_VERSION_MAJOR - 1;
    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_table_cols(&store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_load(&tables, _tmp_file_name, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_FILE_VERSION_TOO_OLD);
    ret = table_collection_free(&tables);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Version too new */
    version[0] = MSP_FILE_FORMAT_VERSION_MAJOR + 1;
    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_table_cols(&store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_load(&tables, _tmp_file_name, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_FILE_VERSION_TOO_NEW);
    ret = table_collection_free(&tables);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    version[0] = MSP_FILE_FORMAT_VERSION_MAJOR;

    /* Bad version length */
    write_cols[1].len = 0;
    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_table_cols(&store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_load(&tables, _tmp_file_name, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_FILE_FORMAT);
    ret = table_collection_free(&tables);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_cols[1].len = 2;


    /* Bad format name length */
    write_cols[0].len = 0;
    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_table_cols(&store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_load(&tables, _tmp_file_name, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_FILE_FORMAT);
    ret = table_collection_free(&tables);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_cols[0].len = MSP_FILE_FORMAT_NAME_LENGTH;

    /* Bad format name */
    format_name[0] = 'X';
    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_table_cols(&store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_load(&tables, _tmp_file_name, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_FILE_FORMAT);
    ret = table_collection_free(&tables);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    format_name[0] = 't';

    /* Bad type for sequence length. */
    write_cols[2].type = KAS_FLOAT32;
    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_table_cols(&store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_load(&tables, _tmp_file_name, 0);
    CU_ASSERT_TRUE(msp_is_kas_error(ret));
    CU_ASSERT_EQUAL_FATAL(ret ^ (1 << MSP_KAS_ERR_BIT), KAS_ERR_TYPE_MISMATCH);
    ret = table_collection_free(&tables);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_cols[2].type = KAS_FLOAT64;

    /* Bad length for sequence length. */
    write_cols[2].len = 2;
    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_table_cols(&store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_load(&tables, _tmp_file_name, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_FILE_FORMAT);
    ret = table_collection_free(&tables);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_cols[2].len = 1;

    /* Bad value for sequence length. */
    L[0] = -1;
    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_table_cols(&store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_load(&tables, _tmp_file_name, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_BAD_SEQUENCE_LENGTH);
    ret = table_collection_free(&tables);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Missing keys */
    for (j = 0; j < sizeof(write_cols) / sizeof(*write_cols) - 1; j++) {
        ret = kastore_open(&store, _tmp_file_name, "w", 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        write_table_cols(&store, write_cols, j);
        ret = kastore_close(&store);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        ret = table_collection_load(&tables, _tmp_file_name, 0);
        CU_ASSERT_TRUE(msp_is_kas_error(ret));
        CU_ASSERT_EQUAL_FATAL(ret ^ (1 << MSP_KAS_ERR_BIT), KAS_ERR_KEY_NOT_FOUND);
        ret = table_collection_free(&tables);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
    }
}

void
test_dump_unindexed(void)
{
    table_collection_t tables, loaded;
    int ret;

    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    tables.sequence_length = 1;
    parse_nodes(single_tree_ex_nodes, &tables.nodes);
    CU_ASSERT_EQUAL_FATAL(tables.nodes.num_rows, 7);
    parse_edges(single_tree_ex_edges, &tables.edges);
    CU_ASSERT_EQUAL_FATAL(tables.edges.num_rows, 6);
    CU_ASSERT_FALSE(table_collection_is_indexed(&tables));
    ret = table_collection_dump(&tables, _tmp_file_name, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_TRUE(table_collection_is_indexed(&tables));

    ret = table_collection_alloc(&loaded, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_load(&loaded, _tmp_file_name, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_TRUE(table_collection_is_indexed(&loaded));
    CU_ASSERT_TRUE(node_table_equal(&tables.nodes, &loaded.nodes));
    CU_ASSERT_TRUE(edge_table_equal(&tables.edges, &loaded.edges));

    table_collection_free(&loaded);
    table_collection_free(&tables);
}

void
test_table_collection_load_errors(void)
{
    table_collection_t tables;
    int ret;
    const char *str;

    ret = table_collection_alloc(&tables, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_load(&tables, "/", 0);
    CU_ASSERT_TRUE(msp_is_kas_error(ret));
    CU_ASSERT_EQUAL_FATAL(ret ^ (1 << MSP_KAS_ERR_BIT), KAS_ERR_IO);
    str = msp_strerror(ret);
    CU_ASSERT_TRUE(strlen(str) > 0);

    table_collection_free(&tables);
}

void
test_table_collection_dump_errors(void)
{
    table_collection_t tables;
    int ret;
    const char *str;

    ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_dump(&tables, "/", 0);
    CU_ASSERT_TRUE(msp_is_kas_error(ret));
    CU_ASSERT_EQUAL_FATAL(ret ^ (1 << MSP_KAS_ERR_BIT), KAS_ERR_IO);
    str = msp_strerror(ret);
    CU_ASSERT_TRUE(strlen(str) > 0);

    table_collection_free(&tables);
}

void
test_load_node_table_errors(void)
{
    char format_name[MSP_FILE_FORMAT_NAME_LENGTH];
    size_t uuid_size = 36;
    char uuid[uuid_size];
    double L = 1;
    double time = 0;
    double flags = 0;
    int32_t population = 0;
    int32_t individual = 0;
    int8_t metadata = 0;
    uint32_t metadata_offset[] = {0, 1};
    uint32_t version[2] = {
        MSP_FILE_FORMAT_VERSION_MAJOR, MSP_FILE_FORMAT_VERSION_MINOR};
    write_table_col_t write_cols[] = {
        {"nodes/time", (void *) &time, 1, KAS_FLOAT64},
        {"nodes/flags", (void *) &flags, 1, KAS_UINT32},
        {"nodes/population", (void *) &population, 1, KAS_INT32},
        {"nodes/individual", (void *) &individual, 1, KAS_INT32},
        {"nodes/metadata", (void *) &metadata, 1, KAS_UINT8},
        {"nodes/metadata_offset", (void *) metadata_offset, 2, KAS_UINT32},
        {"format/name", (void *) format_name, sizeof(format_name), KAS_INT8},
        {"format/version", (void *) version, 2, KAS_UINT32},
        {"uuid", (void *) uuid, uuid_size, KAS_INT8},
        {"sequence_length", (void *) &L, 1, KAS_FLOAT64},
    };
    table_collection_t tables;
    kastore_t store;
    int ret;

    memcpy(format_name, MSP_FILE_FORMAT_NAME, sizeof(format_name));
    /* Note: this will fail if we ever start parsing the form of the UUID */
    memset(uuid, 0, uuid_size);

    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_table_cols(&store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_load(&tables, _tmp_file_name, 0);
    /* We've only defined the format headers and nodes, so we should fail immediately
     * after with key not found */
    CU_ASSERT_TRUE(msp_is_kas_error(ret));
    CU_ASSERT_EQUAL_FATAL(ret ^ (1 << MSP_KAS_ERR_BIT), KAS_ERR_KEY_NOT_FOUND);
    ret = table_collection_free(&tables);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Wrong type for time */
    write_cols[0].type = KAS_INT64;
    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_table_cols(&store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_load(&tables, _tmp_file_name, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_FILE_FORMAT);
    ret = table_collection_free(&tables);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_cols[0].type = KAS_FLOAT64;

    /* Wrong length for flags */
    write_cols[1].len = 0;
    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_table_cols(&store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_load(&tables, _tmp_file_name, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_FILE_FORMAT);
    ret = table_collection_free(&tables);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_cols[1].len = 1;

    /* Missing key */
    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_table_cols(&store, write_cols, sizeof(write_cols) / sizeof(*write_cols) - 1);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_load(&tables, _tmp_file_name, 0);
    CU_ASSERT_TRUE(msp_is_kas_error(ret));
    CU_ASSERT_EQUAL_FATAL(ret ^ (1 << MSP_KAS_ERR_BIT), KAS_ERR_KEY_NOT_FOUND);
    ret = table_collection_free(&tables);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Wrong length for metadata offset */
    write_cols[5].len = 1;
    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_table_cols(&store, write_cols, sizeof(write_cols) / sizeof(*write_cols));
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = table_collection_load(&tables, _tmp_file_name, 0);
    CU_ASSERT_EQUAL_FATAL(ret, MSP_ERR_FILE_FORMAT);
    ret = table_collection_free(&tables);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    write_cols[4].len = 2;
}



static int
msprime_suite_init(void)
{
    int fd;
    static char template[] = "/tmp/msp_c_test_XXXXXX";

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
        {"test_vcf", test_vcf},
        {"test_vcf_no_mutations", test_vcf_no_mutations},
        {"test_node_metadata", test_node_metadata},
        {"test_empty_tree_sequence", test_empty_tree_sequence},
        {"test_zero_edges", test_zero_edges},
        {"test_simplest_records", test_simplest_records},
        {"test_simplest_nonbinary_records", test_simplest_nonbinary_records},
        {"test_simplest_unary_records", test_simplest_unary_records},
        {"test_simplest_non_sample_leaf_records", test_simplest_non_sample_leaf_records},
        {"test_simplest_degenerate_multiple_root_records",
            test_simplest_degenerate_multiple_root_records},
        {"test_simplest_multiple_root_records", test_simplest_multiple_root_records},
        {"test_simplest_root_mutations", test_simplest_root_mutations},
        {"test_simplest_back_mutations", test_simplest_back_mutations},
        {"test_simplest_general_samples", test_simplest_general_samples},
        {"test_simplest_holey_tree_sequence", test_simplest_holey_tree_sequence},
        {"test_simplest_holey_tree_sequence_mutation_parents",
            test_simplest_holey_tree_sequence_mutation_parents},
        {"test_simplest_initial_gap_tree_sequence", test_simplest_initial_gap_tree_sequence},
        {"test_simplest_initial_gap_tree_sequence_mutation_parents",
            test_simplest_initial_gap_tree_sequence_mutation_parents},
        {"test_simplest_final_gap_tree_sequence", test_simplest_final_gap_tree_sequence},
        {"test_simplest_final_gap_tree_sequence_mutation_parents",
            test_simplest_final_gap_tree_sequence_mutation_parents},
        {"test_simplest_bad_records", test_simplest_bad_records},
        {"test_simplest_overlapping_parents", test_simplest_overlapping_parents},
        {"test_simplest_contradictory_children", test_simplest_contradictory_children},
        {"test_simplest_overlapping_edges_simplify",
            test_simplest_overlapping_edges_simplify},
        {"test_simplest_overlapping_unary_edges_simplify",
            test_simplest_overlapping_unary_edges_simplify},
        {"test_simplest_overlapping_unary_edges_internal_samples_simplify",
            test_simplest_overlapping_unary_edges_internal_samples_simplify},
        {"test_single_tree_good_records", test_single_tree_good_records},
        {"test_single_nonbinary_tree_good_records",
            test_single_nonbinary_tree_good_records},
        {"test_single_tree_bad_records", test_single_tree_bad_records},
        {"test_single_tree_good_mutations", test_single_tree_good_mutations},
        {"test_single_tree_bad_mutations", test_single_tree_bad_mutations},
        {"test_single_tree_iter", test_single_tree_iter},
        {"test_single_tree_general_samples_iter", test_single_tree_general_samples_iter},
        {"test_single_nonbinary_tree_iter", test_single_nonbinary_tree_iter},
        {"test_single_tree_iter_times", test_single_tree_iter_times},
        {"test_single_tree_hapgen_char_alphabet", test_single_tree_hapgen_char_alphabet},
        {"test_single_tree_hapgen_binary_alphabet", test_single_tree_hapgen_binary_alphabet},
        {"test_single_tree_vargen_char_alphabet", test_single_tree_vargen_char_alphabet},
        {"test_single_tree_vargen_binary_alphabet", test_single_tree_vargen_binary_alphabet},
        {"test_single_tree_vargen_many_alleles", test_single_tree_vargen_many_alleles},
        {"test_single_tree_simplify", test_single_tree_simplify},
        {"test_single_tree_inconsistent_mutations", test_single_tree_inconsistent_mutations},
        {"test_single_tree_compute_mutation_parents", test_single_tree_compute_mutation_parents},
        {"test_single_unary_tree_hapgen", test_single_unary_tree_hapgen},
        {"test_single_tree_mutgen", test_single_tree_mutgen},
        {"test_sparse_tree_errors", test_sparse_tree_errors},
        {"test_tree_sequence_iter", test_tree_sequence_iter},
        {"test_sample_sets", test_sample_sets},
        {"test_nonbinary_sample_sets", test_nonbinary_sample_sets},
        {"test_internal_sample_sample_sets", test_internal_sample_sample_sets},
        {"test_nonbinary_tree_sequence_iter", test_nonbinary_tree_sequence_iter},
        {"test_unary_tree_sequence_iter", test_unary_tree_sequence_iter},
        {"test_internal_sample_tree_sequence_iter", test_internal_sample_tree_sequence_iter},
        {"test_internal_sample_simplified_tree_sequence_iter",
            test_internal_sample_simplified_tree_sequence_iter},
        {"test_left_to_right_tree_sequence_iter", test_left_to_right_tree_sequence_iter},
        {"test_gappy_tree_sequence_iter", test_gappy_tree_sequence_iter},
        {"test_tree_sequence_bad_records", test_tree_sequence_bad_records},
        {"test_tree_sequence_diff_iter", test_tree_sequence_diff_iter},
        {"test_nonbinary_tree_sequence_diff_iter",
            test_nonbinary_tree_sequence_diff_iter},
        {"test_unary_tree_sequence_diff_iter",
            test_unary_tree_sequence_diff_iter},
        {"test_internal_sample_tree_sequence_diff_iter",
            test_internal_sample_tree_sequence_diff_iter},
        {"test_diff_iter_from_examples", test_diff_iter_from_examples},
        {"test_tree_iter_from_examples", test_tree_iter_from_examples},
        {"test_tree_equals_from_examples", test_tree_equals_from_examples},
        {"test_next_prev_from_examples", test_next_prev_from_examples},
        {"test_sample_sets_from_examples", test_sample_sets_from_examples},
        {"test_hapgen_from_examples", test_hapgen_from_examples},
        {"test_vargen_from_examples", test_vargen_from_examples},
        {"test_newick_from_examples", test_newick_from_examples},
        {"test_stats_from_examples", test_stats_from_examples},
        {"test_compute_mutation_parents_from_examples",
            test_compute_mutation_parents_from_examples},
        {"test_ld_from_examples", test_ld_from_examples},
        {"test_simplify_from_examples", test_simplify_from_examples},
        {"test_save_empty_kas", test_save_empty_kas},
        {"test_save_kas", test_save_kas},
        {"test_save_kas_tables", test_save_kas_tables},
        {"test_dump_tables", test_dump_tables},
        {"test_sort_tables", test_sort_tables},
        {"test_deduplicate_sites", test_deduplicate_sites},
        {"test_deduplicate_sites_errors", test_deduplicate_sites_errors},
        {"test_dump_tables_kas", test_dump_tables_kas},
        {"test_strerror", test_strerror},
        {"test_node_table", test_node_table},
        {"test_edge_table", test_edge_table},
        {"test_site_table", test_site_table},
        {"test_mutation_table", test_mutation_table},
        {"test_migration_table", test_migration_table},
        {"test_individual_table", test_individual_table},
        {"test_population_table", test_population_table},
        {"test_provenance_table", test_provenance_table},
        {"test_format_data_load_errors", test_format_data_load_errors},
        {"test_dump_unindexed", test_dump_unindexed},
        {"test_table_collection_load_errors", test_table_collection_load_errors},
        {"test_table_collection_dump_errors", test_table_collection_dump_errors},
        {"test_load_node_table_errors", test_load_node_table_errors},
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
        printf("usage: ./tests <test_name>\n");
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
