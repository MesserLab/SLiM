from testutils import *


class TestWithMutations(TestSlimOutput):

    def read_test_mutation_output(self, filename="test_output/slim_mutation_output.txt"):
        # Read in the genotypes output by the SLiM function outputMutationResult(),
        # producing a dictionary indexed by position, whose values are dictionaries
        # indexed by SLiM genome ID, giving a mutation carried at that position in
        # that genome.
        slim_file = open(filename, "r")
        slim = {}
        for header in slim_file:
            headstring = header.split()
            self.assertEqual(headstring[0], "#Genome:")
            genome = int(headstring[1])
            mutations = slim_file.readline().split()
            self.assertEqual(mutations[0], "Mutations:")
            mutations = [int(u) for u in mutations[1:]]
            positions = slim_file.readline().split()
            self.assertEqual(positions[0], "Positions:")
            positions = [int(u) for u in positions[1:]]
            for pos, mut in zip(positions, mutations):
                if pos not in slim:
                    slim[pos] = {}
                if genome not in slim[pos]:
                    slim[pos][genome] = []
                slim[pos][genome].append(mut)
        return slim

    def get_ts(self):
        # TODO include more tables so text input/output works
        if False:
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
        # and binary
        bin_ts = pyslim.load("test_output/test_output.trees", slim_format=True)
        print("******** Binary input.")
        yield bin_ts
        # and nonsimplified binary
        print("******** Unsimplified binary.")
        bin_nonsip_ts = pyslim.load("test_output/test_output.unsimplified.trees", slim_format=True)
        yield bin_nonsip_ts

    def test_ts_slim_consistency(self):
        # load tree sequence
        for ts in self.get_ts():
            # this is a dictionary of SLiM -> msprime ID (from metadata in nodes)
            ids = self.get_slim_ids(ts)
            slim_ids = list(ids.keys())
            msp_ids = list(ids.values())
            msp_samples = ts.samples()
            # this contains the genotype information output by SLiM:
            #  indexed by position, then SLiM ID
            slim = self.read_test_mutation_output(filename="test_output/slim_mutation_output.txt")
            pos = -1
            for var in ts.variants():
                pos += 1
                while pos < int(var.position):
                    # invariant sites: no genotypes
                    self.assertTrue(pos not in slim)
                    pos += 1
                print("-----------------")
                print("pos:", pos)
                print(var)
                for j in slim_ids:
                    print("slim id", j, "msp id", ids[j])
                    if ids[j] in msp_samples:
                        msp_genotypes = [u for u in var.alleles[var.genotypes[ids[j]]].split(",")]
                        print("msp:", msp_genotypes)
                        if (pos not in slim) or (j not in slim[pos]):
                            # no mutations at this site
                            self.assertEqual(msp_genotypes, [''])
                        else:
                            print("slim:", slim[pos][j])
                            self.assertEqual(set(msp_genotypes), set([str(u) for u in slim[pos][j]]))

