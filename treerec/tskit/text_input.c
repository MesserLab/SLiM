#include "msprime.h"

#include <stdio.h>


/* Simple utilities to parse text so we can write declaritive
 * tests. This is not intended as a robust general input mechanism.
 *
 * These are from test.c in tskit.
 */

static void
node_table_load_text(FILE *nodes, node_table_t *node_table)
{
    int ret;
    size_t c, k;
    size_t MAX_LINE = 1024;
    char line[MAX_LINE];
    const char *whitespace = " \t";
    char *p;
    double time;
    int flags, population;
    char *name;

    c = 0;
    while (text[c] != '\0') {
        /* Fill in the line */
        k = 0;
        while (text[c] != '\n' && text[c] != '\0') {
            assert(k < MAX_LINE - 1);
            line[k] = text[c];
            c++;
            k++;
        }
        if (text[c] == '\n') {
            c++;
        }
        line[k] = '\0';
        p = strtok(line, whitespace);
        if (p == NULL) {
            out = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        flags = atoi(p);
        p = strtok(NULL, whitespace);
        if (p == NULL) {
            out = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        time = atof(p);
        p = strtok(NULL, whitespace);
        if (p == NULL) {
            out = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        population = atoi(p);
        p = strtok(NULL, whitespace);
        if (p == NULL) {
            name = "";
        } else {
            name = p;
        }
        ret = node_table_add_row(node_table, flags, time, population, name,
                strlen(name));
        assert(ret >= 0);
    }
}

static void
edge_table_load_text(FILE *edges, edge_table_t *edge_table)
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
            assert(k < MAX_LINE - 1);
            line[k] = text[c];
            c++;
            k++;
        }
        if (text[c] == '\n') {
            c++;
        }
        line[k] = '\0';
        p = strtok(line, whitespace);
        if (p == NULL) {
            out = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        left = atof(p);
        p = strtok(NULL, whitespace);
        if (p == NULL) {
            out = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        right = atof(p);
        p = strtok(NULL, whitespace);
        if (p == NULL) {
            out = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        parent = atoi(p);
        num_children = 0;
        p = strtok(NULL, whitespace);
        if (p == NULL) {
            out = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        num_children = 1;
        q = p;
        while (*q != '\0') {
            if (*q == ',') {
                num_children++;
            }
            q++;
        }
        assert(num_children >= 1);
        strncpy(sub_line, p, MAX_LINE);
        q = strtok(sub_line, ",");
        for (k = 0; k < num_children; k++) {
            if (q == NULL) {
                out = MSP_ERR_FILE_FORMAT;
                goto out;
            }
            child = atoi(q);
            ret = edge_table_add_row(edge_table, left, right, parent, child);
            assert(ret >= 0);
            q = strtok(NULL, ",");
        }
        assert(q == NULL);
    }
}

static void
site_table_load_text(FILE *sites, site_table_t *site_table)
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
            assert(k < MAX_LINE - 1);
            line[k] = text[c];
            c++;
            k++;
        }
        if (text[c] == '\n') {
            c++;
        }
        line[k] = '\0';
        p = strtok(line, whitespace);
        if (p == NULL) {
            out = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        position = atof(p);
        p = strtok(NULL, whitespace);
        if (p == NULL) {
            out = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        strncpy(ancestral_state, p, MAX_LINE);
        ret = site_table_add_row(site_table, position, ancestral_state,
                strlen(ancestral_state), NULL, 0);
        assert(ret >= 0);
    }
}

static void
mutation_table_load_text(FILE *mutations, mutation_table_t *mutation_table)
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
            assert(k < MAX_LINE - 1);
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
        if (p == NULL) {
            out = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        p = strtok(NULL, whitespace);
        if (p == NULL) {
            out = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        node = atoi(p);
        p = strtok(NULL, whitespace);
        if (p == NULL) {
            out = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        strncpy(derived_state, p, MAX_LINE);
        parent = MSP_NULL_MUTATION;
        p = strtok(NULL, whitespace);
        if (p != NULL) {
            parent = atoi(p);
        }
        ret = mutation_table_add_row(mutation_table, site, node, parent,
                derived_state, strlen(derived_state), NULL, 0);
        if (ret < 0) {
            goto out;
        }
    }
out:
    return ret;
}

static void
individual_table_load_text(FILE *individuals, individual_table_t *individual_table)
{
    int ret;
    size_t c, k;
    size_t MAX_LINE = 1024;
    char line[MAX_LINE];
    const char *whitespace = " \t";
    char *p;
    int sex;
    double age;
    int population;
    node_id_t nodes_f, nodes_m;
    double spatial_x, spatial_y, spatial_z;
    char *name;

    c = 0;
    while (text[c] != '\0') {
        /* Fill in the line */
        k = 0;
        while (text[c] != '\n' && text[c] != '\0') {
            assert(k < MAX_LINE - 1);
            line[k] = text[c];
            c++;
            k++;
        }
        if (text[c] == '\n') {
            c++;
        }
        line[k] = '\0';
        p = strtok(line, whitespace);
        if (p == NULL) {
            out = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        sex = atoi(p);
        p = strtok(NULL, whitespace);
        if (p == NULL) {
            out = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        age = atof(p);
        p = strtok(NULL, whitespace);
        if (p == NULL) {
            out = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        population = atoi(p);
        p = strtok(NULL, whitespace);
        if (p == NULL) {
            out = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        nodes_f = atoi(p);
        p = strtok(NULL, whitespace);
        if (p == NULL) {
            out = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        nodes_m = atoi(p);
        p = strtok(NULL, whitespace);
        if (p == NULL) {
            out = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        spatial_x = atof(p);
        p = strtok(NULL, whitespace);
        if (p == NULL) {
            out = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        spatial_y = atof(p);
        p = strtok(NULL, whitespace);
        if (p == NULL) {
            out = MSP_ERR_FILE_FORMAT;
            goto out;
        }
        spatial_z = atof(p);
        p = strtok(NULL, whitespace);
        if (p == NULL) {
            name = "";
        } else {
            name = p;
        }
        ret = individual_table_add_row(individual_table, sex, age, population, 
                spatial_x, spatial_y, spatial_z, 
                nodes_f, nodes_m, name, strlen(name));
        assert(ret >= 0);
    }
}

static void
table_collection_load_text(table_collection_t *tables, FILE *nodes, FILE *edges,
        FILE *sites, FILE *mutations, FILE *migrations, FILE *individuals, 
        FILE *provenances)
{
    int ret;
    ret = node_table_load_text(nodes, &tables.nodes);
    if (ret != 0) {
        goto out;
    }
    ret = edge_table_load_text(edges, &tables.edges);
    if (ret != 0) {
        goto out;
    }
    if (sites != NULL) {
        ret = site_table_load_text(sites, &tables.sites);
        if (ret != 0) {
            goto out;
        }
    }
    if (mutations != NULL) {
        ret = mutation_table_load_text(mutations, &tables.mutations);
        if (ret != 0) {
            goto out;
        }
    }
    if (migrations != NULL) {
        ret = migration_table_load_text(migrations, &tables.migrations);
        if (ret != 0) {
            goto out;
        }
    }
    if (individuals != NULL) {
        ret = individual_table_load_text(individuals, &tables.individuals);
        if (ret != 0) {
            goto out;
        }
    }
    if (provenances != NULL) {
        ret = provenance_table_load_text(provenances, &tables.provenances);
        if (ret != 0) {
            goto out;
        }
    }
out :
    return ret;
}


