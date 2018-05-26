#define _GNU_SOURCE

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <float.h>

#include "tables.h"
#include "text_input.h"


/*************************
 * load_text
 *************************/


/****
 * Simple utilities to parse text, mostly for debugging purposes.
 * General assumptions:
 *  files are strictly tab-separated (columns separated by exactly one tab)
 *  columns are in the expected order
 ****/


/* ****
 * Tab-separated parsing:
 * These will be used on null-terminated strings, which are included,
 * so `end`, if not NULL, will always be at least one before the end
 * of the string.
 *
 * These return: 
 *   1 if `sep` is found and delimits a nonzero-length token; 
 *   0 if `sep` is found as the first character;
 *   and -1 otherwise.
 * ***/

static int
get_sep_atoi(char **start, int *out, int sep)
{
    int ret;
    char *next;
    next = strchr(*start, sep);
    if (next == NULL) {
        ret = -1;
    } else {
        ret = (int) (next != *start);
        *next = '\0';
    }
    *out = atoi(*start);
    *start = (next == NULL) ? NULL : next + 1;
    return ret;
}

static int
get_sep_atof(char **start, double *out, int sep)
{
    int ret;
    char *next;
    next = strchr(*start, sep);
    if (next == NULL) {
        ret = -1;
    } else {
        ret = (int) (next != *start);
        *next = '\0';
    }
    *out = atof(*start);
    *start = (next == NULL) ? NULL : next + 1;
    return ret;
}

static int
get_sep_atoa(char **start, char **out, int sep)
{
    int ret;
    char *next;
    next = strchr(*start, sep);
    if (next == NULL) {
        ret = -1;
    } else {
        ret = (int) (next != *start);
        *next = '\0';
    }
    *out = *start;
    *start = (next == NULL) ? NULL : next + 1;
    return ret;
}


