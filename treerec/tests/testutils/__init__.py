import pyslim
import msprime
import unittest

class TestSlimOutput(unittest.TestCase):

    def get_slim_ids(self, ts):
        # get SLiM ID -> msprime ID map from metadata
        ids = {}
        for n in ts.nodes():
            meta = pyslim.decode_node(n.metadata)
            ids[meta.slim_id] = n.id
        return ids

