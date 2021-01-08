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
            assert headstring[0] == "#Genome:"
            genome = int(headstring[1])
            mutations = slim_file.readline().split()
            assert mutations[0] == "Mutations:"
            mutations = [int(u) for u in mutations[1:]]
            positions = slim_file.readline().split()
            assert positions[0] == "Positions:"
            positions = [int(u) for u in positions[1:]]
            for pos, mut in zip(positions, mutations):
                if pos not in slim:
                    slim[pos] = {}
                if genome not in slim[pos]:
                    slim[pos][genome] = []
                slim[pos][genome].append(mut)
        return slim

    def test_ts_slim_consistency(self):
        # load tree sequence
        for ts in self.get_ts():
            # this is a dictionary of SLiM -> tskit ID (from metadata in nodes)
            ids = self.get_slim_ids(ts)
            # this is a dict of tskit ID -> index in samples
            msp_samples = {}
            for k, u in enumerate(ts.samples()):
                msp_samples[u] = k
            # this contains the genotype information output by SLiM:
            #  indexed by position, then SLiM ID
            slim = self.read_test_mutation_output(filename="test_output/slim_mutation_output.txt")
            pos = -1
            for var in ts.variants(isolated_as_missing=False):
                pos += 1
                while pos < int(var.position):
                    # invariant sites: no genotypes
                    assert pos not in slim
                    pos += 1
                print("-----------------")
                print("pos:", pos)
                print(var)
                for j in ids:
                    print("slim id", j, "msp id", ids[j])
                    if ids[j] in msp_samples:
                        sample_num = msp_samples[ids[j]]
                        geno = var.genotypes[sample_num]
                        msp_genotypes = var.alleles[geno].split(",")
                        print("msp:", msp_genotypes)
                        if (pos not in slim) or (j not in slim[pos]):
                            # no mutations at this site
                            assert msp_genotypes == ['']
                        else:
                            print("slim:", slim[pos][j])
                            assert set(msp_genotypes) == set([str(x) for x in slim[pos][j]])

