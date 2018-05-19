/*
** Copyright (C) 2015-2017 University of Oxford
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
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <gsl/gsl_randist.h>

#include "util.h"
#include "msprime.h"
#include "object_heap.h"

typedef struct {
    const char *ancestral_state;
    const char *derived_state;
} mutation_type_t;


static const mutation_type_t binary_mutation_types[] = {
    {"0", "1"}
};

static const mutation_type_t acgt_mutation_types[] = {
    {"A", "C"},
    {"A", "G"},
    {"A", "T"},
    {"C", "A"},
    {"C", "G"},
    {"C", "T"},
    {"G", "A"},
    {"G", "C"},
    {"G", "T"},
    {"T", "A"},
    {"T", "C"},
    {"T", "G"},
};

static int
cmp_double(const void *a, const void *b) {
    const double *ia = (const double *) a;
    const double *ib = (const double *) b;
    return (*ia > *ib) - (*ia < *ib);
}

static int
cmp_infinite_sites_mutation(const void *a, const void *b) {
    const infinite_sites_mutation_t *ia = (const infinite_sites_mutation_t *) a;
    const infinite_sites_mutation_t *ib = (const infinite_sites_mutation_t *) b;
    return (ia->position > ib->position) - (ia->position < ib->position);
}

static void
mutgen_check_state(mutgen_t *self)
{
    /* TODO some checks! */
}

void
mutgen_print_state(mutgen_t *self, FILE *out)
{
    size_t j;

    fprintf(out, "Mutgen state\n");
    fprintf(out, "\tmutation_rate = %f\n", (double) self->mutation_rate);
    fprintf(out, "\tmutation_block_size = %d\n", (int) self->mutation_block_size);
    fprintf(out, "\tmax_num_mutations  = %d\n", (int) self->max_num_mutations);
    fprintf(out, "\tmutations\t%d\n", (int) self->num_mutations);
    for (j = 0; j < self->num_mutations; j++) {
        fprintf(out, "\t%f\t%d\t%s\t%s\n", self->mutations[j].position,
                self->mutations[j].node, self->mutations[j].ancestral_state,
                self->mutations[j].derived_state);
    }
    object_heap_print_state(&self->avl_node_heap, out);
    mutgen_check_state(self);
}


