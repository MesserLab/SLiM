import msprime
import unittest


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
        ts = msprime.load_text(nodes=node_file, edges=edge_file, base64_metadata=False)

        # get SLiM ID -> msprime ID map from metadata
        ids = {}
        for n in ts.nodes():
            meta = n.metadata.decode('utf8')
            self.assertEqual(meta[:7], "SLiMID=")
            slim_id = int(n.metadata.decode('utf8').strip().split("=")[1])
            ids[slim_id] = n.id
            print(n.id)
        print(ids)
        # iterate through SLiM output information
        slim_file = open("test_output/TESToutput.txt", "r")

        # slim will be indexed by position,
        # and contain a dict indexted by mutation type giving the indivs
        # inheriting that mut at that position
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


        # test these agree
        pos = 0
        for t in ts.trees():
            # get partition of leaves from this tree, using SLiM IDs
            print(t.draw(format="unicode",height = 200))
            print("left:", t.interval[0], "right:", t.interval[1])
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

