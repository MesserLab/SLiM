#include "tables.h"

#include <iostream>
#include <stdexcept>
#include <cstdlib>

using namespace std;

static void 
raise_exception(int err) noexcept(false)
{
    throw std::runtime_error(msp_strerror(err));
}

int
main(int argc, char **argv)
{
    int j, ret;
    int exit_status = 0;
    table_collection_t tables;
    node_id_t samples[] = {0};
    
    try {
        /* Allocate a table collection, with all the internal tables initialised
         * using default alloc sizes. It's very important that table_collection_alloc
         * is called BEFORE table_collection_free. */
        ret = table_collection_alloc(&tables, MSP_ALLOC_TABLES);
        if (ret != 0) {
            raise_exception(ret);
        }
        /* NB: must set the sequence_length !! */
        tables.sequence_length = 1.0;

        /* Create a simple chain of nodes, with 0 as the only sample. */
        for (j = 0; j < 10; j++) {
            /* node and edge_table_add_row return < 0 in the case of an error,
             * or the ID of the node/edge just added otherwise. */
            ret = node_table_add_row(&tables.nodes, j == 0, j, 0, NULL, 0);
            if (ret < 0) {
                raise_exception(ret);
            }
            if (j > 0) {
                ret = edge_table_add_row(&tables.edges, 0, 1, j, j - 1);
                if (ret < 0) {
                    raise_exception(ret);
                }
            }
        }

        /* Write the state out to file */
        ret = table_collection_dump(&tables, "tmp.hdf5", 0);
        if (ret != 0) {
            raise_exception(ret);
        }
        /* Useful debugging feature */
        table_collection_print_state(&tables, stdout);

        /* Simplify the tables */
        ret = table_collection_simplify(&tables, samples, 1, 0, NULL);
        if (ret != 0) {
            raise_exception(ret);
        }
        /* After simplify, we only have 1 node left and no edges */
        node_table_print_state(&tables.nodes, stdout);
        edge_table_print_state(&tables.edges, stdout);
    } catch (exception &e) {
        cerr << "Error: " << e.what() << '\n';
        exit_status = 1;
    }

    /* Free the tables */
    table_collection_free(&tables);
    return exit_status;
}
