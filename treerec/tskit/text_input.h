#ifndef TSK_TEXT_INPUT_H
#define TSK_TEXT_INPUT_H

#include <stdio.h>

#include "tables.h"

int node_table_load_text(tsk_node_table_t *node_table, FILE *file);
int edge_table_load_text(tsk_edge_table_t *edge_table, FILE *file);
int site_table_load_text(tsk_site_table_t *site_table, FILE *file);
int mutation_table_load_text(tsk_mutation_table_t *mutation_table, FILE *file);
int migration_table_load_text(tsk_migration_table_t *migration_table, FILE *file);
int individual_table_load_text(tsk_individual_table_t *individual_table, FILE *file);
int population_table_load_text(tsk_population_table_t *population_table, FILE *file);
int provenance_table_load_text(tsk_provenance_table_t *provenance_table, FILE *file);
int table_collection_load_text(tsk_table_collection_t *tables, FILE *nodes, FILE *edges,
        FILE *sites, FILE *mutations, FILE *migrations, FILE *individuals, 
        FILE *populations, FILE *provenances);

#endif /* TSK_TEXT_INPUT_H */

