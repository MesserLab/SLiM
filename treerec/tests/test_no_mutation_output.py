from testutils import *

class TestNoMutations(TestSlimOutput):

    def read_no_mutation_output(self, ids,
                                filename="test_output/slim_no_mutation_output.txt"):
            # slim will be indexed by position,
            # and contain a dict indexted by mutation type giving the indivs
            # inheriting that mut at that position
            slim_file = open(filename, "r")
            slim = []
            for header in slim_file:
                self.assertEqual(header[0:12], "MutationType")
                mut, pos = header[12:].split()
                pos = int(pos)
                if pos == len(slim):
                    slim.append({})
                slim_ids = [int(u) for u in slim_file.readline().split()]
                for u in slim_ids:
                    self.assertTrue(u in ids)
                slim[pos][mut] = [ids[u] for u in slim_ids]
            return slim

    def get_ts(self):
        # TODO include more tables so text input/output works
        if False:
            # read in from text 
            node_file = open("test_output/NodeTable.txt", "r")
            edge_file = open("test_output/EdgeTable.txt", "r")
            text_ts = msprime.load_text(nodes=node_file, edges=edge_file, 
                                   base64_metadata=False)
            print("******* Text input.")
            yield text_ts
        # and binary
        bin_ts = pyslim.load("test_output/test_output.trees")
        print("******** Binary input.")
        yield bin_ts
        # and nonsimplified binary
        print("******** Unsimplified binary.")
        bin_nonsip_ts = pyslim.load("test_output/test_output.unsimplified.trees")
        yield bin_nonsip_ts

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
        for ts in self.get_ts():
            # this is a dictionary of SLiM -> msprime ID (from metadata in nodes)
            ids = self.get_slim_ids(ts)
            slim = self.read_no_mutation_output(ids)
            pos = 0
            for t in ts.trees():
                # get partition of leaves from this tree, using SLiM IDs
                fams = {}
                for x in t.nodes():
                    u = x
                    while t.parent(u) != msprime.NULL_NODE:
                        u = t.parent(u)
                    fams[x] = u
                while pos < t.interval[1]:
                    print("pos:", pos, "------------")
                    self.check_consistency(fams, slim[pos])
                    pos += 1
