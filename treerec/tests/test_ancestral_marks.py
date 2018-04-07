import msprime
import unittest

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

def get_slim_ids(ts):
    # get SLiM ID -> msprime ID map from metadata
    ids = {}
    for n in ts.nodes():
        meta = n.metadata.decode('utf8')
        assert meta[:7] == "SLiMID="
        slim_id = int(n.metadata.decode('utf8').strip().split("=")[1])
        ids[slim_id] = n.id
    return ids


class TestSlimSim(unittest.TestCase):

    def check_consistency(self, x, y):
        # here y is an iterable of disjoint lists of ids (determined by indivs
        # who share mutations in SLiM) and x is a dict whose keys are ids and
        # whose values are labels (the roots in msprime); check that any two
        # ids in the same element of y have the same label in x
        all_ids = []
        for a in y:
            if len(y[a]) > 0:
                a_labels = []
                for u in y[a]:
                    self.assertTrue(u in x)
                    self.assertTrue(u not in all_ids)
                    all_ids.append(u)
                    a_labels.append(x[u])
                print('IDs sharing mutation', a, ":", y[a])
                print('labels of roots in from tree:', a_labels)
                self.assertEqual(len(set(a_labels)), 1)

    def test_ts_slim_consistency(self):
        # load tree sequence
        node_file = open("test_output/NodeTable.txt", "r")
        edge_file = open("test_output/EdgeTable.txt", "r")
        site_file = open("test_output/SiteTable.txt", "r")
        mutation_file = open("test_output/MutationTable.txt", "r")
        ts = msprime.load_text(nodes=node_file, edges=edge_file, 
                               sites=site_file, mutations=mutation_file,
                               base64_metadata=False)

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
            print("----")
            print(var)
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

# class dontTestSlimSimWithoutMutations(unittest.TestCase):
#     for t in ts.trees():
#         # get partition of leaves from this tree, using SLiM IDs
#         print(t.draw(format="unicode",height = 200))
#         print("left:", t.interval[0], "right:", t.interval[1])
#         fams = {}
#         for x in t.nodes():
#             u = x
#             while t.parent(u) != msprime.NULL_NODE:
#                 u = t.parent(u)
#             fams[x] = u
#         while pos < t.interval[1]:
#             print("pos:", pos, "------------")
#             self.check_consistency(fams, slim[pos])
#             pos += 1
# 
