import msprime
import unittest

class TestSlimOutput(unittest.TestCase):

    def get_slim_ids(self, ts):
        # get SLiM ID -> msprime ID map from metadata
        ids = {}
        for n in ts.nodes():
            meta = n.metadata.decode('utf8')
            assert meta[:7] == "SLiMID="
            slim_id = int(n.metadata.decode('utf8').strip().split("=")[1])
            ids[slim_id] = n.id
        return ids

