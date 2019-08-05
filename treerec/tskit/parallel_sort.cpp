#ifdef __cplusplus
#include <tskit/tables.h>
#include <parallel/algorithm>
#include <iostream>
//#include "tbb/parallel_sort.h"

extern "C"{
#endif

void psort_edges(edge_sort_t *sorted_edges, tsk_size_t n);
void psort_sites(tsk_site_t *sorted_sites, tsk_size_t num_sites);
void psort_mutations(tsk_mutation_t *sorted_mutations, tsk_size_t num_mutations);

//Bool based comparator functions for sorting. The old ones were giving me problems so recoded it. Hope that's fine. 
bool cmp_edges(edge_sort_t const &a, const edge_sort_t &b)
{
    if(a.time != b.time)
    {
        return a.time < b.time;
    }
    else if(a.parent != b.parent)
    {
        /* If time values are equal, sort by the parent node */
        return a.parent < b.parent;
    }
    else if (a.child != b.child)
    {
        /* If the parent nodes are equal, sort by the child ID. */
        return a.child < b.child;
    }
    else if (a.left != b.left)
    {
         /* If the child nodes are equal, sort by the left coordinate. */
        return a.left < b.left;
    }
}

bool cmp_sites(tsk_site_t const &a, const tsk_site_t &b)
{
    if(a.position != b.position)
    {
        /* Compare sites by position */
        return a.position < b.position;
    }
    else 
    {
        /* Within a particular position sort by ID.  This ensures that relative ordering
         * of multiple sites at the same position is maintained; the redundant sites
         * will get compacted down by clean_tables(), but in the meantime if the order
         * of the redundant sites changes it will cause the sort order of mutations to
         * be corrupted, as the mutations will follow their sites. */
        return a.id < b.id;
    }
}

bool cmp_mutations(tsk_mutation_t const &a, const tsk_mutation_t &b)
{
    /* Compare mutations by site */
    if(a.site != b.site)
    {
        return a.site < b.site;
    }
    else
    {
        /* Within a particular site sort by ID. This ensures that relative ordering
         * within a site is maintained */
        return a.id < b.id;
    }
}

void psort_edges(edge_sort_t *sorted_edges, tsk_size_t n)
{
#ifdef Parallel
    __gnu_parallel::sort(sorted_edges, sorted_edges+n ,&cmp_edges);
#else
   std::sort(sorted_edges, sorted_edges+n ,&cmp_edges);
#endif   
    //tbb::parallel_sort(sorted_edges, sorted_edges+n ,&cmp_edges);

}

void psort_sites(tsk_site_t *sorted_sites, tsk_size_t num_sites)
{
#ifdef Parallel    
    __gnu_parallel::sort(sorted_sites, sorted_sites+num_sites ,&cmp_sites);
#else  
    std::sort(sorted_sites, sorted_sites+num_sites ,&cmp_sites);
#endif
}


void psort_mutations(tsk_mutation_t *sorted_mutations, tsk_size_t num_mutations)
{
#ifdef Parallel
    __gnu_parallel::sort(sorted_mutations, sorted_mutations+num_mutations, &cmp_mutations);
#else    
    std::sort(sorted_mutations, sorted_mutations+num_mutations, &cmp_mutations);
#endif

}

#ifdef __cplusplus
}
#endif