int WARN_UNUSED
mutgen_alloc(mutgen_t *self, double mutation_rate, gsl_rng *rng, int alphabet,
        size_t mutation_block_size)
{
    int ret = 0;

    assert(rng != NULL);
    memset(self, 0, sizeof(mutgen_t));
    /* TODO This is a bad interface anyway and needs to be fixed. Keep this
     * for now to avoid breaking too much stuff */
    if (! (alphabet == 0 || alphabet == 1)) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    if (mutation_block_size == 0) {
        ret = MSP_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    self->alphabet = alphabet;
    self->mutation_rate = mutation_rate;
    self->rng = rng;
    self->mutation_block_size = mutation_block_size;
    self->num_mutations = 0;
    self->max_num_mutations = 0;
    self->mutations = NULL;
    /* The AVL node heap stores both the avl node and the double payload
     * in adjacent memory */
    ret = object_heap_init(&self->avl_node_heap, sizeof(avl_node_t) + sizeof(double),
           self->mutation_block_size, NULL);
    if (ret != 0) {
        goto out;
    }
out:
    return ret;
}

int
mutgen_free(mutgen_t *self)
{
    msp_safe_free(self->mutations);
    object_heap_free(&self->avl_node_heap);
    return 0;
}

static inline avl_node_t * WARN_UNUSED
mutgen_alloc_avl_node(mutgen_t *self, double position)
{
    avl_node_t *ret = NULL;
    double *value;

    if (object_heap_empty(&self->avl_node_heap)) {
        if (object_heap_expand(&self->avl_node_heap) != 0) {
            goto out;
        }
    }
    ret = (avl_node_t *) object_heap_alloc_object(&self->avl_node_heap);
    if (ret == NULL) {
        goto out;
    }
    /* We store the double value after the node */
    value = (double *) (ret + 1);
    *value = position;
    avl_init_node(ret, value);
out:
    return ret;
}

static int WARN_UNUSED
mutgen_add_mutation(mutgen_t *self, node_id_t node, double position,
        const char *ancestral_state, const char *derived_state)
{
    int ret = 0;
    infinite_sites_mutation_t *tmp_buffer;

    assert(self->num_mutations <= self->max_num_mutations);

    if (self->num_mutations == self->max_num_mutations) {
        self->max_num_mutations += self->mutation_block_size;
        tmp_buffer = realloc(self->mutations, self->max_num_mutations
                * sizeof(infinite_sites_mutation_t));
        if (tmp_buffer == NULL) {
            ret = MSP_ERR_NO_MEMORY;
            goto out;
        }
        self->mutations = tmp_buffer;
    }
    self->mutations[self->num_mutations].node = node;
    self->mutations[self->num_mutations].position = position;
    self->mutations[self->num_mutations].ancestral_state = ancestral_state;
    self->mutations[self->num_mutations].derived_state = derived_state;
    self->num_mutations++;
out:
    return ret;
}

/* temporary interface while we're working out the details of passing around
 * data via tables. */
int WARN_UNUSED
mutgen_generate_tables_tmp(mutgen_t *self, node_table_t *nodes,
        edge_table_t *edges)
{
    int ret;
    size_t j, l,  branch_mutations;
    double left, right, branch_length, distance, mu, position;
    node_id_t parent, child;
    const mutation_type_t *mutation_types;
    unsigned long num_mutation_types;
    const char *ancestral_state, *derived_state;
    unsigned long type;
    avl_tree_t positions;
    avl_node_t *avl_node;

    if (self->alphabet == 0) {
        mutation_types = binary_mutation_types;
        num_mutation_types = 1;
    } else {
        mutation_types = acgt_mutation_types;
        num_mutation_types = 12;
    }

    avl_init_tree(&positions, cmp_double, NULL);
    self->num_mutations = 0;
    for (j = 0; j < edges->num_rows; j++) {
        left = edges->left[j];
        right = edges->right[j];
        distance = right - left;
        parent = edges->parent[j];
        child = edges->child[j];
        assert(child >= 0 && child < (node_id_t) nodes->num_rows);
        branch_length = nodes->time[parent] - nodes->time[child];
        mu = branch_length * distance * self->mutation_rate;
        branch_mutations = gsl_ran_poisson(self->rng, mu);
        for (l = 0; l < branch_mutations; l++) {
            /* Rejection sample positions until we get one we haven't seen before. */
            /* TODO add a maximum number of rejections here */
            do {
                position = gsl_ran_flat(self->rng, left, right);
                avl_node = avl_search(&positions, &position);
            } while (avl_node != NULL);
            avl_node = mutgen_alloc_avl_node(self, position);
            if (avl_node == NULL) {
                ret = MSP_ERR_NO_MEMORY;
                goto out;
            }
            avl_insert_node(&positions, avl_node);
            assert(left <= position && position < right);
            type = gsl_rng_uniform_int(self->rng, num_mutation_types);
            ancestral_state = mutation_types[type].ancestral_state;
            derived_state = mutation_types[type].derived_state;
            ret = mutgen_add_mutation(self, child, position, ancestral_state,
                    derived_state);
            if (ret != 0) {
                goto out;
            }
        }
    }
    qsort(self->mutations, self->num_mutations, sizeof(infinite_sites_mutation_t),
            cmp_infinite_sites_mutation);
    assert(avl_count(&positions) == self->num_mutations);
    /* Free up the AVL nodes */
    for (avl_node = positions.head; avl_node != NULL; avl_node = avl_node->next) {
        object_heap_free_object(&self->avl_node_heap, avl_node);
    }
    ret = 0;
out:
    return ret;
}

int
mutgen_populate_tables(mutgen_t *self, site_table_t *sites, mutation_table_t *mutations)
{
    int ret = 0;
    infinite_sites_mutation_t *mut;
    size_t j;

    ret = site_table_clear(sites);
    if (ret != 0) {
        goto out;
    }
    ret = mutation_table_clear(mutations);
    if (ret != 0) {
        goto out;
    }
    for (j = 0; j < self->num_mutations; j++) {
        mut = self->mutations + j;
        ret = site_table_add_row(sites, mut->position, mut->ancestral_state, 1, NULL, 0);
        if (ret < 0) {
            goto out;
        }
        ret = mutation_table_add_row(mutations, (site_id_t) j, mut->node,
                MSP_NULL_MUTATION, mut->derived_state, 1, NULL, 0);
        if (ret < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    return ret;
}
