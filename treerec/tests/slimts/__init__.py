import msprime

"""
To-do:

    1. Provide methods to get Site and Mutation tables into something that SLiM can read:

        * remove fractional part of positions
        * add metadata

    2. Provide method to add appropriate node metadata for A/X/Y and arbitrary (sequential) genome IDs
        to nodes.

"""

# Metadata structure is from slim_sim.h

## struct.unpack: from https://docs.python.org/3.6/library/struct.html
#  Format  Ci Type             Python type
#  x       pad byte            no value
#  c       char                bytes of length 1
#  b       signed_char         integer  1
#  B       unsigned_char       integer  1
#  ?       _Bool               bool     1
#  h       short               integer  2
#  H       unsigned_short      integer  2
#  i       int                 integer  4
#  I       unsigned_int        integer  4
#  l       long                integer  4
#  L       unsigned_long       integer  4
#  q       long_long           integer  8
#  Q       unsigned_long_long  integer  8
#  n       ssize_t             integer   
#  N       size_t              integer   
#  e       (7)                 float    2
#  f       float               float    4
#  d       double              float    8
#  s       char[]              bytes
#  p       char[]              bytes
#  P       void *              integer   

def decode_mutation_metadata(ts, plaintext=False):
    #    typedef struct __attribute__((__packed__)) {
    #        slim_objectid_t mutation_type_id_;        // 4 bytes (int32_t): the id of the mutation type the mutation belongs to
    #        slim_selcoeff_t selection_coeff_;        // 4 bytes (float): the selection coefficient
    #        slim_objectid_t subpop_index_;            // 4 bytes (int32_t): the id of the subpopulation in which the mutation arose
    #        slim_generation_t origin_generation_;    // 4 bytes (int32_t): the generation in which the mutation arose
    #    } MutationMetadataRec;

def decode_individual_metadata(ts, plaintext=False):
    #    typedef struct __attribute__((__packed__)) {
    #        slim_pedigreeid_t pedigree_id_;            // 8 bytes (int64_t): the SLiM pedigree ID for this individual, assigned by pedigree rec
    #    } IndividualMetadataRec;

def decode_genome_metadata(ts, plaintext=False):
    # get SLiM ID -> msprime ID map from metadata
    #    typedef struct __attribute__((__packed__)) {
    #        // 8 bytes (int64_t): the SLiM genome ID for this genome, assigned by pedigree rec
    #        slim_genomeid_t genome_id_;
    #        // 1 byte (uint8_t): true if this is a null genome (should never contain mutations)
    #        uint8_t is_null_;
    #        // 1 byte (uint8_t): the type of the genome (A, X, Y)
    #        GenomeType type_;
    #    } GenomeMetadataRec;
    ids = {}
    for n in ts.nodes():
        if plaintext:
            slim_id, is_null, genome_type = map(int, n.metadata.decode('utf8').split(","))
        else:

        ids[slim_id] = n.id
    return ids

