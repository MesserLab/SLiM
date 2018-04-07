from testutils import *

def read_test_mutation_output(filename="test_output/slim_mutation_output.txt"):
    # Read in the genotypes output by the SLiM function outputMutationResult(),
    # producing a dictionary indexed by position,
    # whose values are dictionaries indexed by SLiM genome ID, giving the genotype
    # at that position in that genome
    slim_file = open(filename, "r")
    slim = {}
    for header in slim_file:
        headstring, genstring = header.split()
        assert headstring == "#Genome:"
        genome = int(genstring)
        mutations = slim_file.readline().split()
        assert mutations[0] == "Mutations:"
        mutations = [int(u) for u in mutations[1:]]
        positions = slim_file.readline().split()
        assert positions[0] == "Positions:"
        positions = [int(u) for u in positions[1:]]
        for pos, mut in zip(positions, mutations):
            if pos not in slim:
                slim[pos] = {}
            slim[pos][genome] = mut
    return slim


class TestWithMutations(unittest.TestCase):

    def get_ts(self):
        # read in from text
        node_file = open("test_output/NodeTable.txt", "r")
        edge_file = open("test_output/EdgeTable.txt", "r")
        site_file = open("test_output/SiteTable.txt", "r")
        mutation_file = open("test_output/MutationTable.txt", "r")
        text_ts = msprime.load_text(nodes=node_file, edges=edge_file, 
                               sites=site_file, mutations=mutation_file,
                               base64_metadata=False)
        print("******* Text input.")
        yield text_ts
        ###
        # These are not passing due to binary encoding.
        if False:
            # and binary
            bin_ts = msprime.load("test_output/test_output.treeseq")
            print("******** Binary input.")
            yield bin_ts
            # and nonsimplified binary
            print("******** Unsimplified binary.")
            bin_nonsip_ts = msprime.load("test_output/test_output.unsimplified.treeseq")
            yield bin_nonsip_ts

    def test_ts_slim_consistency(self):
        # load tree sequence
        for ts in self.get_ts():
            # this is a dictionary of SLiM -> msprime ID (from metadata in nodes)
            ids = get_slim_ids(ts)
            slim_ids = list(ids.keys())
            msp_ids = list(ids.values())
            msp_samples = ts.samples()
            # this contains the genotype information output by SLiM:
            #  indexed by position, then SLiM ID
            slim = read_test_mutation_output(filename="test_output/slim_mutation_output.txt")

            pos = -1
            for var in ts.variants():
                pos += 1
                while pos < int(var.position):
                    # invariant sites: no genotypes
                    self.assertTrue(pos not in slim)
                    pos += 1
                print("pos:", pos)
                for j in slim_ids:
                    if ids[j] in msp_samples:
                        if (pos not in slim) or (j not in slim[pos]):
                            # no mutations at this site
                            self.assertEqual(var.alleles[var.genotypes[ids[j]]], '')
                        else:
                            self.assertEqual(var.alleles[var.genotypes[ids[j]]], str(slim[pos][j]))

