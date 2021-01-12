from testutils import *
import collections
import numpy as np

SLiMindividual = collections.namedtuple('SLiMindividual', 'type population pos nodes')

class TestWithMutations(TestSlimOutput):

    def read_test_individual_output(self, filename):
        # Read in the individuals output by the SLiM function addIndividuals(),
        # producing a dictionary indexed by individual pedigree ID
        slim_file = open(filename, "r")
        slim = {}
        for line in slim_file:
            fields = line.split()
            assert fields[0].startswith("#Individual:")
            store = fields[0][len("#Individual:"):]
            assert store in ("remember", "retain", "output")
            pedigree_id = int(fields[1])
            if pedigree_id in slim and store == "retain":
                # We have a duplicate: "remember"ed takes priority
                continue
            slim[pedigree_id] = SLiMindividual(
                type=store,
                population=int(fields[2]),
                pos=[float(p) for p in fields[3].split(",")],
                nodes=[int(p) for p in fields[4].split(",")],
            )
        return slim

    def test_ts_slim_consistency(self):
        # load tree sequence
        for ts in self.get_ts():
            ts = ts.simplify(filter_individuals=True)  # check we are fully simplified
            # this is a dictionary of SLiM -> tskit ID (from metadata in nodes)
            ids = self.get_slim_ids(ts)
            # this is a dict of tskit ID -> index in samples
            slim_individuals = self.read_test_individual_output(
                filename="test_output/slim_individual_output.txt")
            # check that all the individual data lines up
            ts_individuals = {i.metadata['pedigree_id']: i for i in ts.individuals()}
            alive = {ts.individual(i).metadata['pedigree_id'] for i in ts.individuals_alive_at(0)}
            num_expected = 0
            for ped_id, slim_ind in slim_individuals.items():
                if slim_ind.type == "retain":
                    # retained individuals should only be present in the TS if they
                    # have genomic material. We should check which have nodes that exist
                    ts_nodes = [ids[n] for n in slim_ind.nodes if n in ids]
                    if len(ts_nodes) > 0:
                        num_expected += 1
                        assert ped_id in ts_individuals
                        metadata = ts_individuals[ped_id].metadata
                        assert slim_ind.population == metadata['subpopulation']
                        assert np.allclose(slim_ind.pos, ts_individuals[ped_id].location)
                        for n in ts_nodes:
                            print(ped_id)
                            assert not ts.node(n).is_sample()
                    else:
                        assert ped_id not in ts_individuals
                else:
                    # permanently "remember"ed individuals and ones that exist during ts
                    # output are always present, and their nodes should be marked as
                    # as samples
                    num_expected += 1
                    assert ped_id in ts_individuals
                    metadata = ts_individuals[ped_id].metadata
                    assert slim_ind.population == metadata['subpopulation']
                    assert np.allclose(slim_ind.pos, ts_individuals[ped_id].location)
                    ts_nodes = set([ids[n] for n in slim_ind.nodes])
                    assert ts_nodes == set(ts_individuals[ped_id].nodes)
                    for n in ts_nodes:
                        assert ts.node(n).is_sample()
                    if ped_id not in alive:
                        assert slim_ind.type == "remember"
            assert num_expected == ts.num_individuals