int
node_table_load_text(node_table_t *node_table, FILE *file)
{
    int ret;
    int err;
    size_t k;
    size_t MAX_LINE = 1024;
    char *line = NULL;
    double time;
    int flags, population, individual, id, is_sample;
    char *name;
    const char *header = "id\tis_sample\ttime\tpopulation\tindividual\tmetadata\n";
    char *start;

    line = malloc(MAX_LINE);
    if (line == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    k = MAX_LINE;

    ret = node_table_clear(node_table);
    if (ret < 0) {
        goto out;
    }

    // check the header
    ret = MSP_ERR_FILE_FORMAT;
    err = (int) getline(&line, &k, file);
    if (err < 0) {
        goto out;
    }
    err = strcmp(line, header);
    if (err != 0) {
        goto out;
    }

    while ((err = (int) getline(&line, &k, file)) != -1) {
        start = line;
        err = get_sep_atoi(&start, &id, '\t');
        if (err <= 0) {
            goto out;
        }
        err = get_sep_atoi(&start, &is_sample, '\t');
        if (err <= 0) {
            goto out;
        }
        flags = is_sample ? MSP_NODE_IS_SAMPLE : 0;
        err = get_sep_atof(&start, &time, '\t');
        if (err <= 0) {
            goto out;
        }
        err = get_sep_atoi(&start, &population, '\t');
        if (err <= 0) {
            goto out;
        }
        err = get_sep_atoi(&start, &individual, '\t');
        if (err <= 0) {
            goto out;
        }
        err = get_sep_atoa(&start, &name, '\n');
        if (err < 0 || *start != '\0') {
            goto out;
        }
        ret = node_table_add_row(node_table, flags, time, population, individual,
                name, strlen(name));
        if (ret < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    free(line);
    return ret;
}


int
edge_table_load_text(edge_table_t *edge_table, FILE *file)
{
    int ret;
    int err;
    size_t k;
    size_t MAX_LINE = 1024;
    char *line = NULL;
    double left, right;
    node_id_t parent, child;
    const char *header = "left\tright\tparent\tchild\n";
    char *start, *childs;

    line = malloc(MAX_LINE);
    if (line == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    k = MAX_LINE;

    ret = edge_table_clear(edge_table);
    if (ret < 0) {
        goto out;
    }
    
    // check the header
    ret = MSP_ERR_FILE_FORMAT;
    err = (int) getline(&line, &k, file);
    if (err < 0) {
        goto out;
    }
    err = strcmp(line, header);
    if (err != 0) {
        goto out;
    }

    while ((err = (int) getline(&line, &k, file)) != -1) {
        start = line;
        err = get_sep_atof(&start, &left, '\t');
        if (err <= 0) {
            goto out;
        }
        err = get_sep_atof(&start, &right, '\t');
        if (err <= 0) {
            goto out;
        }
        err = get_sep_atoi(&start, &parent, '\t');
        if (err <= 0) {
            goto out;
        }
        err = get_sep_atoa(&start, &childs, '\n');
        if (err < 0) {
            goto out;
        }
        do {
            err = get_sep_atoi(&childs, &child, ',');
            ret = edge_table_add_row(edge_table, left, right, parent, child);
            if (ret < 0) {
                goto out;
            }
        } while (err > 0);
        assert(err == -1);
    }
    ret = 0;
out:
    free(line);
    return ret;
}


int
site_table_load_text(site_table_t *site_table, FILE *file)
{
    int ret;
    int err;
    size_t k;
    size_t MAX_LINE = 1024;
    char *line = NULL;
    int id;
    double position;
    char *ancestral_state, *metadata;
    const char *header = "id\tposition\tancestral_state\tmetadata\n";
    char *start;

    line = malloc(MAX_LINE);
    if (line == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    k = MAX_LINE;

    ret = site_table_clear(site_table);
    if (ret < 0) {
        goto out;
    }
    
    // check the header
    ret = MSP_ERR_FILE_FORMAT;
    err = (int) getline(&line, &k, file);
    if (err < 0) {
        goto out;
    }
    err = strcmp(line, header);
    if (err != 0) {
        goto out;
    }

    while ((err = (int) getline(&line, &k, file)) != -1) {
        start = line;
        err = get_sep_atoi(&start, &id, '\t');
        if (err < 0) {
            goto out;
        }
        err = get_sep_atof(&start, &position, '\t');
        if (err < 0) {
            goto out;
        }
        err = get_sep_atoa(&start, &ancestral_state, '\t');
        if (err < 0) {
            goto out;
        }
        err = get_sep_atoa(&start, &metadata, '\n');
        if (err < 0 || *start != '\0') {
            goto out;
        }
        ret = site_table_add_row(site_table, position, ancestral_state,
                (table_size_t) strlen(ancestral_state), metadata, (table_size_t) strlen(metadata));
        if (ret < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    free(line);
    return ret;
}


int
mutation_table_load_text(mutation_table_t *mutation_table, FILE *file)
{
    int ret;
    int err;
    size_t k;
    size_t MAX_LINE = 1024;
    char *line;
    int id;
    node_id_t node;
    site_id_t site;
    mutation_id_t parent;
    char *derived_state, *metadata;
    const char *header = "id\tsite\tnode\tparent\tderived_state\tmetadata\n";
    char *start;

    line = malloc(MAX_LINE);
    if (line == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    k = MAX_LINE;

    ret = mutation_table_clear(mutation_table);
    if (ret < 0) {
        goto out;
    }
    
    // check the header
    ret = MSP_ERR_FILE_FORMAT;
    err = (int) getline(&line, &k, file);
    if (err < 0) {
        goto out;
    }
    err = strcmp(line, header);
    if (err != 0) {
        goto out;
    }

    while ((err = (int) getline(&line, &k, file)) != -1) {
        start = line;
        err = get_sep_atoi(&start, &id, '\t');
        if (err < 0) {
            goto out;
        }
        err = get_sep_atoi(&start, &site, '\t');
        if (err < 0) {
            goto out;
        }
        err = get_sep_atoi(&start, &node, '\t');
        if (err < 0) {
            goto out;
        }
        err = get_sep_atoi(&start, &parent, '\t');
        if (err < 0) {
            goto out;
        }
        err = get_sep_atoa(&start, &derived_state, '\t');
        if (err < 0) {
            goto out;
        }
        err = get_sep_atoa(&start, &metadata, '\n');
        if (err < 0 || *start != '\0') {
            goto out;
        }
        ret = mutation_table_add_row(mutation_table, site, node, parent,
                derived_state, (table_size_t) strlen(derived_state), metadata, (table_size_t) strlen(metadata));
        if (ret < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    free(line);
    return ret;
}


int
migration_table_load_text(migration_table_t *migration_table, FILE *file)
{
    int ret;
    int err;
    size_t k;
    size_t MAX_LINE = 1024;
    char *line = NULL;
    double left, right, time;
    int node, source, dest;
    const char *header = "left\tright\tnode\tsource\tdest\ttime\n";
    char *start;

    line = malloc(MAX_LINE);
    if (line == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    k = MAX_LINE;

    ret = migration_table_clear(migration_table);
    if (ret < 0) {
        goto out;
    }

    // check the header
    ret = MSP_ERR_FILE_FORMAT;
    err = (int) getline(&line, &k, file);
    if (err < 0) {
        goto out;
    }
    err = strcmp(line, header);
    if (err != 0) {
        goto out;
    }

    while ((err = (int) getline(&line, &k, file)) != -1) {
        start = line;
        err = get_sep_atof(&start, &left, '\t');
        if (err < 0) {
            goto out;
        }
        err = get_sep_atof(&start, &right, '\t');
        if (err < 0) {
            goto out;
        }
        err = get_sep_atoi(&start, &node, '\t');
        if (err < 0) {
            goto out;
        }
        err = get_sep_atoi(&start, &source, '\t');
        if (err < 0) {
            goto out;
        }
        err = get_sep_atoi(&start, &dest, '\t');
        if (err < 0) {
            goto out;
        }
        err = get_sep_atof(&start, &time, '\n');
        if (err < 0) {
            goto out;
        }
        ret = migration_table_add_row(migration_table, left, right, node,
                source, dest, time);
        if (ret < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    free(line);
    return ret;
}


int
individual_table_load_text(individual_table_t *individual_table, FILE *file)
{
    int ret;
    int err;
    size_t j, k;
    size_t MAX_LINE = 1024;
    char *line, *start, *loc;
    double location[MAX_LINE];
    int flags, id;
    char *metadata;
    const char *header = "id\tflags\tlocation\tmetadata\n";

    line = malloc(MAX_LINE);
    if (line == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    k = MAX_LINE;

    ret = individual_table_clear(individual_table);
    if (ret < 0) {
        goto out;
    }
    
    // check the header
    ret = MSP_ERR_FILE_FORMAT;
    err = (int) getline(&line, &k, file);
    if (err < 0) {
        goto out;
    }
    err = strcmp(line, header);
    if (err != 0) {
        goto out;
    }

    while ((err = (int) getline(&line, &k, file)) != -1) {
        start = line;
        err = get_sep_atoi(&start, &id, '\t');
        if (err < 0) {
            goto out;
        }
        err = get_sep_atoi(&start, &flags, '\t');
        if (err < 0) {
            goto out;
        }
        j = 0;
        err = get_sep_atoa(&start, &loc, '\t');
        if (err < 0) {
            goto out;
        }
        if (err > 0) {
            while ((err = get_sep_atof(&loc, location + j, ',')) != 0) {
                j++;
				if (err == -1)
					break;
            }
            if (err == 0) {
                goto out;
            }
        }
        err = get_sep_atoa(&start, &metadata, '\n');
        if (err < 0 || *start != '\0') {
            goto out;
        }
        ret = individual_table_add_row(individual_table, flags, location, j,
                metadata, strlen(metadata));
        if (ret < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    free(line);
    return ret;
}


int
provenance_table_load_text(provenance_table_t *provenance_table, FILE *file)
{
    int ret;
    int err;
    size_t k;
    size_t MAX_LINE = 1024;
    char *line = NULL;
    char *record, *timestamp;
    char *start;
    const char *header = "record\ttimestamp\n";

    line = malloc(MAX_LINE);
    if (line == NULL) {
        ret = MSP_ERR_NO_MEMORY;
        goto out;
    }
    k = MAX_LINE;

    ret = provenance_table_clear(provenance_table);
    if (ret < 0) {
        goto out;
    }
    
    // check the header
    ret = MSP_ERR_FILE_FORMAT;
    err = (int) getline(&line, &k, file);
    if (err < 0) {
        goto out;
    }
    err = strcmp(line, header);
    if (err != 0) {
        goto out;
    }

    while ((err = (int) getline(&line, &k, file)) != -1) {
        start = line;
        err = get_sep_atoa(&start, &record, '\t');
        if (err < 0) {
            goto out;
        }
        err = get_sep_atoa(&start, &timestamp, '\n');
        if (err < 0 || *start != '\0') {
            goto out;
        }
        ret = provenance_table_add_row(provenance_table, timestamp, strlen(timestamp), 
                record, strlen(record));
        if (ret < 0) {
            goto out;
        }
    }
    ret = 0;
out:
    free(line);
    return ret;
}


int
table_collection_load_text(table_collection_t *tables, FILE *nodes, FILE *edges,
        FILE *sites, FILE *mutations, FILE *migrations, FILE *individuals, 
        FILE *provenances)
{
    int ret;
    table_size_t j;
    double sequence_length;

    ret = node_table_load_text(&tables->nodes, nodes);
    if (ret != 0) {
        goto out;
    }
    ret = edge_table_load_text(&tables->edges, edges);
    if (ret != 0) {
        goto out;
    }
    if (sites != NULL) {
        ret = site_table_load_text(&tables->sites, sites);
        if (ret != 0) {
            goto out;
        }
    }
    if (mutations != NULL) {
        ret = mutation_table_load_text(&tables->mutations, mutations);
        if (ret != 0) {
            goto out;
        }
    }
    if (migrations != NULL) {
        ret = migration_table_load_text(&tables->migrations, migrations);
        if (ret != 0) {
            goto out;
        }
    }
    if (individuals != NULL) {
        ret = individual_table_load_text(&tables->individuals, individuals);
        if (ret != 0) {
            goto out;
        }
    }
    if (provenances != NULL) {
        ret = provenance_table_load_text(&tables->provenances, provenances);
        if (ret != 0) {
            goto out;
        }
    }
    /* infer sequence length from the edges and/or sites */
    sequence_length = 0.0;
    for (j = 0; j < tables->edges.num_rows; j++) {
        sequence_length = MSP_MAX(sequence_length, tables->edges.right[j]);
    }
    for (j = 0; j < tables->sites.num_rows; j++) {
        sequence_length = MSP_MAX(sequence_length, tables->sites.position[j]);
    }
    if (sequence_length <= 0.0) {
        ret = MSP_ERR_BAD_SEQUENCE_LENGTH;
        goto out;
    }
    tables->sequence_length = sequence_length;
out :
    return ret;
}

